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
} method_flag_t;

static value
f_nil(state *mrb, value cv)
{
  return nil_value();
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
static value
obj_ivar_defined(state *mrb, value self)
{
  sym sym;

  get_args(mrb, "n", &sym);
  iv_name_sym_check(mrb, sym);
  return bool_value(iv_defined(mrb, self, sym));
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
static value
obj_ivar_get(state *mrb, value self)
{
  sym iv_name;

  get_args(mrb, "n", &iv_name);
  iv_name_sym_check(mrb, iv_name);
  return iv_get(mrb, self, iv_name);
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
static value
obj_ivar_set(state *mrb, value self)
{
  sym iv_name;
  value val;

  get_args(mrb, "no", &iv_name, &val);
  iv_name_sym_check(mrb, iv_name);
  iv_set(mrb, self, iv_name, val);
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
static value
local_variables(state *mrb, value self)
{
  struct RProc *proc;
  irep *irep;
  value vars;
  size_t i;

  proc = mrb->c->ci[-1].proc;

  if (PROC_CFUNC_P(proc)) {
    return ary_new(mrb);
  }
  vars = hash_new(mrb);
  while (proc) {
    if (PROC_CFUNC_P(proc)) break;
    irep = proc->body.irep;
    if (!irep->lv) break;
    for (i = 0; i + 1 < irep->nlocals; ++i) {
      if (irep->lv[i].name) {
        sym sym = irep->lv[i].name;
        const char *name = sym2name(mrb, sym);
        switch (name[0]) {
        case '*': case '&':
          break;
        default:
          hash_set(mrb, vars, symbol_value(sym), true_value());
          break;
        }
      }
    }
    if (!PROC_ENV_P(proc)) break;
    proc = proc->upper;
    //if (PROC_SCOPE_P(proc)) break;
    if (!proc->c) break;
  }

  return hash_keys(mrb, vars);
}

KHASH_DECLARE(st, sym, char, FALSE)

static void
method_entry_loop(state *mrb, struct RClass* klass, khash_t(st)* set)
{
  khint_t i;

  khash_t(mt) *h = klass->mt;
  if (!h || kh_size(h) == 0) return;
  for (i=0;i<kh_end(h);i++) {
    if (kh_exist(h, i)) {
      method_t m = kh_value(h, i);
      if (METHOD_UNDEF_P(m)) continue;
      kh_put(st, mrb, set, kh_key(h, i));
    }
  }
}

value
class_instance_method_list(state *mrb, bool recur, struct RClass* klass, int obj)
{
  khint_t i;
  value ary;
  bool prepended = FALSE;
  struct RClass* oldklass;
  khash_t(st)* set = kh_init(st, mrb);

  if (!recur && (klass->flags & FL_CLASS_IS_PREPENDED)) {
    CLASS_ORIGIN(klass);
    prepended = TRUE;
  }

  oldklass = 0;
  while (klass && (klass != oldklass)) {
    method_entry_loop(mrb, klass, set);
    if ((klass->tt == TT_ICLASS && !prepended) ||
        (klass->tt == TT_SCLASS)) {
    }
    else {
      if (!recur) break;
    }
    oldklass = klass;
    klass = klass->super;
  }

  ary = ary_new_capa(mrb, kh_size(set));
  for (i=0;i<kh_end(set);i++) {
    if (kh_exist(set, i)) {
      ary_push(mrb, ary, symbol_value(kh_key(set, i)));
    }
  }
  kh_destroy(st, mrb, set);

  return ary;
}

static value
obj_methods(state *mrb, bool recur, value obj, method_flag_t flag)
{
  return class_instance_method_list(mrb, recur, class(mrb, obj), 0);
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
static value
obj_methods_m(state *mrb, value self)
{
  bool recur = TRUE;
  get_args(mrb, "|b", &recur);
  return obj_methods(mrb, recur, self, (method_flag_t)0); /* everything but private */
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
static value
obj_private_methods(state *mrb, value self)
{
  bool recur = TRUE;
  get_args(mrb, "|b", &recur);
  return obj_methods(mrb, recur, self, NOEX_PRIVATE); /* private attribute not define */
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
static value
obj_protected_methods(state *mrb, value self)
{
  bool recur = TRUE;
  get_args(mrb, "|b", &recur);
  return obj_methods(mrb, recur, self, NOEX_PROTECTED); /* protected attribute not define */
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
static value
obj_public_methods(state *mrb, value self)
{
  bool recur = TRUE;
  get_args(mrb, "|b", &recur);
  return obj_methods(mrb, recur, self, NOEX_PUBLIC); /* public attribute not define */
}

static value
obj_singleton_methods(state *mrb, bool recur, value obj)
{
  khint_t i;
  value ary;
  struct RClass* klass;
  khash_t(st)* set = kh_init(st, mrb);

  klass = class(mrb, obj);

  if (klass && (klass->tt == TT_SCLASS)) {
      method_entry_loop(mrb, klass, set);
      klass = klass->super;
  }
  if (recur) {
      while (klass && ((klass->tt == TT_SCLASS) || (klass->tt == TT_ICLASS))) {
        method_entry_loop(mrb, klass, set);
        klass = klass->super;
      }
  }

  ary = ary_new(mrb);
  for (i=0;i<kh_end(set);i++) {
    if (kh_exist(set, i)) {
      ary_push(mrb, ary, symbol_value(kh_key(set, i)));
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
static value
obj_singleton_methods_m(state *mrb, value self)
{
  bool recur = TRUE;
  get_args(mrb, "|b", &recur);
  return obj_singleton_methods(mrb, recur, self);
}

static value
mod_define_singleton_method(state *mrb, value self)
{
  struct RProc *p;
  method_t m;
  sym mid;
  value blk = nil_value();

  get_args(mrb, "n&", &mid, &blk);
  if (nil_p(blk)) {
    raise(mrb, E_ARGUMENT_ERROR, "no block given");
  }
  p = (struct RProc*)obj_alloc(mrb, TT_PROC, mrb->proc_class);
  proc_copy(p, proc_ptr(blk));
  p->flags |= PROC_STRICT;
  METHOD_FROM_PROC(m, p);
  define_method_raw(mrb, class_ptr(singleton_class(mrb, self)), mid, m);
  return symbol_value(mid);
}

static void
check_cv_name_str(state *mrb, value str)
{
  const char *s = RSTRING_PTR(str);
  int len = RSTRING_LEN(str);

  if (len < 3 || !(s[0] == '@' && s[1] == '@')) {
    name_error(mrb, intern_str(mrb, str), "'%S' is not allowed as a class variable name", str);
  }
}

static void
check_cv_name_sym(state *mrb, sym id)
{
  check_cv_name_str(mrb, sym2str(mrb, id));
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

static value
mod_remove_cvar(state *mrb, value mod)
{
  value val;
  sym id;

  get_args(mrb, "n", &id);
  check_cv_name_sym(mrb, id);

  val = iv_remove(mrb, mod, id);
  if (!undef_p(val)) return val;

  if (cv_defined(mrb, mod, id)) {
    name_error(mrb, id, "cannot remove %S for %S",
                   sym2str(mrb, id), mod);
  }

  name_error(mrb, id, "class variable %S not defined for %S",
                 sym2str(mrb, id), mod);

 /* not reached */
 return nil_value();
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

static value
mod_cvar_defined(state *mrb, value mod)
{
  sym id;

  get_args(mrb, "n", &id);
  check_cv_name_sym(mrb, id);
  return bool_value(cv_defined(mrb, mod, id));
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

static value
mod_cvar_get(state *mrb, value mod)
{
  sym id;

  get_args(mrb, "n", &id);
  check_cv_name_sym(mrb, id);
  return cv_get(mrb, mod, id);
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

static value
mod_cvar_set(state *mrb, value mod)
{
  value value;
  sym id;

  get_args(mrb, "no", &id, &value);
  check_cv_name_sym(mrb, id);
  cv_set(mrb, mod, id, value);
  return value;
}

static value
mod_included_modules(state *mrb, value self)
{
  value result;
  struct RClass *c = class_ptr(self);
  struct RClass *origin = c;

  CLASS_ORIGIN(origin);
  result = ary_new(mrb);
  while (c) {
    if (c != origin && c->tt == TT_ICLASS) {
      if (c->c->tt == TT_MODULE) {
        ary_push(mrb, result, obj_value(c->c));
      }
    }
    c = c->super;
  }

  return result;
}

value class_instance_method_list(state*, bool, struct RClass*, int);

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

static value
mod_instance_methods(state *mrb, value mod)
{
  struct RClass *c = class_ptr(mod);
  bool recur = TRUE;
  get_args(mrb, "|b", &recur);
  return class_instance_method_list(mrb, recur, c, 0);
}

static void
remove_method(state *mrb, value mod, sym mid)
{
  struct RClass *c = class_ptr(mod);
  khash_t(mt) *h;
  khiter_t k;

  CLASS_ORIGIN(c);
  h = c->mt;

  if (h) {
    k = kh_get(mt, mrb, h, mid);
    if (k != kh_end(h)) {
      kh_del(mt, mrb, h, k);
      funcall(mrb, mod, "method_removed", 1, symbol_value(mid));
      return;
    }
  }

  name_error(mrb, mid, "method '%S' not defined in %S",
                 sym2str(mrb, mid), mod);
}

/* 15.2.2.4.41 */
/*
 *  call-seq:
 *     remove_method(symbol)   -> self
 *
 *  Removes the method identified by _symbol_ from the current
 *  class. For an example, see <code>Module.undef_method</code>.
 */

static value
mod_remove_method(state *mrb, value mod)
{
  int argc;
  value *argv;

  get_args(mrb, "*", &argv, &argc);
  while (argc--) {
    remove_method(mrb, mod, obj_to_sym(mrb, *argv));
    argv++;
  }
  return mod;
}

static value
mod_s_constants(state *mrb, value mod)
{
  raise(mrb, E_NOTIMP_ERROR, "Module.constants not implemented");
  return nil_value();       /* not reached */
}

/* implementation of Module.nesting */
value mod_s_nesting(state*, value);

void
mruby_metaprog_gem_init(state* mrb)
{
  struct RClass *krn = mrb->kernel_module;
  struct RClass *mod = mrb->module_class;

  define_method(mrb, krn, "global_variables", f_global_variables, ARGS_NONE()); /* 15.3.1.2.4 */
  define_method(mrb, krn, "local_variables", local_variables, ARGS_NONE()); /* 15.3.1.3.28 */

  define_method(mrb, krn, "singleton_class", singleton_class, ARGS_NONE());
  define_method(mrb, krn, "instance_variable_defined?", obj_ivar_defined, ARGS_REQ(1)); /* 15.3.1.3.20 */
  define_method(mrb, krn, "instance_variable_get", obj_ivar_get, ARGS_REQ(1)); /* 15.3.1.3.21 */
  define_method(mrb, krn, "instance_variable_set", obj_ivar_set, ARGS_REQ(2)); /* 15.3.1.3.22 */
  define_method(mrb, krn, "instance_variables", obj_instance_variables, ARGS_NONE()); /* 15.3.1.3.23 */
  define_method(mrb, krn, "methods", obj_methods_m, ARGS_OPT(1)); /* 15.3.1.3.31 */
  define_method(mrb, krn, "private_methods", obj_private_methods, ARGS_OPT(1)); /* 15.3.1.3.36 */
  define_method(mrb, krn, "protected_methods", obj_protected_methods, ARGS_OPT(1)); /* 15.3.1.3.37 */
  define_method(mrb, krn, "public_methods", obj_public_methods, ARGS_OPT(1)); /* 15.3.1.3.38 */
  define_method(mrb, krn, "singleton_methods", obj_singleton_methods_m, ARGS_OPT(1)); /* 15.3.1.3.45 */
  define_method(mrb, krn, "define_singleton_method", mod_define_singleton_method, ARGS_ANY());
  define_method(mrb, krn, "send", f_send, ARGS_ANY()); /* 15.3.1.3.44 */

  define_method(mrb, mod, "class_variables", mod_class_variables, ARGS_NONE()); /* 15.2.2.4.19 */
  define_method(mrb, mod, "remove_class_variable", mod_remove_cvar, ARGS_REQ(1)); /* 15.2.2.4.39 */
  define_method(mrb, mod, "class_variable_defined?", mod_cvar_defined, ARGS_REQ(1)); /* 15.2.2.4.16 */
  define_method(mrb, mod, "class_variable_get", mod_cvar_get, ARGS_REQ(1)); /* 15.2.2.4.17 */
  define_method(mrb, mod, "class_variable_set", mod_cvar_set, ARGS_REQ(2)); /* 15.2.2.4.18 */
  define_method(mrb, mod, "included_modules", mod_included_modules, ARGS_NONE()); /* 15.2.2.4.30 */
  define_method(mrb, mod, "instance_methods", mod_instance_methods, ARGS_ANY()); /* 15.2.2.4.33 */
  define_method(mrb, mod, "remove_method", mod_remove_method, ARGS_ANY()); /* 15.2.2.4.41 */
  define_method(mrb, mod, "method_removed", f_nil, ARGS_REQ(1));
  define_method(mrb, mod, "constants", mod_constants, ARGS_OPT(1)); /* 15.2.2.4.24 */
  define_class_method(mrb, mod, "constants", mod_s_constants, ARGS_ANY()); /* 15.2.2.3.1 */
  define_class_method(mrb, mod, "nesting", mod_s_nesting, ARGS_REQ(0)); /* 15.2.2.3.2 */
}

void
mruby_metaprog_gem_final(state* mrb)
{
}
