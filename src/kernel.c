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

MRB_API _bool
_func_basic_p(_state *mrb, _value obj, _sym mid, _func_t func)
{
  struct RClass *c = _class(mrb, obj);
  _method_t m = _method_search_vm(mrb, &c, mid);
  struct RProc *p;

  if (MRB_METHOD_UNDEF_P(m)) return FALSE;
  if (MRB_METHOD_FUNC_P(m))
    return MRB_METHOD_FUNC(m) == func;
  p = MRB_METHOD_PROC(m);
  if (MRB_PROC_CFUNC_P(p) && (MRB_PROC_CFUNC(p) == func))
    return TRUE;
  return FALSE;
}

static _bool
_obj_basic_to_s_p(_state *mrb, _value obj)
{
  return _func_basic_p(mrb, obj, _intern_lit(mrb, "to_s"), _any_to_s);
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
MRB_API _value
_obj_inspect(_state *mrb, _value obj)
{
  if ((_type(obj) == MRB_TT_OBJECT) && _obj_basic_to_s_p(mrb, obj)) {
    return _obj_iv_inspect(mrb, _obj_ptr(obj));
  }
  return _any_to_s(mrb, obj);
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
static _value
_equal_m(_state *mrb, _value self)
{
  _value arg;

  _get_args(mrb, "o", &arg);
  return _bool_value(_equal(mrb, self, arg));
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
_value
_obj_id_m(_state *mrb, _value self)
{
  return _fixnum_value(_obj_id(self));
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
static _value
_f_block_given_p_m(_state *mrb, _value self)
{
  _callinfo *ci = &mrb->c->ci[-1];
  _callinfo *cibase = mrb->c->cibase;
  _value *bp;
  struct RProc *p;

  if (ci <= cibase) {
    /* toplevel does not have block */
    return _false_value();
  }
  p = ci->proc;
  /* search method/class/module proc */
  while (p) {
    if (MRB_PROC_SCOPE_P(p)) break;
    p = p->upper;
  }
  if (p == NULL) return _false_value();
  /* search ci corresponding to proc */
  while (cibase < ci) {
    if (ci->proc == p) break;
    ci--;
  }
  if (ci == cibase) {
    return _false_value();
  }
  else if (ci->env) {
    struct REnv *e = ci->env;
    int bidx;

    /* top-level does not have block slot (always false) */
    if (e->stack == mrb->c->stbase)
      return _false_value();
    /* use saved block arg position */
    bidx = MRB_ENV_BIDX(e);
    /* bidx may be useless (e.g. define_method) */
    if (bidx >= MRB_ENV_STACK_LEN(e))
      return _false_value();
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
  if (_nil_p(*bp))
    return _false_value();
  return _true_value();
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
static _value
_obj_class_m(_state *mrb, _value self)
{
  return _obj_value(_obj_class(mrb, self));
}

static struct RClass*
_singleton_class_clone(_state *mrb, _value obj)
{
  struct RClass *klass = _basic_ptr(obj)->c;

  if (klass->tt != MRB_TT_SCLASS)
    return klass;
  else {
    /* copy singleton(unnamed) class */
    struct RClass *clone = (struct RClass*)_obj_alloc(mrb, klass->tt, mrb->class_class);

    switch (_type(obj)) {
    case MRB_TT_CLASS:
    case MRB_TT_SCLASS:
      break;
    default:
      clone->c = _singleton_class_clone(mrb, _obj_value(klass));
      break;
    }
    clone->super = klass->super;
    if (klass->iv) {
      _iv_copy(mrb, _obj_value(clone), _obj_value(klass));
      _obj_iv_set(mrb, (struct RObject*)clone, _intern_lit(mrb, "__attached__"), obj);
    }
    if (klass->mt) {
      clone->mt = kh_copy(mt, mrb, klass->mt);
    }
    else {
      clone->mt = kh_init(mt, mrb);
    }
    clone->tt = MRB_TT_SCLASS;
    return clone;
  }
}

static void
copy_class(_state *mrb, _value dst, _value src)
{
  struct RClass *dc = _class_ptr(dst);
  struct RClass *sc = _class_ptr(src);
  /* if the origin is not the same as the class, then the origin and
     the current class need to be copied */
  if (sc->flags & MRB_FL_CLASS_IS_PREPENDED) {
    struct RClass *c0 = sc->super;
    struct RClass *c1 = dc;

    /* copy prepended iclasses */
    while (!(c0->flags & MRB_FL_CLASS_IS_ORIGIN)) {
      c1->super = _class_ptr(_obj_dup(mrb, _obj_value(c0)));
      c1 = c1->super;
      c0 = c0->super;
    }
    c1->super = _class_ptr(_obj_dup(mrb, _obj_value(c0)));
    c1->super->flags |= MRB_FL_CLASS_IS_ORIGIN;
  }
  if (sc->mt) {
    dc->mt = kh_copy(mt, mrb, sc->mt);
  }
  else {
    dc->mt = kh_init(mt, mrb);
  }
  dc->super = sc->super;
  MRB_SET_INSTANCE_TT(dc, MRB_INSTANCE_TT(sc));
}

static void
init_copy(_state *mrb, _value dest, _value obj)
{
  switch (_type(obj)) {
    case MRB_TT_ICLASS:
      copy_class(mrb, dest, obj);
      return;
    case MRB_TT_CLASS:
    case MRB_TT_MODULE:
      copy_class(mrb, dest, obj);
      _iv_copy(mrb, dest, obj);
      _iv_remove(mrb, dest, _intern_lit(mrb, "__classname__"));
      break;
    case MRB_TT_OBJECT:
    case MRB_TT_SCLASS:
    case MRB_TT_HASH:
    case MRB_TT_DATA:
    case MRB_TT_EXCEPTION:
      _iv_copy(mrb, dest, obj);
      break;
    case MRB_TT_ISTRUCT:
      _istruct_copy(dest, obj);
      break;

    default:
      break;
  }
  _funcall(mrb, dest, "initialize_copy", 1, obj);
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
MRB_API _value
_obj_clone(_state *mrb, _value self)
{
  struct RObject *p;
  _value clone;

  if (_immediate_p(self)) {
    _raisef(mrb, E_TYPE_ERROR, "can't clone %S", self);
  }
  if (_type(self) == MRB_TT_SCLASS) {
    _raise(mrb, E_TYPE_ERROR, "can't clone singleton class");
  }
  p = (struct RObject*)_obj_alloc(mrb, _type(self), _obj_class(mrb, self));
  p->c = _singleton_class_clone(mrb, self);
  _field_write_barrier(mrb, (struct RBasic*)p, (struct RBasic*)p->c);
  clone = _obj_value(p);
  init_copy(mrb, clone, self);
  p->flags |= _obj_ptr(self)->flags & MRB_FL_OBJ_IS_FROZEN;

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

MRB_API _value
_obj_dup(_state *mrb, _value obj)
{
  struct RBasic *p;
  _value dup;

  if (_immediate_p(obj)) {
    _raisef(mrb, E_TYPE_ERROR, "can't dup %S", obj);
  }
  if (_type(obj) == MRB_TT_SCLASS) {
    _raise(mrb, E_TYPE_ERROR, "can't dup singleton class");
  }
  p = _obj_alloc(mrb, _type(obj), _obj_class(mrb, obj));
  dup = _obj_value(p);
  init_copy(mrb, dup, obj);

  return dup;
}

static _value
_obj_extend(_state *mrb, _int argc, _value *argv, _value obj)
{
  _int i;

  if (argc == 0) {
    _raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (at least 1)");
  }
  for (i = 0; i < argc; i++) {
    _check_type(mrb, argv[i], MRB_TT_MODULE);
  }
  while (argc--) {
    _funcall(mrb, argv[argc], "extend_object", 1, obj);
    _funcall(mrb, argv[argc], "extended", 1, obj);
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
static _value
_obj_extend_m(_state *mrb, _value self)
{
  _value *argv;
  _int argc;

  _get_args(mrb, "*", &argv, &argc);
  return _obj_extend(mrb, argc, argv, self);
}

static _value
_obj_freeze(_state *mrb, _value self)
{
  struct RBasic *b;

  switch (_type(self)) {
    case MRB_TT_FALSE:
    case MRB_TT_TRUE:
    case MRB_TT_FIXNUM:
    case MRB_TT_SYMBOL:
#ifndef MRB_WITHOUT_FLOAT
    case MRB_TT_FLOAT:
#endif
      return self;
    default:
      break;
  }

  b = _basic_ptr(self);
  if (!MRB_FROZEN_P(b)) {
    MRB_SET_FROZEN_FLAG(b);
  }
  return self;
}

static _value
_obj_frozen(_state *mrb, _value self)
{
  struct RBasic *b;

  switch (_type(self)) {
    case MRB_TT_FALSE:
    case MRB_TT_TRUE:
    case MRB_TT_FIXNUM:
    case MRB_TT_SYMBOL:
#ifndef MRB_WITHOUT_FLOAT
    case MRB_TT_FLOAT:
#endif
      return _true_value();
    default:
      break;
  }

  b = _basic_ptr(self);
  if (!MRB_FROZEN_P(b)) {
    return _false_value();
  }
  return _true_value();
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
MRB_API _value
_obj_hash(_state *mrb, _value self)
{
  return _fixnum_value(_obj_id(self));
}

/* 15.3.1.3.16 */
static _value
_obj_init_copy(_state *mrb, _value self)
{
  _value orig;

  _get_args(mrb, "o", &orig);
  if (_obj_equal(mrb, self, orig)) return self;
  if ((_type(self) != _type(orig)) || (_obj_class(mrb, self) != _obj_class(mrb, orig))) {
      _raise(mrb, E_TYPE_ERROR, "initialize_copy should take same class object");
  }
  return self;
}


MRB_API _bool
_obj_is_instance_of(_state *mrb, _value obj, struct RClass* c)
{
  if (_obj_class(mrb, obj) == c) return TRUE;
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
static _value
obj_is_instance_of(_state *mrb, _value self)
{
  _value arg;

  _get_args(mrb, "C", &arg);

  return _bool_value(_obj_is_instance_of(mrb, self, _class_ptr(arg)));
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
static _value
_obj_is_kind_of_m(_state *mrb, _value self)
{
  _value arg;

  _get_args(mrb, "C", &arg);

  return _bool_value(_obj_is_kind_of(mrb, self, _class_ptr(arg)));
}

KHASH_DECLARE(st, _sym, char, FALSE)
KHASH_DEFINE(st, _sym, char, FALSE, kh_int_hash_func, kh_int_hash_equal)

/* 15.3.1.3.32 */
/*
 * call_seq:
 *   nil.nil?               -> true
 *   <anything_else>.nil?   -> false
 *
 * Only the object <i>nil</i> responds <code>true</code> to <code>nil?</code>.
 */
static _value
_false(_state *mrb, _value self)
{
  return _false_value();
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
MRB_API _value
_f_raise(_state *mrb, _value self)
{
  _value a[2], exc;
  _int argc;


  argc = _get_args(mrb, "|oo", &a[0], &a[1]);
  switch (argc) {
  case 0:
    _raise(mrb, E_RUNTIME_ERROR, "");
    break;
  case 1:
    if (_string_p(a[0])) {
      a[1] = a[0];
      argc = 2;
      a[0] = _obj_value(E_RUNTIME_ERROR);
    }
    /* fall through */
  default:
    exc = _make_exception(mrb, argc, a);
    _exc_raise(mrb, exc);
    break;
  }
  return _nil_value();            /* not reached */
}

static _value
_krn_class_defined(_state *mrb, _value self)
{
  _value str;

  _get_args(mrb, "S", &str);
  return _bool_value(_class_defined(mrb, RSTRING_PTR(str)));
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
static _value
_obj_remove_instance_variable(_state *mrb, _value self)
{
  _sym sym;
  _value val;

  _get_args(mrb, "n", &sym);
  _iv_name_sym_check(mrb, sym);
  val = _iv_remove(mrb, self, sym);
  if (_undef_p(val)) {
    _name_error(mrb, sym, "instance variable %S not defined", _sym2str(mrb, sym));
  }
  return val;
}

void
_method_missing(_state *mrb, _sym name, _value self, _value args)
{
  _no_method_error(mrb, name, args, "undefined method '%S'", _sym2str(mrb, name));
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
#ifdef MRB_DEFAULT_METHOD_MISSING
static _value
_obj_missing(_state *mrb, _value mod)
{
  _sym name;
  _value *a;
  _int alen;

  _get_args(mrb, "n*!", &name, &a, &alen);
  _method_missing(mrb, name, mod, _ary_new_from_values(mrb, alen, a));
  /* not reached */
  return _nil_value();
}
#endif

static inline _bool
basic_obj_respond_to(_state *mrb, _value obj, _sym id, int pub)
{
  return _respond_to(mrb, obj, id);
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
static _value
obj_respond_to(_state *mrb, _value self)
{
  _value mid;
  _sym id, rtm_id;
  _bool priv = FALSE, respond_to_p = TRUE;

  _get_args(mrb, "o|b", &mid, &priv);

  if (_symbol_p(mid)) {
    id = _symbol(mid);
  }
  else {
    _value tmp;
    if (_string_p(mid)) {
      tmp = _check_intern_str(mrb, mid);
    }
    else {
      tmp = _check_string_type(mrb, mid);
      if (_nil_p(tmp)) {
        tmp = _inspect(mrb, mid);
        _raisef(mrb, E_TYPE_ERROR, "%S is not a symbol", tmp);
      }
      tmp = _check_intern_str(mrb, tmp);
    }
    if (_nil_p(tmp)) {
      respond_to_p = FALSE;
    }
    else {
      id = _symbol(tmp);
    }
  }

  if (respond_to_p) {
    respond_to_p = basic_obj_respond_to(mrb, self, id, !priv);
  }

  if (!respond_to_p) {
    rtm_id = _intern_lit(mrb, "respond_to_missing?");
    if (basic_obj_respond_to(mrb, self, rtm_id, !priv)) {
      _value args[2], v;
      args[0] = mid;
      args[1] = _bool_value(priv);
      v = _funcall_argv(mrb, self, rtm_id, 2, args);
      return _bool_value(_bool(v));
    }
  }
  return _bool_value(respond_to_p);
}

static _value
_obj_ceqq(_state *mrb, _value self)
{
  _value v;
  _int i, len;
  _sym eqq = _intern_lit(mrb, "===");
  _value ary = _ary_splat(mrb, self);

  _get_args(mrb, "o", &v);
  len = RARRAY_LEN(ary);
  for (i=0; i<len; i++) {
    _value c = _funcall_argv(mrb, _ary_entry(ary, i), eqq, 1, &v);
    if (_test(c)) return _true_value();
  }
  return _false_value();
}

_value _obj_equal_m(_state *mrb, _value);
void
_init_kernel(_state *mrb)
{
  struct RClass *krn;

  mrb->kernel_module = krn = _define_module(mrb, "Kernel");                                                    /* 15.3.1 */
  _define_class_method(mrb, krn, "block_given?",         _f_block_given_p_m,           MRB_ARGS_NONE());    /* 15.3.1.2.2  */
  _define_class_method(mrb, krn, "iterator?",            _f_block_given_p_m,           MRB_ARGS_NONE());    /* 15.3.1.2.5  */
;     /* 15.3.1.2.11 */
  _define_class_method(mrb, krn, "raise",                _f_raise,                     MRB_ARGS_OPT(2));    /* 15.3.1.2.12 */


  _define_method(mrb, krn, "===",                        _equal_m,                     MRB_ARGS_REQ(1));    /* 15.3.1.3.2  */
  _define_method(mrb, krn, "block_given?",               _f_block_given_p_m,           MRB_ARGS_NONE());    /* 15.3.1.3.6  */
  _define_method(mrb, krn, "class",                      _obj_class_m,                 MRB_ARGS_NONE());    /* 15.3.1.3.7  */
  _define_method(mrb, krn, "clone",                      _obj_clone,                   MRB_ARGS_NONE());    /* 15.3.1.3.8  */
  _define_method(mrb, krn, "dup",                        _obj_dup,                     MRB_ARGS_NONE());    /* 15.3.1.3.9  */
  _define_method(mrb, krn, "eql?",                       _obj_equal_m,                 MRB_ARGS_REQ(1));    /* 15.3.1.3.10 */
  _define_method(mrb, krn, "equal?",                     _obj_equal_m,                 MRB_ARGS_REQ(1));    /* 15.3.1.3.11 */
  _define_method(mrb, krn, "extend",                     _obj_extend_m,                MRB_ARGS_ANY());     /* 15.3.1.3.13 */
  _define_method(mrb, krn, "freeze",                     _obj_freeze,                  MRB_ARGS_NONE());
  _define_method(mrb, krn, "frozen?",                    _obj_frozen,                  MRB_ARGS_NONE());
  _define_method(mrb, krn, "global_variables",           _f_global_variables,          MRB_ARGS_NONE());    /* 15.3.1.3.14 */
  _define_method(mrb, krn, "hash",                       _obj_hash,                    MRB_ARGS_NONE());    /* 15.3.1.3.15 */
  _define_method(mrb, krn, "initialize_copy",            _obj_init_copy,               MRB_ARGS_REQ(1));    /* 15.3.1.3.16 */
  _define_method(mrb, krn, "inspect",                    _obj_inspect,                 MRB_ARGS_NONE());    /* 15.3.1.3.17 */
  _define_method(mrb, krn, "instance_of?",               obj_is_instance_of,              MRB_ARGS_REQ(1));    /* 15.3.1.3.19 */

  _define_method(mrb, krn, "is_a?",                      _obj_is_kind_of_m,            MRB_ARGS_REQ(1));    /* 15.3.1.3.24 */
  _define_method(mrb, krn, "iterator?",                  _f_block_given_p_m,           MRB_ARGS_NONE());    /* 15.3.1.3.25 */
  _define_method(mrb, krn, "kind_of?",                   _obj_is_kind_of_m,            MRB_ARGS_REQ(1));    /* 15.3.1.3.26 */
#ifdef MRB_DEFAULT_METHOD_MISSING
  _define_method(mrb, krn, "method_missing",             _obj_missing,                 MRB_ARGS_ANY());     /* 15.3.1.3.30 */
#endif
  _define_method(mrb, krn, "nil?",                       _false,                       MRB_ARGS_NONE());    /* 15.3.1.3.32 */
  _define_method(mrb, krn, "object_id",                  _obj_id_m,                    MRB_ARGS_NONE());    /* 15.3.1.3.33 */
  _define_method(mrb, krn, "raise",                      _f_raise,                     MRB_ARGS_ANY());     /* 15.3.1.3.40 */
  _define_method(mrb, krn, "remove_instance_variable",   _obj_remove_instance_variable,MRB_ARGS_REQ(1));    /* 15.3.1.3.41 */
  _define_method(mrb, krn, "respond_to?",                obj_respond_to,                  MRB_ARGS_ANY());     /* 15.3.1.3.43 */
  _define_method(mrb, krn, "to_s",                       _any_to_s,                    MRB_ARGS_NONE());    /* 15.3.1.3.46 */
  _define_method(mrb, krn, "__case_eqq",                 _obj_ceqq,                    MRB_ARGS_REQ(1));    /* internal */

  _define_method(mrb, krn, "class_defined?",             _krn_class_defined,           MRB_ARGS_REQ(1));

  _include_module(mrb, mrb->object_class, mrb->kernel_module);
  _define_alias(mrb, mrb->module_class, "dup", "clone"); /* XXX */
}
