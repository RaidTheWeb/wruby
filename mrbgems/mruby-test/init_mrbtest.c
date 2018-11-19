#include <stdlib.h>
#include <mruby.h>
#include <mruby/irep.h>
#include <mruby/variable.h>

extern const uint8_t mrbtest_assert_irep[];

void mrbgemtest_init(_state* mrb);
void _init_test_driver(_state* mrb, _bool verbose);
void _t_pass_result(_state *_dst, _state *_src);

void
_init_mrbtest(_state *mrb)
{
  _state *core_test;

  _load_irep(mrb, mrbtest_assert_irep);

  core_test = _open_core(_default_allocf, NULL);
  if (core_test == NULL) {
    fprintf(stderr, "Invalid _state, exiting %s", __FUNCTION__);
    exit(EXIT_FAILURE);
  }
  _init_test_driver(core_test, _test(_gv_get(mrb, _intern_lit(mrb, "$mrbtest_verbose"))));
  _load_irep(core_test, mrbtest_assert_irep);
  _t_pass_result(mrb, core_test);

#ifndef DISABLE_GEMS
  mrbgemtest_init(mrb);
#endif

  if (mrb->exc) {
    _print_error(mrb);
    _close(mrb);
    exit(EXIT_FAILURE);
  }
  _close(core_test);
}

