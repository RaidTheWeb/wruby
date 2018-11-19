#include <mruby.h>
#include <mruby/variable.h>

void
$init_version($state* mrb)
{
  $value mruby_version = $str_new_lit(mrb, MRUBY_VERSION);

  $define_global_const(mrb, "RUBY_VERSION", $str_new_lit(mrb, MRUBY_RUBY_VERSION));
  $define_global_const(mrb, "RUBY_ENGINE", $str_new_lit(mrb, MRUBY_RUBY_ENGINE));
  $define_global_const(mrb, "RUBY_ENGINE_VERSION", mruby_version);
  $define_global_const(mrb, "MRUBY_VERSION", mruby_version);
  $define_global_const(mrb, "MRUBY_RELEASE_NO", $fixnum_value(MRUBY_RELEASE_NO));
  $define_global_const(mrb, "MRUBY_RELEASE_DATE", $str_new_lit(mrb, MRUBY_RELEASE_DATE));
  $define_global_const(mrb, "MRUBY_DESCRIPTION", $str_new_lit(mrb, MRUBY_DESCRIPTION));
  $define_global_const(mrb, "MRUBY_COPYRIGHT", $str_new_lit(mrb, MRUBY_COPYRIGHT));
}
