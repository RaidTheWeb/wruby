/*
** mruby/object.h - mruby object definition
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_OBJECT_H
#define MRUBY_OBJECT_H

#define MRB_OBJECT_HEADER \
  enum _vtype tt:8;\
  uint32_t color:3;\
  uint32_t flags:21;\
  struct RClass *c;\
  struct RBasic *gcnext

#define MRB_FLAG_TEST(obj, flag) ((obj)->flags & flag)


struct RBasic {
  MRB_OBJECT_HEADER;
};
#define _basic_ptr(v) ((struct RBasic*)(_ptr(v)))

#define MRB_FL_OBJ_IS_FROZEN (1 << 20)
#define MRB_FROZEN_P(o) ((o)->flags & MRB_FL_OBJ_IS_FROZEN)
#define MRB_SET_FROZEN_FLAG(o) ((o)->flags |= MRB_FL_OBJ_IS_FROZEN)
#define MRB_UNSET_FROZEN_FLAG(o) ((o)->flags &= ~MRB_FL_OBJ_IS_FROZEN)

struct RObject {
  MRB_OBJECT_HEADER;
  struct iv_tbl *iv;
};
#define _obj_ptr(v)   ((struct RObject*)(_ptr(v)))

#define _immediate_p(x) (_type(x) < MRB_TT_HAS_BASIC)
#define _special_const_p(x) _immediate_p(x)

struct RFiber {
  MRB_OBJECT_HEADER;
  struct _context *cxt;
};

#endif  /* MRUBY_OBJECT_H */
