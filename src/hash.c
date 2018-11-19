/*
** hash.c - Hash class
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#ifndef WITHOUT_FLOAT
/* a function to get hash value of a float number */
int float_id(float f);
#endif

/* return non zero to break the loop */
typedef int (sg_foreach_func)(state *mrb,value key, value val, void *data);

#ifndef SG_SEGMENT_SIZE
#define SG_SEGMENT_SIZE 5
#endif

struct segkv {
  value key;
  value val;
};

typedef struct segment {
  struct segment *next;
  struct segkv e[SG_SEGMENT_SIZE];
} segment;

typedef struct segindex {
  size_t size;
  size_t capa;
  struct segkv *table[];
} segindex;

/* Instance variable table structure */
typedef struct seglist {
  segment *rootseg;
  segment *lastseg;
  int size;
  int last_len;
  segindex *index;
} seglist;

static /* inline */ size_t
sg_hash_func(state *mrb, seglist *t, value key)
{
  enum vtype tt = type(key);
  value hv;
  size_t h;
  segindex *index = t->index;
  size_t capa = index ? index->capa : 0;

  switch (tt) {
  case TT_STRING:
    h = str_hash(mrb, key);
    break;

  case TT_TRUE:
  case TT_FALSE:
  case TT_SYMBOL:
  case TT_FIXNUM:
#ifndef WITHOUT_FLOAT
  case TT_FLOAT:
#endif
    h = (size_t)obj_id(key);
    break;

  default:
    hv = funcall(mrb, key, "hash", 0);
    h = (size_t)t ^ (size_t)fixnum(hv);
    break;
  }
  if (index && (index != t->index || capa != index->capa)) {
    raise(mrb, E_RUNTIME_ERROR, "hash modified");
  }
  return ((h)^((h)<<2)^((h)>>2));
}

static inline bool
sg_hash_equal(state *mrb, seglist *t, value a, value b)
{
  enum vtype tt = type(a);

  switch (tt) {
  case TT_STRING:
    return str_equal(mrb, a, b);

  case TT_SYMBOL:
    if (type(b) != TT_SYMBOL) return FALSE;
    return symbol(a) == symbol(b);

  case TT_FIXNUM:
    switch (type(b)) {
    case TT_FIXNUM:
      return fixnum(a) == fixnum(b);
#ifndef WITHOUT_FLOAT
    case TT_FLOAT:
      return (float)fixnum(a) == float(b);
#endif
    default:
      return FALSE;
    }

#ifndef WITHOUT_FLOAT
  case TT_FLOAT:
    switch (type(b)) {
    case TT_FIXNUM:
      return float(a) == (float)fixnum(b);
    case TT_FLOAT:
      return float(a) == float(b);
    default:
      return FALSE;
    }
#endif

  default:
    {
      segindex *index = t->index;
      size_t capa = index ? index->capa : 0;
      bool eql = eql(mrb, a, b);
      if (index && (index != t->index || capa != index->capa)) {
        raise(mrb, E_RUNTIME_ERROR, "hash modified");
      }
      return eql;
    }
  } 
}

/* Creates the instance variable table. */
static seglist*
sg_new(state *mrb)
{
  seglist *t;

  t = (seglist*)malloc(mrb, sizeof(seglist));
  t->size = 0;
  t->rootseg =  NULL;
  t->lastseg =  NULL;
  t->last_len = 0;
  t->index = NULL;

  return t;
}

#define power2(v) do { \
  v--;\
  v |= v >> 1;\
  v |= v >> 2;\
  v |= v >> 4;\
  v |= v >> 8;\
  v |= v >> 16;\
  v++;\
} while (0)

#ifndef UPPER_BOUND
#define UPPER_BOUND(x) ((x)>>2|(x)>>1)
#endif

#define SG_MASK(index) ((index->capa)-1)

/* Build index for the segment list */
static void
sg_index(state *mrb, seglist *t)
{
  size_t size = (size_t)t->size;
  size_t mask;
  segindex *index = t->index;
  segment *seg;
  size_t i;

  if (size < SG_SEGMENT_SIZE) {
    if (index) {
    failed:
      free(mrb, index);
      t->index = NULL;
    }
    return;
  }
  /* allocate index table */
  if (index && index->size >= UPPER_BOUND(index->capa)) {
    size = index->capa+1;
  }
  power2(size);
  if (!index || index->capa < size) {
    index = (segindex*)realloc_simple(mrb, index, sizeof(segindex)+sizeof(struct segkv*)*size);
    if (index == NULL) goto failed;
    t->index = index;
  }
  index->size = t->size;
  index->capa = size;
  for (i=0; i<size; i++) {
    index->table[i] = NULL;
  }

  /* rebuld index */
  mask = SG_MASK(index);
  seg = t->rootseg;
  while (seg) {
    for (i=0; i<SG_SEGMENT_SIZE; i++) {
      value key;
      size_t k, step = 0;

      if (!seg->next && i >= (size_t)t->last_len) {
        return;
      }
      key = seg->e[i].key;
      if (undef_p(key)) continue;
      k = sg_hash_func(mrb, t, key) & mask;
      while (index->table[k]) {
        k = (k+(++step)) & mask;
      }
      index->table[k] = &seg->e[i];
    }
    seg = seg->next;
  }
}

/* Compacts the segment list removing deleted entries. */
static void
sg_compact(state *mrb, seglist *t)
{
  segment *seg;
  int i;
  segment *seg2 = NULL;
  int i2;
  int size = 0;

  if (t == NULL) return;
  seg = t->rootseg;
  if (t->index && (size_t)t->size == t->index->size) {
    sg_index(mrb, t);
    return;
  }
  while (seg) {
    for (i=0; i<SG_SEGMENT_SIZE; i++) {
      value k = seg->e[i].key;

      if (!seg->next && i >= t->last_len) {
        goto exit;
      }
      if (undef_p(k)) {     /* found delete key */
        if (seg2 == NULL) {
          seg2 = seg;
          i2 = i;
        }
      }
      else {
        size++;
        if (seg2 != NULL) {
          seg2->e[i2++] = seg->e[i];
          if (i2 >= SG_SEGMENT_SIZE) {
            seg2 = seg2->next;
            i2 = 0;
          }
        }
      }
    }
    seg = seg->next;
  }
 exit:
  /* reached at end */
  t->size = size;
  if (seg2) {
    seg = seg2->next;
    seg2->next = NULL;
    t->last_len = i2;
    t->lastseg = seg2;
    while (seg) {
      seg2 = seg->next;
      free(mrb, seg);
      seg = seg2;
    }
  }
  if (t->index) {
    sg_index(mrb, t);
  }
}

/* Set the value for the key in the indexed segment list. */
static void
sg_index_put(state *mrb, seglist *t, value key, value val)
{
  segindex *index = t->index;
  size_t k, sp, step = 0, mask;
  segment *seg;

  if (index->size >= UPPER_BOUND(index->capa)) {
    /* need to expand table */
    sg_compact(mrb, t);
    index = t->index;
  }
  mask = SG_MASK(index);
  sp = index->capa;
  k = sg_hash_func(mrb, t, key) & mask;
  while (index->table[k]) {
    value key2 = index->table[k]->key;
    if (undef_p(key2)) {
      if (sp == index->capa) sp = k;
    }
    else if (sg_hash_equal(mrb, t, key, key2)) {
      index->table[k]->val = val;
      return;
    }
    k = (k+(++step)) & mask;
  }
  if (sp < index->capa) {
    k = sp;
  }

  /* put the value at the last */
  seg = t->lastseg;
  if (t->last_len < SG_SEGMENT_SIZE) {
    index->table[k] = &seg->e[t->last_len++];
  }
  else {                        /* append a new segment */
    seg->next = (segment*)malloc(mrb, sizeof(segment));
    seg = seg->next;
    seg->next = NULL;
    t->lastseg = seg;
    t->last_len = 1;
    index->table[k] = &seg->e[0];
  }
  index->table[k]->key = key;
  index->table[k]->val = val;
  index->size++;
  t->size++;
}

/* Set the value for the key in the segment list. */
static void
sg_put(state *mrb, seglist *t, value key, value val)
{
  segment *seg;
  int i, deleted = 0;

  if (t == NULL) return;
  if (t->index) {
    sg_index_put(mrb, t, key, val);
    return;
  }
  seg = t->rootseg;
  while (seg) {
    for (i=0; i<SG_SEGMENT_SIZE; i++) {
      value k = seg->e[i].key;
      /* Found room in last segment after last_len */
      if (!seg->next && i >= t->last_len) {
        seg->e[i].key = key;
        seg->e[i].val = val;
        t->last_len = i+1;
        t->size++;
        return;
      }
      if (undef_p(k)) {
        deleted++;
        continue;
      }
      if (sg_hash_equal(mrb, t, k, key)) {
        seg->e[i].val = val;
        return;
      }
    }
    seg = seg->next;
  }

  /* Not found */
  if (deleted > SG_SEGMENT_SIZE) {
    sg_compact(mrb, t);
  }
  t->size++;

  seg = (segment*)malloc(mrb, sizeof(segment));
  seg->next = NULL;
  seg->e[0].key = key;
  seg->e[0].val = val;
  t->last_len = 1;
  if (t->rootseg == NULL) {
    t->rootseg = seg;
  }
  else {
    t->lastseg->next = seg;
  }
  t->lastseg = seg;
  if (t->index == NULL && t->size > SG_SEGMENT_SIZE*4) {
    sg_index(mrb, t);
  }
}

/* Get a value for a key from the indexed segment list. */
static bool
sg_index_get(state *mrb, seglist *t, value key, value *vp)
{
  segindex *index = t->index;
  size_t mask = SG_MASK(index);
  size_t k = sg_hash_func(mrb, t, key) & mask;
  size_t step = 0;

  while (index->table[k]) {
    value key2 = index->table[k]->key;
    if (!undef_p(key2) && sg_hash_equal(mrb, t, key, key2)) {
      if (vp) *vp = index->table[k]->val;
      return TRUE;
    }
    k = (k+(++step)) & mask;
  }
  return FALSE;
}

/* Get a value for a key from the segment list. */
static bool
sg_get(state *mrb, seglist *t, value key, value *vp)
{
  segment *seg;
  int i;

  if (t == NULL) return FALSE;
  if (t->index) {
    return sg_index_get(mrb, t, key, vp);
  }

  seg = t->rootseg;
  while (seg) {
    for (i=0; i<SG_SEGMENT_SIZE; i++) {
      value k = seg->e[i].key;

      if (!seg->next && i >= t->last_len) {
        return FALSE;
      }
      if (undef_p(k)) continue;
      if (sg_hash_equal(mrb, t, k, key)) {
        if (vp) *vp = seg->e[i].val;
        return TRUE;
      }
    }
    seg = seg->next;
  }
  return FALSE;
}

/* Deletes the value for the symbol from the instance variable table. */
/* Deletion is done by overwriting keys by `undef`. */
static bool
sg_del(state *mrb, seglist *t, value key, value *vp)
{
  segment *seg;
  int i;

  if (t == NULL) return FALSE;
  seg = t->rootseg;
  while (seg) {
    for (i=0; i<SG_SEGMENT_SIZE; i++) {
      value key2;

      if (!seg->next && i >= t->last_len) {
        /* not found */
        return FALSE;
      }
      key2 = seg->e[i].key;
      if (!undef_p(key2) && sg_hash_equal(mrb, t, key, key2)) {
        if (vp) *vp = seg->e[i].val;
        seg->e[i].key = undef_value();
        t->size--;
        return TRUE;
      }
    }
    seg = seg->next;
  }
  return FALSE;
}

/* Iterates over the instance variable table. */
static void
sg_foreach(state *mrb, seglist *t, sg_foreach_func *func, void *p)
{
  segment *seg;
  int i;

  if (t == NULL) return;
  if (t->index && t->index->size-(size_t)t->size > SG_SEGMENT_SIZE) {
    sg_compact(mrb, t);
  }
  seg = t->rootseg;
  while (seg) {
    for (i=0; i<SG_SEGMENT_SIZE; i++) {
      /* no value in last segment after last_len */
      if (!seg->next && i >= t->last_len) {
        return;
      }
      if (undef_p(seg->e[i].key)) continue;
      if ((*func)(mrb, seg->e[i].key, seg->e[i].val, p) != 0)
        return;
    }
    seg = seg->next;
  }
}

/* Get the size of the instance variable table. */
static int
sg_size(state *mrb, seglist *t)
{
  if (t == NULL) return 0;
  return t->size;
}

/* Copy the instance variable table. */
static seglist*
sg_copy(state *mrb, seglist *t)
{
  segment *seg;
  seglist *t2;
  int i;

  seg = t->rootseg;
  t2 = sg_new(mrb);

  while (seg != NULL) {
    for (i=0; i<SG_SEGMENT_SIZE; i++) {
      value key = seg->e[i].key;
      value val = seg->e[i].val;

      if ((seg->next == NULL) && (i >= t->last_len)) {
        return t2;
      }
      sg_put(mrb, t2, key, val);
    }
    seg = seg->next;
  }
  return t2;
}

/* Free memory of the instance variable table. */
static void
sg_free(state *mrb, seglist *t)
{
  segment *seg;

  if (!t) return;
  seg = t->rootseg;
  while (seg) {
    segment *p = seg;
    seg = seg->next;
    free(mrb, p);
  }
  if (t->index) free(mrb, t->index);
  free(mrb, t);
}

static void hash_modify(state *mrb, value hash);

static inline value
ht_key(state *mrb, value key)
{
  if (string_p(key) && !FROZEN_P(str_ptr(key))) {
    key = str_dup(mrb, key);
    SET_FROZEN_FLAG(str_ptr(key));
  }
  return key;
}

#define KEY(key) ht_key(mrb, key)

static int
hash_mark_i(state *mrb, value key, value val, void *p)
{
  gc_mark_value(mrb, key);
  gc_mark_value(mrb, val);
  return 0;
}

void
gc_mark_hash(state *mrb, struct RHash *hash)
{
  sg_foreach(mrb, hash->ht, hash_mark_i, NULL);
}

size_t
gc_mark_hash_size(state *mrb, struct RHash *hash)
{
  if (!hash->ht) return 0;
  return sg_size(mrb, hash->ht)*2;
}

void
gc_free_hash(state *mrb, struct RHash *hash)
{
  sg_free(mrb, hash->ht);
}

API value
hash_new(state *mrb)
{
  struct RHash *h;

  h = (struct RHash*)obj_alloc(mrb, TT_HASH, mrb->hash_class);
  h->ht = 0;
  h->iv = 0;
  return obj_value(h);
}

API value
hash_new_capa(state *mrb, int capa)
{
  struct RHash *h;

  h = (struct RHash*)obj_alloc(mrb, TT_HASH, mrb->hash_class);
  /* preallocate segment list */
  h->ht = sg_new(mrb);
  /* capacity ignored */
  h->iv = 0;
  return obj_value(h);
}

static value hash_default(state *mrb, value hash);
static value hash_default(state *mrb, value hash, value key);

static value
hash_init_copy(state *mrb, value self)
{
  value orig;
  struct RHash* copy;
  seglist *orig_h;
  value ifnone, vret;

  get_args(mrb, "o", &orig);
  if (obj_equal(mrb, self, orig)) return self;
  if ((type(self) != type(orig)) || (obj_class(mrb, self) != obj_class(mrb, orig))) {
      raise(mrb, E_TYPE_ERROR, "initialize_copy should take same class object");
  }

  orig_h = RHASH_TBL(self);
  copy = (struct RHash*)obj_alloc(mrb, TT_HASH, mrb->hash_class);
  copy->ht = sg_copy(mrb, orig_h);

  if (RHASH_DEFAULT_P(self)) {
    copy->flags |= HASH_DEFAULT;
  }
  if (RHASH_PROCDEFAULT_P(self)) {
    copy->flags |= HASH_PROC_DEFAULT;
  }
  vret = obj_value(copy);
  ifnone = RHASH_IFNONE(self);
  if (!nil_p(ifnone)) {
      iv_set(mrb, vret, intern_lit(mrb, "ifnone"), ifnone);
  }
  return vret;
}

static int
check_kdict_i(state *mrb, value key, value val, void *data)
{
  if (!symbol_p(key)) {
    raise(mrb, E_ARGUMENT_ERROR, "keyword argument hash with non symbol keys");
  }
  return 0;
}

void
hash_check_kdict(state *mrb, value self)
{
  seglist *sg;

  sg = RHASH_TBL(self);
  if (!sg || sg_size(mrb, sg) == 0) return;
  sg_foreach(mrb, sg, check_kdict_i, NULL);
}

API value
hash_dup(state *mrb, value self)
{
  struct RHash* copy;
  seglist *orig_h;

  orig_h = RHASH_TBL(self);
  copy = (struct RHash*)obj_alloc(mrb, TT_HASH, mrb->hash_class);
  copy->ht = orig_h ? sg_copy(mrb, orig_h) : NULL;
  return obj_value(copy);
}

API value
hash_get(state *mrb, value hash, value key)
{
  value val;
  sym mid;

  if (sg_get(mrb, RHASH_TBL(hash), key, &val)) {
    return val;
  }

  mid = intern_lit(mrb, "default");
  if (func_basic_p(mrb, hash, mid, hash_default)) {
    return hash_default(mrb, hash, key);
  }
  /* xxx funcall_tailcall(mrb, hash, "default", 1, key); */
  return funcall_argv(mrb, hash, mid, 1, &key);
}

API value
hash_fetch(state *mrb, value hash, value key, value def)
{
  value val;

  if (sg_get(mrb, RHASH_TBL(hash), key, &val)) {
    return val;
  }
  /* not found */
  return def;
}

API void
hash_set(state *mrb, value hash, value key, value val)
{
  hash_modify(mrb, hash);

  key = KEY(key);
  sg_put(mrb, RHASH_TBL(hash), key, val);
  field_write_barrier_value(mrb, (struct RBasic*)RHASH(hash), key);
  field_write_barrier_value(mrb, (struct RBasic*)RHASH(hash), val);
  return;
}

API value
ensure_hash_type(state *mrb, value hash)
{
  return convert_type(mrb, hash, TT_HASH, "Hash", "to_hash");
}

API value
check_hash_type(state *mrb, value hash)
{
  return check_convert_type(mrb, hash, TT_HASH, "Hash", "to_hash");
}

static void
hash_modify(state *mrb, value hash)
{
  if (FROZEN_P(hash_ptr(hash))) {
    raise(mrb, E_FROZEN_ERROR, "can't modify frozen hash");
  }

  if (!RHASH_TBL(hash)) {
    RHASH_TBL(hash) = sg_new(mrb);
  }
}

/* 15.2.13.4.16 */
/*
 *  call-seq:
 *     Hash.new                          -> new_hash
 *     Hash.new(obj)                     -> new_hash
 *     Hash.new {|hash, key| block }     -> new_hash
 *
 *  Returns a new, empty hash. If this hash is subsequently accessed by
 *  a key that doesn't correspond to a hash entry, the value returned
 *  depends on the style of <code>new</code> used to create the hash. In
 *  the first form, the access returns <code>nil</code>. If
 *  <i>obj</i> is specified, this single object will be used for
 *  all <em>default values</em>. If a block is specified, it will be
 *  called with the hash object and the key, and should return the
 *  default value. It is the block's responsibility to store the value
 *  in the hash if required.
 *
 *      h = Hash.new("Go Fish")
 *      h["a"] = 100
 *      h["b"] = 200
 *      h["a"]           #=> 100
 *      h["c"]           #=> "Go Fish"
 *      # The following alters the single default object
 *      h["c"].upcase!   #=> "GO FISH"
 *      h["d"]           #=> "GO FISH"
 *      h.keys           #=> ["a", "b"]
 *
 *      # While this creates a new default object each time
 *      h = Hash.new { |hash, key| hash[key] = "Go Fish: #{key}" }
 *      h["c"]           #=> "Go Fish: c"
 *      h["c"].upcase!   #=> "GO FISH: C"
 *      h["d"]           #=> "Go Fish: d"
 *      h.keys           #=> ["c", "d"]
 *
 */

static value
hash_init(state *mrb, value hash)
{
  value block, ifnone;
  bool ifnone_p;

  ifnone = nil_value();
  get_args(mrb, "&|o?", &block, &ifnone, &ifnone_p);
  hash_modify(mrb, hash);
  if (!nil_p(block)) {
    if (ifnone_p) {
      raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
    }
    RHASH(hash)->flags |= HASH_PROC_DEFAULT;
    ifnone = block;
  }
  if (!nil_p(ifnone)) {
    RHASH(hash)->flags |= HASH_DEFAULT;
    iv_set(mrb, hash, intern_lit(mrb, "ifnone"), ifnone);
  }
  return hash;
}

/* 15.2.13.4.2  */
/*
 *  call-seq:
 *     hsh[key]    ->  value
 *
 *  Element Reference---Retrieves the <i>value</i> object corresponding
 *  to the <i>key</i> object. If not found, returns the default value (see
 *  <code>Hash::new</code> for details).
 *
 *     h = { "a" => 100, "b" => 200 }
 *     h["a"]   #=> 100
 *     h["c"]   #=> nil
 *
 */
static value
hash_aget(state *mrb, value self)
{
  value key;

  get_args(mrb, "o", &key);
  return hash_get(mrb, self, key);
}

static value
hash_default(state *mrb, value hash, value key)
{
  if (RHASH_DEFAULT_P(hash)) {
    if (RHASH_PROCDEFAULT_P(hash)) {
      return funcall(mrb, RHASH_PROCDEFAULT(hash), "call", 2, hash, key);
    }
    else {
      return RHASH_IFNONE(hash);
    }
  }
  return nil_value();
}

/* 15.2.13.4.5  */
/*
 *  call-seq:
 *     hsh.default(key=nil)   -> obj
 *
 *  Returns the default value, the value that would be returned by
 *  <i>hsh</i>[<i>key</i>] if <i>key</i> did not exist in <i>hsh</i>.
 *  See also <code>Hash::new</code> and <code>Hash#default=</code>.
 *
 *     h = Hash.new                            #=> {}
 *     h.default                               #=> nil
 *     h.default(2)                            #=> nil
 *
 *     h = Hash.new("cat")                     #=> {}
 *     h.default                               #=> "cat"
 *     h.default(2)                            #=> "cat"
 *
 *     h = Hash.new {|h,k| h[k] = k.to_i*10}   #=> {}
 *     h.default                               #=> nil
 *     h.default(2)                            #=> 20
 */

static value
hash_default(state *mrb, value hash)
{
  value key;
  bool given;

  get_args(mrb, "|o?", &key, &given);
  if (RHASH_DEFAULT_P(hash)) {
    if (RHASH_PROCDEFAULT_P(hash)) {
      if (!given) return nil_value();
      return funcall(mrb, RHASH_PROCDEFAULT(hash), "call", 2, hash, key);
    }
    else {
      return RHASH_IFNONE(hash);
    }
  }
  return nil_value();
}

/* 15.2.13.4.6  */
/*
 *  call-seq:
 *     hsh.default = obj     -> obj
 *
 *  Sets the default value, the value returned for a key that does not
 *  exist in the hash. It is not possible to set the default to a
 *  <code>Proc</code> that will be executed on each key lookup.
 *
 *     h = { "a" => 100, "b" => 200 }
 *     h.default = "Go fish"
 *     h["a"]     #=> 100
 *     h["z"]     #=> "Go fish"
 *     # This doesn't do what you might hope...
 *     h.default = proc do |hash, key|
 *       hash[key] = key + key
 *     end
 *     h[2]       #=> #<Proc:0x401b3948@-:6>
 *     h["cat"]   #=> #<Proc:0x401b3948@-:6>
 */

static value
hash_set_default(state *mrb, value hash)
{
  value ifnone;

  get_args(mrb, "o", &ifnone);
  hash_modify(mrb, hash);
  iv_set(mrb, hash, intern_lit(mrb, "ifnone"), ifnone);
  RHASH(hash)->flags &= ~HASH_PROC_DEFAULT;
  if (!nil_p(ifnone)) {
    RHASH(hash)->flags |= HASH_DEFAULT;
  }
  else {
    RHASH(hash)->flags &= ~HASH_DEFAULT;
  }
  return ifnone;
}

/* 15.2.13.4.7  */
/*
 *  call-seq:
 *     hsh.default_proc -> anObject
 *
 *  If <code>Hash::new</code> was invoked with a block, return that
 *  block, otherwise return <code>nil</code>.
 *
 *     h = Hash.new {|h,k| h[k] = k*k }   #=> {}
 *     p = h.default_proc                 #=> #<Proc:0x401b3d08@-:1>
 *     a = []                             #=> []
 *     p.call(a, 2)
 *     a                                  #=> [nil, nil, 4]
 */


static value
hash_default_proc(state *mrb, value hash)
{
  if (RHASH_PROCDEFAULT_P(hash)) {
    return RHASH_PROCDEFAULT(hash);
  }
  return nil_value();
}

/*
 *  call-seq:
 *     hsh.default_proc = proc_obj     -> proc_obj
 *
 *  Sets the default proc to be executed on each key lookup.
 *
 *     h.default_proc = proc do |hash, key|
 *       hash[key] = key + key
 *     end
 *     h[2]       #=> 4
 *     h["cat"]   #=> "catcat"
 */

static value
hash_set_default_proc(state *mrb, value hash)
{
  value ifnone;

  get_args(mrb, "o", &ifnone);
  hash_modify(mrb, hash);
  iv_set(mrb, hash, intern_lit(mrb, "ifnone"), ifnone);
  if (!nil_p(ifnone)) {
    RHASH(hash)->flags |= HASH_PROC_DEFAULT;
    RHASH(hash)->flags |= HASH_DEFAULT;
  }
  else {
    RHASH(hash)->flags &= ~HASH_DEFAULT;
    RHASH(hash)->flags &= ~HASH_PROC_DEFAULT;
  }

  return ifnone;
}

API value
hash_delete_key(state *mrb, value hash, value key)
{
  seglist *sg = RHASH_TBL(hash);
  value del_val;

  if (sg_del(mrb, sg, key, &del_val)) {
    return del_val;
  }

  /* not found */
  return nil_value();
}

static value
hash_delete(state *mrb, value self)
{
  value key;

  get_args(mrb, "o", &key);
  hash_modify(mrb, self);
  return hash_delete_key(mrb, self, key);
}

/* find first element in segment list, and remove it. */
static void
sg_shift(state *mrb, seglist *t, value *kp, value *vp)
{
  segment *seg = t->rootseg;
  int i;

  while (seg) {
    for (i=0; i<SG_SEGMENT_SIZE; i++) {
      value key;

      if (!seg->next && i >= t->last_len) {
        return;
      }
      key = seg->e[i].key;
      if (undef_p(key)) continue;
      *kp = key;
      *vp = seg->e[i].val;
      /* delete element */
      seg->e[i].key = undef_value();
      t->size--;
      return;
    }
    seg = seg->next;
  }
}

/* 15.2.13.4.24 */
/*
 *  call-seq:
 *     hsh.shift -> anArray or obj
 *
 *  Removes a key-value pair from <i>hsh</i> and returns it as the
 *  two-item array <code>[</code> <i>key, value</i> <code>]</code>, or
 *  the hash's default value if the hash is empty.
 *
 *      h = { 1 => "a", 2 => "b", 3 => "c" }
 *      h.shift   #=> [1, "a"]
 *      h         #=> {2=>"b", 3=>"c"}
 */

static value
hash_shift(state *mrb, value hash)
{
  seglist *sg = RHASH_TBL(hash);

  hash_modify(mrb, hash);
  if (sg && sg_size(mrb, sg) > 0) {
    value del_key, del_val;

    sg_shift(mrb, sg, &del_key, &del_val);
    return assoc_new(mrb, del_key, del_val);
  }

  if (RHASH_DEFAULT_P(hash)) {
    if (RHASH_PROCDEFAULT_P(hash)) {
      return funcall(mrb, RHASH_PROCDEFAULT(hash), "call", 2, hash, nil_value());
    }
    else {
      return RHASH_IFNONE(hash);
    }
  }
  return nil_value();
}

/* 15.2.13.4.4  */
/*
 *  call-seq:
 *     hsh.clear -> hsh
 *
 *  Removes all key-value pairs from `hsh`.
 *
 *      h = { "a" => 100, "b" => 200 }   #=> {"a"=>100, "b"=>200}
 *      h.clear                          #=> {}
 *
 */

API value
hash_clear(state *mrb, value hash)
{
  seglist *sg = RHASH_TBL(hash);

  hash_modify(mrb, hash);
  if (sg) {
    sg_free(mrb, sg);
    RHASH_TBL(hash) = NULL;
  }
  return hash;
}

/* 15.2.13.4.3  */
/* 15.2.13.4.26 */
/*
 *  call-seq:
 *     hsh[key] = value        -> value
 *     hsh.store(key, value)   -> value
 *
 *  Element Assignment---Associates the value given by
 *  <i>value</i> with the key given by <i>key</i>.
 *  <i>key</i> should not have its value changed while it is in
 *  use as a key (a <code>String</code> passed as a key will be
 *  duplicated and frozen).
 *
 *      h = { "a" => 100, "b" => 200 }
 *      h["a"] = 9
 *      h["c"] = 4
 *      h   #=> {"a"=>9, "b"=>200, "c"=>4}
 *
 */
static value
hash_aset(state *mrb, value self)
{
  value key, val;

  get_args(mrb, "oo", &key, &val);
  hash_set(mrb, self, key, val);
  return val;
}

/* 15.2.13.4.20 */
/* 15.2.13.4.25 */
/*
 *  call-seq:
 *     hsh.length    ->  fixnum
 *     hsh.size      ->  fixnum
 *
 *  Returns the number of key-value pairs in the hash.
 *
 *     h = { "d" => 100, "a" => 200, "v" => 300, "e" => 400 }
 *     h.length        #=> 4
 *     h.delete("a")   #=> 200
 *     h.length        #=> 3
 */
static value
hash_size_m(state *mrb, value self)
{
  seglist *sg = RHASH_TBL(self);

  if (!sg) return fixnum_value(0);
  return fixnum_value(sg_size(mrb, sg));
}

API bool
hash_empty_p(state *mrb, value self)
{
  seglist *sg = RHASH_TBL(self);

  if (!sg) return TRUE;
  return sg_size(mrb, sg) == 0;
}

/* 15.2.13.4.12 */
/*
 *  call-seq:
 *     hsh.empty?    -> true or false
 *
 *  Returns <code>true</code> if <i>hsh</i> contains no key-value pairs.
 *
 *     {}.empty?   #=> true
 *
 */
static value
hash_empty_m(state *mrb, value self)
{
  return bool_value(hash_empty_p(mrb, self));
}

/* 15.2.13.4.29 (x)*/
/*
 * call-seq:
 *    hsh.to_hash   => hsh
 *
 * Returns +self+.
 */

static value
hash_to_hash(state *mrb, value hash)
{
  return hash;
}

static int
hash_keys_i(state *mrb, value key, value val, void *p)
{
  ary_push(mrb, *(value*)p, key);
  return 0;
}

/* 15.2.13.4.19 */
/*
 *  call-seq:
 *     hsh.keys    -> array
 *
 *  Returns a new array populated with the keys from this hash. See also
 *  <code>Hash#values</code>.
 *
 *     h = { "a" => 100, "b" => 200, "c" => 300, "d" => 400 }
 *     h.keys   #=> ["a", "b", "c", "d"]
 *
 */

API value
hash_keys(state *mrb, value hash)
{
  seglist *sg = RHASH_TBL(hash);
  size_t size;
  value ary;

  if (!sg || (size = sg_size(mrb, sg)) == 0)
    return ary_new(mrb);
  ary = a_ary_new_capa(mrb, size);
  sg_foreach(mrb, sg, hash_keys_i, (void*)&ary);
  return ary;
}

static int
hash_vals_i(state *mrb, value key, value val, void *p)
{
  ary_push(mrb, *(value*)p, val);
  return 0;
}

/* 15.2.13.4.28 */
/*
 *  call-seq:
 *     hsh.values    -> array
 *
 *  Returns a new array populated with the values from <i>hsh</i>. See
 *  also <code>Hash#keys</code>.
 *
 *     h = { "a" => 100, "b" => 200, "c" => 300 }
 *     h.values   #=> [100, 200, 300]
 *
 */

API value
hash_values(state *mrb, value hash)
{
  seglist *sg = RHASH_TBL(hash);
  size_t size;
  value ary;

  if (!sg || (size = sg_size(mrb, sg)) == 0)
    return ary_new(mrb);
  ary = a_ary_new_capa(mrb, size);
  sg_foreach(mrb, sg, hash_vals_i, (void*)&ary);
  return ary;
}

/* 15.2.13.4.13 */
/* 15.2.13.4.15 */
/* 15.2.13.4.18 */
/* 15.2.13.4.21 */
/*
 *  call-seq:
 *     hsh.has_key?(key)    -> true or false
 *     hsh.include?(key)    -> true or false
 *     hsh.key?(key)        -> true or false
 *     hsh.member?(key)     -> true or false
 *
 *  Returns <code>true</code> if the given key is present in <i>hsh</i>.
 *
 *     h = { "a" => 100, "b" => 200 }
 *     h.has_key?("a")   #=> true
 *     h.has_key?("z")   #=> false
 *
 */

API bool
hash_key_p(state *mrb, value hash, value key)
{
  seglist *sg;

  sg = RHASH_TBL(hash);
  if (sg_get(mrb, sg, key, NULL)) {
    return TRUE;
  }
  return FALSE;
}

static value
hash_has_key(state *mrb, value hash)
{
  value key;
  bool key_p;

  get_args(mrb, "o", &key);
  key_p = hash_key_p(mrb, hash, key);
  return bool_value(key_p);
}

struct has_v_arg {
  bool found;
  value val;
};

static int
hash_has_value_i(state *mrb, value key, value val, void *p)
{
  struct has_v_arg *arg = (struct has_v_arg*)p;
  
  if (equal(mrb, arg->val, val)) {
    arg->found = TRUE;
    return 1;
  }
  return 0;
}

/* 15.2.13.4.14 */
/* 15.2.13.4.27 */
/*
 *  call-seq:
 *     hsh.has_value?(value)    -> true or false
 *     hsh.value?(value)        -> true or false
 *
 *  Returns <code>true</code> if the given value is present for some key
 *  in <i>hsh</i>.
 *
 *     h = { "a" => 100, "b" => 200 }
 *     h.has_value?(100)   #=> true
 *     h.has_value?(999)   #=> false
 */

static value
hash_has_value(state *mrb, value hash)
{
  value val;
  struct has_v_arg arg;
  
  get_args(mrb, "o", &val);
  arg.found = FALSE;
  arg.val = val;
  sg_foreach(mrb, RHASH_TBL(hash), hash_has_value_i, &arg);
  return bool_value(arg.found);
}

static int
merge_i(state *mrb, value key, value val, void *data)
{
  seglist *h1 = (seglist*)data;

  sg_put(mrb, h1, key, val);
  return 0;
}

API void
hash_merge(state *mrb, value hash1, value hash2)
{
  seglist *h1, *h2;

  hash_modify(mrb, hash1);
  hash2 = ensure_hash_type(mrb, hash2);
  h1 = RHASH_TBL(hash1);
  h2 = RHASH_TBL(hash2);

  if (!h2) return;
  if (!h1) {
    RHASH_TBL(hash1) = sg_copy(mrb, h2);
    return;
  }
  sg_foreach(mrb, h2, merge_i, h1);
  write_barrier(mrb, (struct RBasic*)RHASH(hash1));
  return;
}

/*
 *  call-seq:
 *    hsh.rehash -> hsh
 *
 *  Rebuilds the hash based on the current hash values for each key. If
 *  values of key objects have changed since they were inserted, this
 *  method will reindex <i>hsh</i>.
 *
 *     h = {"AAA" => "b"}
 *     h.keys[0].chop!
 *     h.rehash   #=> {"AA"=>"b"}
 *     h["AA"]    #=> "b"
 */
static value
hash_rehash(state *mrb, value self)
{
  sg_compact(mrb, RHASH_TBL(self));
  return self;
}

void
init_hash(state *mrb)
{
  struct RClass *h;

  mrb->hash_class = h = define_class(mrb, "Hash", mrb->object_class);              /* 15.2.13 */
  SET_INSTANCE_TT(h, TT_HASH);

  define_method(mrb, h, "initialize_copy", hash_init_copy,   ARGS_REQ(1));
  define_method(mrb, h, "[]",              hash_aget,        ARGS_REQ(1)); /* 15.2.13.4.2  */
  define_method(mrb, h, "[]=",             hash_aset,        ARGS_REQ(2)); /* 15.2.13.4.3  */
  define_method(mrb, h, "clear",           hash_clear,       ARGS_NONE()); /* 15.2.13.4.4  */
  define_method(mrb, h, "default",         hash_default,     ARGS_ANY());  /* 15.2.13.4.5  */
  define_method(mrb, h, "default=",        hash_set_default, ARGS_REQ(1)); /* 15.2.13.4.6  */
  define_method(mrb, h, "default_proc",    hash_default_proc,ARGS_NONE()); /* 15.2.13.4.7  */
  define_method(mrb, h, "default_proc=",   hash_set_default_proc,ARGS_REQ(1)); /* 15.2.13.4.7  */
  define_method(mrb, h, "__delete",        hash_delete,      ARGS_REQ(1)); /* core of 15.2.13.4.8  */
  define_method(mrb, h, "empty?",          hash_empty_m,     ARGS_NONE()); /* 15.2.13.4.12 */
  define_method(mrb, h, "has_key?",        hash_has_key,     ARGS_REQ(1)); /* 15.2.13.4.13 */
  define_method(mrb, h, "has_value?",      hash_has_value,   ARGS_REQ(1)); /* 15.2.13.4.14 */
  define_method(mrb, h, "include?",        hash_has_key,     ARGS_REQ(1)); /* 15.2.13.4.15 */
  define_method(mrb, h, "initialize",      hash_init,        ARGS_OPT(1)); /* 15.2.13.4.16 */
  define_method(mrb, h, "key?",            hash_has_key,     ARGS_REQ(1)); /* 15.2.13.4.18 */
  define_method(mrb, h, "keys",            hash_keys,        ARGS_NONE()); /* 15.2.13.4.19 */
  define_method(mrb, h, "length",          hash_size_m,      ARGS_NONE()); /* 15.2.13.4.20 */
  define_method(mrb, h, "member?",         hash_has_key,     ARGS_REQ(1)); /* 15.2.13.4.21 */
  define_method(mrb, h, "shift",           hash_shift,       ARGS_NONE()); /* 15.2.13.4.24 */
  define_method(mrb, h, "size",            hash_size_m,      ARGS_NONE()); /* 15.2.13.4.25 */
  define_method(mrb, h, "store",           hash_aset,        ARGS_REQ(2)); /* 15.2.13.4.26 */
  define_method(mrb, h, "value?",          hash_has_value,   ARGS_REQ(1)); /* 15.2.13.4.27 */
  define_method(mrb, h, "values",          hash_values,      ARGS_NONE()); /* 15.2.13.4.28 */
  define_method(mrb, h, "rehash",          hash_rehash,      ARGS_NONE());

  define_method(mrb, h, "to_hash",         hash_to_hash,     ARGS_NONE()); /* 15.2.13.4.29 (x)*/
}
