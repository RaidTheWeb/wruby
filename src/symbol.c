/*
** symbol.c - Symbol class
**
** See Copyright Notice in mruby.h
*/

#include <limits.h>
#include <string.h>
#include <mruby.h>
#include <mruby/khash.h>
#include <mruby/string.h>
#include <mruby/dump.h>
#include <mruby/class.h>

/* ------------------------------------------------------ */
typedef struct symbol_name {
  $bool lit : 1;
  uint16_t len;
  const char *name;
} symbol_name;

static inline khint_t
sym_hash_func($state *mrb, $sym s)
{
  khint_t h = 0;
  size_t i, len = mrb->symtbl[s].len;
  const char *p = mrb->symtbl[s].name;

  for (i=0; i<len; i++) {
    h = (h << 5) - h + *p++;
  }
  return h;
}
#define sym_hash_equal(mrb,a, b) (mrb->symtbl[a].len == mrb->symtbl[b].len && memcmp(mrb->symtbl[a].name, mrb->symtbl[b].name, mrb->symtbl[a].len) == 0)

KHASH_DECLARE(n2s, $sym, $sym, FALSE)
KHASH_DEFINE (n2s, $sym, $sym, FALSE, sym_hash_func, sym_hash_equal)
/* ------------------------------------------------------ */

static void
sym_validate_len($state *mrb, size_t len)
{
  if (len >= RITE_LV_NULL_MARK) {
    $raise(mrb, E_ARGUMENT_ERROR, "symbol length too long");
  }
}

static $sym
sym_intern($state *mrb, const char *name, size_t len, $bool lit)
{
  khash_t(n2s) *h = mrb->name2sym;
  symbol_name *sname = mrb->symtbl; /* symtbl[0] for working memory */
  khiter_t k;
  $sym sym;
  char *p;

  sym_validate_len(mrb, len);
  if (sname) {
    sname->lit = lit;
    sname->len = (uint16_t)len;
    sname->name = name;
    k = kh_get(n2s, mrb, h, 0);
    if (k != kh_end(h))
      return kh_key(h, k);
  }

  /* registering a new symbol */
  sym = ++mrb->symidx;
  if (mrb->symcapa < sym) {
    if (mrb->symcapa == 0) mrb->symcapa = 100;
    else mrb->symcapa = (size_t)(mrb->symcapa * 6 / 5);
    mrb->symtbl = (symbol_name*)$realloc(mrb, mrb->symtbl, sizeof(symbol_name)*(mrb->symcapa+1));
  }
  sname = &mrb->symtbl[sym];
  sname->len = (uint16_t)len;
  if (lit || $ro_data_p(name)) {
    sname->name = name;
    sname->lit = TRUE;
  }
  else {
    p = (char *)$malloc(mrb, len+1);
    memcpy(p, name, len);
    p[len] = 0;
    sname->name = (const char*)p;
    sname->lit = FALSE;
  }
  kh_put(n2s, mrb, h, sym);

  return sym;
}

$API $sym
$intern($state *mrb, const char *name, size_t len)
{
  return sym_intern(mrb, name, len, FALSE);
}

$API $sym
$intern_static($state *mrb, const char *name, size_t len)
{
  return sym_intern(mrb, name, len, TRUE);
}

$API $sym
$intern_cstr($state *mrb, const char *name)
{
  return $intern(mrb, name, strlen(name));
}

$API $sym
$intern_str($state *mrb, $value str)
{
  return $intern(mrb, RSTRING_PTR(str), RSTRING_LEN(str));
}

$API $value
$check_intern($state *mrb, const char *name, size_t len)
{
  khash_t(n2s) *h = mrb->name2sym;
  symbol_name *sname = mrb->symtbl;
  khiter_t k;

  sym_validate_len(mrb, len);
  sname->len = (uint16_t)len;
  sname->name = name;

  k = kh_get(n2s, mrb, h, 0);
  if (k != kh_end(h)) {
    return $symbol_value(kh_key(h, k));
  }
  return $nil_value();
}

$API $value
$check_intern_cstr($state *mrb, const char *name)
{
  return $check_intern(mrb, name, ($int)strlen(name));
}

$API $value
$check_intern_str($state *mrb, $value str)
{
  return $check_intern(mrb, RSTRING_PTR(str), RSTRING_LEN(str));
}

/* lenp must be a pointer to a size_t variable */
$API const char*
$sym2name_len($state *mrb, $sym sym, $int *lenp)
{
  if (sym == 0 || mrb->symidx < sym) {
    if (lenp) *lenp = 0;
    return NULL;
  }

  if (lenp) *lenp = mrb->symtbl[sym].len;
  return mrb->symtbl[sym].name;
}

void
$free_symtbl($state *mrb)
{
  $sym i, lim;

  for (i=1, lim=mrb->symidx+1; i<lim; i++) {
    if (!mrb->symtbl[i].lit) {
      $free(mrb, (char*)mrb->symtbl[i].name);
    }
  }
  $free(mrb, mrb->symtbl);
  kh_destroy(n2s, mrb, mrb->name2sym);
}

void
$init_symtbl($state *mrb)
{
  mrb->name2sym = kh_init(n2s, mrb);
}

/**********************************************************************
 * Document-class: Symbol
 *
 *  <code>Symbol</code> objects represent names and some strings
 *  inside the Ruby
 *  interpreter. They are generated using the <code>:name</code> and
 *  <code>:"string"</code> literals
 *  syntax, and by the various <code>to_sym</code> methods. The same
 *  <code>Symbol</code> object will be created for a given name or string
 *  for the duration of a program's execution, regardless of the context
 *  or meaning of that name. Thus if <code>Fred</code> is a constant in
 *  one context, a method in another, and a class in a third, the
 *  <code>Symbol</code> <code>:Fred</code> will be the same object in
 *  all three contexts.
 *
 *     module One
 *       class Fred
 *       end
 *       $f1 = :Fred
 *     end
 *     module Two
 *       Fred = 1
 *       $f2 = :Fred
 *     end
 *     def Fred()
 *     end
 *     $f3 = :Fred
 *     $f1.object_id   #=> 2514190
 *     $f2.object_id   #=> 2514190
 *     $f3.object_id   #=> 2514190
 *
 */


/* 15.2.11.3.1  */
/*
 *  call-seq:
 *     sym == obj   -> true or false
 *
 *  Equality---If <i>sym</i> and <i>obj</i> are exactly the same
 *  symbol, returns <code>true</code>.
 */

static $value
sym_equal($state *mrb, $value sym1)
{
  $value sym2;

  $get_args(mrb, "o", &sym2);

  return $bool_value($obj_equal(mrb, sym1, sym2));
}

/* 15.2.11.3.2  */
/* 15.2.11.3.3  */
/*
 *  call-seq:
 *     sym.id2name   -> string
 *     sym.to_s      -> string
 *
 *  Returns the name or string corresponding to <i>sym</i>.
 *
 *     :fred.id2name   #=> "fred"
 */
static $value
$sym_to_s($state *mrb, $value sym)
{
  $sym id = $symbol(sym);
  const char *p;
  $int len;

  p = $sym2name_len(mrb, id, &len);
  return $str_new_static(mrb, p, len);
}

/* 15.2.11.3.4  */
/*
 * call-seq:
 *   sym.to_sym   -> sym
 *   sym.intern   -> sym
 *
 * In general, <code>to_sym</code> returns the <code>Symbol</code> corresponding
 * to an object. As <i>sym</i> is already a symbol, <code>self</code> is returned
 * in this case.
 */

static $value
sym_to_sym($state *mrb, $value sym)
{
  return sym;
}

/* 15.2.11.3.5(x)  */
/*
 *  call-seq:
 *     sym.inspect    -> string
 *
 *  Returns the representation of <i>sym</i> as a symbol literal.
 *
 *     :fred.inspect   #=> ":fred"
 */

#if __STDC__
# define SIGN_EXTEND_CHAR(c) ((signed char)(c))
#else  /* not __STDC__ */
/* As in Harbison and Steele.  */
# define SIGN_EXTEND_CHAR(c) ((((unsigned char)(c)) ^ 128) - 128)
#endif
#define is_identchar(c) (SIGN_EXTEND_CHAR(c)!=-1&&(ISALNUM(c) || (c) == '_'))

static $bool
is_special_global_name(const char* m)
{
  switch (*m) {
    case '~': case '*': case '$': case '?': case '!': case '@':
    case '/': case '\\': case ';': case ',': case '.': case '=':
    case ':': case '<': case '>': case '\"':
    case '&': case '`': case '\'': case '+':
    case '0':
      ++m;
      break;
    case '-':
      ++m;
      if (is_identchar(*m)) m += 1;
      break;
    default:
      if (!ISDIGIT(*m)) return FALSE;
      do ++m; while (ISDIGIT(*m));
      break;
  }
  return !*m;
}

static $bool
symname_p(const char *name)
{
  const char *m = name;
  $bool localid = FALSE;

  if (!m) return FALSE;
  switch (*m) {
    case '\0':
      return FALSE;

    case '$':
      if (is_special_global_name(++m)) return TRUE;
      goto id;

    case '@':
      if (*++m == '@') ++m;
      goto id;

    case '<':
      switch (*++m) {
        case '<': ++m; break;
        case '=': if (*++m == '>') ++m; break;
        default: break;
      }
      break;

    case '>':
      switch (*++m) {
        case '>': case '=': ++m; break;
        default: break;
      }
      break;

    case '=':
      switch (*++m) {
        case '~': ++m; break;
        case '=': if (*++m == '=') ++m; break;
        default: return FALSE;
      }
      break;

    case '*':
      if (*++m == '*') ++m;
      break;
    case '!':
      switch (*++m) {
        case '=': case '~': ++m;
      }
      break;
    case '+': case '-':
      if (*++m == '@') ++m;
      break;
    case '|':
      if (*++m == '|') ++m;
      break;
    case '&':
      if (*++m == '&') ++m;
      break;

    case '^': case '/': case '%': case '~': case '`':
      ++m;
      break;

    case '[':
      if (*++m != ']') return FALSE;
      if (*++m == '=') ++m;
      break;

    default:
      localid = !ISUPPER(*m);
id:
      if (*m != '_' && !ISALPHA(*m)) return FALSE;
      while (is_identchar(*m)) m += 1;
      if (localid) {
        switch (*m) {
          case '!': case '?': case '=': ++m;
          default: break;
        }
      }
      break;
  }
  return *m ? FALSE : TRUE;
}

static $value
sym_inspect($state *mrb, $value sym)
{
  $value str;
  const char *name;
  $int len;
  $sym id = $symbol(sym);
  char *sp;

  name = $sym2name_len(mrb, id, &len);
  str = $str_new(mrb, 0, len+1);
  sp = RSTRING_PTR(str);
  RSTRING_PTR(str)[0] = ':';
  memcpy(sp+1, name, len);
  $assert_int_fit($int, len, size_t, SIZE_MAX);
  if (!symname_p(name) || strlen(name) != (size_t)len) {
    str = $str_dump(mrb, str);
    sp = RSTRING_PTR(str);
    sp[0] = ':';
    sp[1] = '"';
  }
  return str;
}

$API $value
$sym2str($state *mrb, $sym sym)
{
  $int len;
  const char *name = $sym2name_len(mrb, sym, &len);

  if (!name) return $undef_value(); /* can't happen */
  return $str_new_static(mrb, name, len);
}

$API const char*
$sym2name($state *mrb, $sym sym)
{
  $int len;
  const char *name = $sym2name_len(mrb, sym, &len);

  if (!name) return NULL;
  if (symname_p(name) && strlen(name) == (size_t)len) {
    return name;
  }
  else {
    $value str = $str_dump(mrb, $str_new_static(mrb, name, len));
    return RSTRING_PTR(str);
  }
}

#define lesser(a,b) (((a)>(b))?(b):(a))

static $value
sym_cmp($state *mrb, $value s1)
{
  $value s2;
  $sym sym1, sym2;

  $get_args(mrb, "o", &s2);
  if ($type(s2) != $TT_SYMBOL) return $nil_value();
  sym1 = $symbol(s1);
  sym2 = $symbol(s2);
  if (sym1 == sym2) return $fixnum_value(0);
  else {
    const char *p1, *p2;
    int retval;
    $int len, len1, len2;

    p1 = $sym2name_len(mrb, sym1, &len1);
    p2 = $sym2name_len(mrb, sym2, &len2);
    len = lesser(len1, len2);
    retval = memcmp(p1, p2, len);
    if (retval == 0) {
      if (len1 == len2) return $fixnum_value(0);
      if (len1 > len2)  return $fixnum_value(1);
      return $fixnum_value(-1);
    }
    if (retval > 0) return $fixnum_value(1);
    return $fixnum_value(-1);
  }
}

void
$init_symbol($state *mrb)
{
  struct RClass *sym;

  mrb->symbol_class = sym = $define_class(mrb, "Symbol", mrb->object_class);                 /* 15.2.11 */
  $SET_INSTANCE_TT(sym, $TT_SYMBOL);
  $undef_class_method(mrb,  sym, "new");

  $define_method(mrb, sym, "===",             sym_equal,      $ARGS_REQ(1));              /* 15.2.11.3.1  */
  $define_method(mrb, sym, "id2name",         $sym_to_s,   $ARGS_NONE());              /* 15.2.11.3.2  */
  $define_method(mrb, sym, "to_s",            $sym_to_s,   $ARGS_NONE());              /* 15.2.11.3.3  */
  $define_method(mrb, sym, "to_sym",          sym_to_sym,     $ARGS_NONE());              /* 15.2.11.3.4  */
  $define_method(mrb, sym, "inspect",         sym_inspect,    $ARGS_NONE());              /* 15.2.11.3.5(x)  */
  $define_method(mrb, sym, "<=>",             sym_cmp,        $ARGS_REQ(1));
}
