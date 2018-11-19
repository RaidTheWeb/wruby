#include <mruby.h>
#include <mruby/variable.h>

void
_init_version(state* mrb)
{
  value mruby_version = _str_new_lit(mrb, MRUBY_VERSION);

  _define_global_const(mrb, "RUBY_VERSION", _str_new_lit(mrb, MRUBY_RUBY_VERSION));
  _define_global_const(mrb, "RUBY_ENGINE", _str_new_lit(mrb, MRUBY_RUBY_ENGINE));
  _define_global_const(mrb, "RUBY_ENGINE_VERSION", mruby_version);
  _define_global_const(mrb, "MRUBY_VERSION", mruby_version);
  _define_global_const(mrb, "MRUBY_RELEASE_NO", _fixnum_value(MRUBY_RELEASE_NO));
  _define_global_const(mrb, "MRUBY_RELEASE_DATE", _str_new_lit(mrb, MRUBY_RELEASE_DATE));
  _define_global_const(mrb, "MRUBY_DESCRIPTION", _str_new_lit(mrb, MRUBY_DESCRIPTION));
  _define_global_const(mrb, "MRUBY_COPYRIGHT", _str_new_lit(mrb, MRUBY_COPYRIGHT));
}
