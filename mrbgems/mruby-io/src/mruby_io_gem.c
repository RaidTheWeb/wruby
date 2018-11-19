#include "mruby.h"

void _init_io(_state *mrb);
void _init_file(_state *mrb);
void _init_file_test(_state *mrb);

#define DONE _gc_arena_restore(mrb, 0)

void
_mruby_io_gem_init(_state* mrb)
{
  _init_io(mrb); DONE;
  _init_file(mrb); DONE;
  _init_file_test(mrb); DONE;
}

void
_mruby_io_gem_final(_state* mrb)
{
}
