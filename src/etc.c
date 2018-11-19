/*
** etc.c -
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <mruby/re.h>
#include <mruby/irep.h>

MRB_API struct RData*
_data_object_alloc(_state *mrb, struct RClass *klass, void *ptr, const _data_type *type)
{
  struct RData *data;

  data = (struct RData*)_obj_alloc(mrb, MRB_TT_DATA, klass);
  data->data = ptr;
  data->type = type;

  return data;
}

MRB_API void
_data_check_type(_state *mrb, _value obj, const _data_type *type)
{
  if (_type(obj) != MRB_TT_DATA) {
    _check_type(mrb, obj, MRB_TT_DATA);
  }
  if (DATA_TYPE(obj) != type) {
    const _data_type *t2 = DATA_TYPE(obj);

    if (t2) {
      _raisef(mrb, E_TYPE_ERROR, "wrong argument type %S (expected %S)",
                 _str_new_cstr(mrb, t2->struct_name), _str_new_cstr(mrb, type->struct_name));
    }
    else {
      struct RClass *c = _class(mrb, obj);

      _raisef(mrb, E_TYPE_ERROR, "uninitialized %S (expected %S)",
                 _obj_value(c), _str_new_cstr(mrb, type->struct_name));
    }
  }
}

MRB_API void*
_data_check_get_ptr(_state *mrb, _value obj, const _data_type *type)
{
  if (_type(obj) != MRB_TT_DATA) {
    return NULL;
  }
  if (DATA_TYPE(obj) != type) {
    return NULL;
  }
  return DATA_PTR(obj);
}

MRB_API void*
_data_get_ptr(_state *mrb, _value obj, const _data_type *type)
{
  _data_check_type(mrb, obj, type);
  return DATA_PTR(obj);
}

MRB_API _sym
_obj_to_sym(_state *mrb, _value name)
{
  _sym id;

  switch (_type(name)) {
    default:
      name = _check_string_type(mrb, name);
      if (_nil_p(name)) {
        name = _inspect(mrb, name);
        _raisef(mrb, E_TYPE_ERROR, "%S is not a symbol", name);
        /* not reached */
      }
      /* fall through */
    case MRB_TT_STRING:
      name = _str_intern(mrb, name);
      /* fall through */
    case MRB_TT_SYMBOL:
      id = _symbol(name);
  }
  return id;
}

MRB_API _int
#ifdef MRB_WITHOUT_FLOAT
_fixnum_id(_int f)
#else
_float_id(_float f)
#endif
{
  const char *p = (const char*)&f;
  int len = sizeof(f);
  uint32_t id = 0;

#ifndef MRB_WITHOUT_FLOAT
  /* normalize -0.0 to 0.0 */
  if (f == 0) f = 0.0;
#endif
  while (len--) {
    id = id*65599 + *p;
    p++;
  }
  id = id + (id>>5);

  return (_int)id;
}

MRB_API _int
_obj_id(_value obj)
{
  _int tt = _type(obj);

#define MakeID2(p,t) (_int)(((intptr_t)(p))^(t))
#define MakeID(p)    MakeID2(p,tt)

  switch (tt) {
  case MRB_TT_FREE:
  case MRB_TT_UNDEF:
    return MakeID(0); /* not define */
  case MRB_TT_FALSE:
    if (_nil_p(obj))
      return MakeID(1);
    return MakeID(0);
  case MRB_TT_TRUE:
    return MakeID(1);
  case MRB_TT_SYMBOL:
    return MakeID(_symbol(obj));
  case MRB_TT_FIXNUM:
#ifdef MRB_WITHOUT_FLOAT
    return MakeID(_fixnum_id(_fixnum(obj)));
#else
    return MakeID2(_float_id((_float)_fixnum(obj)), MRB_TT_FLOAT);
  case MRB_TT_FLOAT:
    return MakeID(_float_id(_float(obj)));
#endif
  case MRB_TT_STRING:
  case MRB_TT_OBJECT:
  case MRB_TT_CLASS:
  case MRB_TT_MODULE:
  case MRB_TT_ICLASS:
  case MRB_TT_SCLASS:
  case MRB_TT_PROC:
  case MRB_TT_ARRAY:
  case MRB_TT_HASH:
  case MRB_TT_RANGE:
  case MRB_TT_EXCEPTION:
  case MRB_TT_FILE:
  case MRB_TT_DATA:
  case MRB_TT_ISTRUCT:
  default:
    return MakeID(_ptr(obj));
  }
}

#ifdef MRB_WORD_BOXING
#ifndef MRB_WITHOUT_FLOAT
MRB_API _value
_word_boxing_float_value(_state *mrb, _float f)
{
  _value v;

  v.value.p = _obj_alloc(mrb, MRB_TT_FLOAT, mrb->float_class);
  v.value.fp->f = f;
  return v;
}

MRB_API _value
_word_boxing_float_pool(_state *mrb, _float f)
{
  struct RFloat *nf = (struct RFloat *)_malloc(mrb, sizeof(struct RFloat));
  nf->tt = MRB_TT_FLOAT;
  nf->c = mrb->float_class;
  nf->f = f;
  return _obj_value(nf);
}
#endif  /* MRB_WITHOUT_FLOAT */

MRB_API _value
_word_boxing_cptr_value(_state *mrb, void *p)
{
  _value v;

  v.value.p = _obj_alloc(mrb, MRB_TT_CPTR, mrb->object_class);
  v.value.vp->p = p;
  return v;
}
#endif  /* MRB_WORD_BOXING */

MRB_API _bool
_regexp_p(_state *mrb, _value v)
{
  if (mrb->flags & MRB_STATE_NO_REGEXP) {
    return FALSE;
  }
  if ((mrb->flags & MRB_STATE_REGEXP) || _class_defined(mrb, REGEXP_CLASS)) {
    mrb->flags |= MRB_STATE_REGEXP;
    return _obj_is_kind_of(mrb, v, _class_get(mrb, REGEXP_CLASS));
  }
  else {
    mrb->flags |= MRB_STATE_REGEXP;
    mrb->flags |= MRB_STATE_NO_REGEXP;
  }
  return FALSE;
}

#if defined _MSC_VER && _MSC_VER < 1900

#ifndef va_copy
static void
_msvc_va_copy(va_list *dest, va_list src)
{
  *dest = src;
}
#define va_copy(dest, src) _msvc_va_copy(&(dest), src)
#endif

MRB_API int
_msvc_vsnprintf(char *s, size_t n, const char *format, va_list arg)
{
  int cnt;
  va_list argcp;
  va_copy(argcp, arg);
  if (n == 0 || (cnt = _vsnprintf_s(s, n, _TRUNCATE, format, argcp)) < 0) {
    cnt = _vscprintf(format, arg);
  }
  va_end(argcp);
  return cnt;
}

MRB_API int
_msvc_snprintf(char *s, size_t n, const char *format, ...)
{
  va_list arg;
  int ret;
  va_start(arg, format);
  ret = _msvc_vsnprintf(s, n, format, arg);
  va_end(arg);
  return ret;
}

#endif  /* defined _MSC_VER && _MSC_VER < 1900 */
