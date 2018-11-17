#include <string.h>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/string.h>
#include <mruby/range.h>

static value
str_getbyte(state *mrb, value str)
{
  int pos;
  get_args(mrb, "i", &pos);

  if (pos < 0)
    pos += RSTRING_LEN(str);
  if (pos < 0 ||  RSTRING_LEN(str) <= pos)
    return nil_value();

  return fixnum_value((unsigned char)RSTRING_PTR(str)[pos]);
}

static value
str_setbyte(state *mrb, value str)
{
  int pos, byte;
  int len;

  get_args(mrb, "ii", &pos, &byte);

  len = RSTRING_LEN(str);
  if (pos < -len || len <= pos)
    raisef(mrb, E_INDEX_ERROR, "index %S is out of array", fixnum_value(pos));
  if (pos < 0)
    pos += len;

  str_modify(mrb, str_ptr(str));
  byte &= 0xff;
  RSTRING_PTR(str)[pos] = (unsigned char)byte;
  return fixnum_value((unsigned char)byte);
}

static value
str_byteslice(state *mrb, value str)
{
  value a1;
  int len;

  if (get_argc(mrb) == 2) {
    int pos;
    get_args(mrb, "ii", &pos, &len);
    return str_substr(mrb, str, pos, len);
  }
  get_args(mrb, "o|i", &a1, &len);
  switch (type(a1)) {
  case TT_RANGE:
    {
      int beg;

      len = RSTRING_LEN(str);
      switch (range_beg_len(mrb, a1, &beg, &len, len, TRUE)) {
      case 0:                   /* not range */
        break;
      case 1:                   /* range */
        return str_substr(mrb, str, beg, len);
      case 2:                   /* out of range */
        raisef(mrb, E_RANGE_ERROR, "%S out of range", a1);
        break;
      }
      return nil_value();
    }
#ifndef WITHOUT_FLOAT
  case TT_FLOAT:
    a1 = fixnum_value((int)float(a1));
    /* fall through */
#endif
  case TT_FIXNUM:
    return str_substr(mrb, str, fixnum(a1), 1);
  default:
    raise(mrb, E_TYPE_ERROR, "wrong type of argument");
  }
  /* not reached */
  return nil_value();
}

/*
 *  call-seq:
 *     str.swapcase!   -> str or nil
 *
 *  Equivalent to <code>String#swapcase</code>, but modifies the receiver in
 *  place, returning <i>str</i>, or <code>nil</code> if no changes were made.
 *  Note: case conversion is effective only in ASCII region.
 */
static value
str_swapcase_bang(state *mrb, value str)
{
  char *p, *pend;
  int modify = 0;
  struct RString *s = str_ptr(str);

  str_modify(mrb, s);
  p = RSTRING_PTR(str);
  pend = p + RSTRING_LEN(str);
  while (p < pend) {
    if (ISUPPER(*p)) {
      *p = TOLOWER(*p);
      modify = 1;
    }
    else if (ISLOWER(*p)) {
      *p = TOUPPER(*p);
      modify = 1;
    }
    p++;
  }

  if (modify) return str;
  return nil_value();
}

/*
 *  call-seq:
 *     str.swapcase   -> new_str
 *
 *  Returns a copy of <i>str</i> with uppercase alphabetic characters converted
 *  to lowercase and lowercase characters converted to uppercase.
 *  Note: case conversion is effective only in ASCII region.
 *
 *     "Hello".swapcase          #=> "hELLO"
 *     "cYbEr_PuNk11".swapcase   #=> "CyBeR_pUnK11"
 */
static value
str_swapcase(state *mrb, value self)
{
  value str;

  str = str_dup(mrb, self);
  str_swapcase_bang(mrb, str);
  return str;
}

static value fixnum_chr(state *mrb, value num);

/*
 *  call-seq:
 *     str << integer       -> str
 *     str.concat(integer)  -> str
 *     str << obj           -> str
 *     str.concat(obj)      -> str
 *
 *  Append---Concatenates the given object to <i>str</i>. If the object is a
 *  <code>Integer</code>, it is considered as a codepoint, and is converted
 *  to a character before concatenation.
 *
 *     a = "hello "
 *     a << "world"   #=> "hello world"
 *     a.concat(33)   #=> "hello world!"
 */
static value
str_concat_m(state *mrb, value self)
{
  value str;

  get_args(mrb, "o", &str);
  if (fixnum_p(str))
    str = fixnum_chr(mrb, str);
  else
    str = string_type(mrb, str);
  str_concat(mrb, self, str);
  return self;
}

/*
 *  call-seq:
 *     str.start_with?([prefixes]+)   -> true or false
 *
 *  Returns true if +str+ starts with one of the +prefixes+ given.
 *
 *    "hello".start_with?("hell")               #=> true
 *
 *    # returns true if one of the prefixes matches.
 *    "hello".start_with?("heaven", "hell")     #=> true
 *    "hello".start_with?("heaven", "paradise") #=> false
 *    "h".start_with?("heaven", "hell")         #=> false
 */
static value
str_start_with(state *mrb, value self)
{
  value *argv, sub;
  int argc, i;
  get_args(mrb, "*", &argv, &argc);

  for (i = 0; i < argc; i++) {
    size_t len_l, len_r;
    int ai = gc_arena_save(mrb);
    sub = string_type(mrb, argv[i]);
    gc_arena_restore(mrb, ai);
    len_l = RSTRING_LEN(self);
    len_r = RSTRING_LEN(sub);
    if (len_l >= len_r) {
      if (memcmp(RSTRING_PTR(self), RSTRING_PTR(sub), len_r) == 0) {
        return true_value();
      }
    }
  }
  return false_value();
}

/*
 *  call-seq:
 *     str.end_with?([suffixes]+)   -> true or false
 *
 *  Returns true if +str+ ends with one of the +suffixes+ given.
 */
static value
str_end_with(state *mrb, value self)
{
  value *argv, sub;
  int argc, i;
  get_args(mrb, "*", &argv, &argc);

  for (i = 0; i < argc; i++) {
    size_t len_l, len_r;
    int ai = gc_arena_save(mrb);
    sub = string_type(mrb, argv[i]);
    gc_arena_restore(mrb, ai);
    len_l = RSTRING_LEN(self);
    len_r = RSTRING_LEN(sub);
    if (len_l >= len_r) {
      if (memcmp(RSTRING_PTR(self) + (len_l - len_r),
                 RSTRING_PTR(sub),
                 len_r) == 0) {
        return true_value();
      }
    }
  }
  return false_value();
}

enum tr_pattern_type {
  TR_UNINITIALIZED = 0,
  TR_IN_ORDER  = 1,
  TR_RANGE = 2,
};

/*
  #tr Pattern syntax

  <syntax> ::= (<pattern>)* | '^' (<pattern>)*
  <pattern> ::= <in order> | <range>
  <in order> ::= (<ch>)+
  <range> ::= <ch> '-' <ch>
*/
struct tr_pattern {
  uint8_t type;		// 1:in-order, 2:range
  bool flag_reverse : 1;
  bool flag_on_heap : 1;
  uint16_t n;
  union {
    uint16_t start_pos;
    char ch[2];
  } val;
  struct tr_pattern *next;
};

#define STATIC_TR_PATTERN { 0 }

static inline void
tr_free_pattern(state *mrb, struct tr_pattern *pat)
{
  while (pat) {
    struct tr_pattern *p = pat->next;
    if (pat->flag_on_heap) {
      free(mrb, pat);
    }
    pat = p;
  }
}

static struct tr_pattern*
tr_parse_pattern(state *mrb, struct tr_pattern *ret, const value v_pattern, bool flag_reverse_enable)
{
  const char *pattern = RSTRING_PTR(v_pattern);
  int pattern_length = RSTRING_LEN(v_pattern);
  bool flag_reverse = FALSE;
  struct tr_pattern *pat1;
  int i = 0;

  if(flag_reverse_enable && pattern_length >= 2 && pattern[0] == '^') {
    flag_reverse = TRUE;
    i++;
  }

  while (i < pattern_length) {
    /* is range pattern ? */
    bool const ret_uninit = (ret->type == TR_UNINITIALIZED);
    pat1 = ret_uninit
           ? ret
           : (struct tr_pattern*)malloc_simple(mrb, sizeof(struct tr_pattern));
    if ((i+2) < pattern_length && pattern[i] != '\\' && pattern[i+1] == '-') {
      if (pat1 == NULL && ret) {
      nomem:
        tr_free_pattern(mrb, ret);
        exc_raise(mrb, obj_value(mrb->nomem_err));
        return NULL;            /* not reached */
      }
      pat1->type = TR_RANGE;
      pat1->flag_reverse = flag_reverse;
      pat1->flag_on_heap = !ret_uninit;
      pat1->n = pattern[i+2] - pattern[i] + 1;
      pat1->next = NULL;
      pat1->val.ch[0] = pattern[i];
      pat1->val.ch[1] = pattern[i+2];
      i += 3;
    }
    else {
      /* in order pattern. */
      int start_pos = i++;
      int len;

      while (i < pattern_length) {
	if ((i+2) < pattern_length && pattern[i] != '\\' && pattern[i+1] == '-')
          break;
	i++;
      }

      len = i - start_pos;
      if (pat1 == NULL && ret) {
        goto nomem;
      }
      pat1->type = TR_IN_ORDER;
      pat1->flag_reverse = flag_reverse;
      pat1->flag_on_heap = !ret_uninit;
      pat1->n = len;
      pat1->next = NULL;
      pat1->val.start_pos = start_pos;
    }

    if (ret == NULL || ret_uninit) {
      ret = pat1;
    }
    else {
      struct tr_pattern *p = ret;
      while (p->next != NULL) {
        p = p->next;
      }
      p->next = pat1;
    }
  }

  return ret;
}

static inline int
tr_find_character(const struct tr_pattern *pat, const char *pat_str, int ch)
{
  int ret = -1;
  int n_sum = 0;
  int flag_reverse = pat ? pat->flag_reverse : 0;

  while (pat != NULL) {
    if (pat->type == TR_IN_ORDER) {
      int i;
      for (i = 0; i < pat->n; i++) {
	if (pat_str[pat->val.start_pos + i] == ch) ret = n_sum + i;
      }
    }
    else if (pat->type == TR_RANGE) {
      if (pat->val.ch[0] <= ch && ch <= pat->val.ch[1])
        ret = n_sum + ch - pat->val.ch[0];
    }
    else {
      assert(pat->type == TR_UNINITIALIZED);
    }
    n_sum += pat->n;
    pat = pat->next;
  }

  if (flag_reverse) {
    return (ret < 0) ? INT_MAX : -1;
  }
  return ret;
}

static inline int
tr_get_character(const struct tr_pattern *pat, const char *pat_str, int n_th)
{
  int n_sum = 0;

  while (pat != NULL) {
    if (n_th < (n_sum + pat->n)) {
      int i = (n_th - n_sum);

      switch (pat->type) {
      case TR_IN_ORDER:
        return pat_str[pat->val.start_pos + i];
      case TR_RANGE:
        return pat->val.ch[0]+i;
      case TR_UNINITIALIZED:
        return -1;
      }
    }
    if (pat->next == NULL) {
      switch (pat->type) {
      case TR_IN_ORDER:
        return pat_str[pat->val.start_pos + pat->n - 1];
      case TR_RANGE:
        return pat->val.ch[1];
      case TR_UNINITIALIZED:
        return -1;
      }
    }
    n_sum += pat->n;
    pat = pat->next;
  }

  return -1;
}

static bool
str_tr(state *mrb, value str, value p1, value p2, bool squeeze)
{
  struct tr_pattern pat = STATIC_TR_PATTERN;
  struct tr_pattern rep_storage = STATIC_TR_PATTERN;
  char *s;
  int len;
  int i;
  int j;
  bool flag_changed = FALSE;
  int lastch = -1;
  struct tr_pattern *rep;

  str_modify(mrb, str_ptr(str));
  tr_parse_pattern(mrb, &pat, p1, TRUE);
  rep = tr_parse_pattern(mrb, &rep_storage, p2, FALSE);
  s = RSTRING_PTR(str);
  len = RSTRING_LEN(str);

  for (i=j=0; i<len; i++,j++) {
    int n = tr_find_character(&pat, RSTRING_PTR(p1), s[i]);

    if (i>j) s[j] = s[i];
    if (n >= 0) {
      flag_changed = TRUE;
      if (rep == NULL) {
	j--;
      }
      else {
        int c = tr_get_character(rep, RSTRING_PTR(p2), n);

        if (c < 0 || (squeeze && c == lastch)) {
          j--;
          continue;
        }
        if (c > 0x80) {
          raisef(mrb, E_ARGUMENT_ERROR, "character (%S) out of range",
                     fixnum_value((int)c));
        }
	lastch = c;
	s[i] = (char)c;
      }
    }
  }

  tr_free_pattern(mrb, &pat);
  tr_free_pattern(mrb, rep);

  if (flag_changed) {
    RSTR_SET_LEN(RSTRING(str), j);
    RSTRING_PTR(str)[j] = 0;
  }
  return flag_changed;
}

/*
 * call-seq:
 *   str.tr(from_str, to_str)   => new_str
 *
 * Returns a copy of str with the characters in from_str replaced by the
 * corresponding characters in to_str.  If to_str is shorter than from_str,
 * it is padded with its last character in order to maintain the
 * correspondence.
 *
 *  "hello".tr('el', 'ip')      #=> "hippo"
 *  "hello".tr('aeiou', '*')    #=> "h*ll*"
 *  "hello".tr('aeiou', 'AA*')  #=> "hAll*"
 *
 * Both strings may use the c1-c2 notation to denote ranges of characters,
 * and from_str may start with a ^, which denotes all characters except
 * those listed.
 *
 *  "hello".tr('a-y', 'b-z')    #=> "ifmmp"
 *  "hello".tr('^aeiou', '*')   #=> "*e**o"
 *
 * The backslash character \ can be used to escape ^ or - and is otherwise
 * ignored unless it appears at the end of a range or the end of the
 * from_str or to_str:
 *
 *
 *  "hello^world".tr("\\^aeiou", "*") #=> "h*ll**w*rld"
 *  "hello-world".tr("a\\-eo", "*")   #=> "h*ll**w*rld"
 *
 *  "hello\r\nworld".tr("\r", "")   #=> "hello\nworld"
 *  "hello\r\nworld".tr("\\r", "")  #=> "hello\r\nwold"
 *  "hello\r\nworld".tr("\\\r", "") #=> "hello\nworld"
 *
 *  "X['\\b']".tr("X\\", "")   #=> "['b']"
 *  "X['\\b']".tr("X-\\]", "") #=> "'b'"
 *
 *  Note: conversion is effective only in ASCII region.
 */
static value
str_tr(state *mrb, value str)
{
  value dup;
  value p1, p2;

  get_args(mrb, "SS", &p1, &p2);
  dup = str_dup(mrb, str);
  str_tr(mrb, dup, p1, p2, FALSE);
  return dup;
}

/*
 * call-seq:
 *   str.tr!(from_str, to_str)   -> str or nil
 *
 * Translates str in place, using the same rules as String#tr.
 * Returns str, or nil if no changes were made.
 */
static value
str_tr_bang(state *mrb, value str)
{
  value p1, p2;

  get_args(mrb, "SS", &p1, &p2);
  if (str_tr(mrb, str, p1, p2, FALSE)) {
    return str;
  }
  return nil_value();
}

/*
 * call-seq:
 *   str.tr_s(from_str, to_str)   -> new_str
 *
 * Processes a copy of str as described under String#tr, then removes
 * duplicate characters in regions that were affected by the translation.
 *
 *  "hello".tr_s('l', 'r')     #=> "hero"
 *  "hello".tr_s('el', '*')    #=> "h*o"
 *  "hello".tr_s('el', 'hx')   #=> "hhxo"
 */
static value
str_tr_s(state *mrb, value str)
{
  value dup;
  value p1, p2;

  get_args(mrb, "SS", &p1, &p2);
  dup = str_dup(mrb, str);
  str_tr(mrb, dup, p1, p2, TRUE);
  return dup;
}

/*
 * call-seq:
 *   str.tr_s!(from_str, to_str)   -> str or nil
 *
 * Performs String#tr_s processing on str in place, returning
 * str, or nil if no changes were made.
 */
static value
str_tr_s_bang(state *mrb, value str)
{
  value p1, p2;

  get_args(mrb, "SS", &p1, &p2);
  if (str_tr(mrb, str, p1, p2, TRUE)) {
    return str;
  }
  return nil_value();
}

static bool
str_squeeze(state *mrb, value str, value v_pat)
{
  struct tr_pattern pat_storage = STATIC_TR_PATTERN;
  struct tr_pattern *pat = NULL;
  int i, j;
  char *s;
  int len;
  bool flag_changed = FALSE;
  int lastch = -1;

  str_modify(mrb, str_ptr(str));
  if (!nil_p(v_pat)) {
    pat = tr_parse_pattern(mrb, &pat_storage, v_pat, TRUE);
  }
  s = RSTRING_PTR(str);
  len = RSTRING_LEN(str);

  if (pat) {
    for (i=j=0; i<len; i++,j++) {
      int n = tr_find_character(pat, RSTRING_PTR(v_pat), s[i]);

      if (i>j) s[j] = s[i];
      if (n >= 0 && s[i] == lastch) {
        flag_changed = TRUE;
        j--;
      }
      lastch = s[i];
    }
  }
  else {
    for (i=j=0; i<len; i++,j++) {
      if (i>j) s[j] = s[i];
      if (s[i] >= 0 && s[i] == lastch) {
        flag_changed = TRUE;
        j--;
      }
      lastch = s[i];
    }
  }
  tr_free_pattern(mrb, pat);

  if (flag_changed) {
    RSTR_SET_LEN(RSTRING(str), j);
    RSTRING_PTR(str)[j] = 0;
  }
  return flag_changed;
}

/*
 * call-seq:
 *   str.squeeze([other_str])    -> new_str
 *
 * Builds a set of characters from the other_str
 * parameter(s) using the procedure described for String#count. Returns a
 * new string where runs of the same character that occur in this set are
 * replaced by a single character. If no arguments are given, all runs of
 * identical characters are replaced by a single character.
 *
 *  "yellow moon".squeeze                  #=> "yelow mon"
 *  "  now   is  the".squeeze(" ")         #=> " now is the"
 *  "putters shoot balls".squeeze("m-z")   #=> "puters shot balls"
 */
static value
str_squeeze(state *mrb, value str)
{
  value pat = nil_value();
  value dup;

  get_args(mrb, "|S", &pat);
  dup = str_dup(mrb, str);
  str_squeeze(mrb, dup, pat);
  return dup;
}

/*
 * call-seq:
 *   str.squeeze!([other_str])   -> str or nil
 *
 * Squeezes str in place, returning either str, or nil if no
 * changes were made.
 */
static value
str_squeeze_bang(state *mrb, value str)
{
  value pat = nil_value();

  get_args(mrb, "|S", &pat);
  if (str_squeeze(mrb, str, pat)) {
    return str;
  }
  return nil_value();
}

static bool
str_delete(state *mrb, value str, value v_pat)
{
  struct tr_pattern pat = STATIC_TR_PATTERN;
  int i, j;
  char *s;
  int len;
  bool flag_changed = FALSE;

  str_modify(mrb, str_ptr(str));
  tr_parse_pattern(mrb, &pat, v_pat, TRUE);
  s = RSTRING_PTR(str);
  len = RSTRING_LEN(str);

  for (i=j=0; i<len; i++,j++) {
    int n = tr_find_character(&pat, RSTRING_PTR(v_pat), s[i]);

    if (i>j) s[j] = s[i];
    if (n >= 0) {
      flag_changed = TRUE;
      j--;
    }
  }
  tr_free_pattern(mrb, &pat);
  if (flag_changed) {
    RSTR_SET_LEN(RSTRING(str), j);
    RSTRING_PTR(str)[j] = 0;
  }
  return flag_changed;
}

static value
str_delete(state *mrb, value str)
{
  value pat;
  value dup;

  get_args(mrb, "S", &pat);
  dup = str_dup(mrb, str);
  str_delete(mrb, dup, pat);
  return dup;
}

static value
str_delete_bang(state *mrb, value str)
{
  value pat;

  get_args(mrb, "S", &pat);
  if (str_delete(mrb, str, pat)) {
    return str;
  }
  return nil_value();
}

/*
 * call_seq:
 *   str.count([other_str])   -> integer
 *
 * Each other_str parameter defines a set of characters to count.  The
 * intersection of these sets defines the characters to count in str.  Any
 * other_str that starts with a caret ^ is negated.  The sequence c1-c2
 * means all characters between c1 and c2.  The backslash character \ can
 * be used to escape ^ or - and is otherwise ignored unless it appears at
 * the end of a sequence or the end of a other_str.
 */
static value
str_count(state *mrb, value str)
{
  value v_pat = nil_value();
  int i;
  char *s;
  int len;
  int count = 0;
  struct tr_pattern pat = STATIC_TR_PATTERN;

  get_args(mrb, "S", &v_pat);
  tr_parse_pattern(mrb, &pat, v_pat, TRUE);
  s = RSTRING_PTR(str);
  len = RSTRING_LEN(str);
  for (i = 0; i < len; i++) {
    int n = tr_find_character(&pat, RSTRING_PTR(v_pat), s[i]);

    if (n >= 0) count++;
  }
  tr_free_pattern(mrb, &pat);
  return fixnum_value(count);
}

static value
str_hex(state *mrb, value self)
{
  return str_to_inum(mrb, self, 16, FALSE);
}

static value
str_oct(state *mrb, value self)
{
  return str_to_inum(mrb, self, 8, FALSE);
}

/*
 *  call-seq:
 *     string.chr    ->  string
 *
 *  Returns a one-character string at the beginning of the string.
 *
 *     a = "abcde"
 *     a.chr    #=> "a"
 */
static value
str_chr(state *mrb, value self)
{
  return str_substr(mrb, self, 0, 1);
}

static value
fixnum_chr(state *mrb, value num)
{
  int cp = fixnum(num);
#ifdef UTF8_STRING
  char utf8[4];
  int len;

  if (cp < 0 || 0x10FFFF < cp) {
    raisef(mrb, E_RANGE_ERROR, "%S out of char range", num);
  }
  if (cp < 0x80) {
    utf8[0] = (char)cp;
    len = 1;
  }
  else if (cp < 0x800) {
    utf8[0] = (char)(0xC0 | (cp >> 6));
    utf8[1] = (char)(0x80 | (cp & 0x3F));
    len = 2;
  }
  else if (cp < 0x10000) {
    utf8[0] = (char)(0xE0 |  (cp >> 12));
    utf8[1] = (char)(0x80 | ((cp >>  6) & 0x3F));
    utf8[2] = (char)(0x80 | ( cp        & 0x3F));
    len = 3;
  }
  else {
    utf8[0] = (char)(0xF0 |  (cp >> 18));
    utf8[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    utf8[2] = (char)(0x80 | ((cp >>  6) & 0x3F));
    utf8[3] = (char)(0x80 | ( cp        & 0x3F));
    len = 4;
  }
  return str_new(mrb, utf8, len);
#else
  char c;

  if (cp < 0 || 0xff < cp) {
    raisef(mrb, E_RANGE_ERROR, "%S out of char range", num);
  }
  c = (char)cp;
  return str_new(mrb, &c, 1);
#endif
}

/*
 *  call-seq:
 *     string.succ    ->  string
 *
 *  Returns next sequence of the string;
 *
 *     a = "abc"
 *     a.succ    #=> "abd"
 */
static value
str_succ_bang(state *mrb, value self)
{
  value result;
  unsigned char *p, *e, *b, *t;
  const char *prepend;
  struct RString *s = str_ptr(self);
  int l;

  if (RSTRING_LEN(self) == 0)
    return self;

  str_modify(mrb, s);
  l = RSTRING_LEN(self);
  b = p = (unsigned char*) RSTRING_PTR(self);
  t = e = p + l;
  *(e--) = 0;

  // find trailing ascii/number
  while (e >= b) {
    if (ISALNUM(*e))
      break;
    e--;
  }
  if (e < b) {
    e = p + l - 1;
    result = str_new_lit(mrb, "");
  }
  else {
    // find leading letter of the ascii/number
    b = e;
    while (b > p) {
      if (!ISALNUM(*b) || (ISALNUM(*b) && *b != '9' && *b != 'z' && *b != 'Z'))
        break;
      b--;
    }
    if (!ISALNUM(*b))
      b++;
    result = str_new(mrb, (char*) p, b - p);
  }

  while (e >= b) {
    if (!ISALNUM(*e)) {
      if (*e == 0xff) {
        str_cat_lit(mrb, result, "\x01");
        (*e) = 0;
      }
      else
        (*e)++;
      break;
    }
    prepend = NULL;
    if (*e == '9') {
      if (e == b) prepend = "1";
      *e = '0';
    }
    else if (*e == 'z') {
      if (e == b) prepend = "a";
      *e = 'a';
    }
    else if (*e == 'Z') {
      if (e == b) prepend = "A";
      *e = 'A';
    }
    else {
      (*e)++;
      break;
    }
    if (prepend) str_cat_cstr(mrb, result, prepend);
    e--;
  }
  result = str_cat(mrb, result, (char*) b, t - b);
  l = RSTRING_LEN(result);
  str_resize(mrb, self, l);
  memcpy(RSTRING_PTR(self), RSTRING_PTR(result), l);
  return self;
}

static value
str_succ(state *mrb, value self)
{
  value str;

  str = str_dup(mrb, self);
  str_succ_bang(mrb, str);
  return str;
}

#ifdef UTF8_STRING
static const char utf8len_codepage_zero[256] =
{
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,
};

static int
utf8code(unsigned char* p)
{
  int len;

  if (p[0] < 0x80)
    return p[0];

  len = utf8len_codepage_zero[p[0]];
  if (len > 1 && (p[1] & 0xc0) == 0x80) {
    if (len == 2)
      return ((p[0] & 0x1f) << 6) + (p[1] & 0x3f);
    if ((p[2] & 0xc0) == 0x80) {
      if (len == 3)
        return ((p[0] & 0x0f) << 12) + ((p[1] & 0x3f) << 6)
          + (p[2] & 0x3f);
      if ((p[3] & 0xc0) == 0x80) {
        if (len == 4)
          return ((p[0] & 0x07) << 18) + ((p[1] & 0x3f) << 12)
            + ((p[2] & 0x3f) << 6) + (p[3] & 0x3f);
        if ((p[4] & 0xc0) == 0x80) {
          if (len == 5)
            return ((p[0] & 0x03) << 24) + ((p[1] & 0x3f) << 18)
              + ((p[2] & 0x3f) << 12) + ((p[3] & 0x3f) << 6)
              + (p[4] & 0x3f);
          if ((p[5] & 0xc0) == 0x80 && len == 6)
            return ((p[0] & 0x01) << 30) + ((p[1] & 0x3f) << 24)
              + ((p[2] & 0x3f) << 18) + ((p[3] & 0x3f) << 12)
              + ((p[4] & 0x3f) << 6) + (p[5] & 0x3f);
        }
      }
    }
  }
  return p[0];
}

static value
str_ord(state* mrb, value str)
{
  if (RSTRING_LEN(str) == 0)
    raise(mrb, E_ARGUMENT_ERROR, "empty string");
  return fixnum_value(utf8code((unsigned char*) RSTRING_PTR(str)));
}
#else
static value
str_ord(state* mrb, value str)
{
  if (RSTRING_LEN(str) == 0)
    raise(mrb, E_ARGUMENT_ERROR, "empty string");
  return fixnum_value((unsigned char)RSTRING_PTR(str)[0]);
}
#endif

/*
 *  call-seq:
 *     str.delete_prefix!(prefix) -> self or nil
 *
 *  Deletes leading <code>prefix</code> from <i>str</i>, returning
 *  <code>nil</code> if no change was made.
 *
 *     "hello".delete_prefix!("hel") #=> "lo"
 *     "hello".delete_prefix!("llo") #=> nil
 */
static value
str_del_prefix_bang(state *mrb, value self)
{
  int plen, slen;
  char *ptr, *s;
  struct RString *str = RSTRING(self);

  get_args(mrb, "s", &ptr, &plen);
  slen = RSTR_LEN(str);
  if (plen > slen) return nil_value();
  s = RSTR_PTR(str);
  if (memcmp(s, ptr, plen) != 0) return nil_value();
  if (!FROZEN_P(str) && (RSTR_SHARED_P(str) || RSTR_FSHARED_P(str))) {
    str->as.heap.ptr += plen;
  }
  else {
    str_modify(mrb, str);
    s = RSTR_PTR(str);
    memmove(s, s+plen, slen-plen);
  }
  RSTR_SET_LEN(str, slen-plen);
  return self;
}

/*
 *  call-seq:
 *     str.delete_prefix(prefix) -> new_str
 *
 *  Returns a copy of <i>str</i> with leading <code>prefix</code> deleted.
 *
 *     "hello".delete_prefix("hel") #=> "lo"
 *     "hello".delete_prefix("llo") #=> "hello"
 */
static value
str_del_prefix(state *mrb, value self)
{
  int plen, slen;
  char *ptr;

  get_args(mrb, "s", &ptr, &plen);
  slen = RSTRING_LEN(self);
  if (plen > slen) return str_dup(mrb, self);
  if (memcmp(RSTRING_PTR(self), ptr, plen) != 0)
    return str_dup(mrb, self);
  return str_substr(mrb, self, plen, slen-plen);
}

/*
 *  call-seq:
 *     str.delete_suffix!(suffix) -> self or nil
 *
 *  Deletes trailing <code>suffix</code> from <i>str</i>, returning
 *  <code>nil</code> if no change was made.
 *
 *     "hello".delete_suffix!("llo") #=> "he"
 *     "hello".delete_suffix!("hel") #=> nil
 */
static value
str_del_suffix_bang(state *mrb, value self)
{
  int plen, slen;
  char *ptr, *s;
  struct RString *str = RSTRING(self);

  get_args(mrb, "s", &ptr, &plen);
  slen = RSTR_LEN(str);
  if (plen > slen) return nil_value();
  s = RSTR_PTR(str);
  if (memcmp(s+slen-plen, ptr, plen) != 0) return nil_value();
  if (!FROZEN_P(str) && (RSTR_SHARED_P(str) || RSTR_FSHARED_P(str))) {
    /* no need to modify string */
  }
  else {
    str_modify(mrb, str);
  }
  RSTR_SET_LEN(str, slen-plen);
  return self;
}

/*
 *  call-seq:
 *     str.delete_suffix(suffix) -> new_str
 *
 *  Returns a copy of <i>str</i> with leading <code>suffix</code> deleted.
 *
 *     "hello".delete_suffix("hel") #=> "lo"
 *     "hello".delete_suffix("llo") #=> "hello"
 */
static value
str_del_suffix(state *mrb, value self)
{
  int plen, slen;
  char *ptr;

  get_args(mrb, "s", &ptr, &plen);
  slen = RSTRING_LEN(self);
  if (plen > slen) return str_dup(mrb, self);
  if (memcmp(RSTRING_PTR(self)+slen-plen, ptr, plen) != 0)
    return str_dup(mrb, self);
  return str_substr(mrb, self, 0, slen-plen);
}

static value
str_lines(state *mrb, value self)
{
  value result;
  int ai;
  int len;
  char *b = RSTRING_PTR(self);
  char *p = b, *t;
  char *e = b + RSTRING_LEN(self);

  get_args(mrb, "");

  result = ary_new(mrb);
  ai = gc_arena_save(mrb);
  while (p < e) {
    t = p;
    while (p < e && *p != '\n') p++;
    if (*p == '\n') p++;
    len = (int) (p - t);
    ary_push(mrb, result, str_new(mrb, t, len));
    gc_arena_restore(mrb, ai);
  }
  return result;
}

void
mruby_string_ext_gem_init(state* mrb)
{
  struct RClass * s = mrb->string_class;

  define_method(mrb, s, "dump",            str_dump,            ARGS_NONE());
  define_method(mrb, s, "getbyte",         str_getbyte,         ARGS_REQ(1));
  define_method(mrb, s, "setbyte",         str_setbyte,         ARGS_REQ(2));
  define_method(mrb, s, "byteslice",       str_byteslice,       ARGS_REQ(1)|ARGS_OPT(1));
  define_method(mrb, s, "swapcase!",       str_swapcase_bang,   ARGS_NONE());
  define_method(mrb, s, "swapcase",        str_swapcase,        ARGS_NONE());
  define_method(mrb, s, "concat",          str_concat_m,        ARGS_REQ(1));
  define_method(mrb, s, "<<",              str_concat_m,        ARGS_REQ(1));
  define_method(mrb, s, "count",           str_count,           ARGS_REQ(1));
  define_method(mrb, s, "tr",              str_tr,              ARGS_REQ(2));
  define_method(mrb, s, "tr!",             str_tr_bang,         ARGS_REQ(2));
  define_method(mrb, s, "tr_s",            str_tr_s,            ARGS_REQ(2));
  define_method(mrb, s, "tr_s!",           str_tr_s_bang,       ARGS_REQ(2));
  define_method(mrb, s, "squeeze",         str_squeeze,         ARGS_OPT(1));
  define_method(mrb, s, "squeeze!",        str_squeeze_bang,    ARGS_OPT(1));
  define_method(mrb, s, "delete",          str_delete,          ARGS_REQ(1));
  define_method(mrb, s, "delete!",         str_delete_bang,     ARGS_REQ(1));
  define_method(mrb, s, "start_with?",     str_start_with,      ARGS_REST());
  define_method(mrb, s, "end_with?",       str_end_with,        ARGS_REST());
  define_method(mrb, s, "hex",             str_hex,             ARGS_NONE());
  define_method(mrb, s, "oct",             str_oct,             ARGS_NONE());
  define_method(mrb, s, "chr",             str_chr,             ARGS_NONE());
  define_method(mrb, s, "succ",            str_succ,            ARGS_NONE());
  define_method(mrb, s, "succ!",           str_succ_bang,       ARGS_NONE());
  define_alias(mrb,  s, "next",            "succ");
  define_alias(mrb,  s, "next!",           "succ!");
  define_method(mrb, s, "ord",             str_ord,             ARGS_NONE());
  define_method(mrb, s, "delete_prefix!",  str_del_prefix_bang, ARGS_REQ(1));
  define_method(mrb, s, "delete_prefix",   str_del_prefix,      ARGS_REQ(1));
  define_method(mrb, s, "delete_suffix!",  str_del_suffix_bang, ARGS_REQ(1));
  define_method(mrb, s, "delete_suffix",   str_del_suffix,      ARGS_REQ(1));

  define_method(mrb, s, "__lines",         str_lines,           ARGS_NONE());
  define_method(mrb, mrb->fixnum_class, "chr", fixnum_chr, ARGS_NONE());
}

void
mruby_string_ext_gem_final(state* mrb)
{
}
