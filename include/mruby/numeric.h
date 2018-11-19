/*
** mruby/numeric.h - Numeric, Integer, Float, Fixnum class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_NUMERIC_H
#define MRUBY_NUMERIC_H

#include "common.h"

/**
 * Numeric class and it's sub-classes.
 *
 * Integer, Float and Fixnum
 */
MRB_BEGIN_DECL

#define TYPED_POSFIXABLE(f,t) ((f) <= (t)MRB_INT_MAX)
#define TYPED_NEGFIXABLE(f,t) ((f) >= (t)MRB_INT_MIN)
#define TYPED_FIXABLE(f,t) (TYPED_POSFIXABLE(f,t) && TYPED_NEGFIXABLE(f,t))
#define POSFIXABLE(f) TYPED_POSFIXABLE(f,_int)
#define NEGFIXABLE(f) TYPED_NEGFIXABLE(f,_int)
#define FIXABLE(f) TYPED_FIXABLE(f,_int)
#ifndef MRB_WITHOUT_FLOAT
#define FIXABLE_FLOAT(f) TYPED_FIXABLE(f,double)
#endif

#ifndef MRB_WITHOUT_FLOAT
MRB_API _value _flo_to_fixnum(_state *mrb, _value val);
#endif
MRB_API _value _fixnum_to_str(_state *mrb, _value x, _int base);
/* ArgumentError if format string doesn't match /%(\.[0-9]+)?[aAeEfFgG]/ */
#ifndef MRB_WITHOUT_FLOAT
MRB_API _value _float_to_str(_state *mrb, _value x, const char *fmt);
MRB_API _float _to_flo(_state *mrb, _value x);
#endif

_value _fixnum_plus(_state *mrb, _value x, _value y);
_value _fixnum_minus(_state *mrb, _value x, _value y);
_value _fixnum_mul(_state *mrb, _value x, _value y);
_value _num_div(_state *mrb, _value x, _value y);

#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif

#if (defined(__GNUC__) && __GNUC__ >= 5) ||   \
    (__has_builtin(__builtin_add_overflow) && \
     __has_builtin(__builtin_sub_overflow) && \
     __has_builtin(__builtin_mul_overflow))
# define MRB_HAVE_TYPE_GENERIC_CHECKED_ARITHMETIC_BUILTINS
#endif

/*
// Clang 3.8 and 3.9 have problem compiling mruby in 32-bit mode, when MRB_INT64 is set
// because of missing __mulodi4 and similar functions in its runtime. We need to use custom
// implementation for them.
*/
#ifdef MRB_HAVE_TYPE_GENERIC_CHECKED_ARITHMETIC_BUILTINS
#if defined(__clang__) && (__clang_major__ == 3) && (__clang_minor__ >= 8) && \
    defined(MRB_32BIT) && defined(MRB_INT64)
#undef MRB_HAVE_TYPE_GENERIC_CHECKED_ARITHMETIC_BUILTINS
#endif
#endif

#ifdef MRB_HAVE_TYPE_GENERIC_CHECKED_ARITHMETIC_BUILTINS

#ifndef MRB_WORD_BOXING
# define WBCHK(x) 0
#else
# define WBCHK(x) !FIXABLE(x)
#endif

static inline _bool
_int_add_overflow(_int augend, _int addend, _int *sum)
{
  return __builtin_add_overflow(augend, addend, sum) || WBCHK(*sum);
}

static inline _bool
_int_sub_overflow(_int minuend, _int subtrahend, _int *difference)
{
  return __builtin_sub_overflow(minuend, subtrahend, difference) || WBCHK(*difference);
}

static inline _bool
_int_mul_overflow(_int multiplier, _int multiplicand, _int *product)
{
  return __builtin_mul_overflow(multiplier, multiplicand, product) || WBCHK(*product);
}

#undef WBCHK

#else

#define MRB_UINT_MAKE2(n) uint ## n ## _t
#define MRB_UINT_MAKE(n) MRB_UINT_MAKE2(n)
#define _uint MRB_UINT_MAKE(MRB_INT_BIT)

#define MRB_INT_OVERFLOW_MASK ((_uint)1 << (MRB_INT_BIT - 1 - MRB_FIXNUM_SHIFT))

static inline _bool
_int_add_overflow(_int augend, _int addend, _int *sum)
{
  _uint x = (_uint)augend;
  _uint y = (_uint)addend;
  _uint z = (_uint)(x + y);
  *sum = (_int)z;
  return !!(((x ^ z) & (y ^ z)) & MRB_INT_OVERFLOW_MASK);
}

static inline _bool
_int_sub_overflow(_int minuend, _int subtrahend, _int *difference)
{
  _uint x = (_uint)minuend;
  _uint y = (_uint)subtrahend;
  _uint z = (_uint)(x - y);
  *difference = (_int)z;
  return !!(((x ^ z) & (~y ^ z)) & MRB_INT_OVERFLOW_MASK);
}

static inline _bool
_int_mul_overflow(_int multiplier, _int multiplicand, _int *product)
{
#if MRB_INT_BIT == 32
  int64_t n = (int64_t)multiplier * multiplicand;
  *product = (_int)n;
  return !FIXABLE(n);
#else
  if (multiplier > 0) {
    if (multiplicand > 0) {
      if (multiplier > MRB_INT_MAX / multiplicand) return TRUE;
    }
    else {
      if (multiplicand < MRB_INT_MAX / multiplier) return TRUE;
    }
  }
  else {
    if (multiplicand > 0) {
      if (multiplier < MRB_INT_MAX / multiplicand) return TRUE;
    }
    else {
      if (multiplier != 0 && multiplicand < MRB_INT_MAX / multiplier) return TRUE;
    }
  }
  *product = multiplier * multiplicand;
  return FALSE;
#endif
}

#undef MRB_INT_OVERFLOW_MASK
#undef _uint
#undef MRB_UINT_MAKE
#undef MRB_UINT_MAKE2

#endif

MRB_END_DECL

#endif  /* MRUBY_NUMERIC_H */
