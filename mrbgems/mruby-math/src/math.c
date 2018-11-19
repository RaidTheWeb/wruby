/*
** math.c - Math module
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/array.h>

#include <math.h>

static void
domain_error(_state *mrb, const char *func)
{
  struct RClass *math = _module_get(mrb, "Math");
  struct RClass *domainerror = _class_get_under(mrb, math, "DomainError");
  _value str = _str_new_cstr(mrb, func);
  _raisef(mrb, domainerror, "Numerical argument is out of domain - %S", str);
}

/* math functions not provided by Microsoft Visual C++ 2012 or older */
#if defined _MSC_VER && _MSC_VER <= 1700

#include <float.h>

#define MATH_TOLERANCE 1E-12

double
asinh(double x)
{
  double xa, ya, y;

  /* Basic formula loses precision for x < 0, but asinh is an odd function */
  xa = fabs(x);
  if (xa > 3.16227E+18) {
    /* Prevent x*x from overflowing; basic formula reduces to log(2*x) */
    ya = log(xa) + 0.69314718055994530942;
  }
  else {
    /* Basic formula for asinh */
    ya = log(xa + sqrt(xa*xa + 1.0));
  }

  y = _copysign(ya, x);
  return y;
}

double
acosh(double x)
{
  double y;

  if (x > 3.16227E+18) {
    /* Prevent x*x from overflowing; basic formula reduces to log(2*x) */
    y = log(x) + 0.69314718055994530942;
  }
  else {
    /* Basic formula for acosh */
    y = log(x + sqrt(x*x - 1.0));
  }

  return y;
}

double
atanh(double x)
{
  double y;

  if (fabs(x) < 1E-2) {
    /* The sums 1+x and 1-x lose precision for small x.  Use the polynomial
       instead. */
    double x2 = x * x;
    y = x*(1.0 + x2*(1.0/3.0 + x2*(1.0/5.0 + x2*(1.0/7.0))));
  }
  else {
    /* Basic formula for atanh */
    y = 0.5 * (log(1.0+x) - log(1.0-x));
  }

  return y;
}

double
cbrt(double x)
{
  double xa, ya, y;

  /* pow(x, y) is undefined for x < 0 and y not an integer, but cbrt is an
     odd function */
  xa = fabs(x);
  ya = pow(xa, 1.0/3.0);
  y = _copysign(ya, x);
  return y;
}

/* Declaration of complementary Error function */
double
erfc(double x);

/*
** Implementations of error functions
** credits to http://www.digitalmars.com/archives/cplusplus/3634.html
*/

/* Implementation of Error function */
double
erf(double x)
{
  static const double two_sqrtpi =  1.128379167095512574;
  double sum  = x;
  double term = x;
  double xsqr = x*x;
  int j= 1;
  if (fabs(x) > 2.2) {
    return 1.0 - erfc(x);
  }
  do {
    term *= xsqr/j;
    sum  -= term/(2*j+1);
    ++j;
    term *= xsqr/j;
    sum  += term/(2*j+1);
    ++j;
  } while (fabs(term/sum) > MATH_TOLERANCE);
  return two_sqrtpi*sum;
}

/* Implementation of complementary Error function */
double
erfc(double x)
{
  static const double one_sqrtpi=  0.564189583547756287;
  double a = 1;
  double b = x;
  double c = x;
  double d = x*x+0.5;
  double q1;
  double q2 = b/d;
  double n = 1.0;
  double t;
  if (fabs(x) < 2.2) {
    return 1.0 - erf(x);
  }
  if (x < 0.0) { /*signbit(x)*/
    return 2.0 - erfc(-x);
  }
  do {
    t  = a*n+b*x;
    a  = b;
    b  = t;
    t  = c*n+d*x;
    c  = d;
    d  = t;
    n += 0.5;
    q1 = q2;
    q2 = b/d;
  } while (fabs(q1-q2)/q2 > MATH_TOLERANCE);
  return one_sqrtpi*exp(-x*x)*q2;
}

#endif

#if (defined _MSC_VER && _MSC_VER < 1800) || defined __ANDROID__ || (defined __FreeBSD__  &&  __FreeBSD_version < 803000)

double
log2(double x)
{
    return log10(x)/log10(2.0);
}

#endif

/*
  TRIGONOMETRIC FUNCTIONS
*/

/*
 *  call-seq:
 *     Math.sin(x)    -> float
 *
 *  Computes the sine of <i>x</i> (expressed in radians). Returns
 *  -1..1.
 */
static _value
math_sin(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  x = sin(x);

  return _float_value(mrb, x);
}

/*
 *  call-seq:
 *     Math.cos(x)    -> float
 *
 *  Computes the cosine of <i>x</i> (expressed in radians). Returns
 *  -1..1.
 */
static _value
math_cos(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  x = cos(x);

  return _float_value(mrb, x);
}

/*
 *  call-seq:
 *     Math.tan(x)    -> float
 *
 *  Returns the tangent of <i>x</i> (expressed in radians).
 */
static _value
math_tan(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  x = tan(x);

  return _float_value(mrb, x);
}

/*
  INVERSE TRIGONOMETRIC FUNCTIONS
*/

/*
 *  call-seq:
 *     Math.asin(x)    -> float
 *
 *  Computes the arc sine of <i>x</i>.
 *  @return computed value between `-(PI/2)` and `(PI/2)`.
 */
static _value
math_asin(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  if (x < -1.0 || x > 1.0) {
    domain_error(mrb, "asin");
  }
  x = asin(x);

  return _float_value(mrb, x);
}

/*
 *  call-seq:
 *     Math.acos(x)    -> float
 *
 *  Computes the arc cosine of <i>x</i>. Returns 0..PI.
 */
static _value
math_acos(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  if (x < -1.0 || x > 1.0) {
    domain_error(mrb, "acos");
  }
  x = acos(x);

  return _float_value(mrb, x);
}

/*
 *  call-seq:
 *     Math.atan(x)    -> float
 *
 *  Computes the arc tangent of <i>x</i>. Returns `-(PI/2) .. (PI/2)`.
 */
static _value
math_atan(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  x = atan(x);

  return _float_value(mrb, x);
}

/*
 *  call-seq:
 *     Math.atan2(y, x)  -> float
 *
 *  Computes the arc tangent given <i>y</i> and <i>x</i>. Returns
 *  -PI..PI.
 *
 *    Math.atan2(-0.0, -1.0) #=> -3.141592653589793
 *    Math.atan2(-1.0, -1.0) #=> -2.356194490192345
 *    Math.atan2(-1.0, 0.0)  #=> -1.5707963267948966
 *    Math.atan2(-1.0, 1.0)  #=> -0.7853981633974483
 *    Math.atan2(-0.0, 1.0)  #=> -0.0
 *    Math.atan2(0.0, 1.0)   #=> 0.0
 *    Math.atan2(1.0, 1.0)   #=> 0.7853981633974483
 *    Math.atan2(1.0, 0.0)   #=> 1.5707963267948966
 *    Math.atan2(1.0, -1.0)  #=> 2.356194490192345
 *    Math.atan2(0.0, -1.0)  #=> 3.141592653589793
 *
 */
static _value
math_atan2(_state *mrb, _value obj)
{
  _float x, y;

  _get_args(mrb, "ff", &x, &y);
  x = atan2(x, y);

  return _float_value(mrb, x);
}



/*
  HYPERBOLIC TRIG FUNCTIONS
*/
/*
 *  call-seq:
 *     Math.sinh(x)    -> float
 *
 *  Computes the hyperbolic sine of <i>x</i> (expressed in
 *  radians).
 */
static _value
math_sinh(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  x = sinh(x);

  return _float_value(mrb, x);
}

/*
 *  call-seq:
 *     Math.cosh(x)    -> float
 *
 *  Computes the hyperbolic cosine of <i>x</i> (expressed in radians).
 */
static _value
math_cosh(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  x = cosh(x);

  return _float_value(mrb, x);
}

/*
 *  call-seq:
 *     Math.tanh()    -> float
 *
 *  Computes the hyperbolic tangent of <i>x</i> (expressed in
 *  radians).
 */
static _value
math_tanh(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  x = tanh(x);

  return _float_value(mrb, x);
}


/*
  INVERSE HYPERBOLIC TRIG FUNCTIONS
*/

/*
 *  call-seq:
 *     Math.asinh(x)    -> float
 *
 *  Computes the inverse hyperbolic sine of <i>x</i>.
 */
static _value
math_asinh(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);

  x = asinh(x);

  return _float_value(mrb, x);
}

/*
 *  call-seq:
 *     Math.acosh(x)    -> float
 *
 *  Computes the inverse hyperbolic cosine of <i>x</i>.
 */
static _value
math_acosh(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  if (x < 1.0) {
    domain_error(mrb, "acosh");
  }
  x = acosh(x);

  return _float_value(mrb, x);
}

/*
 *  call-seq:
 *     Math.atanh(x)    -> float
 *
 *  Computes the inverse hyperbolic tangent of <i>x</i>.
 */
static _value
math_atanh(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  if (x < -1.0 || x > 1.0) {
    domain_error(mrb, "atanh");
  }
  x = atanh(x);

  return _float_value(mrb, x);
}

/*
  EXPONENTIALS AND LOGARITHMS
*/

/*
 *  call-seq:
 *     Math.exp(x)    -> float
 *
 *  Returns e**x.
 *
 *    Math.exp(0)       #=> 1.0
 *    Math.exp(1)       #=> 2.718281828459045
 *    Math.exp(1.5)     #=> 4.4816890703380645
 *
 */
static _value
math_exp(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  x = exp(x);

  return _float_value(mrb, x);
}

/*
 *  call-seq:
 *     Math.log(numeric)    -> float
 *     Math.log(num,base)   -> float
 *
 *  Returns the natural logarithm of <i>numeric</i>.
 *  If additional second argument is given, it will be the base
 *  of logarithm.
 *
 *    Math.log(1)          #=> 0.0
 *    Math.log(Math::E)    #=> 1.0
 *    Math.log(Math::E**3) #=> 3.0
 *    Math.log(12,3)       #=> 2.2618595071429146
 *
 */
static _value
math_log(_state *mrb, _value obj)
{
  _float x, base;
  _int argc;

  argc = _get_args(mrb, "f|f", &x, &base);
  if (x < 0.0) {
    domain_error(mrb, "log");
  }
  x = log(x);
  if (argc == 2) {
    if (base < 0.0) {
      domain_error(mrb, "log");
    }
    x /= log(base);
  }
  return _float_value(mrb, x);
}

/*
 *  call-seq:
 *     Math.log2(numeric)    -> float
 *
 *  Returns the base 2 logarithm of <i>numeric</i>.
 *
 *    Math.log2(1)      #=> 0.0
 *    Math.log2(2)      #=> 1.0
 *    Math.log2(32768)  #=> 15.0
 *    Math.log2(65536)  #=> 16.0
 *
 */
static _value
math_log2(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  if (x < 0.0) {
    domain_error(mrb, "log2");
  }
  x = log2(x);

  return _float_value(mrb, x);
}

/*
 *  call-seq:
 *     Math.log10(numeric)    -> float
 *
 *  Returns the base 10 logarithm of <i>numeric</i>.
 *
 *    Math.log10(1)       #=> 0.0
 *    Math.log10(10)      #=> 1.0
 *    Math.log10(10**100) #=> 100.0
 *
 */
static _value
math_log10(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  if (x < 0.0) {
    domain_error(mrb, "log10");
  }
  x = log10(x);

  return _float_value(mrb, x);
}

/*
 *  call-seq:
 *     Math.sqrt(numeric)    -> float
 *
 *  Returns the square root of <i>numeric</i>.
 *
 */
static _value
math_sqrt(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  if (x < 0.0) {
    domain_error(mrb, "sqrt");
  }
  x = sqrt(x);

  return _float_value(mrb, x);
}


/*
 *  call-seq:
 *     Math.cbrt(numeric)    -> float
 *
 *  Returns the cube root of <i>numeric</i>.
 *
 *    -9.upto(9) {|x|
 *      p [x, Math.cbrt(x), Math.cbrt(x)**3]
 *    }
 *    #=>
 *    [-9, -2.0800838230519, -9.0]
 *    [-8, -2.0, -8.0]
 *    [-7, -1.91293118277239, -7.0]
 *    [-6, -1.81712059283214, -6.0]
 *    [-5, -1.7099759466767, -5.0]
 *    [-4, -1.5874010519682, -4.0]
 *    [-3, -1.44224957030741, -3.0]
 *    [-2, -1.25992104989487, -2.0]
 *    [-1, -1.0, -1.0]
 *    [0, 0.0, 0.0]
 *    [1, 1.0, 1.0]
 *    [2, 1.25992104989487, 2.0]
 *    [3, 1.44224957030741, 3.0]
 *    [4, 1.5874010519682, 4.0]
 *    [5, 1.7099759466767, 5.0]
 *    [6, 1.81712059283214, 6.0]
 *    [7, 1.91293118277239, 7.0]
 *    [8, 2.0, 8.0]
 *    [9, 2.0800838230519, 9.0]
 *
 */
static _value
math_cbrt(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  x = cbrt(x);

  return _float_value(mrb, x);
}


/*
 *  call-seq:
 *     Math.frexp(numeric)    -> [ fraction, exponent ]
 *
 *  Returns a two-element array containing the normalized fraction (a
 *  <code>Float</code>) and exponent (a <code>Fixnum</code>) of
 *  <i>numeric</i>.
 *
 *     fraction, exponent = Math.frexp(1234)   #=> [0.6025390625, 11]
 *     fraction * 2**exponent                  #=> 1234.0
 */
static _value
math_frexp(_state *mrb, _value obj)
{
  _float x;
  int exp;

  _get_args(mrb, "f", &x);
  x = frexp(x, &exp);

  return _assoc_new(mrb, _float_value(mrb, x), _fixnum_value(exp));
}

/*
 *  call-seq:
 *     Math.ldexp(flt, int) -> float
 *
 *  Returns the value of <i>flt</i>*(2**<i>int</i>).
 *
 *     fraction, exponent = Math.frexp(1234)
 *     Math.ldexp(fraction, exponent)   #=> 1234.0
 */
static _value
math_ldexp(_state *mrb, _value obj)
{
  _float x;
  _int   i;

  _get_args(mrb, "fi", &x, &i);
  x = ldexp(x, (int)i);

  return _float_value(mrb, x);
}

/*
 *  call-seq:
 *     Math.hypot(x, y)    -> float
 *
 *  Returns sqrt(x**2 + y**2), the hypotenuse of a right-angled triangle
 *  with sides <i>x</i> and <i>y</i>.
 *
 *     Math.hypot(3, 4)   #=> 5.0
 */
static _value
math_hypot(_state *mrb, _value obj)
{
  _float x, y;

  _get_args(mrb, "ff", &x, &y);
  x = hypot(x, y);

  return _float_value(mrb, x);
}

/*
 * call-seq:
 *    Math.erf(x)  -> float
 *
 *  Calculates the error function of x.
 */
static _value
math_erf(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  x = erf(x);

  return _float_value(mrb, x);
}


/*
 * call-seq:
 *    Math.erfc(x)  -> float
 *
 *  Calculates the complementary error function of x.
 */
static _value
math_erfc(_state *mrb, _value obj)
{
  _float x;

  _get_args(mrb, "f", &x);
  x = erfc(x);

  return _float_value(mrb, x);
}

/* ------------------------------------------------------------------------*/
void
_mruby_math_gem_init(_state* mrb)
{
  struct RClass *_math;
  _math = _define_module(mrb, "Math");

  _define_class_under(mrb, _math, "DomainError", mrb->eStandardError_class);

#ifdef M_PI
  _define_const(mrb, _math, "PI", _float_value(mrb, M_PI));
#else
  _define_const(mrb, _math, "PI", _float_value(mrb, atan(1.0)*4.0));
#endif

#ifdef M_E
  _define_const(mrb, _math, "E", _float_value(mrb, M_E));
#else
  _define_const(mrb, _math, "E", _float_value(mrb, exp(1.0)));
#endif

#ifdef MRB_USE_FLOAT
  _define_const(mrb, _math, "TOLERANCE", _float_value(mrb, 1e-5));
#else
  _define_const(mrb, _math, "TOLERANCE", _float_value(mrb, 1e-12));
#endif

  _define_module_function(mrb, _math, "sin", math_sin, MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "cos", math_cos, MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "tan", math_tan, MRB_ARGS_REQ(1));

  _define_module_function(mrb, _math, "asin", math_asin, MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "acos", math_acos, MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "atan", math_atan, MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "atan2", math_atan2, MRB_ARGS_REQ(2));

  _define_module_function(mrb, _math, "sinh", math_sinh, MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "cosh", math_cosh, MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "tanh", math_tanh, MRB_ARGS_REQ(1));

  _define_module_function(mrb, _math, "asinh", math_asinh, MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "acosh", math_acosh, MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "atanh", math_atanh, MRB_ARGS_REQ(1));

  _define_module_function(mrb, _math, "exp", math_exp, MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "log", math_log, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  _define_module_function(mrb, _math, "log2", math_log2, MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "log10", math_log10, MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "sqrt", math_sqrt, MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "cbrt", math_cbrt, MRB_ARGS_REQ(1));

  _define_module_function(mrb, _math, "frexp", math_frexp, MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "ldexp", math_ldexp, MRB_ARGS_REQ(2));

  _define_module_function(mrb, _math, "hypot", math_hypot, MRB_ARGS_REQ(2));

  _define_module_function(mrb, _math, "erf",  math_erf,  MRB_ARGS_REQ(1));
  _define_module_function(mrb, _math, "erfc", math_erfc, MRB_ARGS_REQ(1));
}

void
_mruby_math_gem_final(_state* mrb)
{
}
