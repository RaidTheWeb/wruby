#include "mruby.h"
#include "mruby/class.h"
#include "mruby/string.h"

static value
mod_name(state *mrb, value self)
{
  value name = class_path(mrb, class_ptr(self));
  return nil_p(name)? name : str_dup(mrb, name);
}

static value
mod_singleton_class_p(state *mrb, value self)
{
  return bool_value(type(self) == TT_SCLASS);
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

static value
mod_module_exec(state *mrb, value self)
{
  const value *argv;
  int argc;
  value blk;

  get_args(mrb, "*&", &argv, &argc, &blk);

  if (nil_p(blk)) {
    raise(mrb, E_ARGUMENT_ERROR, "no block given");
  }

  mrb->c->ci->target_class = class_ptr(self);
  return yield_cont(mrb, blk, self, argc, argv);
}

void
mruby_class_ext_gem_init(state *mrb)
{
  struct RClass *mod = mrb->module_class;

  define_method(mrb, mod, "name", mod_name, ARGS_NONE());
  define_method(mrb, mod, "singleton_class?", mod_singleton_class_p, ARGS_NONE());
  define_method(mrb, mod, "module_exec", mod_module_exec, ARGS_ANY()|ARGS_BLOCK());
  define_method(mrb, mod, "class_exec", mod_module_exec, ARGS_ANY()|ARGS_BLOCK());
}

void
mruby_class_ext_gem_final(state *mrb)
{
}
