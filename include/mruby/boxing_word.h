/*
** mruby/boxing_word.h - word boxing $value definition
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_BOXING_WORD_H
#define MRUBY_BOXING_WORD_H

#if defined($INT16)
# error $INT16 is too small for $WORD_BOXING.
#endif

#if defined($INT64) && !defined($64BIT)
#error $INT64 cannot be used with $WORD_BOXING in 32-bit mode.
#endif

#ifndef $WITHOUT_FLOAT
struct RFloat {
  $OBJECT_HEADER;
  $float f;
};
#endif

struct RCptr {
  $OBJECT_HEADER;
  void *p;
};

#define $FIXNUM_SHIFT 1
#ifdef $WITHOUT_FLOAT
#define $TT_HAS_BASIC $TT_CPTR
#else
#define $TT_HAS_BASIC $TT_FLOAT
#endif

enum $special_consts {
  $Qnil    = 0,
  $Qfalse  = 2,
  $Qtrue   = 4,
  $Qundef  = 6,
};

#define $FIXNUM_FLAG   0x01
#define $SYMBOL_FLAG   0x0e
#define $SPECIAL_SHIFT 8

#if defined($64BIT)
#define $SYMBOL_BITSIZE  (sizeof($sym) * CHAR_BIT)
#define $SYMBOL_MAX      UINT32_MAX
#else
#define $SYMBOL_BITSIZE  (sizeof($sym) * CHAR_BIT - $SPECIAL_SHIFT)
#define $SYMBOL_MAX      (UINT32_MAX >> $SPECIAL_SHIFT)
#endif

typedef union $value {
  union {
    void *p;
    struct {
      unsigned int i_flag : $FIXNUM_SHIFT;
      $int i : ($INT_BIT - $FIXNUM_SHIFT);
    };
    struct {
      unsigned int sym_flag : $SPECIAL_SHIFT;
      $sym sym : $SYMBOL_BITSIZE;
    };
    struct RBasic *bp;
#ifndef $WITHOUT_FLOAT
    struct RFloat *fp;
#endif
    struct RCptr *vp;
  } value;
  unsigned long w;
} $value;

$API $value $word_boxing_cptr_value(struct $state*, void*);
#ifndef $WITHOUT_FLOAT
$API $value $word_boxing_float_value(struct $state*, $float);
$API $value $word_boxing_float_pool(struct $state*, $float);
#endif

#ifndef $WITHOUT_FLOAT
#define $float_pool(mrb,f) $word_boxing_float_pool(mrb,f)
#endif

#define $ptr(o)     (o).value.p
#define $cptr(o)    (o).value.vp->p
#ifndef $WITHOUT_FLOAT
#define $float(o)   (o).value.fp->f
#endif
#define $fixnum(o)  (($int)(o).value.i)
#define $symbol(o)  (o).value.sym

static inline enum $vtype
$type($value o)
{
  switch (o.w) {
  case $Qfalse:
  case $Qnil:
    return $TT_FALSE;
  case $Qtrue:
    return $TT_TRUE;
  case $Qundef:
    return $TT_UNDEF;
  }
  if (o.value.i_flag == $FIXNUM_FLAG) {
    return $TT_FIXNUM;
  }
  if (o.value.sym_flag == $SYMBOL_FLAG) {
    return $TT_SYMBOL;
  }
  return o.value.bp->tt;
}

#define $bool(o)    ((o).w != $Qnil && (o).w != $Qfalse)
#define $fixnum_p(o) ((o).value.i_flag == $FIXNUM_FLAG)
#define $undef_p(o) ((o).w == $Qundef)
#define $nil_p(o)  ((o).w == $Qnil)

#define BOXWORD_SET_VALUE(o, ttt, attr, v) do { \
  switch (ttt) {\
  case $TT_FALSE:  (o).w = (v) ? $Qfalse : $Qnil; break;\
  case $TT_TRUE:   (o).w = $Qtrue; break;\
  case $TT_UNDEF:  (o).w = $Qundef; break;\
  case $TT_FIXNUM: (o).w = 0;(o).value.i_flag = $FIXNUM_FLAG; (o).attr = (v); break;\
  case $TT_SYMBOL: (o).w = 0;(o).value.sym_flag = $SYMBOL_FLAG; (o).attr = (v); break;\
  default:            (o).w = 0; (o).attr = (v); if ((o).value.bp) (o).value.bp->tt = ttt; break;\
  }\
} while (0)

#ifndef $WITHOUT_FLOAT
#define SET_FLOAT_VALUE(mrb,r,v) r = $word_boxing_float_value(mrb, v)
#endif
#define SET_CPTR_VALUE(mrb,r,v) r = $word_boxing_cptr_value(mrb, v)
#define SET_NIL_VALUE(r) BOXWORD_SET_VALUE(r, $TT_FALSE, value.i, 0)
#define SET_FALSE_VALUE(r) BOXWORD_SET_VALUE(r, $TT_FALSE, value.i, 1)
#define SET_TRUE_VALUE(r) BOXWORD_SET_VALUE(r, $TT_TRUE, value.i, 1)
#define SET_BOOL_VALUE(r,b) BOXWORD_SET_VALUE(r, b ? $TT_TRUE : $TT_FALSE, value.i, 1)
#define SET_INT_VALUE(r,n) BOXWORD_SET_VALUE(r, $TT_FIXNUM, value.i, (n))
#define SET_SYM_VALUE(r,v) BOXWORD_SET_VALUE(r, $TT_SYMBOL, value.sym, (v))
#define SET_OBJ_VALUE(r,v) BOXWORD_SET_VALUE(r, (((struct RObject*)(v))->tt), value.p, (v))
#define SET_UNDEF_VALUE(r) BOXWORD_SET_VALUE(r, $TT_UNDEF, value.i, 0)

#endif  /* MRUBY_BOXING_WORD_H */
