/*
** string.c - String class
**
** See Copyright Notice in mruby.h
*/

#ifdef _MSC_VER
# define _CRT_NONSTDC_NO_DEPRECATE
#endif

#ifndef WITHOUT_FLOAT
#include <float.h>
#endif
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/range.h>
#include <mruby/string.h>
#include <mruby/re.h>

typedef struct shared_string {
  bool nofree : 1;
  int refcnt;
  char *ptr;
  int len;
} shared_string;

const char digitmap[] = "0123456789abcdefghijklmnopqrstuvwxyz";

#define obj_alloc_string(mrb) ((struct RString*)obj_alloc((mrb), TT_STRING, (mrb)->string_class))

static struct RString*
str_new_static(state *mrb, const char *p, size_t len)
{
  struct RString *s;

  if (len >= INT_MAX) {
    raise(mrb, E_ARGUMENT_ERROR, "string size too big");
  }
  s = obj_alloc_string(mrb);
  s->as.heap.len = (int)len;
  s->as.heap.aux.capa = 0;             /* nofree */
  s->as.heap.ptr = (char *)p;
  s->flags = STR_NOFREE;

  return s;
}

static struct RString*
str_new(state *mrb, const char *p, size_t len)
{
  struct RString *s;

  if (p && ro_data_p(p)) {
    return str_new_static(mrb, p, len);
  }
  s = obj_alloc_string(mrb);
  if (len <= RSTRING_EMBED_LEN_MAX) {
    RSTR_SET_EMBED_FLAG(s);
    RSTR_SET_EMBED_LEN(s, len);
    if (p) {
      memcpy(s->as.ary, p, len);
    }
  }
  else {
    if (len >= INT_MAX) {
      raise(mrb, E_ARGUMENT_ERROR, "string size too big");
    }
    s->as.heap.ptr = (char *)malloc(mrb, len+1);
    s->as.heap.len = (int)len;
    s->as.heap.aux.capa = (int)len;
    if (p) {
      memcpy(s->as.heap.ptr, p, len);
    }
  }
  RSTR_PTR(s)[len] = '\0';
  return s;
}

static inline void
str_with_class(state *mrb, struct RString *s, value obj)
{
  s->c = str_ptr(obj)->c;
}

static value
str_new_empty(state *mrb, value str)
{
  struct RString *s = str_new(mrb, 0, 0);

  str_with_class(mrb, s, str);
  return obj_value(s);
}

API value
str_new_capa(state *mrb, size_t capa)
{
  struct RString *s;

  s = obj_alloc_string(mrb);

  if (capa >= INT_MAX) {
    raise(mrb, E_ARGUMENT_ERROR, "string capacity size too big");
  }
  s->as.heap.len = 0;
  s->as.heap.aux.capa = (int)capa;
  s->as.heap.ptr = (char *)malloc(mrb, capa+1);
  RSTR_PTR(s)[0] = '\0';

  return obj_value(s);
}

#ifndef STR_BUF_MIN_SIZE
# define STR_BUF_MIN_SIZE 128
#endif

API value
str_buf_new(state *mrb, size_t capa)
{
  if (capa < STR_BUF_MIN_SIZE) {
    capa = STR_BUF_MIN_SIZE;
  }
  return str_new_capa(mrb, capa);
}

static void
resize_capa(state *mrb, struct RString *s, size_t capacity)
{
#if SIZE_MAX > INT_MAX
    assert(capacity < INT_MAX);
#endif
  if (RSTR_EMBED_P(s)) {
    if (RSTRING_EMBED_LEN_MAX < capacity) {
      char *const tmp = (char *)malloc(mrb, capacity+1);
      const int len = RSTR_EMBED_LEN(s);
      memcpy(tmp, s->as.ary, len);
      RSTR_UNSET_EMBED_FLAG(s);
      s->as.heap.ptr = tmp;
      s->as.heap.len = len;
      s->as.heap.aux.capa = (int)capacity;
    }
  }
  else {
    s->as.heap.ptr = (char*)realloc(mrb, RSTR_PTR(s), capacity+1);
    s->as.heap.aux.capa = (int)capacity;
  }
}

API value
str_new(state *mrb, const char *p, size_t len)
{
  return obj_value(str_new(mrb, p, len));
}

/*
 *  call-seq: (Caution! NULL string)
 *     String.new(str="")   => new_str
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */

API value
str_new_cstr(state *mrb, const char *p)
{
  struct RString *s;
  size_t len;

  if (p) {
    len = strlen(p);
  }
  else {
    len = 0;
  }

  s = str_new(mrb, p, len);

  return obj_value(s);
}

API value
str_new_static(state *mrb, const char *p, size_t len)
{
  struct RString *s = str_new_static(mrb, p, len);
  return obj_value(s);
}

static void
str_decref(state *mrb, shared_string *shared)
{
  shared->refcnt--;
  if (shared->refcnt == 0) {
    if (!shared->nofree) {
      free(mrb, shared->ptr);
    }
    free(mrb, shared);
  }
}

void
gc_free_str(state *mrb, struct RString *str)
{
  if (RSTR_EMBED_P(str))
    /* no code */;
  else if (RSTR_SHARED_P(str))
    str_decref(mrb, str->as.heap.aux.shared);
  else if (!RSTR_NOFREE_P(str) && !RSTR_FSHARED_P(str))
    free(mrb, str->as.heap.ptr);
}

#ifdef UTF8_STRING
static const char utf8len_codepage[256] =
{
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,1,1,1,1,1,1,1,1,1,1,1,
};

static int
utf8len(const char* p, const char* e)
{
  int len;
  int i;

  len = utf8len_codepage[(unsigned char)*p];
  if (p + len > e) return 1;
  for (i = 1; i < len; ++i)
    if ((p[i] & 0xc0) != 0x80)
      return 1;
  return len;
}

static int
utf8_strlen(value str, int len)
{
  int total = 0;
  char* p = RSTRING_PTR(str);
  char* e = p;
  if (RSTRING(str)->flags & STR_NO_UTF) {
    return RSTRING_LEN(str);
  }
  e += len < 0 ? RSTRING_LEN(str) : len;
  while (p<e) {
    p += utf8len(p, e);
    total++;
  }
  if (RSTRING_LEN(str) == total) {
    RSTRING(str)->flags |= STR_NO_UTF;
  }
  return total;
}

#define RSTRING_CHAR_LEN(s) utf8_strlen(s, -1)

/* map character index to byte offset index */
static int
chars2bytes(value s, int off, int idx)
{
  int i, b, n;
  const char *p = RSTRING_PTR(s) + off;
  const char *e = RSTRING_END(s);

  for (b=i=0; p<e && i<idx; i++) {
    n = utf8len(p, e);
    b += n;
    p += n;
  }
  return b;
}

/* map byte offset to character index */
static int
bytes2chars(char *p, int bi)
{
  int i, b, n;

  for (b=i=0; b<bi; i++) {
    n = utf8len_codepage[(unsigned char)*p];
    b += n;
    p += n;
  }
  if (b != bi) return -1;
  return i;
}

#define BYTES_ALIGN_CHECK(pos) if (pos < 0) return nil_value();
#else
#define RSTRING_CHAR_LEN(s) RSTRING_LEN(s)
#define chars2bytes(p, off, ci) (ci)
#define bytes2chars(p, bi) (bi)
#define BYTES_ALIGN_CHECK(pos)
#endif

static inline int
memsearch_qs(const unsigned char *xs, int m, const unsigned char *ys, int n)
{
  const unsigned char *x = xs, *xe = xs + m;
  const unsigned char *y = ys;
  int i;
  ptrdiff_t qstable[256];

  /* Preprocessing */
  for (i = 0; i < 256; ++i)
    qstable[i] = m + 1;
  for (; x < xe; ++x)
    qstable[*x] = xe - x;
  /* Searching */
  for (; y + m <= ys + n; y += *(qstable + y[m])) {
    if (*xs == *y && memcmp(xs, y, m) == 0)
      return (int)(y - ys);
  }
  return -1;
}

static int
memsearch(const void *x0, int m, const void *y0, int n)
{
  const unsigned char *x = (const unsigned char *)x0, *y = (const unsigned char *)y0;

  if (m > n) return -1;
  else if (m == n) {
    return memcmp(x0, y0, m) == 0 ? 0 : -1;
  }
  else if (m < 1) {
    return 0;
  }
  else if (m == 1) {
    const unsigned char *ys = (const unsigned char *)memchr(y, *x, n);

    if (ys)
      return (int)(ys - y);
    else
      return -1;
  }
  return memsearch_qs((const unsigned char *)x0, m, (const unsigned char *)y0, n);
}

static void
str_make_shared(state *mrb, struct RString *orig, struct RString *s)
{
  shared_string *shared;
  int len = RSTR_LEN(orig);

  assert(!RSTR_EMBED_P(orig));
  if (RSTR_SHARED_P(orig)) {
    shared = orig->as.heap.aux.shared;
    shared->refcnt++;
    s->as.heap.ptr = orig->as.heap.ptr;
    s->as.heap.len = len;
    s->as.heap.aux.shared = shared;
    RSTR_SET_SHARED_FLAG(s);
    RSTR_UNSET_EMBED_FLAG(s);
  }
  else if (RSTR_FSHARED_P(orig)) {
    struct RString *fs;

    fs = orig->as.heap.aux.fshared;
    s->as.heap.ptr = orig->as.heap.ptr;
    s->as.heap.len = len;
    s->as.heap.aux.fshared = fs;
    RSTR_SET_FSHARED_FLAG(s);
    RSTR_UNSET_EMBED_FLAG(s);
  }
  else if (FROZEN_P(orig) && !RSTR_POOL_P(orig)) {
    s->as.heap.ptr = orig->as.heap.ptr;
    s->as.heap.len = len;
    s->as.heap.aux.fshared = orig;
    RSTR_SET_FSHARED_FLAG(s);
    RSTR_UNSET_EMBED_FLAG(s);
  }
  else {
    shared = (shared_string *)malloc(mrb, sizeof(shared_string));
    shared->refcnt = 2;
    shared->nofree = !!RSTR_NOFREE_P(orig);
    if (!shared->nofree && orig->as.heap.aux.capa > orig->as.heap.len) {
      shared->ptr = (char *)realloc(mrb, orig->as.heap.ptr, len+1);
      orig->as.heap.ptr = shared->ptr;
    }
    else {
      shared->ptr = orig->as.heap.ptr;
    }
    orig->as.heap.aux.shared = shared;
    RSTR_SET_SHARED_FLAG(orig);
    shared->len = len;
    s->as.heap.aux.shared = shared;
    s->as.heap.ptr = shared->ptr;
    s->as.heap.len = len;
    RSTR_SET_SHARED_FLAG(s);
    RSTR_UNSET_EMBED_FLAG(s);
  }
}

static value
byte_subseq(state *mrb, value str, int beg, int len)
{
  struct RString *orig, *s;

  orig = str_ptr(str);
  if (RSTR_EMBED_P(orig) || RSTR_LEN(orig) == 0 || len <= RSTRING_EMBED_LEN_MAX) {
    s = str_new(mrb, RSTR_PTR(orig)+beg, len);
  }
  else {
    s = obj_alloc_string(mrb);
    str_make_shared(mrb, orig, s);
    s->as.heap.ptr += beg;
    s->as.heap.len = len;
  }
  return obj_value(s);
}
#ifdef UTF8_STRING
static inline value
str_subseq(state *mrb, value str, int beg, int len)
{
  beg = chars2bytes(str, 0, beg);
  len = chars2bytes(str, beg, len);

  return byte_subseq(mrb, str, beg, len);
}
#else
#define str_subseq(mrb, str, beg, len) byte_subseq(mrb, str, beg, len)
#endif

static value
str_substr(state *mrb, value str, int beg, int len)
{
  int clen = RSTRING_CHAR_LEN(str);

  if (len < 0) return nil_value();
  if (clen == 0) {
    len = 0;
  }
  else if (beg < 0) {
    beg = clen + beg;
  }
  if (beg > clen) return nil_value();
  if (beg < 0) {
    beg += clen;
    if (beg < 0) return nil_value();
  }
  if (len > clen - beg)
    len = clen - beg;
  if (len <= 0) {
    len = 0;
  }
  return str_subseq(mrb, str, beg, len);
}

API int
str_index(state *mrb, value str, const char *sptr, int slen, int offset)
{
  int pos;
  char *s;
  int len;

  len = RSTRING_LEN(str);
  if (offset < 0) {
    offset += len;
    if (offset < 0) return -1;
  }
  if (len - offset < slen) return -1;
  s = RSTRING_PTR(str);
  if (offset) {
    s += offset;
  }
  if (slen == 0) return offset;
  /* need proceed one character at a time */
  len = RSTRING_LEN(str) - offset;
  pos = memsearch(sptr, slen, s, len);
  if (pos < 0) return pos;
  return pos + offset;
}

static int
str_index_str(state *mrb, value str, value str2, int offset)
{
  const char *ptr;
  int len;

  ptr = RSTRING_PTR(str2);
  len = RSTRING_LEN(str2);

  return str_index(mrb, str, ptr, len, offset);
}

static void
check_frozen(state *mrb, struct RString *s)
{
  if (FROZEN_P(s)) {
    raise(mrb, E_FROZEN_ERROR, "can't modify frozen string");
  }
}

static value
str_replace(state *mrb, struct RString *s1, struct RString *s2)
{
  int len;

  check_frozen(mrb, s1);
  if (s1 == s2) return obj_value(s1);
  s1->flags &= ~STR_NO_UTF;
  s1->flags |= s2->flags&STR_NO_UTF;
  len = RSTR_LEN(s2);
  if (RSTR_SHARED_P(s1)) {
    str_decref(mrb, s1->as.heap.aux.shared);
    RSTR_UNSET_SHARED_FLAG(s1);
  }
  else if (!RSTR_EMBED_P(s1) && !RSTR_NOFREE_P(s1) && !RSTR_FSHARED_P(s1)
           && s1->as.heap.ptr) {
    free(mrb, s1->as.heap.ptr);
  }

  RSTR_UNSET_FSHARED_FLAG(s1);
  RSTR_UNSET_NOFREE_FLAG(s1);
  if (len <= RSTRING_EMBED_LEN_MAX) {
    RSTR_UNSET_SHARED_FLAG(s1);
    RSTR_UNSET_FSHARED_FLAG(s1);
    RSTR_SET_EMBED_FLAG(s1);
    memcpy(s1->as.ary, RSTR_PTR(s2), len);
    RSTR_SET_EMBED_LEN(s1, len);
  }
  else {
    str_make_shared(mrb, s2, s1);
  }

  return obj_value(s1);
}

static int
str_rindex(state *mrb, value str, value sub, int pos)
{
  char *s, *sbeg, *t;
  struct RString *ps = str_ptr(str);
  int len = RSTRING_LEN(sub);

  /* substring longer than string */
  if (RSTR_LEN(ps) < len) return -1;
  if (RSTR_LEN(ps) - pos < len) {
    pos = RSTR_LEN(ps) - len;
  }
  sbeg = RSTR_PTR(ps);
  s = RSTR_PTR(ps) + pos;
  t = RSTRING_PTR(sub);
  if (len) {
    while (sbeg <= s) {
      if (memcmp(s, t, len) == 0) {
        return (int)(s - RSTR_PTR(ps));
      }
      s--;
    }
    return -1;
  }
  else {
    return pos;
  }
}

API int
str_strlen(state *mrb, struct RString *s)
{
  int i, max = RSTR_LEN(s);
  char *p = RSTR_PTR(s);

  if (!p) return 0;
  for (i=0; i<max; i++) {
    if (p[i] == '\0') {
      raise(mrb, E_ARGUMENT_ERROR, "string contains null byte");
    }
  }
  return max;
}

#ifdef _WIN32
#include <windows.h>

char*
utf8_from_locale(const char *str, int len)
{
  wchar_t* wcsp;
  char* mbsp;
  int mbssize, wcssize;

  if (len == 0)
    return strdup("");
  if (len == -1)
    len = (int)strlen(str);
  wcssize = MultiByteToWideChar(GetACP(), 0, str, len,  NULL, 0);
  wcsp = (wchar_t*) malloc((wcssize + 1) * sizeof(wchar_t));
  if (!wcsp)
    return NULL;
  wcssize = MultiByteToWideChar(GetACP(), 0, str, len, wcsp, wcssize + 1);
  wcsp[wcssize] = 0;

  mbssize = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) wcsp, -1, NULL, 0, NULL, NULL);
  mbsp = (char*) malloc((mbssize + 1));
  if (!mbsp) {
    free(wcsp);
    return NULL;
  }
  mbssize = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) wcsp, -1, mbsp, mbssize, NULL, NULL);
  mbsp[mbssize] = 0;
  free(wcsp);
  return mbsp;
}

char*
locale_from_utf8(const char *utf8, int len)
{
  wchar_t* wcsp;
  char* mbsp;
  int mbssize, wcssize;

  if (len == 0)
    return strdup("");
  if (len == -1)
    len = (int)strlen(utf8);
  wcssize = MultiByteToWideChar(CP_UTF8, 0, utf8, len,  NULL, 0);
  wcsp = (wchar_t*) malloc((wcssize + 1) * sizeof(wchar_t));
  if (!wcsp)
    return NULL;
  wcssize = MultiByteToWideChar(CP_UTF8, 0, utf8, len, wcsp, wcssize + 1);
  wcsp[wcssize] = 0;
  mbssize = WideCharToMultiByte(GetACP(), 0, (LPCWSTR) wcsp, -1, NULL, 0, NULL, NULL);
  mbsp = (char*) malloc((mbssize + 1));
  if (!mbsp) {
    free(wcsp);
    return NULL;
  }
  mbssize = WideCharToMultiByte(GetACP(), 0, (LPCWSTR) wcsp, -1, mbsp, mbssize, NULL, NULL);
  mbsp[mbssize] = 0;
  free(wcsp);
  return mbsp;
}
#endif

API void
str_modify(state *mrb, struct RString *s)
{
  check_frozen(mrb, s);
  s->flags &= ~STR_NO_UTF;
  if (RSTR_SHARED_P(s)) {
    shared_string *shared = s->as.heap.aux.shared;

    if (shared->nofree == 0 && shared->refcnt == 1 && s->as.heap.ptr == shared->ptr) {
      s->as.heap.ptr = shared->ptr;
      s->as.heap.aux.capa = shared->len;
      RSTR_PTR(s)[s->as.heap.len] = '\0';
      free(mrb, shared);
    }
    else {
      char *ptr, *p;
      int len;

      p = RSTR_PTR(s);
      len = s->as.heap.len;
      if (len < RSTRING_EMBED_LEN_MAX) {
        RSTR_SET_EMBED_FLAG(s);
        RSTR_SET_EMBED_LEN(s, len);
        ptr = RSTR_PTR(s);
      }
      else {
        ptr = (char *)malloc(mrb, (size_t)len + 1);
        s->as.heap.ptr = ptr;
        s->as.heap.aux.capa = len;
      }
      if (p) {
        memcpy(ptr, p, len);
      }
      ptr[len] = '\0';
      str_decref(mrb, shared);
    }
    RSTR_UNSET_SHARED_FLAG(s);
    return;
  }
  if (RSTR_NOFREE_P(s) || RSTR_FSHARED_P(s)) {
    char *p = s->as.heap.ptr;
    int len = s->as.heap.len;

    RSTR_UNSET_FSHARED_FLAG(s);
    RSTR_UNSET_NOFREE_FLAG(s);
    RSTR_UNSET_FSHARED_FLAG(s);
    if (len < RSTRING_EMBED_LEN_MAX) {
      RSTR_SET_EMBED_FLAG(s);
      RSTR_SET_EMBED_LEN(s, len);
    }
    else {
      s->as.heap.ptr = (char *)malloc(mrb, (size_t)len+1);
      s->as.heap.aux.capa = len;
    }
    if (p) {
      memcpy(RSTR_PTR(s), p, len);
    }
    RSTR_PTR(s)[len] = '\0';
    return;
  }
}

API value
str_resize(state *mrb, value str, int len)
{
  int slen;
  struct RString *s = str_ptr(str);

  if (len < 0) {
    raise(mrb, E_ARGUMENT_ERROR, "negative (or overflowed) string size");
  }
  str_modify(mrb, s);
  slen = RSTR_LEN(s);
  if (len != slen) {
    if (slen < len || slen - len > 256) {
      resize_capa(mrb, s, len);
    }
    RSTR_SET_LEN(s, len);
    RSTR_PTR(s)[len] = '\0';   /* sentinel */
  }
  return str;
}

API char*
str_to_cstr(state *mrb, value str0)
{
  struct RString *s;

  if (!string_p(str0)) {
    raise(mrb, E_TYPE_ERROR, "expected String");
  }

  s = str_new(mrb, RSTRING_PTR(str0), RSTRING_LEN(str0));
  if ((strlen(RSTR_PTR(s)) ^ RSTR_LEN(s)) != 0) {
    raise(mrb, E_ARGUMENT_ERROR, "string contains null byte");
  }
  return RSTR_PTR(s);
}

/*
 *  call-seq: (Caution! String("abcd") change)
 *     String("abcdefg") = String("abcd") + String("efg")
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */
API void
str_concat(state *mrb, value self, value other)
{
  if (!string_p(other)) {
    other = str_to_str(mrb, other);
  }
  str_cat_str(mrb, self, other);
}

/*
 *  call-seq: (Caution! String("abcd") remain)
 *     String("abcdefg") = String("abcd") + String("efg")
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */
API value
str_plus(state *mrb, value a, value b)
{
  struct RString *s = str_ptr(a);
  struct RString *s2 = str_ptr(b);
  struct RString *t;

  t = str_new(mrb, 0, RSTR_LEN(s) + RSTR_LEN(s2));
  memcpy(RSTR_PTR(t), RSTR_PTR(s), RSTR_LEN(s));
  memcpy(RSTR_PTR(t) + RSTR_LEN(s), RSTR_PTR(s2), RSTR_LEN(s2));

  return obj_value(t);
}

/* 15.2.10.5.2  */

/*
 *  call-seq: (Caution! String("abcd") remain) for stack_argument
 *     String("abcdefg") = String("abcd") + String("efg")
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */
static value
str_plus_m(state *mrb, value self)
{
  value str;

  get_args(mrb, "S", &str);
  return str_plus(mrb, self, str);
}

/* 15.2.10.5.26 */
/* 15.2.10.5.33 */
/*
 *  call-seq:
 *     "abcd".size   => int
 *
 *  Returns the length of string.
 */
static value
str_size(state *mrb, value self)
{
  int len = RSTRING_CHAR_LEN(self);
  return fixnum_value(len);
}

static value
str_bytesize(state *mrb, value self)
{
  int len = RSTRING_LEN(self);
  return fixnum_value(len);
}

/* 15.2.10.5.1  */
/*
 *  call-seq:
 *     str * integer   => new_str
 *
 *  Copy---Returns a new <code>String</code> containing <i>integer</i> copies of
 *  the receiver.
 *
 *     "Ho! " * 3   #=> "Ho! Ho! Ho! "
 */
static value
str_times(state *mrb, value self)
{
  int n,len,times;
  struct RString *str2;
  char *p;

  get_args(mrb, "i", &times);
  if (times < 0) {
    raise(mrb, E_ARGUMENT_ERROR, "negative argument");
  }
  if (times && INT_MAX / times < RSTRING_LEN(self)) {
    raise(mrb, E_ARGUMENT_ERROR, "argument too big");
  }

  len = RSTRING_LEN(self)*times;
  str2 = str_new(mrb, 0, len);
  str_with_class(mrb, str2, self);
  p = RSTR_PTR(str2);
  if (len > 0) {
    n = RSTRING_LEN(self);
    memcpy(p, RSTRING_PTR(self), n);
    while (n <= len/2) {
      memcpy(p + n, p, n);
      n *= 2;
    }
    memcpy(p + n, p, len-n);
  }
  p[RSTR_LEN(str2)] = '\0';

  return obj_value(str2);
}
/* -------------------------------------------------------------- */

#define lesser(a,b) (((a)>(b))?(b):(a))

/* ---------------------------*/
/*
 *  call-seq:
 *     value str1 <=> value str2   => int
 *                     >  1
 *                     =  0
 *                     <  -1
 */
API int
str_cmp(state *mrb, value str1, value str2)
{
  int len;
  int retval;
  struct RString *s1 = str_ptr(str1);
  struct RString *s2 = str_ptr(str2);

  len = lesser(RSTR_LEN(s1), RSTR_LEN(s2));
  retval = memcmp(RSTR_PTR(s1), RSTR_PTR(s2), len);
  if (retval == 0) {
    if (RSTR_LEN(s1) == RSTR_LEN(s2)) return 0;
    if (RSTR_LEN(s1) > RSTR_LEN(s2))  return 1;
    return -1;
  }
  if (retval > 0) return 1;
  return -1;
}

/* 15.2.10.5.3  */

/*
 *  call-seq:
 *     str <=> other_str   => -1, 0, +1
 *
 *  Comparison---Returns -1 if <i>other_str</i> is less than, 0 if
 *  <i>other_str</i> is equal to, and +1 if <i>other_str</i> is greater than
 *  <i>str</i>. If the strings are of different lengths, and the strings are
 *  equal when compared up to the shortest length, then the longer string is
 *  considered greater than the shorter one. If the variable <code>$=</code> is
 *  <code>false</code>, the comparison is based on comparing the binary values
 *  of each character in the string. In older versions of Ruby, setting
 *  <code>$=</code> allowed case-insensitive comparisons; this is now deprecated
 *  in favor of using <code>String#casecmp</code>.
 *
 *  <code><=></code> is the basis for the methods <code><</code>,
 *  <code><=</code>, <code>></code>, <code>>=</code>, and <code>between?</code>,
 *  included from module <code>Comparable</code>.  The method
 *  <code>String#==</code> does not use <code>Comparable#==</code>.
 *
 *     "abcdef" <=> "abcde"     #=> 1
 *     "abcdef" <=> "abcdef"    #=> 0
 *     "abcdef" <=> "abcdefg"   #=> -1
 *     "abcdef" <=> "ABCDEF"    #=> 1
 */
static value
str_cmp_m(state *mrb, value str1)
{
  value str2;
  int result;

  get_args(mrb, "o", &str2);
  if (!string_p(str2)) {
    if (!respond_to(mrb, str2, intern_lit(mrb, "to_s"))) {
      return nil_value();
    }
    else if (!respond_to(mrb, str2, intern_lit(mrb, "<=>"))) {
      return nil_value();
    }
    else {
      value tmp = funcall(mrb, str2, "<=>", 1, str1);

      if (nil_p(tmp)) return nil_value();
      if (!fixnum_p(tmp)) {
        return funcall(mrb, fixnum_value(0), "-", 1, tmp);
      }
      result = -fixnum(tmp);
    }
  }
  else {
    result = str_cmp(mrb, str1, str2);
  }
  return fixnum_value(result);
}

static bool
str_eql(state *mrb, const value str1, const value str2)
{
  const int len = RSTRING_LEN(str1);

  if (len != RSTRING_LEN(str2)) return FALSE;
  if (memcmp(RSTRING_PTR(str1), RSTRING_PTR(str2), (size_t)len) == 0)
    return TRUE;
  return FALSE;
}

API bool
str_equal(state *mrb, value str1, value str2)
{
  if (immediate_p(str2)) return FALSE;
  if (!string_p(str2)) {
    if (nil_p(str2)) return FALSE;
    if (!respond_to(mrb, str2, intern_lit(mrb, "to_str"))) {
      return FALSE;
    }
    str2 = funcall(mrb, str2, "to_str", 0);
    return equal(mrb, str2, str1);
  }
  return str_eql(mrb, str1, str2);
}

/* 15.2.10.5.4  */
/*
 *  call-seq:
 *     str == obj   => true or false
 *
 *  Equality---
 *  If <i>obj</i> is not a <code>String</code>, returns <code>false</code>.
 *  Otherwise, returns <code>false</code> or <code>true</code>
 *
 *   caution:if <i>str</i> <code><=></code> <i>obj</i> returns zero.
 */
static value
str_equal_m(state *mrb, value str1)
{
  value str2;

  get_args(mrb, "o", &str2);

  return bool_value(str_equal(mrb, str1, str2));
}
/* ---------------------------------- */
API value
str_to_str(state *mrb, value str)
{
  value s;

  if (!string_p(str)) {
    s = check_convert_type(mrb, str, TT_STRING, "String", "to_str");
    if (nil_p(s)) {
      s = convert_type(mrb, str, TT_STRING, "String", "to_s");
    }
    return s;
  }
  return str;
}

API const char*
string_value_ptr(state *mrb, value ptr)
{
  value str = str_to_str(mrb, ptr);
  return RSTRING_PTR(str);
}

API int
string_value_len(state *mrb, value ptr)
{
  value str = str_to_str(mrb, ptr);
  return RSTRING_LEN(str);
}

void
noregexp(state *mrb, value self)
{
  raise(mrb, E_NOTIMP_ERROR, "Regexp class not implemented");
}

void
regexp_check(state *mrb, value obj)
{
  if (regexp_p(mrb, obj)) {
    noregexp(mrb, obj);
  }
}

API value
str_dup(state *mrb, value str)
{
  struct RString *s = str_ptr(str);
  struct RString *dup = str_new(mrb, 0, 0);

  str_with_class(mrb, dup, str);
  return str_replace(mrb, dup, s);
}

static value
str_aref(state *mrb, value str, value indx)
{
  int idx;

  regexp_check(mrb, indx);
  switch (type(indx)) {
    case TT_FIXNUM:
      idx = fixnum(indx);

num_index:
      str = str_substr(mrb, str, idx, 1);
      if (!nil_p(str) && RSTRING_LEN(str) == 0) return nil_value();
      return str;

    case TT_STRING:
      if (str_index_str(mrb, str, indx, 0) != -1)
        return str_dup(mrb, indx);
      return nil_value();

    case TT_RANGE:
      goto range_arg;

    default:
      indx = Integer(mrb, indx);
      if (nil_p(indx)) {
      range_arg:
        {
          int beg, len;

          len = RSTRING_CHAR_LEN(str);
          switch (range_beg_len(mrb, indx, &beg, &len, len, TRUE)) {
          case 1:
            return str_subseq(mrb, str, beg, len);
          case 2:
            return nil_value();
          default:
            break;
          }
        }
        raise(mrb, E_TYPE_ERROR, "can't convert to Fixnum");
      }
      idx = fixnum(indx);
      goto num_index;
  }
  return nil_value();    /* not reached */
}

/* 15.2.10.5.6  */
/* 15.2.10.5.34 */
/*
 *  call-seq:
 *     str[fixnum]                 => fixnum or nil
 *     str[fixnum, fixnum]         => new_str or nil
 *     str[range]                  => new_str or nil
 *     str[regexp]                 => new_str or nil
 *     str[regexp, fixnum]         => new_str or nil
 *     str[other_str]              => new_str or nil
 *     str.slice(fixnum)           => fixnum or nil
 *     str.slice(fixnum, fixnum)   => new_str or nil
 *     str.slice(range)            => new_str or nil
 *     str.slice(other_str)        => new_str or nil
 *
 *  Element Reference---If passed a single <code>Fixnum</code>, returns the code
 *  of the character at that position. If passed two <code>Fixnum</code>
 *  objects, returns a substring starting at the offset given by the first, and
 *  a length given by the second. If given a range, a substring containing
 *  characters at offsets given by the range is returned. In all three cases, if
 *  an offset is negative, it is counted from the end of <i>str</i>. Returns
 *  <code>nil</code> if the initial offset falls outside the string, the length
 *  is negative, or the beginning of the range is greater than the end.
 *
 *  If a <code>String</code> is given, that string is returned if it occurs in
 *  <i>str</i>. In both cases, <code>nil</code> is returned if there is no
 *  match.
 *
 *     a = "hello there"
 *     a[1]                   #=> 101(1.8.7) "e"(1.9.2)
 *     a[1.1]                 #=>            "e"(1.9.2)
 *     a[1,3]                 #=> "ell"
 *     a[1..3]                #=> "ell"
 *     a[-3,2]                #=> "er"
 *     a[-4..-2]              #=> "her"
 *     a[12..-1]              #=> nil
 *     a[-2..-4]              #=> ""
 *     a["lo"]                #=> "lo"
 *     a["bye"]               #=> nil
 */
static value
str_aref_m(state *mrb, value str)
{
  value a1, a2;
  int argc;

  argc = get_args(mrb, "o|o", &a1, &a2);
  if (argc == 2) {
    int n1, n2;

    regexp_check(mrb, a1);
    get_args(mrb, "ii", &n1, &n2);
    return str_substr(mrb, str, n1, n2);
  }
  if (argc != 1) {
    raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%S for 1)", fixnum_value(argc));
  }
  return str_aref(mrb, str, a1);
}

/* 15.2.10.5.8  */
/*
 *  call-seq:
 *     str.capitalize!   => str or nil
 *
 *  Modifies <i>str</i> by converting the first character to uppercase and the
 *  remainder to lowercase. Returns <code>nil</code> if no changes are made.
 *
 *     a = "hello"
 *     a.capitalize!   #=> "Hello"
 *     a               #=> "Hello"
 *     a.capitalize!   #=> nil
 */
static value
str_capitalize_bang(state *mrb, value str)
{
  char *p, *pend;
  bool modify = FALSE;
  struct RString *s = str_ptr(str);

  str_modify(mrb, s);
  if (RSTR_LEN(s) == 0 || !RSTR_PTR(s)) return nil_value();
  p = RSTR_PTR(s); pend = RSTR_PTR(s) + RSTR_LEN(s);
  if (ISLOWER(*p)) {
    *p = TOUPPER(*p);
    modify = TRUE;
  }
  while (++p < pend) {
    if (ISUPPER(*p)) {
      *p = TOLOWER(*p);
      modify = TRUE;
    }
  }
  if (modify) return str;
  return nil_value();
}

/* 15.2.10.5.7  */
/*
 *  call-seq:
 *     str.capitalize   => new_str
 *
 *  Returns a copy of <i>str</i> with the first character converted to uppercase
 *  and the remainder to lowercase.
 *
 *     "hello".capitalize    #=> "Hello"
 *     "HELLO".capitalize    #=> "Hello"
 *     "123ABC".capitalize   #=> "123abc"
 */
static value
str_capitalize(state *mrb, value self)
{
  value str;

  str = str_dup(mrb, self);
  str_capitalize_bang(mrb, str);
  return str;
}

/* 15.2.10.5.10  */
/*
 *  call-seq:
 *     str.chomp!(separator="\n")   => str or nil
 *
 *  Modifies <i>str</i> in place as described for <code>String#chomp</code>,
 *  returning <i>str</i>, or <code>nil</code> if no modifications were made.
 */
static value
str_chomp_bang(state *mrb, value str)
{
  value rs;
  int newline;
  char *p, *pp;
  int rslen;
  int len;
  int argc;
  struct RString *s = str_ptr(str);

  argc = get_args(mrb, "|S", &rs);
  str_modify(mrb, s);
  len = RSTR_LEN(s);
  if (argc == 0) {
    if (len == 0) return nil_value();
  smart_chomp:
    if (RSTR_PTR(s)[len-1] == '\n') {
      RSTR_SET_LEN(s, RSTR_LEN(s) - 1);
      if (RSTR_LEN(s) > 0 &&
          RSTR_PTR(s)[RSTR_LEN(s)-1] == '\r') {
        RSTR_SET_LEN(s, RSTR_LEN(s) - 1);
      }
    }
    else if (RSTR_PTR(s)[len-1] == '\r') {
      RSTR_SET_LEN(s, RSTR_LEN(s) - 1);
    }
    else {
      return nil_value();
    }
    RSTR_PTR(s)[RSTR_LEN(s)] = '\0';
    return str;
  }

  if (len == 0 || nil_p(rs)) return nil_value();
  p = RSTR_PTR(s);
  rslen = RSTRING_LEN(rs);
  if (rslen == 0) {
    while (len>0 && p[len-1] == '\n') {
      len--;
      if (len>0 && p[len-1] == '\r')
        len--;
    }
    if (len < RSTR_LEN(s)) {
      RSTR_SET_LEN(s, len);
      p[len] = '\0';
      return str;
    }
    return nil_value();
  }
  if (rslen > len) return nil_value();
  newline = RSTRING_PTR(rs)[rslen-1];
  if (rslen == 1 && newline == '\n')
    newline = RSTRING_PTR(rs)[rslen-1];
  if (rslen == 1 && newline == '\n')
    goto smart_chomp;

  pp = p + len - rslen;
  if (p[len-1] == newline &&
     (rslen <= 1 ||
     memcmp(RSTRING_PTR(rs), pp, rslen) == 0)) {
    RSTR_SET_LEN(s, len - rslen);
    p[RSTR_LEN(s)] = '\0';
    return str;
  }
  return nil_value();
}

/* 15.2.10.5.9  */
/*
 *  call-seq:
 *     str.chomp(separator="\n")   => new_str
 *
 *  Returns a new <code>String</code> with the given record separator removed
 *  from the end of <i>str</i> (if present). If <code>$/</code> has not been
 *  changed from the default Ruby record separator, then <code>chomp</code> also
 *  removes carriage return characters (that is it will remove <code>\n</code>,
 *  <code>\r</code>, and <code>\r\n</code>).
 *
 *     "hello".chomp            #=> "hello"
 *     "hello\n".chomp          #=> "hello"
 *     "hello\r\n".chomp        #=> "hello"
 *     "hello\n\r".chomp        #=> "hello\n"
 *     "hello\r".chomp          #=> "hello"
 *     "hello \n there".chomp   #=> "hello \n there"
 *     "hello".chomp("llo")     #=> "he"
 */
static value
str_chomp(state *mrb, value self)
{
  value str;

  str = str_dup(mrb, self);
  str_chomp_bang(mrb, str);
  return str;
}

/* 15.2.10.5.12 */
/*
 *  call-seq:
 *     str.chop!   => str or nil
 *
 *  Processes <i>str</i> as for <code>String#chop</code>, returning <i>str</i>,
 *  or <code>nil</code> if <i>str</i> is the empty string.  See also
 *  <code>String#chomp!</code>.
 */
static value
str_chop_bang(state *mrb, value str)
{
  struct RString *s = str_ptr(str);

  str_modify(mrb, s);
  if (RSTR_LEN(s) > 0) {
    int len;
#ifdef UTF8_STRING
    const char* t = RSTR_PTR(s), *p = t;
    const char* e = p + RSTR_LEN(s);
    while (p<e) {
      int clen = utf8len(p, e);
      if (p + clen>=e) break;
      p += clen;
    }
    len = p - t;
#else
    len = RSTR_LEN(s) - 1;
#endif
    if (RSTR_PTR(s)[len] == '\n') {
      if (len > 0 &&
          RSTR_PTR(s)[len-1] == '\r') {
        len--;
      }
    }
    RSTR_SET_LEN(s, len);
    RSTR_PTR(s)[len] = '\0';
    return str;
  }
  return nil_value();
}

/* 15.2.10.5.11 */
/*
 *  call-seq:
 *     str.chop   => new_str
 *
 *  Returns a new <code>String</code> with the last character removed.  If the
 *  string ends with <code>\r\n</code>, both characters are removed. Applying
 *  <code>chop</code> to an empty string returns an empty
 *  string. <code>String#chomp</code> is often a safer alternative, as it leaves
 *  the string unchanged if it doesn't end in a record separator.
 *
 *     "string\r\n".chop   #=> "string"
 *     "string\n\r".chop   #=> "string\n"
 *     "string\n".chop     #=> "string"
 *     "string".chop       #=> "strin"
 *     "x".chop            #=> ""
 */
static value
str_chop(state *mrb, value self)
{
  value str;
  str = str_dup(mrb, self);
  str_chop_bang(mrb, str);
  return str;
}

/* 15.2.10.5.14 */
/*
 *  call-seq:
 *     str.downcase!   => str or nil
 *
 *  Downcases the contents of <i>str</i>, returning <code>nil</code> if no
 *  changes were made.
 */
static value
str_downcase_bang(state *mrb, value str)
{
  char *p, *pend;
  bool modify = FALSE;
  struct RString *s = str_ptr(str);

  str_modify(mrb, s);
  p = RSTR_PTR(s);
  pend = RSTR_PTR(s) + RSTR_LEN(s);
  while (p < pend) {
    if (ISUPPER(*p)) {
      *p = TOLOWER(*p);
      modify = TRUE;
    }
    p++;
  }

  if (modify) return str;
  return nil_value();
}

/* 15.2.10.5.13 */
/*
 *  call-seq:
 *     str.downcase   => new_str
 *
 *  Returns a copy of <i>str</i> with all uppercase letters replaced with their
 *  lowercase counterparts. The operation is locale insensitive---only
 *  characters 'A' to 'Z' are affected.
 *
 *     "hEllO".downcase   #=> "hello"
 */
static value
str_downcase(state *mrb, value self)
{
  value str;

  str = str_dup(mrb, self);
  str_downcase_bang(mrb, str);
  return str;
}

/* 15.2.10.5.16 */
/*
 *  call-seq:
 *     str.empty?   => true or false
 *
 *  Returns <code>true</code> if <i>str</i> has a length of zero.
 *
 *     "hello".empty?   #=> false
 *     "".empty?        #=> true
 */
static value
str_empty_p(state *mrb, value self)
{
  struct RString *s = str_ptr(self);

  return bool_value(RSTR_LEN(s) == 0);
}

/* 15.2.10.5.17 */
/*
 * call-seq:
 *   str.eql?(other)   => true or false
 *
 * Two strings are equal if the have the same length and content.
 */
static value
str_eql(state *mrb, value self)
{
  value str2;
  bool eql_p;

  get_args(mrb, "o", &str2);
  eql_p = (type(str2) == TT_STRING) && str_eql(mrb, self, str2);

  return bool_value(eql_p);
}

API value
str_substr(state *mrb, value str, int beg, int len)
{
  return str_substr(mrb, str, beg, len);
}

uint32_t
str_hash(state *mrb, value str)
{
  /* 1-8-7 */
  struct RString *s = str_ptr(str);
  int len = RSTR_LEN(s);
  char *p = RSTR_PTR(s);
  uint64_t key = 0;

  while (len--) {
    key = key*65599 + *p;
    p++;
  }
  return (uint32_t)(key + (key>>5));
}

/* 15.2.10.5.20 */
/*
 * call-seq:
 *    str.hash   => fixnum
 *
 * Return a hash based on the string's length and content.
 */
static value
str_hash_m(state *mrb, value self)
{
  int key = str_hash(mrb, self);
  return fixnum_value(key);
}

/* 15.2.10.5.21 */
/*
 *  call-seq:
 *     str.include? other_str   => true or false
 *     str.include? fixnum      => true or false
 *
 *  Returns <code>true</code> if <i>str</i> contains the given string or
 *  character.
 *
 *     "hello".include? "lo"   #=> true
 *     "hello".include? "ol"   #=> false
 *     "hello".include? ?h     #=> true
 */
static value
str_include(state *mrb, value self)
{
  value str2;

  get_args(mrb, "S", &str2);
  if (str_index_str(mrb, self, str2, 0) < 0)
    return bool_value(FALSE);
  return bool_value(TRUE);
}

/* 15.2.10.5.22 */
/*
 *  call-seq:
 *     str.index(substring [, offset])   => fixnum or nil
 *     str.index(fixnum [, offset])      => fixnum or nil
 *     str.index(regexp [, offset])      => fixnum or nil
 *
 *  Returns the index of the first occurrence of the given
 *  <i>substring</i>,
 *  character (<i>fixnum</i>), or pattern (<i>regexp</i>) in <i>str</i>.
 *  Returns
 *  <code>nil</code> if not found.
 *  If the second parameter is present, it
 *  specifies the position in the string to begin the search.
 *
 *     "hello".index('e')             #=> 1
 *     "hello".index('lo')            #=> 3
 *     "hello".index('a')             #=> nil
 *     "hello".index(101)             #=> 1(101=0x65='e')
 *     "hello".index(/[aeiou]/, -3)   #=> 4
 */
static value
str_index_m(state *mrb, value str)
{
  value *argv;
  int argc;
  value sub;
  int pos, clen;

  get_args(mrb, "*!", &argv, &argc);
  if (argc == 2) {
    get_args(mrb, "oi", &sub, &pos);
  }
  else {
    pos = 0;
    if (argc > 0)
      sub = argv[0];
    else
      sub = nil_value();
  }
  regexp_check(mrb, sub);
  clen = RSTRING_CHAR_LEN(str);
  if (pos < 0) {
    pos += clen;
    if (pos < 0) {
      return nil_value();
    }
  }
  if (pos > clen) return nil_value();
  pos = chars2bytes(str, 0, pos);

  switch (type(sub)) {
    default: {
      value tmp;

      tmp = check_string_type(mrb, sub);
      if (nil_p(tmp)) {
        raisef(mrb, E_TYPE_ERROR, "type mismatch: %S given", sub);
      }
      sub = tmp;
    }
    /* fall through */
    case TT_STRING:
      pos = str_index_str(mrb, str, sub, pos);
      break;
  }

  if (pos == -1) return nil_value();
  pos = bytes2chars(RSTRING_PTR(str), pos);
  BYTES_ALIGN_CHECK(pos);
  return fixnum_value(pos);
}

#define STR_REPLACE_SHARED_MIN 10

/* 15.2.10.5.24 */
/* 15.2.10.5.28 */
/*
 *  call-seq:
 *     str.replace(other_str)   => str
 *
 *     s = "hello"         #=> "hello"
 *     s.replace "world"   #=> "world"
 */
static value
str_replace(state *mrb, value str)
{
  value str2;

  get_args(mrb, "S", &str2);
  return str_replace(mrb, str_ptr(str), str_ptr(str2));
}

/* 15.2.10.5.23 */
/*
 *  call-seq:
 *     String.new(str="")   => new_str
 *
 *  Returns a new string object containing a copy of <i>str</i>.
 */
static value
str_init(state *mrb, value self)
{
  value str2;

  if (get_args(mrb, "|S", &str2) == 0) {
    struct RString *s = str_new(mrb, 0, 0);
    str2 = obj_value(s);
  }
  str_replace(mrb, str_ptr(self), str_ptr(str2));
  return self;
}

/* 15.2.10.5.25 */
/* 15.2.10.5.41 */
/*
 *  call-seq:
 *     str.intern   => symbol
 *     str.to_sym   => symbol
 *
 *  Returns the <code>Symbol</code> corresponding to <i>str</i>, creating the
 *  symbol if it did not previously exist. See <code>Symbol#id2name</code>.
 *
 *     "Koala".intern         #=> :Koala
 *     s = 'cat'.to_sym       #=> :cat
 *     s == :cat              #=> true
 *     s = '@cat'.to_sym      #=> :@cat
 *     s == :@cat             #=> true
 *
 *  This can also be used to create symbols that cannot be represented using the
 *  <code>:xxx</code> notation.
 *
 *     'cat and dog'.to_sym   #=> :"cat and dog"
 */
API value
str_intern(state *mrb, value self)
{
  return symbol_value(intern_str(mrb, self));
}
/* ---------------------------------- */
API value
obj_as_string(state *mrb, value obj)
{
  value str;

  if (string_p(obj)) {
    return obj;
  }
  str = funcall(mrb, obj, "to_s", 0);
  if (!string_p(str))
    return any_to_s(mrb, obj);
  return str;
}

API value
ptr_to_str(state *mrb, void *p)
{
  struct RString *p_str;
  char *p1;
  char *p2;
  uintptr_t n = (uintptr_t)p;

  p_str = str_new(mrb, NULL, 2 + sizeof(uintptr_t) * CHAR_BIT / 4);
  p1 = RSTR_PTR(p_str);
  *p1++ = '0';
  *p1++ = 'x';
  p2 = p1;

  do {
    *p2++ = digitmap[n % 16];
    n /= 16;
  } while (n > 0);
  *p2 = '\0';
  RSTR_SET_LEN(p_str, (int)(p2 - RSTR_PTR(p_str)));

  while (p1 < p2) {
    const char  c = *p1;
    *p1++ = *--p2;
    *p2 = c;
  }

  return obj_value(p_str);
}

API value
string_type(state *mrb, value str)
{
  return convert_type(mrb, str, TT_STRING, "String", "to_str");
}

API value
check_string_type(state *mrb, value str)
{
  return check_convert_type(mrb, str, TT_STRING, "String", "to_str");
}

/* 15.2.10.5.30 */
/*
 *  call-seq:
 *     str.reverse!   => str
 *
 *  Reverses <i>str</i> in place.
 */
static value
str_reverse_bang(state *mrb, value str)
{
#ifdef UTF8_STRING
  int utf8_len = RSTRING_CHAR_LEN(str);
  int len = RSTRING_LEN(str);

  if (utf8_len == len) goto bytes;
  if (utf8_len > 1) {
    char *buf;
    char *p, *e, *r;

    str_modify(mrb, str_ptr(str));
    len = RSTRING_LEN(str);
    buf = (char*)malloc(mrb, (size_t)len);
    p = buf;
    e = buf + len;

    memcpy(buf, RSTRING_PTR(str), len);
    r = RSTRING_PTR(str) + len;

    while (p<e) {
      int clen = utf8len(p, e);
      r -= clen;
      memcpy(r, p, clen);
      p += clen;
    }
    free(mrb, buf);
  }
  return str;

 bytes:
#endif
  {
    struct RString *s = str_ptr(str);
    char *p, *e;
    char c;

    str_modify(mrb, s);
    if (RSTR_LEN(s) > 1) {
      p = RSTR_PTR(s);
      e = p + RSTR_LEN(s) - 1;
      while (p < e) {
      c = *p;
      *p++ = *e;
      *e-- = c;
      }
    }
    return str;
  }
}

/* ---------------------------------- */
/* 15.2.10.5.29 */
/*
 *  call-seq:
 *     str.reverse   => new_str
 *
 *  Returns a new string with the characters from <i>str</i> in reverse order.
 *
 *     "stressed".reverse   #=> "desserts"
 */
static value
str_reverse(state *mrb, value str)
{
  value str2 = str_dup(mrb, str);
  str_reverse_bang(mrb, str2);
  return str2;
}

/* 15.2.10.5.31 */
/*
 *  call-seq:
 *     str.rindex(substring [, fixnum])   => fixnum or nil
 *     str.rindex(fixnum [, fixnum])   => fixnum or nil
 *     str.rindex(regexp [, fixnum])   => fixnum or nil
 *
 *  Returns the index of the last occurrence of the given <i>substring</i>,
 *  character (<i>fixnum</i>), or pattern (<i>regexp</i>) in <i>str</i>. Returns
 *  <code>nil</code> if not found. If the second parameter is present, it
 *  specifies the position in the string to end the search---characters beyond
 *  this point will not be considered.
 *
 *     "hello".rindex('e')             #=> 1
 *     "hello".rindex('l')             #=> 3
 *     "hello".rindex('a')             #=> nil
 *     "hello".rindex(101)             #=> 1
 *     "hello".rindex(/[aeiou]/, -2)   #=> 1
 */
static value
str_rindex(state *mrb, value str)
{
  value *argv;
  int argc;
  value sub;
  int pos, len = RSTRING_CHAR_LEN(str);

  get_args(mrb, "*!", &argv, &argc);
  if (argc == 2) {
    get_args(mrb, "oi", &sub, &pos);
    if (pos < 0) {
      pos += len;
      if (pos < 0) {
        regexp_check(mrb, sub);
        return nil_value();
      }
    }
    if (pos > len) pos = len;
  }
  else {
    pos = len;
    if (argc > 0)
      sub = argv[0];
    else
      sub = nil_value();
  }
  pos = chars2bytes(str, 0, pos);
  regexp_check(mrb, sub);

  switch (type(sub)) {
    default: {
      value tmp;

      tmp = check_string_type(mrb, sub);
      if (nil_p(tmp)) {
        raisef(mrb, E_TYPE_ERROR, "type mismatch: %S given", sub);
      }
      sub = tmp;
    }
     /* fall through */
    case TT_STRING:
      pos = str_rindex(mrb, str, sub, pos);
      if (pos >= 0) {
        pos = bytes2chars(RSTRING_PTR(str), pos);
        BYTES_ALIGN_CHECK(pos);
        return fixnum_value(pos);
      }
      break;

  } /* end of switch (TYPE(sub)) */
  return nil_value();
}

/* 15.2.10.5.35 */

/*
 *  call-seq:
 *     str.split(pattern="\n", [limit])   => anArray
 *
 *  Divides <i>str</i> into substrings based on a delimiter, returning an array
 *  of these substrings.
 *
 *  If <i>pattern</i> is a <code>String</code>, then its contents are used as
 *  the delimiter when splitting <i>str</i>. If <i>pattern</i> is a single
 *  space, <i>str</i> is split on whitespace, with leading whitespace and runs
 *  of contiguous whitespace characters ignored.
 *
 *  If <i>pattern</i> is a <code>Regexp</code>, <i>str</i> is divided where the
 *  pattern matches. Whenever the pattern matches a zero-length string,
 *  <i>str</i> is split into individual characters.
 *
 *  If <i>pattern</i> is omitted, the value of <code>$;</code> is used.  If
 *  <code>$;</code> is <code>nil</code> (which is the default), <i>str</i> is
 *  split on whitespace as if ' ' were specified.
 *
 *  If the <i>limit</i> parameter is omitted, trailing null fields are
 *  suppressed. If <i>limit</i> is a positive number, at most that number of
 *  fields will be returned (if <i>limit</i> is <code>1</code>, the entire
 *  string is returned as the only entry in an array). If negative, there is no
 *  limit to the number of fields returned, and trailing null fields are not
 *  suppressed.
 *
 *     " now's  the time".split        #=> ["now's", "the", "time"]
 *     " now's  the time".split(' ')   #=> ["now's", "the", "time"]
 *     " now's  the time".split(/ /)   #=> ["", "now's", "", "the", "time"]
 *     "hello".split(//)               #=> ["h", "e", "l", "l", "o"]
 *     "hello".split(//, 3)            #=> ["h", "e", "llo"]
 *
 *     "mellow yellow".split("ello")   #=> ["m", "w y", "w"]
 *     "1,2,,3,4,,".split(',')         #=> ["1", "2", "", "3", "4"]
 *     "1,2,,3,4,,".split(',', 4)      #=> ["1", "2", "", "3,4,,"]
 *     "1,2,,3,4,,".split(',', -4)     #=> ["1", "2", "", "3", "4", "", ""]
 */

static value
str_split_m(state *mrb, value str)
{
  int argc;
  value spat = nil_value();
  enum {awk, string, regexp} split_type = string;
  int i = 0;
  int beg;
  int end;
  int lim = 0;
  bool lim_p;
  value result, tmp;

  argc = get_args(mrb, "|oi", &spat, &lim);
  lim_p = (lim > 0 && argc == 2);
  if (argc == 2) {
    if (lim == 1) {
      if (RSTRING_LEN(str) == 0)
        return ary_new_capa(mrb, 0);
      return ary_new_from_values(mrb, 1, &str);
    }
    i = 1;
  }

  if (argc == 0 || nil_p(spat)) {
    split_type = awk;
  }
  else {
    if (string_p(spat)) {
      split_type = string;
      if (RSTRING_LEN(spat) == 1 && RSTRING_PTR(spat)[0] == ' ') {
          split_type = awk;
      }
    }
    else {
      noregexp(mrb, str);
    }
  }

  result = ary_new(mrb);
  beg = 0;
  if (split_type == awk) {
    bool skip = TRUE;
    int idx = 0;
    int str_len = RSTRING_LEN(str);
    unsigned int c;
    int ai = gc_arena_save(mrb);

    idx = end = beg;
    while (idx < str_len) {
      c = (unsigned char)RSTRING_PTR(str)[idx++];
      if (skip) {
        if (ISSPACE(c)) {
          beg = idx;
        }
        else {
          end = idx;
          skip = FALSE;
          if (lim_p && lim <= i) break;
        }
      }
      else if (ISSPACE(c)) {
        ary_push(mrb, result, byte_subseq(mrb, str, beg, end-beg));
        gc_arena_restore(mrb, ai);
        skip = TRUE;
        beg = idx;
        if (lim_p) ++i;
      }
      else {
        end = idx;
      }
    }
  }
  else if (split_type == string) {
    int str_len = RSTRING_LEN(str);
    int pat_len = RSTRING_LEN(spat);
    int idx = 0;
    int ai = gc_arena_save(mrb);

    while (idx < str_len) {
      if (pat_len > 0) {
        end = memsearch(RSTRING_PTR(spat), pat_len, RSTRING_PTR(str)+idx, str_len - idx);
        if (end < 0) break;
      }
      else {
        end = chars2bytes(str, idx, 1);
      }
      ary_push(mrb, result, byte_subseq(mrb, str, idx, end));
      gc_arena_restore(mrb, ai);
      idx += end + pat_len;
      if (lim_p && lim <= ++i) break;
    }
    beg = idx;
  }
  else {
    noregexp(mrb, str);
  }
  if (RSTRING_LEN(str) > 0 && (lim_p || RSTRING_LEN(str) > beg || lim < 0)) {
    if (RSTRING_LEN(str) == beg) {
      tmp = str_new_empty(mrb, str);
    }
    else {
      tmp = byte_subseq(mrb, str, beg, RSTRING_LEN(str)-beg);
    }
    ary_push(mrb, result, tmp);
  }
  if (!lim_p && lim == 0) {
    int len;
    while ((len = RARRAY_LEN(result)) > 0 &&
           (tmp = RARRAY_PTR(result)[len-1], RSTRING_LEN(tmp) == 0))
      ary_pop(mrb, result);
  }

  return result;
}

API value
str_len_to_inum(state *mrb, const char *str, int len, int base, int badcheck)
{
  const char *p = str;
  const char *pend = str + len;
  char sign = 1;
  int c;
  uint64_t n = 0;
  int val;

#define conv_digit(c) \
    (ISDIGIT(c) ? ((c) - '0') : \
     ISLOWER(c) ? ((c) - 'a' + 10) : \
     ISUPPER(c) ? ((c) - 'A' + 10) : \
     -1)

  if (!p) {
    if (badcheck) goto bad;
    return fixnum_value(0);
  }
  while (p<pend && ISSPACE(*p))
    p++;

  if (p[0] == '+') {
    p++;
  }
  else if (p[0] == '-') {
    p++;
    sign = 0;
  }
  if (base <= 0) {
    if (p[0] == '0') {
      switch (p[1]) {
        case 'x': case 'X':
          base = 16;
          break;
        case 'b': case 'B':
          base = 2;
          break;
        case 'o': case 'O':
          base = 8;
          break;
        case 'd': case 'D':
          base = 10;
          break;
        default:
          base = 8;
          break;
      }
    }
    else if (base < -1) {
      base = -base;
    }
    else {
      base = 10;
    }
  }
  switch (base) {
    case 2:
      if (p[0] == '0' && (p[1] == 'b'||p[1] == 'B')) {
        p += 2;
      }
      break;
    case 3:
      break;
    case 8:
      if (p[0] == '0' && (p[1] == 'o'||p[1] == 'O')) {
        p += 2;
      }
    case 4: case 5: case 6: case 7:
      break;
    case 10:
      if (p[0] == '0' && (p[1] == 'd'||p[1] == 'D')) {
        p += 2;
      }
    case 9: case 11: case 12: case 13: case 14: case 15:
      break;
    case 16:
      if (p[0] == '0' && (p[1] == 'x'||p[1] == 'X')) {
        p += 2;
      }
      break;
    default:
      if (base < 2 || 36 < base) {
        raisef(mrb, E_ARGUMENT_ERROR, "illegal radix %S", fixnum_value(base));
      }
      break;
  } /* end of switch (base) { */
  if (p>=pend) {
    if (badcheck) goto bad;
    return fixnum_value(0);
  }
  if (*p == '0') {    /* squeeze preceding 0s */
    p++;
    while (p<pend) {
      c = *p++;
      if (c == '_') {
        if (p<pend && *p == '_') {
          if (badcheck) goto bad;
          break;
        }
        continue;
      }
      if (c != '0') {
        p--;
        break;
      }
    }
    if (*(p - 1) == '0')
      p--;
  }
  if (p == pend) {
    if (badcheck) goto bad;
    return fixnum_value(0);
  }
  for ( ;p<pend;p++) {
    if (*p == '_') {
      p++;
      if (p==pend) {
        if (badcheck) goto bad;
        continue;
      }
      if (*p == '_') {
        if (badcheck) goto bad;
        break;
      }
    }
    if (badcheck && *p == '\0') {
      goto nullbyte;
    }
    c = conv_digit(*p);
    if (c < 0 || c >= base) {
      break;
    }
    n *= base;
    n += c;
    if (n > (uint64_t)INT_MAX + (sign ? 0 : 1)) {
#ifndef WITHOUT_FLOAT
      if (base == 10) {
        return float_value(mrb, str_to_dbl(mrb, str_new(mrb, str, len), badcheck));
      }
      else
#endif
      {
        raisef(mrb, E_ARGUMENT_ERROR, "string (%S) too big for integer",
                   str_new(mrb, str, pend-str));
      }
    }
  }
  val = (int)n;
  if (badcheck) {
    if (p == str) goto bad; /* no number */
    while (p<pend && ISSPACE(*p)) p++;
    if (p<pend) goto bad;       /* trailing garbage */
  }

  return fixnum_value(sign ? val : -val);
 nullbyte:
  raise(mrb, E_ARGUMENT_ERROR, "string contains null byte");
  /* not reached */
 bad:
  raisef(mrb, E_ARGUMENT_ERROR, "invalid string for number(%S)",
             inspect(mrb, str_new(mrb, str, pend-str)));
  /* not reached */
  return fixnum_value(0);
}

API value
cstr_to_inum(state *mrb, const char *str, int base, int badcheck)
{
  return str_len_to_inum(mrb, str, strlen(str), base, badcheck);
}

API const char*
string_value_cstr(state *mrb, value *ptr)
{
  value str = str_to_str(mrb, *ptr);
  struct RString *ps = str_ptr(str);
  int len = str_strlen(mrb, ps);
  char *p = RSTR_PTR(ps);

  if (!p || p[len] != '\0') {
    if (FROZEN_P(ps)) {
      *ptr = str = str_dup(mrb, str);
      ps = str_ptr(str);
    }
    str_modify(mrb, ps);
    return RSTR_PTR(ps);
  }
  return p;
}

API value
str_to_inum(state *mrb, value str, int base, bool badcheck)
{
  const char *s;
  int len;

  s = string_value_ptr(mrb, str);
  len = RSTRING_LEN(str);
  return str_len_to_inum(mrb, s, len, base, badcheck);
}

/* 15.2.10.5.38 */
/*
 *  call-seq:
 *     str.to_i(base=10)   => integer
 *
 *  Returns the result of interpreting leading characters in <i>str</i> as an
 *  integer base <i>base</i> (between 2 and 36). Extraneous characters past the
 *  end of a valid number are ignored. If there is not a valid number at the
 *  start of <i>str</i>, <code>0</code> is returned. This method never raises an
 *  exception.
 *
 *     "12345".to_i             #=> 12345
 *     "99 red balloons".to_i   #=> 99
 *     "0a".to_i                #=> 0
 *     "0a".to_i(16)            #=> 10
 *     "hello".to_i             #=> 0
 *     "1100101".to_i(2)        #=> 101
 *     "1100101".to_i(8)        #=> 294977
 *     "1100101".to_i(10)       #=> 1100101
 *     "1100101".to_i(16)       #=> 17826049
 */
static value
str_to_i(state *mrb, value self)
{
  int base = 10;

  get_args(mrb, "|i", &base);
  if (base < 0) {
    raisef(mrb, E_ARGUMENT_ERROR, "illegal radix %S", fixnum_value(base));
  }
  return str_to_inum(mrb, self, base, FALSE);
}

#ifndef WITHOUT_FLOAT
API double
cstr_to_dbl(state *mrb, const char * p, bool badcheck)
{
  char *end;
  char buf[DBL_DIG * 4 + 10];
  double d;

  enum {max_width = 20};

  if (!p) return 0.0;
  while (ISSPACE(*p)) p++;

  if (!badcheck && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
    return 0.0;
  }
  d = float_read(p, &end);
  if (p == end) {
    if (badcheck) {
bad:
      raisef(mrb, E_ARGUMENT_ERROR, "invalid string for float(%S)", str_new_cstr(mrb, p));
      /* not reached */
    }
    return d;
  }
  if (*end) {
    char *n = buf;
    char *e = buf + sizeof(buf) - 1;
    char prev = 0;

    while (p < end && n < e) prev = *n++ = *p++;
    while (*p) {
      if (*p == '_') {
        /* remove underscores between digits */
        if (badcheck) {
          if (n == buf || !ISDIGIT(prev)) goto bad;
          ++p;
          if (!ISDIGIT(*p)) goto bad;
        }
        else {
          while (*++p == '_');
          continue;
        }
      }
      prev = *p++;
      if (n < e) *n++ = prev;
    }
    *n = '\0';
    p = buf;

    if (!badcheck && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
      return 0.0;
    }

    d = float_read(p, &end);
    if (badcheck) {
      if (!end || p == end) goto bad;
      while (*end && ISSPACE(*end)) end++;
      if (*end) goto bad;
    }
  }
  return d;
}

API double
str_to_dbl(state *mrb, value str, bool badcheck)
{
  char *s;
  int len;

  str = str_to_str(mrb, str);
  s = RSTRING_PTR(str);
  len = RSTRING_LEN(str);
  if (s) {
    if (badcheck && memchr(s, '\0', len)) {
      raise(mrb, E_ARGUMENT_ERROR, "string for Float contains null byte");
    }
    if (s[len]) {    /* no sentinel somehow */
      struct RString *temp_str = str_new(mrb, s, len);
      s = RSTR_PTR(temp_str);
    }
  }
  return cstr_to_dbl(mrb, s, badcheck);
}

/* 15.2.10.5.39 */
/*
 *  call-seq:
 *     str.to_f   => float
 *
 *  Returns the result of interpreting leading characters in <i>str</i> as a
 *  floating point number. Extraneous characters past the end of a valid number
 *  are ignored. If there is not a valid number at the start of <i>str</i>,
 *  <code>0.0</code> is returned. This method never raises an exception.
 *
 *     "123.45e1".to_f        #=> 1234.5
 *     "45.67 degrees".to_f   #=> 45.67
 *     "thx1138".to_f         #=> 0.0
 */
static value
str_to_f(state *mrb, value self)
{
  return float_value(mrb, str_to_dbl(mrb, self, FALSE));
}
#endif

/* 15.2.10.5.40 */
/*
 *  call-seq:
 *     str.to_s     => str
 *     str.to_str   => str
 *
 *  Returns the receiver.
 */
static value
str_to_s(state *mrb, value self)
{
  if (obj_class(mrb, self) != mrb->string_class) {
    return str_dup(mrb, self);
  }
  return self;
}

/* 15.2.10.5.43 */
/*
 *  call-seq:
 *     str.upcase!   => str or nil
 *
 *  Upcases the contents of <i>str</i>, returning <code>nil</code> if no changes
 *  were made.
 */
static value
str_upcase_bang(state *mrb, value str)
{
  struct RString *s = str_ptr(str);
  char *p, *pend;
  bool modify = FALSE;

  str_modify(mrb, s);
  p = RSTRING_PTR(str);
  pend = RSTRING_END(str);
  while (p < pend) {
    if (ISLOWER(*p)) {
      *p = TOUPPER(*p);
      modify = TRUE;
    }
    p++;
  }

  if (modify) return str;
  return nil_value();
}

/* 15.2.10.5.42 */
/*
 *  call-seq:
 *     str.upcase   => new_str
 *
 *  Returns a copy of <i>str</i> with all lowercase letters replaced with their
 *  uppercase counterparts. The operation is locale insensitive---only
 *  characters 'a' to 'z' are affected.
 *
 *     "hEllO".upcase   #=> "HELLO"
 */
static value
str_upcase(state *mrb, value self)
{
  value str;

  str = str_dup(mrb, self);
  str_upcase_bang(mrb, str);
  return str;
}

#define IS_EVSTR(p,e) ((p) < (e) && (*(p) == '$' || *(p) == '@' || *(p) == '{'))

/*
 *  call-seq:
 *     str.dump   -> new_str
 *
 *  Produces a version of <i>str</i> with all nonprinting characters replaced by
 *  <code>\nnn</code> notation and all special characters escaped.
 */
value
str_dump(state *mrb, value str)
{
  int len;
  const char *p, *pend;
  char *q;
  struct RString *result;

  len = 2;                  /* "" */
  p = RSTRING_PTR(str); pend = p + RSTRING_LEN(str);
  while (p < pend) {
    unsigned char c = *p++;
    switch (c) {
      case '"':  case '\\':
      case '\n': case '\r':
      case '\t': case '\f':
      case '\013': case '\010': case '\007': case '\033':
        len += 2;
        break;

      case '#':
        len += IS_EVSTR(p, pend) ? 2 : 1;
        break;

      default:
        if (ISPRINT(c)) {
          len++;
        }
        else {
          len += 4;                /* \NNN */
        }
        break;
    }
  }

  result = str_new(mrb, 0, len);
  str_with_class(mrb, result, str);
  p = RSTRING_PTR(str); pend = p + RSTRING_LEN(str);
  q = RSTR_PTR(result);
  *q++ = '"';
  while (p < pend) {
    unsigned char c = *p++;

    switch (c) {
      case '"':
      case '\\':
        *q++ = '\\';
        *q++ = c;
        break;

      case '\n':
        *q++ = '\\';
        *q++ = 'n';
        break;

      case '\r':
        *q++ = '\\';
        *q++ = 'r';
        break;

      case '\t':
        *q++ = '\\';
        *q++ = 't';
        break;

      case '\f':
        *q++ = '\\';
        *q++ = 'f';
        break;

      case '\013':
        *q++ = '\\';
        *q++ = 'v';
        break;

      case '\010':
        *q++ = '\\';
        *q++ = 'b';
        break;

      case '\007':
        *q++ = '\\';
        *q++ = 'a';
        break;

      case '\033':
        *q++ = '\\';
        *q++ = 'e';
        break;

      case '#':
        if (IS_EVSTR(p, pend)) *q++ = '\\';
        *q++ = '#';
        break;

      default:
        if (ISPRINT(c)) {
          *q++ = c;
        }
        else {
          *q++ = '\\';
          *q++ = 'x';
          q[1] = digitmap[c % 16]; c /= 16;
          q[0] = digitmap[c % 16];
          q += 2;
        }
    }
  }
  *q = '"';
  return obj_value(result);
}

API value
str_cat(state *mrb, value str, const char *ptr, size_t len)
{
  struct RString *s = str_ptr(str);
  size_t capa;
  size_t total;
  ptrdiff_t off = -1;

  if (len == 0) return str;
  str_modify(mrb, s);
  if (ptr >= RSTR_PTR(s) && ptr <= RSTR_PTR(s) + (size_t)RSTR_LEN(s)) {
      off = ptr - RSTR_PTR(s);
  }

  capa = RSTR_CAPA(s);
  total = RSTR_LEN(s)+len;
  if (total >= INT_MAX) {
  size_error:
    raise(mrb, E_ARGUMENT_ERROR, "string size too big");
  }
  if (capa <= total) {
    if (capa == 0) capa = 1;
    while (capa <= total) {
      if (capa <= INT_MAX / 2) {
        capa *= 2;
      }
      else {
        capa = total+1;
      }
    }
    if (capa <= total || capa > INT_MAX) {
      goto size_error;
    }
    resize_capa(mrb, s, capa);
  }
  if (off != -1) {
      ptr = RSTR_PTR(s) + off;
  }
  memcpy(RSTR_PTR(s) + RSTR_LEN(s), ptr, len);
  assert_int_fit(size_t, total, int, INT_MAX);
  RSTR_SET_LEN(s, total);
  RSTR_PTR(s)[total] = '\0';   /* sentinel */
  return str;
}

API value
str_cat_cstr(state *mrb, value str, const char *ptr)
{
  return str_cat(mrb, str, ptr, strlen(ptr));
}

API value
str_cat_str(state *mrb, value str, value str2)
{
  if (str_ptr(str) == str_ptr(str2)) {
    str_modify(mrb, str_ptr(str));
  }
  return str_cat(mrb, str, RSTRING_PTR(str2), RSTRING_LEN(str2));
}

API value
str_append(state *mrb, value str1, value str2)
{
  str2 = str_to_str(mrb, str2);
  return str_cat_str(mrb, str1, str2);
}

#define CHAR_ESC_LEN 13 /* sizeof(\x{ hex of RB32BIT unsigned int } \0) */

/*
 * call-seq:
 *   str.inspect   -> string
 *
 * Returns a printable version of _str_, surrounded by quote marks,
 * with special characters escaped.
 *
 *    str = "hello"
 *    str[3] = "\b"
 *    str.inspect       #=> "\"hel\\bo\""
 */
value
str_inspect(state *mrb, value str)
{
  const char *p, *pend;
  char buf[CHAR_ESC_LEN + 1];
  value result = str_new_lit(mrb, "\"");

  p = RSTRING_PTR(str); pend = RSTRING_END(str);
  for (;p < pend; p++) {
    unsigned char c, cc;
#ifdef UTF8_STRING
    int clen;

    clen = utf8len(p, pend);
    if (clen > 1) {
      int i;

      for (i=0; i<clen; i++) {
        buf[i] = p[i];
      }
      str_cat(mrb, result, buf, clen);
      p += clen-1;
      continue;
    }
#endif
    c = *p;
    if (c == '"'|| c == '\\' || (c == '#' && IS_EVSTR(p+1, pend))) {
      buf[0] = '\\'; buf[1] = c;
      str_cat(mrb, result, buf, 2);
      continue;
    }
    if (ISPRINT(c)) {
      buf[0] = c;
      str_cat(mrb, result, buf, 1);
      continue;
    }
    switch (c) {
      case '\n': cc = 'n'; break;
      case '\r': cc = 'r'; break;
      case '\t': cc = 't'; break;
      case '\f': cc = 'f'; break;
      case '\013': cc = 'v'; break;
      case '\010': cc = 'b'; break;
      case '\007': cc = 'a'; break;
      case 033: cc = 'e'; break;
      default: cc = 0; break;
    }
    if (cc) {
      buf[0] = '\\';
      buf[1] = (char)cc;
      str_cat(mrb, result, buf, 2);
      continue;
    }
    else {
      buf[0] = '\\';
      buf[1] = 'x';
      buf[3] = digitmap[c % 16]; c /= 16;
      buf[2] = digitmap[c % 16];
      str_cat(mrb, result, buf, 4);
      continue;
    }
  }
  str_cat_lit(mrb, result, "\"");

  return result;
}

/*
 * call-seq:
 *   str.bytes   -> array of fixnums
 *
 * Returns an array of bytes in _str_.
 *
 *    str = "hello"
 *    str.bytes       #=> [104, 101, 108, 108, 111]
 */
static value
str_bytes(state *mrb, value str)
{
  struct RString *s = str_ptr(str);
  value a = ary_new_capa(mrb, RSTR_LEN(s));
  unsigned char *p = (unsigned char *)(RSTR_PTR(s)), *pend = p + RSTR_LEN(s);

  while (p < pend) {
    ary_push(mrb, a, fixnum_value(p[0]));
    p++;
  }
  return a;
}

/* ---------------------------*/
void
init_string(state *mrb)
{
  struct RClass *s;

  static_assert(RSTRING_EMBED_LEN_MAX < (1 << 5), "pointer size too big for embedded string");

  mrb->string_class = s = define_class(mrb, "String", mrb->object_class);             /* 15.2.10 */
  SET_INSTANCE_TT(s, TT_STRING);

  define_method(mrb, s, "bytesize",        str_bytesize,        ARGS_NONE());

  define_method(mrb, s, "<=>",             str_cmp_m,           ARGS_REQ(1)); /* 15.2.10.5.1  */
  define_method(mrb, s, "==",              str_equal_m,         ARGS_REQ(1)); /* 15.2.10.5.2  */
  define_method(mrb, s, "+",               str_plus_m,          ARGS_REQ(1)); /* 15.2.10.5.4  */
  define_method(mrb, s, "*",               str_times,           ARGS_REQ(1)); /* 15.2.10.5.5  */
  define_method(mrb, s, "[]",              str_aref_m,          ARGS_ANY());  /* 15.2.10.5.6  */
  define_method(mrb, s, "capitalize",      str_capitalize,      ARGS_NONE()); /* 15.2.10.5.7  */
  define_method(mrb, s, "capitalize!",     str_capitalize_bang, ARGS_NONE()); /* 15.2.10.5.8  */
  define_method(mrb, s, "chomp",           str_chomp,           ARGS_ANY());  /* 15.2.10.5.9  */
  define_method(mrb, s, "chomp!",          str_chomp_bang,      ARGS_ANY());  /* 15.2.10.5.10 */
  define_method(mrb, s, "chop",            str_chop,            ARGS_NONE()); /* 15.2.10.5.11 */
  define_method(mrb, s, "chop!",           str_chop_bang,       ARGS_NONE()); /* 15.2.10.5.12 */
  define_method(mrb, s, "downcase",        str_downcase,        ARGS_NONE()); /* 15.2.10.5.13 */
  define_method(mrb, s, "downcase!",       str_downcase_bang,   ARGS_NONE()); /* 15.2.10.5.14 */
  define_method(mrb, s, "empty?",          str_empty_p,         ARGS_NONE()); /* 15.2.10.5.16 */
  define_method(mrb, s, "eql?",            str_eql,             ARGS_REQ(1)); /* 15.2.10.5.17 */

  define_method(mrb, s, "hash",            str_hash_m,          ARGS_NONE()); /* 15.2.10.5.20 */
  define_method(mrb, s, "include?",        str_include,         ARGS_REQ(1)); /* 15.2.10.5.21 */
  define_method(mrb, s, "index",           str_index_m,         ARGS_ANY());  /* 15.2.10.5.22 */
  define_method(mrb, s, "initialize",      str_init,            ARGS_REQ(1)); /* 15.2.10.5.23 */
  define_method(mrb, s, "initialize_copy", str_replace,         ARGS_REQ(1)); /* 15.2.10.5.24 */
  define_method(mrb, s, "intern",          str_intern,          ARGS_NONE()); /* 15.2.10.5.25 */
  define_method(mrb, s, "length",          str_size,            ARGS_NONE()); /* 15.2.10.5.26 */
  define_method(mrb, s, "replace",         str_replace,         ARGS_REQ(1)); /* 15.2.10.5.28 */
  define_method(mrb, s, "reverse",         str_reverse,         ARGS_NONE()); /* 15.2.10.5.29 */
  define_method(mrb, s, "reverse!",        str_reverse_bang,    ARGS_NONE()); /* 15.2.10.5.30 */
  define_method(mrb, s, "rindex",          str_rindex,          ARGS_ANY());  /* 15.2.10.5.31 */
  define_method(mrb, s, "size",            str_size,            ARGS_NONE()); /* 15.2.10.5.33 */
  define_method(mrb, s, "slice",           str_aref_m,          ARGS_ANY());  /* 15.2.10.5.34 */
  define_method(mrb, s, "split",           str_split_m,         ARGS_ANY());  /* 15.2.10.5.35 */

#ifndef WITHOUT_FLOAT
  define_method(mrb, s, "to_f",            str_to_f,            ARGS_NONE()); /* 15.2.10.5.38 */
#endif
  define_method(mrb, s, "to_i",            str_to_i,            ARGS_ANY());  /* 15.2.10.5.39 */
  define_method(mrb, s, "to_s",            str_to_s,            ARGS_NONE()); /* 15.2.10.5.40 */
  define_method(mrb, s, "to_str",          str_to_s,            ARGS_NONE());
  define_method(mrb, s, "to_sym",          str_intern,          ARGS_NONE()); /* 15.2.10.5.41 */
  define_method(mrb, s, "upcase",          str_upcase,          ARGS_NONE()); /* 15.2.10.5.42 */
  define_method(mrb, s, "upcase!",         str_upcase_bang,     ARGS_NONE()); /* 15.2.10.5.43 */
  define_method(mrb, s, "inspect",         str_inspect,         ARGS_NONE()); /* 15.2.10.5.46(x) */
  define_method(mrb, s, "bytes",           str_bytes,           ARGS_NONE());
}

#ifndef WITHOUT_FLOAT
/*
 * Source code for the "strtod" library procedure.
 *
 * Copyright (c) 1988-1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * RCS: @(#) $Id: strtod.c 11708 2007-02-12 23:01:19Z shyouhei $
 */

#include <ctype.h>
#include <errno.h>

static const int maxExponent = 511; /* Largest possible base 10 exponent.  Any
                                     * exponent larger than this will already
                                     * produce underflow or overflow, so there's
                                     * no need to worry about additional digits.
                                     */
static const double powersOf10[] = {/* Table giving binary powers of 10.  Entry */
    10.,                            /* is 10^2^i.  Used to convert decimal */
    100.,                           /* exponents into floating-point numbers. */
    1.0e4,
    1.0e8,
    1.0e16,
    1.0e32,
    1.0e64,
    1.0e128,
    1.0e256
};

API double
float_read(const char *string, char **endPtr)
/*  const char *string;            A decimal ASCII floating-point number,
                                 * optionally preceded by white space.
                                 * Must have form "-I.FE-X", where I is the
                                 * integer part of the mantissa, F is the
                                 * fractional part of the mantissa, and X
                                 * is the exponent.  Either of the signs
                                 * may be "+", "-", or omitted.  Either I
                                 * or F may be omitted, or both.  The decimal
                                 * point isn't necessary unless F is present.
                                 * The "E" may actually be an "e".  E and X
                                 * may both be omitted (but not just one).
                                 */
/*  char **endPtr;                 If non-NULL, store terminating character's
                                 * address here. */
{
    int sign, expSign = FALSE;
    double fraction, dblExp;
    const double *d;
    const char *p;
    int c;
    int exp = 0;                /* Exponent read from "EX" field. */
    int fracExp = 0;            /* Exponent that derives from the fractional
                                 * part.  Under normal circumstatnces, it is
                                 * the negative of the number of digits in F.
                                 * However, if I is very long, the last digits
                                 * of I get dropped (otherwise a long I with a
                                 * large negative exponent could cause an
                                 * unnecessary overflow on I alone).  In this
                                 * case, fracExp is incremented one for each
                                 * dropped digit. */
    int mantSize;               /* Number of digits in mantissa. */
    int decPt;                  /* Number of mantissa digits BEFORE decimal
                                 * point. */
    const char *pExp;           /* Temporarily holds location of exponent
                                 * in string. */

    /*
     * Strip off leading blanks and check for a sign.
     */

    p = string;
    while (isspace(*p)) {
      p += 1;
    }
    if (*p == '-') {
      sign = TRUE;
      p += 1;
    }
    else {
      if (*p == '+') {
        p += 1;
      }
      sign = FALSE;
    }

    /*
     * Count the number of digits in the mantissa (including the decimal
     * point), and also locate the decimal point.
     */

    decPt = -1;
    for (mantSize = 0; ; mantSize += 1)
    {
      c = *p;
      if (!isdigit(c)) {
        if ((c != '.') || (decPt >= 0)) {
          break;
        }
        decPt = mantSize;
      }
      p += 1;
    }

    /*
     * Now suck up the digits in the mantissa.  Use two integers to
     * collect 9 digits each (this is faster than using floating-point).
     * If the mantissa has more than 18 digits, ignore the extras, since
     * they can't affect the value anyway.
     */

    pExp  = p;
    p -= mantSize;
    if (decPt < 0) {
      decPt = mantSize;
    }
    else {
      mantSize -= 1; /* One of the digits was the point. */
    }
    if (mantSize > 18) {
      if (decPt - 18 > 29999) {
        fracExp = 29999;
      }
      else {
        fracExp = decPt - 18;
      }
      mantSize = 18;
    }
    else {
      fracExp = decPt - mantSize;
    }
    if (mantSize == 0) {
      fraction = 0.0;
      p = string;
      goto done;
    }
    else {
      int frac1, frac2;
      frac1 = 0;
      for ( ; mantSize > 9; mantSize -= 1)
      {
        c = *p;
        p += 1;
        if (c == '.') {
          c = *p;
          p += 1;
        }
        frac1 = 10*frac1 + (c - '0');
      }
      frac2 = 0;
      for (; mantSize > 0; mantSize -= 1)
      {
        c = *p;
        p += 1;
        if (c == '.') {
          c = *p;
          p += 1;
        }
        frac2 = 10*frac2 + (c - '0');
      }
      fraction = (1.0e9 * frac1) + frac2;
    }

    /*
     * Skim off the exponent.
     */

    p = pExp;
    if ((*p == 'E') || (*p == 'e')) {
      p += 1;
      if (*p == '-') {
        expSign = TRUE;
        p += 1;
      }
      else {
        if (*p == '+') {
          p += 1;
        }
        expSign = FALSE;
      }
      while (isdigit(*p)) {
        exp = exp * 10 + (*p - '0');
        if (exp > 19999) {
          exp = 19999;
        }
        p += 1;
      }
    }
    if (expSign) {
      exp = fracExp - exp;
    }
    else {
      exp = fracExp + exp;
    }

    /*
     * Generate a floating-point number that represents the exponent.
     * Do this by processing the exponent one bit at a time to combine
     * many powers of 2 of 10. Then combine the exponent with the
     * fraction.
     */

    if (exp < 0) {
      expSign = TRUE;
      exp = -exp;
    }
    else {
      expSign = FALSE;
    }
    if (exp > maxExponent) {
      exp = maxExponent;
      errno = ERANGE;
    }
    dblExp = 1.0;
    for (d = powersOf10; exp != 0; exp >>= 1, d += 1) {
      if (exp & 01) {
        dblExp *= *d;
      }
    }
    if (expSign) {
      fraction /= dblExp;
    }
    else {
      fraction *= dblExp;
    }

done:
    if (endPtr != NULL) {
      *endPtr = (char *) p;
    }

    if (sign) {
      return -fraction;
    }
    return fraction;
}
#endif
