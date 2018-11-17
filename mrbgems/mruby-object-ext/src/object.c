#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/proc.h>

/*
 *  call-seq:
 *     nil.to_a    -> []
 *
 *  Always returns an empty array.
 */

static value
nil_to_a(state *mrb, value obj)
{
  return ary_new(mrb);
}

#ifndef WITHOUT_FLOAT
/*
 *  call-seq:
 *     nil.to_f    -> 0.0
 *
 *  Always returns zero.
 */

static value
nil_to_f(state *mrb, value obj)
{
  return float_value(mrb, 0.0);
}
#endif

/*
 *  call-seq:
 *     nil.to_i    -> 0
 *
 *  Always returns zero.
 */

static value
nil_to_i(state *mrb, value obj)
{
  return fixnum_value(0);
}

/*
 *  call-seq:
 *     obj.instance_exec(arg...) {|var...| block }                       -> obj
 *
 *  Executes the given block within the context of the receiver
 *  (_obj_). In order to set the context, the variable +self+ is set
 *  to _obj_ while the code is executing, giving the code access to
 *  _obj_'s instance variables.  Arguments are passed as block parameters.
 *
 *     class KlassWithSecret
 *       def initialize
 *         @secret = 99
 *       end
 *     end
 *     k = KlassWithSecret.new
 *     k.instance_exec(5) {|x| @secret+x }   #=> 104
 */

static value
obj_instance_exec(state *mrb, value self)
{
  const value *argv;
  int argc;
  value blk;
  struct RClass *c;

  get_args(mrb, "*&", &argv, &argc, &blk);

  if (nil_p(blk)) {
    raise(mrb, E_ARGUMENT_ERROR, "no block given");
  }

  switch (type(self)) {
  case TT_SYMBOL:
  case TT_FIXNUM:
#ifndef WITHOUT_FLOAT
  case TT_FLOAT:
#endif
    c = NULL;
    break;
  default:
    c = class_ptr(singleton_class(mrb, self));
    break;
  }
  mrb->c->ci->target_class = c;
  return yield_cont(mrb, blk, self, argc, argv);
}

void
mruby_object_ext_gem_init(state* mrb)
{
  struct RClass * n = mrb->nil_class;

  define_method(mrb, n, "to_a", nil_to_a,       ARGS_NONE());
#ifndef WITHOUT_FLOAT
  define_method(mrb, n, "to_f", nil_to_f,       ARGS_NONE());
#endif
  define_method(mrb, n, "to_i", nil_to_i,       ARGS_NONE());

  define_method(mrb, mrb->kernel_module, "instance_exec", obj_instance_exec, ARGS_ANY() | ARGS_BLOCK());
}

void
mruby_object_ext_gem_final(state* mrb)
{
}
