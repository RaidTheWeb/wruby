/*
** mruby/boxing_word.h - word boxing value definition
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_BOXING_WORD_H
#define MRUBY_BOXING_WORD_H

#if defined(MRB_INT16)
# error MRB_INT16 is too small for MRB_WORD_BOXING.
#endif

#if defined(MRB_INT64) && !defined(MRB_64BIT)
#error MRB_INT64 cannot be used with MRB_WORD_BOXING in 32-bit mode.
#endif

#ifndef MRB_WITHOUT_FLOAT
struct RFloat {
  MRB_OBJECT_HEADER;
  _float f;
};
#endif

struct RCptr {
  MRB_OBJECT_HEADER;
  void *p;
};

#define MRB_FIXNUM_SHIFT 1
#ifdef MRB_WITHOUT_FLOAT
#define MRB_TT_HAS_BASIC MRB_TT_CPTR
#else
#define MRB_TT_HAS_BASIC MRB_TT_FLOAT
#endif

enum _special_consts {
  MRB_Qnil    = 0,
  MRB_Qfalse  = 2,
  MRB_Qtrue   = 4,
  MRB_Qundef  = 6,
};

#define MRB_FIXNUM_FLAG   0x01
#define MRB_SYMBOL_FLAG   0x0e
#define MRB_SPECIAL_SHIFT 8

#if defined(MRB_64BIT)
#define MRB_SYMBOL_BITSIZE  (sizeof(_sym) * CHAR_BIT)
#define MRB_SYMBOL_MAX      UINT32_MAX
#else
#define MRB_SYMBOL_BITSIZE  (sizeof(_sym) * CHAR_BIT - MRB_SPECIAL_SHIFT)
#define MRB_SYMBOL_MAX      (UINT32_MAX >> MRB_SPECIAL_SHIFT)
#endif

typedef union value {
  union {
    void *p;
    struct {
      unsigned int i_flag : MRB_FIXNUM_SHIFT;
      _int i : (MRB_INT_BIT - MRB_FIXNUM_SHIFT);
    };
    struct {
      unsigned int sym_flag : MRB_SPECIAL_SHIFT;
      _sym sym : MRB_SYMBOL_BITSIZE;
    };
    struct RBasic *bp;
#ifndef MRB_WITHOUT_FLOAT
    struct RFloat *fp;
#endif
    struct RCptr *vp;
  } value;
  unsigned long w;
} value;

MRB_API value _word_boxing_cptr_value(struct state*, void*);
#ifndef MRB_WITHOUT_FLOAT
MRB_API value _word_boxing_float_value(struct state*, _float);
MRB_API value _word_boxing_float_pool(struct state*, _float);
#endif

#ifndef MRB_WITHOUT_FLOAT
#define _float_pool(mrb,f) _word_boxing_float_pool(mrb,f)
#endif

#define _ptr(o)     (o).value.p
#define _cptr(o)    (o).value.vp->p
#ifndef MRB_WITHOUT_FLOAT
#define _float(o)   (o).value.fp->f
#endif
#define _fixnum(o)  ((_int)(o).value.i)
#define _symbol(o)  (o).value.sym

static inline enum _vtype
_type(value o)
{
  switch (o.w) {
  case MRB_Qfalse:
  case MRB_Qnil:
    return MRB_TT_FALSE;
  case MRB_Qtrue:
    return MRB_TT_TRUE;
  case MRB_Qundef:
    return MRB_TT_UNDEF;
  }
  if (o.value.i_flag == MRB_FIXNUM_FLAG) {
    return MRB_TT_FIXNUM;
  }
  if (o.value.sym_flag == MRB_SYMBOL_FLAG) {
    return MRB_TT_SYMBOL;
  }
  return o.value.bp->tt;
}

#define _bool(o)    ((o).w != MRB_Qnil && (o).w != MRB_Qfalse)
#define _fixnum_p(o) ((o).value.i_flag == MRB_FIXNUM_FLAG)
#define _undef_p(o) ((o).w == MRB_Qundef)
#define _nil_p(o)  ((o).w == MRB_Qnil)

#define BOXWORD_SET_VALUE(o, ttt, attr, v) do { \
  switch (ttt) {\
  case MRB_TT_FALSE:  (o).w = (v) ? MRB_Qfalse : MRB_Qnil; break;\
  case MRB_TT_TRUE:   (o).w = MRB_Qtrue; break;\
  case MRB_TT_UNDEF:  (o).w = MRB_Qundef; break;\
  case MRB_TT_FIXNUM: (o).w = 0;(o).value.i_flag = MRB_FIXNUM_FLAG; (o).attr = (v); break;\
  case MRB_TT_SYMBOL: (o).w = 0;(o).value.sym_flag = MRB_SYMBOL_FLAG; (o).attr = (v); break;\
  default:            (o).w = 0; (o).attr = (v); if ((o).value.bp) (o).value.bp->tt = ttt; break;\
  }\
} while (0)

#ifndef MRB_WITHOUT_FLOAT
#define SET_FLOAT_VALUE(mrb,r,v) r = _word_boxing_float_value(mrb, v)
#endif
#define SET_CPTR_VALUE(mrb,r,v) r = _word_boxing_cptr_value(mrb, v)
#define SET_NIL_VALUE(r) BOXWORD_SET_VALUE(r, MRB_TT_FALSE, value.i, 0)
#define SET_FALSE_VALUE(r) BOXWORD_SET_VALUE(r, MRB_TT_FALSE, value.i, 1)
#define SET_TRUE_VALUE(r) BOXWORD_SET_VALUE(r, MRB_TT_TRUE, value.i, 1)
#define SET_BOOL_VALUE(r,b) BOXWORD_SET_VALUE(r, b ? MRB_TT_TRUE : MRB_TT_FALSE, value.i, 1)
#define SET_INT_VALUE(r,n) BOXWORD_SET_VALUE(r, MRB_TT_FIXNUM, value.i, (n))
#define SET_SYM_VALUE(r,v) BOXWORD_SET_VALUE(r, MRB_TT_SYMBOL, value.sym, (v))
#define SET_OBJ_VALUE(r,v) BOXWORD_SET_VALUE(r, (((struct RObject*)(v))->tt), value.p, (v))
#define SET_UNDEF_VALUE(r) BOXWORD_SET_VALUE(r, MRB_TT_UNDEF, value.i, 0)

#endif  /* MRUBY_BOXING_WORD_H */
