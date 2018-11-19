/*
** mruby/throw.h - mruby exception throwing handler
**
** See Copyright Notice in mruby.h
*/

#ifndef $THROW_H
#define $THROW_H

#if defined($ENABLE_CXX_ABI)
# if !defined(__cplusplus)
#  error Trying to use C++ exception handling in C code
# endif
#endif

#if defined($ENABLE_CXX_EXCEPTION) && defined(__cplusplus)

#define $TRY(buf) do { try {
#define $CATCH(buf) } catch($jmpbuf_impl e) { if (e != (buf)->impl) { throw e; }
#define $END_EXC(buf)  } } while(0)

#define $THROW(buf) throw((buf)->impl)
typedef $int $jmpbuf_impl;

#else

#include <setjmp.h>

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define $SETJMP _setjmp
#define $LONGJMP _longjmp
#else
#define $SETJMP setjmp
#define $LONGJMP longjmp
#endif

#define $TRY(buf) do { if ($SETJMP((buf)->impl) == 0) {
#define $CATCH(buf) } else {
#define $END_EXC(buf) } } while(0)

#define $THROW(buf) $LONGJMP((buf)->impl, 1);
#define $jmpbuf_impl jmp_buf

#endif

struct $jmpbuf {
  $jmpbuf_impl impl;

#if defined($ENABLE_CXX_EXCEPTION) && defined(__cplusplus)
  static $int jmpbuf_id;
  $jmpbuf() : impl(jmpbuf_id++) {}
#endif
};

#endif  /* $THROW_H */
