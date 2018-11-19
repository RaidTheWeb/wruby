/*
** mruby/boxing_no.h - unboxed $value definition
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_BOXING_NO_H
#define MRUBY_BOXING_NO_H

#define $FIXNUM_SHIFT 0
#define $TT_HAS_BASIC $TT_OBJECT

typedef struct $value {
  union {
#ifndef $WITHOUT_FLOAT
    $float f;
#endif
    void *p;
    $int i;
    $sym sym;
  } value;
  enum $vtype tt;
} $value;

#ifndef $WITHOUT_FLOAT
#define $float_pool(mrb,f) $float_value(mrb,f)
#endif

#define $ptr(o)      (o).value.p
#define $cptr(o)     $ptr(o)
#ifndef $WITHOUT_FLOAT
#define $float(o)    (o).value.f
#endif
#define $fixnum(o)   (o).value.i
#define $symbol(o)   (o).value.sym
#define $type(o)     (o).tt

#define BOXNIX_SET_VALUE(o, ttt, attr, v) do {\
  (o).tt = ttt;\
  (o).attr = v;\
} while (0)

#define SET_NIL_VALUE(r) BOXNIX_SET_VALUE(r, $TT_FALSE, value.i, 0)
#define SET_FALSE_VALUE(r) BOXNIX_SET_VALUE(r, $TT_FALSE, value.i, 1)
#define SET_TRUE_VALUE(r) BOXNIX_SET_VALUE(r, $TT_TRUE, value.i, 1)
#define SET_BOOL_VALUE(r,b) BOXNIX_SET_VALUE(r, b ? $TT_TRUE : $TT_FALSE, value.i, 1)
#define SET_INT_VALUE(r,n) BOXNIX_SET_VALUE(r, $TT_FIXNUM, value.i, (n))
#ifndef $WITHOUT_FLOAT
#define SET_FLOAT_VALUE(mrb,r,v) BOXNIX_SET_VALUE(r, $TT_FLOAT, value.f, (v))
#endif
#define SET_SYM_VALUE(r,v) BOXNIX_SET_VALUE(r, $TT_SYMBOL, value.sym, (v))
#define SET_OBJ_VALUE(r,v) BOXNIX_SET_VALUE(r, (((struct RObject*)(v))->tt), value.p, (v))
#define SET_CPTR_VALUE(mrb,r,v) BOXNIX_SET_VALUE(r, $TT_CPTR, value.p, v)
#define SET_UNDEF_VALUE(r) BOXNIX_SET_VALUE(r, $TT_UNDEF, value.i, 0)

#endif  /* MRUBY_BOXING_NO_H */
