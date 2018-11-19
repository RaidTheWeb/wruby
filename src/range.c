/*
** range.c - Range class
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/class.h>
#include <mruby/range.h>
#include <mruby/string.h>
#include <mruby/array.h>

MRB_API struct RRange*
_range_ptr(_state *mrb, _value v)
{
  struct RRange *r = (struct RRange*)_ptr(v);

  if (r->edges == NULL) {
    _raise(mrb, E_ARGUMENT_ERROR, "uninitialized range");
  }
  return r;
}

static void
range_check(_state *mrb, _value a, _value b)
{
  _value ans;
  enum _vtype ta;
  enum _vtype tb;

  ta = _type(a);
  tb = _type(b);
#ifdef MRB_WITHOUT_FLOAT
  if (ta == MRB_TT_FIXNUM && tb == MRB_TT_FIXNUM ) {
#else
  if ((ta == MRB_TT_FIXNUM || ta == MRB_TT_FLOAT) &&
      (tb == MRB_TT_FIXNUM || tb == MRB_TT_FLOAT)) {
#endif
    return;
  }

  ans =  _funcall(mrb, a, "<=>", 1, b);
  if (_nil_p(ans)) {
    /* can not be compared */
    _raise(mrb, E_ARGUMENT_ERROR, "bad value for range");
  }
}

MRB_API _value
_range_new(_state *mrb, _value beg, _value end, _bool excl)
{
  struct RRange *r;

  range_check(mrb, beg, end);
  r = (struct RRange*)_obj_alloc(mrb, MRB_TT_RANGE, mrb->range_class);
  r->edges = (_range_edges *)_malloc(mrb, sizeof(_range_edges));
  r->edges->beg = beg;
  r->edges->end = end;
  r->excl = excl;
  return _range_value(r);
}

/*
 *  call-seq:
 *     rng.first    => obj
 *     rng.begin    => obj
 *
 *  Returns the first object in <i>rng</i>.
 */
_value
_range_beg(_state *mrb, _value range)
{
  struct RRange *r = _range_ptr(mrb, range);

  return r->edges->beg;
}

/*
 *  call-seq:
 *     rng.end    => obj
 *     rng.last   => obj
 *
 *  Returns the object that defines the end of <i>rng</i>.
 *
 *     (1..10).end    #=> 10
 *     (1...10).end   #=> 10
 */

_value
_range_end(_state *mrb, _value range)
{
  struct RRange *r = _range_ptr(mrb, range);

  return r->edges->end;
}

/*
 *  call-seq:
 *     range.exclude_end?    => true or false
 *
 *  Returns <code>true</code> if <i>range</i> excludes its end value.
 */
_value
_range_excl(_state *mrb, _value range)
{
  struct RRange *r = _range_ptr(mrb, range);

  return _bool_value(r->excl);
}

static void
range_init(_state *mrb, _value range, _value beg, _value end, _bool exclude_end)
{
  struct RRange *r = _range_raw_ptr(range);

  range_check(mrb, beg, end);
  r->excl = exclude_end;
  if (!r->edges) {
    r->edges = (_range_edges *)_malloc(mrb, sizeof(_range_edges));
  }
  r->edges->beg = beg;
  r->edges->end = end;
  _write_barrier(mrb, (struct RBasic*)r);
}
/*
 *  call-seq:
 *     Range.new(start, end, exclusive=false)    => range
 *
 *  Constructs a range using the given <i>start</i> and <i>end</i>. If the third
 *  parameter is omitted or is <code>false</code>, the <i>range</i> will include
 *  the end object; otherwise, it will be excluded.
 */

_value
_range_initialize(_state *mrb, _value range)
{
  _value beg, end;
  _bool exclusive;
  _int n;

  n = _get_args(mrb, "oo|b", &beg, &end, &exclusive);
  if (n != 3) {
    exclusive = FALSE;
  }
  /* Ranges are immutable, so that they should be initialized only once. */
  if (_range_raw_ptr(range)->edges) {
    _name_error(mrb, _intern_lit(mrb, "initialize"), "`initialize' called twice");
  }
  range_init(mrb, range, beg, end, exclusive);
  return range;
}
/*
 *  call-seq:
 *     range == obj    => true or false
 *
 *  Returns <code>true</code> only if
 *  1) <i>obj</i> is a Range,
 *  2) <i>obj</i> has equivalent beginning and end items (by comparing them with <code>==</code>),
 *  3) <i>obj</i> has the same #exclude_end? setting as <i>rng</t>.
 *
 *    (0..2) == (0..2)            #=> true
 *    (0..2) == Range.new(0,2)    #=> true
 *    (0..2) == (0...2)           #=> false
 *
 */

_value
_range_eq(_state *mrb, _value range)
{
  struct RRange *rr;
  struct RRange *ro;
  _value obj, v1, v2;

  _get_args(mrb, "o", &obj);

  if (_obj_equal(mrb, range, obj)) return _true_value();
  if (!_obj_is_instance_of(mrb, obj, _obj_class(mrb, range))) { /* same class? */
    return _false_value();
  }

  rr = _range_ptr(mrb, range);
  ro = _range_ptr(mrb, obj);
  v1 = _funcall(mrb, rr->edges->beg, "==", 1, ro->edges->beg);
  v2 = _funcall(mrb, rr->edges->end, "==", 1, ro->edges->end);
  if (!_bool(v1) || !_bool(v2) || rr->excl != ro->excl) {
    return _false_value();
  }
  return _true_value();
}

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
r_gt(_state *mrb, _value a, _value b)
{
  _value r = _funcall(mrb, a, "<=>", 1, b);
  /* output :a < b => -1, a = b =>  0, a > b => +1 */

  return _fixnum_p(r) && _fixnum(r) == 1;
}

static _bool
r_ge(_state *mrb, _value a, _value b)
{
  _value r = _funcall(mrb, a, "<=>", 1, b); /* compare result */
  /* output :a < b => -1, a = b =>  0, a > b => +1 */

  if (_fixnum_p(r)) {
    _int c = _fixnum(r);
    if (c == 0 || c == 1) return TRUE;
  }

  return FALSE;
}

/*
 *  call-seq:
 *     range === obj       =>  true or false
 *     range.member?(val)  =>  true or false
 *     range.include?(val) =>  true or false
 *
 */
_value
_range_include(_state *mrb, _value range)
{
  _value val;
  struct RRange *r = _range_ptr(mrb, range);
  _value beg, end;
  _bool include_p;

  _get_args(mrb, "o", &val);

  beg = r->edges->beg;
  end = r->edges->end;
  include_p = r_le(mrb, beg, val) &&           /* beg <= val */
              (r->excl ? r_gt(mrb, end, val)   /* end >  val */
                       : r_ge(mrb, end, val)); /* end >= val */

  return _bool_value(include_p);
}

MRB_API _int
_range_beg_len(_state *mrb, _value range, _int *begp, _int *lenp, _int len, _bool trunc)
{
  _int beg, end;
  struct RRange *r;

  if (_type(range) != MRB_TT_RANGE) return 0;
  r = _range_ptr(mrb, range);

  beg = _int(mrb, r->edges->beg);
  end = _int(mrb, r->edges->end);

  if (beg < 0) {
    beg += len;
    if (beg < 0) return 2;
  }

  if (trunc) {
    if (beg > len) return 2;
    if (end > len) end = len;
  }

  if (end < 0) end += len;
  if (!r->excl && (!trunc || end < len))
    end++;                      /* include end point */
  len = end - beg;
  if (len < 0) len = 0;

  *begp = beg;
  *lenp = len;
  return 1;
}

/* 15.2.14.4.12(x) */
/*
 * call-seq:
 *   rng.to_s   -> string
 *
 * Convert this range object to a printable form.
 */

static _value
range_to_s(_state *mrb, _value range)
{
  _value str, str2;
  struct RRange *r = _range_ptr(mrb, range);

  str  = _obj_as_string(mrb, r->edges->beg);
  str2 = _obj_as_string(mrb, r->edges->end);
  str  = _str_dup(mrb, str);
  _str_cat(mrb, str, "...", r->excl ? 3 : 2);
  _str_cat_str(mrb, str, str2);

  return str;
}

/* 15.2.14.4.13(x) */
/*
 * call-seq:
 *   rng.inspect  -> string
 *
 * Convert this range object to a printable form (using
 * <code>inspect</code> to convert the start and end
 * objects).
 */

static _value
range_inspect(_state *mrb, _value range)
{
  _value str, str2;
  struct RRange *r = _range_ptr(mrb, range);

  str  = _inspect(mrb, r->edges->beg);
  str2 = _inspect(mrb, r->edges->end);
  str  = _str_dup(mrb, str);
  _str_cat(mrb, str, "...", r->excl ? 3 : 2);
  _str_cat_str(mrb, str, str2);

  return str;
}

/* 15.2.14.4.14(x) */
/*
 *  call-seq:
 *     rng.eql?(obj)    -> true or false
 *
 *  Returns <code>true</code> only if <i>obj</i> is a Range, has equivalent
 *  beginning and end items (by comparing them with #eql?), and has the same
 *  #exclude_end? setting as <i>rng</i>.
 *
 *    (0..2).eql?(0..2)            #=> true
 *    (0..2).eql?(Range.new(0,2))  #=> true
 *    (0..2).eql?(0...2)           #=> false
 *
 */

static _value
range_eql(_state *mrb, _value range)
{
  _value obj;
  struct RRange *r, *o;

  _get_args(mrb, "o", &obj);

  if (_obj_equal(mrb, range, obj)) return _true_value();
  if (!_obj_is_kind_of(mrb, obj, mrb->range_class)) {
    return _false_value();
  }
  if (_type(obj) != MRB_TT_RANGE) return _false_value();

  r = _range_ptr(mrb, range);
  o = _range_ptr(mrb, obj);
  if (!_eql(mrb, r->edges->beg, o->edges->beg) ||
      !_eql(mrb, r->edges->end, o->edges->end) ||
      (r->excl != o->excl)) {
    return _false_value();
  }
  return _true_value();
}

/* 15.2.14.4.15(x) */
static _value
range_initialize_copy(_state *mrb, _value copy)
{
  _value src;
  struct RRange *r;

  _get_args(mrb, "o", &src);

  if (_obj_equal(mrb, copy, src)) return copy;
  if (!_obj_is_instance_of(mrb, src, _obj_class(mrb, copy))) {
    _raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }

  r = _range_ptr(mrb, src);
  range_init(mrb, copy, r->edges->beg, r->edges->end, r->excl);

  return copy;
}

_value
_get_values_at(_state *mrb, _value obj, _int olen, _int argc, const _value *argv, _value (*func)(_state*, _value, _int))
{
  _int i, j, beg, len;
  _value result;
  result = _ary_new(mrb);

  for (i = 0; i < argc; ++i) {
    if (_fixnum_p(argv[i])) {
      _ary_push(mrb, result, func(mrb, obj, _fixnum(argv[i])));
    }
    else if (_range_beg_len(mrb, argv[i], &beg, &len, olen, FALSE) == 1) {
      _int const end = olen < beg + len ? olen : beg + len;
      for (j = beg; j < end; ++j) {
        _ary_push(mrb, result, func(mrb, obj, j));
      }

      for (; j < beg + len; ++j) {
        _ary_push(mrb, result, _nil_value());
      }
    }
    else {
      _raisef(mrb, E_TYPE_ERROR, "invalid values selector: %S", argv[i]);
    }
  }

  return result;
}

void
_init_range(_state *mrb)
{
  struct RClass *r;

  r = _define_class(mrb, "Range", mrb->object_class);                                /* 15.2.14 */
  mrb->range_class = r;
  MRB_SET_INSTANCE_TT(r, MRB_TT_RANGE);

  _define_method(mrb, r, "begin",           _range_beg,         MRB_ARGS_NONE()); /* 15.2.14.4.3  */
  _define_method(mrb, r, "end",             _range_end,         MRB_ARGS_NONE()); /* 15.2.14.4.5  */
  _define_method(mrb, r, "==",              _range_eq,          MRB_ARGS_REQ(1)); /* 15.2.14.4.1  */
  _define_method(mrb, r, "===",             _range_include,     MRB_ARGS_REQ(1)); /* 15.2.14.4.2  */
  _define_method(mrb, r, "exclude_end?",    _range_excl,        MRB_ARGS_NONE()); /* 15.2.14.4.6  */
  _define_method(mrb, r, "first",           _range_beg,         MRB_ARGS_NONE()); /* 15.2.14.4.7  */
  _define_method(mrb, r, "include?",        _range_include,     MRB_ARGS_REQ(1)); /* 15.2.14.4.8  */
  _define_method(mrb, r, "initialize",      _range_initialize,  MRB_ARGS_ANY());  /* 15.2.14.4.9  */
  _define_method(mrb, r, "last",            _range_end,         MRB_ARGS_NONE()); /* 15.2.14.4.10 */
  _define_method(mrb, r, "member?",         _range_include,     MRB_ARGS_REQ(1)); /* 15.2.14.4.11 */

  _define_method(mrb, r, "to_s",            range_to_s,            MRB_ARGS_NONE()); /* 15.2.14.4.12(x) */
  _define_method(mrb, r, "inspect",         range_inspect,         MRB_ARGS_NONE()); /* 15.2.14.4.13(x) */
  _define_method(mrb, r, "eql?",            range_eql,             MRB_ARGS_REQ(1)); /* 15.2.14.4.14(x) */
  _define_method(mrb, r, "initialize_copy", range_initialize_copy, MRB_ARGS_REQ(1)); /* 15.2.14.4.15(x) */
}
