/*
** mruby/value.h - mruby value definitions
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_VALUE_H
#define MRUBY_VALUE_H

#include "common.h"

/**
 * MRuby Value definition functions and macros.
 */
$BEGIN_DECL

typedef uint32_t $sym;
typedef uint8_t $bool;
struct $state;

#if defined($INT16) && defined($INT64)
# error "You can't define $INT16 and $INT64 at the same time."
#endif

#if defined _MSC_VER && _MSC_VER < 1800
# define PRIo64 "llo"
# define PRId64 "lld"
# define PRIx64 "llx"
# define PRIo16 "ho"
# define PRId16 "hd"
# define PRIx16 "hx"
# define PRIo32 "o"
# define PRId32 "d"
# define PRIx32 "x"
#else
# include <inttypes.h>
#endif

#if defined($INT64)
  typedef int64_t $int;
# define $INT_BIT 64
# define $INT_MIN (INT64_MIN>>$FIXNUM_SHIFT)
# define $INT_MAX (INT64_MAX>>$FIXNUM_SHIFT)
# define $PRIo PRIo64
# define $PRId PRId64
# define $PRIx PRIx64
#elif defined($INT16)
  typedef int16_t $int;
# define $INT_BIT 16
# define $INT_MIN (INT16_MIN>>$FIXNUM_SHIFT)
# define $INT_MAX (INT16_MAX>>$FIXNUM_SHIFT)
# define $PRIo PRIo16
# define $PRId PRId16
# define $PRIx PRIx16
#else
  typedef int32_t $int;
# define $INT_BIT 32
# define $INT_MIN (INT32_MIN>>$FIXNUM_SHIFT)
# define $INT_MAX (INT32_MAX>>$FIXNUM_SHIFT)
# define $PRIo PRIo32
# define $PRId PRId32
# define $PRIx PRIx32
#endif


#ifndef $WITHOUT_FLOAT
$API double $float_read(const char*, char**);
#ifdef $USE_FLOAT
  typedef float $float;
#else
  typedef double $float;
#endif
#endif

#if defined _MSC_VER && _MSC_VER < 1900
# ifndef __cplusplus
#  define inline __inline
# endif
# include <stdarg.h>
$API int $msvc_vsnprintf(char *s, size_t n, const char *format, va_list arg);
$API int $msvc_snprintf(char *s, size_t n, const char *format, ...);
# define vsnprintf(s, n, format, arg) $msvc_vsnprintf(s, n, format, arg)
# define snprintf(s, n, format, ...) $msvc_snprintf(s, n, format, __VA_ARGS__)
# if _MSC_VER < 1800 && !defined $WITHOUT_FLOAT
#  include <float.h>
#  define isfinite(n) _finite(n)
#  define isnan _isnan
#  define isinf(n) (!_finite(n) && !_isnan(n))
#  define signbit(n) (_copysign(1.0, (n)) < 0.0)
static const unsigned int IEEE754_INFINITY_BITS_SINGLE = 0x7F800000;
#  define INFINITY (*(float *)&IEEE754_INFINITY_BITS_SINGLE)
#  define NAN ((float)(INFINITY - INFINITY))
# endif
#endif

enum $vtype {
  $TT_FALSE = 0,   /*   0 */
  $TT_FREE,        /*   1 */
  $TT_TRUE,        /*   2 */
  $TT_FIXNUM,      /*   3 */
  $TT_SYMBOL,      /*   4 */
  $TT_UNDEF,       /*   5 */
  $TT_FLOAT,       /*   6 */
  $TT_CPTR,        /*   7 */
  $TT_OBJECT,      /*   8 */
  $TT_CLASS,       /*   9 */
  $TT_MODULE,      /*  10 */
  $TT_ICLASS,      /*  11 */
  $TT_SCLASS,      /*  12 */
  $TT_PROC,        /*  13 */
  $TT_ARRAY,       /*  14 */
  $TT_HASH,        /*  15 */
  $TT_STRING,      /*  16 */
  $TT_RANGE,       /*  17 */
  $TT_EXCEPTION,   /*  18 */
  $TT_FILE,        /*  19 */
  $TT_ENV,         /*  20 */
  $TT_DATA,        /*  21 */
  $TT_FIBER,       /*  22 */
  $TT_ISTRUCT,     /*  23 */
  $TT_BREAK,       /*  24 */
  $TT_MAXDEFINE    /*  25 */
};

#include <mruby/object.h>

#ifdef $DOCUMENTATION_BLOCK

/**
 * @abstract
 * MRuby value boxing.
 *
 * Actual implementation depends on configured boxing type.
 *
 * @see mruby/boxing_no.h Default boxing representation
 * @see mruby/boxing_word.h Word representation
 * @see mruby/boxing_nan.h Boxed double representation
 */
typedef void $value;

#endif

#if defined($NAN_BOXING)
#include "boxing_nan.h"
#elif defined($WORD_BOXING)
#include "boxing_word.h"
#else
#include "boxing_no.h"
#endif

#ifndef $fixnum_p
#define $fixnum_p(o) ($type(o) == $TT_FIXNUM)
#endif
#ifndef $undef_p
#define $undef_p(o) ($type(o) == $TT_UNDEF)
#endif
#ifndef $nil_p
#define $nil_p(o)  ($type(o) == $TT_FALSE && !$fixnum(o))
#endif
#ifndef $bool
#define $bool(o)   ($type(o) != $TT_FALSE)
#endif
#ifndef $WITHOUT_FLOAT
#define $float_p(o) ($type(o) == $TT_FLOAT)
#endif
#define $symbol_p(o) ($type(o) == $TT_SYMBOL)
#define $array_p(o) ($type(o) == $TT_ARRAY)
#define $string_p(o) ($type(o) == $TT_STRING)
#define $hash_p(o) ($type(o) == $TT_HASH)
#define $cptr_p(o) ($type(o) == $TT_CPTR)
#define $exception_p(o) ($type(o) == $TT_EXCEPTION)
#define $test(o)   $bool(o)
$API $bool $regexp_p(struct $state*, $value);

/*
 * Returns a float in Ruby.
 */
#ifndef $WITHOUT_FLOAT
$INLINE $value $float_value(struct $state *mrb, $float f)
{
  $value v;
  (void) mrb;
  SET_FLOAT_VALUE(mrb, v, f);
  return v;
}
#endif

static inline $value
$cptr_value(struct $state *mrb, void *p)
{
  $value v;
  (void) mrb;
  SET_CPTR_VALUE(mrb,v,p);
  return v;
}

/*
 * Returns a fixnum in Ruby.
 */
$INLINE $value $fixnum_value($int i)
{
  $value v;
  SET_INT_VALUE(v, i);
  return v;
}

static inline $value
$symbol_value($sym i)
{
  $value v;
  SET_SYM_VALUE(v, i);
  return v;
}

static inline $value
$obj_value(void *p)
{
  $value v;
  SET_OBJ_VALUE(v, (struct RBasic*)p);
  $assert(p == $ptr(v));
  $assert(((struct RBasic*)p)->tt == $type(v));
  return v;
}


/*
 * Get a nil $value object.
 *
 * @return
 *      nil $value object reference.
 */
$INLINE $value $nil_value(void)
{
  $value v;
  SET_NIL_VALUE(v);
  return v;
}

/*
 * Returns false in Ruby.
 */
$INLINE $value $false_value(void)
{
  $value v;
  SET_FALSE_VALUE(v);
  return v;
}

/*
 * Returns true in Ruby.
 */
$INLINE $value $true_value(void)
{
  $value v;
  SET_TRUE_VALUE(v);
  return v;
}

static inline $value
$bool_value($bool boolean)
{
  $value v;
  SET_BOOL_VALUE(v, boolean);
  return v;
}

static inline $value
$undef_value(void)
{
  $value v;
  SET_UNDEF_VALUE(v);
  return v;
}

#ifdef $USE_ETEXT_EDATA
#if (defined(__APPLE__) && defined(__MACH__))
#include <mach-o/getsect.h>
static inline $bool
$ro_data_p(const char *p)
{
  return (const char*)get_etext() < p && p < (const char*)get_edata();
}
#else
extern char _etext[];
#ifdef $NO_INIT_ARRAY_START
extern char _edata[];

static inline $bool
$ro_data_p(const char *p)
{
  return _etext < p && p < _edata;
}
#else
extern char __init_array_start[];

static inline $bool
$ro_data_p(const char *p)
{
  return _etext < p && p < (char*)&__init_array_start;
}
#endif
#endif
#else
# define $ro_data_p(p) FALSE
#endif

$END_DECL

#endif  /* MRUBY_VALUE_H */
