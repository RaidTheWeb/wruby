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
BEGIN_DECL

#define TYPED_POSFIXABLE(f,t) ((f) <= (t)INT_MAX)
#define TYPED_NEGFIXABLE(f,t) ((f) >= (t)INT_MIN)
#define TYPED_FIXABLE(f,t) (TYPED_POSFIXABLE(f,t) && TYPED_NEGFIXABLE(f,t))
#define POSFIXABLE(f) TYPED_POSFIXABLE(f,int)
#define NEGFIXABLE(f) TYPED_NEGFIXABLE(f,int)
#define FIXABLE(f) TYPED_FIXABLE(f,int)
#ifndef WITHOUT_FLOAT
#define FIXABLE_FLOAT(f) TYPED_FIXABLE(f,double)
#endif

#ifndef WITHOUT_FLOAT
API value flo_to_fixnum(state *mrb, value val);
#endif
API value fixnum_to_str(state *mrb, value x, int base);
/* ArgumentError if format string doesn't match /%(\.[0-9]+)?[aAeEfFgG]/ */
#ifndef WITHOUT_FLOAT
API value float_to_str(state *mrb, value x, const char *fmt);
API float to_flo(state *mrb, value x);
#endif

value fixnum_plus(state *mrb, value x, value y);
value fixnum_minus(state *mrb, value x, value y);
value fixnum_mul(state *mrb, value x, value y);
value num_div(state *mrb, value x, value y);

#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif

#if (defined(__GNUC__) && __GNUC__ >= 5) ||   \
    (__has_builtin(__builtin_add_overflow) && \
     __has_builtin(__builtin_sub_overflow) && \
     __has_builtin(__builtin_mul_overflow))
# define HAVE_TYPE_GENERIC_CHECKED_ARITHMETIC_BUILTINS
#endif

/*
// Clang 3.8 and 3.9 have problem compiling mruby in 32-bit mode, when INT64 is set
// because of missing __mulodi4 and similar functions in its runtime. We need to use custom
// implementation for them.
*/
#ifdef HAVE_TYPE_GENERIC_CHECKED_ARITHMETIC_BUILTINS
#if defined(__clang__) && (__clang_major__ == 3) && (__clang_minor__ >= 8) && \
    defined(RB32BIT) && defined(INT64)
#undef HAVE_TYPE_GENERIC_CHECKED_ARITHMETIC_BUILTINS
#endif
#endif

#ifdef HAVE_TYPE_GENERIC_CHECKED_ARITHMETIC_BUILTINS

#ifndef WORD_BOXING
# define WBCHK(x) 0
#else
# define WBCHK(x) !FIXABLE(x)
#endif

static inline bool
int_add_overflow(int augend, int addend, int *sum)
{
  return __builtin_add_overflow(augend, addend, sum) || WBCHK(*sum);
}

static inline bool
int_sub_overflow(int minuend, int subtrahend, int *difference)
{
  return __builtin_sub_overflow(minuend, subtrahend, difference) || WBCHK(*difference);
}

static inline bool
int_mul_overflow(int multiplier, int multiplicand, int *product)
{
  return __builtin_mul_overflow(multiplier, multiplicand, product) || WBCHK(*product);
}

#undef WBCHK

#else

#define UINT_MAKE2(n) uint ## n ## _t
#define UINT_MAKE(n) UINT_MAKE2(n)
#define uint UINT_MAKE(INT_BIT)

#define INT_OVERFLOW_MASK ((uint)1 << (INT_BIT - 1 - FIXNUM_SHIFT))

static inline bool
int_add_overflow(int augend, int addend, int *sum)
{
  uint x = (uint)augend;
  uint y = (uint)addend;
  uint z = (uint)(x + y);
  *sum = (int)z;
  return !!(((x ^ z) & (y ^ z)) & INT_OVERFLOW_MASK);
}

static inline bool
int_sub_overflow(int minuend, int subtrahend, int *difference)
{
  uint x = (uint)minuend;
  uint y = (uint)subtrahend;
  uint z = (uint)(x - y);
  *difference = (int)z;
  return !!(((x ^ z) & (~y ^ z)) & INT_OVERFLOW_MASK);
}

static inline bool
int_mul_overflow(int multiplier, int multiplicand, int *product)
{
#if INT_BIT == 32
  int64_t n = (int64_t)multiplier * multiplicand;
  *product = (int)n;
  return !FIXABLE(n);
#else
  if (multiplier > 0) {
    if (multiplicand > 0) {
      if (multiplier > INT_MAX / multiplicand) return TRUE;
    }
    else {
      if (multiplicand < INT_MAX / multiplier) return TRUE;
    }
  }
  else {
    if (multiplicand > 0) {
      if (multiplier < INT_MAX / multiplicand) return TRUE;
    }
    else {
      if (multiplier != 0 && multiplicand < INT_MAX / multiplier) return TRUE;
    }
  }
  *product = multiplier * multiplicand;
  return FALSE;
#endif
}

#undef INT_OVERFLOW_MASK
#undef uint
#undef UINT_MAKE
#undef UINT_MAKE2

#endif

END_DECL

#endif  /* MRUBY_NUMERIC_H */
