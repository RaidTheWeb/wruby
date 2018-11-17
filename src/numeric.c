/*
** numeric.c - Numeric, Integer, Float, Fixnum class
**
** See Copyright Notice in mruby.h
*/

#ifndef WITHOUT_FLOAT
#include <float.h>
#include <math.h>
#endif
#include <limits.h>
#include <stdlib.h>

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/numeric.h>
#include <mruby/string.h>
#include <mruby/class.h>

#ifndef WITHOUT_FLOAT
#ifdef USE_FLOAT
#define trunc(f) truncf(f)
#define floor(f) floorf(f)
#define ceil(f) ceilf(f)
#define fmod(x,y) fmodf(x,y)
#define FLO_TO_STR_FMT "%.8g"
#else
#define FLO_TO_STR_FMT "%.16g"
#endif
#endif

#ifndef WITHOUT_FLOAT
API float
to_flo(state *mrb, value val)
{
  switch (type(val)) {
  case TT_FIXNUM:
    return (float)fixnum(val);
  case TT_FLOAT:
    break;
  default:
    raise(mrb, E_TYPE_ERROR, "non float value");
  }
  return float(val);
}
#endif

/*
 * call-seq:
 *
 *  num ** other  ->  num
 *
 * Raises <code>num</code> the <code>other</code> power.
 *
 *    2.0**3      #=> 8.0
 */
static value
num_pow(state *mrb, value x)
{
  value y;
#ifndef WITHOUT_FLOAT
  float d;
#endif

  get_args(mrb, "o", &y);
  if (fixnum_p(x) && fixnum_p(y)) {
    /* try ipow() */
    int base = fixnum(x);
    int exp = fixnum(y);
    int result = 1;

    if (exp < 0)
#ifdef WITHOUT_FLOAT
      return fixnum_value(0);
#else
      goto float_pow;
#endif
    for (;;) {
      if (exp & 1) {
        if (int_mul_overflow(result, base, &result)) {
#ifndef WITHOUT_FLOAT
          goto float_pow;
#endif
        }
      }
      exp >>= 1;
      if (exp == 0) break;
      if (int_mul_overflow(base, base, &base)) {
#ifndef WITHOUT_FLOAT
        goto float_pow;
#endif
      }
    }
    return fixnum_value(result);
  }
#ifdef WITHOUT_FLOAT
  raise(mrb, E_TYPE_ERROR, "non fixnum value");
#else
 float_pow:
  d = pow(to_flo(mrb, x), to_flo(mrb, y));
  return float_value(mrb, d);
#endif
}

/* 15.2.8.3.4  */
/* 15.2.9.3.4  */
/*
 * call-seq:
 *   num / other  ->  num
 *
 * Performs division: the class of the resulting object depends on
 * the class of <code>num</code> and on the magnitude of the
 * result.
 */

value
num_div(state *mrb, value x, value y)
{
#ifdef WITHOUT_FLOAT
  if (!fixnum_p(y)) {
    raise(mrb, E_TYPE_ERROR, "non fixnum value");
  }
  return fixnum_value(fixnum(x) / fixnum(y));
#else
  return float_value(mrb, to_flo(mrb, x) / to_flo(mrb, y));
#endif
}

/* 15.2.9.3.19(x) */
/*
 *  call-seq:
 *     num.quo(numeric)  ->  real
 *
 *  Returns most exact division.
 */

static value
num_div(state *mrb, value x)
{
#ifdef WITHOUT_FLOAT
  value y;

  get_args(mrb, "o", &y);
  if (!fixnum_p(y)) {
    raise(mrb, E_TYPE_ERROR, "non fixnum value");
  }
  return fixnum_value(fixnum(x) / fixnum(y));
#else
  float y;

  get_args(mrb, "f", &y);
  return float_value(mrb, to_flo(mrb, x) / y);
#endif
}

#ifndef WITHOUT_FLOAT
/********************************************************************
 *
 * Document-class: Float
 *
 *  <code>Float</code> objects represent inexact real numbers using
 *  the native architecture's double-precision floating point
 *  representation.
 */

/* 15.2.9.3.16(x) */
/*
 *  call-seq:
 *     flt.to_s  ->  string
 *
 *  Returns a string containing a representation of self. As well as a
 *  fixed or exponential form of the number, the call may return
 *  "<code>NaN</code>", "<code>Infinity</code>", and
 *  "<code>-Infinity</code>".
 */

static value
flo_to_s(state *mrb, value flt)
{
  if (isnan(float(flt))) {
    return str_new_lit(mrb, "NaN");
  }
  return float_to_str(mrb, flt, FLO_TO_STR_FMT);
}

/* 15.2.9.3.2  */
/*
 * call-seq:
 *   float - other  ->  float
 *
 * Returns a new float which is the difference of <code>float</code>
 * and <code>other</code>.
 */

static value
flo_minus(state *mrb, value x)
{
  value y;

  get_args(mrb, "o", &y);
  return float_value(mrb, float(x) - to_flo(mrb, y));
}

/* 15.2.9.3.3  */
/*
 * call-seq:
 *   float * other  ->  float
 *
 * Returns a new float which is the product of <code>float</code>
 * and <code>other</code>.
 */

static value
flo_mul(state *mrb, value x)
{
  value y;

  get_args(mrb, "o", &y);
  return float_value(mrb, float(x) * to_flo(mrb, y));
}

static void
flodivmod(state *mrb, double x, double y, float *divp, float *modp)
{
  double div, mod;

  if (isnan(y)) {
    /* y is NaN so all results are NaN */
    div = mod = y;
    goto exit;
  }
  if (y == 0.0) {
    if (x == 0) div = NAN;
    else if (x > 0.0) div = INFINITY;
    else div = -INFINITY;       /* x < 0.0 */
    mod = NAN;
    goto exit;
  }
  if ((x == 0.0) || (isinf(y) && !isinf(x))) {
    mod = x;
  }
  else {
    mod = fmod(x, y);
  }
  if (isinf(x) && !isinf(y)) {
    div = x;
  }
  else {
    div = (x - mod) / y;
    if (modp && divp) div = round(div);
  }
  if (y*mod < 0) {
    mod += y;
    div -= 1.0;
  }
 exit:
  if (modp) *modp = mod;
  if (divp) *divp = div;
}

/* 15.2.9.3.5  */
/*
 *  call-seq:
 *     flt % other        ->  float
 *     flt.modulo(other)  ->  float
 *
 *  Return the modulo after division of <code>flt</code> by <code>other</code>.
 *
 *     6543.21.modulo(137)      #=> 104.21
 *     6543.21.modulo(137.24)   #=> 92.9299999999996
 */

static value
flo_mod(state *mrb, value x)
{
  value y;
  float mod;

  get_args(mrb, "o", &y);

  flodivmod(mrb, float(x), to_flo(mrb, y), 0, &mod);
  return float_value(mrb, mod);
}
#endif

/* 15.2.8.3.16 */
/*
 *  call-seq:
 *     num.eql?(numeric)  ->  true or false
 *
 *  Returns <code>true</code> if <i>num</i> and <i>numeric</i> are the
 *  same type and have equal values.
 *
 *     1 == 1.0          #=> true
 *     1.eql?(1.0)       #=> false
 *     (1.0).eql?(1.0)   #=> true
 */
static value
fix_eql(state *mrb, value x)
{
  value y;

  get_args(mrb, "o", &y);
  if (!fixnum_p(y)) return false_value();
  return bool_value(fixnum(x) == fixnum(y));
}

#ifndef WITHOUT_FLOAT
static value
flo_eql(state *mrb, value x)
{
  value y;

  get_args(mrb, "o", &y);
  if (!float_p(y)) return false_value();
  return bool_value(float(x) == (float)fixnum(y));
}

/* 15.2.9.3.7  */
/*
 *  call-seq:
 *     flt == obj  ->  true or false
 *
 *  Returns <code>true</code> only if <i>obj</i> has the same value
 *  as <i>flt</i>. Contrast this with <code>Float#eql?</code>, which
 *  requires <i>obj</i> to be a <code>Float</code>.
 *
 *     1.0 == 1   #=> true
 *
 */

static value
flo_eq(state *mrb, value x)
{
  value y;
  get_args(mrb, "o", &y);

  switch (type(y)) {
  case TT_FIXNUM:
    return bool_value(float(x) == (float)fixnum(y));
  case TT_FLOAT:
    return bool_value(float(x) == float(y));
  default:
    return false_value();
  }
}

static int64_t
value_int64(state *mrb, value x)
{
  switch (type(x)) {
  case TT_FIXNUM:
    return (int64_t)fixnum(x);
    break;
  case TT_FLOAT:
    return (int64_t)float(x);
  default:
    raise(mrb, E_TYPE_ERROR, "cannot convert to Integer");
    break;
  }
  /* not reached */
  return 0;
}

static value
int64_value(state *mrb, int64_t v)
{
  if (FIXABLE(v)) {
    return fixnum_value((int)v);
  }
  return float_value(mrb, (float)v);
}

static value
flo_rev(state *mrb, value x)
{
  int64_t v1;
  get_args(mrb, "");
  v1 = (int64_t)float(x);
  return int64_value(mrb, ~v1);
}

static value
flo_and(state *mrb, value x)
{
  value y;
  int64_t v1, v2;
  get_args(mrb, "o", &y);

  v1 = (int64_t)float(x);
  v2 = value_int64(mrb, y);
  return int64_value(mrb, v1 & v2);
}

static value
flo_or(state *mrb, value x)
{
  value y;
  int64_t v1, v2;
  get_args(mrb, "o", &y);

  v1 = (int64_t)float(x);
  v2 = value_int64(mrb, y);
  return int64_value(mrb, v1 | v2);
}

static value
flo_xor(state *mrb, value x)
{
  value y;
  int64_t v1, v2;
  get_args(mrb, "o", &y);

  v1 = (int64_t)float(x);
  v2 = value_int64(mrb, y);
  return int64_value(mrb, v1 ^ v2);
}

static value
flo_shift(state *mrb, value x, int width)
{
  float val;

  if (width == 0) {
    return x;
  }
  val = float(x);
  if (width < 0) {
    while (width++) {
      val /= 2;
    }
#if defined(_ISOC99_SOURCE)
    val = trunc(val);
#else
    if (val > 0){
        val = floor(val);
    } else {
        val = ceil(val);
    }
#endif
    if (val == 0 && float(x) < 0) {
      return fixnum_value(-1);
    }
  }
  else {
    while (width--) {
      val *= 2;
    }
  }
  if (FIXABLE_FLOAT(val)) {
    return fixnum_value((int)val);
  }
  return float_value(mrb, val);
}

static value
flo_lshift(state *mrb, value x)
{
  int width;

  get_args(mrb, "i", &width);
  return flo_shift(mrb, x, -width);
}

static value
flo_rshift(state *mrb, value x)
{
  int width;

  get_args(mrb, "i", &width);
  return flo_shift(mrb, x, width);
}

/* 15.2.9.3.13 */
/*
 * call-seq:
 *   flt.to_f  ->  self
 *
 * As <code>flt</code> is already a float, returns +self+.
 */

static value
flo_to_f(state *mrb, value num)
{
  return num;
}

/* 15.2.9.3.11 */
/*
 *  call-seq:
 *     flt.infinite?  ->  nil, -1, +1
 *
 *  Returns <code>nil</code>, -1, or +1 depending on whether <i>flt</i>
 *  is finite, -infinity, or +infinity.
 *
 *     (0.0).infinite?        #=> nil
 *     (-1.0/0.0).infinite?   #=> -1
 *     (+1.0/0.0).infinite?   #=> 1
 */

static value
flo_infinite_p(state *mrb, value num)
{
  float value = float(num);

  if (isinf(value)) {
    return fixnum_value(value < 0 ? -1 : 1);
  }
  return nil_value();
}

/* 15.2.9.3.9  */
/*
 *  call-seq:
 *     flt.finite?  ->  true or false
 *
 *  Returns <code>true</code> if <i>flt</i> is a valid IEEE floating
 *  point number (it is not infinite, and <code>nan?</code> is
 *  <code>false</code>).
 *
 */

static value
flo_finite_p(state *mrb, value num)
{
  return bool_value(isfinite(float(num)));
}

void
check_num_exact(state *mrb, float num)
{
  if (isinf(num)) {
    raise(mrb, E_FLOATDOMAIN_ERROR, num < 0 ? "-Infinity" : "Infinity");
  }
  if (isnan(num)) {
    raise(mrb, E_FLOATDOMAIN_ERROR, "NaN");
  }
}

/* 15.2.9.3.10 */
/*
 *  call-seq:
 *     flt.floor  ->  integer
 *
 *  Returns the largest integer less than or equal to <i>flt</i>.
 *
 *     1.2.floor      #=> 1
 *     2.0.floor      #=> 2
 *     (-1.2).floor   #=> -2
 *     (-2.0).floor   #=> -2
 */

static value
flo_floor(state *mrb, value num)
{
  float f = floor(float(num));

  check_num_exact(mrb, f);
  if (!FIXABLE_FLOAT(f)) {
    return float_value(mrb, f);
  }
  return fixnum_value((int)f);
}

/* 15.2.9.3.8  */
/*
 *  call-seq:
 *     flt.ceil  ->  integer
 *
 *  Returns the smallest <code>Integer</code> greater than or equal to
 *  <i>flt</i>.
 *
 *     1.2.ceil      #=> 2
 *     2.0.ceil      #=> 2
 *     (-1.2).ceil   #=> -1
 *     (-2.0).ceil   #=> -2
 */

static value
flo_ceil(state *mrb, value num)
{
  float f = ceil(float(num));

  check_num_exact(mrb, f);
  if (!FIXABLE_FLOAT(f)) {
    return float_value(mrb, f);
  }
  return fixnum_value((int)f);
}

/* 15.2.9.3.12 */
/*
 *  call-seq:
 *     flt.round([ndigits])  ->  integer or float
 *
 *  Rounds <i>flt</i> to a given precision in decimal digits (default 0 digits).
 *  Precision may be negative.  Returns a floating point number when ndigits
 *  is more than zero.
 *
 *     1.4.round      #=> 1
 *     1.5.round      #=> 2
 *     1.6.round      #=> 2
 *     (-1.5).round   #=> -2
 *
 *     1.234567.round(2)  #=> 1.23
 *     1.234567.round(3)  #=> 1.235
 *     1.234567.round(4)  #=> 1.2346
 *     1.234567.round(5)  #=> 1.23457
 *
 *     34567.89.round(-5) #=> 0
 *     34567.89.round(-4) #=> 30000
 *     34567.89.round(-3) #=> 35000
 *     34567.89.round(-2) #=> 34600
 *     34567.89.round(-1) #=> 34570
 *     34567.89.round(0)  #=> 34568
 *     34567.89.round(1)  #=> 34567.9
 *     34567.89.round(2)  #=> 34567.89
 *     34567.89.round(3)  #=> 34567.89
 *
 */

static value
flo_round(state *mrb, value num)
{
  double number, f;
  int ndigits = 0;
  int i;

  get_args(mrb, "|i", &ndigits);
  number = float(num);

  if (0 < ndigits && (isinf(number) || isnan(number))) {
    return num;
  }
  check_num_exact(mrb, number);

  f = 1.0;
  i = ndigits >= 0 ? ndigits : -ndigits;
  while  (--i >= 0)
    f = f*10.0;

  if (isinf(f)) {
    if (ndigits < 0) number = 0;
  }
  else {
    double d;

    if (ndigits < 0) number /= f;
    else number *= f;

    /* home-made inline implementation of round(3) */
    if (number > 0.0) {
      d = floor(number);
      number = d + (number - d >= 0.5);
    }
    else if (number < 0.0) {
      d = ceil(number);
      number = d - (d - number >= 0.5);
    }

    if (ndigits < 0) number *= f;
    else number /= f;
  }

  if (ndigits > 0) {
    if (!isfinite(number)) return num;
    return float_value(mrb, number);
  }
  return fixnum_value((int)number);
}

/* 15.2.9.3.14 */
/* 15.2.9.3.15 */
/*
 *  call-seq:
 *     flt.to_i      ->  integer
 *     flt.to_int    ->  integer
 *     flt.truncate  ->  integer
 *
 *  Returns <i>flt</i> truncated to an <code>Integer</code>.
 */

static value
flo_truncate(state *mrb, value num)
{
  float f = float(num);

  if (f > 0.0) f = floor(f);
  if (f < 0.0) f = ceil(f);

  check_num_exact(mrb, f);
  if (!FIXABLE_FLOAT(f)) {
    return float_value(mrb, f);
  }
  return fixnum_value((int)f);
}

static value
flo_nan_p(state *mrb, value num)
{
  return bool_value(isnan(float(num)));
}
#endif

/*
 * Document-class: Integer
 *
 *  <code>Integer</code> is the basis for the two concrete classes that
 *  hold whole numbers, <code>Bignum</code> and <code>Fixnum</code>.
 *
 */


/*
 *  call-seq:
 *     int.to_i      ->  integer
 *     int.to_int    ->  integer
 *
 *  As <i>int</i> is already an <code>Integer</code>, all these
 *  methods simply return the receiver.
 */

static value
int_to_i(state *mrb, value num)
{
  return num;
}

value
fixnum_mul(state *mrb, value x, value y)
{
  int a;

  a = fixnum(x);
  if (fixnum_p(y)) {
    int b, c;

    if (a == 0) return x;
    b = fixnum(y);
    if (int_mul_overflow(a, b, &c)) {
#ifndef WITHOUT_FLOAT
      return float_value(mrb, (float)a * (float)b);
#endif
    }
    return fixnum_value(c);
  }
#ifdef WITHOUT_FLOAT
  raise(mrb, E_TYPE_ERROR, "non fixnum value");
#else
  return float_value(mrb, (float)a * to_flo(mrb, y));
#endif
}

/* 15.2.8.3.3  */
/*
 * call-seq:
 *   fix * numeric  ->  numeric_result
 *
 * Performs multiplication: the class of the resulting object depends on
 * the class of <code>numeric</code> and on the magnitude of the
 * result.
 */

static value
fix_mul(state *mrb, value x)
{
  value y;

  get_args(mrb, "o", &y);
  return fixnum_mul(mrb, x, y);
}

static void
fixdivmod(state *mrb, int x, int y, int *divp, int *modp)
{
  int div, mod;

  /* TODO: add assert(y != 0) to make sure */

  if (y < 0) {
    if (x < 0)
      div = -x / -y;
    else
      div = - (x / -y);
  }
  else {
    if (x < 0)
      div = - (-x / y);
    else
      div = x / y;
  }
  mod = x - div*y;
  if ((mod < 0 && y > 0) || (mod > 0 && y < 0)) {
    mod += y;
    div -= 1;
  }
  if (divp) *divp = div;
  if (modp) *modp = mod;
}

/* 15.2.8.3.5  */
/*
 *  call-seq:
 *    fix % other        ->  real
 *    fix.modulo(other)  ->  real
 *
 *  Returns <code>fix</code> modulo <code>other</code>.
 *  See <code>numeric.divmod</code> for more information.
 */

static value
fix_mod(state *mrb, value x)
{
  value y;
  int a;

  get_args(mrb, "o", &y);
  a = fixnum(x);
  if (fixnum_p(y)) {
    int b, mod;

    if ((b=fixnum(y)) == 0) {
#ifdef WITHOUT_FLOAT
      /* ZeroDivisionError */
      return fixnum_value(0);
#else
      return float_value(mrb, NAN);
#endif
    }
    fixdivmod(mrb, a, b, 0, &mod);
    return fixnum_value(mod);
  }
#ifdef WITHOUT_FLOAT
  raise(mrb, E_TYPE_ERROR, "non fixnum value");
#else
  else {
    float mod;

    flodivmod(mrb, (float)a, to_flo(mrb, y), 0, &mod);
    return float_value(mrb, mod);
  }
#endif
}

/*
 *  call-seq:
 *     fix.divmod(numeric)  ->  array
 *
 *  See <code>Numeric#divmod</code>.
 */
static value
fix_divmod(state *mrb, value x)
{
  value y;

  get_args(mrb, "o", &y);

  if (fixnum_p(y)) {
    int div, mod;

    if (fixnum(y) == 0) {
#ifdef WITHOUT_FLOAT
      return assoc_new(mrb, fixnum_value(0), fixnum_value(0));
#else
      return assoc_new(mrb, ((fixnum(x) == 0) ?
                                 float_value(mrb, NAN):
                                 float_value(mrb, INFINITY)),
                           float_value(mrb, NAN));
#endif
    }
    fixdivmod(mrb, fixnum(x), fixnum(y), &div, &mod);
    return assoc_new(mrb, fixnum_value(div), fixnum_value(mod));
  }
#ifdef WITHOUT_FLOAT
  raise(mrb, E_TYPE_ERROR, "non fixnum value");
#else
  else {
    float div, mod;
    value a, b;

    flodivmod(mrb, (float)fixnum(x), to_flo(mrb, y), &div, &mod);
    a = float_value(mrb, div);
    b = float_value(mrb, mod);
    return assoc_new(mrb, a, b);
  }
#endif
}

#ifndef WITHOUT_FLOAT
static value
flo_divmod(state *mrb, value x)
{
  value y;
  float div, mod;
  value a, b;

  get_args(mrb, "o", &y);

  flodivmod(mrb, float(x), to_flo(mrb, y), &div, &mod);
  a = float_value(mrb, div);
  b = float_value(mrb, mod);
  return assoc_new(mrb, a, b);
}
#endif

/* 15.2.8.3.7  */
/*
 * call-seq:
 *   fix == other  ->  true or false
 *
 * Return <code>true</code> if <code>fix</code> equals <code>other</code>
 * numerically.
 *
 *   1 == 2      #=> false
 *   1 == 1.0    #=> true
 */

static value
fix_equal(state *mrb, value x)
{
  value y;

  get_args(mrb, "o", &y);
  switch (type(y)) {
  case TT_FIXNUM:
    return bool_value(fixnum(x) == fixnum(y));
#ifndef WITHOUT_FLOAT
  case TT_FLOAT:
    return bool_value((float)fixnum(x) == float(y));
#endif
  default:
    return false_value();
  }
}

/* 15.2.8.3.8  */
/*
 * call-seq:
 *   ~fix  ->  integer
 *
 * One's complement: returns a number where each bit is flipped.
 *   ex.0---00001 (1)-> 1---11110 (-2)
 *   ex.0---00010 (2)-> 1---11101 (-3)
 *   ex.0---00100 (4)-> 1---11011 (-5)
 */

static value
fix_rev(state *mrb, value num)
{
  int val = fixnum(num);

  return fixnum_value(~val);
}

#ifdef WITHOUT_FLOAT
#define bit_op(x,y,op1,op2) do {\
  return fixnum_value(fixnum(x) op2 fixnum(y));\
} while(0)
#else
static value flo_and(state *mrb, value x);
static value flo_or(state *mrb, value x);
static value flo_xor(state *mrb, value x);
#define bit_op(x,y,op1,op2) do {\
  if (fixnum_p(y)) return fixnum_value(fixnum(x) op2 fixnum(y));\
  return flo_ ## op1(mrb, float_value(mrb, (float)fixnum(x)));\
} while(0)
#endif

/* 15.2.8.3.9  */
/*
 * call-seq:
 *   fix & integer  ->  integer_result
 *
 * Bitwise AND.
 */

static value
fix_and(state *mrb, value x)
{
  value y;

  get_args(mrb, "o", &y);
  bit_op(x, y, and, &);
}

/* 15.2.8.3.10 */
/*
 * call-seq:
 *   fix | integer  ->  integer_result
 *
 * Bitwise OR.
 */

static value
fix_or(state *mrb, value x)
{
  value y;

  get_args(mrb, "o", &y);
  bit_op(x, y, or, |);
}

/* 15.2.8.3.11 */
/*
 * call-seq:
 *   fix ^ integer  ->  integer_result
 *
 * Bitwise EXCLUSIVE OR.
 */

static value
fix_xor(state *mrb, value x)
{
  value y;

  get_args(mrb, "o", &y);
  bit_op(x, y, or, ^);
}

#define NUMERIC_SHIFT_WIDTH_MAX (INT_BIT-1)

static value
lshift(state *mrb, int val, int width)
{
  if (width < 0) {              /* int overflow */
#ifdef WITHOUT_FLOAT
    return fixnum_value(0);
#else
    return float_value(mrb, INFINITY);
#endif
  }
  if (val > 0) {
    if ((width > NUMERIC_SHIFT_WIDTH_MAX) ||
        (val   > (INT_MAX >> width))) {
#ifdef WITHOUT_FLOAT
      return fixnum_value(-1);
#else
      goto bit_overflow;
#endif
    }
    return fixnum_value(val << width);
  }
  else {
    if ((width > NUMERIC_SHIFT_WIDTH_MAX) ||
        (val   < (INT_MIN >> width))) {
#ifdef WITHOUT_FLOAT
      return fixnum_value(0);
#else
      goto bit_overflow;
#endif
    }
    return fixnum_value(val * ((int)1 << width));
  }

#ifndef WITHOUT_FLOAT
bit_overflow:
  {
    float f = (float)val;
    while (width--) {
      f *= 2;
    }
    return float_value(mrb, f);
  }
#endif
}

static value
rshift(int val, int width)
{
  if (width < 0) {              /* int overflow */
    return fixnum_value(0);
  }
  if (width >= NUMERIC_SHIFT_WIDTH_MAX) {
    if (val < 0) {
      return fixnum_value(-1);
    }
    return fixnum_value(0);
  }
  return fixnum_value(val >> width);
}

/* 15.2.8.3.12 */
/*
 * call-seq:
 *   fix << count  ->  integer or float
 *
 * Shifts _fix_ left _count_ positions (right if _count_ is negative).
 */

static value
fix_lshift(state *mrb, value x)
{
  int width, val;

  get_args(mrb, "i", &width);
  if (width == 0) {
    return x;
  }
  val = fixnum(x);
  if (val == 0) return x;
  if (width < 0) {
    return rshift(val, -width);
  }
  return lshift(mrb, val, width);
}

/* 15.2.8.3.13 */
/*
 * call-seq:
 *   fix >> count  ->  integer or float
 *
 * Shifts _fix_ right _count_ positions (left if _count_ is negative).
 */

static value
fix_rshift(state *mrb, value x)
{
  int width, val;

  get_args(mrb, "i", &width);
  if (width == 0) {
    return x;
  }
  val = fixnum(x);
  if (val == 0) return x;
  if (width < 0) {
    return lshift(mrb, val, -width);
  }
  return rshift(val, width);
}

/* 15.2.8.3.23 */
/*
 *  call-seq:
 *     fix.to_f  ->  float
 *
 *  Converts <i>fix</i> to a <code>Float</code>.
 *
 */

#ifndef WITHOUT_FLOAT
static value
fix_to_f(state *mrb, value num)
{
  return float_value(mrb, (float)fixnum(num));
}

/*
 *  Document-class: FloatDomainError
 *
 *  Raised when attempting to convert special float values
 *  (in particular infinite or NaN)
 *  to numerical classes which don't support them.
 *
 *     Float::INFINITY.to_r
 *
 *  <em>raises the exception:</em>
 *
 *     FloatDomainError: Infinity
 */
/* ------------------------------------------------------------------------*/
API value
flo_to_fixnum(state *mrb, value x)
{
  int z = 0;

  if (!float_p(x)) {
    raise(mrb, E_TYPE_ERROR, "non float value");
    z = 0; /* not reached. just suppress warnings. */
  }
  else {
    float d = float(x);

    if (isinf(d)) {
      raise(mrb, E_FLOATDOMAIN_ERROR, d < 0 ? "-Infinity" : "Infinity");
    }
    if (isnan(d)) {
      raise(mrb, E_FLOATDOMAIN_ERROR, "NaN");
    }
    if (FIXABLE_FLOAT(d)) {
      z = (int)d;
    }
    else {
      raisef(mrb, E_ARGUMENT_ERROR, "number (%S) too big for integer", x);
    }
  }
  return fixnum_value(z);
}
#endif

value
fixnum_plus(state *mrb, value x, value y)
{
  int a;

  a = fixnum(x);
  if (fixnum_p(y)) {
    int b, c;

    if (a == 0) return y;
    b = fixnum(y);
    if (int_add_overflow(a, b, &c)) {
#ifndef WITHOUT_FLOAT
      return float_value(mrb, (float)a + (float)b);
#endif
    }
    return fixnum_value(c);
  }
#ifdef WITHOUT_FLOAT
  raise(mrb, E_TYPE_ERROR, "non fixnum value");
#else
  return float_value(mrb, (float)a + to_flo(mrb, y));
#endif
}

/* 15.2.8.3.1  */
/*
 * call-seq:
 *   fix + numeric  ->  numeric_result
 *
 * Performs addition: the class of the resulting object depends on
 * the class of <code>numeric</code> and on the magnitude of the
 * result.
 */
static value
fix_plus(state *mrb, value self)
{
  value other;

  get_args(mrb, "o", &other);
  return fixnum_plus(mrb, self, other);
}

value
fixnum_minus(state *mrb, value x, value y)
{
  int a;

  a = fixnum(x);
  if (fixnum_p(y)) {
    int b, c;

    b = fixnum(y);
    if (int_sub_overflow(a, b, &c)) {
#ifndef WITHOUT_FLOAT
      return float_value(mrb, (float)a - (float)b);
#endif
    }
    return fixnum_value(c);
  }
#ifdef WITHOUT_FLOAT
  raise(mrb, E_TYPE_ERROR, "non fixnum value");
#else
  return float_value(mrb, (float)a - to_flo(mrb, y));
#endif
}

/* 15.2.8.3.2  */
/* 15.2.8.3.16 */
/*
 * call-seq:
 *   fix - numeric  ->  numeric_result
 *
 * Performs subtraction: the class of the resulting object depends on
 * the class of <code>numeric</code> and on the magnitude of the
 * result.
 */
static value
fix_minus(state *mrb, value self)
{
  value other;

  get_args(mrb, "o", &other);
  return fixnum_minus(mrb, self, other);
}


API value
fixnum_to_str(state *mrb, value x, int base)
{
  char buf[INT_BIT+1];
  char *b = buf + sizeof buf;
  int val = fixnum(x);

  if (base < 2 || 36 < base) {
    raisef(mrb, E_ARGUMENT_ERROR, "invalid radix %S", fixnum_value(base));
  }

  if (val == 0) {
    *--b = '0';
  }
  else if (val < 0) {
    do {
      *--b = digitmap[-(val % base)];
    } while (val /= base);
    *--b = '-';
  }
  else {
    do {
      *--b = digitmap[(int)(val % base)];
    } while (val /= base);
  }

  return str_new(mrb, b, buf + sizeof(buf) - b);
}

/* 15.2.8.3.25 */
/*
 *  call-seq:
 *     fix.to_s(base=10)  ->  string
 *
 *  Returns a string containing the representation of <i>fix</i> radix
 *  <i>base</i> (between 2 and 36).
 *
 *     12345.to_s       #=> "12345"
 *     12345.to_s(2)    #=> "11000000111001"
 *     12345.to_s(8)    #=> "30071"
 *     12345.to_s(10)   #=> "12345"
 *     12345.to_s(16)   #=> "3039"
 *     12345.to_s(36)   #=> "9ix"
 *
 */
static value
fix_to_s(state *mrb, value self)
{
  int base = 10;

  get_args(mrb, "|i", &base);
  return fixnum_to_str(mrb, self, base);
}

/* compare two numbers: (1:0:-1; -2 for error) */
static int
cmpnum(state *mrb, value v1, value v2)
{
#ifdef WITHOUT_FLOAT
  int x, y;
#else
  float x, y;
#endif

#ifdef WITHOUT_FLOAT
  x = fixnum(v1);
#else
  x = to_flo(mrb, v1);
#endif
  switch (type(v2)) {
  case TT_FIXNUM:
#ifdef WITHOUT_FLOAT
    y = fixnum(v2);
#else
    y = (float)fixnum(v2);
#endif
    break;
#ifndef WITHOUT_FLOAT
  case TT_FLOAT:
    y = float(v2);
    break;
#endif
  default:
    return -2;
  }
  if (x > y)
    return 1;
  else {
    if (x < y)
      return -1;
    return 0;
  }
}

/* 15.2.9.3.6  */
/*
 * call-seq:
 *     self.f <=> other.f    => -1, 0, +1
 *             <  => -1
 *             =  =>  0
 *             >  => +1
 *  Comparison---Returns -1, 0, or +1 depending on whether <i>fix</i> is
 *  less than, equal to, or greater than <i>numeric</i>. This is the
 *  basis for the tests in <code>Comparable</code>.
 */
static value
num_cmp(state *mrb, value self)
{
  value other;
  int n;

  get_args(mrb, "o", &other);
  n = cmpnum(mrb, self, other);
  if (n == -2) return nil_value();
  return fixnum_value(n);
}

static void
cmperr(state *mrb, value v1, value v2)
{
  raisef(mrb, E_ARGUMENT_ERROR, "comparison of %S with %S failed",
             obj_value(class(mrb, v1)),
             obj_value(class(mrb, v2)));
}

static value
num_lt(state *mrb, value self)
{
  value other;
  int n;

  get_args(mrb, "o", &other);
  n = cmpnum(mrb, self, other);
  if (n == -2) cmperr(mrb, self, other);
  if (n < 0) return true_value();
  return false_value();
}

static value
num_le(state *mrb, value self)
{
  value other;
  int n;

  get_args(mrb, "o", &other);
  n = cmpnum(mrb, self, other);
  if (n == -2) cmperr(mrb, self, other);
  if (n <= 0) return true_value();
  return false_value();
}

static value
num_gt(state *mrb, value self)
{
  value other;
  int n;

  get_args(mrb, "o", &other);
  n = cmpnum(mrb, self, other);
  if (n == -2) cmperr(mrb, self, other);
  if (n > 0) return true_value();
  return false_value();
}

static value
num_ge(state *mrb, value self)
{
  value other;
  int n;

  get_args(mrb, "o", &other);
  n = cmpnum(mrb, self, other);
  if (n == -2) cmperr(mrb, self, other);
  if (n >= 0) return true_value();
  return false_value();
}

static value
num_finite_p(state *mrb, value self)
{
  get_args(mrb, "");
  return true_value();
}

static value
num_infinite_p(state *mrb, value self)
{
  get_args(mrb, "");
  return false_value();
}

/* 15.2.9.3.1  */
/*
 * call-seq:
 *   float + other  ->  float
 *
 * Returns a new float which is the sum of <code>float</code>
 * and <code>other</code>.
 */
#ifndef WITHOUT_FLOAT
static value
flo_plus(state *mrb, value x)
{
  value y;

  get_args(mrb, "o", &y);
  return float_value(mrb, float(x) + to_flo(mrb, y));
}
#endif

/* ------------------------------------------------------------------------*/
void
init_numeric(state *mrb)
{
  struct RClass *numeric, *integer, *fixnum;
#ifndef WITHOUT_FLOAT
  struct RClass *fl;
#endif

  /* Numeric Class */
  numeric = define_class(mrb, "Numeric",  mrb->object_class);                /* 15.2.7 */

  define_method(mrb, numeric, "**",       num_pow,         ARGS_REQ(1));
  define_method(mrb, numeric, "/",        num_div,         ARGS_REQ(1)); /* 15.2.8.3.4  */
  define_method(mrb, numeric, "quo",      num_div,         ARGS_REQ(1)); /* 15.2.7.4.5 (x) */
  define_method(mrb, numeric, "<=>",      num_cmp,         ARGS_REQ(1)); /* 15.2.9.3.6  */
  define_method(mrb, numeric, "<",        num_lt,          ARGS_REQ(1));
  define_method(mrb, numeric, "<=",       num_le,          ARGS_REQ(1));
  define_method(mrb, numeric, ">",        num_gt,          ARGS_REQ(1));
  define_method(mrb, numeric, ">=",       num_ge,          ARGS_REQ(1));
  define_method(mrb, numeric, "finite?",  num_finite_p,    ARGS_NONE());
  define_method(mrb, numeric, "infinite?",num_infinite_p,  ARGS_NONE());

  /* Integer Class */
  integer = define_class(mrb, "Integer",  numeric);                          /* 15.2.8 */
  SET_INSTANCE_TT(integer, TT_FIXNUM);
  undef_class_method(mrb, integer, "new");
  define_method(mrb, integer, "to_i",     int_to_i,        ARGS_NONE()); /* 15.2.8.3.24 */
  define_method(mrb, integer, "to_int",   int_to_i,        ARGS_NONE());
#ifndef WITHOUT_FLOAT
  define_method(mrb, integer, "ceil",     int_to_i,        ARGS_REQ(1)); /* 15.2.8.3.8 (x) */
  define_method(mrb, integer, "floor",    int_to_i,        ARGS_REQ(1)); /* 15.2.8.3.10 (x) */
  define_method(mrb, integer, "round",    int_to_i,        ARGS_REQ(1)); /* 15.2.8.3.12 (x) */
  define_method(mrb, integer, "truncate", int_to_i,        ARGS_REQ(1)); /* 15.2.8.3.15 (x) */
#endif

  /* Fixnum Class */
  mrb->fixnum_class = fixnum = define_class(mrb, "Fixnum", integer);
  define_method(mrb, fixnum,  "+",        fix_plus,        ARGS_REQ(1)); /* 15.2.8.3.1  */
  define_method(mrb, fixnum,  "-",        fix_minus,       ARGS_REQ(1)); /* 15.2.8.3.2  */
  define_method(mrb, fixnum,  "*",        fix_mul,         ARGS_REQ(1)); /* 15.2.8.3.3  */
  define_method(mrb, fixnum,  "%",        fix_mod,         ARGS_REQ(1)); /* 15.2.8.3.5  */
  define_method(mrb, fixnum,  "==",       fix_equal,       ARGS_REQ(1)); /* 15.2.8.3.7  */
  define_method(mrb, fixnum,  "~",        fix_rev,         ARGS_NONE()); /* 15.2.8.3.8  */
  define_method(mrb, fixnum,  "&",        fix_and,         ARGS_REQ(1)); /* 15.2.8.3.9  */
  define_method(mrb, fixnum,  "|",        fix_or,          ARGS_REQ(1)); /* 15.2.8.3.10 */
  define_method(mrb, fixnum,  "^",        fix_xor,         ARGS_REQ(1)); /* 15.2.8.3.11 */
  define_method(mrb, fixnum,  "<<",       fix_lshift,      ARGS_REQ(1)); /* 15.2.8.3.12 */
  define_method(mrb, fixnum,  ">>",       fix_rshift,      ARGS_REQ(1)); /* 15.2.8.3.13 */
  define_method(mrb, fixnum,  "eql?",     fix_eql,         ARGS_REQ(1)); /* 15.2.8.3.16 */
#ifndef WITHOUT_FLOAT
  define_method(mrb, fixnum,  "to_f",     fix_to_f,        ARGS_NONE()); /* 15.2.8.3.23 */
#endif
  define_method(mrb, fixnum,  "to_s",     fix_to_s,        ARGS_NONE()); /* 15.2.8.3.25 */
  define_method(mrb, fixnum,  "inspect",  fix_to_s,        ARGS_NONE());
  define_method(mrb, fixnum,  "divmod",   fix_divmod,      ARGS_REQ(1)); /* 15.2.8.3.30 (x) */

#ifndef WITHOUT_FLOAT
  /* Float Class */
  mrb->float_class = fl = define_class(mrb, "Float", numeric);                 /* 15.2.9 */
  SET_INSTANCE_TT(fl, TT_FLOAT);
  undef_class_method(mrb,  fl, "new");
  define_method(mrb, fl,      "+",         flo_plus,       ARGS_REQ(1)); /* 15.2.9.3.1  */
  define_method(mrb, fl,      "-",         flo_minus,      ARGS_REQ(1)); /* 15.2.9.3.2  */
  define_method(mrb, fl,      "*",         flo_mul,        ARGS_REQ(1)); /* 15.2.9.3.3  */
  define_method(mrb, fl,      "%",         flo_mod,        ARGS_REQ(1)); /* 15.2.9.3.5  */
  define_method(mrb, fl,      "==",        flo_eq,         ARGS_REQ(1)); /* 15.2.9.3.7  */
  define_method(mrb, fl,      "~",         flo_rev,        ARGS_NONE());
  define_method(mrb, fl,      "&",         flo_and,        ARGS_REQ(1));
  define_method(mrb, fl,      "|",         flo_or,         ARGS_REQ(1));
  define_method(mrb, fl,      "^",         flo_xor,        ARGS_REQ(1));
  define_method(mrb, fl,      ">>",        flo_lshift,     ARGS_REQ(1));
  define_method(mrb, fl,      "<<",        flo_rshift,     ARGS_REQ(1));
  define_method(mrb, fl,      "ceil",      flo_ceil,       ARGS_NONE()); /* 15.2.9.3.8  */
  define_method(mrb, fl,      "finite?",   flo_finite_p,   ARGS_NONE()); /* 15.2.9.3.9  */
  define_method(mrb, fl,      "floor",     flo_floor,      ARGS_NONE()); /* 15.2.9.3.10 */
  define_method(mrb, fl,      "infinite?", flo_infinite_p, ARGS_NONE()); /* 15.2.9.3.11 */
  define_method(mrb, fl,      "round",     flo_round,      ARGS_OPT(1)); /* 15.2.9.3.12 */
  define_method(mrb, fl,      "to_f",      flo_to_f,       ARGS_NONE()); /* 15.2.9.3.13 */
  define_method(mrb, fl,      "to_i",      flo_truncate,   ARGS_NONE()); /* 15.2.9.3.14 */
  define_method(mrb, fl,      "to_int",    flo_truncate,   ARGS_NONE());
  define_method(mrb, fl,      "truncate",  flo_truncate,   ARGS_NONE()); /* 15.2.9.3.15 */
  define_method(mrb, fl,      "divmod",    flo_divmod,     ARGS_REQ(1));
  define_method(mrb, fl,      "eql?",      flo_eql,        ARGS_REQ(1)); /* 15.2.8.3.16 */

  define_method(mrb, fl,      "to_s",      flo_to_s,       ARGS_NONE()); /* 15.2.9.3.16(x) */
  define_method(mrb, fl,      "inspect",   flo_to_s,       ARGS_NONE());
  define_method(mrb, fl,      "nan?",      flo_nan_p,      ARGS_NONE());

#ifdef INFINITY
  define_const(mrb, fl, "INFINITY", float_value(mrb, INFINITY));
#endif
#ifdef NAN
  define_const(mrb, fl, "NAN", float_value(mrb, NAN));
#endif
#endif
  define_module(mrb, "Integral");
}
