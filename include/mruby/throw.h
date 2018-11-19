/*
** mruby/throw.h - mruby exception throwing handler
**
** See Copyright Notice in mruby.h
*/

#ifndef MRB_THROW_H
#define MRB_THROW_H

#if defined(MRB_ENABLE_CXX_ABI)
# if !defined(__cplusplus)
#  error Trying to use C++ exception handling in C code
# endif
#endif

#if defined(MRB_ENABLE_CXX_EXCEPTION) && defined(__cplusplus)

#define MRB_TRY(buf) do { try {
#define MRB_CATCH(buf) } catch(_jmpbuf_impl e) { if (e != (buf)->impl) { throw e; }
#define MRB_END_EXC(buf)  } } while(0)

#define MRB_THROW(buf) throw((buf)->impl)
typedef _int _jmpbuf_impl;

#else

#include <setjmp.h>

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define MRB_SETJMP _setjmp
#define MRB_LONGJMP _longjmp
#else
#define MRB_SETJMP setjmp
#define MRB_LONGJMP longjmp
#endif

#define MRB_TRY(buf) do { if (MRB_SETJMP((buf)->impl) == 0) {
#define MRB_CATCH(buf) } else {
#define MRB_END_EXC(buf) } } while(0)

#define MRB_THROW(buf) MRB_LONGJMP((buf)->impl, 1);
#define _jmpbuf_impl jmp_buf

#endif

struct _jmpbuf {
  _jmpbuf_impl impl;

#if defined(MRB_ENABLE_CXX_EXCEPTION) && defined(__cplusplus)
  static _int jmpbuf_id;
  _jmpbuf() : impl(jmpbuf_id++) {}
#endif
};

#endif  /* MRB_THROW_H */
