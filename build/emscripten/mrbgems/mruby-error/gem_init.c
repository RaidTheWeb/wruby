/*
 * This file is loading the irep
 * Ruby GEM code.
 *
 * IMPORTANT:
 *   This file was generated!
 *   All manual changes will get lost.
 */
#include <mruby.h>
void _mruby_error_gem_init(state *mrb);
void _mruby_error_gem_final(state *mrb);

void GENERATED_TMP__mruby_error_gem_init(state *mrb) {
  int ai = _gc_arena_save(mrb);
  _mruby_error_gem_init(mrb);
  _gc_arena_restore(mrb, ai);
}

void GENERATED_TMP__mruby_error_gem_final(state *mrb) {
  _mruby_error_gem_final(mrb);
}
