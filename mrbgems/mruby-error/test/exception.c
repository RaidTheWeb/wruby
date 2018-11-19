#include <mruby.h>
#include <mruby/error.h>
#include <mruby/array.h>

static value
protect_cb(state *mrb, value b)
{
  return _yield_argv(mrb, b, 0, NULL);
}

static value
run_protect(state *mrb, value self)
{
  value b;
  value ret[2];
  _bool state;
  _get_args(mrb, "&", &b);
  ret[0] = _protect(mrb, protect_cb, b, &state);
  ret[1] = _bool_value(state);
  return _ary_new_from_values(mrb, 2, ret);
}

static value
run_ensure(state *mrb, value self)
{
  value b, e;
  _get_args(mrb, "oo", &b, &e);
  return _ensure(mrb, protect_cb, b, protect_cb, e);
}

static value
run_rescue(state *mrb, value self)
{
  value b, r;
  _get_args(mrb, "oo", &b, &r);
  return _rescue(mrb, protect_cb, b, protect_cb, r);
}

static value
run_rescue_exceptions(state *mrb, value self)
{
  value b, r;
  struct RClass *cls[1];
  _get_args(mrb, "oo", &b, &r);
  cls[0] = E_TYPE_ERROR;
  return _rescue_exceptions(mrb, protect_cb, b, protect_cb, r, 1, cls);
}

void
_mruby_error_gem_test(state *mrb)
{
  struct RClass *cls;

  cls = _define_class(mrb, "ExceptionTest", mrb->object_class);
  _define_module_function(mrb, cls, "_protect", run_protect, MRB_ARGS_NONE() | MRB_ARGS_BLOCK());
  _define_module_function(mrb, cls, "_ensure", run_ensure, MRB_ARGS_REQ(2));
  _define_module_function(mrb, cls, "_rescue", run_rescue, MRB_ARGS_REQ(2));
  _define_module_function(mrb, cls, "_rescue_exceptions", run_rescue_exceptions, MRB_ARGS_REQ(2));
}
