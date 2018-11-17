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
BEGIN_DECL

struct RException {
  OBJECT_HEADER;
  struct iv_tbl *iv;
};

#define exc_ptr(v) ((struct RException*)ptr(v))

API void sys_fail(state *mrb, const char *mesg);
API value exc_new_str(state *mrb, struct RClass* c, value str);
#define exc_new_str_lit(mrb, c, lit) exc_new_str(mrb, c, str_new_lit(mrb, lit))
API value make_exception(state *mrb, int argc, const value *argv);
API value exc_backtrace(state *mrb, value exc);
API value get_backtrace(state *mrb);
API noreturn void no_method_error(state *mrb, sym id, value args, const char *fmt, ...);

/* declaration for fail method */
API value f_raise(state*, value);

struct RBreak {
  OBJECT_HEADER;
  struct RProc *proc;
  value val;
};

/**
 * Protect
 *
 * @mrbgem mruby-error
 */
API value protect(state *mrb, func_t body, value data, bool *state);

/**
 * Ensure
 *
 * @mrbgem mruby-error
 */
API value ensure(state *mrb, func_t body, value b_data,
                             func_t ensure, value e_data);

/**
 * Rescue
 *
 * @mrbgem mruby-error
 */
API value rescue(state *mrb, func_t body, value b_data,
                             func_t rescue, value r_data);

/**
 * Rescue exception
 *
 * @mrbgem mruby-error
 */
API value rescue_exceptions(state *mrb, func_t body, value b_data,
                                        func_t rescue, value r_data,
                                        int len, struct RClass **classes);

END_DECL

#endif  /* MRUBY_ERROR_H */
