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
 * This file contains a test code for mruby-random gem.
 *
 * IMPORTANT:
 *   This file was generated!
 *   All manual changes will get lost.
 */
extern const uint8_t mrbtest_assert_irep[];
/* dumped in little endian order.
   use `mrbc -E` option for big endian CPU. */
#include <stdint.h>
extern const uint8_t gem_test_irep_mruby_random_0[];
const uint8_t
#if defined __GNUC__
__attribute__((aligned(4)))
#elif defined _MSC_VER
__declspec(align(4))
#endif
gem_test_irep_mruby_random_0[] = {
0x45,0x54,0x49,0x52,0x30,0x30,0x30,0x35,0x8b,0x52,0x00,0x00,0x12,0x37,0x4d,0x41,
0x54,0x5a,0x30,0x30,0x30,0x30,0x49,0x52,0x45,0x50,0x00,0x00,0x08,0x4f,0x30,0x30,
0x30,0x32,0x00,0x00,0x02,0xdc,0x00,0x01,0x00,0x04,0x00,0x0a,0x00,0x00,0x00,0x7b,
0x10,0x01,0x4f,0x02,0x00,0x55,0x03,0x00,0x2f,0x01,0x00,0x01,0x10,0x01,0x4f,0x02,
0x01,0x55,0x03,0x01,0x2f,0x01,0x00,0x01,0x10,0x01,0x4f,0x02,0x02,0x55,0x03,0x02,
0x2f,0x01,0x00,0x01,0x10,0x01,0x4f,0x02,0x03,0x55,0x03,0x03,0x2f,0x01,0x00,0x01,
0x10,0x01,0x4f,0x02,0x04,0x55,0x03,0x04,0x2f,0x01,0x00,0x01,0x10,0x01,0x4f,0x02,
0x05,0x55,0x03,0x05,0x2f,0x01,0x00,0x01,0x10,0x01,0x4f,0x02,0x06,0x55,0x03,0x06,
0x2f,0x01,0x00,0x01,0x10,0x01,0x4f,0x02,0x07,0x55,0x03,0x07,0x2f,0x01,0x00,0x01,
0x10,0x01,0x4f,0x02,0x08,0x55,0x03,0x08,0x2f,0x01,0x00,0x01,0x10,0x01,0x4f,0x02,
0x09,0x55,0x03,0x09,0x2f,0x01,0x00,0x01,0x37,0x01,0x67,0x00,0x00,0x00,0x0a,0x00,
0x00,0x0c,0x52,0x61,0x6e,0x64,0x6f,0x6d,0x23,0x73,0x72,0x61,0x6e,0x64,0x00,0x00,
0x0d,0x4b,0x65,0x72,0x6e,0x65,0x6c,0x3a,0x3a,0x73,0x72,0x61,0x6e,0x64,0x00,0x00,
0x0d,0x52,0x61,0x6e,0x64,0x6f,0x6d,0x3a,0x3a,0x73,0x72,0x61,0x6e,0x64,0x00,0x00,
0x06,0x66,0x69,0x78,0x6e,0x75,0x6d,0x00,0x00,0x05,0x66,0x6c,0x6f,0x61,0x74,0x00,
0x00,0x0d,0x41,0x72,0x72,0x61,0x79,0x23,0x73,0x68,0x75,0x66,0x66,0x6c,0x65,0x00,
0x00,0x0e,0x41,0x72,0x72,0x61,0x79,0x23,0x73,0x68,0x75,0x66,0x66,0x6c,0x65,0x21,
0x00,0x00,0x15,0x41,0x72,0x72,0x61,0x79,0x23,0x73,0x68,0x75,0x66,0x66,0x6c,0x65,
0x28,0x72,0x61,0x6e,0x64,0x6f,0x6d,0x29,0x00,0x00,0x16,0x41,0x72,0x72,0x61,0x79,
0x23,0x73,0x68,0x75,0x66,0x66,0x6c,0x65,0x21,0x28,0x72,0x61,0x6e,0x64,0x6f,0x6d,
0x29,0x00,0x00,0x38,0x41,0x72,0x72,0x61,0x79,0x23,0x73,0x61,0x6d,0x70,0x6c,0x65,
0x20,0x63,0x68,0x65,0x63,0x6b,0x73,0x20,0x69,0x6e,0x70,0x75,0x74,0x20,0x6c,0x65,
0x6e,0x67,0x74,0x68,0x20,0x61,0x66,0x74,0x65,0x72,0x20,0x72,0x65,0x61,0x64,0x69,
0x6e,0x67,0x20,0x61,0x72,0x67,0x75,0x6d,0x65,0x6e,0x74,0x73,0x00,0x00,0x00,0x01,
0x00,0x06,0x61,0x73,0x73,0x65,0x72,0x74,0x00,0x00,0x00,0x00,0xe0,0x00,0x03,0x00,
0x06,0x00,0x00,0x00,0x00,0x00,0x2c,0x00,0x1b,0x03,0x00,0x03,0x04,0x7b,0x2e,0x03,
0x01,0x01,0x01,0x01,0x03,0x1b,0x03,0x00,0x03,0x04,0x7b,0x2e,0x03,0x01,0x01,0x01,
0x02,0x03,0x01,0x03,0x01,0x2e,0x03,0x02,0x00,0x01,0x04,0x02,0x2e,0x04,0x02,0x00,
0x41,0x03,0x37,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x06,0x52,0x61,
0x6e,0x64,0x6f,0x6d,0x00,0x00,0x03,0x6e,0x65,0x77,0x00,0x00,0x04,0x72,0x61,0x6e,
0x64,0x00,0x00,0x00,0x00,0xe1,0x00,0x03,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x2e,
0x10,0x03,0x03,0x04,0xea,0x2e,0x03,0x00,0x01,0x10,0x03,0x2e,0x03,0x01,0x00,0x01,
0x01,0x03,0x10,0x03,0x03,0x04,0xea,0x2e,0x03,0x00,0x01,0x10,0x03,0x2e,0x03,0x01,
0x00,0x01,0x02,0x03,0x01,0x03,0x01,0x01,0x04,0x02,0x41,0x03,0x37,0x03,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x05,0x73,0x72,0x61,0x6e,0x64,0x00,0x00,0x04,
0x72,0x61,0x6e,0x64,0x00,0x00,0x00,0x01,0x02,0x00,0x03,0x00,0x06,0x00,0x00,0x00,
0x00,0x00,0x34,0x00,0x1b,0x03,0x00,0x65,0x03,0x04,0x01,0x59,0x2e,0x03,0x01,0x01,
0x10,0x03,0x2e,0x03,0x02,0x00,0x01,0x01,0x03,0x10,0x03,0x65,0x03,0x04,0x01,0x59,
0x2e,0x03,0x01,0x01,0x1b,0x03,0x00,0x2e,0x03,0x02,0x00,0x01,0x02,0x03,0x01,0x03,
0x01,0x01,0x04,0x02,0x41,0x03,0x37,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,
0x00,0x06,0x52,0x61,0x6e,0x64,0x6f,0x6d,0x00,0x00,0x05,0x73,0x72,0x61,0x6e,0x64,
0x00,0x00,0x04,0x72,0x61,0x6e,0x64,0x00,0x00,0x00,0x00,0x7e,0x00,0x01,0x00,0x04,
0x00,0x00,0x00,0x00,0x00,0x13,0x00,0x00,0x10,0x01,0x09,0x02,0x2e,0x01,0x00,0x01,
0x2e,0x01,0x01,0x00,0x1b,0x02,0x02,0x41,0x01,0x37,0x01,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x03,0x00,0x04,0x72,0x61,0x6e,0x64,0x00,0x00,0x05,0x63,0x6c,0x61,0x73,
0x73,0x00,0x00,0x06,0x46,0x69,0x78,0x6e,0x75,0x6d,0x00,0x00,0x00,0x00,0x75,0x00,
0x01,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x11,0x00,0x00,0x00,0x10,0x01,0x2e,0x01,
0x00,0x00,0x2e,0x01,0x01,0x00,0x1b,0x02,0x02,0x41,0x01,0x37,0x01,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x03,0x00,0x04,0x72,0x61,0x6e,0x64,0x00,0x00,0x05,0x63,0x6c,
0x61,0x73,0x73,0x00,0x00,0x05,0x46,0x6c,0x6f,0x61,0x74,0x00,0x00,0x00,0x01,0xb5,
0x00,0x03,0x00,0x0e,0x00,0x01,0x00,0x00,0x00,0x61,0x00,0x00,0x07,0x03,0x08,0x04,
0x09,0x05,0x0a,0x06,0x0b,0x07,0x0c,0x08,0x0d,0x09,0x03,0x0a,0x08,0x03,0x0b,0x09,
0x03,0x0c,0x0a,0x46,0x03,0x0a,0x01,0x01,0x03,0x2e,0x03,0x00,0x00,0x01,0x02,0x03,
0x01,0x03,0x01,0x07,0x04,0x08,0x05,0x09,0x06,0x0a,0x07,0x0b,0x08,0x0c,0x09,0x0d,
0x0a,0x03,0x0b,0x08,0x03,0x0c,0x09,0x03,0x0d,0x0a,0x46,0x04,0x0a,0x41,0x03,0x23,
0x03,0x00,0x51,0x01,0x03,0x02,0x01,0x04,0x01,0x2e,0x03,0x01,0x01,0x23,0x03,0x00,
0x5f,0x03,0x03,0x0a,0x55,0x04,0x00,0x2f,0x03,0x02,0x00,0x37,0x03,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x03,0x00,0x07,0x73,0x68,0x75,0x66,0x66,0x6c,0x65,0x00,0x00,
0x02,0x21,0x3d,0x00,0x00,0x05,0x74,0x69,0x6d,0x65,0x73,0x00,0x00,0x00,0x00,0x69,
0x00,0x03,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x11,0x00,0x00,0x33,0x04,0x00,0x00,
0x1f,0x03,0x01,0x00,0x01,0x04,0x01,0x2e,0x03,0x00,0x01,0x37,0x03,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x01,0x00,0x08,0x69,0x6e,0x63,0x6c,0x75,0x64,0x65,0x3f,0x00,
0x00,0x00,0x01,0x7a,0x00,0x02,0x00,0x0d,0x00,0x01,0x00,0x00,0x00,0x52,0x00,0x00,
0x07,0x02,0x08,0x03,0x09,0x04,0x0a,0x05,0x0b,0x06,0x0c,0x07,0x0d,0x08,0x03,0x09,
0x08,0x03,0x0a,0x09,0x03,0x0b,0x0a,0x46,0x02,0x0a,0x01,0x01,0x02,0x2e,0x02,0x00,
0x00,0x01,0x02,0x01,0x07,0x03,0x08,0x04,0x09,0x05,0x0a,0x06,0x0b,0x07,0x0c,0x08,
0x0d,0x09,0x03,0x0a,0x08,0x03,0x0b,0x09,0x03,0x0c,0x0a,0x46,0x03,0x0a,0x2e,0x02,
0x01,0x01,0x23,0x02,0x00,0x50,0x03,0x02,0x0a,0x55,0x03,0x00,0x2f,0x02,0x02,0x00,
0x37,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x08,0x73,0x68,0x75,0x66,
0x66,0x6c,0x65,0x21,0x00,0x00,0x02,0x21,0x3d,0x00,0x00,0x05,0x74,0x69,0x6d,0x65,
0x73,0x00,0x00,0x00,0x00,0x69,0x00,0x03,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x11,
0x33,0x04,0x00,0x00,0x1f,0x03,0x01,0x00,0x01,0x04,0x01,0x2e,0x03,0x00,0x01,0x37,
0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x08,0x69,0x6e,0x63,0x6c,0x75,
0x64,0x65,0x3f,0x00,0x00,0x00,0x02,0xa3,0x00,0x05,0x00,0x0f,0x00,0x02,0x00,0x00,
0x00,0x92,0x00,0x00,0x10,0x05,0x1b,0x06,0x00,0x55,0x07,0x00,0x2f,0x05,0x01,0x01,
0x07,0x05,0x08,0x06,0x09,0x07,0x0a,0x08,0x0b,0x09,0x0c,0x0a,0x0d,0x0b,0x03,0x0c,
0x08,0x03,0x0d,0x09,0x03,0x0e,0x0a,0x46,0x05,0x0a,0x01,0x01,0x05,0x1b,0x06,0x02,
0x65,0x03,0x07,0x01,0x59,0x2e,0x06,0x03,0x01,0x2e,0x05,0x04,0x01,0x01,0x02,0x05,
0x07,0x05,0x08,0x06,0x09,0x07,0x0a,0x08,0x0b,0x09,0x0c,0x0a,0x0d,0x0b,0x03,0x0c,
0x08,0x03,0x0d,0x09,0x03,0x0e,0x0a,0x46,0x05,0x0a,0x01,0x03,0x05,0x1b,0x06,0x02,
0x65,0x03,0x07,0x01,0x59,0x2e,0x06,0x03,0x01,0x2e,0x05,0x04,0x01,0x01,0x04,0x05,
0x01,0x05,0x01,0x01,0x06,0x02,0x2e,0x05,0x05,0x01,0x23,0x05,0x00,0x84,0x03,0x05,
0x0a,0x55,0x06,0x01,0x2f,0x05,0x06,0x00,0x23,0x05,0x00,0x90,0x01,0x05,0x02,0x01,
0x06,0x04,0x41,0x05,0x37,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x00,0x09,
0x54,0x79,0x70,0x65,0x45,0x72,0x72,0x6f,0x72,0x00,0x00,0x0c,0x61,0x73,0x73,0x65,
0x72,0x74,0x5f,0x72,0x61,0x69,0x73,0x65,0x00,0x00,0x06,0x52,0x61,0x6e,0x64,0x6f,
0x6d,0x00,0x00,0x03,0x6e,0x65,0x77,0x00,0x00,0x07,0x73,0x68,0x75,0x66,0x66,0x6c,
0x65,0x00,0x00,0x02,0x21,0x3d,0x00,0x00,0x05,0x74,0x69,0x6d,0x65,0x73,0x00,0x00,
0x00,0x00,0x7c,0x00,0x01,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,
0x07,0x01,0x08,0x02,0x46,0x01,0x02,0x4f,0x02,0x00,0x2e,0x01,0x00,0x01,0x37,0x01,
0x00,0x00,0x00,0x01,0x00,0x00,0x15,0x4e,0x6f,0x74,0x20,0x61,0x20,0x52,0x61,0x6e,
0x64,0x6f,0x6d,0x20,0x69,0x6e,0x73,0x74,0x61,0x6e,0x63,0x65,0x00,0x00,0x00,0x01,
0x00,0x07,0x73,0x68,0x75,0x66,0x66,0x6c,0x65,0x00,0x00,0x00,0x00,0x69,0x00,0x03,
0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x11,0x33,0x04,0x00,0x00,0x1f,0x03,0x02,0x00,
0x01,0x04,0x01,0x2e,0x03,0x00,0x01,0x37,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x01,0x00,0x08,0x69,0x6e,0x63,0x6c,0x75,0x64,0x65,0x3f,0x00,0x00,0x00,0x02,0xe8,
0x00,0x03,0x00,0x0e,0x00,0x02,0x00,0x00,0x00,0xa3,0x00,0x00,0x10,0x03,0x1b,0x04,
0x00,0x55,0x05,0x00,0x2f,0x03,0x01,0x01,0x07,0x03,0x08,0x04,0x09,0x05,0x0a,0x06,
0x0b,0x07,0x0c,0x08,0x0d,0x09,0x03,0x0a,0x08,0x03,0x0b,0x09,0x03,0x0c,0x0a,0x46,
0x03,0x0a,0x01,0x01,0x03,0x1b,0x04,0x02,0x65,0x03,0x05,0x01,0x59,0x2e,0x04,0x03,
0x01,0x2e,0x03,0x04,0x01,0x07,0x03,0x08,0x04,0x09,0x05,0x0a,0x06,0x0b,0x07,0x0c,
0x08,0x0d,0x09,0x03,0x0a,0x08,0x03,0x0b,0x09,0x03,0x0c,0x0a,0x46,0x03,0x0a,0x01,
0x02,0x03,0x1b,0x04,0x02,0x65,0x03,0x05,0x01,0x59,0x2e,0x04,0x03,0x01,0x2e,0x03,
0x04,0x01,0x01,0x03,0x01,0x07,0x04,0x08,0x05,0x09,0x06,0x0a,0x07,0x0b,0x08,0x0c,
0x09,0x0d,0x0a,0x03,0x0b,0x08,0x03,0x0c,0x09,0x03,0x0d,0x0a,0x46,0x04,0x0a,0x2e,
0x03,0x05,0x01,0x23,0x03,0x00,0x95,0x03,0x03,0x0a,0x55,0x04,0x01,0x2f,0x03,0x06,
0x00,0x23,0x03,0x00,0xa1,0x01,0x03,0x01,0x01,0x04,0x02,0x41,0x03,0x37,0x03,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x00,0x09,0x54,0x79,0x70,0x65,0x45,0x72,0x72,
0x6f,0x72,0x00,0x00,0x0c,0x61,0x73,0x73,0x65,0x72,0x74,0x5f,0x72,0x61,0x69,0x73,
0x65,0x00,0x00,0x06,0x52,0x61,0x6e,0x64,0x6f,0x6d,0x00,0x00,0x03,0x6e,0x65,0x77,
0x00,0x00,0x08,0x73,0x68,0x75,0x66,0x66,0x6c,0x65,0x21,0x00,0x00,0x02,0x21,0x3d,
0x00,0x00,0x05,0x74,0x69,0x6d,0x65,0x73,0x00,0x00,0x00,0x00,0x7d,0x00,0x01,0x00,
0x04,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x07,0x01,0x08,0x02,0x46,0x01,0x02,0x4f,
0x02,0x00,0x2e,0x01,0x00,0x01,0x37,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x15,0x4e,
0x6f,0x74,0x20,0x61,0x20,0x52,0x61,0x6e,0x64,0x6f,0x6d,0x20,0x69,0x6e,0x73,0x74,
0x61,0x6e,0x63,0x65,0x00,0x00,0x00,0x01,0x00,0x08,0x73,0x68,0x75,0x66,0x66,0x6c,
0x65,0x21,0x00,0x00,0x00,0x00,0x69,0x00,0x03,0x00,0x06,0x00,0x00,0x00,0x00,0x00,
0x11,0x00,0x00,0x00,0x33,0x04,0x00,0x00,0x1f,0x03,0x01,0x00,0x01,0x04,0x01,0x2e,
0x03,0x00,0x01,0x37,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x08,0x69,
0x6e,0x63,0x6c,0x75,0x64,0x65,0x3f,0x00,0x00,0x00,0x01,0x40,0x00,0x01,0x00,0x06,
0x00,0x01,0x00,0x00,0x00,0x3b,0x00,0x00,0x07,0x01,0x08,0x02,0x09,0x03,0x46,0x01,
0x03,0x14,0x01,0x00,0x0f,0x01,0x0f,0x02,0x5a,0x01,0x01,0x5c,0x01,0x00,0x10,0x01,
0x07,0x02,0x08,0x03,0x09,0x04,0x0a,0x05,0x46,0x02,0x04,0x13,0x03,0x00,0x1b,0x04,
0x01,0x2e,0x04,0x02,0x00,0x2e,0x03,0x03,0x01,0x2e,0x03,0x04,0x00,0x2e,0x01,0x05,
0x02,0x37,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x04,0x24,0x61,0x72,
0x79,0x00,0x00,0x0b,0x41,0x72,0x72,0x61,0x79,0x43,0x68,0x61,0x6e,0x67,0x65,0x00,
0x00,0x03,0x6e,0x65,0x77,0x00,0x00,0x06,0x73,0x61,0x6d,0x70,0x6c,0x65,0x00,0x00,
0x04,0x73,0x6f,0x72,0x74,0x00,0x00,0x0c,0x61,0x73,0x73,0x65,0x72,0x74,0x5f,0x65,
0x71,0x75,0x61,0x6c,0x00,0x00,0x00,0x00,0x55,0x00,0x01,0x00,0x03,0x00,0x01,0x00,
0x00,0x00,0x0d,0x00,0x61,0x01,0x56,0x02,0x00,0x5d,0x01,0x00,0x0e,0x01,0x00,0x37,
0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x04,0x74,0x6f,0x5f,0x69,0x00,
0x00,0x00,0x00,0x6a,0x00,0x02,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x11,0x00,0x00,
0x33,0x00,0x00,0x00,0x13,0x02,0x00,0x0a,0x03,0x2e,0x02,0x01,0x01,0x0a,0x02,0x37,
0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x04,0x24,0x61,0x72,0x79,0x00,
0x00,0x02,0x3c,0x3c,0x00,0x44,0x42,0x47,0x00,0x00,0x00,0x09,0x21,0x00,0x01,0x00,
0x2e,0x2f,0x6f,0x70,0x74,0x2f,0x6d,0x72,0x75,0x62,0x79,0x2f,0x6d,0x72,0x62,0x67,
0x65,0x6d,0x73,0x2f,0x6d,0x72,0x75,0x62,0x79,0x2d,0x72,0x61,0x6e,0x64,0x6f,0x6d,
0x2f,0x74,0x65,0x73,0x74,0x2f,0x72,0x61,0x6e,0x64,0x6f,0x6d,0x2e,0x72,0x62,0x00,
0x00,0x01,0x07,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7b,0x00,
0x00,0x04,0x00,0x04,0x00,0x04,0x00,0x04,0x00,0x04,0x00,0x04,0x00,0x04,0x00,0x04,
0x00,0x04,0x00,0x04,0x00,0x04,0x00,0x04,0x00,0x0a,0x00,0x0a,0x00,0x0a,0x00,0x0a,
0x00,0x0a,0x00,0x0a,0x00,0x0a,0x00,0x0a,0x00,0x0a,0x00,0x0a,0x00,0x0a,0x00,0x0a,
0x00,0x12,0x00,0x12,0x00,0x12,0x00,0x12,0x00,0x12,0x00,0x12,0x00,0x12,0x00,0x12,
0x00,0x12,0x00,0x12,0x00,0x12,0x00,0x12,0x00,0x1a,0x00,0x1a,0x00,0x1a,0x00,0x1a,
0x00,0x1a,0x00,0x1a,0x00,0x1a,0x00,0x1a,0x00,0x1a,0x00,0x1a,0x00,0x1a,0x00,0x1a,
0x00,0x1e,0x00,0x1e,0x00,0x1e,0x00,0x1e,0x00,0x1e,0x00,0x1e,0x00,0x1e,0x00,0x1e,
0x00,0x1e,0x00,0x1e,0x00,0x1e,0x00,0x1e,0x00,0x22,0x00,0x22,0x00,0x22,0x00,0x22,
0x00,0x22,0x00,0x22,0x00,0x22,0x00,0x22,0x00,0x22,0x00,0x22,0x00,0x22,0x00,0x22,
0x00,0x29,0x00,0x29,0x00,0x29,0x00,0x29,0x00,0x29,0x00,0x29,0x00,0x29,0x00,0x29,
0x00,0x29,0x00,0x29,0x00,0x29,0x00,0x29,0x00,0x30,0x00,0x30,0x00,0x30,0x00,0x30,
0x00,0x30,0x00,0x30,0x00,0x30,0x00,0x30,0x00,0x30,0x00,0x30,0x00,0x30,0x00,0x30,
0x00,0x3f,0x00,0x3f,0x00,0x3f,0x00,0x3f,0x00,0x3f,0x00,0x3f,0x00,0x3f,0x00,0x3f,
0x00,0x3f,0x00,0x3f,0x00,0x3f,0x00,0x3f,0x00,0x4e,0x00,0x4e,0x00,0x4e,0x00,0x4e,
0x00,0x4e,0x00,0x4e,0x00,0x4e,0x00,0x4e,0x00,0x4e,0x00,0x4e,0x00,0x4e,0x00,0x4e,
0x00,0x4e,0x00,0x4e,0x00,0x4e,0x00,0x00,0x00,0x69,0x00,0x01,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x2c,0x00,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,0x00,
0x05,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,0x00,0x05,0x00,
0x05,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,
0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x07,0x00,0x07,0x00,
0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,
0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,
0x00,0x00,0x6d,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2e,0x00,
0x00,0x0b,0x00,0x0b,0x00,0x0b,0x00,0x0b,0x00,0x0b,0x00,0x0b,0x00,0x0b,0x00,0x0b,
0x00,0x0b,0x00,0x0c,0x00,0x0c,0x00,0x0c,0x00,0x0c,0x00,0x0c,0x00,0x0c,0x00,0x0c,
0x00,0x0c,0x00,0x0c,0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0d,
0x00,0x0d,0x00,0x0d,0x00,0x0d,0x00,0x0e,0x00,0x0e,0x00,0x0e,0x00,0x0e,0x00,0x0e,
0x00,0x0e,0x00,0x0e,0x00,0x0e,0x00,0x0e,0x00,0x0f,0x00,0x0f,0x00,0x0f,0x00,0x0f,
0x00,0x0f,0x00,0x0f,0x00,0x0f,0x00,0x0f,0x00,0x0f,0x00,0x0f,0x00,0x00,0x00,0x79,
0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x34,0x00,0x00,0x13,0x00,
0x13,0x00,0x13,0x00,0x13,0x00,0x13,0x00,0x13,0x00,0x13,0x00,0x13,0x00,0x13,0x00,
0x13,0x00,0x13,0x00,0x13,0x00,0x14,0x00,0x14,0x00,0x14,0x00,0x14,0x00,0x14,0x00,
0x14,0x00,0x14,0x00,0x14,0x00,0x14,0x00,0x15,0x00,0x15,0x00,0x15,0x00,0x15,0x00,
0x15,0x00,0x15,0x00,0x15,0x00,0x15,0x00,0x15,0x00,0x15,0x00,0x15,0x00,0x16,0x00,
0x16,0x00,0x16,0x00,0x16,0x00,0x16,0x00,0x16,0x00,0x16,0x00,0x16,0x00,0x16,0x00,
0x16,0x00,0x17,0x00,0x17,0x00,0x17,0x00,0x17,0x00,0x17,0x00,0x17,0x00,0x17,0x00,
0x17,0x00,0x17,0x00,0x17,0x00,0x00,0x00,0x37,0x00,0x01,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x13,0x00,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,
0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,
0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x1b,0x00,0x00,0x00,0x33,
0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x11,0x00,0x00,0x1f,0x00,
0x1f,0x00,0x1f,0x00,0x1f,0x00,0x1f,0x00,0x1f,0x00,0x1f,0x00,0x1f,0x00,0x1f,0x00,
0x1f,0x00,0x1f,0x00,0x1f,0x00,0x1f,0x00,0x1f,0x00,0x1f,0x00,0x1f,0x00,0x1f,0x00,
0x00,0x00,0xd3,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x61,0x00,
0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,
0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,
0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,
0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x23,0x00,0x24,0x00,0x24,0x00,0x24,
0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,
0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,
0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,
0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,
0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,
0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,
0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,
0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,
0x00,0x26,0x00,0x00,0x00,0x33,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x11,0x00,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,
0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,0x26,0x00,
0x26,0x00,0x26,0x00,0x26,0x00,0x00,0x00,0xb5,0x00,0x01,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x52,0x00,0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,
0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,
0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,
0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,0x00,0x2a,
0x00,0x2b,0x00,0x2b,0x00,0x2b,0x00,0x2b,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,
0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,
0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,
0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,
0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,
0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,
0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x00,0x00,0x33,0x00,0x01,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x11,0x00,0x00,0x2d,0x00,0x2d,0x00,
0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,
0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x2d,0x00,0x00,0x01,
0x35,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x92,0x00,0x00,0x31,
0x00,0x31,0x00,0x31,0x00,0x31,0x00,0x31,0x00,0x31,0x00,0x31,0x00,0x31,0x00,0x31,
0x00,0x31,0x00,0x31,0x00,0x31,0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,
0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,
0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,
0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,0x00,0x37,
0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,
0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,
0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,
0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,
0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,
0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,0x00,0x39,
0x00,0x3a,0x00,0x3a,0x00,0x3a,0x00,0x3a,0x00,0x3a,0x00,0x3a,0x00,0x3a,0x00,0x3a,
0x00,0x3a,0x00,0x3a,0x00,0x3a,0x00,0x3a,0x00,0x3a,0x00,0x3a,0x00,0x3a,0x00,0x3a,
0x00,0x3a,0x00,0x3a,0x00,0x3a,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,
0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,
0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,
0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,
0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,
0x00,0x3c,0x00,0x00,0x00,0x31,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x10,0x00,0x00,0x33,0x00,0x33,0x00,0x33,0x00,0x33,0x00,0x33,0x00,0x33,0x00,
0x33,0x00,0x33,0x00,0x33,0x00,0x33,0x00,0x33,0x00,0x33,0x00,0x33,0x00,0x33,0x00,
0x33,0x00,0x33,0x00,0x00,0x00,0x33,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x11,0x00,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,
0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x3c,
0x00,0x3c,0x00,0x3c,0x00,0x3c,0x00,0x00,0x01,0x57,0x00,0x01,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0xa3,0x00,0x00,0x40,0x00,0x40,0x00,0x40,0x00,0x40,0x00,
0x40,0x00,0x40,0x00,0x40,0x00,0x40,0x00,0x40,0x00,0x40,0x00,0x40,0x00,0x40,0x00,
0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,
0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,
0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,
0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,0x46,0x00,0x47,0x00,0x47,0x00,0x47,0x00,
0x47,0x00,0x47,0x00,0x47,0x00,0x47,0x00,0x47,0x00,0x47,0x00,0x47,0x00,0x47,0x00,
0x47,0x00,0x47,0x00,0x47,0x00,0x47,0x00,0x47,0x00,0x48,0x00,0x48,0x00,0x48,0x00,
0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,
0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,
0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,0x48,0x00,
0x48,0x00,0x48,0x00,0x49,0x00,0x49,0x00,0x49,0x00,0x49,0x00,0x49,0x00,0x49,0x00,
0x49,0x00,0x49,0x00,0x49,0x00,0x49,0x00,0x49,0x00,0x49,0x00,0x49,0x00,0x49,0x00,
0x49,0x00,0x49,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,
0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,
0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,
0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,
0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,
0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,
0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,
0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x00,0x00,
0x31,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x42,
0x00,0x42,0x00,0x42,0x00,0x42,0x00,0x42,0x00,0x42,0x00,0x42,0x00,0x42,0x00,0x42,
0x00,0x42,0x00,0x42,0x00,0x42,0x00,0x42,0x00,0x42,0x00,0x42,0x00,0x42,0x00,0x00,
0x00,0x33,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x11,0x00,0x00,
0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,
0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,0x4b,0x00,
0x4b,0x00,0x00,0x00,0x87,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x3b,0x00,0x00,0x4f,0x00,0x4f,0x00,0x4f,0x00,0x4f,0x00,0x4f,0x00,0x4f,0x00,0x4f,
0x00,0x4f,0x00,0x4f,0x00,0x4f,0x00,0x4f,0x00,0x4f,0x00,0x50,0x00,0x50,0x00,0x50,
0x00,0x50,0x00,0x50,0x00,0x50,0x00,0x50,0x00,0x50,0x00,0x50,0x00,0x50,0x00,0x57,
0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,
0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,
0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,
0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,
0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x57,0x00,0x00,0x00,0x2b,0x00,0x01,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0d,0x00,0x00,0x51,0x00,0x51,0x00,0x51,0x00,
0x51,0x00,0x51,0x00,0x51,0x00,0x51,0x00,0x51,0x00,0x51,0x00,0x51,0x00,0x51,0x00,
0x51,0x00,0x51,0x00,0x00,0x00,0x33,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x11,0x00,0x00,0x51,0x00,0x51,0x00,0x51,0x00,0x51,0x00,0x52,0x00,0x52,
0x00,0x52,0x00,0x52,0x00,0x52,0x00,0x52,0x00,0x52,0x00,0x52,0x00,0x52,0x00,0x53,
0x00,0x53,0x00,0x53,0x00,0x53,0x4c,0x56,0x41,0x52,0x00,0x00,0x00,0xa9,0x00,0x00,
0x00,0x0a,0x00,0x02,0x72,0x31,0x00,0x02,0x72,0x32,0x00,0x03,0x61,0x72,0x79,0x00,
0x08,0x73,0x68,0x75,0x66,0x66,0x6c,0x65,0x64,0x00,0x01,0x78,0x00,0x01,0x26,0x00,
0x04,0x61,0x72,0x79,0x31,0x00,0x08,0x73,0x68,0x75,0x66,0x66,0x6c,0x65,0x31,0x00,
0x04,0x61,0x72,0x79,0x32,0x00,0x08,0x73,0x68,0x75,0x66,0x66,0x6c,0x65,0x32,0x00,
0x00,0x00,0x01,0x00,0x01,0x00,0x02,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x02,0x00,
0x00,0x00,0x01,0x00,0x01,0x00,0x02,0x00,0x02,0x00,0x01,0x00,0x03,0x00,0x02,0x00,
0x04,0x00,0x01,0x00,0x05,0x00,0x02,0x00,0x02,0x00,0x01,0x00,0x04,0x00,0x01,0x00,
0x05,0x00,0x02,0x00,0x06,0x00,0x01,0x00,0x07,0x00,0x02,0x00,0x08,0x00,0x03,0x00,
0x09,0x00,0x04,0x00,0x04,0x00,0x01,0x00,0x05,0x00,0x02,0x00,0x06,0x00,0x01,0x00,
0x08,0x00,0x02,0x00,0x04,0x00,0x01,0x00,0x05,0x00,0x02,0x00,0x05,0x00,0x01,0x45,
0x4e,0x44,0x00,0x00,0x00,0x00,0x08,
};
void _mruby_random_gem_test(state *mrb);
void GENERATED_TMP__mruby_random_gem_init(state *mrb);
void GENERATED_TMP__mruby_random_gem_final(state *mrb);
void _init_test_driver(state *mrb, _bool verbose);
void _t_pass_result(state *dst, state *src);
void GENERATED_TMP__mruby_random_gem_test(state *mrb) {
  state *mrb2;
  int ai;
  ai = _gc_arena_save(mrb);
  mrb2 = _open_core(_default_allocf, NULL);
  if (mrb2 == NULL) {
    fprintf(stderr, "Invalid state, exiting %s", __FUNCTION__);
    exit(EXIT_FAILURE);
  }
  GENERATED_TMP__mruby_random_gem_init(mrb2);
  _state_atexit(mrb2, GENERATED_TMP__mruby_random_gem_final);
  _init_test_driver(mrb2, _test(_gv_get(mrb, _intern_lit(mrb, "$mrbtest_verbose"))));
  _load_irep(mrb2, mrbtest_assert_irep);
  if (mrb2->exc) {
    _print_error(mrb2);
    _close(mrb2);
    exit(EXIT_FAILURE);
  }
  _const_set(mrb2, _obj_value(mrb2->object_class), _intern_lit(mrb2, "GEMNAME"), _str_new(mrb2, "mruby-random", 12));
  _load_irep(mrb2, gem_test_irep_mruby_random_0);
  
  _t_pass_result(mrb, mrb2);
  _close(mrb2);
  _gc_arena_restore(mrb, ai);
}
