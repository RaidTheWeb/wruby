#include <mruby.h>
#include <stdio.h>

static $value
$c_method($state *mrb, $value self)
{
  puts("A C Extension");
  return self;
}

void
$c_and_ruby_extension_example_gem_init($state* mrb) {
  struct RClass *class_cextension = $define_module(mrb, "CRubyExtension");
  $define_class_method(mrb, class_cextension, "c_method", $c_method, $ARGS_NONE());
}

void
$c_and_ruby_extension_example_gem_final($state* mrb) {
  /* finalizer */
}
