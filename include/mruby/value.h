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
MRB_BEGIN_DECL

typedef uint32_t _sym;
typedef uint8_t _bool;
struct _state;

#if defined(MRB_INT16) && defined(MRB_INT64)
# error "You can't define MRB_INT16 and MRB_INT64 at the same time."
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

#if defined(MRB_INT64)
  typedef int64_t _int;
# define MRB_INT_BIT 64
# define MRB_INT_MIN (INT64_MIN>>MRB_FIXNUM_SHIFT)
# define MRB_INT_MAX (INT64_MAX>>MRB_FIXNUM_SHIFT)
# define MRB_PRIo PRIo64
# define MRB_PRId PRId64
# define MRB_PRIx PRIx64
#elif defined(MRB_INT16)
  typedef int16_t _int;
# define MRB_INT_BIT 16
# define MRB_INT_MIN (INT16_MIN>>MRB_FIXNUM_SHIFT)
# define MRB_INT_MAX (INT16_MAX>>MRB_FIXNUM_SHIFT)
# define MRB_PRIo PRIo16
# define MRB_PRId PRId16
# define MRB_PRIx PRIx16
#else
  typedef int32_t _int;
# define MRB_INT_BIT 32
# define MRB_INT_MIN (INT32_MIN>>MRB_FIXNUM_SHIFT)
# define MRB_INT_MAX (INT32_MAX>>MRB_FIXNUM_SHIFT)
# define MRB_PRIo PRIo32
# define MRB_PRId PRId32
# define MRB_PRIx PRIx32
#endif


#ifndef MRB_WITHOUT_FLOAT
MRB_API double _float_read(const char*, char**);
#ifdef MRB_USE_FLOAT
  typedef float _float;
#else
  typedef double _float;
#endif
#endif

#if defined _MSC_VER && _MSC_VER < 1900
# ifndef __cplusplus
#  define inline __inline
# endif
# include <stdarg.h>
MRB_API int _msvc_vsnprintf(char *s, size_t n, const char *format, va_list arg);
MRB_API int _msvc_snprintf(char *s, size_t n, const char *format, ...);
# define vsnprintf(s, n, format, arg) _msvc_vsnprintf(s, n, format, arg)
# define snprintf(s, n, format, ...) _msvc_snprintf(s, n, format, __VA_ARGS__)
# if _MSC_VER < 1800 && !defined MRB_WITHOUT_FLOAT
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

enum _vtype {
  MRB_TT_FALSE = 0,   /*   0 */
  MRB_TT_FREE,        /*   1 */
  MRB_TT_TRUE,        /*   2 */
  MRB_TT_FIXNUM,      /*   3 */
  MRB_TT_SYMBOL,      /*   4 */
  MRB_TT_UNDEF,       /*   5 */
  MRB_TT_FLOAT,       /*   6 */
  MRB_TT_CPTR,        /*   7 */
  MRB_TT_OBJECT,      /*   8 */
  MRB_TT_CLASS,       /*   9 */
  MRB_TT_MODULE,      /*  10 */
  MRB_TT_ICLASS,      /*  11 */
  MRB_TT_SCLASS,      /*  12 */
  MRB_TT_PROC,        /*  13 */
  MRB_TT_ARRAY,       /*  14 */
  MRB_TT_HASH,        /*  15 */
  MRB_TT_STRING,      /*  16 */
  MRB_TT_RANGE,       /*  17 */
  MRB_TT_EXCEPTION,   /*  18 */
  MRB_TT_FILE,        /*  19 */
  MRB_TT_ENV,         /*  20 */
  MRB_TT_DATA,        /*  21 */
  MRB_TT_FIBER,       /*  22 */
  MRB_TT_ISTRUCT,     /*  23 */
  MRB_TT_BREAK,       /*  24 */
  MRB_TT_MAXDEFINE    /*  25 */
};

#include <mruby/object.h>

#ifdef MRB_DOCUMENTATION_BLOCK

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
typedef void _value;

#endif

#if defined(MRB_NAN_BOXING)
#include "boxing_nan.h"
#elif defined(MRB_WORD_BOXING)
#include "boxing_word.h"
#else
#include "boxing_no.h"
#endif

#ifndef _fixnum_p
#define _fixnum_p(o) (_type(o) == MRB_TT_FIXNUM)
#endif
#ifndef _undef_p
#define _undef_p(o) (_type(o) == MRB_TT_UNDEF)
#endif
#ifndef _nil_p
#define _nil_p(o)  (_type(o) == MRB_TT_FALSE && !_fixnum(o))
#endif
#ifndef _bool
#define _bool(o)   (_type(o) != MRB_TT_FALSE)
#endif
#ifndef MRB_WITHOUT_FLOAT
#define _float_p(o) (_type(o) == MRB_TT_FLOAT)
#endif
#define _symbol_p(o) (_type(o) == MRB_TT_SYMBOL)
#define _array_p(o) (_type(o) == MRB_TT_ARRAY)
#define _string_p(o) (_type(o) == MRB_TT_STRING)
#define _hash_p(o) (_type(o) == MRB_TT_HASH)
#define _cptr_p(o) (_type(o) == MRB_TT_CPTR)
#define _exception_p(o) (_type(o) == MRB_TT_EXCEPTION)
#define _test(o)   _bool(o)
MRB_API _bool _regexp_p(struct _state*, _value);

/*
 * Returns a float in Ruby.
 */
#ifndef MRB_WITHOUT_FLOAT
MRB_INLINE _value _float_value(struct _state *mrb, _float f)
{
  _value v;
  (void) mrb;
  SET_FLOAT_VALUE(mrb, v, f);
  return v;
}
#endif

static inline _value
_cptr_value(struct _state *mrb, void *p)
{
  _value v;
  (void) mrb;
  SET_CPTR_VALUE(mrb,v,p);
  return v;
}

/*
 * Returns a fixnum in Ruby.
 */
MRB_INLINE _value _fixnum_value(_int i)
{
  _value v;
  SET_INT_VALUE(v, i);
  return v;
}

static inline _value
_symbol_value(_sym i)
{
  _value v;
  SET_SYM_VALUE(v, i);
  return v;
}

static inline _value
_obj_value(void *p)
{
  _value v;
  SET_OBJ_VALUE(v, (struct RBasic*)p);
  _assert(p == _ptr(v));
  _assert(((struct RBasic*)p)->tt == _type(v));
  return v;
}


/*
 * Get a nil _value object.
 *
 * @return
 *      nil _value object reference.
 */
MRB_INLINE _value _nil_value(void)
{
  _value v;
  SET_NIL_VALUE(v);
  return v;
}

/*
 * Returns false in Ruby.
 */
MRB_INLINE _value _false_value(void)
{
  _value v;
  SET_FALSE_VALUE(v);
  return v;
}

/*
 * Returns true in Ruby.
 */
MRB_INLINE _value _true_value(void)
{
  _value v;
  SET_TRUE_VALUE(v);
  return v;
}

static inline _value
_bool_value(_bool boolean)
{
  _value v;
  SET_BOOL_VALUE(v, boolean);
  return v;
}

static inline _value
_undef_value(void)
{
  _value v;
  SET_UNDEF_VALUE(v);
  return v;
}

#ifdef MRB_USE_ETEXT_EDATA
#if (defined(__APPLE__) && defined(__MACH__))
#include <mach-o/getsect.h>
static inline _bool
_ro_data_p(const char *p)
{
  return (const char*)get_etext() < p && p < (const char*)get_edata();
}
#else
extern char _etext[];
#ifdef MRB_NO_INIT_ARRAY_START
extern char _edata[];

static inline _bool
_ro_data_p(const char *p)
{
  return _etext < p && p < _edata;
}
#else
extern char __init_array_start[];

static inline _bool
_ro_data_p(const char *p)
{
  return _etext < p && p < (char*)&__init_array_start;
}
#endif
#endif
#else
# define _ro_data_p(p) FALSE
#endif

MRB_END_DECL

#endif  /* MRUBY_VALUE_H */
