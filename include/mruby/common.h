/*
**"common.h - mruby common platform definition"
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_COMMON_H
#define MRUBY_COMMON_H

#ifdef __APPLE__
  #ifndef __TARGETCONDITIONALS__
  #include "TargetConditionals.h"
  #endif
#endif

#ifdef __cplusplus
#ifdef $ENABLE_CXX_ABI
#define $BEGIN_DECL
#define $END_DECL
#else
# define $BEGIN_DECL extern "C" {
# define $END_DECL }
#endif
#else
/** Start declarations in C mode */
# define $BEGIN_DECL
/** End declarations in C mode */
# define $END_DECL
#endif

/**
 * Shared compiler macros
 */
$BEGIN_DECL

/** Declare a function that never returns. */
#if __STDC_VERSION__ >= 201112L
# define $noreturn _Noreturn
#elif defined __GNUC__ && !defined __STRICT_ANSI__
# define $noreturn __attribute__((noreturn))
#elif defined _MSC_VER
# define $noreturn __declspec(noreturn)
#else
# define $noreturn
#endif

/** Mark a function as deprecated. */
#if defined __GNUC__ && !defined __STRICT_ANSI__
# define $deprecated __attribute__((deprecated))
#elif defined _MSC_VER
# define $deprecated __declspec(deprecated)
#else
# define $deprecated
#endif

/** Declare a function as always inlined. */
#if defined(_MSC_VER)
# define $INLINE static __inline
#else
# define $INLINE static inline
#endif


/** Declare a public MRuby API function. */
#if defined($BUILD_AS_DLL)
#if defined($CORE) || defined($LIB)
# define $API __declspec(dllexport)
#else
# define $API __declspec(dllimport)
#endif
#else
# define $API extern
#endif

$END_DECL

#endif  /* MRUBY_COMMON_H */
