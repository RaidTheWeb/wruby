/*
** class.c - Class class
**
** See Copyright Notice in mruby.h
*/

#include <stdarg.h>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/numeric.h>
#include <mruby/proc.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <mruby/error.h>
#include <mruby/data.h>
#include <mruby/istruct.h>

KHASH_DEFINE(mt, _sym, _method_t, TRUE, kh_int_hash_func, kh_int_hash_equal)

void
_gc_mark_mt(state *mrb, struct RClass *c)
{
  khiter_t k;
  khash_t(mt) *h = c->mt;

  if (!h) return;
  for (k = kh_begin(h); k != kh_end(h); k++) {
    if (kh_exist(h, k)) {
      _method_t m = kh_value(h, k);

      if (MRB_METHOD_PROC_P(m)) {
        struct RProc *p = MRB_METHOD_PROC(m);
        _gc_mark(mrb, (struct RBasic*)p);
      }
    }
  }
}

size_t
_gc_mark_mt_size(state *mrb, struct RClass *c)
{
  khash_t(mt) *h = c->mt;

  if (!h) return 0;
  return kh_size(h);
}

void
_gc_free_mt(state *mrb, struct RClass *c)
{
  kh_destroy(mt, mrb, c->mt);
}

void
_class_name_class(state *mrb, struct RClass *outer, struct RClass *c, _sym id)
{
  value name;
  _sym nsym = _intern_lit(mrb, "__classname__");

  if (_obj_iv_defined(mrb, (struct RObject*)c, nsym)) return;
  if (outer == NULL || outer == mrb->object_class) {
    name = _symbol_value(id);
  }
  else {
    name = _class_path(mrb, outer);
    if (_nil_p(name)) {      /* unnamed outer class */
      if (outer != mrb->object_class && outer != c) {
        _obj_iv_set(mrb, (struct RObject*)c, _intern_lit(mrb, "__outer__"),
                       _obj_value(outer));
      }
      return;
    }
    _str_cat_cstr(mrb, name, "::");
    _str_cat_cstr(mrb, name, _sym2name(mrb, id));
  }
  _obj_iv_set(mrb, (struct RObject*)c, nsym, name);
}

static void
setup_class(state *mrb, struct RClass *outer, struct RClass *c, _sym id)
{
  _class_name_class(mrb, outer, c, id);
  _obj_iv_set(mrb, (struct RObject*)outer, id, _obj_value(c));
}

#define make_metaclass(mrb, c) prepare_singleton_class((mrb), (struct RBasic*)(c))

static void
prepare_singleton_class(state *mrb, struct RBasic *o)
{
  struct RClass *sc, *c;

  if (o->c->tt == MRB_TT_SCLASS) return;
  sc = (struct RClass*)_obj_alloc(mrb, MRB_TT_SCLASS, mrb->class_class);
  sc->flags |= MRB_FL_CLASS_IS_INHERITED;
  sc->mt = kh_init(mt, mrb);
  sc->iv = 0;
  if (o->tt == MRB_TT_CLASS) {
    c = (struct RClass*)o;
    if (!c->super) {
      sc->super = mrb->class_class;
    }
    else {
      sc->super = c->super->c;
    }
  }
  else if (o->tt == MRB_TT_SCLASS) {
    c = (struct RClass*)o;
    while (c->super->tt == MRB_TT_ICLASS)
      c = c->super;
    make_metaclass(mrb, c->super);
    sc->super = c->super->c;
  }
  else {
    sc->super = o->c;
    prepare_singleton_class(mrb, (struct RBasic*)sc);
  }
  o->c = sc;
  _field_write_barrier(mrb, (struct RBasic*)o, (struct RBasic*)sc);
  _field_write_barrier(mrb, (struct RBasic*)sc, (struct RBasic*)o);
  _obj_iv_set(mrb, (struct RObject*)sc, _intern_lit(mrb, "__attached__"), _obj_value(o));
}

static struct RClass*
class_from_sym(state *mrb, struct RClass *klass, _sym id)
{
  value c = _const_get(mrb, _obj_value(klass), id);

  _check_type(mrb, c, MRB_TT_CLASS);
  return _class_ptr(c);
}

static struct RClass*
module_from_sym(state *mrb, struct RClass *klass, _sym id)
{
  value c = _const_get(mrb, _obj_value(klass), id);

  _check_type(mrb, c, MRB_TT_MODULE);
  return _class_ptr(c);
}

static _bool
class_ptr_p(value obj)
{
  switch (_type(obj)) {
  case MRB_TT_CLASS:
  case MRB_TT_SCLASS:
  case MRB_TT_MODULE:
    return TRUE;
  default:
    return FALSE;
  }
}

static void
check_if_class_or_module(state *mrb, value obj)
{
  if (!class_ptr_p(obj)) {
    _raisef(mrb, E_TYPE_ERROR, "%S is not a class/module", _inspect(mrb, obj));
  }
}

static struct RClass*
define_module(state *mrb, _sym name, struct RClass *outer)
{
  struct RClass *m;

  if (_const_defined_at(mrb, _obj_value(outer), name)) {
    return module_from_sym(mrb, outer, name);
  }
  m = _module_new(mrb);
  setup_class(mrb, outer, m, name);

  return m;
}

MRB_API struct RClass*
_define_module_id(state *mrb, _sym name)
{
  return define_module(mrb, name, mrb->object_class);
}

MRB_API struct RClass*
_define_module(state *mrb, const char *name)
{
  return define_module(mrb, _intern_cstr(mrb, name), mrb->object_class);
}

MRB_API struct RClass*
_vm_define_module(state *mrb, value outer, _sym id)
{
  check_if_class_or_module(mrb, outer);
  if (_const_defined_at(mrb, outer, id)) {
    value old = _const_get(mrb, outer, id);

    if (_type(old) != MRB_TT_MODULE) {
      _raisef(mrb, E_TYPE_ERROR, "%S is not a module", _inspect(mrb, old));
    }
    return _class_ptr(old);
  }
  return define_module(mrb, id, _class_ptr(outer));
}

MRB_API struct RClass*
_define_module_under(state *mrb, struct RClass *outer, const char *name)
{
  _sym id = _intern_cstr(mrb, name);
  struct RClass * c = define_module(mrb, id, outer);

  setup_class(mrb, outer, c, id);
  return c;
}

static struct RClass*
find_origin(struct RClass *c)
{
  MRB_CLASS_ORIGIN(c);
  return c;
}

static struct RClass*
define_class(state *mrb, _sym name, struct RClass *super, struct RClass *outer)
{
  struct RClass * c;

  if (_const_defined_at(mrb, _obj_value(outer), name)) {
    c = class_from_sym(mrb, outer, name);
    MRB_CLASS_ORIGIN(c);
    if (super && _class_real(c->super) != super) {
      _raisef(mrb, E_TYPE_ERROR, "superclass mismatch for Class %S (%S not %S)",
                 _sym2str(mrb, name),
                 _obj_value(c->super), _obj_value(super));
    }
    return c;
  }

  c = _class_new(mrb, super);
  setup_class(mrb, outer, c, name);

  return c;
}

MRB_API struct RClass*
_define_class_id(state *mrb, _sym name, struct RClass *super)
{
  if (!super) {
    _warn(mrb, "no super class for '%S', Object assumed", _sym2str(mrb, name));
  }
  return define_class(mrb, name, super, mrb->object_class);
}

MRB_API struct RClass*
_define_class(state *mrb, const char *name, struct RClass *super)
{
  return _define_class_id(mrb, _intern_cstr(mrb, name), super);
}

static value _bob_init(state *mrb, value);
#ifdef MRB_METHOD_CACHE
static void mc_clear_all(state *mrb);
static void mc_clear_by_class(state *mrb, struct RClass*);
static void mc_clear_by_id(state *mrb, struct RClass*, _sym);
#else
#define mc_clear_all(mrb)
#define mc_clear_by_class(mrb,c)
#define mc_clear_by_id(mrb,c,s)
#endif

static void
_class_inherited(state *mrb, struct RClass *super, struct RClass *klass)
{
  value s;
  _sym mid;

  if (!super)
    super = mrb->object_class;
  super->flags |= MRB_FL_CLASS_IS_INHERITED;
  s = _obj_value(super);
  mc_clear_by_class(mrb, klass);
  mid = _intern_lit(mrb, "inherited");
  if (!_func_basic_p(mrb, s, mid, _bob_init)) {
    value c = _obj_value(klass);
    _funcall_argv(mrb, s, mid, 1, &c);
  }
}

MRB_API struct RClass*
_vm_define_class(state *mrb, value outer, value super, _sym id)
{
  struct RClass *s;
  struct RClass *c;

  if (!_nil_p(super)) {
    if (_type(super) != MRB_TT_CLASS) {
      _raisef(mrb, E_TYPE_ERROR, "superclass must be a Class (%S given)",
                 _inspect(mrb, super));
    }
    s = _class_ptr(super);
  }
  else {
    s = 0;
  }
  check_if_class_or_module(mrb, outer);
  if (_const_defined_at(mrb, outer, id)) {
    value old = _const_get(mrb, outer, id);

    if (_type(old) != MRB_TT_CLASS) {
      _raisef(mrb, E_TYPE_ERROR, "%S is not a class", _inspect(mrb, old));
    }
    c = _class_ptr(old);
    if (s) {
      /* check super class */
      if (_class_real(c->super) != s) {
        _raisef(mrb, E_TYPE_ERROR, "superclass mismatch for class %S", old);
      }
    }
    return c;
  }
  c = define_class(mrb, id, s, _class_ptr(outer));
  _class_inherited(mrb, _class_real(c->super), c);

  return c;
}

MRB_API _bool
_class_defined(state *mrb, const char *name)
{
  value sym = _check_intern_cstr(mrb, name);
  if (_nil_p(sym)) {
    return FALSE;
  }
  return _const_defined(mrb, _obj_value(mrb->object_class), _symbol(sym));
}

MRB_API _bool
_class_defined_under(state *mrb, struct RClass *outer, const char *name)
{
  value sym = _check_intern_cstr(mrb, name);
  if (_nil_p(sym)) {
    return FALSE;
  }
  return _const_defined_at(mrb, _obj_value(outer), _symbol(sym));
}

MRB_API struct RClass*
_class_get_under(state *mrb, struct RClass *outer, const char *name)
{
  return class_from_sym(mrb, outer, _intern_cstr(mrb, name));
}

MRB_API struct RClass*
_class_get(state *mrb, const char *name)
{
  return _class_get_under(mrb, mrb->object_class, name);
}

MRB_API struct RClass*
_exc_get(state *mrb, const char *name)
{
  struct RClass *exc, *e;
  value c = _const_get(mrb, _obj_value(mrb->object_class),
                              _intern_cstr(mrb, name));

  if (_type(c) != MRB_TT_CLASS) {
    _raise(mrb, mrb->eException_class, "exception corrupted");
  }
  exc = e = _class_ptr(c);

  while (e) {
    if (e == mrb->eException_class)
      return exc;
    e = e->super;
  }
  return mrb->eException_class;
}

MRB_API struct RClass*
_module_get_under(state *mrb, struct RClass *outer, const char *name)
{
  return module_from_sym(mrb, outer, _intern_cstr(mrb, name));
}

MRB_API struct RClass*
_module_get(state *mrb, const char *name)
{
  return _module_get_under(mrb, mrb->object_class, name);
}

/*!
 * Defines a class under the namespace of \a outer.
 * \param outer  a class which contains the new class.
 * \param id     name of the new class
 * \param super  a class from which the new class will derive.
 *               NULL means \c Object class.
 * \return the created class
 * \throw TypeError if the constant name \a name is already taken but
 *                  the constant is not a \c Class.
 * \throw NameError if the class is already defined but the class can not
 *                  be reopened because its superclass is not \a super.
 * \post top-level constant named \a name refers the returned class.
 *
 * \note if a class named \a name is already defined and its superclass is
 *       \a super, the function just returns the defined class.
 */
MRB_API struct RClass*
_define_class_under(state *mrb, struct RClass *outer, const char *name, struct RClass *super)
{
  _sym id = _intern_cstr(mrb, name);
  struct RClass * c;

#if 0
  if (!super) {
    _warn(mrb, "no super class for '%S::%S', Object assumed",
             _obj_value(outer), _sym2str(mrb, id));
  }
#endif
  c = define_class(mrb, id, super, outer);
  setup_class(mrb, outer, c, id);
  return c;
}

MRB_API void
_define_method_raw(state *mrb, struct RClass *c, _sym mid, _method_t m)
{
  khash_t(mt) *h;
  khiter_t k;
  MRB_CLASS_ORIGIN(c);
  h = c->mt;

  if (MRB_FROZEN_P(c)) {
    if (c->tt == MRB_TT_MODULE)
      _raise(mrb, E_FROZEN_ERROR, "can't modify frozen module");
    else
      _raise(mrb, E_FROZEN_ERROR, "can't modify frozen class");
  }
  if (!h) h = c->mt = kh_init(mt, mrb);
  k = kh_put(mt, mrb, h, mid);
  kh_value(h, k) = m;
  if (MRB_METHOD_PROC_P(m) && !MRB_METHOD_UNDEF_P(m)) {
    struct RProc *p = MRB_METHOD_PROC(m);

    p->flags |= MRB_PROC_SCOPE;
    p->c = NULL;
    _field_write_barrier(mrb, (struct RBasic*)c, (struct RBasic*)p);
    if (!MRB_PROC_ENV_P(p)) {
      MRB_PROC_SET_TARGET_CLASS(p, c);
    }
  }
  mc_clear_by_id(mrb, c, mid);
}

MRB_API void
_define_method_id(state *mrb, struct RClass *c, _sym mid, _func_t func, _aspec aspec)
{
  _method_t m;
  int ai = _gc_arena_save(mrb);

  MRB_METHOD_FROM_FUNC(m, func);
  _define_method_raw(mrb, c, mid, m);
  _gc_arena_restore(mrb, ai);
}

MRB_API void
_define_method(state *mrb, struct RClass *c, const char *name, _func_t func, _aspec aspec)
{
  _define_method_id(mrb, c, _intern_cstr(mrb, name), func, aspec);
}

/* a function to raise NotImplementedError with current method name */
MRB_API void
_notimplement(state *mrb)
{
  const char *str;
  _int len;
  _callinfo *ci = mrb->c->ci;

  if (ci->mid) {
    str = _sym2name_len(mrb, ci->mid, &len);
    _raisef(mrb, E_NOTIMP_ERROR,
      "%S() function is unimplemented on this machine",
      _str_new_static(mrb, str, (size_t)len));
  }
}

/* a function to be replacement of unimplemented method */
MRB_API value
_notimplement_m(state *mrb, value self)
{
  _notimplement(mrb);
  /* not reached */
  return _nil_value();
}

static value
check_type(state *mrb, value val, enum _vtype t, const char *c, const char *m)
{
  value tmp;

  tmp = _check_convert_type(mrb, val, t, c, m);
  if (_nil_p(tmp)) {
    _raisef(mrb, E_TYPE_ERROR, "expected %S", _str_new_cstr(mrb, c));
  }
  return tmp;
}

static value
to_str(state *mrb, value val)
{
  return check_type(mrb, val, MRB_TT_STRING, "String", "to_str");
}

static value
to_ary(state *mrb, value val)
{
  return check_type(mrb, val, MRB_TT_ARRAY, "Array", "to_ary");
}

static value
to_hash(state *mrb, value val)
{
  return check_type(mrb, val, MRB_TT_HASH, "Hash", "to_hash");
}

#define to_sym(mrb, ss) _obj_to_sym(mrb, ss)

MRB_API _int
_get_argc(state *mrb)
{
  _int argc = mrb->c->ci->argc;

  if (argc < 0) {
    struct RArray *a = _ary_ptr(mrb->c->stack[1]);

    argc = ARY_LEN(a);
  }
  return argc;
}

MRB_API value*
_get_argv(state *mrb)
{
  _int argc = mrb->c->ci->argc;
  value *array_argv;
  if (argc < 0) {
    struct RArray *a = _ary_ptr(mrb->c->stack[1]);

    array_argv = ARY_PTR(a);
  }
  else {
    array_argv = NULL;
  }
  return array_argv;
}

/*
  retrieve arguments from state.

  _get_args(mrb, format, ...)

  returns number of arguments parsed.

  format specifiers:

    string  mruby type     C type                 note
    ----------------------------------------------------------------------------------------------
    o:      Object         [value]
    C:      class/module   [value]
    S:      String         [value]            when ! follows, the value may be nil
    A:      Array          [value]            when ! follows, the value may be nil
    H:      Hash           [value]            when ! follows, the value may be nil
    s:      String         [char*,_int]        Receive two arguments; s! gives (NULL,0) for nil
    z:      String         [char*]                NUL terminated string; z! gives NULL for nil
    a:      Array          [value*,_int]   Receive two arguments; a! gives (NULL,0) for nil
    f:      Float          [_float]
    i:      Integer        [_int]
    b:      Boolean        [_bool]
    n:      Symbol         [_sym]
    d:      Data           [void*,_data_type const] 2nd argument will be used to check data type so it won't be modified
    I:      Inline struct  [void*]
    &:      Block          [value]            &! raises exception if no block given
    *:      rest argument  [value*,_int]   The rest of the arguments as an array; *! avoid copy of the stack
    |:      optional                              Following arguments are optional
    ?:      optional given [_bool]             true if preceding argument (optional) is given
 */
MRB_API _int
_get_args(state *mrb, const char *format, ...)
{
  const char *fmt = format;
  char c;
  _int i = 0;
  va_list ap;
  _int argc = _get_argc(mrb);
  _int arg_i = 0;
  value *array_argv = _get_argv(mrb);
  _bool opt = FALSE;
  _bool opt_skip = TRUE;
  _bool given = TRUE;

  va_start(ap, format);

#define ARGV \
  (array_argv ? array_argv : (mrb->c->stack + 1))

  while ((c = *fmt++)) {
    switch (c) {
    case '|':
      opt = TRUE;
      break;
    case '*':
      opt_skip = FALSE;
      goto check_exit;
    case '!':
      break;
    case '&': case '?':
      if (opt) opt_skip = FALSE;
      break;
    default:
      break;
    }
  }

 check_exit:
  opt = FALSE;
  i = 0;
  while ((c = *format++)) {
    switch (c) {
    case '|': case '*': case '&': case '?':
      break;
    default:
      if (argc <= i) {
        if (opt) {
          given = FALSE;
        }
        else {
          _raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
        }
      }
      break;
    }

    switch (c) {
    case 'o':
      {
        value *p;

        p = va_arg(ap, value*);
        if (i < argc) {
          *p = ARGV[arg_i++];
          i++;
        }
      }
      break;
    case 'C':
      {
        value *p;

        p = va_arg(ap, value*);
        if (i < argc) {
          value ss;

          ss = ARGV[arg_i++];
          if (!class_ptr_p(ss)) {
            _raisef(mrb, E_TYPE_ERROR, "%S is not class/module", ss);
          }
          *p = ss;
          i++;
        }
      }
      break;
    case 'S':
      {
        value *p;

        p = va_arg(ap, value*);
        if (*format == '!') {
          format++;
          if (i < argc && _nil_p(ARGV[arg_i])) {
            *p = ARGV[arg_i++];
            i++;
            break;
          }
        }
        if (i < argc) {
          *p = to_str(mrb, ARGV[arg_i++]);
          i++;
        }
      }
      break;
    case 'A':
      {
        value *p;

        p = va_arg(ap, value*);
        if (*format == '!') {
          format++;
          if (i < argc && _nil_p(ARGV[arg_i])) {
            *p = ARGV[arg_i++];
            i++;
            break;
          }
        }
        if (i < argc) {
          *p = to_ary(mrb, ARGV[arg_i++]);
          i++;
        }
      }
      break;
    case 'H':
      {
        value *p;

        p = va_arg(ap, value*);
        if (*format == '!') {
          format++;
          if (i < argc && _nil_p(ARGV[arg_i])) {
            *p = ARGV[arg_i++];
            i++;
            break;
          }
        }
        if (i < argc) {
          *p = to_hash(mrb, ARGV[arg_i++]);
          i++;
        }
      }
      break;
    case 's':
      {
        value ss;
        char **ps = 0;
        _int *pl = 0;

        ps = va_arg(ap, char**);
        pl = va_arg(ap, _int*);
        if (*format == '!') {
          format++;
          if (i < argc && _nil_p(ARGV[arg_i])) {
            *ps = NULL;
            *pl = 0;
            i++; arg_i++;
            break;
          }
        }
        if (i < argc) {
          ss = to_str(mrb, ARGV[arg_i++]);
          *ps = RSTRING_PTR(ss);
          *pl = RSTRING_LEN(ss);
          i++;
        }
      }
      break;
    case 'z':
      {
        value ss;
        const char **ps;

        ps = va_arg(ap, const char**);
        if (*format == '!') {
          format++;
          if (i < argc && _nil_p(ARGV[arg_i])) {
            *ps = NULL;
            i++; arg_i++;
            break;
          }
        }
        if (i < argc) {
          ss = to_str(mrb, ARGV[arg_i++]);
          *ps = _string_value_cstr(mrb, &ss);
          i++;
        }
      }
      break;
    case 'a':
      {
        value aa;
        struct RArray *a;
        value **pb;
        _int *pl;

        pb = va_arg(ap, value**);
        pl = va_arg(ap, _int*);
        if (*format == '!') {
          format++;
          if (i < argc && _nil_p(ARGV[arg_i])) {
            *pb = 0;
            *pl = 0;
            i++; arg_i++;
            break;
          }
        }
        if (i < argc) {
          aa = to_ary(mrb, ARGV[arg_i++]);
          a = _ary_ptr(aa);
          *pb = ARY_PTR(a);
          *pl = ARY_LEN(a);
          i++;
        }
      }
      break;
    case 'I':
      {
        void* *p;
        value ss;

        p = va_arg(ap, void**);
        if (i < argc) {
          ss = ARGV[arg_i];
          if (_type(ss) != MRB_TT_ISTRUCT)
          {
            _raisef(mrb, E_TYPE_ERROR, "%S is not inline struct", ss);
          }
          *p = _istruct_ptr(ss);
          arg_i++;
          i++;
        }
      }
      break;
#ifndef MRB_WITHOUT_FLOAT
    case 'f':
      {
        _float *p;

        p = va_arg(ap, _float*);
        if (i < argc) {
          *p = _to_flo(mrb, ARGV[arg_i]);
          arg_i++;
          i++;
        }
      }
      break;
#endif
    case 'i':
      {
        _int *p;

        p = va_arg(ap, _int*);
        if (i < argc) {
          switch (_type(ARGV[arg_i])) {
            case MRB_TT_FIXNUM:
              *p = _fixnum(ARGV[arg_i]);
              break;
#ifndef MRB_WITHOUT_FLOAT
            case MRB_TT_FLOAT:
              {
                _float f = _float(ARGV[arg_i]);

                if (!FIXABLE_FLOAT(f)) {
                  _raise(mrb, E_RANGE_ERROR, "float too big for int");
                }
                *p = (_int)f;
              }
              break;
#endif
            case MRB_TT_STRING:
              _raise(mrb, E_TYPE_ERROR, "no implicit conversion of String into Integer");
              break;
            default:
              *p = _fixnum(_Integer(mrb, ARGV[arg_i]));
              break;
          }
          arg_i++;
          i++;
        }
      }
      break;
    case 'b':
      {
        _bool *boolp = va_arg(ap, _bool*);

        if (i < argc) {
          value b = ARGV[arg_i++];
          *boolp = _test(b);
          i++;
        }
      }
      break;
    case 'n':
      {
        _sym *symp;

        symp = va_arg(ap, _sym*);
        if (i < argc) {
          value ss;

          ss = ARGV[arg_i++];
          *symp = to_sym(mrb, ss);
          i++;
        }
      }
      break;
    case 'd':
      {
        void** datap;
        struct _data_type const* type;

        datap = va_arg(ap, void**);
        type = va_arg(ap, struct _data_type const*);
        if (*format == '!') {
          format++;
          if (i < argc && _nil_p(ARGV[arg_i])) {
            *datap = 0;
            i++; arg_i++;
            break;
          }
        }
        if (i < argc) {
          *datap = _data_get_ptr(mrb, ARGV[arg_i++], type);
          ++i;
        }
      }
      break;

    case '&':
      {
        value *p, *bp;

        p = va_arg(ap, value*);
        if (mrb->c->ci->argc < 0) {
          bp = mrb->c->stack + 2;
        }
        else {
          bp = mrb->c->stack + mrb->c->ci->argc + 1;
        }
        if (*format == '!') {
          format ++;
          if (_nil_p(*bp)) {
            _raise(mrb, E_ARGUMENT_ERROR, "no block given");
          }
        }
        *p = *bp;
      }
      break;
    case '|':
      if (opt_skip && i == argc) return argc;
      opt = TRUE;
      break;
    case '?':
      {
        _bool *p;

        p = va_arg(ap, _bool*);
        *p = given;
      }
      break;

    case '*':
      {
        value **var;
        _int *pl;
        _bool nocopy = array_argv ? TRUE : FALSE;

        if (*format == '!') {
          format++;
          nocopy = TRUE;
        }
        var = va_arg(ap, value**);
        pl = va_arg(ap, _int*);
        if (argc > i) {
          *pl = argc-i;
          if (*pl > 0) {
            if (nocopy) {
              *var = ARGV+arg_i;
            }
            else {
              value args = _ary_new_from_values(mrb, *pl, ARGV+arg_i);
              RARRAY(args)->c = NULL;
              *var = RARRAY_PTR(args);
            }
          }
          i = argc;
          arg_i += *pl;
        }
        else {
          *pl = 0;
          *var = NULL;
        }
      }
      break;
    default:
      _raisef(mrb, E_ARGUMENT_ERROR, "invalid argument specifier %S", _str_new(mrb, &c, 1));
      break;
    }
  }

#undef ARGV

  if (!c && argc > i) {
    _raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
  }
  va_end(ap);
  return i;
}

static struct RClass*
boot_defclass(state *mrb, struct RClass *super)
{
  struct RClass *c;

  c = (struct RClass*)_obj_alloc(mrb, MRB_TT_CLASS, mrb->class_class);
  if (super) {
    c->super = super;
    _field_write_barrier(mrb, (struct RBasic*)c, (struct RBasic*)super);
  }
  else {
    c->super = mrb->object_class;
  }
  c->mt = kh_init(mt, mrb);
  return c;
}

static void
boot_initmod(state *mrb, struct RClass *mod)
{
  if (!mod->mt) {
    mod->mt = kh_init(mt, mrb);
  }
}

static struct RClass*
include_class_new(state *mrb, struct RClass *m, struct RClass *super)
{
  struct RClass *ic = (struct RClass*)_obj_alloc(mrb, MRB_TT_ICLASS, mrb->class_class);
  if (m->tt == MRB_TT_ICLASS) {
    m = m->c;
  }
  MRB_CLASS_ORIGIN(m);
  ic->iv = m->iv;
  ic->mt = m->mt;
  ic->super = super;
  if (m->tt == MRB_TT_ICLASS) {
    ic->c = m->c;
  }
  else {
    ic->c = m;
  }
  return ic;
}

static int
include_module_at(state *mrb, struct RClass *c, struct RClass *ins_pos, struct RClass *m, int search_super)
{
  struct RClass *p, *ic;
  void *klass_mt = find_origin(c)->mt;

  while (m) {
    int superclass_seen = 0;

    if (m->flags & MRB_FL_CLASS_IS_PREPENDED)
      goto skip;

    if (klass_mt && klass_mt == m->mt)
      return -1;

    p = c->super;
    while (p) {
      if (p->tt == MRB_TT_ICLASS) {
        if (p->mt == m->mt) {
          if (!superclass_seen) {
            ins_pos = p; /* move insert point */
          }
          goto skip;
        }
      } else if (p->tt == MRB_TT_CLASS) {
        if (!search_super) break;
        superclass_seen = 1;
      }
      p = p->super;
    }

    ic = include_class_new(mrb, m, ins_pos->super);
    m->flags |= MRB_FL_CLASS_IS_INHERITED;
    ins_pos->super = ic;
    _field_write_barrier(mrb, (struct RBasic*)ins_pos, (struct RBasic*)ic);
    mc_clear_by_class(mrb, ins_pos);
    ins_pos = ic;
  skip:
    m = m->super;
  }
  mc_clear_all(mrb);
  return 0;
}

MRB_API void
_include_module(state *mrb, struct RClass *c, struct RClass *m)
{
  int changed = include_module_at(mrb, c, find_origin(c), m, 1);
  if (changed < 0) {
    _raise(mrb, E_ARGUMENT_ERROR, "cyclic include detected");
  }
}

MRB_API void
_prepend_module(state *mrb, struct RClass *c, struct RClass *m)
{
  struct RClass *origin;
  int changed = 0;

  if (!(c->flags & MRB_FL_CLASS_IS_PREPENDED)) {
    origin = (struct RClass*)_obj_alloc(mrb, MRB_TT_ICLASS, c);
    origin->flags |= MRB_FL_CLASS_IS_ORIGIN | MRB_FL_CLASS_IS_INHERITED;
    origin->super = c->super;
    c->super = origin;
    origin->mt = c->mt;
    c->mt = kh_init(mt, mrb);
    _field_write_barrier(mrb, (struct RBasic*)c, (struct RBasic*)origin);
    c->flags |= MRB_FL_CLASS_IS_PREPENDED;
  }
  changed = include_module_at(mrb, c, c, m, 0);
  if (changed < 0) {
    _raise(mrb, E_ARGUMENT_ERROR, "cyclic prepend detected");
  }
}

static value
_mod_prepend_features(state *mrb, value mod)
{
  value klass;

  _check_type(mrb, mod, MRB_TT_MODULE);
  _get_args(mrb, "C", &klass);
  _prepend_module(mrb, _class_ptr(klass), _class_ptr(mod));
  return mod;
}

static value
_mod_append_features(state *mrb, value mod)
{
  value klass;

  _check_type(mrb, mod, MRB_TT_MODULE);
  _get_args(mrb, "C", &klass);
  _include_module(mrb, _class_ptr(klass), _class_ptr(mod));
  return mod;
}

/* 15.2.2.4.28 */
/*
 *  call-seq:
 *     mod.include?(module)    -> true or false
 *
 *  Returns <code>true</code> if <i>module</i> is included in
 *  <i>mod</i> or one of <i>mod</i>'s ancestors.
 *
 *     module A
 *     end
 *     class B
 *       include A
 *     end
 *     class C < B
 *     end
 *     B.include?(A)   #=> true
 *     C.include?(A)   #=> true
 *     A.include?(A)   #=> false
 */
static value
_mod_include_p(state *mrb, value mod)
{
  value mod2;
  struct RClass *c = _class_ptr(mod);

  _get_args(mrb, "C", &mod2);
  _check_type(mrb, mod2, MRB_TT_MODULE);

  while (c) {
    if (c->tt == MRB_TT_ICLASS) {
      if (c->c == _class_ptr(mod2)) return _true_value();
    }
    c = c->super;
  }
  return _false_value();
}

static value
_mod_ancestors(state *mrb, value self)
{
  value result;
  struct RClass *c = _class_ptr(self);
  result = _ary_new(mrb);
  while (c) {
    if (c->tt == MRB_TT_ICLASS) {
      _ary_push(mrb, result, _obj_value(c->c));
    }
    else if (!(c->flags & MRB_FL_CLASS_IS_PREPENDED)) {
      _ary_push(mrb, result, _obj_value(c));
    }
    c = c->super;
  }

  return result;
}

static value
_mod_extend_object(state *mrb, value mod)
{
  value obj;

  _check_type(mrb, mod, MRB_TT_MODULE);
  _get_args(mrb, "o", &obj);
  _include_module(mrb, _class_ptr(_singleton_class(mrb, obj)), _class_ptr(mod));
  return mod;
}

static value
_mod_initialize(state *mrb, value mod)
{
  value b;
  struct RClass *m = _class_ptr(mod);
  boot_initmod(mrb, m); /* bootstrap a newly initialized module */
  _get_args(mrb, "|&", &b);
  if (!_nil_p(b)) {
    _yield_with_class(mrb, b, 1, &mod, mod, m);
  }
  return mod;
}

/* implementation of module_eval/class_eval */
value _mod_module_eval(state*, value);

static value
_mod_dummy_visibility(state *mrb, value mod)
{
  return mod;
}

MRB_API value
_singleton_class(state *mrb, value v)
{
  struct RBasic *obj;

  switch (_type(v)) {
  case MRB_TT_FALSE:
    if (_nil_p(v))
      return _obj_value(mrb->nil_class);
    return _obj_value(mrb->false_class);
  case MRB_TT_TRUE:
    return _obj_value(mrb->true_class);
  case MRB_TT_CPTR:
    return _obj_value(mrb->object_class);
  case MRB_TT_SYMBOL:
  case MRB_TT_FIXNUM:
#ifndef MRB_WITHOUT_FLOAT
  case MRB_TT_FLOAT:
#endif
    _raise(mrb, E_TYPE_ERROR, "can't define singleton");
    return _nil_value();    /* not reached */
  default:
    break;
  }
  obj = _basic_ptr(v);
  prepare_singleton_class(mrb, obj);
  return _obj_value(obj->c);
}

MRB_API void
_define_singleton_method(state *mrb, struct RObject *o, const char *name, _func_t func, _aspec aspec)
{
  prepare_singleton_class(mrb, (struct RBasic*)o);
  _define_method_id(mrb, o->c, _intern_cstr(mrb, name), func, aspec);
}

MRB_API void
_define_class_method(state *mrb, struct RClass *c, const char *name, _func_t func, _aspec aspec)
{
  _define_singleton_method(mrb, (struct RObject*)c, name, func, aspec);
}

MRB_API void
_define_module_function(state *mrb, struct RClass *c, const char *name, _func_t func, _aspec aspec)
{
  _define_class_method(mrb, c, name, func, aspec);
  _define_method(mrb, c, name, func, aspec);
}

#ifdef MRB_METHOD_CACHE
static void
mc_clear_all(state *mrb)
{
  struct _cache_entry *mc = mrb->cache;
  int i;

  for (i=0; i<MRB_METHOD_CACHE_SIZE; i++) {
    mc[i].c = 0;
  }
}

static void
mc_clear_by_class(state *mrb, struct RClass *c)
{
  struct _cache_entry *mc = mrb->cache;
  int i;

  if (c->flags & MRB_FL_CLASS_IS_INHERITED) {
    mc_clear_all(mrb);
    c->flags &= ~MRB_FL_CLASS_IS_INHERITED;
    return;
  }
  for (i=0; i<MRB_METHOD_CACHE_SIZE; i++) {
    if (mc[i].c == c) mc[i].c = 0;
  }
}

static void
mc_clear_by_id(state *mrb, struct RClass *c, _sym mid)
{
  struct _cache_entry *mc = mrb->cache;
  int i;

  if (c->flags & MRB_FL_CLASS_IS_INHERITED) {
    mc_clear_all(mrb);
    c->flags &= ~MRB_FL_CLASS_IS_INHERITED;
    return;
  }
  for (i=0; i<MRB_METHOD_CACHE_SIZE; i++) {
    if (mc[i].c == c || mc[i].mid == mid)
      mc[i].c = 0;
  }
}
#endif

MRB_API _method_t
_method_search_vm(state *mrb, struct RClass **cp, _sym mid)
{
  khiter_t k;
  _method_t m;
  struct RClass *c = *cp;
#ifdef MRB_METHOD_CACHE
  struct RClass *oc = c;
  int h = kh_int_hash_func(mrb, ((intptr_t)oc) ^ mid) & (MRB_METHOD_CACHE_SIZE-1);
  struct _cache_entry *mc = &mrb->cache[h];

  if (mc->c == c && mc->mid == mid) {
    *cp = mc->c0;
    return mc->m;
  }
#endif

  while (c) {
    khash_t(mt) *h = c->mt;

    if (h) {
      k = kh_get(mt, mrb, h, mid);
      if (k != kh_end(h)) {
        m = kh_value(h, k);
        if (MRB_METHOD_UNDEF_P(m)) break;
        *cp = c;
#ifdef MRB_METHOD_CACHE
        mc->c = oc;
        mc->c0 = c;
        mc->mid = mid;
        mc->m = m;
#endif
        return m;
      }
    }
    c = c->super;
  }
  MRB_METHOD_FROM_PROC(m, NULL);
  return m;                  /* no method */
}

MRB_API _method_t
_method_search(state *mrb, struct RClass* c, _sym mid)
{
  _method_t m;

  m = _method_search_vm(mrb, &c, mid);
  if (MRB_METHOD_UNDEF_P(m)) {
    value inspect = _funcall(mrb, _obj_value(c), "inspect", 0);
    if (_string_p(inspect) && RSTRING_LEN(inspect) > 64) {
      inspect = _any_to_s(mrb, _obj_value(c));
    }
    _name_error(mrb, mid, "undefined method '%S' for class %S",
               _sym2str(mrb, mid), inspect);
  }
  return m;
}

static value
attr_reader(state *mrb, value obj)
{
  value name = _proc_cfunc_env_get(mrb, 0);
  return _iv_get(mrb, obj, to_sym(mrb, name));
}

static value
_mod_attr_reader(state *mrb, value mod)
{
  struct RClass *c = _class_ptr(mod);
  value *argv;
  _int argc, i;
  int ai;

  _get_args(mrb, "*", &argv, &argc);
  ai = _gc_arena_save(mrb);
  for (i=0; i<argc; i++) {
    value name, str;
    _sym method, sym;
    struct RProc *p;
    _method_t m;

    method = to_sym(mrb, argv[i]);
    name = _sym2str(mrb, method);
    str = _str_new_capa(mrb, RSTRING_LEN(name)+1);
    _str_cat_lit(mrb, str, "@");
    _str_cat_str(mrb, str, name);
    sym = _intern_str(mrb, str);
    _iv_name_sym_check(mrb, sym);
    name = _symbol_value(sym);
    p = _proc_new_cfunc_with_env(mrb, attr_reader, 1, &name);
    MRB_METHOD_FROM_PROC(m, p);
    _define_method_raw(mrb, c, method, m);
    _gc_arena_restore(mrb, ai);
  }
  return _nil_value();
}

static value
attr_writer(state *mrb, value obj)
{
  value name = _proc_cfunc_env_get(mrb, 0);
  value val;

  _get_args(mrb, "o", &val);
  _iv_set(mrb, obj, to_sym(mrb, name), val);
  return val;
}

static value
_mod_attr_writer(state *mrb, value mod)
{
  struct RClass *c = _class_ptr(mod);
  value *argv;
  _int argc, i;
  int ai;

  _get_args(mrb, "*", &argv, &argc);
  ai = _gc_arena_save(mrb);
  for (i=0; i<argc; i++) {
    value name, str, attr;
    _sym method, sym;
    struct RProc *p;
    _method_t m;

    method = to_sym(mrb, argv[i]);

    /* prepare iv name (@name) */
    name = _sym2str(mrb, method);
    str = _str_new_capa(mrb, RSTRING_LEN(name)+1);
    _str_cat_lit(mrb, str, "@");
    _str_cat_str(mrb, str, name);
    sym = _intern_str(mrb, str);
    _iv_name_sym_check(mrb, sym);
    attr = _symbol_value(sym);

    /* prepare method name (name=) */
    str = _str_new_capa(mrb, RSTRING_LEN(str));
    _str_cat_str(mrb, str, name);
    _str_cat_lit(mrb, str, "=");
    method = _intern_str(mrb, str);

    p = _proc_new_cfunc_with_env(mrb, attr_writer, 1, &attr);
    MRB_METHOD_FROM_PROC(m, p);
    _define_method_raw(mrb, c, method, m);
    _gc_arena_restore(mrb, ai);
  }
  return _nil_value();
}

static value
_instance_alloc(state *mrb, value cv)
{
  struct RClass *c = _class_ptr(cv);
  struct RObject *o;
  enum _vtype ttype = MRB_INSTANCE_TT(c);

  if (c->tt == MRB_TT_SCLASS)
    _raise(mrb, E_TYPE_ERROR, "can't create instance of singleton class");

  if (ttype == 0) ttype = MRB_TT_OBJECT;
  if (ttype <= MRB_TT_CPTR) {
    _raisef(mrb, E_TYPE_ERROR, "can't create instance of %S", cv);
  }
  o = (struct RObject*)_obj_alloc(mrb, ttype, c);
  return _obj_value(o);
}

/*
 *  call-seq:
 *     class.new(args, ...)    ->  obj
 *
 *  Creates a new object of <i>class</i>'s class, then
 *  invokes that object's <code>initialize</code> method,
 *  passing it <i>args</i>. This is the method that ends
 *  up getting called whenever an object is constructed using
 *  `.new`.
 *
 */

MRB_API value
_instance_new(state *mrb, value cv)
{
  value obj, blk;
  value *argv;
  _int argc;
  _sym init;
  _method_t m;

  _get_args(mrb, "*&", &argv, &argc, &blk);
  obj = _instance_alloc(mrb, cv);
  init = _intern_lit(mrb, "initialize");
  m = _method_search(mrb, _class(mrb, obj), init);
  if (MRB_METHOD_CFUNC_P(m)) {
    _func_t f = MRB_METHOD_CFUNC(m);
    if (f != _bob_init) {
      f(mrb, obj);
    }
  }
  else {
    _funcall_with_block(mrb, obj, init, argc, argv, blk);
  }

  return obj;
}

MRB_API value
_obj_new(state *mrb, struct RClass *c, _int argc, const value *argv)
{
  value obj;
  _sym mid;

  obj = _instance_alloc(mrb, _obj_value(c));
  mid = _intern_lit(mrb, "initialize");
  if (!_func_basic_p(mrb, obj, mid, _bob_init)) {
    _funcall_argv(mrb, obj, mid, argc, argv);
  }
  return obj;
}

static value
_class_initialize(state *mrb, value c)
{
  value a, b;

  _get_args(mrb, "|C&", &a, &b);
  if (!_nil_p(b)) {
    _yield_with_class(mrb, b, 1, &c, c, _class_ptr(c));
  }
  return c;
}

static value
_class_new_class(state *mrb, value cv)
{
  _int n;
  value super, blk;
  value new_class;
  _sym mid;

  n = _get_args(mrb, "|C&", &super, &blk);
  if (n == 0) {
    super = _obj_value(mrb->object_class);
  }
  new_class = _obj_value(_class_new(mrb, _class_ptr(super)));
  mid = _intern_lit(mrb, "initialize");
  if (!_func_basic_p(mrb, new_class, mid, _bob_init)) {
    _funcall_with_block(mrb, new_class, mid, n, &super, blk);
  }
  _class_inherited(mrb, _class_ptr(super), _class_ptr(new_class));
  return new_class;
}

static value
_class_superclass(state *mrb, value klass)
{
  struct RClass *c;

  c = _class_ptr(klass);
  c = find_origin(c)->super;
  while (c && c->tt == MRB_TT_ICLASS) {
    c = find_origin(c)->super;
  }
  if (!c) return _nil_value();
  return _obj_value(c);
}

static value
_bob_init(state *mrb, value cv)
{
  return _nil_value();
}

static value
_bob_not(state *mrb, value cv)
{
  return _bool_value(!_test(cv));
}

/* 15.3.1.3.1  */
/* 15.3.1.3.10 */
/* 15.3.1.3.11 */
/*
 *  call-seq:
 *     obj == other        -> true or false
 *     obj.equal?(other)   -> true or false
 *     obj.eql?(other)     -> true or false
 *
 *  Equality---At the <code>Object</code> level, <code>==</code> returns
 *  <code>true</code> only if <i>obj</i> and <i>other</i> are the
 *  same object. Typically, this method is overridden in descendant
 *  classes to provide class-specific meaning.
 *
 *  Unlike <code>==</code>, the <code>equal?</code> method should never be
 *  overridden by subclasses: it is used to determine object identity
 *  (that is, <code>a.equal?(b)</code> iff <code>a</code> is the same
 *  object as <code>b</code>).
 *
 *  The <code>eql?</code> method returns <code>true</code> if
 *  <i>obj</i> and <i>anObject</i> have the same value. Used by
 *  <code>Hash</code> to test members for equality.  For objects of
 *  class <code>Object</code>, <code>eql?</code> is synonymous with
 *  <code>==</code>. Subclasses normally continue this tradition, but
 *  there are exceptions. <code>Numeric</code> types, for example,
 *  perform type conversion across <code>==</code>, but not across
 *  <code>eql?</code>, so:
 *
 *     1 == 1.0     #=> true
 *     1.eql? 1.0   #=> false
 */
value
_obj_equal_m(state *mrb, value self)
{
  value arg;

  _get_args(mrb, "o", &arg);
  return _bool_value(_obj_equal(mrb, self, arg));
}

static value
_obj_not_equal_m(state *mrb, value self)
{
  value arg;

  _get_args(mrb, "o", &arg);
  return _bool_value(!_equal(mrb, self, arg));
}

MRB_API _bool
_obj_respond_to(state *mrb, struct RClass* c, _sym mid)
{
  _method_t m;

  m = _method_search_vm(mrb, &c, mid);
  if (MRB_METHOD_UNDEF_P(m)) {
    return FALSE;
  }
  return TRUE;
}

MRB_API _bool
_respond_to(state *mrb, value obj, _sym mid)
{
  return _obj_respond_to(mrb, _class(mrb, obj), mid);
}

MRB_API value
_class_path(state *mrb, struct RClass *c)
{
  value path;
  _sym nsym = _intern_lit(mrb, "__classname__");

  path = _obj_iv_get(mrb, (struct RObject*)c, nsym);
  if (_nil_p(path)) {
    /* no name (yet) */
    return _class_find_path(mrb, c);
  }
  else if (_symbol_p(path)) {
    /* toplevel class/module */
    const char *str;
    _int len;

    str = _sym2name_len(mrb, _symbol(path), &len);
    return _str_new(mrb, str, len);
  }
  return _str_dup(mrb, path);
}

MRB_API struct RClass*
_class_real(struct RClass* cl)
{
  if (cl == 0) return NULL;
  while ((cl->tt == MRB_TT_SCLASS) || (cl->tt == MRB_TT_ICLASS)) {
    cl = cl->super;
    if (cl == 0) return NULL;
  }
  return cl;
}

MRB_API const char*
_class_name(state *mrb, struct RClass* c)
{
  value path = _class_path(mrb, c);
  if (_nil_p(path)) {
    path = c->tt == MRB_TT_MODULE ? _str_new_lit(mrb, "#<Module:") :
                                    _str_new_lit(mrb, "#<Class:");
    _str_concat(mrb, path, _ptr_to_str(mrb, c));
    _str_cat_lit(mrb, path, ">");
  }
  return RSTRING_PTR(path);
}

MRB_API const char*
_obj_classname(state *mrb, value obj)
{
  return _class_name(mrb, _obj_class(mrb, obj));
}

/*!
 * Ensures a class can be derived from super.
 *
 * \param super a reference to an object.
 * \exception TypeError if \a super is not a Class or \a super is a singleton class.
 */
static void
_check_inheritable(state *mrb, struct RClass *super)
{
  if (super->tt != MRB_TT_CLASS) {
    _raisef(mrb, E_TYPE_ERROR, "superclass must be a Class (%S given)", _obj_value(super));
  }
  if (super->tt == MRB_TT_SCLASS) {
    _raise(mrb, E_TYPE_ERROR, "can't make subclass of singleton class");
  }
  if (super == mrb->class_class) {
    _raise(mrb, E_TYPE_ERROR, "can't make subclass of Class");
  }
}

/*!
 * Creates a new class.
 * \param super     a class from which the new class derives.
 * \exception TypeError \a super is not inheritable.
 * \exception TypeError \a super is the Class class.
 */
MRB_API struct RClass*
_class_new(state *mrb, struct RClass *super)
{
  struct RClass *c;

  if (super) {
    _check_inheritable(mrb, super);
  }
  c = boot_defclass(mrb, super);
  if (super) {
    MRB_SET_INSTANCE_TT(c, MRB_INSTANCE_TT(super));
  }
  make_metaclass(mrb, c);

  return c;
}

/*!
 * Creates a new module.
 */
MRB_API struct RClass*
_module_new(state *mrb)
{
  struct RClass *m = (struct RClass*)_obj_alloc(mrb, MRB_TT_MODULE, mrb->module_class);
  boot_initmod(mrb, m);
  return m;
}

/*
 *  call-seq:
 *     obj.class    => class
 *
 *  Returns the class of <i>obj</i>, now preferred over
 *  <code>Object#type</code>, as an object's type in Ruby is only
 *  loosely tied to that object's class. This method must always be
 *  called with an explicit receiver, as <code>class</code> is also a
 *  reserved word in Ruby.
 *
 *     1.class      #=> Fixnum
 *     self.class   #=> Object
 */

MRB_API struct RClass*
_obj_class(state *mrb, value obj)
{
  return _class_real(_class(mrb, obj));
}

MRB_API void
_alias_method(state *mrb, struct RClass *c, _sym a, _sym b)
{
  _method_t m = _method_search(mrb, c, b);

  _define_method_raw(mrb, c, a, m);
}

/*!
 * Defines an alias of a method.
 * \param klass  the class which the original method belongs to
 * \param name1  a new name for the method
 * \param name2  the original name of the method
 */
MRB_API void
_define_alias(state *mrb, struct RClass *klass, const char *name1, const char *name2)
{
  _alias_method(mrb, klass, _intern_cstr(mrb, name1), _intern_cstr(mrb, name2));
}

/*
 * call-seq:
 *   mod.to_s   -> string
 *
 * Return a string representing this module or class. For basic
 * classes and modules, this is the name. For singletons, we
 * show information on the thing we're attached to as well.
 */

static value
_mod_to_s(state *mrb, value klass)
{
  value str;

  if (_type(klass) == MRB_TT_SCLASS) {
    value v = _iv_get(mrb, klass, _intern_lit(mrb, "__attached__"));

    str = _str_new_lit(mrb, "#<Class:");

    if (class_ptr_p(v)) {
      _str_cat_str(mrb, str, _inspect(mrb, v));
    }
    else {
      _str_cat_str(mrb, str, _any_to_s(mrb, v));
    }
    return _str_cat_lit(mrb, str, ">");
  }
  else {
    struct RClass *c;
    value path;

    str = _str_new_capa(mrb, 32);
    c = _class_ptr(klass);
    path = _class_path(mrb, c);

    if (_nil_p(path)) {
      switch (_type(klass)) {
        case MRB_TT_CLASS:
          _str_cat_lit(mrb, str, "#<Class:");
          break;

        case MRB_TT_MODULE:
          _str_cat_lit(mrb, str, "#<Module:");
          break;

        default:
          /* Shouldn't be happened? */
          _str_cat_lit(mrb, str, "#<??????:");
          break;
      }
      _str_concat(mrb, str, _ptr_to_str(mrb, c));
      return _str_cat_lit(mrb, str, ">");
    }
    else {
      return path;
    }
  }
}

static value
_mod_alias(state *mrb, value mod)
{
  struct RClass *c = _class_ptr(mod);
  _sym new_name, old_name;

  _get_args(mrb, "nn", &new_name, &old_name);
  _alias_method(mrb, c, new_name, old_name);
  return _nil_value();
}

void
_undef_method_id(state *mrb, struct RClass *c, _sym a)
{
  if (!_obj_respond_to(mrb, c, a)) {
    _name_error(mrb, a, "undefined method '%S' for class '%S'", _sym2str(mrb, a), _obj_value(c));
  }
  else {
    _method_t m;

    MRB_METHOD_FROM_PROC(m, NULL);
    _define_method_raw(mrb, c, a, m);
  }
}

MRB_API void
_undef_method(state *mrb, struct RClass *c, const char *name)
{
  _undef_method_id(mrb, c, _intern_cstr(mrb, name));
}

MRB_API void
_undef_class_method(state *mrb, struct RClass *c, const char *name)
{
  _undef_method(mrb,  _class_ptr(_singleton_class(mrb, _obj_value(c))), name);
}

static value
_mod_undef(state *mrb, value mod)
{
  struct RClass *c = _class_ptr(mod);
  _int argc;
  value *argv;

  _get_args(mrb, "*", &argv, &argc);
  while (argc--) {
    _undef_method_id(mrb, c, to_sym(mrb, *argv));
    argv++;
  }
  return _nil_value();
}

static void
check_const_name_str(state *mrb, value str)
{
  if (RSTRING_LEN(str) < 1 || !ISUPPER(*RSTRING_PTR(str))) {
    _name_error(mrb, _intern_str(mrb, str), "wrong constant name %S", str);
  }
}

static void
check_const_name_sym(state *mrb, _sym id)
{
  check_const_name_str(mrb, _sym2str(mrb, id));
}

static value
_mod_const_defined(state *mrb, value mod)
{
  _sym id;
  _bool inherit = TRUE;

  _get_args(mrb, "n|b", &id, &inherit);
  check_const_name_sym(mrb, id);
  if (inherit) {
    return _bool_value(_const_defined(mrb, mod, id));
  }
  return _bool_value(_const_defined_at(mrb, mod, id));
}

static value
_const_get_sym(state *mrb, value mod, _sym id)
{
  check_const_name_sym(mrb, id);
  return _const_get(mrb, mod, id);
}

static value
_mod_const_get(state *mrb, value mod)
{
  value path;
  _sym id;
  char *ptr;
  _int off, end, len;

  _get_args(mrb, "o", &path);

  if (_symbol_p(path)) {
    /* const get with symbol */
    id = _symbol(path);
    return _const_get_sym(mrb, mod, id);
  }

  /* const get with class path string */
  path = _string_type(mrb, path);
  ptr = RSTRING_PTR(path);
  len = RSTRING_LEN(path);
  off = 0;

  while (off < len) {
    end = _str_index_lit(mrb, path, "::", off);
    end = (end == -1) ? len : end;
    id = _intern(mrb, ptr+off, end-off);
    mod = _const_get_sym(mrb, mod, id);
    if (end == len)
      off = end;
    else {
      off = end + 2;
      if (off == len) {         /* trailing "::" */
        _name_error(mrb, id, "wrong constant name '%S'", path);
      }
    }
  }

  return mod;
}

static value
_mod_const_set(state *mrb, value mod)
{
  _sym id;
  value value;

  _get_args(mrb, "no", &id, &value);
  check_const_name_sym(mrb, id);
  _const_set(mrb, mod, id, value);
  return value;
}

static value
_mod_remove_const(state *mrb, value mod)
{
  _sym id;
  value val;

  _get_args(mrb, "n", &id);
  check_const_name_sym(mrb, id);
  val = _iv_remove(mrb, mod, id);
  if (_undef_p(val)) {
    _name_error(mrb, id, "constant %S not defined", _sym2str(mrb, id));
  }
  return val;
}

static value
_mod_const_missing(state *mrb, value mod)
{
  _sym sym;

  _get_args(mrb, "n", &sym);

  if (_class_real(_class_ptr(mod)) != mrb->object_class) {
    _name_error(mrb, sym, "uninitialized constant %S::%S",
                   mod,
                   _sym2str(mrb, sym));
  }
  else {
    _name_error(mrb, sym, "uninitialized constant %S",
                   _sym2str(mrb, sym));
  }
  /* not reached */
  return _nil_value();
}

/* 15.2.2.4.34 */
/*
 *  call-seq:
 *     mod.method_defined?(symbol)    -> true or false
 *
 *  Returns +true+ if the named method is defined by
 *  _mod_ (or its included modules and, if _mod_ is a class,
 *  its ancestors). Public and protected methods are matched.
 *
 *     module A
 *       def method1()  end
 *     end
 *     class B
 *       def method2()  end
 *     end
 *     class C < B
 *       include A
 *       def method3()  end
 *     end
 *
 *     A.method_defined? :method1    #=> true
 *     C.method_defined? "method1"   #=> true
 *     C.method_defined? "method2"   #=> true
 *     C.method_defined? "method3"   #=> true
 *     C.method_defined? "method4"   #=> false
 */

static value
_mod_method_defined(state *mrb, value mod)
{
  _sym id;

  _get_args(mrb, "n", &id);
  return _bool_value(_obj_respond_to(mrb, _class_ptr(mod), id));
}

static value
mod_define_method(state *mrb, value self)
{
  struct RClass *c = _class_ptr(self);
  struct RProc *p;
  _method_t m;
  _sym mid;
  value proc = _undef_value();
  value blk;

  _get_args(mrb, "n|o&", &mid, &proc, &blk);
  switch (_type(proc)) {
    case MRB_TT_PROC:
      blk = proc;
      break;
    case MRB_TT_UNDEF:
      /* ignored */
      break;
    default:
      _raisef(mrb, E_TYPE_ERROR, "wrong argument type %S (expected Proc)", _obj_value(_obj_class(mrb, proc)));
      break;
  }
  if (_nil_p(blk)) {
    _raise(mrb, E_ARGUMENT_ERROR, "no block given");
  }
  p = (struct RProc*)_obj_alloc(mrb, MRB_TT_PROC, mrb->proc_class);
  _proc_copy(p, _proc_ptr(blk));
  p->flags |= MRB_PROC_STRICT;
  MRB_METHOD_FROM_PROC(m, p);
  _define_method_raw(mrb, c, mid, m);
  return _symbol_value(mid);
}

static value
top_define_method(state *mrb, value self)
{
  return mod_define_method(mrb, _obj_value(mrb->object_class));
}

static value
_mod_eqq(state *mrb, value mod)
{
  value obj;
  _bool eqq;

  _get_args(mrb, "o", &obj);
  eqq = _obj_is_kind_of(mrb, obj, _class_ptr(mod));

  return _bool_value(eqq);
}

MRB_API value
_mod_module_function(state *mrb, value mod)
{
  value *argv;
  _int argc, i;
  _sym mid;
  _method_t m;
  struct RClass *rclass;
  int ai;

  _check_type(mrb, mod, MRB_TT_MODULE);

  _get_args(mrb, "*", &argv, &argc);
  if (argc == 0) {
    /* set MODFUNC SCOPE if implemented */
    return mod;
  }

  /* set PRIVATE method visibility if implemented */
  /* _mod_dummy_visibility(mrb, mod); */

  for (i=0; i<argc; i++) {
    _check_type(mrb, argv[i], MRB_TT_SYMBOL);

    mid = _symbol(argv[i]);
    rclass = _class_ptr(mod);
    m = _method_search(mrb, rclass, mid);

    prepare_singleton_class(mrb, (struct RBasic*)rclass);
    ai = _gc_arena_save(mrb);
    _define_method_raw(mrb, rclass->c, mid, m);
    _gc_arena_restore(mrb, ai);
  }

  return mod;
}

/* implementation of __id__ */
value _obj_id_m(state *mrb, value self);
/* implementation of instance_eval */
value _obj_instance_eval(state*, value);

static value
inspect_main(state *mrb, value mod)
{
  return _str_new_lit(mrb, "main");
}

void
_init_class(state *mrb)
{
  struct RClass *bob;           /* BasicObject */
  struct RClass *obj;           /* Object */
  struct RClass *mod;           /* Module */
  struct RClass *cls;           /* Class */

  /* boot class hierarchy */
  bob = boot_defclass(mrb, 0);
  obj = boot_defclass(mrb, bob); mrb->object_class = obj;
  mod = boot_defclass(mrb, obj); mrb->module_class = mod;/* obj -> mod */
  cls = boot_defclass(mrb, mod); mrb->class_class = cls; /* obj -> cls */
  /* fix-up loose ends */
  bob->c = obj->c = mod->c = cls->c = cls;
  make_metaclass(mrb, bob);
  make_metaclass(mrb, obj);
  make_metaclass(mrb, mod);
  make_metaclass(mrb, cls);

  /* name basic classes */
  _define_const(mrb, bob, "BasicObject", _obj_value(bob));
  _define_const(mrb, obj, "BasicObject", _obj_value(bob));
  _define_const(mrb, obj, "Object",      _obj_value(obj));
  _define_const(mrb, obj, "Module",      _obj_value(mod));
  _define_const(mrb, obj, "Class",       _obj_value(cls));

  /* name each classes */
  _class_name_class(mrb, NULL, bob, _intern_lit(mrb, "BasicObject"));
  _class_name_class(mrb, NULL, obj, _intern_lit(mrb, "Object")); /* 15.2.1 */
  _class_name_class(mrb, NULL, mod, _intern_lit(mrb, "Module")); /* 15.2.2 */
  _class_name_class(mrb, NULL, cls, _intern_lit(mrb, "Class"));  /* 15.2.3 */

  mrb->proc_class = _define_class(mrb, "Proc", mrb->object_class);  /* 15.2.17 */
  MRB_SET_INSTANCE_TT(mrb->proc_class, MRB_TT_PROC);

  MRB_SET_INSTANCE_TT(cls, MRB_TT_CLASS);
  _define_method(mrb, bob, "initialize",              _bob_init,             MRB_ARGS_NONE());
  _define_method(mrb, bob, "!",                       _bob_not,              MRB_ARGS_NONE());
  _define_method(mrb, bob, "==",                      _obj_equal_m,          MRB_ARGS_REQ(1)); /* 15.3.1.3.1  */
  _define_method(mrb, bob, "!=",                      _obj_not_equal_m,      MRB_ARGS_REQ(1));
  _define_method(mrb, bob, "__id__",                  _obj_id_m,             MRB_ARGS_NONE()); /* 15.3.1.3.4  */
  _define_method(mrb, bob, "__send__",                _f_send,               MRB_ARGS_ANY());  /* 15.3.1.3.5  */
  _define_method(mrb, bob, "instance_eval",           _obj_instance_eval,    MRB_ARGS_ANY());  /* 15.3.1.3.18 */

  _define_class_method(mrb, cls, "new",               _class_new_class,      MRB_ARGS_OPT(1));
  _define_method(mrb, cls, "superclass",              _class_superclass,     MRB_ARGS_NONE()); /* 15.2.3.3.4 */
  _define_method(mrb, cls, "new",                     _instance_new,         MRB_ARGS_ANY());  /* 15.2.3.3.3 */
  _define_method(mrb, cls, "initialize",              _class_initialize,     MRB_ARGS_OPT(1)); /* 15.2.3.3.1 */
  _define_method(mrb, cls, "inherited",               _bob_init,             MRB_ARGS_REQ(1));

  MRB_SET_INSTANCE_TT(mod, MRB_TT_MODULE);
  _define_method(mrb, mod, "extend_object",           _mod_extend_object,    MRB_ARGS_REQ(1)); /* 15.2.2.4.25 */
  _define_method(mrb, mod, "extended",                _bob_init,             MRB_ARGS_REQ(1)); /* 15.2.2.4.26 */
  _define_method(mrb, mod, "prepended",               _bob_init,             MRB_ARGS_REQ(1));
  _define_method(mrb, mod, "prepend_features",        _mod_prepend_features, MRB_ARGS_REQ(1));
  _define_method(mrb, mod, "include?",                _mod_include_p,        MRB_ARGS_REQ(1)); /* 15.2.2.4.28 */
  _define_method(mrb, mod, "append_features",         _mod_append_features,  MRB_ARGS_REQ(1)); /* 15.2.2.4.10 */
  _define_method(mrb, mod, "class_eval",              _mod_module_eval,      MRB_ARGS_ANY());  /* 15.2.2.4.15 */
  _define_method(mrb, mod, "included",                _bob_init,             MRB_ARGS_REQ(1)); /* 15.2.2.4.29 */
  _define_method(mrb, mod, "initialize",              _mod_initialize,       MRB_ARGS_NONE()); /* 15.2.2.4.31 */
  _define_method(mrb, mod, "module_eval",             _mod_module_eval,      MRB_ARGS_ANY());  /* 15.2.2.4.35 */
  _define_method(mrb, mod, "module_function",         _mod_module_function,  MRB_ARGS_ANY());
  _define_method(mrb, mod, "private",                 _mod_dummy_visibility, MRB_ARGS_ANY());  /* 15.2.2.4.36 */
  _define_method(mrb, mod, "protected",               _mod_dummy_visibility, MRB_ARGS_ANY());  /* 15.2.2.4.37 */
  _define_method(mrb, mod, "public",                  _mod_dummy_visibility, MRB_ARGS_ANY());  /* 15.2.2.4.38 */
  _define_method(mrb, mod, "attr_reader",             _mod_attr_reader,      MRB_ARGS_ANY());  /* 15.2.2.4.13 */
  _define_method(mrb, mod, "attr_writer",             _mod_attr_writer,      MRB_ARGS_ANY());  /* 15.2.2.4.14 */
  _define_method(mrb, mod, "to_s",                    _mod_to_s,             MRB_ARGS_NONE());
  _define_method(mrb, mod, "inspect",                 _mod_to_s,             MRB_ARGS_NONE());
  _define_method(mrb, mod, "alias_method",            _mod_alias,            MRB_ARGS_ANY());  /* 15.2.2.4.8 */
  _define_method(mrb, mod, "ancestors",               _mod_ancestors,        MRB_ARGS_NONE()); /* 15.2.2.4.9 */
  _define_method(mrb, mod, "undef_method",            _mod_undef,            MRB_ARGS_ANY());  /* 15.2.2.4.41 */
  _define_method(mrb, mod, "const_defined?",          _mod_const_defined,    MRB_ARGS_ARG(1,1)); /* 15.2.2.4.20 */
  _define_method(mrb, mod, "const_get",               _mod_const_get,        MRB_ARGS_REQ(1)); /* 15.2.2.4.21 */
  _define_method(mrb, mod, "const_set",               _mod_const_set,        MRB_ARGS_REQ(2)); /* 15.2.2.4.23 */
  _define_method(mrb, mod, "remove_const",            _mod_remove_const,     MRB_ARGS_REQ(1)); /* 15.2.2.4.40 */
  _define_method(mrb, mod, "const_missing",           _mod_const_missing,    MRB_ARGS_REQ(1));
  _define_method(mrb, mod, "method_defined?",         _mod_method_defined,   MRB_ARGS_REQ(1)); /* 15.2.2.4.34 */
  _define_method(mrb, mod, "define_method",           mod_define_method,        MRB_ARGS_ARG(1,1));
  _define_method(mrb, mod, "===",                     _mod_eqq,              MRB_ARGS_REQ(1));

  _undef_method(mrb, cls, "append_features");
  _undef_method(mrb, cls, "extend_object");

  mrb->top_self = (struct RObject*)_obj_alloc(mrb, MRB_TT_OBJECT, mrb->object_class);
  _define_singleton_method(mrb, mrb->top_self, "inspect", inspect_main, MRB_ARGS_NONE());
  _define_singleton_method(mrb, mrb->top_self, "to_s", inspect_main, MRB_ARGS_NONE());
  _define_singleton_method(mrb, mrb->top_self, "define_method", top_define_method, MRB_ARGS_ARG(1,1));
}
