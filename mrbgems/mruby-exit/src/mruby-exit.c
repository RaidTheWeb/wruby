#include <stdlib.h>
#include <mruby.h>

static value
f_exit(state *mrb, value self)
{
  int i = EXIT_SUCCESS;

  get_args(mrb, "|i", &i);
  exit((int)i);
  /* not reached */
  return nil_value();
}

void
mruby_exit_gem_init(state* mrb)
{
  define_method(mrb, mrb->kernel_module, "exit", f_exit, ARGS_OPT(1));
}

void
mruby_exit_gem_final(state* mrb)
{
}
