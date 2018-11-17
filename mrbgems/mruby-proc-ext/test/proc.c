#include <mruby.h>
#include <mruby/proc.h>
#include <mruby/class.h>

static value
return_func_name(state *mrb, value self)
{
  return cfunc_env_get(mrb, 0);
}

static value
proc_new_cfunc_with_env(state *mrb, value self)
{
  sym n;
  value n_val;
  method_t m;
  struct RProc *p;
  get_args(mrb, "n", &n);
  n_val = symbol_value(n);
  p = proc_new_cfunc_with_env(mrb, return_func_name, 1, &n_val);
  METHOD_FROM_PROC(m, p);
  define_method_raw(mrb, class_ptr(self), n, m);
  return self;
}

static value
return_env(state *mrb, value self)
{
  int idx;
  get_args(mrb, "i", &idx);
  return cfunc_env_get(mrb, idx);
}

static value
cfunc_env_get(state *mrb, value self)
{
  sym n;
  value *argv; int argc;
  method_t m;
  struct RProc *p;
  get_args(mrb, "na", &n, &argv, &argc);
  p = proc_new_cfunc_with_env(mrb, return_env, argc, argv);
  METHOD_FROM_PROC(m, p);
  define_method_raw(mrb, class_ptr(self), n, m);
  return self;
}

static value
cfunc_without_env(state *mrb, value self)
{
  return cfunc_env_get(mrb, 0);
}

void mruby_proc_ext_gem_test(state *mrb)
{
  struct RClass *cls;

  cls = define_class(mrb, "ProcExtTest", mrb->object_class);
  define_module_function(mrb, cls, "proc_new_cfunc_with_env", proc_new_cfunc_with_env, ARGS_REQ(1));
  define_module_function(mrb, cls, "cfunc_env_get", cfunc_env_get, ARGS_REQ(2));
  define_module_function(mrb, cls, "cfunc_without_env", cfunc_without_env, ARGS_NONE());
}
