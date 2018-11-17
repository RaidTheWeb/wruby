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

API struct RData*
data_object_alloc(state *mrb, struct RClass *klass, void *ptr, const data_type *type)
{
  struct RData *data;

  data = (struct RData*)obj_alloc(mrb, TT_DATA, klass);
  data->data = ptr;
  data->type = type;

  return data;
}

API void
data_check_type(state *mrb, value obj, const data_type *type)
{
  if (type(obj) != TT_DATA) {
    check_type(mrb, obj, TT_DATA);
  }
  if (DATA_TYPE(obj) != type) {
    const data_type *t2 = DATA_TYPE(obj);

    if (t2) {
      raisef(mrb, E_TYPE_ERROR, "wrong argument type %S (expected %S)",
                 str_new_cstr(mrb, t2->struct_name), str_new_cstr(mrb, type->struct_name));
    }
    else {
      struct RClass *c = class(mrb, obj);

      raisef(mrb, E_TYPE_ERROR, "uninitialized %S (expected %S)",
                 obj_value(c), str_new_cstr(mrb, type->struct_name));
    }
  }
}

API void*
data_check_get_ptr(state *mrb, value obj, const data_type *type)
{
  if (type(obj) != TT_DATA) {
    return NULL;
  }
  if (DATA_TYPE(obj) != type) {
    return NULL;
  }
  return DATA_PTR(obj);
}

API void*
data_get_ptr(state *mrb, value obj, const data_type *type)
{
  data_check_type(mrb, obj, type);
  return DATA_PTR(obj);
}

API sym
obj_to_sym(state *mrb, value name)
{
  sym id;

  switch (type(name)) {
    default:
      name = check_string_type(mrb, name);
      if (nil_p(name)) {
        name = inspect(mrb, name);
        raisef(mrb, E_TYPE_ERROR, "%S is not a symbol", name);
        /* not reached */
      }
      /* fall through */
    case TT_STRING:
      name = str_intern(mrb, name);
      /* fall through */
    case TT_SYMBOL:
      id = symbol(name);
  }
  return id;
}

API int
#ifdef WITHOUT_FLOAT
fixnum_id(int f)
#else
float_id(float f)
#endif
{
  const char *p = (const char*)&f;
  int len = sizeof(f);
  uint32_t id = 0;

#ifndef WITHOUT_FLOAT
  /* normalize -0.0 to 0.0 */
  if (f == 0) f = 0.0;
#endif
  while (len--) {
    id = id*65599 + *p;
    p++;
  }
  id = id + (id>>5);

  return (int)id;
}

API int
obj_id(value obj)
{
  int tt = type(obj);

#define MakeID2(p,t) (int)(((intptr_t)(p))^(t))
#define MakeID(p)    MakeID2(p,tt)

  switch (tt) {
  case TT_FREE:
  case TT_UNDEF:
    return MakeID(0); /* not define */
  case TT_FALSE:
    if (nil_p(obj))
      return MakeID(1);
    return MakeID(0);
  case TT_TRUE:
    return MakeID(1);
  case TT_SYMBOL:
    return MakeID(symbol(obj));
  case TT_FIXNUM:
#ifdef WITHOUT_FLOAT
    return MakeID(fixnum_id(fixnum(obj)));
#else
    return MakeID2(float_id((float)fixnum(obj)), TT_FLOAT);
  case TT_FLOAT:
    return MakeID(float_id(float(obj)));
#endif
  case TT_STRING:
  case TT_OBJECT:
  case TT_CLASS:
  case TT_MODULE:
  case TT_ICLASS:
  case TT_SCLASS:
  case TT_PROC:
  case TT_ARRAY:
  case TT_HASH:
  case TT_RANGE:
  case TT_EXCEPTION:
  case TT_FILE:
  case TT_DATA:
  case TT_ISTRUCT:
  default:
    return MakeID(ptr(obj));
  }
}

#ifdef WORD_BOXING
#ifndef WITHOUT_FLOAT
API value
word_boxing_float_value(state *mrb, float f)
{
  value v;

  v.value.p = obj_alloc(mrb, TT_FLOAT, mrb->float_class);
  v.value.fp->f = f;
  return v;
}

API value
word_boxing_float_pool(state *mrb, float f)
{
  struct RFloat *nf = (struct RFloat *)malloc(mrb, sizeof(struct RFloat));
  nf->tt = TT_FLOAT;
  nf->c = mrb->float_class;
  nf->f = f;
  return obj_value(nf);
}
#endif  /* WITHOUT_FLOAT */

API value
word_boxing_cptr_value(state *mrb, void *p)
{
  value v;

  v.value.p = obj_alloc(mrb, TT_CPTR, mrb->object_class);
  v.value.vp->p = p;
  return v;
}
#endif  /* WORD_BOXING */

API bool
regexp_p(state *mrb, value v)
{
  if (mrb->flags & STATE_NO_REGEXP) {
    return FALSE;
  }
  if ((mrb->flags & STATE_REGEXP) || class_defined(mrb, REGEXP_CLASS)) {
    mrb->flags |= STATE_REGEXP;
    return obj_is_kind_of(mrb, v, class_get(mrb, REGEXP_CLASS));
  }
  else {
    mrb->flags |= STATE_REGEXP;
    mrb->flags |= STATE_NO_REGEXP;
  }
  return FALSE;
}

#if defined _MSC_VER && _MSC_VER < 1900

#ifndef va_copy
static void
msvc_va_copy(va_list *dest, va_list src)
{
  *dest = src;
}
#define va_copy(dest, src) msvc_va_copy(&(dest), src)
#endif

API int
msvc_vsnprintf(char *s, size_t n, const char *format, va_list arg)
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

API int
msvc_snprintf(char *s, size_t n, const char *format, ...)
{
  va_list arg;
  int ret;
  va_start(arg, format);
  ret = msvc_vsnprintf(s, n, format, arg);
  va_end(arg);
  return ret;
}

#endif  /* defined _MSC_VER && _MSC_VER < 1900 */
