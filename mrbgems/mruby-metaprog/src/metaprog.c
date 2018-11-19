#include "mruby.h"
#include "mruby/array.h"
#include "mruby/hash.h"
#include "mruby/variable.h"
#include "mruby/proc.h"
#include "mruby/class.h"
#include "mruby/string.h"

typedef enum {
  NOEX_PUBLIC    = 0x00,
  NOEX_NOSUPER   = 0x01,
  NOEX_PRIVATE   = 0x02,
  NOEX_PROTECTED = 0x04,
  NOEX_MASK      = 0x06,
  NOEX_BASIC     = 0x08,
  NOEX_UNDEF     = NOEX_NOSUPER,
  NOEX_MODFUNC   = 0x12,
  NOEX_SUPER     = 0x20,
  NOEX_VCALL     = 0x40,
  NOEX_RESPONDS  = 0x80
} _method_flag_t;

static _value
_f_nil(_state *mrb, _value cv)
{
  return _nil_value();
}

/* 15.3.1.3.20 */
/*
 *  call-seq:
 *     obj.instance_variable_defined?(symbol)    -> true or false
 *
 *  Returns <code>true</code> if the given instance variable is
 *  defined in <i>obj</i>.
 *
 *     class Fred
 *       def initialize(p1, p2)
 *         @a, @b = p1, p2
 *       end
 *     end
 *     fred = Fred.new('cat', 99)
 *     fred.instance_variable_defined?(:@a)    #=> true
 *     fred.instance_variable_defined?("@b")   #=> true
 *     fred.instance_variable_defined?("@c")   #=> false
 */
static _value
_obj_ivar_defined(_state *mrb, _value self)
{
  _sym sym;

  _get_args(mrb, "n", &sym);
  _iv_name_sym_check(mrb, sym);
  return _bool_value(_iv_defined(mrb, self, sym));
}

/* 15.3.1.3.21 */
/*
 *  call-seq:
 *     obj.instance_variable_get(symbol)    -> obj
 *
 *  Returns the value of the given instance variable, or nil if the
 *  instance variable is not set. The <code>@</code> part of the
 *  variable name should be included for regular instance
 *  variables. Throws a <code>NameError</code> exception if the
 *  supplied symbol is not valid as an instance variable name.
 *
 *     class Fred
 *       def initialize(p1, p2)
 *         @a, @b = p1, p2
 *       end
 *     end
 *     fred = Fred.new('cat', 99)
 *     fred.instance_variable_get(:@a)    #=> "cat"
 *     fred.instance_variable_get("@b")   #=> 99
 */
static _value
_obj_ivar_get(_state *mrb, _value self)
{
  _sym iv_name;

  _get_args(mrb, "n", &iv_name);
  _iv_name_sym_check(mrb, iv_name);
  return _iv_get(mrb, self, iv_name);
}

/* 15.3.1.3.22 */
/*
 *  call-seq:
 *     obj.instance_variable_set(symbol, obj)    -> obj
 *
 *  Sets the instance variable names by <i>symbol</i> to
 *  <i>object</i>, thereby frustrating the efforts of the class's
 *  author to attempt to provide proper encapsulation. The variable
 *  did not have to exist prior to this call.
 *
 *     class Fred
 *       def initialize(p1, p2)
 *         @a, @b = p1, p2
 *       end
 *     end
 *     fred = Fred.new('cat', 99)
 *     fred.instance_variable_set(:@a, 'dog')   #=> "dog"
 *     fred.instance_variable_set(:@c, 'cat')   #=> "cat"
 *     fred.inspect                             #=> "#<Fred:0x401b3da8 @a=\"dog\", @b=99, @c=\"cat\">"
 */
static _value
_obj_ivar_set(_state *mrb, _value self)
{
  _sym iv_name;
  _value val;

  _get_args(mrb, "no", &iv_name, &val);
  _iv_name_sym_check(mrb, iv_name);
  _iv_set(mrb, self, iv_name, val);
  return val;
}

/* 15.3.1.2.7 */
/*
 *  call-seq:
 *     local_variables   -> array
 *
 *  Returns the names of local variables in the current scope.
 *
 *  [mruby limitation]
 *  If variable symbol information was stripped out from
 *  compiled binary files using `mruby-strip -l`, this
 *  method always returns an empty array.
 */
static _value
_local_variables(_state *mrb, _value self)
{
  struct RProc *proc;
  _irep *irep;
  _value vars;
  size_t i;

  proc = mrb->c->ci[-1].proc;

  if (MRB_PROC_CFUNC_P(proc)) {
    return _ary_new(mrb);
  }
  vars = _hash_new(mrb);
  while (proc) {
    if (MRB_PROC_CFUNC_P(proc)) break;
    irep = proc->body.irep;
    if (!irep->lv) break;
    for (i = 0; i + 1 < irep->nlocals; ++i) {
      if (irep->lv[i].name) {
        _sym sym = irep->lv[i].name;
        const char *name = _sym2name(mrb, sym);
        switch (name[0]) {
        case '*': case '&':
          break;
        default:
          _hash_set(mrb, vars, _symbol_value(sym), _true_value());
          break;
        }
      }
    }
    if (!MRB_PROC_ENV_P(proc)) break;
    proc = proc->upper;
    //if (MRB_PROC_SCOPE_P(proc)) break;
    if (!proc->c) break;
  }

  return _hash_keys(mrb, vars);
}

KHASH_DECLARE(st, _sym, char, FALSE)

static void
method_entry_loop(_state *mrb, struct RClass* klass, khash_t(st)* set)
{
  khint_t i;

  khash_t(mt) *h = klass->mt;
  if (!h || kh_size(h) == 0) return;
  for (i=0;i<kh_end(h);i++) {
    if (kh_exist(h, i)) {
      _method_t m = kh_value(h, i);
      if (MRB_METHOD_UNDEF_P(m)) continue;
      kh_put(st, mrb, set, kh_key(h, i));
    }
  }
}

_value
_class_instance_method_list(_state *mrb, _bool recur, struct RClass* klass, int obj)
{
  khint_t i;
  _value ary;
  _bool prepended = FALSE;
  struct RClass* oldklass;
  khash_t(st)* set = kh_init(st, mrb);

  if (!recur && (klass->flags & MRB_FL_CLASS_IS_PREPENDED)) {
    MRB_CLASS_ORIGIN(klass);
    prepended = TRUE;
  }

  oldklass = 0;
  while (klass && (klass != oldklass)) {
    method_entry_loop(mrb, klass, set);
    if ((klass->tt == MRB_TT_ICLASS && !prepended) ||
        (klass->tt == MRB_TT_SCLASS)) {
    }
    else {
      if (!recur) break;
    }
    oldklass = klass;
    klass = klass->super;
  }

  ary = _ary_new_capa(mrb, kh_size(set));
  for (i=0;i<kh_end(set);i++) {
    if (kh_exist(set, i)) {
      _ary_push(mrb, ary, _symbol_value(kh_key(set, i)));
    }
  }
  kh_destroy(st, mrb, set);

  return ary;
}

static _value
_obj_methods(_state *mrb, _bool recur, _value obj, _method_flag_t flag)
{
  return _class_instance_method_list(mrb, recur, _class(mrb, obj), 0);
}
/* 15.3.1.3.31 */
/*
 *  call-seq:
 *     obj.methods    -> array
 *
 *  Returns a list of the names of methods publicly accessible in
 *  <i>obj</i>. This will include all the methods accessible in
 *  <i>obj</i>'s ancestors.
 *
 *     class Klass
 *       def kMethod()
 *       end
 *     end
 *     k = Klass.new
 *     k.methods[0..9]    #=> [:kMethod, :respond_to?, :nil?, :is_a?,
 *                        #    :class, :instance_variable_set,
 *                        #    :methods, :extend, :__send__, :instance_eval]
 *     k.methods.length   #=> 42
 */
static _value
_obj_methods_m(_state *mrb, _value self)
{
  _bool recur = TRUE;
  _get_args(mrb, "|b", &recur);
  return _obj_methods(mrb, recur, self, (_method_flag_t)0); /* everything but private */
}

/* 15.3.1.3.36 */
/*
 *  call-seq:
 *     obj.private_methods(all=true)   -> array
 *
 *  Returns the list of private methods accessible to <i>obj</i>. If
 *  the <i>all</i> parameter is set to <code>false</code>, only those methods
 *  in the receiver will be listed.
 */
static _value
_obj_private_methods(_state *mrb, _value self)
{
  _bool recur = TRUE;
  _get_args(mrb, "|b", &recur);
  return _obj_methods(mrb, recur, self, NOEX_PRIVATE); /* private attribute not define */
}

/* 15.3.1.3.37 */
/*
 *  call-seq:
 *     obj.protected_methods(all=true)   -> array
 *
 *  Returns the list of protected methods accessible to <i>obj</i>. If
 *  the <i>all</i> parameter is set to <code>false</code>, only those methods
 *  in the receiver will be listed.
 */
static _value
_obj_protected_methods(_state *mrb, _value self)
{
  _bool recur = TRUE;
  _get_args(mrb, "|b", &recur);
  return _obj_methods(mrb, recur, self, NOEX_PROTECTED); /* protected attribute not define */
}

/* 15.3.1.3.38 */
/*
 *  call-seq:
 *     obj.public_methods(all=true)   -> array
 *
 *  Returns the list of public methods accessible to <i>obj</i>. If
 *  the <i>all</i> parameter is set to <code>false</code>, only those methods
 *  in the receiver will be listed.
 */
static _value
_obj_public_methods(_state *mrb, _value self)
{
  _bool recur = TRUE;
  _get_args(mrb, "|b", &recur);
  return _obj_methods(mrb, recur, self, NOEX_PUBLIC); /* public attribute not define */
}

static _value
_obj_singleton_methods(_state *mrb, _bool recur, _value obj)
{
  khint_t i;
  _value ary;
  struct RClass* klass;
  khash_t(st)* set = kh_init(st, mrb);

  klass = _class(mrb, obj);

  if (klass && (klass->tt == MRB_TT_SCLASS)) {
      method_entry_loop(mrb, klass, set);
      klass = klass->super;
  }
  if (recur) {
      while (klass && ((klass->tt == MRB_TT_SCLASS) || (klass->tt == MRB_TT_ICLASS))) {
        method_entry_loop(mrb, klass, set);
        klass = klass->super;
      }
  }

  ary = _ary_new(mrb);
  for (i=0;i<kh_end(set);i++) {
    if (kh_exist(set, i)) {
      _ary_push(mrb, ary, _symbol_value(kh_key(set, i)));
    }
  }
  kh_destroy(st, mrb, set);

  return ary;
}

/* 15.3.1.3.45 */
/*
 *  call-seq:
 *     obj.singleton_methods(all=true)    -> array
 *
 *  Returns an array of the names of singleton methods for <i>obj</i>.
 *  If the optional <i>all</i> parameter is true, the list will include
 *  methods in modules included in <i>obj</i>.
 *  Only public and protected singleton methods are returned.
 *
 *     module Other
 *       def three() end
 *     end
 *
 *     class Single
 *       def Single.four() end
 *     end
 *
 *     a = Single.new
 *
 *     def a.one()
 *     end
 *
 *     class << a
 *       include Other
 *       def two()
 *       end
 *     end
 *
 *     Single.singleton_methods    #=> [:four]
 *     a.singleton_methods(false)  #=> [:two, :one]
 *     a.singleton_methods         #=> [:two, :one, :three]
 */
static _value
_obj_singleton_methods_m(_state *mrb, _value self)
{
  _bool recur = TRUE;
  _get_args(mrb, "|b", &recur);
  return _obj_singleton_methods(mrb, recur, self);
}

static _value
mod_define_singleton_method(_state *mrb, _value self)
{
  struct RProc *p;
  _method_t m;
  _sym mid;
  _value blk = _nil_value();

  _get_args(mrb, "n&", &mid, &blk);
  if (_nil_p(blk)) {
    _raise(mrb, E_ARGUMENT_ERROR, "no block given");
  }
  p = (struct RProc*)_obj_alloc(mrb, MRB_TT_PROC, mrb->proc_class);
  _proc_copy(p, _proc_ptr(blk));
  p->flags |= MRB_PROC_STRICT;
  MRB_METHOD_FROM_PROC(m, p);
  _define_method_raw(mrb, _class_ptr(_singleton_class(mrb, self)), mid, m);
  return _symbol_value(mid);
}

static void
check_cv_name_str(_state *mrb, _value str)
{
  const char *s = RSTRING_PTR(str);
  _int len = RSTRING_LEN(str);

  if (len < 3 || !(s[0] == '@' && s[1] == '@')) {
    _name_error(mrb, _intern_str(mrb, str), "'%S' is not allowed as a class variable name", str);
  }
}

static void
check_cv_name_sym(_state *mrb, _sym id)
{
  check_cv_name_str(mrb, _sym2str(mrb, id));
}

/* 15.2.2.4.39 */
/*
 *  call-seq:
 *     remove_class_variable(sym)    -> obj
 *
 *  Removes the definition of the <i>sym</i>, returning that
 *  constant's value.
 *
 *     class Dummy
 *       @@var = 99
 *       puts @@var
 *       p class_variables
 *       remove_class_variable(:@@var)
 *       p class_variables
 *     end
 *
 *  <em>produces:</em>
 *
 *     99
 *     [:@@var]
 *     []
 */

static _value
_mod_remove_cvar(_state *mrb, _value mod)
{
  _value val;
  _sym id;

  _get_args(mrb, "n", &id);
  check_cv_name_sym(mrb, id);

  val = _iv_remove(mrb, mod, id);
  if (!_undef_p(val)) return val;

  if (_cv_defined(mrb, mod, id)) {
    _name_error(mrb, id, "cannot remove %S for %S",
                   _sym2str(mrb, id), mod);
  }

  _name_error(mrb, id, "class variable %S not defined for %S",
                 _sym2str(mrb, id), mod);

 /* not reached */
 return _nil_value();
}

/* 15.2.2.4.16 */
/*
 *  call-seq:
 *     obj.class_variable_defined?(symbol)    -> true or false
 *
 *  Returns <code>true</code> if the given class variable is defined
 *  in <i>obj</i>.
 *
 *     class Fred
 *       @@foo = 99
 *     end
 *     Fred.class_variable_defined?(:@@foo)    #=> true
 *     Fred.class_variable_defined?(:@@bar)    #=> false
 */

static _value
_mod_cvar_defined(_state *mrb, _value mod)
{
  _sym id;

  _get_args(mrb, "n", &id);
  check_cv_name_sym(mrb, id);
  return _bool_value(_cv_defined(mrb, mod, id));
}

/* 15.2.2.4.17 */
/*
 *  call-seq:
 *     mod.class_variable_get(symbol)    -> obj
 *
 *  Returns the value of the given class variable (or throws a
 *  <code>NameError</code> exception). The <code>@@</code> part of the
 *  variable name should be included for regular class variables
 *
 *     class Fred
 *       @@foo = 99
 *     end
 *     Fred.class_variable_get(:@@foo)     #=> 99
 */

static _value
_mod_cvar_get(_state *mrb, _value mod)
{
  _sym id;

  _get_args(mrb, "n", &id);
  check_cv_name_sym(mrb, id);
  return _cv_get(mrb, mod, id);
}

/* 15.2.2.4.18 */
/*
 *  call-seq:
 *     obj.class_variable_set(symbol, obj)    -> obj
 *
 *  Sets the class variable names by <i>symbol</i> to
 *  <i>object</i>.
 *
 *     class Fred
 *       @@foo = 99
 *       def foo
 *         @@foo
 *       end
 *     end
 *     Fred.class_variable_set(:@@foo, 101)     #=> 101
 *     Fred.new.foo                             #=> 101
 */

static _value
_mod_cvar_set(_state *mrb, _value mod)
{
  _value value;
  _sym id;

  _get_args(mrb, "no", &id, &value);
  check_cv_name_sym(mrb, id);
  _cv_set(mrb, mod, id, value);
  return value;
}

static _value
_mod_included_modules(_state *mrb, _value self)
{
  _value result;
  struct RClass *c = _class_ptr(self);
  struct RClass *origin = c;

  MRB_CLASS_ORIGIN(origin);
  result = _ary_new(mrb);
  while (c) {
    if (c != origin && c->tt == MRB_TT_ICLASS) {
      if (c->c->tt == MRB_TT_MODULE) {
        _ary_push(mrb, result, _obj_value(c->c));
      }
    }
    c = c->super;
  }

  return result;
}

_value _class_instance_method_list(_state*, _bool, struct RClass*, int);

/* 15.2.2.4.33 */
/*
 *  call-seq:
 *     mod.instance_methods(include_super=true)   -> array
 *
 *  Returns an array containing the names of the public and protected instance
 *  methods in the receiver. For a module, these are the public and protected methods;
 *  for a class, they are the instance (not singleton) methods. With no
 *  argument, or with an argument that is <code>false</code>, the
 *  instance methods in <i>mod</i> are returned, otherwise the methods
 *  in <i>mod</i> and <i>mod</i>'s superclasses are returned.
 *
 *     module A
 *       def method1()  end
 *     end
 *     class B
 *       def method2()  end
 *     end
 *     class C < B
 *       def method3()  end
 *     end
 *
 *     A.instance_methods                #=> [:method1]
 *     B.instance_methods(false)         #=> [:method2]
 *     C.instance_methods(false)         #=> [:method3]
 *     C.instance_methods(true).length   #=> 43
 */

static _value
_mod_instance_methods(_state *mrb, _value mod)
{
  struct RClass *c = _class_ptr(mod);
  _bool recur = TRUE;
  _get_args(mrb, "|b", &recur);
  return _class_instance_method_list(mrb, recur, c, 0);
}

static void
remove_method(_state *mrb, _value mod, _sym mid)
{
  struct RClass *c = _class_ptr(mod);
  khash_t(mt) *h;
  khiter_t k;

  MRB_CLASS_ORIGIN(c);
  h = c->mt;

  if (h) {
    k = kh_get(mt, mrb, h, mid);
    if (k != kh_end(h)) {
      kh_del(mt, mrb, h, k);
      _funcall(mrb, mod, "method_removed", 1, _symbol_value(mid));
      return;
    }
  }

  _name_error(mrb, mid, "method '%S' not defined in %S",
                 _sym2str(mrb, mid), mod);
}

/* 15.2.2.4.41 */
/*
 *  call-seq:
 *     remove_method(symbol)   -> self
 *
 *  Removes the method identified by _symbol_ from the current
 *  class. For an example, see <code>Module.undef_method</code>.
 */

static _value
_mod_remove_method(_state *mrb, _value mod)
{
  _int argc;
  _value *argv;

  _get_args(mrb, "*", &argv, &argc);
  while (argc--) {
    remove_method(mrb, mod, _obj_to_sym(mrb, *argv));
    argv++;
  }
  return mod;
}

static _value
_mod_s_constants(_state *mrb, _value mod)
{
  _raise(mrb, E_NOTIMP_ERROR, "Module.constants not implemented");
  return _nil_value();       /* not reached */
}

/* implementation of Module.nesting */
_value _mod_s_nesting(_state*, _value);

void
_mruby_metaprog_gem_init(_state* mrb)
{
  struct RClass *krn = mrb->kernel_module;
  struct RClass *mod = mrb->module_class;

  _define_method(mrb, krn, "global_variables", _f_global_variables, MRB_ARGS_NONE()); /* 15.3.1.2.4 */
  _define_method(mrb, krn, "local_variables", _local_variables, MRB_ARGS_NONE()); /* 15.3.1.3.28 */

  _define_method(mrb, krn, "singleton_class", _singleton_class, MRB_ARGS_NONE());
  _define_method(mrb, krn, "instance_variable_defined?", _obj_ivar_defined, MRB_ARGS_REQ(1)); /* 15.3.1.3.20 */
  _define_method(mrb, krn, "instance_variable_get", _obj_ivar_get, MRB_ARGS_REQ(1)); /* 15.3.1.3.21 */
  _define_method(mrb, krn, "instance_variable_set", _obj_ivar_set, MRB_ARGS_REQ(2)); /* 15.3.1.3.22 */
  _define_method(mrb, krn, "instance_variables", _obj_instance_variables, MRB_ARGS_NONE()); /* 15.3.1.3.23 */
  _define_method(mrb, krn, "methods", _obj_methods_m, MRB_ARGS_OPT(1)); /* 15.3.1.3.31 */
  _define_method(mrb, krn, "private_methods", _obj_private_methods, MRB_ARGS_OPT(1)); /* 15.3.1.3.36 */
  _define_method(mrb, krn, "protected_methods", _obj_protected_methods, MRB_ARGS_OPT(1)); /* 15.3.1.3.37 */
  _define_method(mrb, krn, "public_methods", _obj_public_methods, MRB_ARGS_OPT(1)); /* 15.3.1.3.38 */
  _define_method(mrb, krn, "singleton_methods", _obj_singleton_methods_m, MRB_ARGS_OPT(1)); /* 15.3.1.3.45 */
  _define_method(mrb, krn, "define_singleton_method", mod_define_singleton_method, MRB_ARGS_ANY());
  _define_method(mrb, krn, "send", _f_send, MRB_ARGS_ANY()); /* 15.3.1.3.44 */

  _define_method(mrb, mod, "class_variables", _mod_class_variables, MRB_ARGS_NONE()); /* 15.2.2.4.19 */
  _define_method(mrb, mod, "remove_class_variable", _mod_remove_cvar, MRB_ARGS_REQ(1)); /* 15.2.2.4.39 */
  _define_method(mrb, mod, "class_variable_defined?", _mod_cvar_defined, MRB_ARGS_REQ(1)); /* 15.2.2.4.16 */
  _define_method(mrb, mod, "class_variable_get", _mod_cvar_get, MRB_ARGS_REQ(1)); /* 15.2.2.4.17 */
  _define_method(mrb, mod, "class_variable_set", _mod_cvar_set, MRB_ARGS_REQ(2)); /* 15.2.2.4.18 */
  _define_method(mrb, mod, "included_modules", _mod_included_modules, MRB_ARGS_NONE()); /* 15.2.2.4.30 */
  _define_method(mrb, mod, "instance_methods", _mod_instance_methods, MRB_ARGS_ANY()); /* 15.2.2.4.33 */
  _define_method(mrb, mod, "remove_method", _mod_remove_method, MRB_ARGS_ANY()); /* 15.2.2.4.41 */
  _define_method(mrb, mod, "method_removed", _f_nil, MRB_ARGS_REQ(1));
  _define_method(mrb, mod, "constants", _mod_constants, MRB_ARGS_OPT(1)); /* 15.2.2.4.24 */
  _define_class_method(mrb, mod, "constants", _mod_s_constants, MRB_ARGS_ANY()); /* 15.2.2.3.1 */
  _define_class_method(mrb, mod, "nesting", _mod_s_nesting, MRB_ARGS_REQ(0)); /* 15.2.2.3.2 */
}

void
_mruby_metaprog_gem_final(_state* mrb)
{
}
