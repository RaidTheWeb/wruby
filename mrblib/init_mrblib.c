#include <mruby.h>
#include <mruby/irep.h>

extern const uint8_t mrblib_irep[];

void
_init_mrblib(state *mrb)
{
  _load_irep(mrb, mrblib_irep);
}

