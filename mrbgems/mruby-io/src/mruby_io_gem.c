#include "mruby.h"

void init_io(state *mrb);
void init_file(state *mrb);
void init_file_test(state *mrb);

#define DONE gc_arena_restore(mrb, 0)

void
mruby_io_gem_init(state* mrb)
{
  init_io(mrb); DONE;
  init_file(mrb); DONE;
  init_file_test(mrb); DONE;
}

void
mruby_io_gem_final(state* mrb)
{
}
