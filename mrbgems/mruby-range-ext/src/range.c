#include <mruby.h>
#include <mruby/range.h>
#include <math.h>

static _bool
r_le(_state *mrb, _value a, _value b)
{
  _value r = _funcall(mrb, a, "<=>", 1, b); /* compare result */
  /* output :a < b => -1, a = b =>  0, a > b => +1 */

  if (_fixnum_p(r)) {
    _int c = _fixnum(r);
    if (c == 0 || c == -1) return TRUE;
  }

  return FALSE;
}

static _bool
r_lt(_state *mrb, _value a, _value b)
{
  _value r = _funcall(mrb, a, "<=>", 1, b);
  /* output :a < b => -1, a = b =>  0, a > b => +1 */

  return _fixnum_p(r) && _fixnum(r) == -1;
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
static _value
_range_cover(_state *mrb, _value range)
{
  _value val;
  struct RRange *r = _range_ptr(mrb, range);
  _value beg, end;

  _get_args(mrb, "o", &val);

  beg = r->edges->beg;
  end = r->edges->end;

  if (r_le(mrb, beg, val)) {
    if (r->excl) {
      if (r_lt(mrb, val, end))
        return _true_value();
    }
    else {
      if (r_le(mrb, val, end))
        return _true_value();
    }
  }

  return _false_value();
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
static _value
_range_last(_state *mrb, _value range)
{
  _value num;
  _value array;
  struct RRange *r = _range_ptr(mrb, range);

  if (_get_args(mrb, "|o", &num) == 0) {
    return r->edges->end;
  }

  array = _funcall(mrb, range, "to_a", 0);
  return _funcall(mrb, array, "last", 1, _to_int(mrb, num));
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

static _value
_range_size(_state *mrb, _value range)
{
  struct RRange *r = _range_ptr(mrb, range);
  _value beg, end;
  _float beg_f, end_f;
  _bool num_p = TRUE;
  _bool excl;

  beg = r->edges->beg;
  end = r->edges->end;
  excl = r->excl;
  if (_fixnum_p(beg)) {
    beg_f = (_float)_fixnum(beg);
  }
  else if (_float_p(beg)) {
    beg_f = _float(beg);
  }
  else {
    num_p = FALSE;
  }
  if (_fixnum_p(end)) {
    end_f = (_float)_fixnum(end);
  }
  else if (_float_p(end)) {
    end_f = _float(end);
  }
  else {
    num_p = FALSE;
  }
  if (num_p) {
    _float n = end_f - beg_f;
    _float err = (fabs(beg_f) + fabs(end_f) + fabs(end_f-beg_f)) * MRB_FLOAT_EPSILON;

    if (err>0.5) err=0.5;
    if (excl) {
      if (n<=0) return _fixnum_value(0);
      if (n<1)
        n = 0;
      else
        n = floor(n - err);
    }
    else {
      if (n<0) return _fixnum_value(0);
      n = floor(n + err);
    }
    if (isinf(n+1))
      return _float_value(mrb, INFINITY);
    return _fixnum_value((_int)n+1);
  }
  return _nil_value();
}

void
_mruby_range_ext_gem_init(_state* mrb)
{
  struct RClass * s = _class_get(mrb, "Range");

  _define_method(mrb, s, "cover?", _range_cover, MRB_ARGS_REQ(1));
  _define_method(mrb, s, "last",   _range_last,  MRB_ARGS_OPT(1));
  _define_method(mrb, s, "size",   _range_size,  MRB_ARGS_NONE());
}

void
_mruby_range_ext_gem_final(_state* mrb)
{
}
