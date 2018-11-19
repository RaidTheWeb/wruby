#include <mruby.h>
#include <mruby/error.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/range.h>

static $value
$f_caller($state *mrb, $value self)
{
  $value bt, v, length;
  $int bt_len, argc, lev, n;

  bt = $get_backtrace(mrb);
  bt_len = RARRAY_LEN(bt);
  argc = $get_args(mrb, "|oo", &v, &length);

  switch (argc) {
    case 0:
      lev = 1;
      n = bt_len - lev;
      break;
    case 1:
      if ($type(v) == $TT_RANGE) {
        $int beg, len;
        if ($range_beg_len(mrb, v, &beg, &len, bt_len, TRUE) == 1) {
          lev = beg;
          n = len;
        }
        else {
          return $nil_value();
        }
      }
      else {
        v = $to_int(mrb, v);
        lev = $fixnum(v);
        if (lev < 0) {
          $raisef(mrb, E_ARGUMENT_ERROR, "negative level (%S)", v);
        }
        n = bt_len - lev;
      }
      break;
    case 2:
      lev = $fixnum($to_int(mrb, v));
      n = $fixnum($to_int(mrb, length));
      if (lev < 0) {
        $raisef(mrb, E_ARGUMENT_ERROR, "negative level (%S)", v);
      }
      if (n < 0) {
        $raisef(mrb, E_ARGUMENT_ERROR, "negative size (%S)", length);
      }
      break;
    default:
      lev = n = 0;
      break;
  }

  if (n == 0) {
    return $ary_new(mrb);
  }

  return $funcall(mrb, bt, "[]", 2, $fixnum_value(lev), $fixnum_value(n));
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
static $value
$f_method($state *mrb, $value self)
{
  $callinfo *ci = mrb->c->ci;
  ci--;
  if (ci->mid)
    return $symbol_value(ci->mid);
  else
    return $nil_value();
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
static $value
$f_integer($state *mrb, $value self)
{
  $value arg;
  $int base = 0;

  $get_args(mrb, "o|i", &arg, &base);
  return $convert_to_integer(mrb, arg, base);
}

#ifndef $WITHOUT_FLOAT
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
static $value
$f_float($state *mrb, $value self)
{
  $value arg;

  $get_args(mrb, "o", &arg);
  return $Float(mrb, arg);
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
static $value
$f_string($state *mrb, $value self)
{
  $value arg, tmp;

  $get_args(mrb, "o", &arg);
  tmp = $check_convert_type(mrb, arg, $TT_STRING, "String", "to_str");
  if ($nil_p(tmp)) {
    tmp = $check_convert_type(mrb, arg, $TT_STRING, "String", "to_s");
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
static $value
$f_array($state *mrb, $value self)
{
  $value arg, tmp;

  $get_args(mrb, "o", &arg);
  tmp = $check_convert_type(mrb, arg, $TT_ARRAY, "Array", "to_ary");
  if ($nil_p(tmp)) {
    tmp = $check_convert_type(mrb, arg, $TT_ARRAY, "Array", "to_a");
  }
  if ($nil_p(tmp)) {
    return $ary_new_from_values(mrb, 1, &arg);
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
static $value
$f_hash($state *mrb, $value self)
{
  $value arg, tmp;

  $get_args(mrb, "o", &arg);
  if ($nil_p(arg)) {
    return $hash_new(mrb);
  }
  tmp = $check_convert_type(mrb, arg, $TT_HASH, "Hash", "to_hash");
  if ($nil_p(tmp)) {
    if ($array_p(arg) && RARRAY_LEN(arg) == 0) {
      return $hash_new(mrb);
    }
    $raisef(mrb, E_TYPE_ERROR, "can't convert %S into Hash",
      $str_new_cstr(mrb, $obj_classname(mrb, arg)));
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
static $value
$f_itself($state *mrb, $value self)
{
  return self;
}

void
$mruby_kernel_ext_gem_init($state *mrb)
{
  struct RClass *krn = mrb->kernel_module;

  $define_module_function(mrb, krn, "fail", $f_raise, $ARGS_OPT(2));
  $define_module_function(mrb, krn, "caller", $f_caller, $ARGS_OPT(2));
  $define_method(mrb, krn, "__method__", $f_method, $ARGS_NONE());
  $define_module_function(mrb, krn, "Integer", $f_integer, $ARGS_ANY());
#ifndef $WITHOUT_FLOAT
  $define_module_function(mrb, krn, "Float", $f_float, $ARGS_REQ(1));
#endif
  $define_module_function(mrb, krn, "String", $f_string, $ARGS_REQ(1));
  $define_module_function(mrb, krn, "Array", $f_array, $ARGS_REQ(1));
  $define_module_function(mrb, krn, "Hash", $f_hash, $ARGS_REQ(1));
  $define_module_function(mrb, krn, "itself", $f_itself, $ARGS_NONE());
}

void
$mruby_kernel_ext_gem_final($state *mrb)
{
}
