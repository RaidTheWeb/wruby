/*
** numeric.c - Numeric, Integer, Float, Fixnum class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRB_WITHOUT_FLOAT
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

#ifndef MRB_WITHOUT_FLOAT
#ifdef MRB_USE_FLOAT
#define trunc(f) truncf(f)
#define floor(f) floorf(f)
#define ceil(f) ceilf(f)
#define fmod(x,y) fmodf(x,y)
#define MRB_FLO_TO_STR_FMT "%.8g"
#else
#define MRB_FLO_TO_STR_FMT "%.16g"
#endif
#endif

#ifndef MRB_WITHOUT_FLOAT
MRB_API _float
_to_flo(state *mrb, value val)
{
  switch (_type(val)) {
  case MRB_TT_FIXNUM:
    return (_float)_fixnum(val);
  case MRB_TT_FLOAT:
    break;
  default:
    _raise(mrb, E_TYPE_ERROR, "non float value");
  }
  return _float(val);
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
#ifndef MRB_WITHOUT_FLOAT
  _float d;
#endif

  _get_args(mrb, "o", &y);
  if (_fixnum_p(x) && _fixnum_p(y)) {
    /* try ipow() */
    _int base = _fixnum(x);
    _int exp = _fixnum(y);
    _int result = 1;

    if (exp < 0)
#ifdef MRB_WITHOUT_FLOAT
      return _fixnum_value(0);
#else
      goto float_pow;
#endif
    for (;;) {
      if (exp & 1) {
        if (_int_mul_overflow(result, base, &result)) {
#ifndef MRB_WITHOUT_FLOAT
          goto float_pow;
#endif
        }
      }
      exp >>= 1;
      if (exp == 0) break;
      if (_int_mul_overflow(base, base, &base)) {
#ifndef MRB_WITHOUT_FLOAT
        goto float_pow;
#endif
      }
    }
    return _fixnum_value(result);
  }
#ifdef MRB_WITHOUT_FLOAT
  _raise(mrb, E_TYPE_ERROR, "non fixnum value");
#else
 float_pow:
  d = pow(_to_flo(mrb, x), _to_flo(mrb, y));
  return _float_value(mrb, d);
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
_num_div(state *mrb, value x, value y)
{
#ifdef MRB_WITHOUT_FLOAT
  if (!_fixnum_p(y)) {
    _raise(mrb, E_TYPE_ERROR, "non fixnum value");
  }
  return _fixnum_value(_fixnum(x) / _fixnum(y));
#else
  return _float_value(mrb, _to_flo(mrb, x) / _to_flo(mrb, y));
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
#ifdef MRB_WITHOUT_FLOAT
  value y;

  _get_args(mrb, "o", &y);
  if (!_fixnum_p(y)) {
    _raise(mrb, E_TYPE_ERROR, "non fixnum value");
  }
  return _fixnum_value(_fixnum(x) / _fixnum(y));
#else
  _float y;

  _get_args(mrb, "f", &y);
  return _float_value(mrb, _to_flo(mrb, x) / y);
#endif
}

#ifndef MRB_WITHOUT_FLOAT
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
  if (isnan(_float(flt))) {
    return _str_new_lit(mrb, "NaN");
  }
  return _float_to_str(mrb, flt, MRB_FLO_TO_STR_FMT);
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

  _get_args(mrb, "o", &y);
  return _float_value(mrb, _float(x) - _to_flo(mrb, y));
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

  _get_args(mrb, "o", &y);
  return _float_value(mrb, _float(x) * _to_flo(mrb, y));
}

static void
flodivmod(state *mrb, double x, double y, _float *divp, _float *modp)
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
  _float mod;

  _get_args(mrb, "o", &y);

  flodivmod(mrb, _float(x), _to_flo(mrb, y), 0, &mod);
  return _float_value(mrb, mod);
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

  _get_args(mrb, "o", &y);
  if (!_fixnum_p(y)) return _false_value();
  return _bool_value(_fixnum(x) == _fixnum(y));
}

#ifndef MRB_WITHOUT_FLOAT
static value
flo_eql(state *mrb, value x)
{
  value y;

  _get_args(mrb, "o", &y);
  if (!_float_p(y)) return _false_value();
  return _bool_value(_float(x) == (_float)_fixnum(y));
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
  _get_args(mrb, "o", &y);

  switch (_type(y)) {
  case MRB_TT_FIXNUM:
    return _bool_value(_float(x) == (_float)_fixnum(y));
  case MRB_TT_FLOAT:
    return _bool_value(_float(x) == _float(y));
  default:
    return _false_value();
  }
}

static int64_t
value_int64(state *mrb, value x)
{
  switch (_type(x)) {
  case MRB_TT_FIXNUM:
    return (int64_t)_fixnum(x);
    break;
  case MRB_TT_FLOAT:
    return (int64_t)_float(x);
  default:
    _raise(mrb, E_TYPE_ERROR, "cannot convert to Integer");
    break;
  }
  /* not reached */
  return 0;
}

static value
int64_value(state *mrb, int64_t v)
{
  if (FIXABLE(v)) {
    return _fixnum_value((_int)v);
  }
  return _float_value(mrb, (_float)v);
}

static value
flo_rev(state *mrb, value x)
{
  int64_t v1;
  _get_args(mrb, "");
  v1 = (int64_t)_float(x);
  return int64_value(mrb, ~v1);
}

static value
flo_and(state *mrb, value x)
{
  value y;
  int64_t v1, v2;
  _get_args(mrb, "o", &y);

  v1 = (int64_t)_float(x);
  v2 = value_int64(mrb, y);
  return int64_value(mrb, v1 & v2);
}

static value
flo_or(state *mrb, value x)
{
  value y;
  int64_t v1, v2;
  _get_args(mrb, "o", &y);

  v1 = (int64_t)_float(x);
  v2 = value_int64(mrb, y);
  return int64_value(mrb, v1 | v2);
}

static value
flo_xor(state *mrb, value x)
{
  value y;
  int64_t v1, v2;
  _get_args(mrb, "o", &y);

  v1 = (int64_t)_float(x);
  v2 = value_int64(mrb, y);
  return int64_value(mrb, v1 ^ v2);
}

static value
flo_shift(state *mrb, value x, _int width)
{
  _float val;

  if (width == 0) {
    return x;
  }
  val = _float(x);
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
    if (val == 0 && _float(x) < 0) {
      return _fixnum_value(-1);
    }
  }
  else {
    while (width--) {
      val *= 2;
    }
  }
  if (FIXABLE_FLOAT(val)) {
    return _fixnum_value((_int)val);
  }
  return _float_value(mrb, val);
}

static value
flo_lshift(state *mrb, value x)
{
  _int width;

  _get_args(mrb, "i", &width);
  return flo_shift(mrb, x, -width);
}

static value
flo_rshift(state *mrb, value x)
{
  _int width;

  _get_args(mrb, "i", &width);
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
  _float value = _float(num);

  if (isinf(value)) {
    return _fixnum_value(value < 0 ? -1 : 1);
  }
  return _nil_value();
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
  return _bool_value(isfinite(_float(num)));
}

void
_check_num_exact(state *mrb, _float num)
{
  if (isinf(num)) {
    _raise(mrb, E_FLOATDOMAIN_ERROR, num < 0 ? "-Infinity" : "Infinity");
  }
  if (isnan(num)) {
    _raise(mrb, E_FLOATDOMAIN_ERROR, "NaN");
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
  _float f = floor(_float(num));

  _check_num_exact(mrb, f);
  if (!FIXABLE_FLOAT(f)) {
    return _float_value(mrb, f);
  }
  return _fixnum_value((_int)f);
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
  _float f = ceil(_float(num));

  _check_num_exact(mrb, f);
  if (!FIXABLE_FLOAT(f)) {
    return _float_value(mrb, f);
  }
  return _fixnum_value((_int)f);
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
  _int ndigits = 0;
  _int i;

  _get_args(mrb, "|i", &ndigits);
  number = _float(num);

  if (0 < ndigits && (isinf(number) || isnan(number))) {
    return num;
  }
  _check_num_exact(mrb, number);

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
    return _float_value(mrb, number);
  }
  return _fixnum_value((_int)number);
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
  _float f = _float(num);

  if (f > 0.0) f = floor(f);
  if (f < 0.0) f = ceil(f);

  _check_num_exact(mrb, f);
  if (!FIXABLE_FLOAT(f)) {
    return _float_value(mrb, f);
  }
  return _fixnum_value((_int)f);
}

static value
flo_nan_p(state *mrb, value num)
{
  return _bool_value(isnan(_float(num)));
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
_fixnum_mul(state *mrb, value x, value y)
{
  _int a;

  a = _fixnum(x);
  if (_fixnum_p(y)) {
    _int b, c;

    if (a == 0) return x;
    b = _fixnum(y);
    if (_int_mul_overflow(a, b, &c)) {
#ifndef MRB_WITHOUT_FLOAT
      return _float_value(mrb, (_float)a * (_float)b);
#endif
    }
    return _fixnum_value(c);
  }
#ifdef MRB_WITHOUT_FLOAT
  _raise(mrb, E_TYPE_ERROR, "non fixnum value");
#else
  return _float_value(mrb, (_float)a * _to_flo(mrb, y));
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

  _get_args(mrb, "o", &y);
  return _fixnum_mul(mrb, x, y);
}

static void
fixdivmod(state *mrb, _int x, _int y, _int *divp, _int *modp)
{
  _int div, mod;

  /* TODO: add _assert(y != 0) to make sure */

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
  _int a;

  _get_args(mrb, "o", &y);
  a = _fixnum(x);
  if (_fixnum_p(y)) {
    _int b, mod;

    if ((b=_fixnum(y)) == 0) {
#ifdef MRB_WITHOUT_FLOAT
      /* ZeroDivisionError */
      return _fixnum_value(0);
#else
      return _float_value(mrb, NAN);
#endif
    }
    fixdivmod(mrb, a, b, 0, &mod);
    return _fixnum_value(mod);
  }
#ifdef MRB_WITHOUT_FLOAT
  _raise(mrb, E_TYPE_ERROR, "non fixnum value");
#else
  else {
    _float mod;

    flodivmod(mrb, (_float)a, _to_flo(mrb, y), 0, &mod);
    return _float_value(mrb, mod);
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

  _get_args(mrb, "o", &y);

  if (_fixnum_p(y)) {
    _int div, mod;

    if (_fixnum(y) == 0) {
#ifdef MRB_WITHOUT_FLOAT
      return _assoc_new(mrb, _fixnum_value(0), _fixnum_value(0));
#else
      return _assoc_new(mrb, ((_fixnum(x) == 0) ?
                                 _float_value(mrb, NAN):
                                 _float_value(mrb, INFINITY)),
                           _float_value(mrb, NAN));
#endif
    }
    fixdivmod(mrb, _fixnum(x), _fixnum(y), &div, &mod);
    return _assoc_new(mrb, _fixnum_value(div), _fixnum_value(mod));
  }
#ifdef MRB_WITHOUT_FLOAT
  _raise(mrb, E_TYPE_ERROR, "non fixnum value");
#else
  else {
    _float div, mod;
    value a, b;

    flodivmod(mrb, (_float)_fixnum(x), _to_flo(mrb, y), &div, &mod);
    a = _float_value(mrb, div);
    b = _float_value(mrb, mod);
    return _assoc_new(mrb, a, b);
  }
#endif
}

#ifndef MRB_WITHOUT_FLOAT
static value
flo_divmod(state *mrb, value x)
{
  value y;
  _float div, mod;
  value a, b;

  _get_args(mrb, "o", &y);

  flodivmod(mrb, _float(x), _to_flo(mrb, y), &div, &mod);
  a = _float_value(mrb, div);
  b = _float_value(mrb, mod);
  return _assoc_new(mrb, a, b);
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

  _get_args(mrb, "o", &y);
  switch (_type(y)) {
  case MRB_TT_FIXNUM:
    return _bool_value(_fixnum(x) == _fixnum(y));
#ifndef MRB_WITHOUT_FLOAT
  case MRB_TT_FLOAT:
    return _bool_value((_float)_fixnum(x) == _float(y));
#endif
  default:
    return _false_value();
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
  _int val = _fixnum(num);

  return _fixnum_value(~val);
}

#ifdef MRB_WITHOUT_FLOAT
#define bit_op(x,y,op1,op2) do {\
  return _fixnum_value(_fixnum(x) op2 _fixnum(y));\
} while(0)
#else
static value flo_and(state *mrb, value x);
static value flo_or(state *mrb, value x);
static value flo_xor(state *mrb, value x);
#define bit_op(x,y,op1,op2) do {\
  if (_fixnum_p(y)) return _fixnum_value(_fixnum(x) op2 _fixnum(y));\
  return flo_ ## op1(mrb, _float_value(mrb, (_float)_fixnum(x)));\
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

  _get_args(mrb, "o", &y);
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

  _get_args(mrb, "o", &y);
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

  _get_args(mrb, "o", &y);
  bit_op(x, y, or, ^);
}

#define NUMERIC_SHIFT_WIDTH_MAX (MRB_INT_BIT-1)

static value
lshift(state *mrb, _int val, _int width)
{
  if (width < 0) {              /* _int overflow */
#ifdef MRB_WITHOUT_FLOAT
    return _fixnum_value(0);
#else
    return _float_value(mrb, INFINITY);
#endif
  }
  if (val > 0) {
    if ((width > NUMERIC_SHIFT_WIDTH_MAX) ||
        (val   > (MRB_INT_MAX >> width))) {
#ifdef MRB_WITHOUT_FLOAT
      return _fixnum_value(-1);
#else
      goto bit_overflow;
#endif
    }
    return _fixnum_value(val << width);
  }
  else {
    if ((width > NUMERIC_SHIFT_WIDTH_MAX) ||
        (val   < (MRB_INT_MIN >> width))) {
#ifdef MRB_WITHOUT_FLOAT
      return _fixnum_value(0);
#else
      goto bit_overflow;
#endif
    }
    return _fixnum_value(val * ((_int)1 << width));
  }

#ifndef MRB_WITHOUT_FLOAT
bit_overflow:
  {
    _float f = (_float)val;
    while (width--) {
      f *= 2;
    }
    return _float_value(mrb, f);
  }
#endif
}

static value
rshift(_int val, _int width)
{
  if (width < 0) {              /* _int overflow */
    return _fixnum_value(0);
  }
  if (width >= NUMERIC_SHIFT_WIDTH_MAX) {
    if (val < 0) {
      return _fixnum_value(-1);
    }
    return _fixnum_value(0);
  }
  return _fixnum_value(val >> width);
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
  _int width, val;

  _get_args(mrb, "i", &width);
  if (width == 0) {
    return x;
  }
  val = _fixnum(x);
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
  _int width, val;

  _get_args(mrb, "i", &width);
  if (width == 0) {
    return x;
  }
  val = _fixnum(x);
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

#ifndef MRB_WITHOUT_FLOAT
static value
fix_to_f(state *mrb, value num)
{
  return _float_value(mrb, (_float)_fixnum(num));
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
MRB_API value
_flo_to_fixnum(state *mrb, value x)
{
  _int z = 0;

  if (!_float_p(x)) {
    _raise(mrb, E_TYPE_ERROR, "non float value");
    z = 0; /* not reached. just suppress warnings. */
  }
  else {
    _float d = _float(x);

    if (isinf(d)) {
      _raise(mrb, E_FLOATDOMAIN_ERROR, d < 0 ? "-Infinity" : "Infinity");
    }
    if (isnan(d)) {
      _raise(mrb, E_FLOATDOMAIN_ERROR, "NaN");
    }
    if (FIXABLE_FLOAT(d)) {
      z = (_int)d;
    }
    else {
      _raisef(mrb, E_ARGUMENT_ERROR, "number (%S) too big for integer", x);
    }
  }
  return _fixnum_value(z);
}
#endif

value
_fixnum_plus(state *mrb, value x, value y)
{
  _int a;

  a = _fixnum(x);
  if (_fixnum_p(y)) {
    _int b, c;

    if (a == 0) return y;
    b = _fixnum(y);
    if (_int_add_overflow(a, b, &c)) {
#ifndef MRB_WITHOUT_FLOAT
      return _float_value(mrb, (_float)a + (_float)b);
#endif
    }
    return _fixnum_value(c);
  }
#ifdef MRB_WITHOUT_FLOAT
  _raise(mrb, E_TYPE_ERROR, "non fixnum value");
#else
  return _float_value(mrb, (_float)a + _to_flo(mrb, y));
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

  _get_args(mrb, "o", &other);
  return _fixnum_plus(mrb, self, other);
}

value
_fixnum_minus(state *mrb, value x, value y)
{
  _int a;

  a = _fixnum(x);
  if (_fixnum_p(y)) {
    _int b, c;

    b = _fixnum(y);
    if (_int_sub_overflow(a, b, &c)) {
#ifndef MRB_WITHOUT_FLOAT
      return _float_value(mrb, (_float)a - (_float)b);
#endif
    }
    return _fixnum_value(c);
  }
#ifdef MRB_WITHOUT_FLOAT
  _raise(mrb, E_TYPE_ERROR, "non fixnum value");
#else
  return _float_value(mrb, (_float)a - _to_flo(mrb, y));
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

  _get_args(mrb, "o", &other);
  return _fixnum_minus(mrb, self, other);
}


MRB_API value
_fixnum_to_str(state *mrb, value x, _int base)
{
  char buf[MRB_INT_BIT+1];
  char *b = buf + sizeof buf;
  _int val = _fixnum(x);

  if (base < 2 || 36 < base) {
    _raisef(mrb, E_ARGUMENT_ERROR, "invalid radix %S", _fixnum_value(base));
  }

  if (val == 0) {
    *--b = '0';
  }
  else if (val < 0) {
    do {
      *--b = _digitmap[-(val % base)];
    } while (val /= base);
    *--b = '-';
  }
  else {
    do {
      *--b = _digitmap[(int)(val % base)];
    } while (val /= base);
  }

  return _str_new(mrb, b, buf + sizeof(buf) - b);
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
  _int base = 10;

  _get_args(mrb, "|i", &base);
  return _fixnum_to_str(mrb, self, base);
}

/* compare two numbers: (1:0:-1; -2 for error) */
static _int
cmpnum(state *mrb, value v1, value v2)
{
#ifdef MRB_WITHOUT_FLOAT
  _int x, y;
#else
  _float x, y;
#endif

#ifdef MRB_WITHOUT_FLOAT
  x = _fixnum(v1);
#else
  x = _to_flo(mrb, v1);
#endif
  switch (_type(v2)) {
  case MRB_TT_FIXNUM:
#ifdef MRB_WITHOUT_FLOAT
    y = _fixnum(v2);
#else
    y = (_float)_fixnum(v2);
#endif
    break;
#ifndef MRB_WITHOUT_FLOAT
  case MRB_TT_FLOAT:
    y = _float(v2);
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
  _int n;

  _get_args(mrb, "o", &other);
  n = cmpnum(mrb, self, other);
  if (n == -2) return _nil_value();
  return _fixnum_value(n);
}

static void
cmperr(state *mrb, value v1, value v2)
{
  _raisef(mrb, E_ARGUMENT_ERROR, "comparison of %S with %S failed",
             _obj_value(_class(mrb, v1)),
             _obj_value(_class(mrb, v2)));
}

static value
num_lt(state *mrb, value self)
{
  value other;
  _int n;

  _get_args(mrb, "o", &other);
  n = cmpnum(mrb, self, other);
  if (n == -2) cmperr(mrb, self, other);
  if (n < 0) return _true_value();
  return _false_value();
}

static value
num_le(state *mrb, value self)
{
  value other;
  _int n;

  _get_args(mrb, "o", &other);
  n = cmpnum(mrb, self, other);
  if (n == -2) cmperr(mrb, self, other);
  if (n <= 0) return _true_value();
  return _false_value();
}

static value
num_gt(state *mrb, value self)
{
  value other;
  _int n;

  _get_args(mrb, "o", &other);
  n = cmpnum(mrb, self, other);
  if (n == -2) cmperr(mrb, self, other);
  if (n > 0) return _true_value();
  return _false_value();
}

static value
num_ge(state *mrb, value self)
{
  value other;
  _int n;

  _get_args(mrb, "o", &other);
  n = cmpnum(mrb, self, other);
  if (n == -2) cmperr(mrb, self, other);
  if (n >= 0) return _true_value();
  return _false_value();
}

static value
num_finite_p(state *mrb, value self)
{
  _get_args(mrb, "");
  return _true_value();
}

static value
num_infinite_p(state *mrb, value self)
{
  _get_args(mrb, "");
  return _false_value();
}

/* 15.2.9.3.1  */
/*
 * call-seq:
 *   float + other  ->  float
 *
 * Returns a new float which is the sum of <code>float</code>
 * and <code>other</code>.
 */
#ifndef MRB_WITHOUT_FLOAT
static value
flo_plus(state *mrb, value x)
{
  value y;

  _get_args(mrb, "o", &y);
  return _float_value(mrb, _float(x) + _to_flo(mrb, y));
}
#endif

/* ------------------------------------------------------------------------*/
void
_init_numeric(state *mrb)
{
  struct RClass *numeric, *integer, *fixnum;
#ifndef MRB_WITHOUT_FLOAT
  struct RClass *fl;
#endif

  /* Numeric Class */
  numeric = _define_class(mrb, "Numeric",  mrb->object_class);                /* 15.2.7 */

  _define_method(mrb, numeric, "**",       num_pow,         MRB_ARGS_REQ(1));
  _define_method(mrb, numeric, "/",        num_div,         MRB_ARGS_REQ(1)); /* 15.2.8.3.4  */
  _define_method(mrb, numeric, "quo",      num_div,         MRB_ARGS_REQ(1)); /* 15.2.7.4.5 (x) */
  _define_method(mrb, numeric, "<=>",      num_cmp,         MRB_ARGS_REQ(1)); /* 15.2.9.3.6  */
  _define_method(mrb, numeric, "<",        num_lt,          MRB_ARGS_REQ(1));
  _define_method(mrb, numeric, "<=",       num_le,          MRB_ARGS_REQ(1));
  _define_method(mrb, numeric, ">",        num_gt,          MRB_ARGS_REQ(1));
  _define_method(mrb, numeric, ">=",       num_ge,          MRB_ARGS_REQ(1));
  _define_method(mrb, numeric, "finite?",  num_finite_p,    MRB_ARGS_NONE());
  _define_method(mrb, numeric, "infinite?",num_infinite_p,  MRB_ARGS_NONE());

  /* Integer Class */
  integer = _define_class(mrb, "Integer",  numeric);                          /* 15.2.8 */
  MRB_SET_INSTANCE_TT(integer, MRB_TT_FIXNUM);
  _undef_class_method(mrb, integer, "new");
  _define_method(mrb, integer, "to_i",     int_to_i,        MRB_ARGS_NONE()); /* 15.2.8.3.24 */
  _define_method(mrb, integer, "to_int",   int_to_i,        MRB_ARGS_NONE());
#ifndef MRB_WITHOUT_FLOAT
  _define_method(mrb, integer, "ceil",     int_to_i,        MRB_ARGS_REQ(1)); /* 15.2.8.3.8 (x) */
  _define_method(mrb, integer, "floor",    int_to_i,        MRB_ARGS_REQ(1)); /* 15.2.8.3.10 (x) */
  _define_method(mrb, integer, "round",    int_to_i,        MRB_ARGS_REQ(1)); /* 15.2.8.3.12 (x) */
  _define_method(mrb, integer, "truncate", int_to_i,        MRB_ARGS_REQ(1)); /* 15.2.8.3.15 (x) */
#endif

  /* Fixnum Class */
  mrb->fixnum_class = fixnum = _define_class(mrb, "Fixnum", integer);
  _define_method(mrb, fixnum,  "+",        fix_plus,        MRB_ARGS_REQ(1)); /* 15.2.8.3.1  */
  _define_method(mrb, fixnum,  "-",        fix_minus,       MRB_ARGS_REQ(1)); /* 15.2.8.3.2  */
  _define_method(mrb, fixnum,  "*",        fix_mul,         MRB_ARGS_REQ(1)); /* 15.2.8.3.3  */
  _define_method(mrb, fixnum,  "%",        fix_mod,         MRB_ARGS_REQ(1)); /* 15.2.8.3.5  */
  _define_method(mrb, fixnum,  "==",       fix_equal,       MRB_ARGS_REQ(1)); /* 15.2.8.3.7  */
  _define_method(mrb, fixnum,  "~",        fix_rev,         MRB_ARGS_NONE()); /* 15.2.8.3.8  */
  _define_method(mrb, fixnum,  "&",        fix_and,         MRB_ARGS_REQ(1)); /* 15.2.8.3.9  */
  _define_method(mrb, fixnum,  "|",        fix_or,          MRB_ARGS_REQ(1)); /* 15.2.8.3.10 */
  _define_method(mrb, fixnum,  "^",        fix_xor,         MRB_ARGS_REQ(1)); /* 15.2.8.3.11 */
  _define_method(mrb, fixnum,  "<<",       fix_lshift,      MRB_ARGS_REQ(1)); /* 15.2.8.3.12 */
  _define_method(mrb, fixnum,  ">>",       fix_rshift,      MRB_ARGS_REQ(1)); /* 15.2.8.3.13 */
  _define_method(mrb, fixnum,  "eql?",     fix_eql,         MRB_ARGS_REQ(1)); /* 15.2.8.3.16 */
#ifndef MRB_WITHOUT_FLOAT
  _define_method(mrb, fixnum,  "to_f",     fix_to_f,        MRB_ARGS_NONE()); /* 15.2.8.3.23 */
#endif
  _define_method(mrb, fixnum,  "to_s",     fix_to_s,        MRB_ARGS_NONE()); /* 15.2.8.3.25 */
  _define_method(mrb, fixnum,  "inspect",  fix_to_s,        MRB_ARGS_NONE());
  _define_method(mrb, fixnum,  "divmod",   fix_divmod,      MRB_ARGS_REQ(1)); /* 15.2.8.3.30 (x) */

#ifndef MRB_WITHOUT_FLOAT
  /* Float Class */
  mrb->float_class = fl = _define_class(mrb, "Float", numeric);                 /* 15.2.9 */
  MRB_SET_INSTANCE_TT(fl, MRB_TT_FLOAT);
  _undef_class_method(mrb,  fl, "new");
  _define_method(mrb, fl,      "+",         flo_plus,       MRB_ARGS_REQ(1)); /* 15.2.9.3.1  */
  _define_method(mrb, fl,      "-",         flo_minus,      MRB_ARGS_REQ(1)); /* 15.2.9.3.2  */
  _define_method(mrb, fl,      "*",         flo_mul,        MRB_ARGS_REQ(1)); /* 15.2.9.3.3  */
  _define_method(mrb, fl,      "%",         flo_mod,        MRB_ARGS_REQ(1)); /* 15.2.9.3.5  */
  _define_method(mrb, fl,      "==",        flo_eq,         MRB_ARGS_REQ(1)); /* 15.2.9.3.7  */
  _define_method(mrb, fl,      "~",         flo_rev,        MRB_ARGS_NONE());
  _define_method(mrb, fl,      "&",         flo_and,        MRB_ARGS_REQ(1));
  _define_method(mrb, fl,      "|",         flo_or,         MRB_ARGS_REQ(1));
  _define_method(mrb, fl,      "^",         flo_xor,        MRB_ARGS_REQ(1));
  _define_method(mrb, fl,      ">>",        flo_lshift,     MRB_ARGS_REQ(1));
  _define_method(mrb, fl,      "<<",        flo_rshift,     MRB_ARGS_REQ(1));
  _define_method(mrb, fl,      "ceil",      flo_ceil,       MRB_ARGS_NONE()); /* 15.2.9.3.8  */
  _define_method(mrb, fl,      "finite?",   flo_finite_p,   MRB_ARGS_NONE()); /* 15.2.9.3.9  */
  _define_method(mrb, fl,      "floor",     flo_floor,      MRB_ARGS_NONE()); /* 15.2.9.3.10 */
  _define_method(mrb, fl,      "infinite?", flo_infinite_p, MRB_ARGS_NONE()); /* 15.2.9.3.11 */
  _define_method(mrb, fl,      "round",     flo_round,      MRB_ARGS_OPT(1)); /* 15.2.9.3.12 */
  _define_method(mrb, fl,      "to_f",      flo_to_f,       MRB_ARGS_NONE()); /* 15.2.9.3.13 */
  _define_method(mrb, fl,      "to_i",      flo_truncate,   MRB_ARGS_NONE()); /* 15.2.9.3.14 */
  _define_method(mrb, fl,      "to_int",    flo_truncate,   MRB_ARGS_NONE());
  _define_method(mrb, fl,      "truncate",  flo_truncate,   MRB_ARGS_NONE()); /* 15.2.9.3.15 */
  _define_method(mrb, fl,      "divmod",    flo_divmod,     MRB_ARGS_REQ(1));
  _define_method(mrb, fl,      "eql?",      flo_eql,        MRB_ARGS_REQ(1)); /* 15.2.8.3.16 */

  _define_method(mrb, fl,      "to_s",      flo_to_s,       MRB_ARGS_NONE()); /* 15.2.9.3.16(x) */
  _define_method(mrb, fl,      "inspect",   flo_to_s,       MRB_ARGS_NONE());
  _define_method(mrb, fl,      "nan?",      flo_nan_p,      MRB_ARGS_NONE());

#ifdef INFINITY
  _define_const(mrb, fl, "INFINITY", _float_value(mrb, INFINITY));
#endif
#ifdef NAN
  _define_const(mrb, fl, "NAN", _float_value(mrb, NAN));
#endif
#endif
  _define_module(mrb, "Integral");
}
