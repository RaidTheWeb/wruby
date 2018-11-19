/*
 * This file contains a list of all
 * initializing methods which are
 * necessary to bootstrap all gems.
 *
 * IMPORTANT:
 *   This file was generated!
 *   All manual changes will get lost.
 */

#include <mruby.h>

void GENERATED_TMP__mruby_metaprog_gem_init(_state*);
void GENERATED_TMP__mruby_metaprog_gem_final(_state*);
void GENERATED_TMP__mruby_time_gem_init(_state*);
void GENERATED_TMP__mruby_time_gem_final(_state*);
void GENERATED_TMP__mruby_io_gem_init(_state*);
void GENERATED_TMP__mruby_io_gem_final(_state*);
void GENERATED_TMP__mruby_pack_gem_init(_state*);
void GENERATED_TMP__mruby_pack_gem_final(_state*);
void GENERATED_TMP__mruby_sprintf_gem_init(_state*);
void GENERATED_TMP__mruby_sprintf_gem_final(_state*);
void GENERATED_TMP__mruby_print_gem_init(_state*);
void GENERATED_TMP__mruby_print_gem_final(_state*);
void GENERATED_TMP__mruby_math_gem_init(_state*);
void GENERATED_TMP__mruby_math_gem_final(_state*);
void GENERATED_TMP__mruby_struct_gem_init(_state*);
void GENERATED_TMP__mruby_struct_gem_final(_state*);
void GENERATED_TMP__mruby_compar_ext_gem_init(_state*);
void GENERATED_TMP__mruby_compar_ext_gem_final(_state*);
void GENERATED_TMP__mruby_enum_ext_gem_init(_state*);
void GENERATED_TMP__mruby_enum_ext_gem_final(_state*);
void GENERATED_TMP__mruby_fiber_gem_init(_state*);
void GENERATED_TMP__mruby_fiber_gem_final(_state*);
void GENERATED_TMP__mruby_enumerator_gem_init(_state*);
void GENERATED_TMP__mruby_enumerator_gem_final(_state*);
void GENERATED_TMP__mruby_string_ext_gem_init(_state*);
void GENERATED_TMP__mruby_string_ext_gem_final(_state*);
void GENERATED_TMP__mruby_numeric_ext_gem_init(_state*);
void GENERATED_TMP__mruby_numeric_ext_gem_final(_state*);
void GENERATED_TMP__mruby_array_ext_gem_init(_state*);
void GENERATED_TMP__mruby_array_ext_gem_final(_state*);
void GENERATED_TMP__mruby_hash_ext_gem_init(_state*);
void GENERATED_TMP__mruby_hash_ext_gem_final(_state*);
void GENERATED_TMP__mruby_range_ext_gem_init(_state*);
void GENERATED_TMP__mruby_range_ext_gem_final(_state*);
void GENERATED_TMP__mruby_proc_ext_gem_init(_state*);
void GENERATED_TMP__mruby_proc_ext_gem_final(_state*);
void GENERATED_TMP__mruby_symbol_ext_gem_init(_state*);
void GENERATED_TMP__mruby_symbol_ext_gem_final(_state*);
void GENERATED_TMP__mruby_random_gem_init(_state*);
void GENERATED_TMP__mruby_random_gem_final(_state*);
void GENERATED_TMP__mruby_object_ext_gem_init(_state*);
void GENERATED_TMP__mruby_object_ext_gem_final(_state*);
void GENERATED_TMP__mruby_objectspace_gem_init(_state*);
void GENERATED_TMP__mruby_objectspace_gem_final(_state*);
void GENERATED_TMP__mruby_enum_lazy_gem_init(_state*);
void GENERATED_TMP__mruby_enum_lazy_gem_final(_state*);
void GENERATED_TMP__mruby_toplevel_ext_gem_init(_state*);
void GENERATED_TMP__mruby_toplevel_ext_gem_final(_state*);
void GENERATED_TMP__mruby_error_gem_init(_state*);
void GENERATED_TMP__mruby_error_gem_final(_state*);
void GENERATED_TMP__mruby_kernel_ext_gem_init(_state*);
void GENERATED_TMP__mruby_kernel_ext_gem_final(_state*);
void GENERATED_TMP__mruby_class_ext_gem_init(_state*);
void GENERATED_TMP__mruby_class_ext_gem_final(_state*);

static void
_final_mrbgems(_state *mrb) {
  GENERATED_TMP__mruby_class_ext_gem_final(mrb);
  GENERATED_TMP__mruby_kernel_ext_gem_final(mrb);
  GENERATED_TMP__mruby_error_gem_final(mrb);
  GENERATED_TMP__mruby_toplevel_ext_gem_final(mrb);
  GENERATED_TMP__mruby_enum_lazy_gem_final(mrb);
  GENERATED_TMP__mruby_objectspace_gem_final(mrb);
  GENERATED_TMP__mruby_object_ext_gem_final(mrb);
  GENERATED_TMP__mruby_random_gem_final(mrb);
  GENERATED_TMP__mruby_symbol_ext_gem_final(mrb);
  GENERATED_TMP__mruby_proc_ext_gem_final(mrb);
  GENERATED_TMP__mruby_range_ext_gem_final(mrb);
  GENERATED_TMP__mruby_hash_ext_gem_final(mrb);
  GENERATED_TMP__mruby_array_ext_gem_final(mrb);
  GENERATED_TMP__mruby_numeric_ext_gem_final(mrb);
  GENERATED_TMP__mruby_string_ext_gem_final(mrb);
  GENERATED_TMP__mruby_enumerator_gem_final(mrb);
  GENERATED_TMP__mruby_fiber_gem_final(mrb);
  GENERATED_TMP__mruby_enum_ext_gem_final(mrb);
  GENERATED_TMP__mruby_compar_ext_gem_final(mrb);
  GENERATED_TMP__mruby_struct_gem_final(mrb);
  GENERATED_TMP__mruby_math_gem_final(mrb);
  GENERATED_TMP__mruby_print_gem_final(mrb);
  GENERATED_TMP__mruby_sprintf_gem_final(mrb);
  GENERATED_TMP__mruby_pack_gem_final(mrb);
  GENERATED_TMP__mruby_io_gem_final(mrb);
  GENERATED_TMP__mruby_time_gem_final(mrb);
  GENERATED_TMP__mruby_metaprog_gem_final(mrb);
}

void
_init_mrbgems(_state *mrb) {
  GENERATED_TMP__mruby_metaprog_gem_init(mrb);
  GENERATED_TMP__mruby_time_gem_init(mrb);
  GENERATED_TMP__mruby_io_gem_init(mrb);
  GENERATED_TMP__mruby_pack_gem_init(mrb);
  GENERATED_TMP__mruby_sprintf_gem_init(mrb);
  GENERATED_TMP__mruby_print_gem_init(mrb);
  GENERATED_TMP__mruby_math_gem_init(mrb);
  GENERATED_TMP__mruby_struct_gem_init(mrb);
  GENERATED_TMP__mruby_compar_ext_gem_init(mrb);
  GENERATED_TMP__mruby_enum_ext_gem_init(mrb);
  GENERATED_TMP__mruby_fiber_gem_init(mrb);
  GENERATED_TMP__mruby_enumerator_gem_init(mrb);
  GENERATED_TMP__mruby_string_ext_gem_init(mrb);
  GENERATED_TMP__mruby_numeric_ext_gem_init(mrb);
  GENERATED_TMP__mruby_array_ext_gem_init(mrb);
  GENERATED_TMP__mruby_hash_ext_gem_init(mrb);
  GENERATED_TMP__mruby_range_ext_gem_init(mrb);
  GENERATED_TMP__mruby_proc_ext_gem_init(mrb);
  GENERATED_TMP__mruby_symbol_ext_gem_init(mrb);
  GENERATED_TMP__mruby_random_gem_init(mrb);
  GENERATED_TMP__mruby_object_ext_gem_init(mrb);
  GENERATED_TMP__mruby_objectspace_gem_init(mrb);
  GENERATED_TMP__mruby_enum_lazy_gem_init(mrb);
  GENERATED_TMP__mruby_toplevel_ext_gem_init(mrb);
  GENERATED_TMP__mruby_error_gem_init(mrb);
  GENERATED_TMP__mruby_kernel_ext_gem_init(mrb);
  GENERATED_TMP__mruby_class_ext_gem_init(mrb);
  _state_atexit(mrb, _final_mrbgems);
}
