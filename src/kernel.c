/*
** kernel.c - Kernel module
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/class.h>
#include <mruby/proc.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <mruby/error.h>
#include <mruby/istruct.h>

API bool
func_basic_p(state *mrb, value obj, sym mid, func_t func)
{
  struct RClass *c = class(mrb, obj);
  method_t m = method_search_vm(mrb, &c, mid);
  struct RProc *p;

  if (METHOD_UNDEF_P(m)) return FALSE;
  if (METHOD_FUNC_P(m))
    return METHOD_FUNC(m) == func;
  p = METHOD_PROC(m);
  if (PROC_CFUNC_P(p) && (PROC_CFUNC(p) == func))
    return TRUE;
  return FALSE;
}

static bool
obj_basic_to_s_p(state *mrb, value obj)
{
  return func_basic_p(mrb, obj, intern_lit(mrb, "to_s"), any_to_s);
}

/* 15.3.1.3.17 */
/*
 *  call-seq:
 *     obj.inspect   -> string
 *
 *  Returns a string containing a human-readable representation of
 *  <i>obj</i>. If not overridden and no instance variables, uses the
 *  <code>to_s</code> method to generate the string.
 *  <i>obj</i>.  If not overridden, uses the <code>to_s</code> method to
 *  generate the string.
 *
 *     [ 1, 2, 3..4, 'five' ].inspect   #=> "[1, 2, 3..4, \"five\"]"
 *     Time.new.inspect                 #=> "2008-03-08 19:43:39 +0900"
 */
API value
obj_inspect(state *mrb, value obj)
{
  if ((type(obj) == TT_OBJECT) && obj_basic_to_s_p(mrb, obj)) {
    return obj_iv_inspect(mrb, obj_ptr(obj));
  }
  return any_to_s(mrb, obj);
}

/* 15.3.1.3.2  */
/*
 *  call-seq:
 *     obj === other   -> true or false
 *
 *  Case Equality---For class <code>Object</code>, effectively the same
 *  as calling  <code>#==</code>, but typically overridden by descendants
 *  to provide meaningful semantics in <code>case</code> statements.
 */
static value
equal_m(state *mrb, value self)
{
  value arg;

  get_args(mrb, "o", &arg);
  return bool_value(equal(mrb, self, arg));
}

/* 15.3.1.3.3  */
/* 15.3.1.3.33 */
/*
 *  Document-method: __id__
 *  Document-method: object_id
 *
 *  call-seq:
 *     obj.__id__       -> fixnum
 *     obj.object_id    -> fixnum
 *
 *  Returns an integer identifier for <i>obj</i>. The same number will
 *  be returned on all calls to <code>id</code> for a given object, and
 *  no two active objects will share an id.
 *  <code>Object#object_id</code> is a different concept from the
 *  <code>:name</code> notation, which returns the symbol id of
 *  <code>name</code>. Replaces the deprecated <code>Object#id</code>.
 */
value
obj_id_m(state *mrb, value self)
{
  return fixnum_value(obj_id(self));
}

/* 15.3.1.2.2  */
/* 15.3.1.2.5  */
/* 15.3.1.3.6  */
/* 15.3.1.3.25 */
/*
 *  call-seq:
 *     block_given?   -> true or false
 *     iterator?      -> true or false
 *
 *  Returns <code>true</code> if <code>yield</code> would execute a
 *  block in the current context. The <code>iterator?</code> form
 *  is mildly deprecated.
 *
 *     def try
 *       if block_given?
 *         yield
 *       else
 *         "no block"
 *       end
 *     end
 *     try                  #=> "no block"
 *     try { "hello" }      #=> "hello"
 *     try do "hello" end   #=> "hello"
 */
static value
f_block_given_p_m(state *mrb, value self)
{
  callinfo *ci = &mrb->c->ci[-1];
  callinfo *cibase = mrb->c->cibase;
  value *bp;
  struct RProc *p;

  if (ci <= cibase) {
    /* toplevel does not have block */
    return false_value();
  }
  p = ci->proc;
  /* search method/class/module proc */
  while (p) {
    if (PROC_SCOPE_P(p)) break;
    p = p->upper;
  }
  if (p == NULL) return false_value();
  /* search ci corresponding to proc */
  while (cibase < ci) {
    if (ci->proc == p) break;
    ci--;
  }
  if (ci == cibase) {
    return false_value();
  }
  else if (ci->env) {
    struct REnv *e = ci->env;
    int bidx;

    /* top-level does not have block slot (always false) */
    if (e->stack == mrb->c->stbase)
      return false_value();
    /* use saved block arg position */
    bidx = ENV_BIDX(e);
    /* bidx may be useless (e.g. define_method) */
    if (bidx >= ENV_STACK_LEN(e))
      return false_value();
    bp = &e->stack[bidx];
  }
  else {
    bp = ci[1].stackent+1;
    if (ci->argc >= 0) {
      bp += ci->argc;
    }
    else {
      bp++;
    }
  }
  if (nil_p(*bp))
    return false_value();
  return true_value();
}

/* 15.3.1.3.7  */
/*
 *  call-seq:
 *     obj.class    -> class
 *
 *  Returns the class of <i>obj</i>. This method must always be
 *  called with an explicit receiver, as <code>class</code> is also a
 *  reserved word in Ruby.
 *
 *     1.class      #=> Fixnum
 *     self.class   #=> Object
 */
static value
obj_class_m(state *mrb, value self)
{
  return obj_value(obj_class(mrb, self));
}

static struct RClass*
singleton_class_clone(state *mrb, value obj)
{
  struct RClass *klass = basic_ptr(obj)->c;

  if (klass->tt != TT_SCLASS)
    return klass;
  else {
    /* copy singleton(unnamed) class */
    struct RClass *clone = (struct RClass*)obj_alloc(mrb, klass->tt, mrb->class_class);

    switch (type(obj)) {
    case TT_CLASS:
    case TT_SCLASS:
      break;
    default:
      clone->c = singleton_class_clone(mrb, obj_value(klass));
      break;
    }
    clone->super = klass->super;
    if (klass->iv) {
      iv_copy(mrb, obj_value(clone), obj_value(klass));
      obj_iv_set(mrb, (struct RObject*)clone, intern_lit(mrb, "__attached__"), obj);
    }
    if (klass->mt) {
      clone->mt = kh_copy(mt, mrb, klass->mt);
    }
    else {
      clone->mt = kh_init(mt, mrb);
    }
    clone->tt = TT_SCLASS;
    return clone;
  }
}

static void
copy_class(state *mrb, value dst, value src)
{
  struct RClass *dc = class_ptr(dst);
  struct RClass *sc = class_ptr(src);
  /* if the origin is not the same as the class, then the origin and
     the current class need to be copied */
  if (sc->flags & FL_CLASS_IS_PREPENDED) {
    struct RClass *c0 = sc->super;
    struct RClass *c1 = dc;

    /* copy prepended iclasses */
    while (!(c0->flags & FL_CLASS_IS_ORIGIN)) {
      c1->super = class_ptr(obj_dup(mrb, obj_value(c0)));
      c1 = c1->super;
      c0 = c0->super;
    }
    c1->super = class_ptr(obj_dup(mrb, obj_value(c0)));
    c1->super->flags |= FL_CLASS_IS_ORIGIN;
  }
  if (sc->mt) {
    dc->mt = kh_copy(mt, mrb, sc->mt);
  }
  else {
    dc->mt = kh_init(mt, mrb);
  }
  dc->super = sc->super;
  SET_INSTANCE_TT(dc, INSTANCE_TT(sc));
}

static void
init_copy(state *mrb, value dest, value obj)
{
  switch (type(obj)) {
    case TT_ICLASS:
      copy_class(mrb, dest, obj);
      return;
    case TT_CLASS:
    case TT_MODULE:
      copy_class(mrb, dest, obj);
      iv_copy(mrb, dest, obj);
      iv_remove(mrb, dest, intern_lit(mrb, "__classname__"));
      break;
    case TT_OBJECT:
    case TT_SCLASS:
    case TT_HASH:
    case TT_DATA:
    case TT_EXCEPTION:
      iv_copy(mrb, dest, obj);
      break;
    case TT_ISTRUCT:
      istruct_copy(dest, obj);
      break;

    default:
      break;
  }
  funcall(mrb, dest, "initialize_copy", 1, obj);
}

/* 15.3.1.3.8  */
/*
 *  call-seq:
 *     obj.clone -> an_object
 *
 *  Produces a shallow copy of <i>obj</i>---the instance variables of
 *  <i>obj</i> are copied, but not the objects they reference. Copies
 *  the frozen state of <i>obj</i>. See also the discussion
 *  under <code>Object#dup</code>.
 *
 *     class Klass
 *        attr_accessor :str
 *     end
 *     s1 = Klass.new      #=> #<Klass:0x401b3a38>
 *     s1.str = "Hello"    #=> "Hello"
 *     s2 = s1.clone       #=> #<Klass:0x401b3998 @str="Hello">
 *     s2.str[1,4] = "i"   #=> "i"
 *     s1.inspect          #=> "#<Klass:0x401b3a38 @str=\"Hi\">"
 *     s2.inspect          #=> "#<Klass:0x401b3998 @str=\"Hi\">"
 *
 *  This method may have class-specific behavior.  If so, that
 *  behavior will be documented under the #+initialize_copy+ method of
 *  the class.
 *
 *  Some Class(True False Nil Symbol Fixnum Float) Object  cannot clone.
 */
API value
obj_clone(state *mrb, value self)
{
  struct RObject *p;
  value clone;

  if (immediate_p(self)) {
    raisef(mrb, E_TYPE_ERROR, "can't clone %S", self);
  }
  if (type(self) == TT_SCLASS) {
    raise(mrb, E_TYPE_ERROR, "can't clone singleton class");
  }
  p = (struct RObject*)obj_alloc(mrb, type(self), obj_class(mrb, self));
  p->c = singleton_class_clone(mrb, self);
  field_write_barrier(mrb, (struct RBasic*)p, (struct RBasic*)p->c);
  clone = obj_value(p);
  init_copy(mrb, clone, self);
  p->flags |= obj_ptr(self)->flags & FL_OBJ_IS_FROZEN;

  return clone;
}

/* 15.3.1.3.9  */
/*
 *  call-seq:
 *     obj.dup -> an_object
 *
 *  Produces a shallow copy of <i>obj</i>---the instance variables of
 *  <i>obj</i> are copied, but not the objects they reference.
 *  <code>dup</code> copies the frozen state of <i>obj</i>. See also
 *  the discussion under <code>Object#clone</code>. In general,
 *  <code>clone</code> and <code>dup</code> may have different semantics
 *  in descendant classes. While <code>clone</code> is used to duplicate
 *  an object, including its internal state, <code>dup</code> typically
 *  uses the class of the descendant object to create the new instance.
 *
 *  This method may have class-specific behavior.  If so, that
 *  behavior will be documented under the #+initialize_copy+ method of
 *  the class.
 */

API value
obj_dup(state *mrb, value obj)
{
  struct RBasic *p;
  value dup;

  if (immediate_p(obj)) {
    raisef(mrb, E_TYPE_ERROR, "can't dup %S", obj);
  }
  if (type(obj) == TT_SCLASS) {
    raise(mrb, E_TYPE_ERROR, "can't dup singleton class");
  }
  p = obj_alloc(mrb, type(obj), obj_class(mrb, obj));
  dup = obj_value(p);
  init_copy(mrb, dup, obj);

  return dup;
}

static value
obj_extend(state *mrb, int argc, value *argv, value obj)
{
  int i;

  if (argc == 0) {
    raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (at least 1)");
  }
  for (i = 0; i < argc; i++) {
    check_type(mrb, argv[i], TT_MODULE);
  }
  while (argc--) {
    funcall(mrb, argv[argc], "extend_object", 1, obj);
    funcall(mrb, argv[argc], "extended", 1, obj);
  }
  return obj;
}

/* 15.3.1.3.13 */
/*
 *  call-seq:
 *     obj.extend(module, ...)    -> obj
 *
 *  Adds to _obj_ the instance methods from each module given as a
 *  parameter.
 *
 *     module Mod
 *       def hello
 *         "Hello from Mod.\n"
 *       end
 *     end
 *
 *     class Klass
 *       def hello
 *         "Hello from Klass.\n"
 *       end
 *     end
 *
 *     k = Klass.new
 *     k.hello         #=> "Hello from Klass.\n"
 *     k.extend(Mod)   #=> #<Klass:0x401b3bc8>
 *     k.hello         #=> "Hello from Mod.\n"
 */
static value
obj_extend_m(state *mrb, value self)
{
  value *argv;
  int argc;

  get_args(mrb, "*", &argv, &argc);
  return obj_extend(mrb, argc, argv, self);
}

static value
obj_freeze(state *mrb, value self)
{
  struct RBasic *b;

  switch (type(self)) {
    case TT_FALSE:
    case TT_TRUE:
    case TT_FIXNUM:
    case TT_SYMBOL:
#ifndef WITHOUT_FLOAT
    case TT_FLOAT:
#endif
      return self;
    default:
      break;
  }

  b = basic_ptr(self);
  if (!FROZEN_P(b)) {
    SET_FROZEN_FLAG(b);
  }
  return self;
}

static value
obj_frozen(state *mrb, value self)
{
  struct RBasic *b;

  switch (type(self)) {
    case TT_FALSE:
    case TT_TRUE:
    case TT_FIXNUM:
    case TT_SYMBOL:
#ifndef WITHOUT_FLOAT
    case TT_FLOAT:
#endif
      return true_value();
    default:
      break;
  }

  b = basic_ptr(self);
  if (!FROZEN_P(b)) {
    return false_value();
  }
  return true_value();
}

/* 15.3.1.3.15 */
/*
 *  call-seq:
 *     obj.hash    -> fixnum
 *
 *  Generates a <code>Fixnum</code> hash value for this object. This
 *  function must have the property that <code>a.eql?(b)</code> implies
 *  <code>a.hash == b.hash</code>. The hash value is used by class
 *  <code>Hash</code>. Any hash value that exceeds the capacity of a
 *  <code>Fixnum</code> will be truncated before being used.
 */
API value
obj_hash(state *mrb, value self)
{
  return fixnum_value(obj_id(self));
}

/* 15.3.1.3.16 */
static value
obj_init_copy(state *mrb, value self)
{
  value orig;

  get_args(mrb, "o", &orig);
  if (obj_equal(mrb, self, orig)) return self;
  if ((type(self) != type(orig)) || (obj_class(mrb, self) != obj_class(mrb, orig))) {
      raise(mrb, E_TYPE_ERROR, "initialize_copy should take same class object");
  }
  return self;
}


API bool
obj_is_instance_of(state *mrb, value obj, struct RClass* c)
{
  if (obj_class(mrb, obj) == c) return TRUE;
  return FALSE;
}

/* 15.3.1.3.19 */
/*
 *  call-seq:
 *     obj.instance_of?(class)    -> true or false
 *
 *  Returns <code>true</code> if <i>obj</i> is an instance of the given
 *  class. See also <code>Object#kind_of?</code>.
 */
static value
obj_is_instance_of(state *mrb, value self)
{
  value arg;

  get_args(mrb, "C", &arg);

  return bool_value(obj_is_instance_of(mrb, self, class_ptr(arg)));
}

/* 15.3.1.3.24 */
/* 15.3.1.3.26 */
/*
 *  call-seq:
 *     obj.is_a?(class)       -> true or false
 *     obj.kind_of?(class)    -> true or false
 *
 *  Returns <code>true</code> if <i>class</i> is the class of
 *  <i>obj</i>, or if <i>class</i> is one of the superclasses of
 *  <i>obj</i> or modules included in <i>obj</i>.
 *
 *     module M;    end
 *     class A
 *       include M
 *     end
 *     class B < A; end
 *     class C < B; end
 *     b = B.new
 *     b.instance_of? A   #=> false
 *     b.instance_of? B   #=> true
 *     b.instance_of? C   #=> false
 *     b.instance_of? M   #=> false
 *     b.kind_of? A       #=> true
 *     b.kind_of? B       #=> true
 *     b.kind_of? C       #=> false
 *     b.kind_of? M       #=> true
 */
static value
obj_is_kind_of_m(state *mrb, value self)
{
  value arg;

  get_args(mrb, "C", &arg);

  return bool_value(obj_is_kind_of(mrb, self, class_ptr(arg)));
}

KHASH_DECLARE(st, sym, char, FALSE)
KHASH_DEFINE(st, sym, char, FALSE, kh_int_hash_func, kh_int_hash_equal)

/* 15.3.1.3.32 */
/*
 * call_seq:
 *   nil.nil?               -> true
 *   <anything_else>.nil?   -> false
 *
 * Only the object <i>nil</i> responds <code>true</code> to <code>nil?</code>.
 */
static value
false(state *mrb, value self)
{
  return false_value();
}

/* 15.3.1.2.12  */
/* 15.3.1.3.40 */
/*
 *  call-seq:
 *     raise
 *     raise(string)
 *     raise(exception [, string])
 *
 *  With no arguments, raises a <code>RuntimeError</code>
 *  With a single +String+ argument, raises a
 *  +RuntimeError+ with the string as a message. Otherwise,
 *  the first parameter should be the name of an +Exception+
 *  class (or an object that returns an +Exception+ object when sent
 *  an +exception+ message). The optional second parameter sets the
 *  message associated with the exception, and the third parameter is an
 *  array of callback information. Exceptions are caught by the
 *  +rescue+ clause of <code>begin...end</code> blocks.
 *
 *     raise "Failed to create socket"
 *     raise ArgumentError, "No parameters", caller
 */
API value
f_raise(state *mrb, value self)
{
  value a[2], exc;
  int argc;


  argc = get_args(mrb, "|oo", &a[0], &a[1]);
  switch (argc) {
  case 0:
    raise(mrb, E_RUNTIME_ERROR, "");
    break;
  case 1:
    if (string_p(a[0])) {
      a[1] = a[0];
      argc = 2;
      a[0] = obj_value(E_RUNTIME_ERROR);
    }
    /* fall through */
  default:
    exc = make_exception(mrb, argc, a);
    exc_raise(mrb, exc);
    break;
  }
  return nil_value();            /* not reached */
}

static value
krn_class_defined(state *mrb, value self)
{
  value str;

  get_args(mrb, "S", &str);
  return bool_value(class_defined(mrb, RSTRING_PTR(str)));
}


/* 15.3.1.3.41 */
/*
 *  call-seq:
 *     obj.remove_instance_variable(symbol)    -> obj
 *
 *  Removes the named instance variable from <i>obj</i>, returning that
 *  variable's value.
 *
 *     class Dummy
 *       attr_reader :var
 *       def initialize
 *         @var = 99
 *       end
 *       def remove
 *         remove_instance_variable(:@var)
 *       end
 *     end
 *     d = Dummy.new
 *     d.var      #=> 99
 *     d.remove   #=> 99
 *     d.var      #=> nil
 */
static value
obj_remove_instance_variable(state *mrb, value self)
{
  sym sym;
  value val;

  get_args(mrb, "n", &sym);
  iv_name_sym_check(mrb, sym);
  val = iv_remove(mrb, self, sym);
  if (undef_p(val)) {
    name_error(mrb, sym, "instance variable %S not defined", sym2str(mrb, sym));
  }
  return val;
}

void
method_missing(state *mrb, sym name, value self, value args)
{
  no_method_error(mrb, name, args, "undefined method '%S'", sym2str(mrb, name));
}

/* 15.3.1.3.30 */
/*
 *  call-seq:
 *     obj.method_missing(symbol [, *args] )   -> result
 *
 *  Invoked by Ruby when <i>obj</i> is sent a message it cannot handle.
 *  <i>symbol</i> is the symbol for the method called, and <i>args</i>
 *  are any arguments that were passed to it. By default, the interpreter
 *  raises an error when this method is called. However, it is possible
 *  to override the method to provide more dynamic behavior.
 *  If it is decided that a particular method should not be handled, then
 *  <i>super</i> should be called, so that ancestors can pick up the
 *  missing method.
 *  The example below creates
 *  a class <code>Roman</code>, which responds to methods with names
 *  consisting of roman numerals, returning the corresponding integer
 *  values.
 *
 *     class Roman
 *       def romanToInt(str)
 *         # ...
 *       end
 *       def method_missing(methId)
 *         str = methId.id2name
 *         romanToInt(str)
 *       end
 *     end
 *
 *     r = Roman.new
 *     r.iv      #=> 4
 *     r.xxiii   #=> 23
 *     r.mm      #=> 2000
 */
#ifdef DEFAULT_METHOD_MISSING
static value
obj_missing(state *mrb, value mod)
{
  sym name;
  value *a;
  int alen;

  get_args(mrb, "n*!", &name, &a, &alen);
  method_missing(mrb, name, mod, ary_new_from_values(mrb, alen, a));
  /* not reached */
  return nil_value();
}
#endif

static inline bool
basic_obj_respond_to(state *mrb, value obj, sym id, int pub)
{
  return respond_to(mrb, obj, id);
}
/* 15.3.1.3.43 */
/*
 *  call-seq:
 *     obj.respond_to?(symbol, include_private=false) -> true or false
 *
 *  Returns +true+ if _obj_ responds to the given
 *  method. Private methods are included in the search only if the
 *  optional second parameter evaluates to +true+.
 *
 *  If the method is not implemented,
 *  as Process.fork on Windows, File.lchmod on GNU/Linux, etc.,
 *  false is returned.
 *
 *  If the method is not defined, <code>respond_to_missing?</code>
 *  method is called and the result is returned.
 */
static value
obj_respond_to(state *mrb, value self)
{
  value mid;
  sym id, rtm_id;
  bool priv = FALSE, respond_to_p = TRUE;

  get_args(mrb, "o|b", &mid, &priv);

  if (symbol_p(mid)) {
    id = symbol(mid);
  }
  else {
    value tmp;
    if (string_p(mid)) {
      tmp = check_intern_str(mrb, mid);
    }
    else {
      tmp = check_string_type(mrb, mid);
      if (nil_p(tmp)) {
        tmp = inspect(mrb, mid);
        raisef(mrb, E_TYPE_ERROR, "%S is not a symbol", tmp);
      }
      tmp = check_intern_str(mrb, tmp);
    }
    if (nil_p(tmp)) {
      respond_to_p = FALSE;
    }
    else {
      id = symbol(tmp);
    }
  }

  if (respond_to_p) {
    respond_to_p = basic_obj_respond_to(mrb, self, id, !priv);
  }

  if (!respond_to_p) {
    rtm_id = intern_lit(mrb, "respond_to_missing?");
    if (basic_obj_respond_to(mrb, self, rtm_id, !priv)) {
      value args[2], v;
      args[0] = mid;
      args[1] = bool_value(priv);
      v = funcall_argv(mrb, self, rtm_id, 2, args);
      return bool_value(bool(v));
    }
  }
  return bool_value(respond_to_p);
}

static value
obj_ceqq(state *mrb, value self)
{
  value v;
  int i, len;
  sym eqq = intern_lit(mrb, "===");
  value ary = ary_splat(mrb, self);

  get_args(mrb, "o", &v);
  len = RARRAY_LEN(ary);
  for (i=0; i<len; i++) {
    value c = funcall_argv(mrb, ary_entry(ary, i), eqq, 1, &v);
    if (test(c)) return true_value();
  }
  return false_value();
}

value obj_equal_m(state *mrb, value);
void
init_kernel(state *mrb)
{
  struct RClass *krn;

  mrb->kernel_module = krn = define_module(mrb, "Kernel");                                                    /* 15.3.1 */
  define_class_method(mrb, krn, "block_given?",         f_block_given_p_m,           ARGS_NONE());    /* 15.3.1.2.2  */
  define_class_method(mrb, krn, "iterator?",            f_block_given_p_m,           ARGS_NONE());    /* 15.3.1.2.5  */
;     /* 15.3.1.2.11 */
  define_class_method(mrb, krn, "raise",                f_raise,                     ARGS_OPT(2));    /* 15.3.1.2.12 */


  define_method(mrb, krn, "===",                        equal_m,                     ARGS_REQ(1));    /* 15.3.1.3.2  */
  define_method(mrb, krn, "block_given?",               f_block_given_p_m,           ARGS_NONE());    /* 15.3.1.3.6  */
  define_method(mrb, krn, "class",                      obj_class_m,                 ARGS_NONE());    /* 15.3.1.3.7  */
  define_method(mrb, krn, "clone",                      obj_clone,                   ARGS_NONE());    /* 15.3.1.3.8  */
  define_method(mrb, krn, "dup",                        obj_dup,                     ARGS_NONE());    /* 15.3.1.3.9  */
  define_method(mrb, krn, "eql?",                       obj_equal_m,                 ARGS_REQ(1));    /* 15.3.1.3.10 */
  define_method(mrb, krn, "equal?",                     obj_equal_m,                 ARGS_REQ(1));    /* 15.3.1.3.11 */
  define_method(mrb, krn, "extend",                     obj_extend_m,                ARGS_ANY());     /* 15.3.1.3.13 */
  define_method(mrb, krn, "freeze",                     obj_freeze,                  ARGS_NONE());
  define_method(mrb, krn, "frozen?",                    obj_frozen,                  ARGS_NONE());
  define_method(mrb, krn, "global_variables",           f_global_variables,          ARGS_NONE());    /* 15.3.1.3.14 */
  define_method(mrb, krn, "hash",                       obj_hash,                    ARGS_NONE());    /* 15.3.1.3.15 */
  define_method(mrb, krn, "initialize_copy",            obj_init_copy,               ARGS_REQ(1));    /* 15.3.1.3.16 */
  define_method(mrb, krn, "inspect",                    obj_inspect,                 ARGS_NONE());    /* 15.3.1.3.17 */
  define_method(mrb, krn, "instance_of?",               obj_is_instance_of,              ARGS_REQ(1));    /* 15.3.1.3.19 */

  define_method(mrb, krn, "is_a?",                      obj_is_kind_of_m,            ARGS_REQ(1));    /* 15.3.1.3.24 */
  define_method(mrb, krn, "iterator?",                  f_block_given_p_m,           ARGS_NONE());    /* 15.3.1.3.25 */
  define_method(mrb, krn, "kind_of?",                   obj_is_kind_of_m,            ARGS_REQ(1));    /* 15.3.1.3.26 */
#ifdef DEFAULT_METHOD_MISSING
  define_method(mrb, krn, "method_missing",             obj_missing,                 ARGS_ANY());     /* 15.3.1.3.30 */
#endif
  define_method(mrb, krn, "nil?",                       false,                       ARGS_NONE());    /* 15.3.1.3.32 */
  define_method(mrb, krn, "object_id",                  obj_id_m,                    ARGS_NONE());    /* 15.3.1.3.33 */
  define_method(mrb, krn, "raise",                      f_raise,                     ARGS_ANY());     /* 15.3.1.3.40 */
  define_method(mrb, krn, "remove_instance_variable",   obj_remove_instance_variable,ARGS_REQ(1));    /* 15.3.1.3.41 */
  define_method(mrb, krn, "respond_to?",                obj_respond_to,                  ARGS_ANY());     /* 15.3.1.3.43 */
  define_method(mrb, krn, "to_s",                       any_to_s,                    ARGS_NONE());    /* 15.3.1.3.46 */
  define_method(mrb, krn, "__case_eqq",                 obj_ceqq,                    ARGS_REQ(1));    /* internal */

  define_method(mrb, krn, "class_defined?",             krn_class_defined,           ARGS_REQ(1));

  include_module(mrb, mrb->object_class, mrb->kernel_module);
  define_alias(mrb, mrb->module_class, "dup", "clone"); /* XXX */
}
