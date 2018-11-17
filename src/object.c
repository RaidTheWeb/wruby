/*
** object.c - Object, NilClass, TrueClass, FalseClass class
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/class.h>
#include <mruby/numeric.h>
#include <mruby/string.h>
#include <mruby/class.h>

API bool
obj_eq(state *mrb, value v1, value v2)
{
  if (type(v1) != type(v2)) return FALSE;
  switch (type(v1)) {
  case TT_TRUE:
    return TRUE;

  case TT_FALSE:
  case TT_FIXNUM:
    return (fixnum(v1) == fixnum(v2));
  case TT_SYMBOL:
    return (symbol(v1) == symbol(v2));

#ifndef WITHOUT_FLOAT
  case TT_FLOAT:
    return (float(v1) == float(v2));
#endif

  default:
    return (ptr(v1) == ptr(v2));
  }
}

API bool
obj_equal(state *mrb, value v1, value v2)
{
  /* temporary definition */
  return obj_eq(mrb, v1, v2);
}

API bool
equal(state *mrb, value obj1, value obj2)
{
  value result;

  if (obj_eq(mrb, obj1, obj2)) return TRUE;
  result = funcall(mrb, obj1, "==", 1, obj2);
  if (test(result)) return TRUE;
  return FALSE;
}

/*
 * Document-class: NilClass
 *
 *  The class of the singleton object <code>nil</code>.
 */

/* 15.2.4.3.4  */
/*
 * call_seq:
 *   nil.nil?               -> true
 *
 * Only the object <i>nil</i> responds <code>true</code> to <code>nil?</code>.
 */

static value
true(state *mrb, value obj)
{
  return true_value();
}

/* 15.2.4.3.5  */
/*
 *  call-seq:
 *     nil.to_s    -> ""
 *
 *  Always returns the empty string.
 */

static value
nil_to_s(state *mrb, value obj)
{
  return str_new(mrb, 0, 0);
}

static value
nil_inspect(state *mrb, value obj)
{
  return str_new_lit(mrb, "nil");
}

/***********************************************************************
 *  Document-class: TrueClass
 *
 *  The global value <code>true</code> is the only instance of class
 *  <code>TrueClass</code> and represents a logically true value in
 *  boolean expressions. The class provides operators allowing
 *  <code>true</code> to be used in logical expressions.
 */

/* 15.2.5.3.1  */
/*
 *  call-seq:
 *     true & obj    -> true or false
 *
 *  And---Returns <code>false</code> if <i>obj</i> is
 *  <code>nil</code> or <code>false</code>, <code>true</code> otherwise.
 */

static value
true_and(state *mrb, value obj)
{
  bool obj2;

  get_args(mrb, "b", &obj2);

  return bool_value(obj2);
}

/* 15.2.5.3.2  */
/*
 *  call-seq:
 *     true ^ obj   -> !obj
 *
 *  Exclusive Or---Returns <code>true</code> if <i>obj</i> is
 *  <code>nil</code> or <code>false</code>, <code>false</code>
 *  otherwise.
 */

static value
true_xor(state *mrb, value obj)
{
  bool obj2;

  get_args(mrb, "b", &obj2);
  return bool_value(!obj2);
}

/* 15.2.5.3.3  */
/*
 * call-seq:
 *   true.to_s   ->  "true"
 *
 * The string representation of <code>true</code> is "true".
 */

static value
true_to_s(state *mrb, value obj)
{
  return str_new_lit(mrb, "true");
}

/* 15.2.5.3.4  */
/*
 *  call-seq:
 *     true | obj   -> true
 *
 *  Or---Returns <code>true</code>. As <i>anObject</i> is an argument to
 *  a method call, it is always evaluated; there is no short-circuit
 *  evaluation in this case.
 *
 *     true |  puts("or")
 *     true || puts("logical or")
 *
 *  <em>produces:</em>
 *
 *     or
 */

static value
true_or(state *mrb, value obj)
{
  return true_value();
}

/*
 *  Document-class: FalseClass
 *
 *  The global value <code>false</code> is the only instance of class
 *  <code>FalseClass</code> and represents a logically false value in
 *  boolean expressions. The class provides operators allowing
 *  <code>false</code> to participate correctly in logical expressions.
 *
 */

/* 15.2.4.3.1  */
/* 15.2.6.3.1  */
/*
 *  call-seq:
 *     false & obj   -> false
 *     nil & obj     -> false
 *
 *  And---Returns <code>false</code>. <i>obj</i> is always
 *  evaluated as it is the argument to a method call---there is no
 *  short-circuit evaluation in this case.
 */

static value
false_and(state *mrb, value obj)
{
  return false_value();
}

/* 15.2.4.3.2  */
/* 15.2.6.3.2  */
/*
 *  call-seq:
 *     false ^ obj    -> true or false
 *     nil   ^ obj    -> true or false
 *
 *  Exclusive Or---If <i>obj</i> is <code>nil</code> or
 *  <code>false</code>, returns <code>false</code>; otherwise, returns
 *  <code>true</code>.
 *
 */

static value
false_xor(state *mrb, value obj)
{
  bool obj2;

  get_args(mrb, "b", &obj2);
  return bool_value(obj2);
}

/* 15.2.4.3.3  */
/* 15.2.6.3.4  */
/*
 *  call-seq:
 *     false | obj   ->   true or false
 *     nil   | obj   ->   true or false
 *
 *  Or---Returns <code>false</code> if <i>obj</i> is
 *  <code>nil</code> or <code>false</code>; <code>true</code> otherwise.
 */

static value
false_or(state *mrb, value obj)
{
  bool obj2;

  get_args(mrb, "b", &obj2);
  return bool_value(obj2);
}

/* 15.2.6.3.3  */
/*
 * call-seq:
 *   false.to_s   ->  "false"
 *
 * 'nuf said...
 */

static value
false_to_s(state *mrb, value obj)
{
  return str_new_lit(mrb, "false");
}

void
init_object(state *mrb)
{
  struct RClass *n;
  struct RClass *t;
  struct RClass *f;

  mrb->nil_class   = n = define_class(mrb, "NilClass",   mrb->object_class);
  SET_INSTANCE_TT(n, TT_TRUE);
  undef_class_method(mrb, n, "new");
  define_method(mrb, n, "&",    false_and,      ARGS_REQ(1));  /* 15.2.4.3.1  */
  define_method(mrb, n, "^",    false_xor,      ARGS_REQ(1));  /* 15.2.4.3.2  */
  define_method(mrb, n, "|",    false_or,       ARGS_REQ(1));  /* 15.2.4.3.3  */
  define_method(mrb, n, "nil?", true,       ARGS_NONE());  /* 15.2.4.3.4  */
  define_method(mrb, n, "to_s", nil_to_s,       ARGS_NONE());  /* 15.2.4.3.5  */
  define_method(mrb, n, "inspect", nil_inspect, ARGS_NONE());

  mrb->true_class  = t = define_class(mrb, "TrueClass",  mrb->object_class);
  SET_INSTANCE_TT(t, TT_TRUE);
  undef_class_method(mrb, t, "new");
  define_method(mrb, t, "&",    true_and,       ARGS_REQ(1));  /* 15.2.5.3.1  */
  define_method(mrb, t, "^",    true_xor,       ARGS_REQ(1));  /* 15.2.5.3.2  */
  define_method(mrb, t, "to_s", true_to_s,      ARGS_NONE());  /* 15.2.5.3.3  */
  define_method(mrb, t, "|",    true_or,        ARGS_REQ(1));  /* 15.2.5.3.4  */
  define_method(mrb, t, "inspect", true_to_s,   ARGS_NONE());

  mrb->false_class = f = define_class(mrb, "FalseClass", mrb->object_class);
  SET_INSTANCE_TT(f, TT_TRUE);
  undef_class_method(mrb, f, "new");
  define_method(mrb, f, "&",    false_and,      ARGS_REQ(1));  /* 15.2.6.3.1  */
  define_method(mrb, f, "^",    false_xor,      ARGS_REQ(1));  /* 15.2.6.3.2  */
  define_method(mrb, f, "to_s", false_to_s,     ARGS_NONE());  /* 15.2.6.3.3  */
  define_method(mrb, f, "|",    false_or,       ARGS_REQ(1));  /* 15.2.6.3.4  */
  define_method(mrb, f, "inspect", false_to_s,  ARGS_NONE());
}

static value
inspect_type(state *mrb, value val)
{
  if (type(val) == TT_FALSE || type(val) == TT_TRUE) {
    return inspect(mrb, val);
  }
  else {
    return str_new_cstr(mrb, obj_classname(mrb, val));
  }
}

static value
convert_type(state *mrb, value val, const char *tname, const char *method, bool raise)
{
  sym m = 0;

  m = intern_cstr(mrb, method);
  if (!respond_to(mrb, val, m)) {
    if (raise) {
      raisef(mrb, E_TYPE_ERROR, "can't convert %S into %S", inspect_type(mrb, val), str_new_cstr(mrb, tname));
    }
    return nil_value();
  }
  return funcall_argv(mrb, val, m, 0, 0);
}

API value
check_to_integer(state *mrb, value val, const char *method)
{
  value v;

  if (fixnum_p(val)) return val;
  v = convert_type(mrb, val, "Integer", method, FALSE);
  if (nil_p(v) || !fixnum_p(v)) {
    return nil_value();
  }
  return v;
}

API value
convert_type(state *mrb, value val, enum vtype type, const char *tname, const char *method)
{
  value v;

  if (type(val) == type) return val;
  v = convert_type(mrb, val, tname, method, TRUE);
  if (type(v) != type) {
    raisef(mrb, E_TYPE_ERROR, "%S cannot be converted to %S by #%S", val,
               str_new_cstr(mrb, tname), str_new_cstr(mrb, method));
  }
  return v;
}

API value
check_convert_type(state *mrb, value val, enum vtype type, const char *tname, const char *method)
{
  value v;

  if (type(val) == type && type != TT_DATA && type != TT_ISTRUCT) return val;
  v = convert_type(mrb, val, tname, method, FALSE);
  if (nil_p(v) || type(v) != type) return nil_value();
  return v;
}

static const struct types {
  unsigned char type;
  const char *name;
} builtin_types[] = {
/*    {TT_NIL,  "nil"}, */
  {TT_FALSE,  "false"},
  {TT_TRUE,   "true"},
  {TT_FIXNUM, "Fixnum"},
  {TT_SYMBOL, "Symbol"},  /* :symbol */
  {TT_MODULE, "Module"},
  {TT_OBJECT, "Object"},
  {TT_CLASS,  "Class"},
  {TT_ICLASS, "iClass"},  /* internal use: mixed-in module holder */
  {TT_SCLASS, "SClass"},
  {TT_PROC,   "Proc"},
#ifndef WITHOUT_FLOAT
  {TT_FLOAT,  "Float"},
#endif
  {TT_ARRAY,  "Array"},
  {TT_HASH,   "Hash"},
  {TT_STRING, "String"},
  {TT_RANGE,  "Range"},
/*    {TT_BIGNUM,  "Bignum"}, */
  {TT_FILE,   "File"},
  {TT_DATA,   "Data"},  /* internal use: wrapped C pointers */
/*    {TT_VARMAP,  "Varmap"}, */ /* internal use: dynamic variables */
/*    {TT_NODE,  "Node"}, */ /* internal use: syntax tree node */
/*    {TT_UNDEF,  "undef"}, */ /* internal use: #undef; should not happen */
  {TT_MAXDEFINE,  0}
};

API void
check_type(state *mrb, value x, enum vtype t)
{
  const struct types *type = builtin_types;
  enum vtype xt;

  xt = type(x);
  if ((xt != t) || (xt == TT_DATA) || (xt == TT_ISTRUCT)) {
    while (type->type < TT_MAXDEFINE) {
      if (type->type == t) {
        const char *etype;

        if (nil_p(x)) {
          etype = "nil";
        }
        else if (fixnum_p(x)) {
          etype = "Fixnum";
        }
        else if (type(x) == TT_SYMBOL) {
          etype = "Symbol";
        }
        else if (immediate_p(x)) {
          etype = RSTRING_PTR(obj_as_string(mrb, x));
        }
        else {
          etype = obj_classname(mrb, x);
        }
        raisef(mrb, E_TYPE_ERROR, "wrong argument type %S (expected %S)",
                   str_new_cstr(mrb, etype), str_new_cstr(mrb, type->name));
      }
      type++;
    }
    raisef(mrb, E_TYPE_ERROR, "unknown type %S (%S given)",
               fixnum_value(t), fixnum_value(type(x)));
  }
}

/* 15.3.1.3.46 */
/*
 *  call-seq:
 *     obj.to_s    => string
 *
 *  Returns a string representing <i>obj</i>. The default
 *  <code>to_s</code> prints the object's class and an encoding of the
 *  object id. As a special case, the top-level object that is the
 *  initial execution context of Ruby programs returns "main."
 */

API value
any_to_s(state *mrb, value obj)
{
  value str = str_new_capa(mrb, 20);
  const char *cname = obj_classname(mrb, obj);

  str_cat_lit(mrb, str, "#<");
  str_cat_cstr(mrb, str, cname);
  str_cat_lit(mrb, str, ":");
  str_concat(mrb, str, ptr_to_str(mrb, ptr(obj)));
  str_cat_lit(mrb, str, ">");

  return str;
}

/*
 *  call-seq:
 *     obj.is_a?(class)       => true or false
 *     obj.kind_of?(class)    => true or false
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

API bool
obj_is_kind_of(state *mrb, value obj, struct RClass *c)
{
  struct RClass *cl = class(mrb, obj);

  switch (c->tt) {
    case TT_MODULE:
    case TT_CLASS:
    case TT_ICLASS:
    case TT_SCLASS:
      break;

    default:
      raise(mrb, E_TYPE_ERROR, "class or module required");
  }

  CLASS_ORIGIN(c);
  while (cl) {
    if (cl == c || cl->mt == c->mt)
      return TRUE;
    cl = cl->super;
  }
  return FALSE;
}

static value
to_integer(state *mrb, value val, const char *method)
{
  value v;

  if (fixnum_p(val)) return val;
  v = convert_type(mrb, val, "Integer", method, TRUE);
  if (!obj_is_kind_of(mrb, v, mrb->fixnum_class)) {
    value type = inspect_type(mrb, val);
    raisef(mrb, E_TYPE_ERROR, "can't convert %S to Integer (%S#%S gives %S)",
               type, type, str_new_cstr(mrb, method), inspect_type(mrb, v));
  }
  return v;
}

API value
to_int(state *mrb, value val)
{
  return to_integer(mrb, val, "to_int");
}

API value
convert_to_integer(state *mrb, value val, int base)
{
  value tmp;

  if (nil_p(val)) {
    if (base != 0) goto arg_error;
      raise(mrb, E_TYPE_ERROR, "can't convert nil into Integer");
  }
  switch (type(val)) {
#ifndef WITHOUT_FLOAT
    case TT_FLOAT:
      if (base != 0) goto arg_error;
      else {
        float f = float(val);
        if (FIXABLE_FLOAT(f)) {
          break;
        }
      }
      return flo_to_fixnum(mrb, val);
#endif

    case TT_FIXNUM:
      if (base != 0) goto arg_error;
      return val;

    case TT_STRING:
    string_conv:
      return str_to_inum(mrb, val, base, TRUE);

    default:
      break;
  }
  if (base != 0) {
    tmp = check_string_type(mrb, val);
    if (!nil_p(tmp)) {
      val = tmp;
      goto string_conv;
    }
arg_error:
    raise(mrb, E_ARGUMENT_ERROR, "base specified for non string value");
  }
  tmp = convert_type(mrb, val, "Integer", "to_int", FALSE);
  if (nil_p(tmp) || !fixnum_p(tmp)) {
    tmp = to_integer(mrb, val, "to_i");
  }
  return tmp;
}

API value
Integer(state *mrb, value val)
{
  return convert_to_integer(mrb, val, 0);
}

#ifndef WITHOUT_FLOAT
API value
Float(state *mrb, value val)
{
  if (nil_p(val)) {
    raise(mrb, E_TYPE_ERROR, "can't convert nil into Float");
  }
  switch (type(val)) {
    case TT_FIXNUM:
      return float_value(mrb, (float)fixnum(val));

    case TT_FLOAT:
      return val;

    case TT_STRING:
      return float_value(mrb, str_to_dbl(mrb, val, TRUE));

    default:
      return convert_type(mrb, val, TT_FLOAT, "Float", "to_f");
  }
}
#endif

API value
inspect(state *mrb, value obj)
{
  return obj_as_string(mrb, funcall(mrb, obj, "inspect", 0));
}

API bool
eql(state *mrb, value obj1, value obj2)
{
  if (obj_eq(mrb, obj1, obj2)) return TRUE;
  return test(funcall(mrb, obj1, "eql?", 1, obj2));
}
