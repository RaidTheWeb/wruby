/*
 * This file is loading the irep
 * Ruby GEM code.
 *
 * IMPORTANT:
 *   This file was generated!
 *   All manual changes will get lost.
 */
#include <stdlib.h>
#include <mruby.h>
#include <mruby/irep.h>
/* dumped in little endian order.
   use `mrbc -E` option for big endian CPU. */
#include <stdint.h>
extern const uint8_t gem_mrblib_irep_mruby_struct[];
const uint8_t
#if defined __GNUC__
__attribute__((aligned(4)))
#elif defined _MSC_VER
__declspec(align(4))
#endif
gem_mrblib_irep_mruby_struct[] = {
0x45,0x54,0x49,0x52,0x30,0x30,0x30,0x35,0x72,0x79,0x00,0x00,0x05,0x89,0x4d,0x41,
0x54,0x5a,0x30,0x30,0x30,0x30,0x49,0x52,0x45,0x50,0x00,0x00,0x04,0xc4,0x30,0x30,
0x30,0x32,0x00,0x00,0x00,0xef,0x00,0x01,0x00,0x04,0x00,0x02,0x00,0x00,0x00,0x2b,
0x1b,0x01,0x00,0x0e,0x02,0x01,0x2e,0x01,0x02,0x01,0x23,0x01,0x00,0x26,0x0f,0x01,
0x0f,0x02,0x5a,0x01,0x01,0x5c,0x01,0x00,0x61,0x01,0x56,0x02,0x01,0x5d,0x01,0x03,
0x0e,0x01,0x03,0x21,0x00,0x28,0x0f,0x01,0x37,0x01,0x67,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x04,0x00,0x06,0x4f,0x62,0x6a,0x65,0x63,0x74,0x00,0x00,0x06,0x53,0x74,
0x72,0x75,0x63,0x74,0x00,0x00,0x0e,0x63,0x6f,0x6e,0x73,0x74,0x5f,0x64,0x65,0x66,
0x69,0x6e,0x65,0x64,0x3f,0x00,0x00,0x03,0x64,0x69,0x67,0x00,0x00,0x00,0x01,0x0e,
0x00,0x01,0x00,0x03,0x00,0x05,0x00,0x00,0x00,0x2f,0x00,0x00,0x61,0x01,0x56,0x02,
0x00,0x5d,0x01,0x00,0x61,0x01,0x56,0x02,0x01,0x5d,0x01,0x01,0x61,0x01,0x56,0x02,
0x02,0x5d,0x01,0x02,0x61,0x01,0x56,0x02,0x03,0x5d,0x01,0x03,0x61,0x01,0x56,0x02,
0x04,0x5d,0x01,0x04,0x5e,0x05,0x04,0x0f,0x01,0x37,0x01,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x06,0x00,0x04,0x65,0x61,0x63,0x68,0x00,0x00,0x09,0x65,0x61,0x63,0x68,
0x5f,0x70,0x61,0x69,0x72,0x00,0x00,0x06,0x73,0x65,0x6c,0x65,0x63,0x74,0x00,0x00,
0x08,0x5f,0x69,0x6e,0x73,0x70,0x65,0x63,0x74,0x00,0x00,0x07,0x69,0x6e,0x73,0x70,
0x65,0x63,0x74,0x00,0x00,0x04,0x74,0x6f,0x5f,0x73,0x00,0x00,0x00,0x00,0x97,0x00,
0x02,0x00,0x04,0x00,0x01,0x00,0x00,0x00,0x19,0x00,0x00,0x00,0x33,0x00,0x00,0x01,
0x10,0x02,0x2e,0x02,0x00,0x00,0x2e,0x02,0x01,0x00,0x55,0x03,0x00,0x2f,0x02,0x02,
0x00,0x10,0x02,0x37,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x05,0x63,
0x6c,0x61,0x73,0x73,0x00,0x00,0x07,0x6d,0x65,0x6d,0x62,0x65,0x72,0x73,0x00,0x00,
0x04,0x65,0x61,0x63,0x68,0x00,0x00,0x00,0x00,0x82,0x00,0x03,0x00,0x07,0x00,0x00,
0x00,0x00,0x00,0x17,0x33,0x04,0x00,0x00,0x1f,0x03,0x01,0x00,0x10,0x04,0x01,0x05,
0x01,0x2e,0x04,0x00,0x01,0x2e,0x03,0x01,0x01,0x37,0x03,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x02,0x00,0x02,0x5b,0x5d,0x00,0x00,0x04,0x63,0x61,0x6c,0x6c,0x00,0x00,
0x00,0x00,0x97,0x00,0x02,0x00,0x04,0x00,0x01,0x00,0x00,0x00,0x19,0x00,0x00,0x00,
0x33,0x00,0x00,0x01,0x10,0x02,0x2e,0x02,0x00,0x00,0x2e,0x02,0x01,0x00,0x55,0x03,
0x00,0x2f,0x02,0x02,0x00,0x10,0x02,0x37,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x03,0x00,0x05,0x63,0x6c,0x61,0x73,0x73,0x00,0x00,0x07,0x6d,0x65,0x6d,0x62,0x65,
0x72,0x73,0x00,0x00,0x04,0x65,0x61,0x63,0x68,0x00,0x00,0x00,0x00,0xa7,0x00,0x03,
0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x1e,0x33,0x04,0x00,0x00,0x1f,0x03,0x01,0x00,
0x01,0x04,0x01,0x2e,0x04,0x00,0x00,0x10,0x05,0x01,0x06,0x01,0x2e,0x05,0x01,0x01,
0x2e,0x03,0x02,0x02,0x37,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x06,
0x74,0x6f,0x5f,0x73,0x79,0x6d,0x00,0x00,0x02,0x5b,0x5d,0x00,0x00,0x04,0x63,0x61,
0x6c,0x6c,0x00,0x00,0x00,0x00,0xa7,0x00,0x03,0x00,0x05,0x00,0x01,0x00,0x00,0x00,
0x1d,0x00,0x00,0x00,0x33,0x00,0x00,0x01,0x46,0x03,0x00,0x01,0x02,0x03,0x10,0x03,
0x2e,0x03,0x00,0x00,0x2e,0x03,0x01,0x00,0x55,0x04,0x00,0x2f,0x03,0x02,0x00,0x37,
0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x05,0x63,0x6c,0x61,0x73,0x73,
0x00,0x00,0x07,0x6d,0x65,0x6d,0x62,0x65,0x72,0x73,0x00,0x00,0x04,0x65,0x61,0x63,
0x68,0x00,0x00,0x00,0x00,0xf1,0x00,0x04,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x31,
0x33,0x04,0x00,0x00,0x10,0x04,0x01,0x05,0x01,0x2e,0x04,0x00,0x01,0x01,0x03,0x04,
0x1f,0x04,0x01,0x00,0x01,0x05,0x03,0x2e,0x04,0x01,0x01,0x23,0x04,0x00,0x2d,0x1f,
0x04,0x02,0x00,0x01,0x05,0x03,0x2e,0x04,0x02,0x01,0x21,0x00,0x2f,0x0f,0x04,0x37,
0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x02,0x5b,0x5d,0x00,0x00,0x04,
0x63,0x61,0x6c,0x6c,0x00,0x00,0x04,0x70,0x75,0x73,0x68,0x00,0x00,0x00,0x01,0xca,
0x00,0x05,0x00,0x09,0x00,0x01,0x00,0x00,0x00,0x5b,0x00,0x00,0x33,0x00,0x00,0x00,
0x10,0x05,0x2e,0x05,0x00,0x00,0x2e,0x05,0x01,0x00,0x01,0x02,0x05,0x06,0x06,0x2e,
0x05,0x02,0x01,0x4f,0x06,0x00,0x41,0x05,0x23,0x05,0x00,0x26,0x4f,0x03,0x01,0x21,
0x00,0x36,0x4f,0x05,0x01,0x01,0x06,0x02,0x50,0x05,0x4f,0x06,0x02,0x50,0x05,0x01,
0x03,0x05,0x46,0x05,0x00,0x01,0x04,0x05,0x10,0x05,0x55,0x06,0x00,0x2f,0x05,0x03,
0x00,0x01,0x05,0x03,0x01,0x06,0x04,0x4f,0x07,0x03,0x2e,0x06,0x04,0x01,0x3b,0x05,
0x4f,0x06,0x04,0x3b,0x05,0x37,0x05,0x00,0x00,0x00,0x05,0x00,0x00,0x01,0x23,0x00,
0x00,0x09,0x23,0x3c,0x73,0x74,0x72,0x75,0x63,0x74,0x20,0x00,0x00,0x01,0x20,0x00,
0x00,0x02,0x2c,0x20,0x00,0x00,0x01,0x3e,0x00,0x00,0x00,0x05,0x00,0x05,0x63,0x6c,
0x61,0x73,0x73,0x00,0x00,0x04,0x74,0x6f,0x5f,0x73,0x00,0x00,0x02,0x5b,0x5d,0x00,
0x00,0x09,0x65,0x61,0x63,0x68,0x5f,0x70,0x61,0x69,0x72,0x00,0x00,0x04,0x6a,0x6f,
0x69,0x6e,0x00,0x00,0x00,0x00,0xcf,0x00,0x04,0x00,0x08,0x00,0x00,0x00,0x00,0x00,
0x26,0x00,0x00,0x00,0x33,0x08,0x00,0x00,0x1f,0x04,0x04,0x00,0x01,0x05,0x01,0x2e,
0x05,0x00,0x00,0x4f,0x06,0x00,0x3b,0x05,0x01,0x06,0x02,0x2e,0x06,0x01,0x00,0x3b,
0x05,0x46,0x05,0x01,0x2e,0x04,0x02,0x01,0x37,0x04,0x00,0x00,0x00,0x01,0x00,0x00,
0x01,0x3d,0x00,0x00,0x00,0x03,0x00,0x04,0x74,0x6f,0x5f,0x73,0x00,0x00,0x08,0x5f,
0x69,0x6e,0x73,0x70,0x65,0x63,0x74,0x00,0x00,0x04,0x70,0x75,0x73,0x68,0x00,0x00,
0x00,0x01,0x4b,0x00,0x02,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x3c,0x00,0x00,0x00,
0x33,0x00,0x00,0x00,0x25,0x00,0x10,0x10,0x02,0x2e,0x02,0x00,0x00,0x21,0x00,0x38,
0x26,0x02,0x1b,0x03,0x01,0x27,0x02,0x03,0x22,0x03,0x00,0x1f,0x21,0x00,0x36,0x4f,
0x02,0x00,0x10,0x03,0x2e,0x03,0x02,0x00,0x2e,0x03,0x03,0x00,0x50,0x02,0x4f,0x03,
0x01,0x50,0x02,0x21,0x00,0x3a,0x29,0x02,0x28,0x01,0x37,0x02,0x00,0x00,0x00,0x02,
0x00,0x00,0x09,0x23,0x3c,0x73,0x74,0x72,0x75,0x63,0x74,0x20,0x00,0x00,0x05,0x3a,
0x2e,0x2e,0x2e,0x3e,0x00,0x00,0x00,0x04,0x00,0x08,0x5f,0x69,0x6e,0x73,0x70,0x65,
0x63,0x74,0x00,0x00,0x10,0x53,0x79,0x73,0x74,0x65,0x6d,0x53,0x74,0x61,0x63,0x6b,
0x45,0x72,0x72,0x6f,0x72,0x00,0x00,0x05,0x63,0x6c,0x61,0x73,0x73,0x00,0x00,0x04,
0x74,0x6f,0x5f,0x73,0x00,0x00,0x00,0x01,0x1c,0x00,0x05,0x00,0x08,0x00,0x00,0x00,
0x00,0x00,0x3c,0x00,0x33,0x04,0x10,0x00,0x10,0x05,0x01,0x06,0x01,0x2e,0x05,0x00,
0x01,0x01,0x04,0x05,0x01,0x05,0x02,0x2e,0x05,0x01,0x00,0x06,0x06,0x44,0x05,0x23,
0x05,0x00,0x37,0x01,0x05,0x04,0x01,0x06,0x05,0x24,0x06,0x00,0x34,0x46,0x06,0x00,
0x01,0x07,0x02,0x48,0x06,0x2c,0x05,0x02,0x21,0x00,0x3a,0x01,0x05,0x04,0x37,0x05,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x02,0x5b,0x5d,0x00,0x00,0x04,0x73,
0x69,0x7a,0x65,0x00,0x00,0x03,0x64,0x69,0x67,0x00,0x4c,0x56,0x41,0x52,0x00,0x00,
0x00,0xa7,0x00,0x00,0x00,0x0d,0x00,0x05,0x62,0x6c,0x6f,0x63,0x6b,0x00,0x05,0x66,
0x69,0x65,0x6c,0x64,0x00,0x01,0x26,0x00,0x03,0x61,0x72,0x79,0x00,0x03,0x76,0x61,
0x6c,0x00,0x04,0x6e,0x61,0x6d,0x65,0x00,0x03,0x73,0x74,0x72,0x00,0x03,0x62,0x75,
0x66,0x00,0x01,0x6b,0x00,0x01,0x76,0x00,0x03,0x69,0x64,0x78,0x00,0x04,0x61,0x72,
0x67,0x73,0x00,0x01,0x6e,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x02,0x00,
0x02,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x02,0x00,0x02,0x00,0x00,0x00,
0x01,0x00,0x03,0x00,0x02,0x00,0x01,0x00,0x01,0x00,0x02,0x00,0x02,0x00,0x04,0x00,
0x03,0x00,0x02,0x00,0x01,0x00,0x05,0x00,0x02,0x00,0x06,0x00,0x03,0x00,0x07,0x00,
0x04,0x00,0x08,0x00,0x01,0x00,0x09,0x00,0x02,0x00,0x02,0x00,0x03,0x00,0x02,0x00,
0x01,0x00,0x0a,0x00,0x01,0x00,0x0b,0x00,0x02,0x00,0x02,0x00,0x03,0x00,0x0c,0x00,
0x04,0x45,0x4e,0x44,0x00,0x00,0x00,0x00,0x08,
};
void _mruby_struct_gem_init(state *mrb);
void _mruby_struct_gem_final(state *mrb);

void GENERATED_TMP__mruby_struct_gem_init(state *mrb) {
  int ai = _gc_arena_save(mrb);
  _mruby_struct_gem_init(mrb);
  _load_irep(mrb, gem_mrblib_irep_mruby_struct);
  if (mrb->exc) {
    _print_error(mrb);
    _close(mrb);
    exit(EXIT_FAILURE);
  }
  _gc_arena_restore(mrb, ai);
}

void GENERATED_TMP__mruby_struct_gem_final(state *mrb) {
  _mruby_struct_gem_final(mrb);
}
