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
$init_mrbtest($state *);

/* Print a short remark for the user */
static void
print_hint(void)
{
  printf("mrbtest - Embeddable Ruby Test\n\n");
}

static int
check_error($state *mrb)
{
  /* Error check */
  /* $ko_test and $kill_test should be 0 */
  $value ko_test = $gv_get(mrb, $intern_lit(mrb, "$ko_test"));
  $value kill_test = $gv_get(mrb, $intern_lit(mrb, "$kill_test"));

  return $fixnum_p(ko_test) && $fixnum(ko_test) == 0 && $fixnum_p(kill_test) && $fixnum(kill_test) == 0;
}

static int
eval_test($state *mrb)
{
  /* evaluate the test */
  $funcall(mrb, $top_self(mrb), "report", 0);
  /* did an exception occur? */
  if (mrb->exc) {
    $print_error(mrb);
    mrb->exc = 0;
    return EXIT_FAILURE;
  }
  else if (!check_error(mrb)) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

static void
t_printstr($state *mrb, $value obj)
{
  char *s;
  $int len;

  if ($string_p(obj)) {
    s = RSTRING_PTR(obj);
    len = RSTRING_LEN(obj);
    fwrite(s, len, 1, stdout);
    fflush(stdout);
  }
}

$value
$t_printstr($state *mrb, $value self)
{
  $value argv;

  $get_args(mrb, "o", &argv);
  t_printstr(mrb, argv);

  return argv;
}

void
$init_test_driver($state *mrb, $bool verbose)
{
  struct RClass *krn, *mrbtest;

  krn = mrb->kernel_module;
  $define_method(mrb, krn, "__t_printstr__", $t_printstr, $ARGS_REQ(1));

  mrbtest = $define_module(mrb, "Mrbtest");

  $define_const(mrb, mrbtest, "FIXNUM_MAX", $fixnum_value($INT_MAX));
  $define_const(mrb, mrbtest, "FIXNUM_MIN", $fixnum_value($INT_MIN));
  $define_const(mrb, mrbtest, "FIXNUM_BIT", $fixnum_value($INT_BIT));

#ifndef $WITHOUT_FLOAT
#ifdef $USE_FLOAT
  $define_const(mrb, mrbtest, "FLOAT_TOLERANCE", $float_value(mrb, 1e-6));
#else
  $define_const(mrb, mrbtest, "FLOAT_TOLERANCE", $float_value(mrb, 1e-12));
#endif
#endif

  if (verbose) {
    $gv_set(mrb, $intern_lit(mrb, "$mrbtest_verbose"), $true_value());
  }
}

void
$t_pass_result($state *$dst, $state *$src)
{
  $value res_src;

  if ($src->exc) {
    $print_error($src);
    exit(EXIT_FAILURE);
  }

#define TEST_COUNT_PASS(name)                                           \
  do {                                                                  \
    res_src = $gv_get($src, $intern_lit($src, "$" #name));  \
    if ($fixnum_p(res_src)) {                                        \
      $value res_dst = $gv_get($dst, $intern_lit($dst, "$" #name)); \
      $gv_set($dst, $intern_lit($dst, "$" #name), $fixnum_value($fixnum(res_dst) + $fixnum(res_src))); \
    }                                                                   \
  } while (FALSE)                                                       \

  TEST_COUNT_PASS(ok_test);
  TEST_COUNT_PASS(ko_test);
  TEST_COUNT_PASS(kill_test);

#undef TEST_COUNT_PASS

  res_src = $gv_get($src, $intern_lit($src, "$asserts"));

  if ($array_p(res_src)) {
    $int i;
    $value res_dst = $gv_get($dst, $intern_lit($dst, "$asserts"));
    for (i = 0; i < RARRAY_LEN(res_src); ++i) {
      $value val_src = RARRAY_PTR(res_src)[i];
      $ary_push($dst, res_dst, $str_new($dst, RSTRING_PTR(val_src), RSTRING_LEN(val_src)));
    }
  }
}

int
main(int argc, char **argv)
{
  $state *mrb;
  int ret;
  $bool verbose = FALSE;

  print_hint();

  /* new interpreter instance */
  mrb = $open();
  if (mrb == NULL) {
    fprintf(stderr, "Invalid $state, exiting test driver");
    return EXIT_FAILURE;
  }

  if (argc == 2 && argv[1][0] == '-' && argv[1][1] == 'v') {
    printf("verbose mode: enable\n\n");
    verbose = TRUE;
  }

  $init_test_driver(mrb, verbose);
  $init_mrbtest(mrb);
  ret = eval_test(mrb);
  $close(mrb);

  return ret;
}
