#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/variable.h"
#include "mruby/proc.h"
#include "mruby/string.h"

static struct RObject *
method_object_alloc(state *mrb, struct RClass *mclass)
{
  return (struct RObject*)_obj_alloc(mrb, MRB_TT_OBJECT, mclass);
}

static value
unbound_method_bind(state *mrb, value self)
{
  struct RObject *me;
  value owner = _iv_get(mrb, self, _intern_lit(mrb, "@owner"));
  value name = _iv_get(mrb, self, _intern_lit(mrb, "@name"));
  value proc = _iv_get(mrb, self, _intern_lit(mrb, "proc"));
  value klass = _iv_get(mrb, self, _intern_lit(mrb, "@klass"));
  value recv;

  _get_args(mrb, "o", &recv);

  if (_type(owner) != MRB_TT_MODULE &&
      _class_ptr(owner) != _obj_class(mrb, recv) &&
      !_obj_is_kind_of(mrb, recv, _class_ptr(owner))) {
        if (_type(owner) == MRB_TT_SCLASS) {
          _raise(mrb, E_TYPE_ERROR, "singleton method called for a different object");
        } else {
          const char *s = _class_name(mrb, _class_ptr(owner));
          _raisef(mrb, E_TYPE_ERROR, "bind argument must be an instance of %S", _str_new_static(mrb, s, strlen(s)));
        }
  }
  me = method_object_alloc(mrb, _class_get(mrb, "Method"));
  _obj_iv_set(mrb, me, _intern_lit(mrb, "@owner"), owner);
  _obj_iv_set(mrb, me, _intern_lit(mrb, "@recv"), recv);
  _obj_iv_set(mrb, me, _intern_lit(mrb, "@name"), name);
  _obj_iv_set(mrb, me, _intern_lit(mrb, "proc"), proc);
  _obj_iv_set(mrb, me, _intern_lit(mrb, "@klass"), klass);

  return _obj_value(me);
}

#define IV_GET(value, name) _iv_get(mrb, value, _intern_lit(mrb, name))
static value
method_eql(state *mrb, value self)
{
  value other, receiver, orig_proc, other_proc;
  struct RClass *owner, *klass;
  struct RProc *orig_rproc, *other_rproc;

  _get_args(mrb, "o", &other);
  if (!_obj_is_instance_of(mrb, other, _class(mrb, self)))
    return _false_value();

  if (_class(mrb, self) != _class(mrb, other))
    return _false_value();

  klass = _class_ptr(IV_GET(self, "@klass"));
  if (klass != _class_ptr(IV_GET(other, "@klass")))
    return _false_value();

  owner = _class_ptr(IV_GET(self, "@owner"));
  if (owner != _class_ptr(IV_GET(other, "@owner")))
    return _false_value();

  receiver = IV_GET(self, "@recv");
  if (!_obj_equal(mrb, receiver, IV_GET(other, "@recv")))
    return _false_value();

  orig_proc = IV_GET(self, "proc");
  other_proc = IV_GET(other, "proc");
  if (_nil_p(orig_proc) && _nil_p(other_proc)) {
    if (_symbol(IV_GET(self, "@name")) == _symbol(IV_GET(other, "@name")))
      return _true_value();
    else
      return _false_value();
  }

  if (_nil_p(orig_proc))
    return _false_value();
  if (_nil_p(other_proc))
    return _false_value();

  orig_rproc = _proc_ptr(orig_proc);
  other_rproc = _proc_ptr(other_proc);
  if (MRB_PROC_CFUNC_P(orig_rproc)) {
    if (!MRB_PROC_CFUNC_P(other_rproc))
      return _false_value();
    if (orig_rproc->body.func != other_rproc->body.func)
      return _false_value();
  }
  else {
    if (MRB_PROC_CFUNC_P(other_rproc))
      return _false_value();
    if (orig_rproc->body.irep != other_rproc->body.irep)
      return _false_value();
  }

  return _true_value();
}

#undef IV_GET

static value
method_call(state *mrb, value self)
{
  value proc = _iv_get(mrb, self, _intern_lit(mrb, "proc"));
  value name = _iv_get(mrb, self, _intern_lit(mrb, "@name"));
  value recv = _iv_get(mrb, self, _intern_lit(mrb, "@recv"));
  struct RClass *owner = _class_ptr(_iv_get(mrb, self, _intern_lit(mrb, "@owner")));
  _int argc;
  value *argv, ret, block;
  _sym orig_mid;

  _get_args(mrb, "*&", &argv, &argc, &block);
  orig_mid = mrb->c->ci->mid;
  mrb->c->ci->mid = _symbol(name);
  if (_nil_p(proc)) {
    value missing_argv = _ary_new_from_values(mrb, argc, argv);
    _ary_unshift(mrb, missing_argv, name);
    ret = _funcall_argv(mrb, recv, _intern_lit(mrb, "method_missing"), argc + 1, RARRAY_PTR(missing_argv));
  }
  else if (!_nil_p(block)) {
    /*
      workaround since `_yield_with_class` does not support passing block as parameter
      need new API that initializes `mrb->c->stack[argc+1]` with block passed by argument
    */
    ret = _funcall_with_block(mrb, recv, _symbol(name), argc, argv, block);
  }
  else {
    ret = _yield_with_class(mrb, proc, argc, argv, recv, owner);
  }
  mrb->c->ci->mid = orig_mid;
  return ret;
}

static value
method_unbind(state *mrb, value self)
{
  struct RObject *ume;
  value owner = _iv_get(mrb, self, _intern_lit(mrb, "@owner"));
  value name = _iv_get(mrb, self, _intern_lit(mrb, "@name"));
  value proc = _iv_get(mrb, self, _intern_lit(mrb, "proc"));
  value klass = _iv_get(mrb, self, _intern_lit(mrb, "@klass"));

  ume = method_object_alloc(mrb, _class_get(mrb, "UnboundMethod"));
  _obj_iv_set(mrb, ume, _intern_lit(mrb, "@owner"), owner);
  _obj_iv_set(mrb, ume, _intern_lit(mrb, "@recv"), _nil_value());
  _obj_iv_set(mrb, ume, _intern_lit(mrb, "@name"), name);
  _obj_iv_set(mrb, ume, _intern_lit(mrb, "proc"), proc);
  _obj_iv_set(mrb, ume, _intern_lit(mrb, "@klass"), klass);

  return _obj_value(ume);
}

static struct RProc *
method_search_vm(state *mrb, struct RClass **cp, _sym mid)
{
  _method_t m = _method_search_vm(mrb, cp, mid);
  if (MRB_METHOD_UNDEF_P(m))
    return NULL;
  if (MRB_METHOD_PROC_P(m))
    return MRB_METHOD_PROC(m);
  return _proc_new_cfunc(mrb, MRB_METHOD_FUNC(m));
}

static value
method_super_method(state *mrb, value self)
{
  value recv = _iv_get(mrb, self, _intern_lit(mrb, "@recv"));
  value klass = _iv_get(mrb, self, _intern_lit(mrb, "@klass"));
  value owner = _iv_get(mrb, self, _intern_lit(mrb, "@owner"));
  value name = _iv_get(mrb, self, _intern_lit(mrb, "@name"));
  struct RClass *super, *rklass;
  struct RProc *proc;
  struct RObject *me;

  switch (_type(klass)) {
    case MRB_TT_SCLASS:
      super = _class_ptr(klass)->super->super;
      break;
    case MRB_TT_ICLASS:
      super = _class_ptr(klass)->super;
      break;
    default:
      super = _class_ptr(owner)->super;
      break;
  }

  proc = method_search_vm(mrb, &super, _symbol(name));
  if (!proc)
    return _nil_value();

  rklass = super;
  while (super->tt == MRB_TT_ICLASS)
    super = super->c;

  me = method_object_alloc(mrb, _obj_class(mrb, self));
  _obj_iv_set(mrb, me, _intern_lit(mrb, "@owner"), _obj_value(super));
  _obj_iv_set(mrb, me, _intern_lit(mrb, "@recv"), recv);
  _obj_iv_set(mrb, me, _intern_lit(mrb, "@name"), name);
  _obj_iv_set(mrb, me, _intern_lit(mrb, "proc"), _obj_value(proc));
  _obj_iv_set(mrb, me, _intern_lit(mrb, "@klass"), _obj_value(rklass));

  return _obj_value(me);
}

static value
method_arity(state *mrb, value self)
{
  value proc = _iv_get(mrb, self, _intern_lit(mrb, "proc"));
  struct RProc *rproc;
  struct RClass *orig;
  value ret;

  if (_nil_p(proc))
    return _fixnum_value(-1);

  rproc = _proc_ptr(proc);
  orig = rproc->c;
  rproc->c = mrb->proc_class;
  ret = _funcall(mrb, proc, "arity", 0);
  rproc->c = orig;
  return ret;
}

static value
method_source_location(state *mrb, value self)
{
  value proc = _iv_get(mrb, self, _intern_lit(mrb, "proc"));
  struct RProc *rproc;
  struct RClass *orig;
  value ret;

  if (_nil_p(proc))
    return _nil_value();

  rproc = _proc_ptr(proc);
  orig = rproc->c;
  rproc->c = mrb->proc_class;
  ret = _funcall(mrb, proc, "source_location", 0);
  rproc->c = orig;
  return ret;
}

static value
method_parameters(state *mrb, value self)
{
  value proc = _iv_get(mrb, self, _intern_lit(mrb, "proc"));
  struct RProc *rproc;
  struct RClass *orig;
  value ret;

  if (_nil_p(proc)) {
    value rest = _symbol_value(_intern_lit(mrb, "rest"));
    value arest = _ary_new_from_values(mrb, 1, &rest);
    return _ary_new_from_values(mrb, 1, &arest);
  }

  rproc = _proc_ptr(proc);
  orig = rproc->c;
  rproc->c = mrb->proc_class;
  ret = _funcall(mrb, proc, "parameters", 0);
  rproc->c = orig;
  return ret;
}

static value
method_to_s(state *mrb, value self)
{
  value owner = _iv_get(mrb, self, _intern_lit(mrb, "@owner"));
  value klass = _iv_get(mrb, self, _intern_lit(mrb, "@klass"));
  value name = _iv_get(mrb, self, _intern_lit(mrb, "@name"));
  value str = _str_new_lit(mrb, "#<");
  struct RClass *rklass;

  _str_cat_cstr(mrb, str, _obj_classname(mrb, self));
  _str_cat_lit(mrb, str, ": ");
  rklass = _class_ptr(klass);
  if (_class_ptr(owner) == rklass) {
    _str_cat_str(mrb, str, _funcall(mrb, owner, "to_s", 0));
    _str_cat_lit(mrb, str, "#");
    _str_cat_str(mrb, str, _funcall(mrb, name, "to_s", 0));
  }
  else {
    _str_cat_cstr(mrb, str, _class_name(mrb, rklass));
    _str_cat_lit(mrb, str, "(");
    _str_cat_str(mrb, str, _funcall(mrb, owner, "to_s", 0));
    _str_cat_lit(mrb, str, ")#");
    _str_cat_str(mrb, str, _funcall(mrb, name, "to_s", 0));
  }
  _str_cat_lit(mrb, str, ">");
  return str;
}

static void
_search_method_owner(state *mrb, struct RClass *c, value obj, _sym name, struct RClass **owner, struct RProc **proc, _bool unbound)
{
  value ret;
  const char *s;

  *owner = c;
  *proc = method_search_vm(mrb, owner, name);
  if (!*proc) {
    if (unbound) {
      goto name_error;
    }
    if (!_respond_to(mrb, obj, _intern_lit(mrb, "respond_to_missing?"))) {
      goto name_error;
    }
    ret = _funcall(mrb, obj, "respond_to_missing?", 2, _symbol_value(name), _true_value());
    if (!_test(ret)) {
      goto name_error;
    }
    *owner = c;
  }

  while ((*owner)->tt == MRB_TT_ICLASS)
    *owner = (*owner)->c;

  return;

name_error:
  s = _class_name(mrb, c);
  _raisef(
    mrb, E_NAME_ERROR,
    "undefined method `%S' for class `%S'",
    _sym2str(mrb, name),
    _str_new_static(mrb, s, strlen(s))
  );
}

static value
_kernel_method(state *mrb, value self)
{
  struct RClass *owner;
  struct RProc *proc;
  struct RObject *me;
  _sym name;

  _get_args(mrb, "n", &name);

  _search_method_owner(mrb, _class(mrb, self), self, name, &owner, &proc, FALSE);

  me = method_object_alloc(mrb, _class_get(mrb, "Method"));
  _obj_iv_set(mrb, me, _intern_lit(mrb, "@owner"), _obj_value(owner));
  _obj_iv_set(mrb, me, _intern_lit(mrb, "@recv"), self);
  _obj_iv_set(mrb, me, _intern_lit(mrb, "@name"), _symbol_value(name));
  _obj_iv_set(mrb, me, _intern_lit(mrb, "proc"), proc ? _obj_value(proc) : _nil_value());
  _obj_iv_set(mrb, me, _intern_lit(mrb, "@klass"), _obj_value(_class(mrb, self)));

  return _obj_value(me);
}

static value
_module_instance_method(state *mrb, value self)
{
  struct RClass *owner;
  struct RProc *proc;
  struct RObject *ume;
  _sym name;

  _get_args(mrb, "n", &name);

  _search_method_owner(mrb, _class_ptr(self), self, name, &owner, &proc, TRUE);

  ume = method_object_alloc(mrb, _class_get(mrb, "UnboundMethod"));
  _obj_iv_set(mrb, ume, _intern_lit(mrb, "@owner"), _obj_value(owner));
  _obj_iv_set(mrb, ume, _intern_lit(mrb, "@recv"), _nil_value());
  _obj_iv_set(mrb, ume, _intern_lit(mrb, "@name"), _symbol_value(name));
  _obj_iv_set(mrb, ume, _intern_lit(mrb, "proc"), proc ? _obj_value(proc) : _nil_value());
  _obj_iv_set(mrb, ume, _intern_lit(mrb, "@klass"), self);

  return _obj_value(ume);
}

void
_mruby_method_gem_init(state* mrb)
{
  struct RClass *unbound_method = _define_class(mrb, "UnboundMethod", mrb->object_class);
  struct RClass *method = _define_class(mrb, "Method", mrb->object_class);

  _undef_class_method(mrb, unbound_method, "new");
  _define_method(mrb, unbound_method, "bind", unbound_method_bind, MRB_ARGS_REQ(1));
  _define_method(mrb, unbound_method, "super_method", method_super_method, MRB_ARGS_NONE());
  _define_method(mrb, unbound_method, "==", method_eql, MRB_ARGS_REQ(1));
  _define_alias(mrb,  unbound_method, "eql?", "==");
  _define_method(mrb, unbound_method, "to_s", method_to_s, MRB_ARGS_NONE());
  _define_method(mrb, unbound_method, "inspect", method_to_s, MRB_ARGS_NONE());
  _define_method(mrb, unbound_method, "arity", method_arity, MRB_ARGS_NONE());
  _define_method(mrb, unbound_method, "source_location", method_source_location, MRB_ARGS_NONE());
  _define_method(mrb, unbound_method, "parameters", method_parameters, MRB_ARGS_NONE());

  _undef_class_method(mrb, method, "new");
  _define_method(mrb, method, "==", method_eql, MRB_ARGS_REQ(1));
  _define_alias(mrb,  method, "eql?", "==");
  _define_method(mrb, method, "to_s", method_to_s, MRB_ARGS_NONE());
  _define_method(mrb, method, "inspect", method_to_s, MRB_ARGS_NONE());
  _define_method(mrb, method, "call", method_call, MRB_ARGS_ANY());
  _define_alias(mrb,  method, "[]", "call");
  _define_method(mrb, method, "unbind", method_unbind, MRB_ARGS_NONE());
  _define_method(mrb, method, "super_method", method_super_method, MRB_ARGS_NONE());
  _define_method(mrb, method, "arity", method_arity, MRB_ARGS_NONE());
  _define_method(mrb, method, "source_location", method_source_location, MRB_ARGS_NONE());
  _define_method(mrb, method, "parameters", method_parameters, MRB_ARGS_NONE());

  _define_method(mrb, mrb->kernel_module, "method", _kernel_method, MRB_ARGS_REQ(1));

  _define_method(mrb, mrb->module_class, "instance_method", _module_instance_method, MRB_ARGS_REQ(1));
}

void
_mruby_method_gem_final(state* mrb)
{
}
