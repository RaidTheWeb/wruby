#include <mruby.h>
#include <mruby/value.h>
#include <mruby/array.h>
#include <mruby/range.h>
#include <mruby/hash.h>

/*
 *  call-seq:
 *     ary.assoc(obj)   -> new_ary  or  nil
 *
 *  Searches through an array whose elements are also arrays
 *  comparing _obj_ with the first element of each contained array
 *  using obj.==.
 *  Returns the first contained array that matches (that
 *  is, the first associated array),
 *  or +nil+ if no match is found.
 *  See also <code>Array#rassoc</code>.
 *
 *     s1 = [ "colors", "red", "blue", "green" ]
 *     s2 = [ "letters", "a", "b", "c" ]
 *     s3 = "foo"
 *     a  = [ s1, s2, s3 ]
 *     a.assoc("letters")  #=> [ "letters", "a", "b", "c" ]
 *     a.assoc("foo")      #=> nil
 */

static _value
_ary_assoc(_state *mrb, _value ary)
{
  _int i;
  _value v, k;

  _get_args(mrb, "o", &k);

  for (i = 0; i < RARRAY_LEN(ary); ++i) {
    v = _check_array_type(mrb, RARRAY_PTR(ary)[i]);
    if (!_nil_p(v) && RARRAY_LEN(v) > 0 &&
        _equal(mrb, RARRAY_PTR(v)[0], k))
      return v;
  }
  return _nil_value();
}

/*
 *  call-seq:
 *     ary.rassoc(obj) -> new_ary or nil
 *
 *  Searches through the array whose elements are also arrays. Compares
 *  _obj_ with the second element of each contained array using
 *  <code>==</code>. Returns the first contained array that matches. See
 *  also <code>Array#assoc</code>.
 *
 *     a = [ [ 1, "one"], [2, "two"], [3, "three"], ["ii", "two"] ]
 *     a.rassoc("two")    #=> [2, "two"]
 *     a.rassoc("four")   #=> nil
 */

static _value
_ary_rassoc(_state *mrb, _value ary)
{
  _int i;
  _value v, value;

  _get_args(mrb, "o", &value);

  for (i = 0; i < RARRAY_LEN(ary); ++i) {
    v = RARRAY_PTR(ary)[i];
    if (_type(v) == MRB_TT_ARRAY &&
        RARRAY_LEN(v) > 1 &&
        _equal(mrb, RARRAY_PTR(v)[1], value))
      return v;
  }
  return _nil_value();
}

/*
 *  call-seq:
 *     ary.at(index)   ->   obj  or nil
 *
 *  Returns the element at _index_. A
 *  negative index counts from the end of +self+.  Returns +nil+
 *  if the index is out of range. See also <code>Array#[]</code>.
 *
 *     a = [ "a", "b", "c", "d", "e" ]
 *     a.at(0)     #=> "a"
 *     a.at(-1)    #=> "e"
 */

static _value
_ary_at(_state *mrb, _value ary)
{
  _int pos;
  _get_args(mrb, "i", &pos);

  return _ary_entry(ary, pos);
}

static _value
_ary_values_at(_state *mrb, _value self)
{
  _int argc;
  _value *argv;

  _get_args(mrb, "*", &argv, &argc);

  return _get_values_at(mrb, self, RARRAY_LEN(self), argc, argv, _ary_ref);
}


/*
 *  call-seq:
 *     ary.slice!(index)         -> obj or nil
 *     ary.slice!(start, length) -> new_ary or nil
 *     ary.slice!(range)         -> new_ary or nil
 *
 *  Deletes the element(s) given by an +index+ (optionally up to +length+
 *  elements) or by a +range+.
 *
 *  Returns the deleted object (or objects), or +nil+ if the +index+ is out of
 *  range.
 *
 *     a = [ "a", "b", "c" ]
 *     a.slice!(1)     #=> "b"
 *     a               #=> ["a", "c"]
 *     a.slice!(-1)    #=> "c"
 *     a               #=> ["a"]
 *     a.slice!(100)   #=> nil
 *     a               #=> ["a"]
 */

static _value
_ary_slice_bang(_state *mrb, _value self)
{
  struct RArray *a = _ary_ptr(self);
  _int i, j, k, len, alen;
  _value val;
  _value *ptr;
  _value ary;

  _ary_modify(mrb, a);

  if (_get_argc(mrb) == 1) {
    _value index;

    _get_args(mrb, "o|i", &index, &len);
    switch (_type(index)) {
    case MRB_TT_RANGE:
      if (_range_beg_len(mrb, index, &i, &len, ARY_LEN(a), TRUE) == 1) {
        goto delete_pos_len;
      }
      else {
        return _nil_value();
      }
    case MRB_TT_FIXNUM:
      val = _funcall(mrb, self, "delete_at", 1, index);
      return val;
    default:
      val = _funcall(mrb, self, "delete_at", 1, index);
      return val;
    }
  }

  _get_args(mrb, "ii", &i, &len);
 delete_pos_len:
  alen = ARY_LEN(a);
  if (i < 0) i += alen;
  if (i < 0 || alen < i) return _nil_value();
  if (len < 0) return _nil_value();
  if (alen == i) return _ary_new(mrb);
  if (len > alen - i) len = alen - i;

  ary = _ary_new_capa(mrb, len);
  ptr = ARY_PTR(a);
  for (j = i, k = 0; k < len; ++j, ++k) {
    _ary_push(mrb, ary, ptr[j]);
  }

  ptr += i;
  for (j = i; j < alen - len; ++j) {
    *ptr = *(ptr+len);
    ++ptr;
  }

  _ary_resize(mrb, self, alen - len);
  return ary;
}

void
_mruby_array_ext_gem_init(_state* mrb)
{
  struct RClass * a = mrb->array_class;

  _define_method(mrb, a, "assoc",  _ary_assoc,  MRB_ARGS_REQ(1));
  _define_method(mrb, a, "at",     _ary_at,     MRB_ARGS_REQ(1));
  _define_method(mrb, a, "rassoc", _ary_rassoc, MRB_ARGS_REQ(1));
  _define_method(mrb, a, "values_at", _ary_values_at, MRB_ARGS_ANY());
  _define_method(mrb, a, "slice!", _ary_slice_bang,   MRB_ARGS_ANY());
}

void
_mruby_array_ext_gem_final(_state* mrb)
{
}
