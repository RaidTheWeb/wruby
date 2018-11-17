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

API struct RRange*
range_ptr(state *mrb, value v)
{
  struct RRange *r = (struct RRange*)ptr(v);

  if (r->edges == NULL) {
    raise(mrb, E_ARGUMENT_ERROR, "uninitialized range");
  }
  return r;
}

static void
range_check(state *mrb, value a, value b)
{
  value ans;
  enum vtype ta;
  enum vtype tb;

  ta = type(a);
  tb = type(b);
#ifdef WITHOUT_FLOAT
  if (ta == TT_FIXNUM && tb == TT_FIXNUM ) {
#else
  if ((ta == TT_FIXNUM || ta == TT_FLOAT) &&
      (tb == TT_FIXNUM || tb == TT_FLOAT)) {
#endif
    return;
  }

  ans =  funcall(mrb, a, "<=>", 1, b);
  if (nil_p(ans)) {
    /* can not be compared */
    raise(mrb, E_ARGUMENT_ERROR, "bad value for range");
  }
}

API value
range_new(state *mrb, value beg, value end, bool excl)
{
  struct RRange *r;

  range_check(mrb, beg, end);
  r = (struct RRange*)obj_alloc(mrb, TT_RANGE, mrb->range_class);
  r->edges = (range_edges *)malloc(mrb, sizeof(range_edges));
  r->edges->beg = beg;
  r->edges->end = end;
  r->excl = excl;
  return range_value(r);
}

/*
 *  call-seq:
 *     rng.first    => obj
 *     rng.begin    => obj
 *
 *  Returns the first object in <i>rng</i>.
 */
value
range_beg(state *mrb, value range)
{
  struct RRange *r = range_ptr(mrb, range);

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

value
range_end(state *mrb, value range)
{
  struct RRange *r = range_ptr(mrb, range);

  return r->edges->end;
}

/*
 *  call-seq:
 *     range.exclude_end?    => true or false
 *
 *  Returns <code>true</code> if <i>range</i> excludes its end value.
 */
value
range_excl(state *mrb, value range)
{
  struct RRange *r = range_ptr(mrb, range);

  return bool_value(r->excl);
}

static void
range_init(state *mrb, value range, value beg, value end, bool exclude_end)
{
  struct RRange *r = range_raw_ptr(range);

  range_check(mrb, beg, end);
  r->excl = exclude_end;
  if (!r->edges) {
    r->edges = (range_edges *)malloc(mrb, sizeof(range_edges));
  }
  r->edges->beg = beg;
  r->edges->end = end;
  write_barrier(mrb, (struct RBasic*)r);
}
/*
 *  call-seq:
 *     Range.new(start, end, exclusive=false)    => range
 *
 *  Constructs a range using the given <i>start</i> and <i>end</i>. If the third
 *  parameter is omitted or is <code>false</code>, the <i>range</i> will include
 *  the end object; otherwise, it will be excluded.
 */

value
range_initialize(state *mrb, value range)
{
  value beg, end;
  bool exclusive;
  int n;

  n = get_args(mrb, "oo|b", &beg, &end, &exclusive);
  if (n != 3) {
    exclusive = FALSE;
  }
  /* Ranges are immutable, so that they should be initialized only once. */
  if (range_raw_ptr(range)->edges) {
    name_error(mrb, intern_lit(mrb, "initialize"), "`initialize' called twice");
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

value
range_eq(state *mrb, value range)
{
  struct RRange *rr;
  struct RRange *ro;
  value obj, v1, v2;

  get_args(mrb, "o", &obj);

  if (obj_equal(mrb, range, obj)) return true_value();
  if (!obj_is_instance_of(mrb, obj, obj_class(mrb, range))) { /* same class? */
    return false_value();
  }

  rr = range_ptr(mrb, range);
  ro = range_ptr(mrb, obj);
  v1 = funcall(mrb, rr->edges->beg, "==", 1, ro->edges->beg);
  v2 = funcall(mrb, rr->edges->end, "==", 1, ro->edges->end);
  if (!bool(v1) || !bool(v2) || rr->excl != ro->excl) {
    return false_value();
  }
  return true_value();
}

static bool
r_le(state *mrb, value a, value b)
{
  value r = funcall(mrb, a, "<=>", 1, b); /* compare result */
  /* output :a < b => -1, a = b =>  0, a > b => +1 */

  if (fixnum_p(r)) {
    int c = fixnum(r);
    if (c == 0 || c == -1) return TRUE;
  }

  return FALSE;
}

static bool
r_gt(state *mrb, value a, value b)
{
  value r = funcall(mrb, a, "<=>", 1, b);
  /* output :a < b => -1, a = b =>  0, a > b => +1 */

  return fixnum_p(r) && fixnum(r) == 1;
}

static bool
r_ge(state *mrb, value a, value b)
{
  value r = funcall(mrb, a, "<=>", 1, b); /* compare result */
  /* output :a < b => -1, a = b =>  0, a > b => +1 */

  if (fixnum_p(r)) {
    int c = fixnum(r);
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
value
range_include(state *mrb, value range)
{
  value val;
  struct RRange *r = range_ptr(mrb, range);
  value beg, end;
  bool include_p;

  get_args(mrb, "o", &val);

  beg = r->edges->beg;
  end = r->edges->end;
  include_p = r_le(mrb, beg, val) &&           /* beg <= val */
              (r->excl ? r_gt(mrb, end, val)   /* end >  val */
                       : r_ge(mrb, end, val)); /* end >= val */

  return bool_value(include_p);
}

API int
range_beg_len(state *mrb, value range, int *begp, int *lenp, int len, bool trunc)
{
  int beg, end;
  struct RRange *r;

  if (type(range) != TT_RANGE) return 0;
  r = range_ptr(mrb, range);

  beg = int(mrb, r->edges->beg);
  end = int(mrb, r->edges->end);

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

static value
range_to_s(state *mrb, value range)
{
  value str, str2;
  struct RRange *r = range_ptr(mrb, range);

  str  = obj_as_string(mrb, r->edges->beg);
  str2 = obj_as_string(mrb, r->edges->end);
  str  = str_dup(mrb, str);
  str_cat(mrb, str, "...", r->excl ? 3 : 2);
  str_cat_str(mrb, str, str2);

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

static value
range_inspect(state *mrb, value range)
{
  value str, str2;
  struct RRange *r = range_ptr(mrb, range);

  str  = inspect(mrb, r->edges->beg);
  str2 = inspect(mrb, r->edges->end);
  str  = str_dup(mrb, str);
  str_cat(mrb, str, "...", r->excl ? 3 : 2);
  str_cat_str(mrb, str, str2);

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

static value
range_eql(state *mrb, value range)
{
  value obj;
  struct RRange *r, *o;

  get_args(mrb, "o", &obj);

  if (obj_equal(mrb, range, obj)) return true_value();
  if (!obj_is_kind_of(mrb, obj, mrb->range_class)) {
    return false_value();
  }
  if (type(obj) != TT_RANGE) return false_value();

  r = range_ptr(mrb, range);
  o = range_ptr(mrb, obj);
  if (!eql(mrb, r->edges->beg, o->edges->beg) ||
      !eql(mrb, r->edges->end, o->edges->end) ||
      (r->excl != o->excl)) {
    return false_value();
  }
  return true_value();
}

/* 15.2.14.4.15(x) */
static value
range_initialize_copy(state *mrb, value copy)
{
  value src;
  struct RRange *r;

  get_args(mrb, "o", &src);

  if (obj_equal(mrb, copy, src)) return copy;
  if (!obj_is_instance_of(mrb, src, obj_class(mrb, copy))) {
    raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }

  r = range_ptr(mrb, src);
  range_init(mrb, copy, r->edges->beg, r->edges->end, r->excl);

  return copy;
}

value
get_values_at(state *mrb, value obj, int olen, int argc, const value *argv, value (*func)(state*, value, int))
{
  int i, j, beg, len;
  value result;
  result = ary_new(mrb);

  for (i = 0; i < argc; ++i) {
    if (fixnum_p(argv[i])) {
      ary_push(mrb, result, func(mrb, obj, fixnum(argv[i])));
    }
    else if (range_beg_len(mrb, argv[i], &beg, &len, olen, FALSE) == 1) {
      int const end = olen < beg + len ? olen : beg + len;
      for (j = beg; j < end; ++j) {
        ary_push(mrb, result, func(mrb, obj, j));
      }

      for (; j < beg + len; ++j) {
        ary_push(mrb, result, nil_value());
      }
    }
    else {
      raisef(mrb, E_TYPE_ERROR, "invalid values selector: %S", argv[i]);
    }
  }

  return result;
}

void
init_range(state *mrb)
{
  struct RClass *r;

  r = define_class(mrb, "Range", mrb->object_class);                                /* 15.2.14 */
  mrb->range_class = r;
  SET_INSTANCE_TT(r, TT_RANGE);

  define_method(mrb, r, "begin",           range_beg,         ARGS_NONE()); /* 15.2.14.4.3  */
  define_method(mrb, r, "end",             range_end,         ARGS_NONE()); /* 15.2.14.4.5  */
  define_method(mrb, r, "==",              range_eq,          ARGS_REQ(1)); /* 15.2.14.4.1  */
  define_method(mrb, r, "===",             range_include,     ARGS_REQ(1)); /* 15.2.14.4.2  */
  define_method(mrb, r, "exclude_end?",    range_excl,        ARGS_NONE()); /* 15.2.14.4.6  */
  define_method(mrb, r, "first",           range_beg,         ARGS_NONE()); /* 15.2.14.4.7  */
  define_method(mrb, r, "include?",        range_include,     ARGS_REQ(1)); /* 15.2.14.4.8  */
  define_method(mrb, r, "initialize",      range_initialize,  ARGS_ANY());  /* 15.2.14.4.9  */
  define_method(mrb, r, "last",            range_end,         ARGS_NONE()); /* 15.2.14.4.10 */
  define_method(mrb, r, "member?",         range_include,     ARGS_REQ(1)); /* 15.2.14.4.11 */

  define_method(mrb, r, "to_s",            range_to_s,            ARGS_NONE()); /* 15.2.14.4.12(x) */
  define_method(mrb, r, "inspect",         range_inspect,         ARGS_NONE()); /* 15.2.14.4.13(x) */
  define_method(mrb, r, "eql?",            range_eql,             ARGS_REQ(1)); /* 15.2.14.4.14(x) */
  define_method(mrb, r, "initialize_copy", range_initialize_copy, ARGS_REQ(1)); /* 15.2.14.4.15(x) */
}
