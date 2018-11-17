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
BEGIN_DECL

/**
 * Custom data type description.
 */
typedef struct data_type {
  /** data type name */
  const char *struct_name;

  /** data type release function pointer */
  void (*dfree)(state *mrb, void*);
} data_type;

struct RData {
  OBJECT_HEADER;
  struct iv_tbl *iv;
  const data_type *type;
  void *data;
};

API struct RData *data_object_alloc(state *mrb, struct RClass* klass, void *datap, const data_type *type);

#define Data_Wrap_Struct(mrb,klass,type,ptr)\
  data_object_alloc(mrb,klass,ptr,type)

#define Data_Make_Struct(mrb,klass,strct,type,sval,data) do { \
  sval = malloc(mrb, sizeof(strct));                     \
  { static const strct zero = { 0 }; *sval = zero; };\
  data = Data_Wrap_Struct(mrb,klass,type,sval);\
} while (0)

#define RDATA(obj)         ((struct RData *)(ptr(obj)))
#define DATA_PTR(d)        (RDATA(d)->data)
#define DATA_TYPE(d)       (RDATA(d)->type)
API void data_check_type(state *mrb, value, const data_type*);
API void *data_get_ptr(state *mrb, value, const data_type*);
#define DATA_GET_PTR(mrb,obj,dtype,type) (type*)data_get_ptr(mrb,obj,dtype)
API void *data_check_get_ptr(state *mrb, value, const data_type*);
#define DATA_CHECK_GET_PTR(mrb,obj,dtype,type) (type*)data_check_get_ptr(mrb,obj,dtype)

/* obsolete functions and macros */
#define data_check_and_get(mrb,obj,dtype) data_get_ptr(mrb,obj,dtype)
#define get_datatype(mrb,val,type) data_get_ptr(mrb, val, type)
#define check_datatype(mrb,val,type) data_get_ptr(mrb, val, type)
#define Data_Get_Struct(mrb,obj,type,sval) do {\
  *(void**)&sval = data_get_ptr(mrb, obj, type); \
} while (0)

static inline void
data_init(value v, void *ptr, const data_type *type)
{
  assert(type(v) == TT_DATA);
  DATA_PTR(v) = ptr;
  DATA_TYPE(v) = type;
}

END_DECL

#endif /* MRUBY_DATA_H */
