#include <stdlib.h>
#include <mruby.h>
#include <mruby/irep.h>
#include <mruby/variable.h>

extern const uint8_t mrbtest_assert_irep[];

void mrbgemtest_init($state* mrb);
void $init_test_driver($state* mrb, $bool verbose);
void $t_pass_result($state *$dst, $state *$src);

void
$init_mrbtest($state *mrb)
{
  $state *core_test;

  $load_irep(mrb, mrbtest_assert_irep);

  core_test = $open_core($default_allocf, NULL);
  if (core_test == NULL) {
    fprintf(stderr, "Invalid $state, exiting %s", __FUNCTION__);
    exit(EXIT_FAILURE);
  }
  $init_test_driver(core_test, $test($gv_get(mrb, $intern_lit(mrb, "$mrbtest_verbose"))));
  $load_irep(core_test, mrbtest_assert_irep);
  $t_pass_result(mrb, core_test);

#ifndef DISABLE_GEMS
  mrbgemtest_init(mrb);
#endif

  if (mrb->exc) {
    $print_error(mrb);
    $close(mrb);
    exit(EXIT_FAILURE);
  }
  $close(core_test);
}

