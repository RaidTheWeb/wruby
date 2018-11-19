/*
** mrbconf.h - mruby core configuration
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBYCONF_H
#define MRUBYCONF_H

#include <limits.h>
#include <stdint.h>

/* architecture selection: */
/* specify -D$32BIT or -D$64BIT to override */
#if !defined($32BIT) && !defined($64BIT)
#if UINT64_MAX == SIZE_MAX
#define $64BIT
#else
#define $32BIT
#endif
#endif

#if defined($32BIT) && defined($64BIT)
#error Cannot build for 32 and 64 bit architecture at the same time
#endif

/* configuration options: */
/* add -D$USE_FLOAT to use float instead of double for floating point numbers */
//#define $USE_FLOAT

/* exclude floating point numbers */
//#define $WITHOUT_FLOAT

/* add -D$METHOD_CACHE to use method cache to improve performance */
//#define $METHOD_CACHE
/* size of the method cache (need to be the power of 2) */
//#define $METHOD_CACHE_SIZE (1<<7)

/* add -D$METHOD_TABLE_INLINE to reduce the size of method table */
/* $METHOD_TABLE_INLINE requires LSB of function pointers to be zero */
/* you might need to specify --falign-functions=n (where n>1) */
//#define $METHOD_TABLE_INLINE

/* add -D$INT16 to use 16bit integer for $int; conflict with $INT64 */
//#define $INT16

/* add -D$INT64 to use 64bit integer for $int; conflict with $INT16 */
//#define $INT64

/* if no specific integer type is chosen */
#if !defined($INT16) && !defined($INT32) && !defined($INT64)
# if defined($64BIT) && !defined($NAN_BOXING)
/* Use 64bit integers on 64bit architecture (without $NAN_BOXING) */
#  define $INT64
# else
/* Otherwise use 32bit integers */
#  define $INT32
# endif
#endif

/* represent $value in boxed double; conflict with $USE_FLOAT and $WITHOUT_FLOAT */
//#define $NAN_BOXING

/* define on big endian machines; used by $NAN_BOXING */
//#define $ENDIAN_BIG

/* represent $value as a word (natural unit of data for the processor) */
//#define $WORD_BOXING

/* string class to handle UTF-8 encoding */
//#define $UTF8_STRING

/* argv max size in $funcall */
//#define $FUNCALL_ARGC_MAX 16

/* number of object per heap page */
//#define $HEAP_PAGE_SIZE 1024

/* if _etext and _edata available, mruby can reduce memory used by symbols */
//#define $USE_ETEXT_EDATA

/* do not use __init_array_start to determine readonly data section;
   effective only when $USE_ETEXT_EDATA is defined */
//#define $NO_INIT_ARRAY_START

/* turn off generational GC by default */
//#define $GC_TURN_OFF_GENERATIONAL

/* default size of khash table bucket */
//#define KHASH_DEFAULT_SIZE 32

/* allocated memory address alignment */
//#define POOL_ALIGNMENT 4

/* page size of memory pool */
//#define POOL_PAGE_SIZE 16000

/* initial minimum size for string buffer */
//#define $STR_BUF_MIN_SIZE 128

/* arena size */
//#define $GC_ARENA_SIZE 100

/* fixed size GC arena */
//#define $GC_FIXED_ARENA

/* state atexit stack size */
//#define $FIXED_STATE_ATEXIT_STACK_SIZE 5

/* fixed size state atexit stack */
//#define $FIXED_STATE_ATEXIT_STACK

/* -D$DISABLE_XXXX to drop following features */
//#define $DISABLE_STDIO /* use of stdio */

/* -D$ENABLE_XXXX to enable following features */
//#define $ENABLE_DEBUG_HOOK /* hooks for debugger */

/* end of configuration */

/* define $DISABLE_XXXX from DISABLE_XXX (for compatibility) */
#ifdef DISABLE_STDIO
#define $DISABLE_STDIO
#endif

/* define $ENABLE_XXXX from ENABLE_XXX (for compatibility) */
#ifdef ENABLE_DEBUG
#define $ENABLE_DEBUG_HOOK
#endif

#ifndef $DISABLE_STDIO
# include <stdio.h>
#endif

#ifndef FALSE
# define FALSE 0
#endif

#ifndef TRUE
# define TRUE 1
#endif

#endif  /* MRUBYCONF_H */
