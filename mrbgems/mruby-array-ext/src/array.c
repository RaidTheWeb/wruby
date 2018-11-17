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

static value
ary_assoc(state *mrb, value ary)
{
  int i;
  value v, k;

  get_args(mrb, "o", &k);

  for (i = 0; i < RARRAY_LEN(ary); ++i) {
    v = check_array_type(mrb, RARRAY_PTR(ary)[i]);
    if (!nil_p(v) && RARRAY_LEN(v) > 0 &&
        equal(mrb, RARRAY_PTR(v)[0], k))
      return v;
  }
  return nil_value();
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

static value
ary_rassoc(state *mrb, value ary)
{
  int i;
  value v, value;

  get_args(mrb, "o", &value);

  for (i = 0; i < RARRAY_LEN(ary); ++i) {
    v = RARRAY_PTR(ary)[i];
    if (type(v) == TT_ARRAY &&
        RARRAY_LEN(v) > 1 &&
        equal(mrb, RARRAY_PTR(v)[1], value))
      return v;
  }
  return nil_value();
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

static value
ary_at(state *mrb, value ary)
{
  int pos;
  get_args(mrb, "i", &pos);

  return ary_entry(ary, pos);
}

static value
ary_values_at(state *mrb, value self)
{
  int argc;
  value *argv;

  get_args(mrb, "*", &argv, &argc);

  return get_values_at(mrb, self, RARRAY_LEN(self), argc, argv, ary_ref);
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

static value
ary_slice_bang(state *mrb, value self)
{
  struct RArray *a = ary_ptr(self);
  int i, j, k, len, alen;
  value val;
  value *ptr;
  value ary;

  ary_modify(mrb, a);

  if (get_argc(mrb) == 1) {
    value index;

    get_args(mrb, "o|i", &index, &len);
    switch (type(index)) {
    case TT_RANGE:
      if (range_beg_len(mrb, index, &i, &len, ARY_LEN(a), TRUE) == 1) {
        goto delete_pos_len;
      }
      else {
        return nil_value();
      }
    case TT_FIXNUM:
      val = funcall(mrb, self, "delete_at", 1, index);
      return val;
    default:
      val = funcall(mrb, self, "delete_at", 1, index);
      return val;
    }
  }

  get_args(mrb, "ii", &i, &len);
 delete_pos_len:
  alen = ARY_LEN(a);
  if (i < 0) i += alen;
  if (i < 0 || alen < i) return nil_value();
  if (len < 0) return nil_value();
  if (alen == i) return ary_new(mrb);
  if (len > alen - i) len = alen - i;

  ary = ary_new_capa(mrb, len);
  ptr = ARY_PTR(a);
  for (j = i, k = 0; k < len; ++j, ++k) {
    ary_push(mrb, ary, ptr[j]);
  }

  ptr += i;
  for (j = i; j < alen - len; ++j) {
    *ptr = *(ptr+len);
    ++ptr;
  }

  ary_resize(mrb, self, alen - len);
  return ary;
}

void
mruby_array_ext_gem_init(state* mrb)
{
  struct RClass * a = mrb->array_class;

  define_method(mrb, a, "assoc",  ary_assoc,  ARGS_REQ(1));
  define_method(mrb, a, "at",     ary_at,     ARGS_REQ(1));
  define_method(mrb, a, "rassoc", ary_rassoc, ARGS_REQ(1));
  define_method(mrb, a, "values_at", ary_values_at, ARGS_ANY());
  define_method(mrb, a, "slice!", ary_slice_bang,   ARGS_ANY());
}

void
mruby_array_ext_gem_final(state* mrb)
{
}
