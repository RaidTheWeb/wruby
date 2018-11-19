#include "mruby.h"
#include "mruby/class.h"
#include "mruby/string.h"

static _value
_mod_name(_state *mrb, _value self)
{
  _value name = _class_path(mrb, _class_ptr(self));
  return _nil_p(name)? name : _str_dup(mrb, name);
}

static _value
_mod_singleton_class_p(_state *mrb, _value self)
{
  return _bool_value(_type(self) == MRB_TT_SCLASS);
}

/*
 *  call-seq:
 *     module_exec(arg...) {|var...| block } -> obj
 *     class_exec(arg...) {|var...| block } -> obj
 *
 * Evaluates the given block in the context of the
 * class/module. The method defined in the block will belong
 * to the receiver. Any arguments passed to the method will be
 * passed to the block. This can be used if the block needs to
 * access instance variables.
 *
 *     class Thing
 *     end
 *     Thing.class_exec{
 *       def hello() "Hello there!" end
 *     }
 *     puts Thing.new.hello()
 */

static _value
_mod_module_exec(_state *mrb, _value self)
{
  const _value *argv;
  _int argc;
  _value blk;

  _get_args(mrb, "*&", &argv, &argc, &blk);

  if (_nil_p(blk)) {
    _raise(mrb, E_ARGUMENT_ERROR, "no block given");
  }

  mrb->c->ci->target_class = _class_ptr(self);
  return _yield_cont(mrb, blk, self, argc, argv);
}

void
_mruby_class_ext_gem_init(_state *mrb)
{
  struct RClass *mod = mrb->module_class;

  _define_method(mrb, mod, "name", _mod_name, MRB_ARGS_NONE());
  _define_method(mrb, mod, "singleton_class?", _mod_singleton_class_p, MRB_ARGS_NONE());
  _define_method(mrb, mod, "module_exec", _mod_module_exec, MRB_ARGS_ANY()|MRB_ARGS_BLOCK());
  _define_method(mrb, mod, "class_exec", _mod_module_exec, MRB_ARGS_ANY()|MRB_ARGS_BLOCK());
}

void
_mruby_class_ext_gem_final(_state *mrb)
{
}
