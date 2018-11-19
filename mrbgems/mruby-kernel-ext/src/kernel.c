#include <mruby.h>
#include <mruby/error.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/range.h>

static _value
_f_caller(_state *mrb, _value self)
{
  _value bt, v, length;
  _int bt_len, argc, lev, n;

  bt = _get_backtrace(mrb);
  bt_len = RARRAY_LEN(bt);
  argc = _get_args(mrb, "|oo", &v, &length);

  switch (argc) {
    case 0:
      lev = 1;
      n = bt_len - lev;
      break;
    case 1:
      if (_type(v) == MRB_TT_RANGE) {
        _int beg, len;
        if (_range_beg_len(mrb, v, &beg, &len, bt_len, TRUE) == 1) {
          lev = beg;
          n = len;
        }
        else {
          return _nil_value();
        }
      }
      else {
        v = _to_int(mrb, v);
        lev = _fixnum(v);
        if (lev < 0) {
          _raisef(mrb, E_ARGUMENT_ERROR, "negative level (%S)", v);
        }
        n = bt_len - lev;
      }
      break;
    case 2:
      lev = _fixnum(_to_int(mrb, v));
      n = _fixnum(_to_int(mrb, length));
      if (lev < 0) {
        _raisef(mrb, E_ARGUMENT_ERROR, "negative level (%S)", v);
      }
      if (n < 0) {
        _raisef(mrb, E_ARGUMENT_ERROR, "negative size (%S)", length);
      }
      break;
    default:
      lev = n = 0;
      break;
  }

  if (n == 0) {
    return _ary_new(mrb);
  }

  return _funcall(mrb, bt, "[]", 2, _fixnum_value(lev), _fixnum_value(n));
}

/*
 *  call-seq:
 *     __method__         -> symbol
 *
 *  Returns the name at the definition of the current method as a
 *  Symbol.
 *  If called outside of a method, it returns <code>nil</code>.
 *
 */
static _value
_f_method(_state *mrb, _value self)
{
  _callinfo *ci = mrb->c->ci;
  ci--;
  if (ci->mid)
    return _symbol_value(ci->mid);
  else
    return _nil_value();
}

/*
 *  call-seq:
 *     Integer(arg,base=0)    -> integer
 *
 *  Converts <i>arg</i> to a <code>Fixnum</code>.
 *  Numeric types are converted directly (with floating point numbers
 *  being truncated).    <i>base</i> (0, or between 2 and 36) is a base for
 *  integer string representation.  If <i>arg</i> is a <code>String</code>,
 *  when <i>base</i> is omitted or equals to zero, radix indicators
 *  (<code>0</code>, <code>0b</code>, and <code>0x</code>) are honored.
 *  In any case, strings should be strictly conformed to numeric
 *  representation. This behavior is different from that of
 *  <code>String#to_i</code>.  Non string values will be converted using
 *  <code>to_int</code>, and <code>to_i</code>. Passing <code>nil</code>
 *  raises a TypeError.
 *
 *     Integer(123.999)    #=> 123
 *     Integer("0x1a")     #=> 26
 *     Integer(Time.new)   #=> 1204973019
 *     Integer("0930", 10) #=> 930
 *     Integer("111", 2)   #=> 7
 *     Integer(nil)        #=> TypeError
 */
static _value
_f_integer(_state *mrb, _value self)
{
  _value arg;
  _int base = 0;

  _get_args(mrb, "o|i", &arg, &base);
  return _convert_to_integer(mrb, arg, base);
}

#ifndef MRB_WITHOUT_FLOAT
/*
 *  call-seq:
 *     Float(arg)    -> float
 *
 *  Returns <i>arg</i> converted to a float. Numeric types are converted
 *  directly, the rest are converted using <i>arg</i>.to_f.
 *
 *     Float(1)           #=> 1.0
 *     Float(123.456)     #=> 123.456
 *     Float("123.456")   #=> 123.456
 *     Float(nil)         #=> TypeError
 */
static _value
_f_float(_state *mrb, _value self)
{
  _value arg;

  _get_args(mrb, "o", &arg);
  return _Float(mrb, arg);
}
#endif

/*
 *  call-seq:
 *     String(arg)   -> string
 *
 *  Returns <i>arg</i> as an <code>String</code>.
 *
 *  First tries to call its <code>to_str</code> method, then its to_s method.
 *
 *     String(self)        #=> "main"
 *     String(self.class)  #=> "Object"
 *     String(123456)      #=> "123456"
 */
static _value
_f_string(_state *mrb, _value self)
{
  _value arg, tmp;

  _get_args(mrb, "o", &arg);
  tmp = _check_convert_type(mrb, arg, MRB_TT_STRING, "String", "to_str");
  if (_nil_p(tmp)) {
    tmp = _check_convert_type(mrb, arg, MRB_TT_STRING, "String", "to_s");
  }
  return tmp;
}

/*
 *  call-seq:
 *     Array(arg)    -> array
 *
 *  Returns +arg+ as an Array.
 *
 *  First tries to call Array#to_ary on +arg+, then Array#to_a.
 *
 *     Array(1..5)   #=> [1, 2, 3, 4, 5]
 *
 */
static _value
_f_array(_state *mrb, _value self)
{
  _value arg, tmp;

  _get_args(mrb, "o", &arg);
  tmp = _check_convert_type(mrb, arg, MRB_TT_ARRAY, "Array", "to_ary");
  if (_nil_p(tmp)) {
    tmp = _check_convert_type(mrb, arg, MRB_TT_ARRAY, "Array", "to_a");
  }
  if (_nil_p(tmp)) {
    return _ary_new_from_values(mrb, 1, &arg);
  }

  return tmp;
}

/*
 *  call-seq:
 *     Hash(arg)    -> hash
 *
 *  Converts <i>arg</i> to a <code>Hash</code> by calling
 *  <i>arg</i><code>.to_hash</code>. Returns an empty <code>Hash</code> when
 *  <i>arg</i> is <tt>nil</tt> or <tt>[]</tt>.
 *
 *      Hash([])          #=> {}
 *      Hash(nil)         #=> {}
 *      Hash(key: :value) #=> {:key => :value}
 *      Hash([1, 2, 3])   #=> TypeError
 *
 */
static _value
_f_hash(_state *mrb, _value self)
{
  _value arg, tmp;

  _get_args(mrb, "o", &arg);
  if (_nil_p(arg)) {
    return _hash_new(mrb);
  }
  tmp = _check_convert_type(mrb, arg, MRB_TT_HASH, "Hash", "to_hash");
  if (_nil_p(tmp)) {
    if (_array_p(arg) && RARRAY_LEN(arg) == 0) {
      return _hash_new(mrb);
    }
    _raisef(mrb, E_TYPE_ERROR, "can't convert %S into Hash",
      _str_new_cstr(mrb, _obj_classname(mrb, arg)));
  }
  return tmp;
}

/*
 *  call-seq:
 *     obj.itself -> an_object
 *
 *  Returns <i>obj</i>.
 *
 *      string = 'my string' #=> "my string"
 *      string.itself.object_id == string.object_id #=> true
 *
 */
static _value
_f_itself(_state *mrb, _value self)
{
  return self;
}

void
_mruby_kernel_ext_gem_init(_state *mrb)
{
  struct RClass *krn = mrb->kernel_module;

  _define_module_function(mrb, krn, "fail", _f_raise, MRB_ARGS_OPT(2));
  _define_module_function(mrb, krn, "caller", _f_caller, MRB_ARGS_OPT(2));
  _define_method(mrb, krn, "__method__", _f_method, MRB_ARGS_NONE());
  _define_module_function(mrb, krn, "Integer", _f_integer, MRB_ARGS_ANY());
#ifndef MRB_WITHOUT_FLOAT
  _define_module_function(mrb, krn, "Float", _f_float, MRB_ARGS_REQ(1));
#endif
  _define_module_function(mrb, krn, "String", _f_string, MRB_ARGS_REQ(1));
  _define_module_function(mrb, krn, "Array", _f_array, MRB_ARGS_REQ(1));
  _define_module_function(mrb, krn, "Hash", _f_hash, MRB_ARGS_REQ(1));
  _define_module_function(mrb, krn, "itself", _f_itself, MRB_ARGS_NONE());
}

void
_mruby_kernel_ext_gem_final(_state *mrb)
{
}
