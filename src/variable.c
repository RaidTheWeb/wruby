/*
** variable.c - mruby variables
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/proc.h>
#include <mruby/string.h>

typedef int (iv_foreach_func)($state*,$sym,$value,void*);

#ifndef MRB_IV_SEGMENT_SIZE
#define MRB_IV_SEGMENT_SIZE 4
#endif

typedef struct segment {
  $sym key[MRB_IV_SEGMENT_SIZE];
  $value val[MRB_IV_SEGMENT_SIZE];
  struct segment *next;
} segment;

/* Instance variable table structure */
typedef struct iv_tbl {
  segment *rootseg;
  size_t size;
  size_t last_len;
} iv_tbl;

/* Creates the instance variable table. */
static iv_tbl*
iv_new($state *mrb)
{
  iv_tbl *t;

  t = (iv_tbl*)$malloc(mrb, sizeof(iv_tbl));
  t->size = 0;
  t->rootseg =  NULL;
  t->last_len = 0;

  return t;
}

/* Set the value for the symbol in the instance variable table. */
static void
iv_put($state *mrb, iv_tbl *t, $sym sym, $value val)
{
  segment *seg;
  segment *prev = NULL;
  segment *matched_seg = NULL;
  size_t matched_idx = 0;
  size_t i;

  if (t == NULL) return;
  seg = t->rootseg;
  while (seg) {
    for (i=0; i<MRB_IV_SEGMENT_SIZE; i++) {
      $sym key = seg->key[i];
      /* Found room in last segment after last_len */
      if (!seg->next && i >= t->last_len) {
        seg->key[i] = sym;
        seg->val[i] = val;
        t->last_len = i+1;
        t->size++;
        return;
      }
      if (!matched_seg && key == 0) {
        matched_seg = seg;
        matched_idx = i;
      }
      else if (key == sym) {
        seg->val[i] = val;
        return;
      }
    }
    prev = seg;
    seg = seg->next;
  }

  /* Not found */
  t->size++;
  if (matched_seg) {
    matched_seg->key[matched_idx] = sym;
    matched_seg->val[matched_idx] = val;
    return;
  }

  seg = (segment*)$malloc(mrb, sizeof(segment));
  if (!seg) return;
  seg->next = NULL;
  seg->key[0] = sym;
  seg->val[0] = val;
  t->last_len = 1;
  if (prev) {
    prev->next = seg;
  }
  else {
    t->rootseg = seg;
  }
}

/* Get a value for a symbol from the instance variable table. */
static $bool
iv_get($state *mrb, iv_tbl *t, $sym sym, $value *vp)
{
  segment *seg;
  size_t i;

  if (t == NULL) return FALSE;
  seg = t->rootseg;
  while (seg) {
    for (i=0; i<MRB_IV_SEGMENT_SIZE; i++) {
      $sym key = seg->key[i];

      if (!seg->next && i >= t->last_len) {
        return FALSE;
      }
      if (key == sym) {
        if (vp) *vp = seg->val[i];
        return TRUE;
      }
    }
    seg = seg->next;
  }
  return FALSE;
}

/* Deletes the value for the symbol from the instance variable table. */
static $bool
iv_del($state *mrb, iv_tbl *t, $sym sym, $value *vp)
{
  segment *seg;
  size_t i;

  if (t == NULL) return FALSE;
  seg = t->rootseg;
  while (seg) {
    for (i=0; i<MRB_IV_SEGMENT_SIZE; i++) {
      $sym key = seg->key[i];

      if (!seg->next && i >= t->last_len) {
        return FALSE;
      }
      if (key == sym) {
        t->size--;
        seg->key[i] = 0;
        if (vp) *vp = seg->val[i];
        return TRUE;
      }
    }
    seg = seg->next;
  }
  return FALSE;
}

/* Iterates over the instance variable table. */
static $bool
iv_foreach($state *mrb, iv_tbl *t, iv_foreach_func *func, void *p)
{
  segment *seg;
  size_t i;
  int n;

  if (t == NULL) return TRUE;
  seg = t->rootseg;
  while (seg) {
    for (i=0; i<MRB_IV_SEGMENT_SIZE; i++) {
      $sym key = seg->key[i];

      /* no value in last segment after last_len */
      if (!seg->next && i >= t->last_len) {
        return FALSE;
      }
      if (key != 0) {
        n =(*func)(mrb, key, seg->val[i], p);
        if (n > 0) return FALSE;
        if (n < 0) {
          t->size--;
          seg->key[i] = 0;
        }
      }
    }
    seg = seg->next;
  }
  return TRUE;
}

/* Get the size of the instance variable table. */
static size_t
iv_size($state *mrb, iv_tbl *t)
{
  segment *seg;
  size_t size = 0;

  if (t == NULL) return 0;
  if (t->size > 0) return t->size;
  seg = t->rootseg;
  while (seg) {
    if (seg->next == NULL) {
      size += t->last_len;
      return size;
    }
    seg = seg->next;
    size += MRB_IV_SEGMENT_SIZE;
  }
  /* empty iv_tbl */
  return 0;
}

/* Copy the instance variable table. */
static iv_tbl*
iv_copy($state *mrb, iv_tbl *t)
{
  segment *seg;
  iv_tbl *t2;

  size_t i;

  seg = t->rootseg;
  t2 = iv_new(mrb);

  while (seg != NULL) {
    for (i=0; i<MRB_IV_SEGMENT_SIZE; i++) {
      $sym key = seg->key[i];
      $value val = seg->val[i];

      if ((seg->next == NULL) && (i >= t->last_len)) {
        return t2;
      }
      iv_put(mrb, t2, key, val);
    }
    seg = seg->next;
  }
  return t2;
}

/* Free memory of the instance variable table. */
static void
iv_free($state *mrb, iv_tbl *t)
{
  segment *seg;

  seg = t->rootseg;
  while (seg) {
    segment *p = seg;
    seg = seg->next;
    $free(mrb, p);
  }
  $free(mrb, t);
}

static int
iv_mark_i($state *mrb, $sym sym, $value v, void *p)
{
  $gc_mark_value(mrb, v);
  return 0;
}

static void
mark_tbl($state *mrb, iv_tbl *t)
{
  iv_foreach(mrb, t, iv_mark_i, 0);
}

void
$gc_mark_gv($state *mrb)
{
  mark_tbl(mrb, mrb->globals);
}

void
$gc_free_gv($state *mrb)
{
  if (mrb->globals)
    iv_free(mrb, mrb->globals);
}

void
$gc_mark_iv($state *mrb, struct RObject *obj)
{
  mark_tbl(mrb, obj->iv);
}

size_t
$gc_mark_iv_size($state *mrb, struct RObject *obj)
{
  return iv_size(mrb, obj->iv);
}

void
$gc_free_iv($state *mrb, struct RObject *obj)
{
  if (obj->iv) {
    iv_free(mrb, obj->iv);
  }
}

$value
$vm_special_get($state *mrb, $sym i)
{
  return $fixnum_value(0);
}

void
$vm_special_set($state *mrb, $sym i, $value v)
{
}

static $bool
obj_iv_p($value obj)
{
  switch ($type(obj)) {
    case MRB_TT_OBJECT:
    case MRB_TT_CLASS:
    case MRB_TT_MODULE:
    case MRB_TT_SCLASS:
    case MRB_TT_HASH:
    case MRB_TT_DATA:
    case MRB_TT_EXCEPTION:
      return TRUE;
    default:
      return FALSE;
  }
}

MRB_API $value
$obj_iv_get($state *mrb, struct RObject *obj, $sym sym)
{
  $value v;

  if (obj->iv && iv_get(mrb, obj->iv, sym, &v))
    return v;
  return $nil_value();
}

MRB_API $value
$iv_get($state *mrb, $value obj, $sym sym)
{
  if (obj_iv_p(obj)) {
    return $obj_iv_get(mrb, $obj_ptr(obj), sym);
  }
  return $nil_value();
}

static inline void assign_class_name($state *mrb, struct RObject *obj, $sym sym, $value v);

MRB_API void
$obj_iv_set($state *mrb, struct RObject *obj, $sym sym, $value v)
{
  iv_tbl *t;

  if (MRB_FROZEN_P(obj)) {
    $raisef(mrb, E_FROZEN_ERROR, "can't modify frozen %S", $obj_value(obj));
  }
  assign_class_name(mrb, obj, sym, v);
  if (!obj->iv) {
    obj->iv = iv_new(mrb);
  }
  t = obj->iv;
  iv_put(mrb, t, sym, v);
  $write_barrier(mrb, (struct RBasic*)obj);
}

static inline $bool
namespace_p(enum $vtype tt)
{
  return tt == MRB_TT_CLASS || tt == MRB_TT_MODULE ? TRUE : FALSE;
}

static inline void
assign_class_name($state *mrb, struct RObject *obj, $sym sym, $value v)
{
  if (namespace_p(obj->tt) && namespace_p($type(v))) {
    struct RObject *c = $obj_ptr(v);
    if (obj != c && ISUPPER($sym2name(mrb, sym)[0])) {
      $sym id_classname = $intern_lit(mrb, "__classname__");
      $value o = $obj_iv_get(mrb, c, id_classname);

      if ($nil_p(o)) {
        $sym id_outer = $intern_lit(mrb, "__outer__");
        o = $obj_iv_get(mrb, c, id_outer);

        if ($nil_p(o)) {
          if ((struct RClass *)obj == mrb->object_class) {
            $obj_iv_set(mrb, c, id_classname, $symbol_value(sym));
          }
          else {
            $obj_iv_set(mrb, c, id_outer, $obj_value(obj));
          }
        }
      }
    }
  }
}

MRB_API void
$iv_set($state *mrb, $value obj, $sym sym, $value v)
{
  if (obj_iv_p(obj)) {
    $obj_iv_set(mrb, $obj_ptr(obj), sym, v);
  }
  else {
    $raise(mrb, E_ARGUMENT_ERROR, "cannot set instance variable");
  }
}

MRB_API $bool
$obj_iv_defined($state *mrb, struct RObject *obj, $sym sym)
{
  iv_tbl *t;

  t = obj->iv;
  if (t) {
    return iv_get(mrb, t, sym, NULL);
  }
  return FALSE;
}

MRB_API $bool
$iv_defined($state *mrb, $value obj, $sym sym)
{
  if (!obj_iv_p(obj)) return FALSE;
  return $obj_iv_defined(mrb, $obj_ptr(obj), sym);
}

#define identchar(c) (ISALNUM(c) || (c) == '_' || !ISASCII(c))

MRB_API $bool
$iv_name_sym_p($state *mrb, $sym iv_name)
{
  const char *s;
  $int i, len;

  s = $sym2name_len(mrb, iv_name, &len);
  if (len < 2) return FALSE;
  if (s[0] != '@') return FALSE;
  if (s[1] == '@') return FALSE;
  for (i=1; i<len; i++) {
    if (!identchar(s[i])) return FALSE;
  }
  return TRUE;
}

MRB_API void
$iv_name_sym_check($state *mrb, $sym iv_name)
{
  if (!$iv_name_sym_p(mrb, iv_name)) {
    $name_error(mrb, iv_name, "'%S' is not allowed as an instance variable name", $sym2str(mrb, iv_name));
  }
}

MRB_API void
$iv_copy($state *mrb, $value dest, $value src)
{
  struct RObject *d = $obj_ptr(dest);
  struct RObject *s = $obj_ptr(src);

  if (d->iv) {
    iv_free(mrb, d->iv);
    d->iv = 0;
  }
  if (s->iv) {
    $write_barrier(mrb, (struct RBasic*)d);
    d->iv = iv_copy(mrb, s->iv);
  }
}

static int
inspect_i($state *mrb, $sym sym, $value v, void *p)
{
  $value str = *($value*)p;
  const char *s;
  $int len;
  $value ins;
  char *sp = RSTRING_PTR(str);

  /* need not to show internal data */
  if (sp[0] == '-') { /* first element */
    sp[0] = '#';
    $str_cat_lit(mrb, str, " ");
  }
  else {
    $str_cat_lit(mrb, str, ", ");
  }
  s = $sym2name_len(mrb, sym, &len);
  $str_cat(mrb, str, s, len);
  $str_cat_lit(mrb, str, "=");
  if ($type(v) == MRB_TT_OBJECT) {
    ins = $any_to_s(mrb, v);
  }
  else {
    ins = $inspect(mrb, v);
  }
  $str_cat_str(mrb, str, ins);
  return 0;
}

$value
$obj_iv_inspect($state *mrb, struct RObject *obj)
{
  iv_tbl *t = obj->iv;
  size_t len = iv_size(mrb, t);

  if (len > 0) {
    const char *cn = $obj_classname(mrb, $obj_value(obj));
    $value str = $str_new_capa(mrb, 30);

    $str_cat_lit(mrb, str, "-<");
    $str_cat_cstr(mrb, str, cn);
    $str_cat_lit(mrb, str, ":");
    $str_concat(mrb, str, $ptr_to_str(mrb, obj));

    iv_foreach(mrb, t, inspect_i, &str);
    $str_cat_lit(mrb, str, ">");
    return str;
  }
  return $any_to_s(mrb, $obj_value(obj));
}

MRB_API $value
$iv_remove($state *mrb, $value obj, $sym sym)
{
  if (obj_iv_p(obj)) {
    iv_tbl *t = $obj_ptr(obj)->iv;
    $value val;

    if (iv_del(mrb, t, sym, &val)) {
      return val;
    }
  }
  return $undef_value();
}

static int
iv_i($state *mrb, $sym sym, $value v, void *p)
{
  $value ary;
  const char* s;
  $int len;

  ary = *($value*)p;
  s = $sym2name_len(mrb, sym, &len);
  if (len > 1 && s[0] == '@' && s[1] != '@') {
    $ary_push(mrb, ary, $symbol_value(sym));
  }
  return 0;
}

/* 15.3.1.3.23 */
/*
 *  call-seq:
 *     obj.instance_variables    -> array
 *
 *  Returns an array of instance variable names for the receiver. Note
 *  that simply defining an accessor does not create the corresponding
 *  instance variable.
 *
 *     class Fred
 *       attr_accessor :a1
 *       def initialize
 *         @iv = 3
 *       end
 *     end
 *     Fred.new.instance_variables   #=> [:@iv]
 */
$value
$obj_instance_variables($state *mrb, $value self)
{
  $value ary;

  ary = $ary_new(mrb);
  if (obj_iv_p(self)) {
    iv_foreach(mrb, $obj_ptr(self)->iv, iv_i, &ary);
  }
  return ary;
}

static int
cv_i($state *mrb, $sym sym, $value v, void *p)
{
  $value ary;
  const char* s;
  $int len;

  ary = *($value*)p;
  s = $sym2name_len(mrb, sym, &len);
  if (len > 2 && s[0] == '@' && s[1] == '@') {
    $ary_push(mrb, ary, $symbol_value(sym));
  }
  return 0;
}

/* 15.2.2.4.19 */
/*
 *  call-seq:
 *     mod.class_variables   -> array
 *
 *  Returns an array of the names of class variables in <i>mod</i>.
 *
 *     class One
 *       @@var1 = 1
 *     end
 *     class Two < One
 *       @@var2 = 2
 *     end
 *     One.class_variables   #=> [:@@var1]
 *     Two.class_variables   #=> [:@@var2]
 */
$value
$mod_class_variables($state *mrb, $value mod)
{
  $value ary;
  struct RClass *c;

  ary = $ary_new(mrb);
  c = $class_ptr(mod);
  while (c) {
    iv_foreach(mrb, c->iv, cv_i, &ary);
    c = c->super;
  }
  return ary;
}

MRB_API $value
$mod_cv_get($state *mrb, struct RClass *c, $sym sym)
{
  struct RClass * cls = c;
  $value v;
  int given = FALSE;

  while (c) {
    if (c->iv && iv_get(mrb, c->iv, sym, &v)) {
      given = TRUE;
    }
    c = c->super;
  }
  if (given) return v;
  if (cls && cls->tt == MRB_TT_SCLASS) {
    $value klass;

    klass = $obj_iv_get(mrb, (struct RObject *)cls,
                           $intern_lit(mrb, "__attached__"));
    c = $class_ptr(klass);
    if (c->tt == MRB_TT_CLASS || c->tt == MRB_TT_MODULE) {
      given = FALSE;
      while (c) {
        if (c->iv && iv_get(mrb, c->iv, sym, &v)) {
          given = TRUE;
        }
        c = c->super;
      }
      if (given) return v;
    }
  }
  $name_error(mrb, sym, "uninitialized class variable %S in %S",
                 $sym2str(mrb, sym), $obj_value(cls));
  /* not reached */
  return $nil_value();
}

MRB_API $value
$cv_get($state *mrb, $value mod, $sym sym)
{
  return $mod_cv_get(mrb, $class_ptr(mod), sym);
}

MRB_API void
$mod_cv_set($state *mrb, struct RClass *c, $sym sym, $value v)
{
  struct RClass * cls = c;

  while (c) {
    iv_tbl *t = c->iv;

    if (iv_get(mrb, t, sym, NULL)) {
      iv_put(mrb, t, sym, v);
      $write_barrier(mrb, (struct RBasic*)c);
      return;
    }
    c = c->super;
  }

  if (cls && cls->tt == MRB_TT_SCLASS) {
    $value klass;

    klass = $obj_iv_get(mrb, (struct RObject*)cls,
                           $intern_lit(mrb, "__attached__"));
    switch ($type(klass)) {
    case MRB_TT_CLASS:
    case MRB_TT_MODULE:
    case MRB_TT_SCLASS:
      c = $class_ptr(klass);
      break;
    default:
      c = cls;
      break;
    }
  }
  else{
    c = cls;
  }

  if (!c->iv) {
    c->iv = iv_new(mrb);
  }

  iv_put(mrb, c->iv, sym, v);
  $write_barrier(mrb, (struct RBasic*)c);
}

MRB_API void
$cv_set($state *mrb, $value mod, $sym sym, $value v)
{
  $mod_cv_set(mrb, $class_ptr(mod), sym, v);
}

MRB_API $bool
$mod_cv_defined($state *mrb, struct RClass * c, $sym sym)
{
  while (c) {
    iv_tbl *t = c->iv;
    if (iv_get(mrb, t, sym, NULL)) return TRUE;
    c = c->super;
  }

  return FALSE;
}

MRB_API $bool
$cv_defined($state *mrb, $value mod, $sym sym)
{
  return $mod_cv_defined(mrb, $class_ptr(mod), sym);
}

$value
$vm_cv_get($state *mrb, $sym sym)
{
  struct RClass *c;

  c = MRB_PROC_TARGET_CLASS(mrb->c->ci->proc);
  return $mod_cv_get(mrb, c, sym);
}

void
$vm_cv_set($state *mrb, $sym sym, $value v)
{
  struct RClass *c;

  c = MRB_PROC_TARGET_CLASS(mrb->c->ci->proc);
  $mod_cv_set(mrb, c, sym, v);
}

static void
mod_const_check($state *mrb, $value mod)
{
  switch ($type(mod)) {
  case MRB_TT_CLASS:
  case MRB_TT_MODULE:
  case MRB_TT_SCLASS:
    break;
  default:
    $raise(mrb, E_TYPE_ERROR, "constant look-up for non class/module");
    break;
  }
}

static $value
const_get($state *mrb, struct RClass *base, $sym sym)
{
  struct RClass *c = base;
  $value v;
  $bool retry = FALSE;
  $value name;

L_RETRY:
  while (c) {
    if (c->iv) {
      if (iv_get(mrb, c->iv, sym, &v))
        return v;
    }
    c = c->super;
  }
  if (!retry && base->tt == MRB_TT_MODULE) {
    c = mrb->object_class;
    retry = TRUE;
    goto L_RETRY;
  }
  name = $symbol_value(sym);
  return $funcall_argv(mrb, $obj_value(base), $intern_lit(mrb, "const_missing"), 1, &name);
}

MRB_API $value
$const_get($state *mrb, $value mod, $sym sym)
{
  mod_const_check(mrb, mod);
  return const_get(mrb, $class_ptr(mod), sym);
}

$value
$vm_const_get($state *mrb, $sym sym)
{
  struct RClass *c;
  struct RClass *c2;
  $value v;
  struct RProc *proc;

  c = MRB_PROC_TARGET_CLASS(mrb->c->ci->proc);
  if (iv_get(mrb, c->iv, sym, &v)) {
    return v;
  }
  c2 = c;
  while (c2 && c2->tt == MRB_TT_SCLASS) {
    $value klass;

    if (!iv_get(mrb, c2->iv, $intern_lit(mrb, "__attached__"), &klass)) {
      c2 = NULL;
      break;
    }
    c2 = $class_ptr(klass);
  }
  if (c2 && (c2->tt == MRB_TT_CLASS || c2->tt == MRB_TT_MODULE)) c = c2;
  $assert(!MRB_PROC_CFUNC_P(mrb->c->ci->proc));
  proc = mrb->c->ci->proc;
  while (proc) {
    c2 = MRB_PROC_TARGET_CLASS(proc);
    if (c2 && iv_get(mrb, c2->iv, sym, &v)) {
      return v;
    }
    proc = proc->upper;
  }
  return const_get(mrb, c, sym);
}

MRB_API void
$const_set($state *mrb, $value mod, $sym sym, $value v)
{
  mod_const_check(mrb, mod);
  if ($type(v) == MRB_TT_CLASS || $type(v) == MRB_TT_MODULE) {
    $class_name_class(mrb, $class_ptr(mod), $class_ptr(v), sym);
  }
  $iv_set(mrb, mod, sym, v);
}

void
$vm_const_set($state *mrb, $sym sym, $value v)
{
  struct RClass *c;

  c = MRB_PROC_TARGET_CLASS(mrb->c->ci->proc);
  $obj_iv_set(mrb, (struct RObject*)c, sym, v);
}

MRB_API void
$const_remove($state *mrb, $value mod, $sym sym)
{
  mod_const_check(mrb, mod);
  $iv_remove(mrb, mod, sym);
}

MRB_API void
$define_const($state *mrb, struct RClass *mod, const char *name, $value v)
{
  $obj_iv_set(mrb, (struct RObject*)mod, $intern_cstr(mrb, name), v);
}

MRB_API void
$define_global_const($state *mrb, const char *name, $value val)
{
  $define_const(mrb, mrb->object_class, name, val);
}

static int
const_i($state *mrb, $sym sym, $value v, void *p)
{
  $value ary;
  const char* s;
  $int len;

  ary = *($value*)p;
  s = $sym2name_len(mrb, sym, &len);
  if (len >= 1 && ISUPPER(s[0])) {
    $ary_push(mrb, ary, $symbol_value(sym));
  }
  return 0;
}

/* 15.2.2.4.24 */
/*
 *  call-seq:
 *     mod.constants    -> array
 *
 *  Returns an array of all names of contants defined in the receiver.
 */
$value
$mod_constants($state *mrb, $value mod)
{
  $value ary;
  $bool inherit = TRUE;
  struct RClass *c = $class_ptr(mod);

  $get_args(mrb, "|b", &inherit);
  ary = $ary_new(mrb);
  while (c) {
    iv_foreach(mrb, c->iv, const_i, &ary);
    if (!inherit) break;
    c = c->super;
    if (c == mrb->object_class) break;
  }
  return ary;
}

MRB_API $value
$gv_get($state *mrb, $sym sym)
{
  $value v;

  if (iv_get(mrb, mrb->globals, sym, &v))
    return v;
  return $nil_value();
}

MRB_API void
$gv_set($state *mrb, $sym sym, $value v)
{
  iv_tbl *t;

  if (!mrb->globals) {
    mrb->globals = iv_new(mrb);
  }
  t = mrb->globals;
  iv_put(mrb, t, sym, v);
}

MRB_API void
$gv_remove($state *mrb, $sym sym)
{
  iv_del(mrb, mrb->globals, sym, NULL);
}

static int
gv_i($state *mrb, $sym sym, $value v, void *p)
{
  $value ary;

  ary = *($value*)p;
  $ary_push(mrb, ary, $symbol_value(sym));
  return 0;
}

/* 15.3.1.2.4  */
/* 15.3.1.3.14 */
/*
 *  call-seq:
 *     global_variables    -> array
 *
 *  Returns an array of the names of global variables.
 *
 *     global_variables.grep /std/   #=> [:$stdin, :$stdout, :$stderr]
 */
$value
$f_global_variables($state *mrb, $value self)
{
  iv_tbl *t = mrb->globals;
  $value ary = $ary_new(mrb);
  size_t i;
  char buf[3];

  iv_foreach(mrb, t, gv_i, &ary);
  buf[0] = '$';
  buf[2] = 0;
  for (i = 1; i <= 9; ++i) {
    buf[1] = (char)(i + '0');
    $ary_push(mrb, ary, $symbol_value($intern(mrb, buf, 2)));
  }
  return ary;
}

static $bool
$const_defined_0($state *mrb, $value mod, $sym id, $bool exclude, $bool recurse)
{
  struct RClass *klass = $class_ptr(mod);
  struct RClass *tmp;
  $bool mod_retry = FALSE;

  tmp = klass;
retry:
  while (tmp) {
    if (iv_get(mrb, tmp->iv, id, NULL)) {
      return TRUE;
    }
    if (!recurse && (klass != mrb->object_class)) break;
    tmp = tmp->super;
  }
  if (!exclude && !mod_retry && (klass->tt == MRB_TT_MODULE)) {
    mod_retry = TRUE;
    tmp = mrb->object_class;
    goto retry;
  }
  return FALSE;
}

MRB_API $bool
$const_defined($state *mrb, $value mod, $sym id)
{
  return $const_defined_0(mrb, mod, id, TRUE, TRUE);
}

MRB_API $bool
$const_defined_at($state *mrb, $value mod, $sym id)
{
  return $const_defined_0(mrb, mod, id, TRUE, FALSE);
}

MRB_API $value
$attr_get($state *mrb, $value obj, $sym id)
{
  return $iv_get(mrb, obj, id);
}

struct csym_arg {
  struct RClass *c;
  $sym sym;
};

static int
csym_i($state *mrb, $sym sym, $value v, void *p)
{
  struct csym_arg *a = (struct csym_arg*)p;
  struct RClass *c = a->c;

  if ($type(v) == c->tt && $class_ptr(v) == c) {
    a->sym = sym;
    return 1;     /* stop iteration */
  }
  return 0;
}

static $sym
find_class_sym($state *mrb, struct RClass *outer, struct RClass *c)
{
  struct csym_arg arg;

  if (!outer) return 0;
  if (outer == c) return 0;
  arg.c = c;
  arg.sym = 0;
  iv_foreach(mrb, outer->iv, csym_i, &arg);
  return arg.sym;
}

static struct RClass*
outer_class($state *mrb, struct RClass *c)
{
  $value ov;

  ov = $obj_iv_get(mrb, (struct RObject*)c, $intern_lit(mrb, "__outer__"));
  if ($nil_p(ov)) return NULL;
  switch ($type(ov)) {
  case MRB_TT_CLASS:
  case MRB_TT_MODULE:
    return $class_ptr(ov);
  default:
    break;
  }
  return NULL;
}

static $bool
detect_outer_loop($state *mrb, struct RClass *c)
{
  struct RClass *t = c;         /* tortoise */
  struct RClass *h = c;         /* hare */

  for (;;) {
    if (h == NULL) return FALSE;
    h = outer_class(mrb, h);
    if (h == NULL) return FALSE;
    h = outer_class(mrb, h);
    t = outer_class(mrb, t);
    if (t == h) return TRUE;
  }
}

$value
$class_find_path($state *mrb, struct RClass *c)
{
  struct RClass *outer;
  $value path;
  $sym name;
  const char *str;
  $int len;

  if (detect_outer_loop(mrb, c)) return $nil_value();
  outer = outer_class(mrb, c);
  if (outer == NULL) return $nil_value();
  name = find_class_sym(mrb, outer, c);
  if (name == 0) return $nil_value();
  str = $class_name(mrb, outer);
  path = $str_new_capa(mrb, 40);
  $str_cat_cstr(mrb, path, str);
  $str_cat_cstr(mrb, path, "::");

  str = $sym2name_len(mrb, name, &len);
  $str_cat(mrb, path, str, len);
  if (RSTRING_PTR(path)[0] != '#') {
    iv_del(mrb, c->iv, $intern_lit(mrb, "__outer__"), NULL);
    iv_put(mrb, c->iv, $intern_lit(mrb, "__classname__"), path);
    $field_write_barrier_value(mrb, (struct RBasic*)c, path);
  }
  return path;
}
