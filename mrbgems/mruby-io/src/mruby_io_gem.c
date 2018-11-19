#include "mruby.h"

void _init_io(state *mrb);
void _init_file(state *mrb);
void _init_file_test(state *mrb);

#define DONE _gc_arena_restore(mrb, 0)

void
_mruby_io_gem_init(state* mrb)
{
  _init_io(mrb); DONE;
  _init_file(mrb); DONE;
  _init_file_test(mrb); DONE;
}

void
_mruby_io_gem_final(state* mrb)
{
}
