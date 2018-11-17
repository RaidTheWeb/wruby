#include <limits.h>
#include <mruby.h>

static inline int
to_int(value x)
{
  double f;

  if (fixnum_p(x)) return fixnum(x);
  f = float(x);
  return (int)f;
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
int_chr(state *mrb, value x)
{
  int chr;
  char c;

  chr = to_int(x);
  if (chr >= (1 << CHAR_BIT)) {
    raisef(mrb, E_RANGE_ERROR, "%S out of char range", x);
  }
  c = (char)chr;

  return str_new(mrb, &c, 1);
}

/*
 *  call-seq:
 *     int.allbits?(mask)  ->  true or false
 *
 *  Returns +true+ if all bits of <code>+int+ & +mask+</code> are 1.
 */
static value
int_allbits(state *mrb, value self)
{
  int n, m;

  n = to_int(self);
  get_args(mrb, "i", &m);
  return bool_value((n & m) == m);
}

/*
 *  call-seq:
 *     int.anybits?(mask)  ->  true or false
 *
 *  Returns +true+ if any bits of <code>+int+ & +mask+</code> are 1.
 */
static value
int_anybits(state *mrb, value self)
{
  int n, m;

  n = to_int(self);
  get_args(mrb, "i", &m);
  return bool_value((n & m) != 0);
}

/*
 *  call-seq:
 *     int.nobits?(mask)  ->  true or false
 *
 *  Returns +true+ if no bits of <code>+int+ & +mask+</code> are 1.
 */
static value
int_nobits(state *mrb, value self)
{
  int n, m;

  n = to_int(self);
  get_args(mrb, "i", &m);
  return bool_value((n & m) == 0);
}

void
mruby_numeric_ext_gem_init(state* mrb)
{
  struct RClass *i = module_get(mrb, "Integral");

  define_method(mrb, i, "chr", int_chr, ARGS_NONE());
  define_method(mrb, i, "allbits?", int_allbits, ARGS_REQ(1));
  define_method(mrb, i, "anybits?", int_anybits, ARGS_REQ(1));
  define_method(mrb, i, "nobits?", int_nobits, ARGS_REQ(1));
}

void
mruby_numeric_ext_gem_final(state* mrb)
{
}
