#include <mruby.h>
#include <mruby/irep.h>

extern const uint8_t mrblib_irep[];

void
init_mrblib(state *mrb)
{
  load_irep(mrb, mrblib_irep);
}

