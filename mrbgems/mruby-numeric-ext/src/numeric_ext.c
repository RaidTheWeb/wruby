#include <limits.h>
#include <mruby.h>

static inline _int
to_int(value x)
{
  double f;

  if (_fixnum_p(x)) return _fixnum(x);
  f = _float(x);
  return (_int)f;
}

/*
 *  Document-method: Integer#chr
 *  call-seq:
 *     int.chr  ->  string
 *
 *  Returns a string containing the character represented by the +int+'s value
 *  according to +encoding+.
 *
 *     65.chr    #=> "A"
 *     230.chr   #=> "\xE6"
 */
static value
_int_chr(state *mrb, value x)
{
  _int chr;
  char c;

  chr = to_int(x);
  if (chr >= (1 << CHAR_BIT)) {
    _raisef(mrb, E_RANGE_ERROR, "%S out of char range", x);
  }
  c = (char)chr;

  return _str_new(mrb, &c, 1);
}

/*
 *  call-seq:
 *     int.allbits?(mask)  ->  true or false
 *
 *  Returns +true+ if all bits of <code>+int+ & +mask+</code> are 1.
 */
static value
_int_allbits(state *mrb, value self)
{
  _int n, m;

  n = to_int(self);
  _get_args(mrb, "i", &m);
  return _bool_value((n & m) == m);
}

/*
 *  call-seq:
 *     int.anybits?(mask)  ->  true or false
 *
 *  Returns +true+ if any bits of <code>+int+ & +mask+</code> are 1.
 */
static value
_int_anybits(state *mrb, value self)
{
  _int n, m;

  n = to_int(self);
  _get_args(mrb, "i", &m);
  return _bool_value((n & m) != 0);
}

/*
 *  call-seq:
 *     int.nobits?(mask)  ->  true or false
 *
 *  Returns +true+ if no bits of <code>+int+ & +mask+</code> are 1.
 */
static value
_int_nobits(state *mrb, value self)
{
  _int n, m;

  n = to_int(self);
  _get_args(mrb, "i", &m);
  return _bool_value((n & m) == 0);
}

void
_mruby_numeric_ext_gem_init(state* mrb)
{
  struct RClass *i = _module_get(mrb, "Integral");

  _define_method(mrb, i, "chr", _int_chr, MRB_ARGS_NONE());
  _define_method(mrb, i, "allbits?", _int_allbits, MRB_ARGS_REQ(1));
  _define_method(mrb, i, "anybits?", _int_anybits, MRB_ARGS_REQ(1));
  _define_method(mrb, i, "nobits?", _int_nobits, MRB_ARGS_REQ(1));
}

void
_mruby_numeric_ext_gem_final(state* mrb)
{
}
