#include <mruby.h>
#include <stdio.h>

static value
_c_method(state *mrb, value self)
{
  puts("A C Extension");
  return self;
}

void
_c_extension_example_gem_init(state* mrb) {
  struct RClass *class_cextension = _define_module(mrb, "CExtension");
  _define_class_method(mrb, class_cextension, "c_method", _c_method, MRB_ARGS_NONE());
}

void
_c_extension_example_gem_final(state* mrb) {
  /* finalizer */
}
