#include <mruby.h>
#include <mruby/error.h>
#include <mruby/array.h>

static value
protect_cb(state *mrb, value b)
{
  return yield_argv(mrb, b, 0, NULL);
}

static value
run_protect(state *mrb, value self)
{
  value b;
  value ret[2];
  bool state;
  get_args(mrb, "&", &b);
  ret[0] = protect(mrb, protect_cb, b, &state);
  ret[1] = bool_value(state);
  return ary_new_from_values(mrb, 2, ret);
}

static value
run_ensure(state *mrb, value self)
{
  value b, e;
  get_args(mrb, "oo", &b, &e);
  return ensure(mrb, protect_cb, b, protect_cb, e);
}

static value
run_rescue(state *mrb, value self)
{
  value b, r;
  get_args(mrb, "oo", &b, &r);
  return rescue(mrb, protect_cb, b, protect_cb, r);
}

static value
run_rescue_exceptions(state *mrb, value self)
{
  value b, r;
  struct RClass *cls[1];
  get_args(mrb, "oo", &b, &r);
  cls[0] = E_TYPE_ERROR;
  return rescue_exceptions(mrb, protect_cb, b, protect_cb, r, 1, cls);
}

void
mruby_error_gem_test(state *mrb)
{
  struct RClass *cls;

  cls = define_class(mrb, "ExceptionTest", mrb->object_class);
  define_module_function(mrb, cls, "protect", run_protect, ARGS_NONE() | ARGS_BLOCK());
  define_module_function(mrb, cls, "ensure", run_ensure, ARGS_REQ(2));
  define_module_function(mrb, cls, "rescue", run_rescue, ARGS_REQ(2));
  define_module_function(mrb, cls, "rescue_exceptions", run_rescue_exceptions, ARGS_REQ(2));
}
