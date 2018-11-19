#include <mruby.h>
#include <mruby/proc.h>
#include <mruby/class.h>

static _value
return_func_name(_state *mrb, _value self)
{
  return _cfunc_env_get(mrb, 0);
}

static _value
proc_new_cfunc_with_env(_state *mrb, _value self)
{
  _sym n;
  _value n_val;
  _method_t m;
  struct RProc *p;
  _get_args(mrb, "n", &n);
  n_val = _symbol_value(n);
  p = _proc_new_cfunc_with_env(mrb, return_func_name, 1, &n_val);
  MRB_METHOD_FROM_PROC(m, p);
  _define_method_raw(mrb, _class_ptr(self), n, m);
  return self;
}

static _value
return_env(_state *mrb, _value self)
{
  _int idx;
  _get_args(mrb, "i", &idx);
  return _cfunc_env_get(mrb, idx);
}

static _value
cfunc_env_get(_state *mrb, _value self)
{
  _sym n;
  _value *argv; _int argc;
  _method_t m;
  struct RProc *p;
  _get_args(mrb, "na", &n, &argv, &argc);
  p = _proc_new_cfunc_with_env(mrb, return_env, argc, argv);
  MRB_METHOD_FROM_PROC(m, p);
  _define_method_raw(mrb, _class_ptr(self), n, m);
  return self;
}

static _value
cfunc_without_env(_state *mrb, _value self)
{
  return _cfunc_env_get(mrb, 0);
}

void _mruby_proc_ext_gem_test(_state *mrb)
{
  struct RClass *cls;

  cls = _define_class(mrb, "ProcExtTest", mrb->object_class);
  _define_module_function(mrb, cls, "_proc_new_cfunc_with_env", proc_new_cfunc_with_env, MRB_ARGS_REQ(1));
  _define_module_function(mrb, cls, "_cfunc_env_get", cfunc_env_get, MRB_ARGS_REQ(2));
  _define_module_function(mrb, cls, "cfunc_without_env", cfunc_without_env, MRB_ARGS_NONE());
}
