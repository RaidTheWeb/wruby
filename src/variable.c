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

typedef int (iv_foreach_func)(state*,_sym,value,void*);

#ifndef MRB_IV_SEGMENT_SIZE
#define MRB_IV_SEGMENT_SIZE 4
#endif

typedef struct segment {
  _sym key[MRB_IV_SEGMENT_SIZE];
  value val[MRB_IV_SEGMENT_SIZE];
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
iv_new(state *mrb)
{
  iv_tbl *t;

  t = (iv_tbl*)_malloc(mrb, sizeof(iv_tbl));
  t->size = 0;
  t->rootseg =  NULL;
  t->last_len = 0;

  return t;
}

/* Set the value for the symbol in the instance variable table. */
static void
iv_put(state *mrb, iv_tbl *t, _sym sym, value val)
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
      _sym key = seg->key[i];
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

  seg = (segment*)_malloc(mrb, sizeof(segment));
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
static _bool
iv_get(state *mrb, iv_tbl *t, _sym sym, value *vp)
{
  segment *seg;
  size_t i;

  if (t == NULL) return FALSE;
  seg = t->rootseg;
  while (seg) {
    for (i=0; i<MRB_IV_SEGMENT_SIZE; i++) {
      _sym key = seg->key[i];

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
static _bool
iv_del(state *mrb, iv_tbl *t, _sym sym, value *vp)
{
  segment *seg;
  size_t i;

  if (t == NULL) return FALSE;
  seg = t->rootseg;
  while (seg) {
    for (i=0; i<MRB_IV_SEGMENT_SIZE; i++) {
      _sym key = seg->key[i];

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
static _bool
iv_foreach(state *mrb, iv_tbl *t, iv_foreach_func *func, void *p)
{
  segment *seg;
  size_t i;
  int n;

  if (t == NULL) return TRUE;
  seg = t->rootseg;
  while (seg) {
    for (i=0; i<MRB_IV_SEGMENT_SIZE; i++) {
      _sym key = seg->key[i];

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
iv_size(state *mrb, iv_tbl *t)
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
iv_copy(state *mrb, iv_tbl *t)
{
  segment *seg;
  iv_tbl *t2;

  size_t i;

  seg = t->rootseg;
  t2 = iv_new(mrb);

  while (seg != NULL) {
    for (i=0; i<MRB_IV_SEGMENT_SIZE; i++) {
      _sym key = seg->key[i];
      value val = seg->val[i];

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
iv_free(state *mrb, iv_tbl *t)
{
  segment *seg;

  seg = t->rootseg;
  while (seg) {
    segment *p = seg;
    seg = seg->next;
    _free(mrb, p);
  }
  _free(mrb, t);
}

static int
iv_mark_i(state *mrb, _sym sym, value v, void *p)
{
  _gc_mark_value(mrb, v);
  return 0;
}

static void
mark_tbl(state *mrb, iv_tbl *t)
{
  iv_foreach(mrb, t, iv_mark_i, 0);
}

void
_gc_mark_gv(state *mrb)
{
  mark_tbl(mrb, mrb->globals);
}

void
_gc_free_gv(state *mrb)
{
  if (mrb->globals)
    iv_free(mrb, mrb->globals);
}

void
_gc_mark_iv(state *mrb, struct RObject *obj)
{
  mark_tbl(mrb, obj->iv);
}

size_t
_gc_mark_iv_size(state *mrb, struct RObject *obj)
{
  return iv_size(mrb, obj->iv);
}

void
_gc_free_iv(state *mrb, struct RObject *obj)
{
  if (obj->iv) {
    iv_free(mrb, obj->iv);
  }
}

value
_vm_special_get(state *mrb, _sym i)
{
  return _fixnum_value(0);
}

void
_vm_special_set(state *mrb, _sym i, value v)
{
}

static _bool
obj_iv_p(value obj)
{
  switch (_type(obj)) {
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

MRB_API value
_obj_iv_get(state *mrb, struct RObject *obj, _sym sym)
{
  value v;

  if (obj->iv && iv_get(mrb, obj->iv, sym, &v))
    return v;
  return _nil_value();
}

MRB_API value
_iv_get(state *mrb, value obj, _sym sym)
{
  if (obj_iv_p(obj)) {
    return _obj_iv_get(mrb, _obj_ptr(obj), sym);
  }
  return _nil_value();
}

static inline void assign_class_name(state *mrb, struct RObject *obj, _sym sym, value v);

MRB_API void
_obj_iv_set(state *mrb, struct RObject *obj, _sym sym, value v)
{
  iv_tbl *t;

  if (MRB_FROZEN_P(obj)) {
    _raisef(mrb, E_FROZEN_ERROR, "can't modify frozen %S", _obj_value(obj));
  }
  assign_class_name(mrb, obj, sym, v);
  if (!obj->iv) {
    obj->iv = iv_new(mrb);
  }
  t = obj->iv;
  iv_put(mrb, t, sym, v);
  _write_barrier(mrb, (struct RBasic*)obj);
}

static inline _bool
namespace_p(enum _vtype tt)
{
  return tt == MRB_TT_CLASS || tt == MRB_TT_MODULE ? TRUE : FALSE;
}

static inline void
assign_class_name(state *mrb, struct RObject *obj, _sym sym, value v)
{
  if (namespace_p(obj->tt) && namespace_p(_type(v))) {
    struct RObject *c = _obj_ptr(v);
    if (obj != c && ISUPPER(_sym2name(mrb, sym)[0])) {
      _sym id_classname = _intern_lit(mrb, "__classname__");
      value o = _obj_iv_get(mrb, c, id_classname);

      if (_nil_p(o)) {
        _sym id_outer = _intern_lit(mrb, "__outer__");
        o = _obj_iv_get(mrb, c, id_outer);

        if (_nil_p(o)) {
          if ((struct RClass *)obj == mrb->object_class) {
            _obj_iv_set(mrb, c, id_classname, _symbol_value(sym));
          }
          else {
            _obj_iv_set(mrb, c, id_outer, _obj_value(obj));
          }
        }
      }
    }
  }
}

MRB_API void
_iv_set(state *mrb, value obj, _sym sym, value v)
{
  if (obj_iv_p(obj)) {
    _obj_iv_set(mrb, _obj_ptr(obj), sym, v);
  }
  else {
    _raise(mrb, E_ARGUMENT_ERROR, "cannot set instance variable");
  }
}

MRB_API _bool
_obj_iv_defined(state *mrb, struct RObject *obj, _sym sym)
{
  iv_tbl *t;

  t = obj->iv;
  if (t) {
    return iv_get(mrb, t, sym, NULL);
  }
  return FALSE;
}

MRB_API _bool
_iv_defined(state *mrb, value obj, _sym sym)
{
  if (!obj_iv_p(obj)) return FALSE;
  return _obj_iv_defined(mrb, _obj_ptr(obj), sym);
}

#define identchar(c) (ISALNUM(c) || (c) == '_' || !ISASCII(c))

MRB_API _bool
_iv_name_sym_p(state *mrb, _sym iv_name)
{
  const char *s;
  _int i, len;

  s = _sym2name_len(mrb, iv_name, &len);
  if (len < 2) return FALSE;
  if (s[0] != '@') return FALSE;
  if (s[1] == '@') return FALSE;
  for (i=1; i<len; i++) {
    if (!identchar(s[i])) return FALSE;
  }
  return TRUE;
}

MRB_API void
_iv_name_sym_check(state *mrb, _sym iv_name)
{
  if (!_iv_name_sym_p(mrb, iv_name)) {
    _name_error(mrb, iv_name, "'%S' is not allowed as an instance variable name", _sym2str(mrb, iv_name));
  }
}

MRB_API void
_iv_copy(state *mrb, value dest, value src)
{
  struct RObject *d = _obj_ptr(dest);
  struct RObject *s = _obj_ptr(src);

  if (d->iv) {
    iv_free(mrb, d->iv);
    d->iv = 0;
  }
  if (s->iv) {
    _write_barrier(mrb, (struct RBasic*)d);
    d->iv = iv_copy(mrb, s->iv);
  }
}

static int
inspect_i(state *mrb, _sym sym, value v, void *p)
{
  value str = *(value*)p;
  const char *s;
  _int len;
  value ins;
  char *sp = RSTRING_PTR(str);

  /* need not to show internal data */
  if (sp[0] == '-') { /* first element */
    sp[0] = '#';
    _str_cat_lit(mrb, str, " ");
  }
  else {
    _str_cat_lit(mrb, str, ", ");
  }
  s = _sym2name_len(mrb, sym, &len);
  _str_cat(mrb, str, s, len);
  _str_cat_lit(mrb, str, "=");
  if (_type(v) == MRB_TT_OBJECT) {
    ins = _any_to_s(mrb, v);
  }
  else {
    ins = _inspect(mrb, v);
  }
  _str_cat_str(mrb, str, ins);
  return 0;
}

value
_obj_iv_inspect(state *mrb, struct RObject *obj)
{
  iv_tbl *t = obj->iv;
  size_t len = iv_size(mrb, t);

  if (len > 0) {
    const char *cn = _obj_classname(mrb, _obj_value(obj));
    value str = _str_new_capa(mrb, 30);

    _str_cat_lit(mrb, str, "-<");
    _str_cat_cstr(mrb, str, cn);
    _str_cat_lit(mrb, str, ":");
    _str_concat(mrb, str, _ptr_to_str(mrb, obj));

    iv_foreach(mrb, t, inspect_i, &str);
    _str_cat_lit(mrb, str, ">");
    return str;
  }
  return _any_to_s(mrb, _obj_value(obj));
}

MRB_API value
_iv_remove(state *mrb, value obj, _sym sym)
{
  if (obj_iv_p(obj)) {
    iv_tbl *t = _obj_ptr(obj)->iv;
    value val;

    if (iv_del(mrb, t, sym, &val)) {
      return val;
    }
  }
  return _undef_value();
}

static int
iv_i(state *mrb, _sym sym, value v, void *p)
{
  value ary;
  const char* s;
  _int len;

  ary = *(value*)p;
  s = _sym2name_len(mrb, sym, &len);
  if (len > 1 && s[0] == '@' && s[1] != '@') {
    _ary_push(mrb, ary, _symbol_value(sym));
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
value
_obj_instance_variables(state *mrb, value self)
{
  value ary;

  ary = _ary_new(mrb);
  if (obj_iv_p(self)) {
    iv_foreach(mrb, _obj_ptr(self)->iv, iv_i, &ary);
  }
  return ary;
}

static int
cv_i(state *mrb, _sym sym, value v, void *p)
{
  value ary;
  const char* s;
  _int len;

  ary = *(value*)p;
  s = _sym2name_len(mrb, sym, &len);
  if (len > 2 && s[0] == '@' && s[1] == '@') {
    _ary_push(mrb, ary, _symbol_value(sym));
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
value
_mod_class_variables(state *mrb, value mod)
{
  value ary;
  struct RClass *c;

  ary = _ary_new(mrb);
  c = _class_ptr(mod);
  while (c) {
    iv_foreach(mrb, c->iv, cv_i, &ary);
    c = c->super;
  }
  return ary;
}

MRB_API value
_mod_cv_get(state *mrb, struct RClass *c, _sym sym)
{
  struct RClass * cls = c;
  value v;
  int given = FALSE;

  while (c) {
    if (c->iv && iv_get(mrb, c->iv, sym, &v)) {
      given = TRUE;
    }
    c = c->super;
  }
  if (given) return v;
  if (cls && cls->tt == MRB_TT_SCLASS) {
    value klass;

    klass = _obj_iv_get(mrb, (struct RObject *)cls,
                           _intern_lit(mrb, "__attached__"));
    c = _class_ptr(klass);
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
  _name_error(mrb, sym, "uninitialized class variable %S in %S",
                 _sym2str(mrb, sym), _obj_value(cls));
  /* not reached */
  return _nil_value();
}

MRB_API value
_cv_get(state *mrb, value mod, _sym sym)
{
  return _mod_cv_get(mrb, _class_ptr(mod), sym);
}

MRB_API void
_mod_cv_set(state *mrb, struct RClass *c, _sym sym, value v)
{
  struct RClass * cls = c;

  while (c) {
    iv_tbl *t = c->iv;

    if (iv_get(mrb, t, sym, NULL)) {
      iv_put(mrb, t, sym, v);
      _write_barrier(mrb, (struct RBasic*)c);
      return;
    }
    c = c->super;
  }

  if (cls && cls->tt == MRB_TT_SCLASS) {
    value klass;

    klass = _obj_iv_get(mrb, (struct RObject*)cls,
                           _intern_lit(mrb, "__attached__"));
    switch (_type(klass)) {
    case MRB_TT_CLASS:
    case MRB_TT_MODULE:
    case MRB_TT_SCLASS:
      c = _class_ptr(klass);
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
  _write_barrier(mrb, (struct RBasic*)c);
}

MRB_API void
_cv_set(state *mrb, value mod, _sym sym, value v)
{
  _mod_cv_set(mrb, _class_ptr(mod), sym, v);
}

MRB_API _bool
_mod_cv_defined(state *mrb, struct RClass * c, _sym sym)
{
  while (c) {
    iv_tbl *t = c->iv;
    if (iv_get(mrb, t, sym, NULL)) return TRUE;
    c = c->super;
  }

  return FALSE;
}

MRB_API _bool
_cv_defined(state *mrb, value mod, _sym sym)
{
  return _mod_cv_defined(mrb, _class_ptr(mod), sym);
}

value
_vm_cv_get(state *mrb, _sym sym)
{
  struct RClass *c;

  c = MRB_PROC_TARGET_CLASS(mrb->c->ci->proc);
  return _mod_cv_get(mrb, c, sym);
}

void
_vm_cv_set(state *mrb, _sym sym, value v)
{
  struct RClass *c;

  c = MRB_PROC_TARGET_CLASS(mrb->c->ci->proc);
  _mod_cv_set(mrb, c, sym, v);
}

static void
mod_const_check(state *mrb, value mod)
{
  switch (_type(mod)) {
  case MRB_TT_CLASS:
  case MRB_TT_MODULE:
  case MRB_TT_SCLASS:
    break;
  default:
    _raise(mrb, E_TYPE_ERROR, "constant look-up for non class/module");
    break;
  }
}

static value
const_get(state *mrb, struct RClass *base, _sym sym)
{
  struct RClass *c = base;
  value v;
  _bool retry = FALSE;
  value name;

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
  name = _symbol_value(sym);
  return _funcall_argv(mrb, _obj_value(base), _intern_lit(mrb, "const_missing"), 1, &name);
}

MRB_API value
_const_get(state *mrb, value mod, _sym sym)
{
  mod_const_check(mrb, mod);
  return const_get(mrb, _class_ptr(mod), sym);
}

value
_vm_const_get(state *mrb, _sym sym)
{
  struct RClass *c;
  struct RClass *c2;
  value v;
  struct RProc *proc;

  c = MRB_PROC_TARGET_CLASS(mrb->c->ci->proc);
  if (iv_get(mrb, c->iv, sym, &v)) {
    return v;
  }
  c2 = c;
  while (c2 && c2->tt == MRB_TT_SCLASS) {
    value klass;

    if (!iv_get(mrb, c2->iv, _intern_lit(mrb, "__attached__"), &klass)) {
      c2 = NULL;
      break;
    }
    c2 = _class_ptr(klass);
  }
  if (c2 && (c2->tt == MRB_TT_CLASS || c2->tt == MRB_TT_MODULE)) c = c2;
  _assert(!MRB_PROC_CFUNC_P(mrb->c->ci->proc));
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
_const_set(state *mrb, value mod, _sym sym, value v)
{
  mod_const_check(mrb, mod);
  if (_type(v) == MRB_TT_CLASS || _type(v) == MRB_TT_MODULE) {
    _class_name_class(mrb, _class_ptr(mod), _class_ptr(v), sym);
  }
  _iv_set(mrb, mod, sym, v);
}

void
_vm_const_set(state *mrb, _sym sym, value v)
{
  struct RClass *c;

  c = MRB_PROC_TARGET_CLASS(mrb->c->ci->proc);
  _obj_iv_set(mrb, (struct RObject*)c, sym, v);
}

MRB_API void
_const_remove(state *mrb, value mod, _sym sym)
{
  mod_const_check(mrb, mod);
  _iv_remove(mrb, mod, sym);
}

MRB_API void
_define_const(state *mrb, struct RClass *mod, const char *name, value v)
{
  _obj_iv_set(mrb, (struct RObject*)mod, _intern_cstr(mrb, name), v);
}

MRB_API void
_define_global_const(state *mrb, const char *name, value val)
{
  _define_const(mrb, mrb->object_class, name, val);
}

static int
const_i(state *mrb, _sym sym, value v, void *p)
{
  value ary;
  const char* s;
  _int len;

  ary = *(value*)p;
  s = _sym2name_len(mrb, sym, &len);
  if (len >= 1 && ISUPPER(s[0])) {
    _ary_push(mrb, ary, _symbol_value(sym));
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
value
_mod_constants(state *mrb, value mod)
{
  value ary;
  _bool inherit = TRUE;
  struct RClass *c = _class_ptr(mod);

  _get_args(mrb, "|b", &inherit);
  ary = _ary_new(mrb);
  while (c) {
    iv_foreach(mrb, c->iv, const_i, &ary);
    if (!inherit) break;
    c = c->super;
    if (c == mrb->object_class) break;
  }
  return ary;
}

MRB_API value
_gv_get(state *mrb, _sym sym)
{
  value v;

  if (iv_get(mrb, mrb->globals, sym, &v))
    return v;
  return _nil_value();
}

MRB_API void
_gv_set(state *mrb, _sym sym, value v)
{
  iv_tbl *t;

  if (!mrb->globals) {
    mrb->globals = iv_new(mrb);
  }
  t = mrb->globals;
  iv_put(mrb, t, sym, v);
}

MRB_API void
_gv_remove(state *mrb, _sym sym)
{
  iv_del(mrb, mrb->globals, sym, NULL);
}

static int
gv_i(state *mrb, _sym sym, value v, void *p)
{
  value ary;

  ary = *(value*)p;
  _ary_push(mrb, ary, _symbol_value(sym));
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
value
_f_global_variables(state *mrb, value self)
{
  iv_tbl *t = mrb->globals;
  value ary = _ary_new(mrb);
  size_t i;
  char buf[3];

  iv_foreach(mrb, t, gv_i, &ary);
  buf[0] = '$';
  buf[2] = 0;
  for (i = 1; i <= 9; ++i) {
    buf[1] = (char)(i + '0');
    _ary_push(mrb, ary, _symbol_value(_intern(mrb, buf, 2)));
  }
  return ary;
}

static _bool
_const_defined_0(state *mrb, value mod, _sym id, _bool exclude, _bool recurse)
{
  struct RClass *klass = _class_ptr(mod);
  struct RClass *tmp;
  _bool mod_retry = FALSE;

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

MRB_API _bool
_const_defined(state *mrb, value mod, _sym id)
{
  return _const_defined_0(mrb, mod, id, TRUE, TRUE);
}

MRB_API _bool
_const_defined_at(state *mrb, value mod, _sym id)
{
  return _const_defined_0(mrb, mod, id, TRUE, FALSE);
}

MRB_API value
_attr_get(state *mrb, value obj, _sym id)
{
  return _iv_get(mrb, obj, id);
}

struct csym_arg {
  struct RClass *c;
  _sym sym;
};

static int
csym_i(state *mrb, _sym sym, value v, void *p)
{
  struct csym_arg *a = (struct csym_arg*)p;
  struct RClass *c = a->c;

  if (_type(v) == c->tt && _class_ptr(v) == c) {
    a->sym = sym;
    return 1;     /* stop iteration */
  }
  return 0;
}

static _sym
find_class_sym(state *mrb, struct RClass *outer, struct RClass *c)
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
outer_class(state *mrb, struct RClass *c)
{
  value ov;

  ov = _obj_iv_get(mrb, (struct RObject*)c, _intern_lit(mrb, "__outer__"));
  if (_nil_p(ov)) return NULL;
  switch (_type(ov)) {
  case MRB_TT_CLASS:
  case MRB_TT_MODULE:
    return _class_ptr(ov);
  default:
    break;
  }
  return NULL;
}

static _bool
detect_outer_loop(state *mrb, struct RClass *c)
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

value
_class_find_path(state *mrb, struct RClass *c)
{
  struct RClass *outer;
  value path;
  _sym name;
  const char *str;
  _int len;

  if (detect_outer_loop(mrb, c)) return _nil_value();
  outer = outer_class(mrb, c);
  if (outer == NULL) return _nil_value();
  name = find_class_sym(mrb, outer, c);
  if (name == 0) return _nil_value();
  str = _class_name(mrb, outer);
  path = _str_new_capa(mrb, 40);
  _str_cat_cstr(mrb, path, str);
  _str_cat_cstr(mrb, path, "::");

  str = _sym2name_len(mrb, name, &len);
  _str_cat(mrb, path, str, len);
  if (RSTRING_PTR(path)[0] != '#') {
    iv_del(mrb, c->iv, _intern_lit(mrb, "__outer__"), NULL);
    iv_put(mrb, c->iv, _intern_lit(mrb, "__classname__"), path);
    _field_write_barrier_value(mrb, (struct RBasic*)c, path);
  }
  return path;
}
