/*
 * This file is loading the irep
 * Ruby GEM code.
 *
 * IMPORTANT:
 *   This file was generated!
 *   All manual changes will get lost.
 */
#include <stdio.h>
#include <stdlib.h>
#include <mruby.h>
#include <mruby/irep.h>
#include <mruby/variable.h>
/*
 * This file contains a test code for mruby-numeric-ext gem.
 *
 * IMPORTANT:
 *   This file was generated!
 *   All manual changes will get lost.
 */
extern const uint8_t mrbtest_assert_irep[];
/* dumped in little endian order.
   use `mrbc -E` option for big endian CPU. */
#include <stdint.h>
extern const uint8_t gem_test_irep_mruby_numeric_ext_0[];
const uint8_t
#if defined __GNUC__
__attribute__((aligned(4)))
#elif defined _MSC_VER
__declspec(align(4))
#endif
gem_test_irep_mruby_numeric_ext_0[] = {
0x45,0x54,0x49,0x52,0x30,0x30,0x30,0x35,0x96,0x06,0x00,0x00,0x05,0x77,0x4d,0x41,
0x54,0x5a,0x30,0x30,0x30,0x30,0x49,0x52,0x45,0x50,0x00,0x00,0x02,0xc0,0x30,0x30,
0x30,0x32,0x00,0x00,0x01,0xb7,0x00,0x01,0x00,0x04,0x00,0x05,0x00,0x00,0x00,0x4c,
0x10,0x01,0x4f,0x02,0x00,0x55,0x03,0x00,0x2f,0x01,0x00,0x01,0x10,0x01,0x4f,0x02,
0x01,0x55,0x03,0x01,0x2f,0x01,0x00,0x01,0x10,0x01,0x4f,0x02,0x02,0x2e,0x01,0x01,
0x01,0x23,0x01,0x00,0x31,0x10,0x01,0x4f,0x02,0x03,0x55,0x03,0x02,0x2f,0x01,0x00,
0x01,0x10,0x01,0x4f,0x02,0x04,0x55,0x03,0x03,0x2f,0x01,0x00,0x01,0x10,0x01,0x4f,
0x02,0x05,0x55,0x03,0x04,0x2f,0x01,0x00,0x01,0x37,0x01,0x67,0x00,0x00,0x00,0x06,
0x00,0x00,0x0b,0x49,0x6e,0x74,0x65,0x67,0x65,0x72,0x23,0x63,0x68,0x72,0x00,0x00,
0x0b,0x49,0x6e,0x74,0x65,0x67,0x65,0x72,0x23,0x64,0x69,0x76,0x00,0x00,0x05,0x46,
0x6c,0x6f,0x61,0x74,0x00,0x00,0x09,0x46,0x6c,0x6f,0x61,0x74,0x23,0x64,0x69,0x76,
0x00,0x00,0x0d,0x49,0x6e,0x74,0x65,0x67,0x65,0x72,0x23,0x7a,0x65,0x72,0x6f,0x3f,
0x00,0x00,0x10,0x49,0x6e,0x74,0x65,0x67,0x65,0x72,0x23,0x6e,0x6f,0x6e,0x7a,0x65,
0x72,0x6f,0x3f,0x00,0x00,0x00,0x02,0x00,0x06,0x61,0x73,0x73,0x65,0x72,0x74,0x00,
0x00,0x0e,0x63,0x6c,0x61,0x73,0x73,0x5f,0x64,0x65,0x66,0x69,0x6e,0x65,0x64,0x3f,
0x00,0x00,0x00,0x01,0x0b,0x00,0x01,0x00,0x05,0x00,0x01,0x00,0x00,0x00,0x2e,0x00,
0x10,0x01,0x4f,0x02,0x00,0x03,0x03,0x41,0x2e,0x03,0x00,0x00,0x2e,0x01,0x01,0x02,
0x10,0x01,0x4f,0x02,0x01,0x03,0x03,0x42,0x2e,0x03,0x00,0x00,0x2e,0x01,0x01,0x02,
0x10,0x01,0x1b,0x02,0x02,0x55,0x03,0x00,0x2f,0x01,0x03,0x01,0x37,0x01,0x00,0x00,
0x00,0x02,0x00,0x00,0x01,0x41,0x00,0x00,0x01,0x42,0x00,0x00,0x00,0x04,0x00,0x03,
0x63,0x68,0x72,0x00,0x00,0x0c,0x61,0x73,0x73,0x65,0x72,0x74,0x5f,0x65,0x71,0x75,
0x61,0x6c,0x00,0x00,0x0a,0x52,0x61,0x6e,0x67,0x65,0x45,0x72,0x72,0x6f,0x72,0x00,
0x00,0x0c,0x61,0x73,0x73,0x65,0x72,0x74,0x5f,0x72,0x61,0x69,0x73,0x65,0x00,0x00,
0x00,0x00,0x4c,0x00,0x01,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,
0x65,0x03,0x01,0x01,0x00,0x2e,0x01,0x00,0x00,0x37,0x01,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x01,0x00,0x03,0x63,0x68,0x72,0x00,0x00,0x00,0x00,0x87,0x00,0x01,0x00,
0x06,0x00,0x00,0x00,0x00,0x00,0x16,0x00,0x10,0x01,0x03,0x02,0x34,0x65,0x03,0x03,
0x01,0x6d,0x0d,0x04,0x2e,0x03,0x00,0x01,0x2e,0x01,0x01,0x02,0x37,0x01,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x03,0x64,0x69,0x76,0x00,0x00,0x0c,0x61,0x73,
0x73,0x65,0x72,0x74,0x5f,0x65,0x71,0x75,0x61,0x6c,0x00,0x00,0x00,0x00,0x98,0x00,
0x01,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x14,0x00,0x00,0x00,0x10,0x01,0x03,0x02,
0x34,0x02,0x03,0x00,0x0d,0x04,0x2e,0x03,0x00,0x01,0x2e,0x01,0x01,0x02,0x37,0x01,
0x00,0x00,0x00,0x01,0x02,0x00,0x16,0x33,0x2e,0x36,0x35,0x32,0x34,0x32,0x35,0x30,
0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x31,0x65,0x2b,0x30,0x32,0x00,0x00,0x00,
0x02,0x00,0x03,0x64,0x69,0x76,0x00,0x00,0x0c,0x61,0x73,0x73,0x65,0x72,0x74,0x5f,
0x66,0x6c,0x6f,0x61,0x74,0x00,0x00,0x00,0x00,0xa9,0x00,0x01,0x00,0x05,0x00,0x00,
0x00,0x00,0x00,0x1e,0x10,0x01,0x11,0x02,0x06,0x03,0x2e,0x03,0x00,0x00,0x2e,0x01,
0x01,0x02,0x10,0x01,0x12,0x02,0x07,0x03,0x2e,0x03,0x00,0x00,0x2e,0x01,0x01,0x02,
0x37,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x05,0x7a,0x65,0x72,0x6f,
0x3f,0x00,0x00,0x0c,0x61,0x73,0x73,0x65,0x72,0x74,0x5f,0x65,0x71,0x75,0x61,0x6c,
0x00,0x00,0x00,0x00,0xc4,0x00,0x01,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x24,0x00,
0x10,0x01,0x0f,0x02,0x06,0x03,0x2e,0x03,0x00,0x00,0x2e,0x01,0x01,0x02,0x10,0x01,
0x65,0x03,0x02,0x03,0xe8,0x65,0x03,0x03,0x03,0xe8,0x2e,0x03,0x00,0x00,0x2e,0x01,
0x01,0x02,0x37,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x08,0x6e,0x6f,
0x6e,0x7a,0x65,0x72,0x6f,0x3f,0x00,0x00,0x0c,0x61,0x73,0x73,0x65,0x72,0x74,0x5f,
0x65,0x71,0x75,0x61,0x6c,0x00,0x44,0x42,0x47,0x00,0x00,0x00,0x02,0x99,0x00,0x01,
0x00,0x34,0x2f,0x6f,0x70,0x74,0x2f,0x6d,0x72,0x75,0x62,0x79,0x2f,0x6d,0x72,0x62,
0x67,0x65,0x6d,0x73,0x2f,0x6d,0x72,0x75,0x62,0x79,0x2d,0x6e,0x75,0x6d,0x65,0x72,
0x69,0x63,0x2d,0x65,0x78,0x74,0x2f,0x74,0x65,0x73,0x74,0x2f,0x6e,0x75,0x6d,0x65,
0x72,0x69,0x63,0x2e,0x72,0x62,0x00,0x00,0x00,0xa9,0x00,0x01,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x4c,0x00,0x00,0x04,0x00,0x04,0x00,0x04,0x00,0x04,0x00,
0x04,0x00,0x04,0x00,0x04,0x00,0x04,0x00,0x04,0x00,0x04,0x00,0x04,0x00,0x04,0x00,
0x0c,0x00,0x0c,0x00,0x0c,0x00,0x0c,0x00,0x0c,0x00,0x0c,0x00,0x0c,0x00,0x0c,0x00,
0x0c,0x00,0x0c,0x00,0x0c,0x00,0x0c,0x00,0x12,0x00,0x12,0x00,0x12,0x00,0x12,0x00,
0x12,0x00,0x12,0x00,0x12,0x00,0x12,0x00,0x12,0x00,0x12,0x00,0x12,0x00,0x10,0x00,
0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,
0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x14,0x00,0x14,0x00,0x14,0x00,
0x14,0x00,0x14,0x00,0x14,0x00,0x14,0x00,0x14,0x00,0x14,0x00,0x14,0x00,0x14,0x00,
0x14,0x00,0x19,0x00,0x19,0x00,0x19,0x00,0x19,0x00,0x19,0x00,0x19,0x00,0x19,0x00,
0x19,0x00,0x19,0x00,0x19,0x00,0x19,0x00,0x19,0x00,0x19,0x00,0x19,0x00,0x19,0x00,
0x00,0x00,0x6d,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2e,0x00,
0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,
0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,
0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,
0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,
0x00,0x09,0x00,0x09,0x00,0x09,0x00,0x09,0x00,0x09,0x00,0x09,0x00,0x09,0x00,0x09,
0x00,0x09,0x00,0x09,0x00,0x09,0x00,0x09,0x00,0x09,0x00,0x09,0x00,0x00,0x00,0x27,
0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0x00,0x00,0x09,0x00,
0x09,0x00,0x09,0x00,0x09,0x00,0x09,0x00,0x09,0x00,0x09,0x00,0x09,0x00,0x09,0x00,
0x09,0x00,0x09,0x00,0x00,0x00,0x3d,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x16,0x00,0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,
0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,
0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,
0x00,0x00,0x00,0x39,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x14,
0x00,0x00,0x11,0x00,0x11,0x00,0x11,0x00,0x11,0x00,0x11,0x00,0x11,0x00,0x11,0x00,
0x11,0x00,0x11,0x00,0x11,0x00,0x11,0x00,0x11,0x00,0x11,0x00,0x11,0x00,0x11,0x00,
0x11,0x00,0x11,0x00,0x11,0x00,0x11,0x00,0x11,0x00,0x00,0x00,0x4d,0x00,0x01,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1e,0x00,0x00,0x15,0x00,0x15,0x00,0x15,
0x00,0x15,0x00,0x15,0x00,0x15,0x00,0x15,0x00,0x15,0x00,0x15,0x00,0x15,0x00,0x15,
0x00,0x15,0x00,0x15,0x00,0x15,0x00,0x16,0x00,0x16,0x00,0x16,0x00,0x16,0x00,0x16,
0x00,0x16,0x00,0x16,0x00,0x16,0x00,0x16,0x00,0x16,0x00,0x16,0x00,0x16,0x00,0x16,
0x00,0x16,0x00,0x16,0x00,0x16,0x00,0x00,0x00,0x59,0x00,0x01,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x24,0x00,0x00,0x1a,0x00,0x1a,0x00,0x1a,0x00,0x1a,0x00,
0x1a,0x00,0x1a,0x00,0x1a,0x00,0x1a,0x00,0x1a,0x00,0x1a,0x00,0x1a,0x00,0x1a,0x00,
0x1a,0x00,0x1a,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,
0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,
0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x45,
0x4e,0x44,0x00,0x00,0x00,0x00,0x08,
};
void _mruby_numeric_ext_gem_test(state *mrb);
void GENERATED_TMP__mruby_numeric_ext_gem_init(state *mrb);
void GENERATED_TMP__mruby_numeric_ext_gem_final(state *mrb);
void _init_test_driver(state *mrb, _bool verbose);
void _t_pass_result(state *dst, state *src);
void GENERATED_TMP__mruby_numeric_ext_gem_test(state *mrb) {
  state *mrb2;
  int ai;
  ai = _gc_arena_save(mrb);
  mrb2 = _open_core(_default_allocf, NULL);
  if (mrb2 == NULL) {
    fprintf(stderr, "Invalid state, exiting %s", __FUNCTION__);
    exit(EXIT_FAILURE);
  }
  GENERATED_TMP__mruby_numeric_ext_gem_init(mrb2);
  _state_atexit(mrb2, GENERATED_TMP__mruby_numeric_ext_gem_final);
  _init_test_driver(mrb2, _test(_gv_get(mrb, _intern_lit(mrb, "$mrbtest_verbose"))));
  _load_irep(mrb2, mrbtest_assert_irep);
  if (mrb2->exc) {
    _print_error(mrb2);
    _close(mrb2);
    exit(EXIT_FAILURE);
  }
  _const_set(mrb2, _obj_value(mrb2->object_class), _intern_lit(mrb2, "GEMNAME"), _str_new(mrb2, "mruby-numeric-ext", 17));
  _load_irep(mrb2, gem_test_irep_mruby_numeric_ext_0);
  
  _t_pass_result(mrb, mrb2);
  _close(mrb2);
  _gc_arena_restore(mrb, ai);
}
