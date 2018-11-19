/*
** mruby/class.h - Class class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_CLASS_H
#define MRUBY_CLASS_H

#include "common.h"

/**
 * Class class
 */
MRB_BEGIN_DECL

struct RClass {
  MRB_OBJECT_HEADER;
  struct iv_tbl *iv;
  struct kh_mt *mt;
  struct RClass *super;
};

#define _class_ptr(v)    ((struct RClass*)(_ptr(v)))

static inline struct RClass*
_class(state *mrb, value v)
{
  switch (_type(v)) {
  case MRB_TT_FALSE:
    if (_fixnum(v))
      return mrb->false_class;
    return mrb->nil_class;
  case MRB_TT_TRUE:
    return mrb->true_class;
  case MRB_TT_SYMBOL:
    return mrb->symbol_class;
  case MRB_TT_FIXNUM:
    return mrb->fixnum_class;
#ifndef MRB_WITHOUT_FLOAT
  case MRB_TT_FLOAT:
    return mrb->float_class;
#endif
  case MRB_TT_CPTR:
    return mrb->object_class;
  case MRB_TT_ENV:
    return NULL;
  default:
    return _obj_ptr(v)->c;
  }
}

/* flags:
   20: frozen
   19: is_prepended
   18: is_origin
   17: is_inherited (used by method cache)
   16: unused
   0-15: instance type
*/
#define MRB_FL_CLASS_IS_PREPENDED (1 << 19)
#define MRB_FL_CLASS_IS_ORIGIN (1 << 18)
#define MRB_CLASS_ORIGIN(c) do {\
  if (c->flags & MRB_FL_CLASS_IS_PREPENDED) {\
    c = c->super;\
    while (!(c->flags & MRB_FL_CLASS_IS_ORIGIN)) {\
      c = c->super;\
    }\
  }\
} while (0)
#define MRB_FL_CLASS_IS_INHERITED (1 << 17)
#define MRB_INSTANCE_TT_MASK (0xFF)
#define MRB_SET_INSTANCE_TT(c, tt) c->flags = ((c->flags & ~MRB_INSTANCE_TT_MASK) | (char)tt)
#define MRB_INSTANCE_TT(c) (enum _vtype)(c->flags & MRB_INSTANCE_TT_MASK)

MRB_API struct RClass* _define_class_id(state*, _sym, struct RClass*);
MRB_API struct RClass* _define_module_id(state*, _sym);
MRB_API struct RClass *_vm_define_class(state*, value, value, _sym);
MRB_API struct RClass *_vm_define_module(state*, value, _sym);
MRB_API void _define_method_raw(state*, struct RClass*, _sym, _method_t);
MRB_API void _define_method_id(state *mrb, struct RClass *c, _sym mid, _func_t func, _aspec aspec);
MRB_API void _alias_method(state*, struct RClass *c, _sym a, _sym b);

MRB_API _method_t _method_search_vm(state*, struct RClass**, _sym);
MRB_API _method_t _method_search(state*, struct RClass*, _sym);

MRB_API struct RClass* _class_real(struct RClass* cl);

void _class_name_class(state*, struct RClass*, struct RClass*, _sym);
value _class_find_path(state*, struct RClass*);
void _gc_mark_mt(state*, struct RClass*);
size_t _gc_mark_mt_size(state*, struct RClass*);
void _gc_free_mt(state*, struct RClass*);

MRB_END_DECL

#endif  /* MRUBY_CLASS_H */
