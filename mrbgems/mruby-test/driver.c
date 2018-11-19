/*
** mrbtest - Test for Embeddable Ruby
**
** This program runs Ruby test programs in test/t directory
** against the current mruby implementation.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mruby.h>
#include <mruby/proc.h>
#include <mruby/data.h>
#include <mruby/compile.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <mruby/array.h>

void
_init_mrbtest(state *);

/* Print a short remark for the user */
static void
print_hint(void)
{
  printf("mrbtest - Embeddable Ruby Test\n\n");
}

static int
check_error(state *mrb)
{
  /* Error check */
  /* $ko_test and $kill_test should be 0 */
  value ko_test = _gv_get(mrb, _intern_lit(mrb, "$ko_test"));
  value kill_test = _gv_get(mrb, _intern_lit(mrb, "$kill_test"));

  return _fixnum_p(ko_test) && _fixnum(ko_test) == 0 && _fixnum_p(kill_test) && _fixnum(kill_test) == 0;
}

static int
eval_test(state *mrb)
{
  /* evaluate the test */
  _funcall(mrb, _top_self(mrb), "report", 0);
  /* did an exception occur? */
  if (mrb->exc) {
    _print_error(mrb);
    mrb->exc = 0;
    return EXIT_FAILURE;
  }
  else if (!check_error(mrb)) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

static void
t_printstr(state *mrb, value obj)
{
  char *s;
  _int len;

  if (_string_p(obj)) {
    s = RSTRING_PTR(obj);
    len = RSTRING_LEN(obj);
    fwrite(s, len, 1, stdout);
    fflush(stdout);
  }
}

value
_t_printstr(state *mrb, value self)
{
  value argv;

  _get_args(mrb, "o", &argv);
  t_printstr(mrb, argv);

  return argv;
}

void
_init_test_driver(state *mrb, _bool verbose)
{
  struct RClass *krn, *mrbtest;

  krn = mrb->kernel_module;
  _define_method(mrb, krn, "__t_printstr__", _t_printstr, MRB_ARGS_REQ(1));

  mrbtest = _define_module(mrb, "Mrbtest");

  _define_const(mrb, mrbtest, "FIXNUM_MAX", _fixnum_value(MRB_INT_MAX));
  _define_const(mrb, mrbtest, "FIXNUM_MIN", _fixnum_value(MRB_INT_MIN));
  _define_const(mrb, mrbtest, "FIXNUM_BIT", _fixnum_value(MRB_INT_BIT));

#ifndef MRB_WITHOUT_FLOAT
#ifdef MRB_USE_FLOAT
  _define_const(mrb, mrbtest, "FLOAT_TOLERANCE", _float_value(mrb, 1e-6));
#else
  _define_const(mrb, mrbtest, "FLOAT_TOLERANCE", _float_value(mrb, 1e-12));
#endif
#endif

  if (verbose) {
    _gv_set(mrb, _intern_lit(mrb, "$mrbtest_verbose"), _true_value());
  }
}

void
_t_pass_result(state *_dst, state *_src)
{
  value res_src;

  if (_src->exc) {
    _print_error(_src);
    exit(EXIT_FAILURE);
  }

#define TEST_COUNT_PASS(name)                                           \
  do {                                                                  \
    res_src = _gv_get(_src, _intern_lit(_src, "$" #name));  \
    if (_fixnum_p(res_src)) {                                        \
      value res_dst = _gv_get(_dst, _intern_lit(_dst, "$" #name)); \
      _gv_set(_dst, _intern_lit(_dst, "$" #name), _fixnum_value(_fixnum(res_dst) + _fixnum(res_src))); \
    }                                                                   \
  } while (FALSE)                                                       \

  TEST_COUNT_PASS(ok_test);
  TEST_COUNT_PASS(ko_test);
  TEST_COUNT_PASS(kill_test);

#undef TEST_COUNT_PASS

  res_src = _gv_get(_src, _intern_lit(_src, "$asserts"));

  if (_array_p(res_src)) {
    _int i;
    value res_dst = _gv_get(_dst, _intern_lit(_dst, "$asserts"));
    for (i = 0; i < RARRAY_LEN(res_src); ++i) {
      value val_src = RARRAY_PTR(res_src)[i];
      _ary_push(_dst, res_dst, _str_new(_dst, RSTRING_PTR(val_src), RSTRING_LEN(val_src)));
    }
  }
}

int
main(int argc, char **argv)
{
  state *mrb;
  int ret;
  _bool verbose = FALSE;

  print_hint();

  /* new interpreter instance */
  mrb = _open();
  if (mrb == NULL) {
    fprintf(stderr, "Invalid state, exiting test driver");
    return EXIT_FAILURE;
  }

  if (argc == 2 && argv[1][0] == '-' && argv[1][1] == 'v') {
    printf("verbose mode: enable\n\n");
    verbose = TRUE;
  }

  _init_test_driver(mrb, verbose);
  _init_mrbtest(mrb);
  ret = eval_test(mrb);
  _close(mrb);

  return ret;
}
