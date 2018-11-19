/*
** mruby/boxing_no.h - unboxed _value definition
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_BOXING_NO_H
#define MRUBY_BOXING_NO_H

#define MRB_FIXNUM_SHIFT 0
#define MRB_TT_HAS_BASIC MRB_TT_OBJECT

typedef struct _value {
  union {
#ifndef MRB_WITHOUT_FLOAT
    _float f;
#endif
    void *p;
    _int i;
    _sym sym;
  } value;
  enum _vtype tt;
} _value;

#ifndef MRB_WITHOUT_FLOAT
#define _float_pool(mrb,f) _float_value(mrb,f)
#endif

#define _ptr(o)      (o).value.p
#define _cptr(o)     _ptr(o)
#ifndef MRB_WITHOUT_FLOAT
#define _float(o)    (o).value.f
#endif
#define _fixnum(o)   (o).value.i
#define _symbol(o)   (o).value.sym
#define _type(o)     (o).tt

#define BOXNIX_SET_VALUE(o, ttt, attr, v) do {\
  (o).tt = ttt;\
  (o).attr = v;\
} while (0)

#define SET_NIL_VALUE(r) BOXNIX_SET_VALUE(r, MRB_TT_FALSE, value.i, 0)
#define SET_FALSE_VALUE(r) BOXNIX_SET_VALUE(r, MRB_TT_FALSE, value.i, 1)
#define SET_TRUE_VALUE(r) BOXNIX_SET_VALUE(r, MRB_TT_TRUE, value.i, 1)
#define SET_BOOL_VALUE(r,b) BOXNIX_SET_VALUE(r, b ? MRB_TT_TRUE : MRB_TT_FALSE, value.i, 1)
#define SET_INT_VALUE(r,n) BOXNIX_SET_VALUE(r, MRB_TT_FIXNUM, value.i, (n))
#ifndef MRB_WITHOUT_FLOAT
#define SET_FLOAT_VALUE(mrb,r,v) BOXNIX_SET_VALUE(r, MRB_TT_FLOAT, value.f, (v))
#endif
#define SET_SYM_VALUE(r,v) BOXNIX_SET_VALUE(r, MRB_TT_SYMBOL, value.sym, (v))
#define SET_OBJ_VALUE(r,v) BOXNIX_SET_VALUE(r, (((struct RObject*)(v))->tt), value.p, (v))
#define SET_CPTR_VALUE(mrb,r,v) BOXNIX_SET_VALUE(r, MRB_TT_CPTR, value.p, v)
#define SET_UNDEF_VALUE(r) BOXNIX_SET_VALUE(r, MRB_TT_UNDEF, value.i, 0)

#endif  /* MRUBY_BOXING_NO_H */
