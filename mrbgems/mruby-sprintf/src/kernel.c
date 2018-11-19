/*
** kernel.c - Kernel module suppliment
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>

$value $f_sprintf($state *mrb, $value obj); /* in sprintf.c */

void
$mruby_sprintf_gem_init($state* mrb)
{
  struct RClass *krn;

  if (mrb->kernel_module == NULL) {
    mrb->kernel_module = $define_module(mrb, "Kernel"); /* Might be PARANOID. */
  }
  krn = mrb->kernel_module;

  $define_method(mrb, krn, "sprintf", $f_sprintf, $ARGS_ANY());
  $define_method(mrb, krn, "format",  $f_sprintf, $ARGS_ANY());
}

void
$mruby_sprintf_gem_final($state* mrb)
{
  /* nothing to do. */
}

