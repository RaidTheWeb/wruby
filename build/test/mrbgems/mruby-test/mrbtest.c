/*
 * This file contains a list of all
 * test functions.
 *
 * IMPORTANT:
 *   This file was generated!
 *   All manual changes will get lost.
 */

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

void GENERATED_TMP__mruby_metaprog_gem_test(_state *mrb);
void GENERATED_TMP__mruby_time_gem_test(_state *mrb);
void GENERATED_TMP__mruby_io_gem_test(_state *mrb);
void GENERATED_TMP__mruby_pack_gem_test(_state *mrb);
void GENERATED_TMP__mruby_sprintf_gem_test(_state *mrb);
void GENERATED_TMP__mruby_print_gem_test(_state *mrb);
void GENERATED_TMP__mruby_math_gem_test(_state *mrb);
void GENERATED_TMP__mruby_struct_gem_test(_state *mrb);
void GENERATED_TMP__mruby_compar_ext_gem_test(_state *mrb);
void GENERATED_TMP__mruby_enum_ext_gem_test(_state *mrb);
void GENERATED_TMP__mruby_fiber_gem_test(_state *mrb);
void GENERATED_TMP__mruby_enumerator_gem_test(_state *mrb);
void GENERATED_TMP__mruby_string_ext_gem_test(_state *mrb);
void GENERATED_TMP__mruby_numeric_ext_gem_test(_state *mrb);
void GENERATED_TMP__mruby_array_ext_gem_test(_state *mrb);
void GENERATED_TMP__mruby_hash_ext_gem_test(_state *mrb);
void GENERATED_TMP__mruby_range_ext_gem_test(_state *mrb);
void GENERATED_TMP__mruby_proc_ext_gem_test(_state *mrb);
void GENERATED_TMP__mruby_symbol_ext_gem_test(_state *mrb);
void GENERATED_TMP__mruby_random_gem_test(_state *mrb);
void GENERATED_TMP__mruby_object_ext_gem_test(_state *mrb);
void GENERATED_TMP__mruby_objectspace_gem_test(_state *mrb);
void GENERATED_TMP__mruby_enum_lazy_gem_test(_state *mrb);
void GENERATED_TMP__mruby_toplevel_ext_gem_test(_state *mrb);
void GENERATED_TMP__mruby_compiler_gem_test(_state *mrb);
void GENERATED_TMP__mruby_bin_mirb_gem_test(_state *mrb);
void GENERATED_TMP__mruby_error_gem_test(_state *mrb);
void GENERATED_TMP__mruby_bin_mruby_gem_test(_state *mrb);
void GENERATED_TMP__mruby_bin_strip_gem_test(_state *mrb);
void GENERATED_TMP__mruby_kernel_ext_gem_test(_state *mrb);
void GENERATED_TMP__mruby_class_ext_gem_test(_state *mrb);
void GENERATED_TMP__mruby_test_gem_test(_state *mrb);
void mrbgemtest_init(_state* mrb) {
    GENERATED_TMP__mruby_metaprog_gem_test(mrb);
    GENERATED_TMP__mruby_time_gem_test(mrb);
    GENERATED_TMP__mruby_io_gem_test(mrb);
    GENERATED_TMP__mruby_pack_gem_test(mrb);
    GENERATED_TMP__mruby_sprintf_gem_test(mrb);
    GENERATED_TMP__mruby_print_gem_test(mrb);
    GENERATED_TMP__mruby_math_gem_test(mrb);
    GENERATED_TMP__mruby_struct_gem_test(mrb);
    GENERATED_TMP__mruby_compar_ext_gem_test(mrb);
    GENERATED_TMP__mruby_enum_ext_gem_test(mrb);
    GENERATED_TMP__mruby_fiber_gem_test(mrb);
    GENERATED_TMP__mruby_enumerator_gem_test(mrb);
    GENERATED_TMP__mruby_string_ext_gem_test(mrb);
    GENERATED_TMP__mruby_numeric_ext_gem_test(mrb);
    GENERATED_TMP__mruby_array_ext_gem_test(mrb);
    GENERATED_TMP__mruby_hash_ext_gem_test(mrb);
    GENERATED_TMP__mruby_range_ext_gem_test(mrb);
    GENERATED_TMP__mruby_proc_ext_gem_test(mrb);
    GENERATED_TMP__mruby_symbol_ext_gem_test(mrb);
    GENERATED_TMP__mruby_random_gem_test(mrb);
    GENERATED_TMP__mruby_object_ext_gem_test(mrb);
    GENERATED_TMP__mruby_objectspace_gem_test(mrb);
    GENERATED_TMP__mruby_enum_lazy_gem_test(mrb);
    GENERATED_TMP__mruby_toplevel_ext_gem_test(mrb);
    GENERATED_TMP__mruby_compiler_gem_test(mrb);
    GENERATED_TMP__mruby_bin_mirb_gem_test(mrb);
    GENERATED_TMP__mruby_error_gem_test(mrb);
    GENERATED_TMP__mruby_bin_mruby_gem_test(mrb);
    GENERATED_TMP__mruby_bin_strip_gem_test(mrb);
    GENERATED_TMP__mruby_kernel_ext_gem_test(mrb);
    GENERATED_TMP__mruby_class_ext_gem_test(mrb);
    GENERATED_TMP__mruby_test_gem_test(mrb);
}
