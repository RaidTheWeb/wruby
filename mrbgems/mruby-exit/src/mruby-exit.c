#include <stdlib.h>
#include <mruby.h>

static _value
f_exit(_state *mrb, _value self)
{
  _int i = EXIT_SUCCESS;

  _get_args(mrb, "|i", &i);
  exit((int)i);
  /* not reached */
  return _nil_value();
}

void
_mruby_exit_gem_init(_state* mrb)
{
  _define_method(mrb, mrb->kernel_module, "exit", f_exit, MRB_ARGS_OPT(1));
}

void
_mruby_exit_gem_final(_state* mrb)
{
}
