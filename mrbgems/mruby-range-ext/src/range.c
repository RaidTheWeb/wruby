#include <mruby.h>
#include <mruby/range.h>
#include <math.h>

static $bool
r_le($state *mrb, $value a, $value b)
{
  $value r = $funcall(mrb, a, "<=>", 1, b); /* compare result */
  /* output :a < b => -1, a = b =>  0, a > b => +1 */

  if ($fixnum_p(r)) {
    $int c = $fixnum(r);
    if (c == 0 || c == -1) return TRUE;
  }

  return FALSE;
}

static $bool
r_lt($state *mrb, $value a, $value b)
{
  $value r = $funcall(mrb, a, "<=>", 1, b);
  /* output :a < b => -1, a = b =>  0, a > b => +1 */

  return $fixnum_p(r) && $fixnum(r) == -1;
}

/*
 *  call-seq:
 *     rng.cover?(obj)  ->  true or false
 *
 *  Returns <code>true</code> if +obj+ is between the begin and end of
 *  the range.
 *
 *  This tests <code>begin <= obj <= end</code> when #exclude_end? is +false+
 *  and <code>begin <= obj < end</code> when #exclude_end? is +true+.
 *
 *     ("a".."z").cover?("c")    #=> true
 *     ("a".."z").cover?("5")    #=> false
 *     ("a".."z").cover?("cc")   #=> true
 */
static $value
$range_cover($state *mrb, $value range)
{
  $value val;
  struct RRange *r = $range_ptr(mrb, range);
  $value beg, end;

  $get_args(mrb, "o", &val);

  beg = r->edges->beg;
  end = r->edges->end;

  if (r_le(mrb, beg, val)) {
    if (r->excl) {
      if (r_lt(mrb, val, end))
        return $true_value();
    }
    else {
      if (r_le(mrb, val, end))
        return $true_value();
    }
  }

  return $false_value();
}

/*
 *  call-seq:
 *     rng.last    -> obj
 *     rng.last(n) -> an_array
 *
 *  Returns the last object in the range,
 *  or an array of the last +n+ elements.
 *
 *  Note that with no arguments +last+ will return the object that defines
 *  the end of the range even if #exclude_end? is +true+.
 *
 *    (10..20).last      #=> 20
 *    (10...20).last     #=> 20
 *    (10..20).last(3)   #=> [18, 19, 20]
 *    (10...20).last(3)  #=> [17, 18, 19]
 */
static $value
$range_last($state *mrb, $value range)
{
  $value num;
  $value array;
  struct RRange *r = $range_ptr(mrb, range);

  if ($get_args(mrb, "|o", &num) == 0) {
    return r->edges->end;
  }

  array = $funcall(mrb, range, "to_a", 0);
  return $funcall(mrb, array, "last", 1, $to_int(mrb, num));
}

/*
 *  call-seq:
 *     rng.size                   -> num
 *
 *  Returns the number of elements in the range. Both the begin and the end of
 *  the Range must be Numeric, otherwise nil is returned.
 *
 *    (10..20).size    #=> 11
 *    ('a'..'z').size  #=> nil
 */

static $value
$range_size($state *mrb, $value range)
{
  struct RRange *r = $range_ptr(mrb, range);
  $value beg, end;
  $float beg_f, end_f;
  $bool num_p = TRUE;
  $bool excl;

  beg = r->edges->beg;
  end = r->edges->end;
  excl = r->excl;
  if ($fixnum_p(beg)) {
    beg_f = ($float)$fixnum(beg);
  }
  else if ($float_p(beg)) {
    beg_f = $float(beg);
  }
  else {
    num_p = FALSE;
  }
  if ($fixnum_p(end)) {
    end_f = ($float)$fixnum(end);
  }
  else if ($float_p(end)) {
    end_f = $float(end);
  }
  else {
    num_p = FALSE;
  }
  if (num_p) {
    $float n = end_f - beg_f;
    $float err = (fabs(beg_f) + fabs(end_f) + fabs(end_f-beg_f)) * $FLOAT_EPSILON;

    if (err>0.5) err=0.5;
    if (excl) {
      if (n<=0) return $fixnum_value(0);
      if (n<1)
        n = 0;
      else
        n = floor(n - err);
    }
    else {
      if (n<0) return $fixnum_value(0);
      n = floor(n + err);
    }
    if (isinf(n+1))
      return $float_value(mrb, INFINITY);
    return $fixnum_value(($int)n+1);
  }
  return $nil_value();
}

void
$mruby_range_ext_gem_init($state* mrb)
{
  struct RClass * s = $class_get(mrb, "Range");

  $define_method(mrb, s, "cover?", $range_cover, $ARGS_REQ(1));
  $define_method(mrb, s, "last",   $range_last,  $ARGS_OPT(1));
  $define_method(mrb, s, "size",   $range_size,  $ARGS_NONE());
}

void
$mruby_range_ext_gem_final($state* mrb)
{
}
