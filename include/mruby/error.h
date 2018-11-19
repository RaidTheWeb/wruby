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

MRB_API void _sys_fail(_state *mrb, const char *mesg);
MRB_API _value _exc_new_str(_state *mrb, struct RClass* c, _value str);
#define _exc_new_str_lit(mrb, c, lit) _exc_new_str(mrb, c, _str_new_lit(mrb, lit))
MRB_API _value _make_exception(_state *mrb, _int argc, const _value *argv);
MRB_API _value _exc_backtrace(_state *mrb, _value exc);
MRB_API _value _get_backtrace(_state *mrb);
MRB_API _noreturn void _no_method_error(_state *mrb, _sym id, _value args, const char *fmt, ...);

/* declaration for fail method */
MRB_API _value _f_raise(_state*, _value);

struct RBreak {
  MRB_OBJECT_HEADER;
  struct RProc *proc;
  _value val;
};

/**
 * Protect
 *
 * @mrbgem mruby-error
 */
MRB_API _value _protect(_state *mrb, _func_t body, _value data, _bool *state);

/**
 * Ensure
 *
 * @mrbgem mruby-error
 */
MRB_API _value _ensure(_state *mrb, _func_t body, _value b_data,
                             _func_t ensure, _value e_data);

/**
 * Rescue
 *
 * @mrbgem mruby-error
 */
MRB_API _value _rescue(_state *mrb, _func_t body, _value b_data,
                             _func_t rescue, _value r_data);

/**
 * Rescue exception
 *
 * @mrbgem mruby-error
 */
MRB_API _value _rescue_exceptions(_state *mrb, _func_t body, _value b_data,
                                        _func_t rescue, _value r_data,
                                        _int len, struct RClass **classes);

MRB_END_DECL

#endif  /* MRUBY_ERROR_H */
