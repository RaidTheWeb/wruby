/*
** mruby/data.h - Data class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_DATA_H
#define MRUBY_DATA_H

#include "common.h"

/**
 * Custom C wrapped data.
 *
 * Defining Ruby wrappers around native objects.
 */
MRB_BEGIN_DECL

/**
 * Custom data type description.
 */
typedef struct _data_type {
  /** data type name */
  const char *struct_name;

  /** data type release function pointer */
  void (*dfree)(_state *mrb, void*);
} _data_type;

struct RData {
  MRB_OBJECT_HEADER;
  struct iv_tbl *iv;
  const _data_type *type;
  void *data;
};

MRB_API struct RData *_data_object_alloc(_state *mrb, struct RClass* klass, void *datap, const _data_type *type);

#define Data_Wrap_Struct(mrb,klass,type,ptr)\
  _data_object_alloc(mrb,klass,ptr,type)

#define Data_Make_Struct(mrb,klass,strct,type,sval,data) do { \
  sval = _malloc(mrb, sizeof(strct));                     \
  { static const strct zero = { 0 }; *sval = zero; };\
  data = Data_Wrap_Struct(mrb,klass,type,sval);\
} while (0)

#define RDATA(obj)         ((struct RData *)(_ptr(obj)))
#define DATA_PTR(d)        (RDATA(d)->data)
#define DATA_TYPE(d)       (RDATA(d)->type)
MRB_API void _data_check_type(_state *mrb, _value, const _data_type*);
MRB_API void *_data_get_ptr(_state *mrb, _value, const _data_type*);
#define DATA_GET_PTR(mrb,obj,dtype,type) (type*)_data_get_ptr(mrb,obj,dtype)
MRB_API void *_data_check_get_ptr(_state *mrb, _value, const _data_type*);
#define DATA_CHECK_GET_PTR(mrb,obj,dtype,type) (type*)_data_check_get_ptr(mrb,obj,dtype)

/* obsolete functions and macros */
#define _data_check_and_get(mrb,obj,dtype) _data_get_ptr(mrb,obj,dtype)
#define _get_datatype(mrb,val,type) _data_get_ptr(mrb, val, type)
#define _check_datatype(mrb,val,type) _data_get_ptr(mrb, val, type)
#define Data_Get_Struct(mrb,obj,type,sval) do {\
  *(void**)&sval = _data_get_ptr(mrb, obj, type); \
} while (0)

static inline void
_data_init(_value v, void *ptr, const _data_type *type)
{
  _assert(_type(v) == MRB_TT_DATA);
  DATA_PTR(v) = ptr;
  DATA_TYPE(v) = type;
}

MRB_END_DECL

#endif /* MRUBY_DATA_H */
