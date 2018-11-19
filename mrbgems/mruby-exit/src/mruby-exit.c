#include <stdlib.h>
#include <mruby.h>

static value
f_exit(state *mrb, value self)
{
  _int i = EXIT_SUCCESS;

  _get_args(mrb, "|i", &i);
  exit((int)i);
  /* not reached */
  return _nil_value();
}

void
_mruby_exit_gem_init(state* mrb)
{
  _define_method(mrb, mrb->kernel_module, "exit", f_exit, MRB_ARGS_OPT(1));
}

void
_mruby_exit_gem_final(state* mrb)
{
}
