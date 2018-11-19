#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/variable.h"
#include "mruby/proc.h"
#include "mruby/string.h"

static struct RObject *
method_object_alloc($state *mrb, struct RClass *mclass)
{
  return (struct RObject*)$obj_alloc(mrb, $TT_OBJECT, mclass);
}

static $value
unbound_method_bind($state *mrb, $value self)
{
  struct RObject *me;
  $value owner = $iv_get(mrb, self, $intern_lit(mrb, "@owner"));
  $value name = $iv_get(mrb, self, $intern_lit(mrb, "@name"));
  $value proc = $iv_get(mrb, self, $intern_lit(mrb, "proc"));
  $value klass = $iv_get(mrb, self, $intern_lit(mrb, "@klass"));
  $value recv;

  $get_args(mrb, "o", &recv);

  if ($type(owner) != $TT_MODULE &&
      $class_ptr(owner) != $obj_class(mrb, recv) &&
      !$obj_is_kind_of(mrb, recv, $class_ptr(owner))) {
        if ($type(owner) == $TT_SCLASS) {
          $raise(mrb, E_TYPE_ERROR, "singleton method called for a different object");
        } else {
          const char *s = $class_name(mrb, $class_ptr(owner));
          $raisef(mrb, E_TYPE_ERROR, "bind argument must be an instance of %S", $str_new_static(mrb, s, strlen(s)));
        }
  }
  me = method_object_alloc(mrb, $class_get(mrb, "Method"));
  $obj_iv_set(mrb, me, $intern_lit(mrb, "@owner"), owner);
  $obj_iv_set(mrb, me, $intern_lit(mrb, "@recv"), recv);
  $obj_iv_set(mrb, me, $intern_lit(mrb, "@name"), name);
  $obj_iv_set(mrb, me, $intern_lit(mrb, "proc"), proc);
  $obj_iv_set(mrb, me, $intern_lit(mrb, "@klass"), klass);

  return $obj_value(me);
}

#define IV_GET(value, name) $iv_get(mrb, value, $intern_lit(mrb, name))
static $value
method_eql($state *mrb, $value self)
{
  $value other, receiver, orig_proc, other_proc;
  struct RClass *owner, *klass;
  struct RProc *orig_rproc, *other_rproc;

  $get_args(mrb, "o", &other);
  if (!$obj_is_instance_of(mrb, other, $class(mrb, self)))
    return $false_value();

  if ($class(mrb, self) != $class(mrb, other))
    return $false_value();

  klass = $class_ptr(IV_GET(self, "@klass"));
  if (klass != $class_ptr(IV_GET(other, "@klass")))
    return $false_value();

  owner = $class_ptr(IV_GET(self, "@owner"));
  if (owner != $class_ptr(IV_GET(other, "@owner")))
    return $false_value();

  receiver = IV_GET(self, "@recv");
  if (!$obj_equal(mrb, receiver, IV_GET(other, "@recv")))
    return $false_value();

  orig_proc = IV_GET(self, "proc");
  other_proc = IV_GET(other, "proc");
  if ($nil_p(orig_proc) && $nil_p(other_proc)) {
    if ($symbol(IV_GET(self, "@name")) == $symbol(IV_GET(other, "@name")))
      return $true_value();
    else
      return $false_value();
  }

  if ($nil_p(orig_proc))
    return $false_value();
  if ($nil_p(other_proc))
    return $false_value();

  orig_rproc = $proc_ptr(orig_proc);
  other_rproc = $proc_ptr(other_proc);
  if ($PROC_CFUNC_P(orig_rproc)) {
    if (!$PROC_CFUNC_P(other_rproc))
      return $false_value();
    if (orig_rproc->body.func != other_rproc->body.func)
      return $false_value();
  }
  else {
    if ($PROC_CFUNC_P(other_rproc))
      return $false_value();
    if (orig_rproc->body.irep != other_rproc->body.irep)
      return $false_value();
  }

  return $true_value();
}

#undef IV_GET

static $value
method_call($state *mrb, $value self)
{
  $value proc = $iv_get(mrb, self, $intern_lit(mrb, "proc"));
  $value name = $iv_get(mrb, self, $intern_lit(mrb, "@name"));
  $value recv = $iv_get(mrb, self, $intern_lit(mrb, "@recv"));
  struct RClass *owner = $class_ptr($iv_get(mrb, self, $intern_lit(mrb, "@owner")));
  $int argc;
  $value *argv, ret, block;
  $sym orig_mid;

  $get_args(mrb, "*&", &argv, &argc, &block);
  orig_mid = mrb->c->ci->mid;
  mrb->c->ci->mid = $symbol(name);
  if ($nil_p(proc)) {
    $value missing_argv = $ary_new_from_values(mrb, argc, argv);
    $ary_unshift(mrb, missing_argv, name);
    ret = $funcall_argv(mrb, recv, $intern_lit(mrb, "method_missing"), argc + 1, RARRAY_PTR(missing_argv));
  }
  else if (!$nil_p(block)) {
    /*
      workaround since `$yield_with_class` does not support passing block as parameter
      need new API that initializes `mrb->c->stack[argc+1]` with block passed by argument
    */
    ret = $funcall_with_block(mrb, recv, $symbol(name), argc, argv, block);
  }
  else {
    ret = $yield_with_class(mrb, proc, argc, argv, recv, owner);
  }
  mrb->c->ci->mid = orig_mid;
  return ret;
}

static $value
method_unbind($state *mrb, $value self)
{
  struct RObject *ume;
  $value owner = $iv_get(mrb, self, $intern_lit(mrb, "@owner"));
  $value name = $iv_get(mrb, self, $intern_lit(mrb, "@name"));
  $value proc = $iv_get(mrb, self, $intern_lit(mrb, "proc"));
  $value klass = $iv_get(mrb, self, $intern_lit(mrb, "@klass"));

  ume = method_object_alloc(mrb, $class_get(mrb, "UnboundMethod"));
  $obj_iv_set(mrb, ume, $intern_lit(mrb, "@owner"), owner);
  $obj_iv_set(mrb, ume, $intern_lit(mrb, "@recv"), $nil_value());
  $obj_iv_set(mrb, ume, $intern_lit(mrb, "@name"), name);
  $obj_iv_set(mrb, ume, $intern_lit(mrb, "proc"), proc);
  $obj_iv_set(mrb, ume, $intern_lit(mrb, "@klass"), klass);

  return $obj_value(ume);
}

static struct RProc *
method_search_vm($state *mrb, struct RClass **cp, $sym mid)
{
  $method_t m = $method_search_vm(mrb, cp, mid);
  if ($METHOD_UNDEF_P(m))
    return NULL;
  if ($METHOD_PROC_P(m))
    return $METHOD_PROC(m);
  return $proc_new_cfunc(mrb, $METHOD_FUNC(m));
}

static $value
method_super_method($state *mrb, $value self)
{
  $value recv = $iv_get(mrb, self, $intern_lit(mrb, "@recv"));
  $value klass = $iv_get(mrb, self, $intern_lit(mrb, "@klass"));
  $value owner = $iv_get(mrb, self, $intern_lit(mrb, "@owner"));
  $value name = $iv_get(mrb, self, $intern_lit(mrb, "@name"));
  struct RClass *super, *rklass;
  struct RProc *proc;
  struct RObject *me;

  switch ($type(klass)) {
    case $TT_SCLASS:
      super = $class_ptr(klass)->super->super;
      break;
    case $TT_ICLASS:
      super = $class_ptr(klass)->super;
      break;
    default:
      super = $class_ptr(owner)->super;
      break;
  }

  proc = method_search_vm(mrb, &super, $symbol(name));
  if (!proc)
    return $nil_value();

  rklass = super;
  while (super->tt == $TT_ICLASS)
    super = super->c;

  me = method_object_alloc(mrb, $obj_class(mrb, self));
  $obj_iv_set(mrb, me, $intern_lit(mrb, "@owner"), $obj_value(super));
  $obj_iv_set(mrb, me, $intern_lit(mrb, "@recv"), recv);
  $obj_iv_set(mrb, me, $intern_lit(mrb, "@name"), name);
  $obj_iv_set(mrb, me, $intern_lit(mrb, "proc"), $obj_value(proc));
  $obj_iv_set(mrb, me, $intern_lit(mrb, "@klass"), $obj_value(rklass));

  return $obj_value(me);
}

static $value
method_arity($state *mrb, $value self)
{
  $value proc = $iv_get(mrb, self, $intern_lit(mrb, "proc"));
  struct RProc *rproc;
  struct RClass *orig;
  $value ret;

  if ($nil_p(proc))
    return $fixnum_value(-1);

  rproc = $proc_ptr(proc);
  orig = rproc->c;
  rproc->c = mrb->proc_class;
  ret = $funcall(mrb, proc, "arity", 0);
  rproc->c = orig;
  return ret;
}

static $value
method_source_location($state *mrb, $value self)
{
  $value proc = $iv_get(mrb, self, $intern_lit(mrb, "proc"));
  struct RProc *rproc;
  struct RClass *orig;
  $value ret;

  if ($nil_p(proc))
    return $nil_value();

  rproc = $proc_ptr(proc);
  orig = rproc->c;
  rproc->c = mrb->proc_class;
  ret = $funcall(mrb, proc, "source_location", 0);
  rproc->c = orig;
  return ret;
}

static $value
method_parameters($state *mrb, $value self)
{
  $value proc = $iv_get(mrb, self, $intern_lit(mrb, "proc"));
  struct RProc *rproc;
  struct RClass *orig;
  $value ret;

  if ($nil_p(proc)) {
    $value rest = $symbol_value($intern_lit(mrb, "rest"));
    $value arest = $ary_new_from_values(mrb, 1, &rest);
    return $ary_new_from_values(mrb, 1, &arest);
  }

  rproc = $proc_ptr(proc);
  orig = rproc->c;
  rproc->c = mrb->proc_class;
  ret = $funcall(mrb, proc, "parameters", 0);
  rproc->c = orig;
  return ret;
}

static $value
method_to_s($state *mrb, $value self)
{
  $value owner = $iv_get(mrb, self, $intern_lit(mrb, "@owner"));
  $value klass = $iv_get(mrb, self, $intern_lit(mrb, "@klass"));
  $value name = $iv_get(mrb, self, $intern_lit(mrb, "@name"));
  $value str = $str_new_lit(mrb, "#<");
  struct RClass *rklass;

  $str_cat_cstr(mrb, str, $obj_classname(mrb, self));
  $str_cat_lit(mrb, str, ": ");
  rklass = $class_ptr(klass);
  if ($class_ptr(owner) == rklass) {
    $str_cat_str(mrb, str, $funcall(mrb, owner, "to_s", 0));
    $str_cat_lit(mrb, str, "#");
    $str_cat_str(mrb, str, $funcall(mrb, name, "to_s", 0));
  }
  else {
    $str_cat_cstr(mrb, str, $class_name(mrb, rklass));
    $str_cat_lit(mrb, str, "(");
    $str_cat_str(mrb, str, $funcall(mrb, owner, "to_s", 0));
    $str_cat_lit(mrb, str, ")#");
    $str_cat_str(mrb, str, $funcall(mrb, name, "to_s", 0));
  }
  $str_cat_lit(mrb, str, ">");
  return str;
}

static void
$search_method_owner($state *mrb, struct RClass *c, $value obj, $sym name, struct RClass **owner, struct RProc **proc, $bool unbound)
{
  $value ret;
  const char *s;

  *owner = c;
  *proc = method_search_vm(mrb, owner, name);
  if (!*proc) {
    if (unbound) {
      goto name_error;
    }
    if (!$respond_to(mrb, obj, $intern_lit(mrb, "respond_to_missing?"))) {
      goto name_error;
    }
    ret = $funcall(mrb, obj, "respond_to_missing?", 2, $symbol_value(name), $true_value());
    if (!$test(ret)) {
      goto name_error;
    }
    *owner = c;
  }

  while ((*owner)->tt == $TT_ICLASS)
    *owner = (*owner)->c;

  return;

name_error:
  s = $class_name(mrb, c);
  $raisef(
    mrb, E_NAME_ERROR,
    "undefined method `%S' for class `%S'",
    $sym2str(mrb, name),
    $str_new_static(mrb, s, strlen(s))
  );
}

static $value
$kernel_method($state *mrb, $value self)
{
  struct RClass *owner;
  struct RProc *proc;
  struct RObject *me;
  $sym name;

  $get_args(mrb, "n", &name);

  $search_method_owner(mrb, $class(mrb, self), self, name, &owner, &proc, FALSE);

  me = method_object_alloc(mrb, $class_get(mrb, "Method"));
  $obj_iv_set(mrb, me, $intern_lit(mrb, "@owner"), $obj_value(owner));
  $obj_iv_set(mrb, me, $intern_lit(mrb, "@recv"), self);
  $obj_iv_set(mrb, me, $intern_lit(mrb, "@name"), $symbol_value(name));
  $obj_iv_set(mrb, me, $intern_lit(mrb, "proc"), proc ? $obj_value(proc) : $nil_value());
  $obj_iv_set(mrb, me, $intern_lit(mrb, "@klass"), $obj_value($class(mrb, self)));

  return $obj_value(me);
}

static $value
$module_instance_method($state *mrb, $value self)
{
  struct RClass *owner;
  struct RProc *proc;
  struct RObject *ume;
  $sym name;

  $get_args(mrb, "n", &name);

  $search_method_owner(mrb, $class_ptr(self), self, name, &owner, &proc, TRUE);

  ume = method_object_alloc(mrb, $class_get(mrb, "UnboundMethod"));
  $obj_iv_set(mrb, ume, $intern_lit(mrb, "@owner"), $obj_value(owner));
  $obj_iv_set(mrb, ume, $intern_lit(mrb, "@recv"), $nil_value());
  $obj_iv_set(mrb, ume, $intern_lit(mrb, "@name"), $symbol_value(name));
  $obj_iv_set(mrb, ume, $intern_lit(mrb, "proc"), proc ? $obj_value(proc) : $nil_value());
  $obj_iv_set(mrb, ume, $intern_lit(mrb, "@klass"), self);

  return $obj_value(ume);
}

void
$mruby_method_gem_init($state* mrb)
{
  struct RClass *unbound_method = $define_class(mrb, "UnboundMethod", mrb->object_class);
  struct RClass *method = $define_class(mrb, "Method", mrb->object_class);

  $undef_class_method(mrb, unbound_method, "new");
  $define_method(mrb, unbound_method, "bind", unbound_method_bind, $ARGS_REQ(1));
  $define_method(mrb, unbound_method, "super_method", method_super_method, $ARGS_NONE());
  $define_method(mrb, unbound_method, "==", method_eql, $ARGS_REQ(1));
  $define_alias(mrb,  unbound_method, "eql?", "==");
  $define_method(mrb, unbound_method, "to_s", method_to_s, $ARGS_NONE());
  $define_method(mrb, unbound_method, "inspect", method_to_s, $ARGS_NONE());
  $define_method(mrb, unbound_method, "arity", method_arity, $ARGS_NONE());
  $define_method(mrb, unbound_method, "source_location", method_source_location, $ARGS_NONE());
  $define_method(mrb, unbound_method, "parameters", method_parameters, $ARGS_NONE());

  $undef_class_method(mrb, method, "new");
  $define_method(mrb, method, "==", method_eql, $ARGS_REQ(1));
  $define_alias(mrb,  method, "eql?", "==");
  $define_method(mrb, method, "to_s", method_to_s, $ARGS_NONE());
  $define_method(mrb, method, "inspect", method_to_s, $ARGS_NONE());
  $define_method(mrb, method, "call", method_call, $ARGS_ANY());
  $define_alias(mrb,  method, "[]", "call");
  $define_method(mrb, method, "unbind", method_unbind, $ARGS_NONE());
  $define_method(mrb, method, "super_method", method_super_method, $ARGS_NONE());
  $define_method(mrb, method, "arity", method_arity, $ARGS_NONE());
  $define_method(mrb, method, "source_location", method_source_location, $ARGS_NONE());
  $define_method(mrb, method, "parameters", method_parameters, $ARGS_NONE());

  $define_method(mrb, mrb->kernel_module, "method", $kernel_method, $ARGS_REQ(1));

  $define_method(mrb, mrb->module_class, "instance_method", $module_instance_method, $ARGS_REQ(1));
}

void
$mruby_method_gem_final($state* mrb)
{
}
