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
  return _ary_new(mrb);
}

#ifndef MRB_WITHOUT_FLOAT
/*
 *  call-seq:
 *     nil.to_f    -> 0.0
 *
 *  Always returns zero.
 */

static value
nil_to_f(state *mrb, value obj)
{
  return _float_value(mrb, 0.0);
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
  return _fixnum_value(0);
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
_obj_instance_exec(state *mrb, value self)
{
  const value *argv;
  _int argc;
  value blk;
  struct RClass *c;

  _get_args(mrb, "*&", &argv, &argc, &blk);

  if (_nil_p(blk)) {
    _raise(mrb, E_ARGUMENT_ERROR, "no block given");
  }

  switch (_type(self)) {
  case MRB_TT_SYMBOL:
  case MRB_TT_FIXNUM:
#ifndef MRB_WITHOUT_FLOAT
  case MRB_TT_FLOAT:
#endif
    c = NULL;
    break;
  default:
    c = _class_ptr(_singleton_class(mrb, self));
    break;
  }
  mrb->c->ci->target_class = c;
  return _yield_cont(mrb, blk, self, argc, argv);
}

void
_mruby_object_ext_gem_init(state* mrb)
{
  struct RClass * n = mrb->nil_class;

  _define_method(mrb, n, "to_a", nil_to_a,       MRB_ARGS_NONE());
#ifndef MRB_WITHOUT_FLOAT
  _define_method(mrb, n, "to_f", nil_to_f,       MRB_ARGS_NONE());
#endif
  _define_method(mrb, n, "to_i", nil_to_i,       MRB_ARGS_NONE());

  _define_method(mrb, mrb->kernel_module, "instance_exec", _obj_instance_exec, MRB_ARGS_ANY() | MRB_ARGS_BLOCK());
}

void
_mruby_object_ext_gem_final(state* mrb)
{
}
