/*
** sprintf.c - Kernel.#sprintf
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <mruby/string.h>
#include <mruby/hash.h>
#include <mruby/numeric.h>
#ifndef MRB_WITHOUT_FLOAT
#include <math.h>
#endif
#include <ctype.h>

#define BIT_DIGITS(N)   (((N)*146)/485 + 1)  /* log2(10) =~ 146/485 */
#define BITSPERDIG MRB_INT_BIT
#define EXTENDSIGN(n, l) (((~0U << (n)) >> (((n)*(l)) % BITSPERDIG)) & ~(~0U << (n)))

value _str_format(state *, _int, const value *, value);
#ifndef MRB_WITHOUT_FLOAT
static void fmt_setup(char*,size_t,int,int,_int,_int);
#endif

static char*
remove_sign_bits(char *str, int base)
{
  char *t;

  t = str;
  if (base == 16) {
    while (*t == 'f') {
      t++;
    }
  }
  else if (base == 8) {
    *t |= EXTENDSIGN(3, strlen(t));
    while (*t == '7') {
      t++;
    }
  }
  else if (base == 2) {
    while (*t == '1') {
      t++;
    }
  }

  return t;
}

static char
sign_bits(int base, const char *p)
{
  char c;

  switch (base) {
  case 16:
    if (*p == 'X') c = 'F';
    else c = 'f';
    break;
  case 8:
    c = '7'; break;
  case 2:
    c = '1'; break;
  default:
    c = '.'; break;
  }
  return c;
}

static value
_fix2binstr(state *mrb, value x, int base)
{
  char buf[66], *b = buf + sizeof buf;
  _int num = _fixnum(x);
  uint64_t val = (uint64_t)num;
  char d;

  if (base != 2) {
    _raisef(mrb, E_ARGUMENT_ERROR, "invalid radix %S", _fixnum_value(base));
  }
  if (val == 0) {
    return _str_new_lit(mrb, "0");
  }
  *--b = '\0';
  do {
    *--b = _digitmap[(int)(val % base)];
  } while (val /= base);

  if (num < 0) {
    b = remove_sign_bits(b, base);
    switch (base) {
    case 16: d = 'f'; break;
    case 8:  d = '7'; break;
    case 2:  d = '1'; break;
    default: d = 0;   break;
    }

    if (d && *b != d) {
      *--b = d;
    }
  }

  return _str_new_cstr(mrb, b);
}

#define FNONE  0
#define FSHARP 1
#define FMINUS 2
#define FPLUS  4
#define FZERO  8
#define FSPACE 16
#define FWIDTH 32
#define FPREC  64
#define FPREC0 128

#define CHECK(l) do {\
  while ((l) >= bsiz - blen) {\
    if (bsiz > MRB_INT_MAX/2) _raise(mrb, E_ARGUMENT_ERROR, "too big specifier"); \
    bsiz*=2;\
  }\
  _str_resize(mrb, result, bsiz);\
  buf = RSTRING_PTR(result);\
} while (0)

#define PUSH(s, l) do { \
  CHECK(l);\
  memcpy(&buf[blen], s, l);\
  blen += (_int)(l);\
} while (0)

#define FILL(c, l) do { \
  CHECK(l);\
  memset(&buf[blen], c, l);\
  blen += (l);\
} while (0)

static void
check_next_arg(state *mrb, int posarg, int nextarg)
{
  switch (posarg) {
  case -1:
    _raisef(mrb, E_ARGUMENT_ERROR, "unnumbered(%S) mixed with numbered", _fixnum_value(nextarg));
    break;
  case -2:
    _raisef(mrb, E_ARGUMENT_ERROR, "unnumbered(%S) mixed with named", _fixnum_value(nextarg));
    break;
  default:
    break;
  }
}

static void
check_pos_arg(state *mrb, _int posarg, _int n)
{
  if (posarg > 0) {
    _raisef(mrb, E_ARGUMENT_ERROR, "numbered(%S) after unnumbered(%S)",
               _fixnum_value(n), _fixnum_value(posarg));
  }
  if (posarg == -2) {
    _raisef(mrb, E_ARGUMENT_ERROR, "numbered(%S) after named", _fixnum_value(n));
  }
  if (n < 1) {
    _raisef(mrb, E_ARGUMENT_ERROR, "invalid index - %S$", _fixnum_value(n));
  }
}

static void
check_name_arg(state *mrb, int posarg, const char *name, _int len)
{
  if (posarg > 0) {
    _raisef(mrb, E_ARGUMENT_ERROR, "named%S after unnumbered(%S)",
               _str_new(mrb, (name), (len)), _fixnum_value(posarg));
  }
  if (posarg == -1) {
    _raisef(mrb, E_ARGUMENT_ERROR, "named%S after numbered", _str_new(mrb, (name), (len)));
  }
}

#define GETNEXTARG() (\
  check_next_arg(mrb, posarg, nextarg),\
  (posarg = nextarg++, GETNTHARG(posarg)))

#define GETARG() (!_undef_p(nextvalue) ? nextvalue : GETNEXTARG())

#define GETPOSARG(n) (\
  check_pos_arg(mrb, posarg, n),\
  (posarg = -1, GETNTHARG(n)))

#define GETNTHARG(nth) \
  ((nth >= argc) ? (_raise(mrb, E_ARGUMENT_ERROR, "too few arguments"), _undef_value()) : argv[nth])

#define GETNAMEARG(id, name, len) (\
  check_name_arg(mrb, posarg, name, len),\
  (posarg = -2, _hash_fetch(mrb, get_hash(mrb, &hash, argc, argv), id, _undef_value())))

#define GETNUM(n, val) \
  for (; p < end && ISDIGIT(*p); p++) {\
    if (n > (MRB_INT_MAX - (*p - '0'))/10) {\
      _raise(mrb, E_ARGUMENT_ERROR, #val " too big"); \
    } \
    n = 10 * n + (*p - '0'); \
  } \
  if (p >= end) { \
    _raise(mrb, E_ARGUMENT_ERROR, "malformed format string - %*[0-9]"); \
  }

#define GETASTER(num) do { \
  value tmp_v; \
  t = p++; \
  n = 0; \
  GETNUM(n, val); \
  if (*p == '$') { \
    tmp_v = GETPOSARG(n); \
  } \
  else { \
    tmp_v = GETNEXTARG(); \
    p = t; \
  } \
  num = _int(mrb, tmp_v); \
} while (0)

static value
get_hash(state *mrb, value *hash, _int argc, const value *argv)
{
  value tmp;

  if (!_undef_p(*hash)) return *hash;
  if (argc != 2) {
    _raise(mrb, E_ARGUMENT_ERROR, "one hash required");
  }
  tmp = _check_convert_type(mrb, argv[1], MRB_TT_HASH, "Hash", "to_hash");
  if (_nil_p(tmp)) {
    _raise(mrb, E_ARGUMENT_ERROR, "one hash required");
  }
  return (*hash = tmp);
}

/*
 *  call-seq:
 *     format(format_string [, arguments...] )   -> string
 *     sprintf(format_string [, arguments...] )  -> string
 *
 *  Returns the string resulting from applying <i>format_string</i> to
 *  any additional arguments.  Within the format string, any characters
 *  other than format sequences are copied to the result.
 *
 *  The syntax of a format sequence is follows.
 *
 *    %[flags][width][.precision]type
 *
 *  A format
 *  sequence consists of a percent sign, followed by optional flags,
 *  width, and precision indicators, then terminated with a field type
 *  character.  The field type controls how the corresponding
 *  <code>sprintf</code> argument is to be interpreted, while the flags
 *  modify that interpretation.
 *
 *  The field type characters are:
 *
 *      Field |  Integer Format
 *      ------+--------------------------------------------------------------
 *        b   | Convert argument as a binary number.
 *            | Negative numbers will be displayed as a two's complement
 *            | prefixed with '..1'.
 *        B   | Equivalent to 'b', but uses an uppercase 0B for prefix
 *            | in the alternative format by #.
 *        d   | Convert argument as a decimal number.
 *        i   | Identical to 'd'.
 *        o   | Convert argument as an octal number.
 *            | Negative numbers will be displayed as a two's complement
 *            | prefixed with '..7'.
 *        u   | Identical to 'd'.
 *        x   | Convert argument as a hexadecimal number.
 *            | Negative numbers will be displayed as a two's complement
 *            | prefixed with '..f' (representing an infinite string of
 *            | leading 'ff's).
 *        X   | Equivalent to 'x', but uses uppercase letters.
 *
 *      Field |  Float Format
 *      ------+--------------------------------------------------------------
 *        e   | Convert floating point argument into exponential notation
 *            | with one digit before the decimal point as [-]d.dddddde[+-]dd.
 *            | The precision specifies the number of digits after the decimal
 *            | point (defaulting to six).
 *        E   | Equivalent to 'e', but uses an uppercase E to indicate
 *            | the exponent.
 *        f   | Convert floating point argument as [-]ddd.dddddd,
 *            | where the precision specifies the number of digits after
 *            | the decimal point.
 *        g   | Convert a floating point number using exponential form
 *            | if the exponent is less than -4 or greater than or
 *            | equal to the precision, or in dd.dddd form otherwise.
 *            | The precision specifies the number of significant digits.
 *        G   | Equivalent to 'g', but use an uppercase 'E' in exponent form.
 *        a   | Convert floating point argument as [-]0xh.hhhhp[+-]dd,
 *            | which is consisted from optional sign, "0x", fraction part
 *            | as hexadecimal, "p", and exponential part as decimal.
 *        A   | Equivalent to 'a', but use uppercase 'X' and 'P'.
 *
 *      Field |  Other Format
 *      ------+--------------------------------------------------------------
 *        c   | Argument is the numeric code for a single character or
 *            | a single character string itself.
 *        p   | The valuing of argument.inspect.
 *        s   | Argument is a string to be substituted.  If the format
 *            | sequence contains a precision, at most that many characters
 *            | will be copied.
 *        %   | A percent sign itself will be displayed.  No argument taken.
 *
 *  The flags modifies the behavior of the formats.
 *  The flag characters are:
 *
 *    Flag     | Applies to    | Meaning
 *    ---------+---------------+-----------------------------------------
 *    space    | bBdiouxX      | Leave a space at the start of
 *             | aAeEfgG       | non-negative numbers.
 *             | (numeric fmt) | For 'o', 'x', 'X', 'b' and 'B', use
 *             |               | a minus sign with absolute value for
 *             |               | negative values.
 *    ---------+---------------+-----------------------------------------
 *    (digit)$ | all           | Specifies the absolute argument number
 *             |               | for this field.  Absolute and relative
 *             |               | argument numbers cannot be mixed in a
 *             |               | sprintf string.
 *    ---------+---------------+-----------------------------------------
 *     #       | bBoxX         | Use an alternative format.
 *             | aAeEfgG       | For the conversions 'o', increase the precision
 *             |               | until the first digit will be '0' if
 *             |               | it is not formatted as complements.
 *             |               | For the conversions 'x', 'X', 'b' and 'B'
 *             |               | on non-zero, prefix the result with "0x",
 *             |               | "0X", "0b" and "0B", respectively.
 *             |               | For 'a', 'A', 'e', 'E', 'f', 'g', and 'G',
 *             |               | force a decimal point to be added,
 *             |               | even if no digits follow.
 *             |               | For 'g' and 'G', do not remove trailing zeros.
 *    ---------+---------------+-----------------------------------------
 *    +        | bBdiouxX      | Add a leading plus sign to non-negative
 *             | aAeEfgG       | numbers.
 *             | (numeric fmt) | For 'o', 'x', 'X', 'b' and 'B', use
 *             |               | a minus sign with absolute value for
 *             |               | negative values.
 *    ---------+---------------+-----------------------------------------
 *    -        | all           | Left-justify the result of this conversion.
 *    ---------+---------------+-----------------------------------------
 *    0 (zero) | bBdiouxX      | Pad with zeros, not spaces.
 *             | aAeEfgG       | For 'o', 'x', 'X', 'b' and 'B', radix-1
 *             | (numeric fmt) | is used for negative numbers formatted as
 *             |               | complements.
 *    ---------+---------------+-----------------------------------------
 *    *        | all           | Use the next argument as the field width.
 *             |               | If negative, left-justify the result. If the
 *             |               | asterisk is followed by a number and a dollar
 *             |               | sign, use the indicated argument as the width.
 *
 *  Examples of flags:
 *
 *   # '+' and space flag specifies the sign of non-negative numbers.
 *   sprintf("%d", 123)  #=> "123"
 *   sprintf("%+d", 123) #=> "+123"
 *   sprintf("% d", 123) #=> " 123"
 *
 *   # '#' flag for 'o' increases number of digits to show '0'.
 *   # '+' and space flag changes format of negative numbers.
 *   sprintf("%o", 123)   #=> "173"
 *   sprintf("%#o", 123)  #=> "0173"
 *   sprintf("%+o", -123) #=> "-173"
 *   sprintf("%o", -123)  #=> "..7605"
 *   sprintf("%#o", -123) #=> "..7605"
 *
 *   # '#' flag for 'x' add a prefix '0x' for non-zero numbers.
 *   # '+' and space flag disables complements for negative numbers.
 *   sprintf("%x", 123)   #=> "7b"
 *   sprintf("%#x", 123)  #=> "0x7b"
 *   sprintf("%+x", -123) #=> "-7b"
 *   sprintf("%x", -123)  #=> "..f85"
 *   sprintf("%#x", -123) #=> "0x..f85"
 *   sprintf("%#x", 0)    #=> "0"
 *
 *   # '#' for 'X' uses the prefix '0X'.
 *   sprintf("%X", 123)  #=> "7B"
 *   sprintf("%#X", 123) #=> "0X7B"
 *
 *   # '#' flag for 'b' add a prefix '0b' for non-zero numbers.
 *   # '+' and space flag disables complements for negative numbers.
 *   sprintf("%b", 123)   #=> "1111011"
 *   sprintf("%#b", 123)  #=> "0b1111011"
 *   sprintf("%+b", -123) #=> "-1111011"
 *   sprintf("%b", -123)  #=> "..10000101"
 *   sprintf("%#b", -123) #=> "0b..10000101"
 *   sprintf("%#b", 0)    #=> "0"
 *
 *   # '#' for 'B' uses the prefix '0B'.
 *   sprintf("%B", 123)  #=> "1111011"
 *   sprintf("%#B", 123) #=> "0B1111011"
 *
 *   # '#' for 'e' forces to show the decimal point.
 *   sprintf("%.0e", 1)  #=> "1e+00"
 *   sprintf("%#.0e", 1) #=> "1.e+00"
 *
 *   # '#' for 'f' forces to show the decimal point.
 *   sprintf("%.0f", 1234)  #=> "1234"
 *   sprintf("%#.0f", 1234) #=> "1234."
 *
 *   # '#' for 'g' forces to show the decimal point.
 *   # It also disables stripping lowest zeros.
 *   sprintf("%g", 123.4)   #=> "123.4"
 *   sprintf("%#g", 123.4)  #=> "123.400"
 *   sprintf("%g", 123456)  #=> "123456"
 *   sprintf("%#g", 123456) #=> "123456."
 *
 *  The field width is an optional integer, followed optionally by a
 *  period and a precision.  The width specifies the minimum number of
 *  characters that will be written to the result for this field.
 *
 *  Examples of width:
 *
 *   # padding is done by spaces,       width=20
 *   # 0 or radix-1.             <------------------>
 *   sprintf("%20d", 123)   #=> "                 123"
 *   sprintf("%+20d", 123)  #=> "                +123"
 *   sprintf("%020d", 123)  #=> "00000000000000000123"
 *   sprintf("%+020d", 123) #=> "+0000000000000000123"
 *   sprintf("% 020d", 123) #=> " 0000000000000000123"
 *   sprintf("%-20d", 123)  #=> "123                 "
 *   sprintf("%-+20d", 123) #=> "+123                "
 *   sprintf("%- 20d", 123) #=> " 123                "
 *   sprintf("%020x", -123) #=> "..ffffffffffffffff85"
 *
 *  For
 *  numeric fields, the precision controls the number of decimal places
 *  displayed.  For string fields, the precision determines the maximum
 *  number of characters to be copied from the string.  (Thus, the format
 *  sequence <code>%10.10s</code> will always contribute exactly ten
 *  characters to the result.)
 *
 *  Examples of precisions:
 *
 *   # precision for 'd', 'o', 'x' and 'b' is
 *   # minimum number of digits               <------>
 *   sprintf("%20.8d", 123)  #=> "            00000123"
 *   sprintf("%20.8o", 123)  #=> "            00000173"
 *   sprintf("%20.8x", 123)  #=> "            0000007b"
 *   sprintf("%20.8b", 123)  #=> "            01111011"
 *   sprintf("%20.8d", -123) #=> "           -00000123"
 *   sprintf("%20.8o", -123) #=> "            ..777605"
 *   sprintf("%20.8x", -123) #=> "            ..ffff85"
 *   sprintf("%20.8b", -11)  #=> "            ..110101"
 *
 *   # "0x" and "0b" for '#x' and '#b' is not counted for
 *   # precision but "0" for '#o' is counted.  <------>
 *   sprintf("%#20.8d", 123)  #=> "            00000123"
 *   sprintf("%#20.8o", 123)  #=> "            00000173"
 *   sprintf("%#20.8x", 123)  #=> "          0x0000007b"
 *   sprintf("%#20.8b", 123)  #=> "          0b01111011"
 *   sprintf("%#20.8d", -123) #=> "           -00000123"
 *   sprintf("%#20.8o", -123) #=> "            ..777605"
 *   sprintf("%#20.8x", -123) #=> "          0x..ffff85"
 *   sprintf("%#20.8b", -11)  #=> "          0b..110101"
 *
 *   # precision for 'e' is number of
 *   # digits after the decimal point           <------>
 *   sprintf("%20.8e", 1234.56789) #=> "      1.23456789e+03"
 *
 *   # precision for 'f' is number of
 *   # digits after the decimal point               <------>
 *   sprintf("%20.8f", 1234.56789) #=> "       1234.56789000"
 *
 *   # precision for 'g' is number of
 *   # significant digits                          <------->
 *   sprintf("%20.8g", 1234.56789) #=> "           1234.5679"
 *
 *   #                                         <------->
 *   sprintf("%20.8g", 123456789)  #=> "       1.2345679e+08"
 *
 *   # precision for 's' is
 *   # maximum number of characters                    <------>
 *   sprintf("%20.8s", "string test") #=> "            string t"
 *
 *  Examples:
 *
 *     sprintf("%d %04x", 123, 123)               #=> "123 007b"
 *     sprintf("%08b '%4s'", 123, 123)            #=> "01111011 ' 123'"
 *     sprintf("%1$*2$s %2$d %1$s", "hello", 8)   #=> "   hello 8 hello"
 *     sprintf("%1$*2$s %2$d", "hello", -8)       #=> "hello    -8"
 *     sprintf("%+g:% g:%-g", 1.23, 1.23, 1.23)   #=> "+1.23: 1.23:1.23"
 *     sprintf("%u", -123)                        #=> "-123"
 *
 *  For more complex formatting, Ruby supports a reference by name.
 *  %<name>s style uses format style, but %{name} style doesn't.
 *
 *  Exapmles:
 *    sprintf("%<foo>d : %<bar>f", { :foo => 1, :bar => 2 })
 *      #=> 1 : 2.000000
 *    sprintf("%{foo}f", { :foo => 1 })
 *      # => "1f"
 */

value
_f_sprintf(state *mrb, value obj)
{
  _int argc;
  value *argv;

  _get_args(mrb, "*", &argv, &argc);

  if (argc <= 0) {
    _raise(mrb, E_ARGUMENT_ERROR, "too few arguments");
    return _nil_value();
  }
  else {
    return _str_format(mrb, argc - 1, argv + 1, argv[0]);
  }
}

value
_str_format(state *mrb, _int argc, const value *argv, value fmt)
{
  const char *p, *end;
  char *buf;
  _int blen;
  _int bsiz;
  value result;
  _int n;
  _int width;
  _int prec;
  int nextarg = 1;
  int posarg = 0;
  value nextvalue;
  value str;
  value hash = _undef_value();

#define CHECK_FOR_WIDTH(f)                                                  \
  if ((f) & FWIDTH) {                                                       \
    _raise(mrb, E_ARGUMENT_ERROR, "width given twice");         \
  }                                                                         \
  if ((f) & FPREC0) {                                                       \
    _raise(mrb, E_ARGUMENT_ERROR, "width after precision");     \
  }
#define CHECK_FOR_FLAGS(f)                                                  \
  if ((f) & FWIDTH) {                                                       \
    _raise(mrb, E_ARGUMENT_ERROR, "flag after width");          \
  }                                                                         \
  if ((f) & FPREC0) {                                                       \
    _raise(mrb, E_ARGUMENT_ERROR, "flag after precision");      \
  }

  ++argc;
  --argv;
  fmt = _str_to_str(mrb, fmt);
  p = RSTRING_PTR(fmt);
  end = p + RSTRING_LEN(fmt);
  blen = 0;
  bsiz = 120;
  result = _str_new_capa(mrb, bsiz);
  buf = RSTRING_PTR(result);
  memset(buf, 0, bsiz);

  for (; p < end; p++) {
    const char *t;
    _sym id = 0;
    int flags = FNONE;

    for (t = p; t < end && *t != '%'; t++) ;
    if (t + 1 == end) ++t;
    PUSH(p, t - p);
    if (t >= end)
      goto sprint_exit; /* end of fmt string */

    p = t + 1;    /* skip '%' */

    width = prec = -1;
    nextvalue = _undef_value();

retry:
    switch (*p) {
      default:
        _raisef(mrb, E_ARGUMENT_ERROR, "malformed format string - \\%%S", _str_new(mrb, p, 1));
        break;

      case ' ':
        CHECK_FOR_FLAGS(flags);
        flags |= FSPACE;
        p++;
        goto retry;

      case '#':
        CHECK_FOR_FLAGS(flags);
        flags |= FSHARP;
        p++;
        goto retry;

      case '+':
        CHECK_FOR_FLAGS(flags);
        flags |= FPLUS;
        p++;
        goto retry;

      case '-':
        CHECK_FOR_FLAGS(flags);
        flags |= FMINUS;
        p++;
        goto retry;

      case '0':
        CHECK_FOR_FLAGS(flags);
        flags |= FZERO;
        p++;
        goto retry;

      case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        n = 0;
        GETNUM(n, width);
        if (*p == '$') {
          if (!_undef_p(nextvalue)) {
            _raisef(mrb, E_ARGUMENT_ERROR, "value given twice - %S$", _fixnum_value(n));
          }
          nextvalue = GETPOSARG(n);
          p++;
          goto retry;
        }
        CHECK_FOR_WIDTH(flags);
        width = n;
        flags |= FWIDTH;
        goto retry;

      case '<':
      case '{': {
        const char *start = p;
        char term = (*p == '<') ? '>' : '}';
        value symname;

        for (; p < end && *p != term; )
          p++;
        if (id) {
          _raisef(mrb, E_ARGUMENT_ERROR, "name%S after <%S>",
                     _str_new(mrb, start, p - start + 1), _sym2str(mrb, id));
        }
        symname = _str_new(mrb, start + 1, p - start - 1);
        id = _intern_str(mrb, symname);
        nextvalue = GETNAMEARG(_symbol_value(id), start, (_int)(p - start + 1));
        if (_undef_p(nextvalue)) {
          _raisef(mrb, E_KEY_ERROR, "key%S not found", _str_new(mrb, start, p - start + 1));
        }
        if (term == '}') goto format_s;
        p++;
        goto retry;
      }

      case '*':
        CHECK_FOR_WIDTH(flags);
        flags |= FWIDTH;
        GETASTER(width);
        if (width < 0) {
          flags |= FMINUS;
          width = -width;
        }
        p++;
        goto retry;

      case '.':
        if (flags & FPREC0) {
          _raise(mrb, E_ARGUMENT_ERROR, "precision given twice");
        }
        flags |= FPREC|FPREC0;

        prec = 0;
        p++;
        if (*p == '*') {
          GETASTER(prec);
          if (prec < 0) {  /* ignore negative precision */
            flags &= ~FPREC;
          }
          p++;
          goto retry;
        }

        GETNUM(prec, precision);
        goto retry;

      case '\n':
      case '\0':
        p--;
        /* fallthrough */
      case '%':
        if (flags != FNONE) {
          _raise(mrb, E_ARGUMENT_ERROR, "invalid format character - %");
        }
        PUSH("%", 1);
        break;

      case 'c': {
        value val = GETARG();
        value tmp;
        char *c;

        tmp = _check_string_type(mrb, val);
        if (!_nil_p(tmp)) {
          if (RSTRING_LEN(tmp) != 1) {
            _raise(mrb, E_ARGUMENT_ERROR, "%c requires a character");
          }
        }
        else if (_fixnum_p(val)) {
          _int n = _fixnum(val);

          if (n < 0x80) {
            char buf[1];

            buf[0] = (char)n;
            tmp = _str_new(mrb, buf, 1);
          }
          else {
            tmp = _funcall(mrb, val, "chr", 0);
            _check_type(mrb, tmp, MRB_TT_STRING);
          }
        }
        else {
          _raise(mrb, E_ARGUMENT_ERROR, "invalid character");
        }
        c = RSTRING_PTR(tmp);
        n = RSTRING_LEN(tmp);
        if (!(flags & FWIDTH)) {
          PUSH(c, n);
        }
        else if ((flags & FMINUS)) {
          PUSH(c, n);
          if (width>0) FILL(' ', width-1);
        }
        else {
          if (width>0) FILL(' ', width-1);
          PUSH(c, n);
        }
      }
      break;

      case 's':
      case 'p':
  format_s:
      {
        value arg = GETARG();
        _int len;
        _int slen;

        if (*p == 'p') arg = _inspect(mrb, arg);
        str = _obj_as_string(mrb, arg);
        len = RSTRING_LEN(str);
        if (RSTRING(result)->flags & MRB_STR_EMBED) {
          _int tmp_n = len;
          RSTRING(result)->flags &= ~MRB_STR_EMBED_LEN_MASK;
          RSTRING(result)->flags |= tmp_n << MRB_STR_EMBED_LEN_SHIFT;
        }
        else {
          RSTRING(result)->as.heap.len = blen;
        }
        if (flags&(FPREC|FWIDTH)) {
          slen = RSTRING_LEN(str);
          if (slen < 0) {
            _raise(mrb, E_ARGUMENT_ERROR, "invalid mbstring sequence");
          }
          if ((flags&FPREC) && (prec < slen)) {
            char *p = RSTRING_PTR(str) + prec;
            slen = prec;
            len = (_int)(p - RSTRING_PTR(str));
          }
          /* need to adjust multi-byte string pos */
          if ((flags&FWIDTH) && (width > slen)) {
            width -= (int)slen;
            if (!(flags&FMINUS)) {
              FILL(' ', width);
            }
            PUSH(RSTRING_PTR(str), len);
            if (flags&FMINUS) {
              FILL(' ', width);
            }
            break;
          }
        }
        PUSH(RSTRING_PTR(str), len);
      }
      break;

      case 'd':
      case 'i':
      case 'o':
      case 'x':
      case 'X':
      case 'b':
      case 'B':
      case 'u': {
        value val = GETARG();
        char nbuf[68], *s;
        const char *prefix = NULL;
        int sign = 0, dots = 0;
        char sc = 0;
        _int v = 0;
        int base;
        _int len;

        if (flags & FSHARP) {
          switch (*p) {
            case 'o': prefix = "0"; break;
            case 'x': prefix = "0x"; break;
            case 'X': prefix = "0X"; break;
            case 'b': prefix = "0b"; break;
            case 'B': prefix = "0B"; break;
            default: break;
          }
        }

  bin_retry:
        switch (_type(val)) {
#ifndef MRB_WITHOUT_FLOAT
          case MRB_TT_FLOAT:
            val = _flo_to_fixnum(mrb, val);
            if (_fixnum_p(val)) goto bin_retry;
            break;
#endif
          case MRB_TT_STRING:
            val = _str_to_inum(mrb, val, 0, TRUE);
            goto bin_retry;
          case MRB_TT_FIXNUM:
            v = _fixnum(val);
            break;
          default:
            val = _Integer(mrb, val);
            goto bin_retry;
        }

        switch (*p) {
          case 'o':
            base = 8; break;
          case 'x':
          case 'X':
            base = 16; break;
          case 'b':
          case 'B':
            base = 2; break;
          case 'u':
          case 'd':
          case 'i':
            sign = 1;
          default:
            base = 10; break;
        }

        if (sign) {
          if (v >= 0) {
            if (flags & FPLUS) {
              sc = '+';
              width--;
            }
            else if (flags & FSPACE) {
              sc = ' ';
              width--;
            }
          }
          else {
            sc = '-';
            width--;
          }
          _assert(base == 10);
          snprintf(nbuf, sizeof(nbuf), "%" MRB_PRId, v);
          s = nbuf;
          if (v < 0) s++;       /* skip minus sign */
        }
        else {
          s = nbuf;
          if (v < 0) {
            dots = 1;
          }
          switch (base) {
          case 2:
            if (v < 0) {
              val = _fix2binstr(mrb, _fixnum_value(v), base);
            }
            else {
              val = _fixnum_to_str(mrb, _fixnum_value(v), base);
            }
            strncpy(++s, RSTRING_PTR(val), sizeof(nbuf)-1);
            break;
          case 8:
            snprintf(++s, sizeof(nbuf)-1, "%" MRB_PRIo, v);
            break;
          case 16:
            snprintf(++s, sizeof(nbuf)-1, "%" MRB_PRIx, v);
            break;
          }
          if (v < 0) {
            char d;

            s = remove_sign_bits(s, base);
            switch (base) {
              case 16: d = 'f'; break;
              case 8:  d = '7'; break;
              case 2:  d = '1'; break;
              default: d = 0; break;
            }

            if (d && *s != d) {
              *--s = d;
            }
          }
        }
        {
          size_t size;
          size = strlen(s);
          /* PARANOID: assert(size <= MRB_INT_MAX) */
          len = (_int)size;
        }

        if (*p == 'X') {
          char *pp = s;
          int c;
          while ((c = (int)(unsigned char)*pp) != 0) {
            *pp = toupper(c);
            pp++;
          }
        }

        if (prefix && !prefix[1]) { /* octal */
          if (dots) {
            prefix = NULL;
          }
          else if (len == 1 && *s == '0') {
            len = 0;
            if (flags & FPREC) prec--;
          }
          else if ((flags & FPREC) && (prec > len)) {
            prefix = NULL;
          }
        }
        else if (len == 1 && *s == '0') {
          prefix = NULL;
        }

        if (prefix) {
          size_t size;
          size = strlen(prefix);
          /* PARANOID: assert(size <= MRB_INT_MAX).
           *  this check is absolutely paranoid. */
          width -= (_int)size;
        }

        if ((flags & (FZERO|FMINUS|FPREC)) == FZERO) {
          prec = width;
          width = 0;
        }
        else {
          if (prec < len) {
            if (!prefix && prec == 0 && len == 1 && *s == '0') len = 0;
            prec = len;
          }
          width -= prec;
        }

        if (!(flags&FMINUS) && width > 0) {
          FILL(' ', width);
          width = 0;
        }

        if (sc) PUSH(&sc, 1);

        if (prefix) {
          int plen = (int)strlen(prefix);
          PUSH(prefix, plen);
        }
        if (dots) {
          prec -= 2;
          width -= 2;
          PUSH("..", 2);
        }

        if (prec > len) {
          CHECK(prec - len);
          if ((flags & (FMINUS|FPREC)) != FMINUS) {
            char c = '0';
            FILL(c, prec - len);
          } else if (v < 0) {
            char c = sign_bits(base, p);
            FILL(c, prec - len);
          }
        }
        PUSH(s, len);
        if (width > 0) {
          FILL(' ', width);
        }
      }
      break;

#ifndef MRB_WITHOUT_FLOAT
      case 'f':
      case 'g':
      case 'G':
      case 'e':
      case 'E':
      case 'a':
      case 'A': {
        value val = GETARG();
        double fval;
        _int i;
        _int need = 6;
        char fbuf[32];
        int frexp_result;

        fval = _float(_Float(mrb, val));
        if (!isfinite(fval)) {
          const char *expr;
          const _int elen = 3;
          char sign = '\0';

          if (isnan(fval)) {
            expr = "NaN";
          }
          else {
            expr = "Inf";
          }
          need = elen;
          if (!isnan(fval) && fval < 0.0)
            sign = '-';
          else if (flags & (FPLUS|FSPACE))
            sign = (flags & FPLUS) ? '+' : ' ';
          if (sign)
            ++need;
          if ((flags & FWIDTH) && need < width)
            need = width;

          if (need < 0) {
            _raise(mrb, E_ARGUMENT_ERROR, "width too big");
          }
          FILL(' ', need);
          if (flags & FMINUS) {
            if (sign)
              buf[blen - need--] = sign;
            memcpy(&buf[blen - need], expr, elen);
          }
          else {
            if (sign)
              buf[blen - elen - 1] = sign;
            memcpy(&buf[blen - elen], expr, elen);
          }
          break;
        }

        fmt_setup(fbuf, sizeof(fbuf), *p, flags, width, prec);
        need = 0;
        if (*p != 'e' && *p != 'E') {
          i = INT_MIN;
          frexp(fval, &frexp_result);
          i = (_int)frexp_result;
          if (i > 0)
            need = BIT_DIGITS(i);
        }
        if (need > MRB_INT_MAX - ((flags&FPREC) ? prec : 6)) {
        too_big_width:
          _raise(mrb, E_ARGUMENT_ERROR,
                    (width > prec ? "width too big" : "prec too big"));
        }
        need += (flags&FPREC) ? prec : 6;
        if ((flags&FWIDTH) && need < width)
          need = width;
        if (need > MRB_INT_MAX - 20) {
          goto too_big_width;
        }
        need += 20;

        CHECK(need);
        n = snprintf(&buf[blen], need, fbuf, fval);
        if (n < 0 || n >= need) {
          _raise(mrb, E_RUNTIME_ERROR, "formatting error");
        }
        blen += n;
      }
      break;
#endif
    }
    flags = FNONE;
  }

  sprint_exit:
#if 0
  /* XXX - We cannot validate the number of arguments if (digit)$ style used.
   */
  if (posarg >= 0 && nextarg < argc) {
    const char *mesg = "too many arguments for format string";
    if (_test(ruby_debug)) _raise(mrb, E_ARGUMENT_ERROR, mesg);
    if (_test(ruby_verbose)) _warn(mrb, "%S", _str_new_cstr(mrb, mesg));
  }
#endif
  _str_resize(mrb, result, blen);

  return result;
}

#ifndef MRB_WITHOUT_FLOAT
static void
fmt_setup(char *buf, size_t size, int c, int flags, _int width, _int prec)
{
  char *end = buf + size;
  int n;

  *buf++ = '%';
  if (flags & FSHARP) *buf++ = '#';
  if (flags & FPLUS)  *buf++ = '+';
  if (flags & FMINUS) *buf++ = '-';
  if (flags & FZERO)  *buf++ = '0';
  if (flags & FSPACE) *buf++ = ' ';

  if (flags & FWIDTH) {
    n = snprintf(buf, end - buf, "%d", (int)width);
    buf += n;
  }

  if (flags & FPREC) {
    n = snprintf(buf, end - buf, ".%d", (int)prec);
    buf += n;
  }

  *buf++ = c;
  *buf = '\0';
}
#endif
