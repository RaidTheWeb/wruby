/*
** mruby/object.h - mruby object definition
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_OBJECT_H
#define MRUBY_OBJECT_H

#define OBJECT_HEADER \
  enum vtype tt:8;\
  uint32_t color:3;\
  uint32_t flags:21;\
  struct RClass *c;\
  struct RBasic *gcnext

#define FLAG_TEST(obj, flag) ((obj)->flags & flag)


struct RBasic {
  OBJECT_HEADER;
};
#define basic_ptr(v) ((struct RBasic*)(ptr(v)))

#define FL_OBJ_IS_FROZEN (1 << 20)
#define FROZEN_P(o) ((o)->flags & FL_OBJ_IS_FROZEN)
#define SET_FROZEN_FLAG(o) ((o)->flags |= FL_OBJ_IS_FROZEN)
#define UNSET_FROZEN_FLAG(o) ((o)->flags &= ~FL_OBJ_IS_FROZEN)

struct RObject {
  OBJECT_HEADER;
  struct iv_tbl *iv;
};
#define obj_ptr(v)   ((struct RObject*)(ptr(v)))

#define immediate_p(x) (type(x) < TT_HAS_BASIC)
#define special_const_p(x) immediate_p(x)

struct RFiber {
  OBJECT_HEADER;
  struct context *cxt;
};

#endif  /* MRUBY_OBJECT_H */
