/*
** kernel.c - Kernel module suppliment
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>

_value _f_sprintf(_state *mrb, _value obj); /* in sprintf.c */

void
_mruby_sprintf_gem_init(_state* mrb)
{
  struct RClass *krn;

  if (mrb->kernel_module == NULL) {
    mrb->kernel_module = _define_module(mrb, "Kernel"); /* Might be PARANOID. */
  }
  krn = mrb->kernel_module;

  _define_method(mrb, krn, "sprintf", _f_sprintf, MRB_ARGS_ANY());
  _define_method(mrb, krn, "format",  _f_sprintf, MRB_ARGS_ANY());
}

void
_mruby_sprintf_gem_final(_state* mrb)
{
  /* nothing to do. */
}

