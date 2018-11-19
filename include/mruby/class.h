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
$BEGIN_DECL

struct RClass {
  $OBJECT_HEADER;
  struct iv_tbl *iv;
  struct kh_mt *mt;
  struct RClass *super;
};

#define $class_ptr(v)    ((struct RClass*)($ptr(v)))

static inline struct RClass*
$class($state *mrb, $value v)
{
  switch ($type(v)) {
  case $TT_FALSE:
    if ($fixnum(v))
      return mrb->false_class;
    return mrb->nil_class;
  case $TT_TRUE:
    return mrb->true_class;
  case $TT_SYMBOL:
    return mrb->symbol_class;
  case $TT_FIXNUM:
    return mrb->fixnum_class;
#ifndef $WITHOUT_FLOAT
  case $TT_FLOAT:
    return mrb->float_class;
#endif
  case $TT_CPTR:
    return mrb->object_class;
  case $TT_ENV:
    return NULL;
  default:
    return $obj_ptr(v)->c;
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
#define $FL_CLASS_IS_PREPENDED (1 << 19)
#define $FL_CLASS_IS_ORIGIN (1 << 18)
#define $CLASS_ORIGIN(c) do {\
  if (c->flags & $FL_CLASS_IS_PREPENDED) {\
    c = c->super;\
    while (!(c->flags & $FL_CLASS_IS_ORIGIN)) {\
      c = c->super;\
    }\
  }\
} while (0)
#define $FL_CLASS_IS_INHERITED (1 << 17)
#define $INSTANCE_TT_MASK (0xFF)
#define $SET_INSTANCE_TT(c, tt) c->flags = ((c->flags & ~$INSTANCE_TT_MASK) | (char)tt)
#define $INSTANCE_TT(c) (enum $vtype)(c->flags & $INSTANCE_TT_MASK)

$API struct RClass* $define_class_id($state*, $sym, struct RClass*);
$API struct RClass* $define_module_id($state*, $sym);
$API struct RClass *$vm_define_class($state*, $value, $value, $sym);
$API struct RClass *$vm_define_module($state*, $value, $sym);
$API void $define_method_raw($state*, struct RClass*, $sym, $method_t);
$API void $define_method_id($state *mrb, struct RClass *c, $sym mid, $func_t func, $aspec aspec);
$API void $alias_method($state*, struct RClass *c, $sym a, $sym b);

$API $method_t $method_search_vm($state*, struct RClass**, $sym);
$API $method_t $method_search($state*, struct RClass*, $sym);

$API struct RClass* $class_real(struct RClass* cl);

void $class_name_class($state*, struct RClass*, struct RClass*, $sym);
$value $class_find_path($state*, struct RClass*);
void $gc_mark_mt($state*, struct RClass*);
size_t $gc_mark_mt_size($state*, struct RClass*);
void $gc_free_mt($state*, struct RClass*);

$END_DECL

#endif  /* MRUBY_CLASS_H */
