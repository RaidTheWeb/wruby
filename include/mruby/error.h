/*
** mruby/error.h - Exception class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_ERROR_H
#define MRUBY_ERROR_H

#include "common.h"

/**
 * MRuby error handling.
 */
MRB_BEGIN_DECL

struct RException {
  MRB_OBJECT_HEADER;
  struct iv_tbl *iv;
};

#define _exc_ptr(v) ((struct RException*)_ptr(v))

MRB_API void _sys_fail(state *mrb, const char *mesg);
MRB_API value _exc_new_str(state *mrb, struct RClass* c, value str);
#define _exc_new_str_lit(mrb, c, lit) _exc_new_str(mrb, c, _str_new_lit(mrb, lit))
MRB_API value _make_exception(state *mrb, _int argc, const value *argv);
MRB_API value _exc_backtrace(state *mrb, value exc);
MRB_API value _get_backtrace(state *mrb);
MRB_API _noreturn void _no_method_error(state *mrb, _sym id, value args, const char *fmt, ...);

/* declaration for fail method */
MRB_API value _f_raise(state*, value);

struct RBreak {
  MRB_OBJECT_HEADER;
  struct RProc *proc;
  value val;
};

/**
 * Protect
 *
 * @mrbgem mruby-error
 */
MRB_API value _protect(state *mrb, _func_t body, value data, _bool *state);

/**
 * Ensure
 *
 * @mrbgem mruby-error
 */
MRB_API value _ensure(state *mrb, _func_t body, value b_data,
                             _func_t ensure, value e_data);

/**
 * Rescue
 *
 * @mrbgem mruby-error
 */
MRB_API value _rescue(state *mrb, _func_t body, value b_data,
                             _func_t rescue, value r_data);

/**
 * Rescue exception
 *
 * @mrbgem mruby-error
 */
MRB_API value _rescue_exceptions(state *mrb, _func_t body, value b_data,
                                        _func_t rescue, value r_data,
                                        _int len, struct RClass **classes);

MRB_END_DECL

#endif  /* MRUBY_ERROR_H */
