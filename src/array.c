/*
** array.c - Array class
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/string.h>
#include <mruby/range.h>
#include "value_array.h"

#define ARY_DEFAULT_LEN   4
#define ARY_SHRINK_RATIO  5 /* must be larger than 2 */
#define ARY_C_MAX_SIZE (SIZE_MAX / sizeof($value))
#define ARY_MAX_SIZE (($int)((ARY_C_MAX_SIZE < (size_t)$INT_MAX) ? ARY_C_MAX_SIZE : $INT_MAX-1))

static struct RArray*
ary_new_capa($state *mrb, $int capa)
{
  struct RArray *a;
  size_t blen;

  if (capa > ARY_MAX_SIZE) {
    $raise(mrb, E_ARGUMENT_ERROR, "array size too big");
  }
  blen = capa * sizeof($value);

  a = (struct RArray*)$obj_alloc(mrb, $TT_ARRAY, mrb->array_class);
  if (capa <= $ARY_EMBED_LEN_MAX) {
    ARY_SET_EMBED_LEN(a, 0);
  }
  else {
    a->as.heap.ptr = ($value *)$malloc(mrb, blen);
    a->as.heap.aux.capa = capa;
    a->as.heap.len = 0;
  }

  return a;
}

$API $value
$ary_new_capa($state *mrb, $int capa)
{
  struct RArray *a = ary_new_capa(mrb, capa);
  return $obj_value(a);
}

$API $value
$ary_new($state *mrb)
{
  return $ary_new_capa(mrb, 0);
}

/*
 * to copy array, use this instead of memcpy because of portability
 * * gcc on ARM may fail optimization of memcpy
 *   http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.faqs/ka3934.html
 * * gcc on MIPS also fail
 *   http://gcc.gnu.org/bugzilla/show_bug.cgi?id=39755
 * * memcpy doesn't exist on freestanding environment
 *
 * If you optimize for binary size, use memcpy instead of this at your own risk
 * of above portability issue.
 *
 * see also http://togetter.com/li/462898
 *
 */
static inline void
array_copy($value *dst, const $value *src, $int size)
{
  $int i;

  for (i = 0; i < size; i++) {
    dst[i] = src[i];
  }
}

static struct RArray*
ary_new_from_values($state *mrb, $int size, const $value *vals)
{
  struct RArray *a = ary_new_capa(mrb, size);

  array_copy(ARY_PTR(a), vals, size);
  ARY_SET_LEN(a, size);

  return a;
}

$API $value
$ary_new_from_values($state *mrb, $int size, const $value *vals)
{
  struct RArray *a = ary_new_from_values(mrb, size, vals);
  return $obj_value(a);
}

$API $value
$assoc_new($state *mrb, $value car, $value cdr)
{
  struct RArray *a;

  a = ary_new_capa(mrb, 2);
  ARY_PTR(a)[0] = car;
  ARY_PTR(a)[1] = cdr;
  ARY_SET_LEN(a, 2);
  return $obj_value(a);
}

static void
ary_fill_with_nil($value *ptr, $int size)
{
  $value nil = $nil_value();

  while (size--) {
    *ptr++ = nil;
  }
}

static void
ary_modify_check($state *mrb, struct RArray *a)
{
  if ($FROZEN_P(a)) {
    $raise(mrb, E_FROZEN_ERROR, "can't modify frozen array");
  }
}

static void
ary_modify($state *mrb, struct RArray *a)
{
  ary_modify_check(mrb, a);

  if (ARY_SHARED_P(a)) {
    $shared_array *shared = a->as.heap.aux.shared;

    if (shared->refcnt == 1 && a->as.heap.ptr == shared->ptr) {
      a->as.heap.ptr = shared->ptr;
      a->as.heap.aux.capa = a->as.heap.len;
      $free(mrb, shared);
    }
    else {
      $value *ptr, *p;
      $int len;

      p = a->as.heap.ptr;
      len = a->as.heap.len * sizeof($value);
      ptr = ($value *)$malloc(mrb, len);
      if (p) {
        array_copy(ptr, p, a->as.heap.len);
      }
      a->as.heap.ptr = ptr;
      a->as.heap.aux.capa = a->as.heap.len;
      $ary_decref(mrb, shared);
    }
    ARY_UNSET_SHARED_FLAG(a);
  }
}

$API void
$ary_modify($state *mrb, struct RArray* a)
{
  $write_barrier(mrb, (struct RBasic*)a);
  ary_modify(mrb, a);
}

static void
ary_make_shared($state *mrb, struct RArray *a)
{
  if (!ARY_SHARED_P(a) && !ARY_EMBED_P(a)) {
    $shared_array *shared = ($shared_array *)$malloc(mrb, sizeof($shared_array));
    $value *ptr = a->as.heap.ptr;
    $int len = a->as.heap.len;

    shared->refcnt = 1;
    if (a->as.heap.aux.capa > len) {
      a->as.heap.ptr = shared->ptr = ($value *)$realloc(mrb, ptr, sizeof($value)*len+1);
    }
    else {
      shared->ptr = ptr;
    }
    shared->len = len;
    a->as.heap.aux.shared = shared;
    ARY_SET_SHARED_FLAG(a);
  }
}

static void
ary_expand_capa($state *mrb, struct RArray *a, $int len)
{
  $int capa = ARY_CAPA(a);

  if (len > ARY_MAX_SIZE || len < 0) {
  size_error:
    $raise(mrb, E_ARGUMENT_ERROR, "array size too big");
  }

  if (capa < ARY_DEFAULT_LEN) {
    capa = ARY_DEFAULT_LEN;
  }
  while (capa < len) {
    if (capa <= ARY_MAX_SIZE / 2) {
      capa *= 2;
    }
    else {
      capa = len;
    }
  }
  if (capa < len || capa > ARY_MAX_SIZE) {
    goto size_error;
  }

  if (ARY_EMBED_P(a)) {
    $value *ptr = ARY_EMBED_PTR(a);
    $int len = ARY_EMBED_LEN(a);
    $value *expanded_ptr = ($value *)$malloc(mrb, sizeof($value)*capa);

    ARY_UNSET_EMBED_FLAG(a);
    array_copy(expanded_ptr, ptr, len);
    a->as.heap.len = len;
    a->as.heap.aux.capa = capa;
    a->as.heap.ptr = expanded_ptr;
  }
  else if (capa > a->as.heap.aux.capa) {
    $value *expanded_ptr = ($value *)$realloc(mrb, a->as.heap.ptr, sizeof($value)*capa);

    a->as.heap.aux.capa = capa;
    a->as.heap.ptr = expanded_ptr;
  }
}

static void
ary_shrink_capa($state *mrb, struct RArray *a)
{

  $int capa;

  if (ARY_EMBED_P(a)) return;

  capa = a->as.heap.aux.capa;
  if (capa < ARY_DEFAULT_LEN * 2) return;
  if (capa <= a->as.heap.len * ARY_SHRINK_RATIO) return;

  do {
    capa /= 2;
    if (capa < ARY_DEFAULT_LEN) {
      capa = ARY_DEFAULT_LEN;
      break;
    }
  } while (capa > a->as.heap.len * ARY_SHRINK_RATIO);

  if (capa > a->as.heap.len && capa < a->as.heap.aux.capa) {
    a->as.heap.aux.capa = capa;
    a->as.heap.ptr = ($value *)$realloc(mrb, a->as.heap.ptr, sizeof($value)*capa);
  }
}

$API $value
$ary_resize($state *mrb, $value ary, $int new_len)
{
  $int old_len;
  struct RArray *a = $ary_ptr(ary);

  ary_modify(mrb, a);
  old_len = RARRAY_LEN(ary);
  if (old_len != new_len) {
    if (new_len < old_len) {
      ary_shrink_capa(mrb, a);
    }
    else {
      ary_expand_capa(mrb, a, new_len);
      ary_fill_with_nil(ARY_PTR(a) + old_len, new_len - old_len);
    }
    ARY_SET_LEN(a, new_len);
  }

  return ary;
}

static $value
$ary_s_create($state *mrb, $value klass)
{
  $value ary;
  $value *vals;
  $int len;
  struct RArray *a;

  $get_args(mrb, "*!", &vals, &len);
  ary = $ary_new_from_values(mrb, len, vals);
  a = $ary_ptr(ary);
  a->c = $class_ptr(klass);

  return ary;
}

static void ary_replace($state*, struct RArray*, struct RArray*);

static void
ary_concat($state *mrb, struct RArray *a, struct RArray *a2)
{
  $int len;

  if (ARY_LEN(a) == 0) {
    ary_replace(mrb, a, a2);
    return;
  }
  if (ARY_LEN(a2) > ARY_MAX_SIZE - ARY_LEN(a)) {
    $raise(mrb, E_ARGUMENT_ERROR, "array size too big");
  }
  len = ARY_LEN(a) + ARY_LEN(a2);

  ary_modify(mrb, a);
  if (ARY_CAPA(a) < len) {
    ary_expand_capa(mrb, a, len);
  }
  array_copy(ARY_PTR(a)+ARY_LEN(a), ARY_PTR(a2), ARY_LEN(a2));
  $write_barrier(mrb, (struct RBasic*)a);
  ARY_SET_LEN(a, len);
}

$API void
$ary_concat($state *mrb, $value self, $value other)
{
  struct RArray *a2 = $ary_ptr(other);

  ary_concat(mrb, $ary_ptr(self), a2);
}

static $value
$ary_concat_m($state *mrb, $value self)
{
  $value ary;

  $get_args(mrb, "A", &ary);
  $ary_concat(mrb, self, ary);
  return self;
}

static $value
$ary_plus($state *mrb, $value self)
{
  struct RArray *a1 = $ary_ptr(self);
  struct RArray *a2;
  $value *ptr;
  $int blen, len1;

  $get_args(mrb, "a", &ptr, &blen);
  if (ARY_MAX_SIZE - blen < ARY_LEN(a1)) {
    $raise(mrb, E_ARGUMENT_ERROR, "array size too big");
  }
  len1 = ARY_LEN(a1);
  a2 = ary_new_capa(mrb, len1 + blen);
  array_copy(ARY_PTR(a2), ARY_PTR(a1), len1);
  array_copy(ARY_PTR(a2) + len1, ptr, blen);
  ARY_SET_LEN(a2, len1+blen);

  return $obj_value(a2);
}

#define ARY_REPLACE_SHARED_MIN 20

static void
ary_replace($state *mrb, struct RArray *a, struct RArray *b)
{
  $int len = ARY_LEN(b);

  ary_modify_check(mrb, a);
  if (a == b) return;
  if (ARY_SHARED_P(a)) {
    $ary_decref(mrb, a->as.heap.aux.shared);
    a->as.heap.aux.capa = 0;
    a->as.heap.len = 0;
    a->as.heap.ptr = NULL;
    ARY_UNSET_SHARED_FLAG(a);
  }
  if (ARY_SHARED_P(b)) {
  shared_b:
    if (ARY_EMBED_P(a)) {
      ARY_UNSET_EMBED_FLAG(a);
    }
    else {
      $free(mrb, a->as.heap.ptr);
    }
    a->as.heap.ptr = b->as.heap.ptr;
    a->as.heap.len = len;
    a->as.heap.aux.shared = b->as.heap.aux.shared;
    a->as.heap.aux.shared->refcnt++;
    ARY_SET_SHARED_FLAG(a);
    $write_barrier(mrb, (struct RBasic*)a);
    return;
  }
  if (!$FROZEN_P(b) && len > ARY_REPLACE_SHARED_MIN) {
    ary_make_shared(mrb, b);
    goto shared_b;
  }
  if (ARY_CAPA(a) < len)
    ary_expand_capa(mrb, a, len);
  array_copy(ARY_PTR(a), ARY_PTR(b), len);
  $write_barrier(mrb, (struct RBasic*)a);
  ARY_SET_LEN(a, len);
}

$API void
$ary_replace($state *mrb, $value self, $value other)
{
  struct RArray *a1 = $ary_ptr(self);
  struct RArray *a2 = $ary_ptr(other);

  if (a1 != a2) {
    ary_replace(mrb, a1, a2);
  }
}

static $value
$ary_replace_m($state *mrb, $value self)
{
  $value other;

  $get_args(mrb, "A", &other);
  $ary_replace(mrb, self, other);

  return self;
}

static $value
$ary_times($state *mrb, $value self)
{
  struct RArray *a1 = $ary_ptr(self);
  struct RArray *a2;
  $value *ptr;
  $int times, len1;

  $get_args(mrb, "i", &times);
  if (times < 0) {
    $raise(mrb, E_ARGUMENT_ERROR, "negative argument");
  }
  if (times == 0) return $ary_new(mrb);
  if (ARY_MAX_SIZE / times < ARY_LEN(a1)) {
    $raise(mrb, E_ARGUMENT_ERROR, "array size too big");
  }
  len1 = ARY_LEN(a1);
  a2 = ary_new_capa(mrb, len1 * times);
  ARY_SET_LEN(a2, len1 * times);
  ptr = ARY_PTR(a2);
  while (times--) {
    array_copy(ptr, ARY_PTR(a1), len1);
    ptr += len1;
  }

  return $obj_value(a2);
}

static $value
$ary_reverse_bang($state *mrb, $value self)
{
  struct RArray *a = $ary_ptr(self);
  $int len = ARY_LEN(a);

  if (len > 1) {
    $value *p1, *p2;

    ary_modify(mrb, a);
    p1 = ARY_PTR(a);
    p2 = p1 + len - 1;

    while (p1 < p2) {
      $value tmp = *p1;
      *p1++ = *p2;
      *p2-- = tmp;
    }
  }
  return self;
}

static $value
$ary_reverse($state *mrb, $value self)
{
  struct RArray *a = $ary_ptr(self), *b = ary_new_capa(mrb, ARY_LEN(a));
  $int len = ARY_LEN(a);

  if (len > 0) {
    $value *p1, *p2, *e;

    p1 = ARY_PTR(a);
    e  = p1 + len;
    p2 = ARY_PTR(b) + len - 1;
    while (p1 < e) {
      *p2-- = *p1++;
    }
    ARY_SET_LEN(b, len);
  }
  return $obj_value(b);
}

$API void
$ary_push($state *mrb, $value ary, $value elem)
{
  struct RArray *a = $ary_ptr(ary);
  $int len = ARY_LEN(a);

  ary_modify(mrb, a);
  if (len == ARY_CAPA(a))
    ary_expand_capa(mrb, a, len + 1);
  ARY_PTR(a)[len] = elem;
  ARY_SET_LEN(a, len+1);
  $field_write_barrier_value(mrb, (struct RBasic*)a, elem);
}

static $value
$ary_push_m($state *mrb, $value self)
{
  $value *argv;
  $int len, len2, alen;
  struct RArray *a;

  $get_args(mrb, "*!", &argv, &alen);
  a = $ary_ptr(self);
  ary_modify(mrb, a);
  len = ARY_LEN(a);
  len2 = len + alen;
  if (ARY_CAPA(a) < len2) {
    ary_expand_capa(mrb, a, len2);
  }
  array_copy(ARY_PTR(a)+len, argv, alen);
  ARY_SET_LEN(a, len2);
  $write_barrier(mrb, (struct RBasic*)a);

  return self;
}

$API $value
$ary_pop($state *mrb, $value ary)
{
  struct RArray *a = $ary_ptr(ary);
  $int len = ARY_LEN(a);

  ary_modify_check(mrb, a);
  if (len == 0) return $nil_value();
  ARY_SET_LEN(a, len-1);
  return ARY_PTR(a)[len-1];
}

#define ARY_SHIFT_SHARED_MIN 10

$API $value
$ary_shift($state *mrb, $value self)
{
  struct RArray *a = $ary_ptr(self);
  $int len = ARY_LEN(a);
  $value val;

  ary_modify_check(mrb, a);
  if (len == 0) return $nil_value();
  if (ARY_SHARED_P(a)) {
  L_SHIFT:
    val = a->as.heap.ptr[0];
    a->as.heap.ptr++;
    a->as.heap.len--;
    return val;
  }
  if (len > ARY_SHIFT_SHARED_MIN) {
    ary_make_shared(mrb, a);
    goto L_SHIFT;
  }
  else {
    $value *ptr = ARY_PTR(a);
    $int size = len;

    val = *ptr;
    while (--size) {
      *ptr = *(ptr+1);
      ++ptr;
    }
    ARY_SET_LEN(a, len-1);
  }
  return val;
}

/* self = [1,2,3]
   item = 0
   self.unshift item
   p self #=> [0, 1, 2, 3] */
$API $value
$ary_unshift($state *mrb, $value self, $value item)
{
  struct RArray *a = $ary_ptr(self);
  $int len = ARY_LEN(a);

  if (ARY_SHARED_P(a)
      && a->as.heap.aux.shared->refcnt == 1 /* shared only referenced from this array */
      && a->as.heap.ptr - a->as.heap.aux.shared->ptr >= 1) /* there's room for unshifted item */ {
    a->as.heap.ptr--;
    a->as.heap.ptr[0] = item;
  }
  else {
    $value *ptr;

    ary_modify(mrb, a);
    if (ARY_CAPA(a) < len + 1)
      ary_expand_capa(mrb, a, len + 1);
    ptr = ARY_PTR(a);
    value_move(ptr + 1, ptr, len);
    ptr[0] = item;
  }
  ARY_SET_LEN(a, len+1);
  $field_write_barrier_value(mrb, (struct RBasic*)a, item);

  return self;
}

static $value
$ary_unshift_m($state *mrb, $value self)
{
  struct RArray *a = $ary_ptr(self);
  $value *vals, *ptr;
  $int alen, len;

  $get_args(mrb, "*!", &vals, &alen);
  if (alen == 0) {
    ary_modify_check(mrb, a);
    return self;
  }
  len = ARY_LEN(a);
  if (alen > ARY_MAX_SIZE - len) {
    $raise(mrb, E_ARGUMENT_ERROR, "array size too big");
  }
  if (ARY_SHARED_P(a)
      && a->as.heap.aux.shared->refcnt == 1 /* shared only referenced from this array */
      && a->as.heap.ptr - a->as.heap.aux.shared->ptr >= alen) /* there's room for unshifted item */ {
    ary_modify_check(mrb, a);
    a->as.heap.ptr -= alen;
    ptr = a->as.heap.ptr;
  }
  else {
    ary_modify(mrb, a);
    if (ARY_CAPA(a) < len + alen)
      ary_expand_capa(mrb, a, len + alen);
    ptr = ARY_PTR(a);
    value_move(ptr + alen, ptr, len);
  }
  array_copy(ptr, vals, alen);
  ARY_SET_LEN(a, len+alen);
  while (alen--) {
    $field_write_barrier_value(mrb, (struct RBasic*)a, vals[alen]);
  }

  return self;
}

$API $value
$ary_ref($state *mrb, $value ary, $int n)
{
  struct RArray *a = $ary_ptr(ary);
  $int len = ARY_LEN(a);

  /* range check */
  if (n < 0) n += len;
  if (n < 0 || len <= n) return $nil_value();

  return ARY_PTR(a)[n];
}

$API void
$ary_set($state *mrb, $value ary, $int n, $value val)
{
  struct RArray *a = $ary_ptr(ary);
  $int len = ARY_LEN(a);

  ary_modify(mrb, a);
  /* range check */
  if (n < 0) {
    n += len;
    if (n < 0) {
      $raisef(mrb, E_INDEX_ERROR, "index %S out of array", $fixnum_value(n - len));
    }
  }
  if (len <= n) {
    if (ARY_CAPA(a) <= n)
      ary_expand_capa(mrb, a, n + 1);
    ary_fill_with_nil(ARY_PTR(a) + len, n + 1 - len);
    ARY_SET_LEN(a, n+1);
  }

  ARY_PTR(a)[n] = val;
  $field_write_barrier_value(mrb, (struct RBasic*)a, val);
}

static struct RArray*
ary_dup($state *mrb, struct RArray *a)
{
  return ary_new_from_values(mrb, ARY_LEN(a), ARY_PTR(a));
}

$API $value
$ary_splice($state *mrb, $value ary, $int head, $int len, $value rpl)
{
  struct RArray *a = $ary_ptr(ary);
  $int alen = ARY_LEN(a);
  const $value *argv;
  $int argc;
  $int tail;

  ary_modify(mrb, a);

  /* len check */
  if (len < 0) $raisef(mrb, E_INDEX_ERROR, "negative length (%S)", $fixnum_value(len));

  /* range check */
  if (head < 0) {
    head += alen;
    if (head < 0) {
      $raise(mrb, E_INDEX_ERROR, "index is out of array");
    }
  }
  tail = head + len;
  if (alen < len || alen < tail) {
    len = alen - head;
  }

  /* size check */
  if ($array_p(rpl)) {
    argc = RARRAY_LEN(rpl);
    argv = RARRAY_PTR(rpl);
    if (argv == ARY_PTR(a)) {
      struct RArray *r;

      if (argc > 32767) {
        $raise(mrb, E_ARGUMENT_ERROR, "too big recursive splice");
      }
      r = ary_dup(mrb, a);
      argv = ARY_PTR(r);
    }
  }
  else {
    argc = 1;
    argv = &rpl;
  }
  if (head >= alen) {
    if (head > ARY_MAX_SIZE - argc) {
      $raisef(mrb, E_INDEX_ERROR, "index %S too big", $fixnum_value(head));
    }
    len = head + argc;
    if (len > ARY_CAPA(a)) {
      ary_expand_capa(mrb, a, head + argc);
    }
    ary_fill_with_nil(ARY_PTR(a) + alen, head - alen);
    if (argc > 0) {
      array_copy(ARY_PTR(a) + head, argv, argc);
    }
    ARY_SET_LEN(a, len);
  }
  else {
    $int newlen;

    if (alen - len > ARY_MAX_SIZE - argc) {
      $raisef(mrb, E_INDEX_ERROR, "index %S too big", $fixnum_value(alen + argc - len));
    }
    newlen = alen + argc - len;
    if (newlen > ARY_CAPA(a)) {
      ary_expand_capa(mrb, a, newlen);
    }

    if (len != argc) {
      $value *ptr = ARY_PTR(a);
      tail = head + len;
      value_move(ptr + head + argc, ptr + tail, alen - tail);
      ARY_SET_LEN(a, newlen);
    }
    if (argc > 0) {
      value_move(ARY_PTR(a) + head, argv, argc);
    }
  }
  $write_barrier(mrb, (struct RBasic*)a);
  return ary;
}

void
$ary_decref($state *mrb, $shared_array *shared)
{
  shared->refcnt--;
  if (shared->refcnt == 0) {
    $free(mrb, shared->ptr);
    $free(mrb, shared);
  }
}

static $value
ary_subseq($state *mrb, struct RArray *a, $int beg, $int len)
{
  struct RArray *b;

  if (!ARY_SHARED_P(a) && len <= ARY_SHIFT_SHARED_MIN) {
    return $ary_new_from_values(mrb, len, ARY_PTR(a)+beg);
  }
  ary_make_shared(mrb, a);
  b  = (struct RArray*)$obj_alloc(mrb, $TT_ARRAY, mrb->array_class);
  b->as.heap.ptr = a->as.heap.ptr + beg;
  b->as.heap.len = len;
  b->as.heap.aux.shared = a->as.heap.aux.shared;
  b->as.heap.aux.shared->refcnt++;
  ARY_SET_SHARED_FLAG(b);

  return $obj_value(b);
}

static $int
aget_index($state *mrb, $value index)
{
  if ($fixnum_p(index)) {
    return $fixnum(index);
  }
#ifndef $WITHOUT_FLOAT
  else if ($float_p(index)) {
    return ($int)$float(index);
  }
#endif
  else {
    $int i, argc;
    $value *argv;

    $get_args(mrb, "i*!", &i, &argv, &argc);
    return i;
  }
}

/*
 *  call-seq:
 *     ary[index]                -> obj     or nil
 *     ary[start, length]        -> new_ary or nil
 *     ary[range]                -> new_ary or nil
 *     ary.slice(index)          -> obj     or nil
 *     ary.slice(start, length)  -> new_ary or nil
 *     ary.slice(range)          -> new_ary or nil
 *
 *  Element Reference --- Returns the element at +index+, or returns a
 *  subarray starting at the +start+ index and continuing for +length+
 *  elements, or returns a subarray specified by +range+ of indices.
 *
 *  Negative indices count backward from the end of the array (-1 is the last
 *  element).  For +start+ and +range+ cases the starting index is just before
 *  an element.  Additionally, an empty array is returned when the starting
 *  index for an element range is at the end of the array.
 *
 *  Returns +nil+ if the index (or starting index) are out of range.
 *
 *  a = [ "a", "b", "c", "d", "e" ]
 *  a[1]     => "b"
 *  a[1,2]   => ["b", "c"]
 *  a[1..-2] => ["b", "c", "d"]
 *
 */

static $value
$ary_aget($state *mrb, $value self)
{
  struct RArray *a = $ary_ptr(self);
  $int i, len, alen;
  $value index;

  if ($get_args(mrb, "o|i", &index, &len) == 1) {
    switch ($type(index)) {
      /* a[n..m] */
    case $TT_RANGE:
      if ($range_beg_len(mrb, index, &i, &len, ARY_LEN(a), TRUE) == 1) {
        return ary_subseq(mrb, a, i, len);
      }
      else {
        return $nil_value();
      }
    case $TT_FIXNUM:
      return $ary_ref(mrb, self, $fixnum(index));
    default:
      return $ary_ref(mrb, self, aget_index(mrb, index));
    }
  }

  i = aget_index(mrb, index);
  alen = ARY_LEN(a);
  if (i < 0) i += alen;
  if (i < 0 || alen < i) return $nil_value();
  if (len < 0) return $nil_value();
  if (alen == i) return $ary_new(mrb);
  if (len > alen - i) len = alen - i;

  return ary_subseq(mrb, a, i, len);
}

/*
 *  call-seq:
 *     ary[index]         = obj                      ->  obj
 *     ary[start, length] = obj or other_ary or nil  ->  obj or other_ary or nil
 *     ary[range]         = obj or other_ary or nil  ->  obj or other_ary or nil
 *
 *  Element Assignment --- Sets the element at +index+, or replaces a subarray
 *  from the +start+ index for +length+ elements, or replaces a subarray
 *  specified by the +range+ of indices.
 *
 *  If indices are greater than the current capacity of the array, the array
 *  grows automatically.  Elements are inserted into the array at +start+ if
 *  +length+ is zero.
 *
 *  Negative indices will count backward from the end of the array.  For
 *  +start+ and +range+ cases the starting index is just before an element.
 *
 *  An IndexError is raised if a negative index points past the beginning of
 *  the array.
 *
 *  See also Array#push, and Array#unshift.
 *
 *     a = Array.new
 *     a[4] = "4";                 #=> [nil, nil, nil, nil, "4"]
 *     a[0, 3] = [ 'a', 'b', 'c' ] #=> ["a", "b", "c", nil, "4"]
 *     a[1..2] = [ 1, 2 ]          #=> ["a", 1, 2, nil, "4"]
 *     a[0, 2] = "?"               #=> ["?", 2, nil, "4"]
 *     a[0..2] = "A"               #=> ["A", "4"]
 *     a[-1]   = "Z"               #=> ["A", "Z"]
 *     a[1..-1] = nil              #=> ["A", nil]
 *     a[1..-1] = []               #=> ["A"]
 *     a[0, 0] = [ 1, 2 ]          #=> [1, 2, "A"]
 *     a[3, 0] = "B"               #=> [1, 2, "A", "B"]
 */

static $value
$ary_aset($state *mrb, $value self)
{
  $value v1, v2, v3;
  $int i, len;

  $ary_modify(mrb, $ary_ptr(self));
  if ($get_args(mrb, "oo|o", &v1, &v2, &v3) == 2) {
    /* a[n..m] = v */
    switch ($range_beg_len(mrb, v1, &i, &len, RARRAY_LEN(self), FALSE)) {
    case 0:                   /* not range */
      $ary_set(mrb, self, aget_index(mrb, v1), v2);
      break;
    case 1:                   /* range */
      $ary_splice(mrb, self, i, len, v2);
      break;
    case 2:                   /* out of range */
      $raisef(mrb, E_RANGE_ERROR, "%S out of range", v1);
      break;
    }
    return v2;
  }

  /* a[n,m] = v */
  $ary_splice(mrb, self, aget_index(mrb, v1), aget_index(mrb, v2), v3);
  return v3;
}

static $value
$ary_delete_at($state *mrb, $value self)
{
  struct RArray *a = $ary_ptr(self);
  $int   index;
  $value val;
  $value *ptr;
  $int len, alen;

  $get_args(mrb, "i", &index);
  alen = ARY_LEN(a);
  if (index < 0) index += alen;
  if (index < 0 || alen <= index) return $nil_value();

  ary_modify(mrb, a);
  ptr = ARY_PTR(a);
  val = ptr[index];

  ptr += index;
  len = alen - index;
  while (--len) {
    *ptr = *(ptr+1);
    ++ptr;
  }
  ARY_SET_LEN(a, alen-1);

  ary_shrink_capa(mrb, a);

  return val;
}

static $value
$ary_first($state *mrb, $value self)
{
  struct RArray *a = $ary_ptr(self);
  $int size, alen;

  if ($get_argc(mrb) == 0) {
    return (ARY_LEN(a) > 0)? ARY_PTR(a)[0]: $nil_value();
  }
  $get_args(mrb, "|i", &size);
  if (size < 0) {
    $raise(mrb, E_ARGUMENT_ERROR, "negative array size");
  }

  alen = ARY_LEN(a);
  if (size > alen) size = alen;
  if (ARY_SHARED_P(a)) {
    return ary_subseq(mrb, a, 0, size);
  }
  return $ary_new_from_values(mrb, size, ARY_PTR(a));
}

static $value
$ary_last($state *mrb, $value self)
{
  struct RArray *a = $ary_ptr(self);
  $int n, size, alen;

  n = $get_args(mrb, "|i", &size);
  alen = ARY_LEN(a);
  if (n == 0) {
    return (alen > 0) ? ARY_PTR(a)[alen - 1]: $nil_value();
  }

  if (size < 0) {
    $raise(mrb, E_ARGUMENT_ERROR, "negative array size");
  }
  if (size > alen) size = alen;
  if (ARY_SHARED_P(a) || size > ARY_DEFAULT_LEN) {
    return ary_subseq(mrb, a, alen - size, size);
  }
  return $ary_new_from_values(mrb, size, ARY_PTR(a) + alen - size);
}

static $value
$ary_index_m($state *mrb, $value self)
{
  $value obj;
  $int i;

  $get_args(mrb, "o", &obj);
  for (i = 0; i < RARRAY_LEN(self); i++) {
    if ($equal(mrb, RARRAY_PTR(self)[i], obj)) {
      return $fixnum_value(i);
    }
  }
  return $nil_value();
}

static $value
$ary_rindex_m($state *mrb, $value self)
{
  $value obj;
  $int i, len;

  $get_args(mrb, "o", &obj);
  for (i = RARRAY_LEN(self) - 1; i >= 0; i--) {
    if ($equal(mrb, RARRAY_PTR(self)[i], obj)) {
      return $fixnum_value(i);
    }
    if (i > (len = RARRAY_LEN(self))) {
      i = len;
    }
  }
  return $nil_value();
}

$API $value
$ary_splat($state *mrb, $value v)
{
  $value a, recv_class;

  if ($array_p(v)) {
    return v;
  }

  if (!$respond_to(mrb, v, $intern_lit(mrb, "to_a"))) {
    return $ary_new_from_values(mrb, 1, &v);
  }

  a = $funcall(mrb, v, "to_a", 0);
  if ($array_p(a)) {
    return a;
  }
  else if ($nil_p(a)) {
    return $ary_new_from_values(mrb, 1, &v);
  }
  else {
    recv_class = $obj_value($obj_class(mrb, v));
    $raisef(mrb, E_TYPE_ERROR, "can't convert %S to Array (%S#to_a gives %S)",
      recv_class,
      recv_class,
      $obj_value($obj_class(mrb, a))
    );
    /* not reached */
    return $undef_value();
  }
}

static $value
$ary_size($state *mrb, $value self)
{
  struct RArray *a = $ary_ptr(self);

  return $fixnum_value(ARY_LEN(a));
}

$API $value
$ary_clear($state *mrb, $value self)
{
  struct RArray *a = $ary_ptr(self);

  $get_args(mrb, "");
  ary_modify(mrb, a);
  if (ARY_SHARED_P(a)) {
    $ary_decref(mrb, a->as.heap.aux.shared);
    ARY_UNSET_SHARED_FLAG(a);
  }
  else if (!ARY_EMBED_P(a)){
    $free(mrb, a->as.heap.ptr);
  }
  ARY_SET_EMBED_LEN(a, 0);

  return self;
}

static $value
$ary_empty_p($state *mrb, $value self)
{
  struct RArray *a = $ary_ptr(self);

  return $bool_value(ARY_LEN(a) == 0);
}

$API $value
$check_array_type($state *mrb, $value ary)
{
  return $check_convert_type(mrb, ary, $TT_ARRAY, "Array", "to_ary");
}

$API $value
$ary_entry($value ary, $int offset)
{
  if (offset < 0) {
    offset += RARRAY_LEN(ary);
  }
  if (offset < 0 || RARRAY_LEN(ary) <= offset) {
    return $nil_value();
  }
  return RARRAY_PTR(ary)[offset];
}

static $value
join_ary($state *mrb, $value ary, $value sep, $value list)
{
  $int i;
  $value result, val, tmp;

  /* check recursive */
  for (i=0; i<RARRAY_LEN(list); i++) {
    if ($obj_equal(mrb, ary, RARRAY_PTR(list)[i])) {
      $raise(mrb, E_ARGUMENT_ERROR, "recursive array join");
    }
  }

  $ary_push(mrb, list, ary);

  result = $str_new_capa(mrb, 64);

  for (i=0; i<RARRAY_LEN(ary); i++) {
    if (i > 0 && !$nil_p(sep)) {
      $str_cat_str(mrb, result, sep);
    }

    val = RARRAY_PTR(ary)[i];
    switch ($type(val)) {
    case $TT_ARRAY:
    ary_join:
      val = join_ary(mrb, val, sep, list);
      /* fall through */

    case $TT_STRING:
    str_join:
      $str_cat_str(mrb, result, val);
      break;

    default:
      if (!$immediate_p(val)) {
        tmp = $check_string_type(mrb, val);
        if (!$nil_p(tmp)) {
          val = tmp;
          goto str_join;
        }
        tmp = $check_convert_type(mrb, val, $TT_ARRAY, "Array", "to_ary");
        if (!$nil_p(tmp)) {
          val = tmp;
          goto ary_join;
        }
      }
      val = $obj_as_string(mrb, val);
      goto str_join;
    }
  }

  $ary_pop(mrb, list);

  return result;
}

$API $value
$ary_join($state *mrb, $value ary, $value sep)
{
  if (!$nil_p(sep)) {
    sep = $obj_as_string(mrb, sep);
  }
  return join_ary(mrb, ary, sep, $ary_new(mrb));
}

/*
 *  call-seq:
 *     ary.join(sep="")    -> str
 *
 *  Returns a string created by converting each element of the array to
 *  a string, separated by <i>sep</i>.
 *
 *     [ "a", "b", "c" ].join        #=> "abc"
 *     [ "a", "b", "c" ].join("-")   #=> "a-b-c"
 */

static $value
$ary_join_m($state *mrb, $value ary)
{
  $value sep = $nil_value();

  $get_args(mrb, "|S!", &sep);
  return $ary_join(mrb, ary, sep);
}

static $value
$ary_eq($state *mrb, $value ary1)
{
  $value ary2;

  $get_args(mrb, "o", &ary2);
  if ($obj_equal(mrb, ary1, ary2)) return $true_value();
  if (!$array_p(ary2)) {
    return $false_value();
  }
  if (RARRAY_LEN(ary1) != RARRAY_LEN(ary2)) return $false_value();

  return ary2;
}

static $value
$ary_cmp($state *mrb, $value ary1)
{
  $value ary2;

  $get_args(mrb, "o", &ary2);
  if ($obj_equal(mrb, ary1, ary2)) return $fixnum_value(0);
  if (!$array_p(ary2)) {
    return $nil_value();
  }

  return ary2;
}

/* internal method to convert multi-value to single value */
static $value
$ary_svalue($state *mrb, $value ary)
{
  $get_args(mrb, "");
  switch (RARRAY_LEN(ary)) {
  case 0:
    return $nil_value();
  case 1:
    return RARRAY_PTR(ary)[0];
  default:
    return ary;
  }
}

void
$init_array($state *mrb)
{
  struct RClass *a;

  mrb->array_class = a = $define_class(mrb, "Array", mrb->object_class);            /* 15.2.12 */
  $SET_INSTANCE_TT(a, $TT_ARRAY);

  $define_class_method(mrb, a, "[]",        $ary_s_create,     $ARGS_ANY());  /* 15.2.12.4.1 */

  $define_method(mrb, a, "+",               $ary_plus,         $ARGS_REQ(1)); /* 15.2.12.5.1  */
  $define_method(mrb, a, "*",               $ary_times,        $ARGS_REQ(1)); /* 15.2.12.5.2  */
  $define_method(mrb, a, "<<",              $ary_push_m,       $ARGS_REQ(1)); /* 15.2.12.5.3  */
  $define_method(mrb, a, "[]",              $ary_aget,         $ARGS_ANY());  /* 15.2.12.5.4  */
  $define_method(mrb, a, "[]=",             $ary_aset,         $ARGS_ANY());  /* 15.2.12.5.5  */
  $define_method(mrb, a, "clear",           $ary_clear,        $ARGS_NONE()); /* 15.2.12.5.6  */
  $define_method(mrb, a, "concat",          $ary_concat_m,     $ARGS_REQ(1)); /* 15.2.12.5.8  */
  $define_method(mrb, a, "delete_at",       $ary_delete_at,    $ARGS_REQ(1)); /* 15.2.12.5.9  */
  $define_method(mrb, a, "empty?",          $ary_empty_p,      $ARGS_NONE()); /* 15.2.12.5.12 */
  $define_method(mrb, a, "first",           $ary_first,        $ARGS_OPT(1)); /* 15.2.12.5.13 */
  $define_method(mrb, a, "index",           $ary_index_m,      $ARGS_REQ(1)); /* 15.2.12.5.14 */
  $define_method(mrb, a, "initialize_copy", $ary_replace_m,    $ARGS_REQ(1)); /* 15.2.12.5.16 */
  $define_method(mrb, a, "join",            $ary_join_m,       $ARGS_ANY());  /* 15.2.12.5.17 */
  $define_method(mrb, a, "last",            $ary_last,         $ARGS_ANY());  /* 15.2.12.5.18 */
  $define_method(mrb, a, "length",          $ary_size,         $ARGS_NONE()); /* 15.2.12.5.19 */
  $define_method(mrb, a, "pop",             $ary_pop,          $ARGS_NONE()); /* 15.2.12.5.21 */
  $define_method(mrb, a, "push",            $ary_push_m,       $ARGS_ANY());  /* 15.2.12.5.22 */
  $define_method(mrb, a, "append",          $ary_push_m,       $ARGS_ANY());
  $define_method(mrb, a, "replace",         $ary_replace_m,    $ARGS_REQ(1)); /* 15.2.12.5.23 */
  $define_method(mrb, a, "reverse",         $ary_reverse,      $ARGS_NONE()); /* 15.2.12.5.24 */
  $define_method(mrb, a, "reverse!",        $ary_reverse_bang, $ARGS_NONE()); /* 15.2.12.5.25 */
  $define_method(mrb, a, "rindex",          $ary_rindex_m,     $ARGS_REQ(1)); /* 15.2.12.5.26 */
  $define_method(mrb, a, "shift",           $ary_shift,        $ARGS_NONE()); /* 15.2.12.5.27 */
  $define_method(mrb, a, "size",            $ary_size,         $ARGS_NONE()); /* 15.2.12.5.28 */
  $define_method(mrb, a, "slice",           $ary_aget,         $ARGS_ANY());  /* 15.2.12.5.29 */
  $define_method(mrb, a, "unshift",         $ary_unshift_m,    $ARGS_ANY());  /* 15.2.12.5.30 */
  $define_method(mrb, a, "prepend",         $ary_unshift_m,    $ARGS_ANY());

  $define_method(mrb, a, "__ary_eq",        $ary_eq,           $ARGS_REQ(1));
  $define_method(mrb, a, "__ary_cmp",       $ary_cmp,          $ARGS_REQ(1));
  $define_method(mrb, a, "__ary_index",     $ary_index_m,      $ARGS_REQ(1)); /* kept for mruby-array-ext */
  $define_method(mrb, a, "__svalue",        $ary_svalue,       $ARGS_NONE());
}
