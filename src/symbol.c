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
  _bool lit : 1;
  uint16_t len;
  const char *name;
} symbol_name;

static inline khint_t
sym_hash_func(_state *mrb, _sym s)
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

KHASH_DECLARE(n2s, _sym, _sym, FALSE)
KHASH_DEFINE (n2s, _sym, _sym, FALSE, sym_hash_func, sym_hash_equal)
/* ------------------------------------------------------ */

static void
sym_validate_len(_state *mrb, size_t len)
{
  if (len >= RITE_LV_NULL_MARK) {
    _raise(mrb, E_ARGUMENT_ERROR, "symbol length too long");
  }
}

static _sym
sym_intern(_state *mrb, const char *name, size_t len, _bool lit)
{
  khash_t(n2s) *h = mrb->name2sym;
  symbol_name *sname = mrb->symtbl; /* symtbl[0] for working memory */
  khiter_t k;
  _sym sym;
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
    mrb->symtbl = (symbol_name*)_realloc(mrb, mrb->symtbl, sizeof(symbol_name)*(mrb->symcapa+1));
  }
  sname = &mrb->symtbl[sym];
  sname->len = (uint16_t)len;
  if (lit || _ro_data_p(name)) {
    sname->name = name;
    sname->lit = TRUE;
  }
  else {
    p = (char *)_malloc(mrb, len+1);
    memcpy(p, name, len);
    p[len] = 0;
    sname->name = (const char*)p;
    sname->lit = FALSE;
  }
  kh_put(n2s, mrb, h, sym);

  return sym;
}

MRB_API _sym
_intern(_state *mrb, const char *name, size_t len)
{
  return sym_intern(mrb, name, len, FALSE);
}

MRB_API _sym
_intern_static(_state *mrb, const char *name, size_t len)
{
  return sym_intern(mrb, name, len, TRUE);
}

MRB_API _sym
_intern_cstr(_state *mrb, const char *name)
{
  return _intern(mrb, name, strlen(name));
}

MRB_API _sym
_intern_str(_state *mrb, _value str)
{
  return _intern(mrb, RSTRING_PTR(str), RSTRING_LEN(str));
}

MRB_API _value
_check_intern(_state *mrb, const char *name, size_t len)
{
  khash_t(n2s) *h = mrb->name2sym;
  symbol_name *sname = mrb->symtbl;
  khiter_t k;

  sym_validate_len(mrb, len);
  sname->len = (uint16_t)len;
  sname->name = name;

  k = kh_get(n2s, mrb, h, 0);
  if (k != kh_end(h)) {
    return _symbol_value(kh_key(h, k));
  }
  return _nil_value();
}

MRB_API _value
_check_intern_cstr(_state *mrb, const char *name)
{
  return _check_intern(mrb, name, (_int)strlen(name));
}

MRB_API _value
_check_intern_str(_state *mrb, _value str)
{
  return _check_intern(mrb, RSTRING_PTR(str), RSTRING_LEN(str));
}

/* lenp must be a pointer to a size_t variable */
MRB_API const char*
_sym2name_len(_state *mrb, _sym sym, _int *lenp)
{
  if (sym == 0 || mrb->symidx < sym) {
    if (lenp) *lenp = 0;
    return NULL;
  }

  if (lenp) *lenp = mrb->symtbl[sym].len;
  return mrb->symtbl[sym].name;
}

void
_free_symtbl(_state *mrb)
{
  _sym i, lim;

  for (i=1, lim=mrb->symidx+1; i<lim; i++) {
    if (!mrb->symtbl[i].lit) {
      _free(mrb, (char*)mrb->symtbl[i].name);
    }
  }
  _free(mrb, mrb->symtbl);
  kh_destroy(n2s, mrb, mrb->name2sym);
}

void
_init_symtbl(_state *mrb)
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

static _value
sym_equal(_state *mrb, _value sym1)
{
  _value sym2;

  _get_args(mrb, "o", &sym2);

  return _bool_value(_obj_equal(mrb, sym1, sym2));
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
static _value
_sym_to_s(_state *mrb, _value sym)
{
  _sym id = _symbol(sym);
  const char *p;
  _int len;

  p = _sym2name_len(mrb, id, &len);
  return _str_new_static(mrb, p, len);
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

static _value
sym_to_sym(_state *mrb, _value sym)
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

static _bool
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

static _bool
symname_p(const char *name)
{
  const char *m = name;
  _bool localid = FALSE;

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

static _value
sym_inspect(_state *mrb, _value sym)
{
  _value str;
  const char *name;
  _int len;
  _sym id = _symbol(sym);
  char *sp;

  name = _sym2name_len(mrb, id, &len);
  str = _str_new(mrb, 0, len+1);
  sp = RSTRING_PTR(str);
  RSTRING_PTR(str)[0] = ':';
  memcpy(sp+1, name, len);
  _assert_int_fit(_int, len, size_t, SIZE_MAX);
  if (!symname_p(name) || strlen(name) != (size_t)len) {
    str = _str_dump(mrb, str);
    sp = RSTRING_PTR(str);
    sp[0] = ':';
    sp[1] = '"';
  }
  return str;
}

MRB_API _value
_sym2str(_state *mrb, _sym sym)
{
  _int len;
  const char *name = _sym2name_len(mrb, sym, &len);

  if (!name) return _undef_value(); /* can't happen */
  return _str_new_static(mrb, name, len);
}

MRB_API const char*
_sym2name(_state *mrb, _sym sym)
{
  _int len;
  const char *name = _sym2name_len(mrb, sym, &len);

  if (!name) return NULL;
  if (symname_p(name) && strlen(name) == (size_t)len) {
    return name;
  }
  else {
    _value str = _str_dump(mrb, _str_new_static(mrb, name, len));
    return RSTRING_PTR(str);
  }
}

#define lesser(a,b) (((a)>(b))?(b):(a))

static _value
sym_cmp(_state *mrb, _value s1)
{
  _value s2;
  _sym sym1, sym2;

  _get_args(mrb, "o", &s2);
  if (_type(s2) != MRB_TT_SYMBOL) return _nil_value();
  sym1 = _symbol(s1);
  sym2 = _symbol(s2);
  if (sym1 == sym2) return _fixnum_value(0);
  else {
    const char *p1, *p2;
    int retval;
    _int len, len1, len2;

    p1 = _sym2name_len(mrb, sym1, &len1);
    p2 = _sym2name_len(mrb, sym2, &len2);
    len = lesser(len1, len2);
    retval = memcmp(p1, p2, len);
    if (retval == 0) {
      if (len1 == len2) return _fixnum_value(0);
      if (len1 > len2)  return _fixnum_value(1);
      return _fixnum_value(-1);
    }
    if (retval > 0) return _fixnum_value(1);
    return _fixnum_value(-1);
  }
}

void
_init_symbol(_state *mrb)
{
  struct RClass *sym;

  mrb->symbol_class = sym = _define_class(mrb, "Symbol", mrb->object_class);                 /* 15.2.11 */
  MRB_SET_INSTANCE_TT(sym, MRB_TT_SYMBOL);
  _undef_class_method(mrb,  sym, "new");

  _define_method(mrb, sym, "===",             sym_equal,      MRB_ARGS_REQ(1));              /* 15.2.11.3.1  */
  _define_method(mrb, sym, "id2name",         _sym_to_s,   MRB_ARGS_NONE());              /* 15.2.11.3.2  */
  _define_method(mrb, sym, "to_s",            _sym_to_s,   MRB_ARGS_NONE());              /* 15.2.11.3.3  */
  _define_method(mrb, sym, "to_sym",          sym_to_sym,     MRB_ARGS_NONE());              /* 15.2.11.3.4  */
  _define_method(mrb, sym, "inspect",         sym_inspect,    MRB_ARGS_NONE());              /* 15.2.11.3.5(x)  */
  _define_method(mrb, sym, "<=>",             sym_cmp,        MRB_ARGS_REQ(1));
}
