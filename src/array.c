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
#define ARY_C_MAX_SIZE (SIZE_MAX / sizeof(_value))
#define ARY_MAX_SIZE ((_int)((ARY_C_MAX_SIZE < (size_t)MRB_INT_MAX) ? ARY_C_MAX_SIZE : MRB_INT_MAX-1))

static struct RArray*
ary_new_capa(_state *mrb, _int capa)
{
  struct RArray *a;
  size_t blen;

  if (capa > ARY_MAX_SIZE) {
    _raise(mrb, E_ARGUMENT_ERROR, "array size too big");
  }
  blen = capa * sizeof(_value);

  a = (struct RArray*)_obj_alloc(mrb, MRB_TT_ARRAY, mrb->array_class);
  if (capa <= MRB_ARY_EMBED_LEN_MAX) {
    ARY_SET_EMBED_LEN(a, 0);
  }
  else {
    a->as.heap.ptr = (_value *)_malloc(mrb, blen);
    a->as.heap.aux.capa = capa;
    a->as.heap.len = 0;
  }

  return a;
}

MRB_API _value
_ary_new_capa(_state *mrb, _int capa)
{
  struct RArray *a = ary_new_capa(mrb, capa);
  return _obj_value(a);
}

MRB_API _value
_ary_new(_state *mrb)
{
  return _ary_new_capa(mrb, 0);
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
array_copy(_value *dst, const _value *src, _int size)
{
  _int i;

  for (i = 0; i < size; i++) {
    dst[i] = src[i];
  }
}

static struct RArray*
ary_new_from_values(_state *mrb, _int size, const _value *vals)
{
  struct RArray *a = ary_new_capa(mrb, size);

  array_copy(ARY_PTR(a), vals, size);
  ARY_SET_LEN(a, size);

  return a;
}

MRB_API _value
_ary_new_from_values(_state *mrb, _int size, const _value *vals)
{
  struct RArray *a = ary_new_from_values(mrb, size, vals);
  return _obj_value(a);
}

MRB_API _value
_assoc_new(_state *mrb, _value car, _value cdr)
{
  struct RArray *a;

  a = ary_new_capa(mrb, 2);
  ARY_PTR(a)[0] = car;
  ARY_PTR(a)[1] = cdr;
  ARY_SET_LEN(a, 2);
  return _obj_value(a);
}

static void
ary_fill_with_nil(_value *ptr, _int size)
{
  _value nil = _nil_value();

  while (size--) {
    *ptr++ = nil;
  }
}

static void
ary_modify_check(_state *mrb, struct RArray *a)
{
  if (MRB_FROZEN_P(a)) {
    _raise(mrb, E_FROZEN_ERROR, "can't modify frozen array");
  }
}

static void
ary_modify(_state *mrb, struct RArray *a)
{
  ary_modify_check(mrb, a);

  if (ARY_SHARED_P(a)) {
    _shared_array *shared = a->as.heap.aux.shared;

    if (shared->refcnt == 1 && a->as.heap.ptr == shared->ptr) {
      a->as.heap.ptr = shared->ptr;
      a->as.heap.aux.capa = a->as.heap.len;
      _free(mrb, shared);
    }
    else {
      _value *ptr, *p;
      _int len;

      p = a->as.heap.ptr;
      len = a->as.heap.len * sizeof(_value);
      ptr = (_value *)_malloc(mrb, len);
      if (p) {
        array_copy(ptr, p, a->as.heap.len);
      }
      a->as.heap.ptr = ptr;
      a->as.heap.aux.capa = a->as.heap.len;
      _ary_decref(mrb, shared);
    }
    ARY_UNSET_SHARED_FLAG(a);
  }
}

MRB_API void
_ary_modify(_state *mrb, struct RArray* a)
{
  _write_barrier(mrb, (struct RBasic*)a);
  ary_modify(mrb, a);
}

static void
ary_make_shared(_state *mrb, struct RArray *a)
{
  if (!ARY_SHARED_P(a) && !ARY_EMBED_P(a)) {
    _shared_array *shared = (_shared_array *)_malloc(mrb, sizeof(_shared_array));
    _value *ptr = a->as.heap.ptr;
    _int len = a->as.heap.len;

    shared->refcnt = 1;
    if (a->as.heap.aux.capa > len) {
      a->as.heap.ptr = shared->ptr = (_value *)_realloc(mrb, ptr, sizeof(_value)*len+1);
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
ary_expand_capa(_state *mrb, struct RArray *a, _int len)
{
  _int capa = ARY_CAPA(a);

  if (len > ARY_MAX_SIZE || len < 0) {
  size_error:
    _raise(mrb, E_ARGUMENT_ERROR, "array size too big");
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
    _value *ptr = ARY_EMBED_PTR(a);
    _int len = ARY_EMBED_LEN(a);
    _value *expanded_ptr = (_value *)_malloc(mrb, sizeof(_value)*capa);

    ARY_UNSET_EMBED_FLAG(a);
    array_copy(expanded_ptr, ptr, len);
    a->as.heap.len = len;
    a->as.heap.aux.capa = capa;
    a->as.heap.ptr = expanded_ptr;
  }
  else if (capa > a->as.heap.aux.capa) {
    _value *expanded_ptr = (_value *)_realloc(mrb, a->as.heap.ptr, sizeof(_value)*capa);

    a->as.heap.aux.capa = capa;
    a->as.heap.ptr = expanded_ptr;
  }
}

static void
ary_shrink_capa(_state *mrb, struct RArray *a)
{

  _int capa;

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
    a->as.heap.ptr = (_value *)_realloc(mrb, a->as.heap.ptr, sizeof(_value)*capa);
  }
}

MRB_API _value
_ary_resize(_state *mrb, _value ary, _int new_len)
{
  _int old_len;
  struct RArray *a = _ary_ptr(ary);

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

static _value
_ary_s_create(_state *mrb, _value klass)
{
  _value ary;
  _value *vals;
  _int len;
  struct RArray *a;

  _get_args(mrb, "*!", &vals, &len);
  ary = _ary_new_from_values(mrb, len, vals);
  a = _ary_ptr(ary);
  a->c = _class_ptr(klass);

  return ary;
}

static void ary_replace(_state*, struct RArray*, struct RArray*);

static void
ary_concat(_state *mrb, struct RArray *a, struct RArray *a2)
{
  _int len;

  if (ARY_LEN(a) == 0) {
    ary_replace(mrb, a, a2);
    return;
  }
  if (ARY_LEN(a2) > ARY_MAX_SIZE - ARY_LEN(a)) {
    _raise(mrb, E_ARGUMENT_ERROR, "array size too big");
  }
  len = ARY_LEN(a) + ARY_LEN(a2);

  ary_modify(mrb, a);
  if (ARY_CAPA(a) < len) {
    ary_expand_capa(mrb, a, len);
  }
  array_copy(ARY_PTR(a)+ARY_LEN(a), ARY_PTR(a2), ARY_LEN(a2));
  _write_barrier(mrb, (struct RBasic*)a);
  ARY_SET_LEN(a, len);
}

MRB_API void
_ary_concat(_state *mrb, _value self, _value other)
{
  struct RArray *a2 = _ary_ptr(other);

  ary_concat(mrb, _ary_ptr(self), a2);
}

static _value
_ary_concat_m(_state *mrb, _value self)
{
  _value ary;

  _get_args(mrb, "A", &ary);
  _ary_concat(mrb, self, ary);
  return self;
}

static _value
_ary_plus(_state *mrb, _value self)
{
  struct RArray *a1 = _ary_ptr(self);
  struct RArray *a2;
  _value *ptr;
  _int blen, len1;

  _get_args(mrb, "a", &ptr, &blen);
  if (ARY_MAX_SIZE - blen < ARY_LEN(a1)) {
    _raise(mrb, E_ARGUMENT_ERROR, "array size too big");
  }
  len1 = ARY_LEN(a1);
  a2 = ary_new_capa(mrb, len1 + blen);
  array_copy(ARY_PTR(a2), ARY_PTR(a1), len1);
  array_copy(ARY_PTR(a2) + len1, ptr, blen);
  ARY_SET_LEN(a2, len1+blen);

  return _obj_value(a2);
}

#define ARY_REPLACE_SHARED_MIN 20

static void
ary_replace(_state *mrb, struct RArray *a, struct RArray *b)
{
  _int len = ARY_LEN(b);

  ary_modify_check(mrb, a);
  if (a == b) return;
  if (ARY_SHARED_P(a)) {
    _ary_decref(mrb, a->as.heap.aux.shared);
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
      _free(mrb, a->as.heap.ptr);
    }
    a->as.heap.ptr = b->as.heap.ptr;
    a->as.heap.len = len;
    a->as.heap.aux.shared = b->as.heap.aux.shared;
    a->as.heap.aux.shared->refcnt++;
    ARY_SET_SHARED_FLAG(a);
    _write_barrier(mrb, (struct RBasic*)a);
    return;
  }
  if (!MRB_FROZEN_P(b) && len > ARY_REPLACE_SHARED_MIN) {
    ary_make_shared(mrb, b);
    goto shared_b;
  }
  if (ARY_CAPA(a) < len)
    ary_expand_capa(mrb, a, len);
  array_copy(ARY_PTR(a), ARY_PTR(b), len);
  _write_barrier(mrb, (struct RBasic*)a);
  ARY_SET_LEN(a, len);
}

MRB_API void
_ary_replace(_state *mrb, _value self, _value other)
{
  struct RArray *a1 = _ary_ptr(self);
  struct RArray *a2 = _ary_ptr(other);

  if (a1 != a2) {
    ary_replace(mrb, a1, a2);
  }
}

static _value
_ary_replace_m(_state *mrb, _value self)
{
  _value other;

  _get_args(mrb, "A", &other);
  _ary_replace(mrb, self, other);

  return self;
}

static _value
_ary_times(_state *mrb, _value self)
{
  struct RArray *a1 = _ary_ptr(self);
  struct RArray *a2;
  _value *ptr;
  _int times, len1;

  _get_args(mrb, "i", &times);
  if (times < 0) {
    _raise(mrb, E_ARGUMENT_ERROR, "negative argument");
  }
  if (times == 0) return _ary_new(mrb);
  if (ARY_MAX_SIZE / times < ARY_LEN(a1)) {
    _raise(mrb, E_ARGUMENT_ERROR, "array size too big");
  }
  len1 = ARY_LEN(a1);
  a2 = ary_new_capa(mrb, len1 * times);
  ARY_SET_LEN(a2, len1 * times);
  ptr = ARY_PTR(a2);
  while (times--) {
    array_copy(ptr, ARY_PTR(a1), len1);
    ptr += len1;
  }

  return _obj_value(a2);
}

static _value
_ary_reverse_bang(_state *mrb, _value self)
{
  struct RArray *a = _ary_ptr(self);
  _int len = ARY_LEN(a);

  if (len > 1) {
    _value *p1, *p2;

    ary_modify(mrb, a);
    p1 = ARY_PTR(a);
    p2 = p1 + len - 1;

    while (p1 < p2) {
      _value tmp = *p1;
      *p1++ = *p2;
      *p2-- = tmp;
    }
  }
  return self;
}

static _value
_ary_reverse(_state *mrb, _value self)
{
  struct RArray *a = _ary_ptr(self), *b = ary_new_capa(mrb, ARY_LEN(a));
  _int len = ARY_LEN(a);

  if (len > 0) {
    _value *p1, *p2, *e;

    p1 = ARY_PTR(a);
    e  = p1 + len;
    p2 = ARY_PTR(b) + len - 1;
    while (p1 < e) {
      *p2-- = *p1++;
    }
    ARY_SET_LEN(b, len);
  }
  return _obj_value(b);
}

MRB_API void
_ary_push(_state *mrb, _value ary, _value elem)
{
  struct RArray *a = _ary_ptr(ary);
  _int len = ARY_LEN(a);

  ary_modify(mrb, a);
  if (len == ARY_CAPA(a))
    ary_expand_capa(mrb, a, len + 1);
  ARY_PTR(a)[len] = elem;
  ARY_SET_LEN(a, len+1);
  _field_write_barrier_value(mrb, (struct RBasic*)a, elem);
}

static _value
_ary_push_m(_state *mrb, _value self)
{
  _value *argv;
  _int len, len2, alen;
  struct RArray *a;

  _get_args(mrb, "*!", &argv, &alen);
  a = _ary_ptr(self);
  ary_modify(mrb, a);
  len = ARY_LEN(a);
  len2 = len + alen;
  if (ARY_CAPA(a) < len2) {
    ary_expand_capa(mrb, a, len2);
  }
  array_copy(ARY_PTR(a)+len, argv, alen);
  ARY_SET_LEN(a, len2);
  _write_barrier(mrb, (struct RBasic*)a);

  return self;
}

MRB_API _value
_ary_pop(_state *mrb, _value ary)
{
  struct RArray *a = _ary_ptr(ary);
  _int len = ARY_LEN(a);

  ary_modify_check(mrb, a);
  if (len == 0) return _nil_value();
  ARY_SET_LEN(a, len-1);
  return ARY_PTR(a)[len-1];
}

#define ARY_SHIFT_SHARED_MIN 10

MRB_API _value
_ary_shift(_state *mrb, _value self)
{
  struct RArray *a = _ary_ptr(self);
  _int len = ARY_LEN(a);
  _value val;

  ary_modify_check(mrb, a);
  if (len == 0) return _nil_value();
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
    _value *ptr = ARY_PTR(a);
    _int size = len;

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
MRB_API _value
_ary_unshift(_state *mrb, _value self, _value item)
{
  struct RArray *a = _ary_ptr(self);
  _int len = ARY_LEN(a);

  if (ARY_SHARED_P(a)
      && a->as.heap.aux.shared->refcnt == 1 /* shared only referenced from this array */
      && a->as.heap.ptr - a->as.heap.aux.shared->ptr >= 1) /* there's room for unshifted item */ {
    a->as.heap.ptr--;
    a->as.heap.ptr[0] = item;
  }
  else {
    _value *ptr;

    ary_modify(mrb, a);
    if (ARY_CAPA(a) < len + 1)
      ary_expand_capa(mrb, a, len + 1);
    ptr = ARY_PTR(a);
    value_move(ptr + 1, ptr, len);
    ptr[0] = item;
  }
  ARY_SET_LEN(a, len+1);
  _field_write_barrier_value(mrb, (struct RBasic*)a, item);

  return self;
}

static _value
_ary_unshift_m(_state *mrb, _value self)
{
  struct RArray *a = _ary_ptr(self);
  _value *vals, *ptr;
  _int alen, len;

  _get_args(mrb, "*!", &vals, &alen);
  if (alen == 0) {
    ary_modify_check(mrb, a);
    return self;
  }
  len = ARY_LEN(a);
  if (alen > ARY_MAX_SIZE - len) {
    _raise(mrb, E_ARGUMENT_ERROR, "array size too big");
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
    _field_write_barrier_value(mrb, (struct RBasic*)a, vals[alen]);
  }

  return self;
}

MRB_API _value
_ary_ref(_state *mrb, _value ary, _int n)
{
  struct RArray *a = _ary_ptr(ary);
  _int len = ARY_LEN(a);

  /* range check */
  if (n < 0) n += len;
  if (n < 0 || len <= n) return _nil_value();

  return ARY_PTR(a)[n];
}

MRB_API void
_ary_set(_state *mrb, _value ary, _int n, _value val)
{
  struct RArray *a = _ary_ptr(ary);
  _int len = ARY_LEN(a);

  ary_modify(mrb, a);
  /* range check */
  if (n < 0) {
    n += len;
    if (n < 0) {
      _raisef(mrb, E_INDEX_ERROR, "index %S out of array", _fixnum_value(n - len));
    }
  }
  if (len <= n) {
    if (ARY_CAPA(a) <= n)
      ary_expand_capa(mrb, a, n + 1);
    ary_fill_with_nil(ARY_PTR(a) + len, n + 1 - len);
    ARY_SET_LEN(a, n+1);
  }

  ARY_PTR(a)[n] = val;
  _field_write_barrier_value(mrb, (struct RBasic*)a, val);
}

static struct RArray*
ary_dup(_state *mrb, struct RArray *a)
{
  return ary_new_from_values(mrb, ARY_LEN(a), ARY_PTR(a));
}

MRB_API _value
_ary_splice(_state *mrb, _value ary, _int head, _int len, _value rpl)
{
  struct RArray *a = _ary_ptr(ary);
  _int alen = ARY_LEN(a);
  const _value *argv;
  _int argc;
  _int tail;

  ary_modify(mrb, a);

  /* len check */
  if (len < 0) _raisef(mrb, E_INDEX_ERROR, "negative length (%S)", _fixnum_value(len));

  /* range check */
  if (head < 0) {
    head += alen;
    if (head < 0) {
      _raise(mrb, E_INDEX_ERROR, "index is out of array");
    }
  }
  tail = head + len;
  if (alen < len || alen < tail) {
    len = alen - head;
  }

  /* size check */
  if (_array_p(rpl)) {
    argc = RARRAY_LEN(rpl);
    argv = RARRAY_PTR(rpl);
    if (argv == ARY_PTR(a)) {
      struct RArray *r;

      if (argc > 32767) {
        _raise(mrb, E_ARGUMENT_ERROR, "too big recursive splice");
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
      _raisef(mrb, E_INDEX_ERROR, "index %S too big", _fixnum_value(head));
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
    _int newlen;

    if (alen - len > ARY_MAX_SIZE - argc) {
      _raisef(mrb, E_INDEX_ERROR, "index %S too big", _fixnum_value(alen + argc - len));
    }
    newlen = alen + argc - len;
    if (newlen > ARY_CAPA(a)) {
      ary_expand_capa(mrb, a, newlen);
    }

    if (len != argc) {
      _value *ptr = ARY_PTR(a);
      tail = head + len;
      value_move(ptr + head + argc, ptr + tail, alen - tail);
      ARY_SET_LEN(a, newlen);
    }
    if (argc > 0) {
      value_move(ARY_PTR(a) + head, argv, argc);
    }
  }
  _write_barrier(mrb, (struct RBasic*)a);
  return ary;
}

void
_ary_decref(_state *mrb, _shared_array *shared)
{
  shared->refcnt--;
  if (shared->refcnt == 0) {
    _free(mrb, shared->ptr);
    _free(mrb, shared);
  }
}

static _value
ary_subseq(_state *mrb, struct RArray *a, _int beg, _int len)
{
  struct RArray *b;

  if (!ARY_SHARED_P(a) && len <= ARY_SHIFT_SHARED_MIN) {
    return _ary_new_from_values(mrb, len, ARY_PTR(a)+beg);
  }
  ary_make_shared(mrb, a);
  b  = (struct RArray*)_obj_alloc(mrb, MRB_TT_ARRAY, mrb->array_class);
  b->as.heap.ptr = a->as.heap.ptr + beg;
  b->as.heap.len = len;
  b->as.heap.aux.shared = a->as.heap.aux.shared;
  b->as.heap.aux.shared->refcnt++;
  ARY_SET_SHARED_FLAG(b);

  return _obj_value(b);
}

static _int
aget_index(_state *mrb, _value index)
{
  if (_fixnum_p(index)) {
    return _fixnum(index);
  }
#ifndef MRB_WITHOUT_FLOAT
  else if (_float_p(index)) {
    return (_int)_float(index);
  }
#endif
  else {
    _int i, argc;
    _value *argv;

    _get_args(mrb, "i*!", &i, &argv, &argc);
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

static _value
_ary_aget(_state *mrb, _value self)
{
  struct RArray *a = _ary_ptr(self);
  _int i, len, alen;
  _value index;

  if (_get_args(mrb, "o|i", &index, &len) == 1) {
    switch (_type(index)) {
      /* a[n..m] */
    case MRB_TT_RANGE:
      if (_range_beg_len(mrb, index, &i, &len, ARY_LEN(a), TRUE) == 1) {
        return ary_subseq(mrb, a, i, len);
      }
      else {
        return _nil_value();
      }
    case MRB_TT_FIXNUM:
      return _ary_ref(mrb, self, _fixnum(index));
    default:
      return _ary_ref(mrb, self, aget_index(mrb, index));
    }
  }

  i = aget_index(mrb, index);
  alen = ARY_LEN(a);
  if (i < 0) i += alen;
  if (i < 0 || alen < i) return _nil_value();
  if (len < 0) return _nil_value();
  if (alen == i) return _ary_new(mrb);
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

static _value
_ary_aset(_state *mrb, _value self)
{
  _value v1, v2, v3;
  _int i, len;

  _ary_modify(mrb, _ary_ptr(self));
  if (_get_args(mrb, "oo|o", &v1, &v2, &v3) == 2) {
    /* a[n..m] = v */
    switch (_range_beg_len(mrb, v1, &i, &len, RARRAY_LEN(self), FALSE)) {
    case 0:                   /* not range */
      _ary_set(mrb, self, aget_index(mrb, v1), v2);
      break;
    case 1:                   /* range */
      _ary_splice(mrb, self, i, len, v2);
      break;
    case 2:                   /* out of range */
      _raisef(mrb, E_RANGE_ERROR, "%S out of range", v1);
      break;
    }
    return v2;
  }

  /* a[n,m] = v */
  _ary_splice(mrb, self, aget_index(mrb, v1), aget_index(mrb, v2), v3);
  return v3;
}

static _value
_ary_delete_at(_state *mrb, _value self)
{
  struct RArray *a = _ary_ptr(self);
  _int   index;
  _value val;
  _value *ptr;
  _int len, alen;

  _get_args(mrb, "i", &index);
  alen = ARY_LEN(a);
  if (index < 0) index += alen;
  if (index < 0 || alen <= index) return _nil_value();

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

static _value
_ary_first(_state *mrb, _value self)
{
  struct RArray *a = _ary_ptr(self);
  _int size, alen;

  if (_get_argc(mrb) == 0) {
    return (ARY_LEN(a) > 0)? ARY_PTR(a)[0]: _nil_value();
  }
  _get_args(mrb, "|i", &size);
  if (size < 0) {
    _raise(mrb, E_ARGUMENT_ERROR, "negative array size");
  }

  alen = ARY_LEN(a);
  if (size > alen) size = alen;
  if (ARY_SHARED_P(a)) {
    return ary_subseq(mrb, a, 0, size);
  }
  return _ary_new_from_values(mrb, size, ARY_PTR(a));
}

static _value
_ary_last(_state *mrb, _value self)
{
  struct RArray *a = _ary_ptr(self);
  _int n, size, alen;

  n = _get_args(mrb, "|i", &size);
  alen = ARY_LEN(a);
  if (n == 0) {
    return (alen > 0) ? ARY_PTR(a)[alen - 1]: _nil_value();
  }

  if (size < 0) {
    _raise(mrb, E_ARGUMENT_ERROR, "negative array size");
  }
  if (size > alen) size = alen;
  if (ARY_SHARED_P(a) || size > ARY_DEFAULT_LEN) {
    return ary_subseq(mrb, a, alen - size, size);
  }
  return _ary_new_from_values(mrb, size, ARY_PTR(a) + alen - size);
}

static _value
_ary_index_m(_state *mrb, _value self)
{
  _value obj;
  _int i;

  _get_args(mrb, "o", &obj);
  for (i = 0; i < RARRAY_LEN(self); i++) {
    if (_equal(mrb, RARRAY_PTR(self)[i], obj)) {
      return _fixnum_value(i);
    }
  }
  return _nil_value();
}

static _value
_ary_rindex_m(_state *mrb, _value self)
{
  _value obj;
  _int i, len;

  _get_args(mrb, "o", &obj);
  for (i = RARRAY_LEN(self) - 1; i >= 0; i--) {
    if (_equal(mrb, RARRAY_PTR(self)[i], obj)) {
      return _fixnum_value(i);
    }
    if (i > (len = RARRAY_LEN(self))) {
      i = len;
    }
  }
  return _nil_value();
}

MRB_API _value
_ary_splat(_state *mrb, _value v)
{
  _value a, recv_class;

  if (_array_p(v)) {
    return v;
  }

  if (!_respond_to(mrb, v, _intern_lit(mrb, "to_a"))) {
    return _ary_new_from_values(mrb, 1, &v);
  }

  a = _funcall(mrb, v, "to_a", 0);
  if (_array_p(a)) {
    return a;
  }
  else if (_nil_p(a)) {
    return _ary_new_from_values(mrb, 1, &v);
  }
  else {
    recv_class = _obj_value(_obj_class(mrb, v));
    _raisef(mrb, E_TYPE_ERROR, "can't convert %S to Array (%S#to_a gives %S)",
      recv_class,
      recv_class,
      _obj_value(_obj_class(mrb, a))
    );
    /* not reached */
    return _undef_value();
  }
}

static _value
_ary_size(_state *mrb, _value self)
{
  struct RArray *a = _ary_ptr(self);

  return _fixnum_value(ARY_LEN(a));
}

MRB_API _value
_ary_clear(_state *mrb, _value self)
{
  struct RArray *a = _ary_ptr(self);

  _get_args(mrb, "");
  ary_modify(mrb, a);
  if (ARY_SHARED_P(a)) {
    _ary_decref(mrb, a->as.heap.aux.shared);
    ARY_UNSET_SHARED_FLAG(a);
  }
  else if (!ARY_EMBED_P(a)){
    _free(mrb, a->as.heap.ptr);
  }
  ARY_SET_EMBED_LEN(a, 0);

  return self;
}

static _value
_ary_empty_p(_state *mrb, _value self)
{
  struct RArray *a = _ary_ptr(self);

  return _bool_value(ARY_LEN(a) == 0);
}

MRB_API _value
_check_array_type(_state *mrb, _value ary)
{
  return _check_convert_type(mrb, ary, MRB_TT_ARRAY, "Array", "to_ary");
}

MRB_API _value
_ary_entry(_value ary, _int offset)
{
  if (offset < 0) {
    offset += RARRAY_LEN(ary);
  }
  if (offset < 0 || RARRAY_LEN(ary) <= offset) {
    return _nil_value();
  }
  return RARRAY_PTR(ary)[offset];
}

static _value
join_ary(_state *mrb, _value ary, _value sep, _value list)
{
  _int i;
  _value result, val, tmp;

  /* check recursive */
  for (i=0; i<RARRAY_LEN(list); i++) {
    if (_obj_equal(mrb, ary, RARRAY_PTR(list)[i])) {
      _raise(mrb, E_ARGUMENT_ERROR, "recursive array join");
    }
  }

  _ary_push(mrb, list, ary);

  result = _str_new_capa(mrb, 64);

  for (i=0; i<RARRAY_LEN(ary); i++) {
    if (i > 0 && !_nil_p(sep)) {
      _str_cat_str(mrb, result, sep);
    }

    val = RARRAY_PTR(ary)[i];
    switch (_type(val)) {
    case MRB_TT_ARRAY:
    ary_join:
      val = join_ary(mrb, val, sep, list);
      /* fall through */

    case MRB_TT_STRING:
    str_join:
      _str_cat_str(mrb, result, val);
      break;

    default:
      if (!_immediate_p(val)) {
        tmp = _check_string_type(mrb, val);
        if (!_nil_p(tmp)) {
          val = tmp;
          goto str_join;
        }
        tmp = _check_convert_type(mrb, val, MRB_TT_ARRAY, "Array", "to_ary");
        if (!_nil_p(tmp)) {
          val = tmp;
          goto ary_join;
        }
      }
      val = _obj_as_string(mrb, val);
      goto str_join;
    }
  }

  _ary_pop(mrb, list);

  return result;
}

MRB_API _value
_ary_join(_state *mrb, _value ary, _value sep)
{
  if (!_nil_p(sep)) {
    sep = _obj_as_string(mrb, sep);
  }
  return join_ary(mrb, ary, sep, _ary_new(mrb));
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

static _value
_ary_join_m(_state *mrb, _value ary)
{
  _value sep = _nil_value();

  _get_args(mrb, "|S!", &sep);
  return _ary_join(mrb, ary, sep);
}

static _value
_ary_eq(_state *mrb, _value ary1)
{
  _value ary2;

  _get_args(mrb, "o", &ary2);
  if (_obj_equal(mrb, ary1, ary2)) return _true_value();
  if (!_array_p(ary2)) {
    return _false_value();
  }
  if (RARRAY_LEN(ary1) != RARRAY_LEN(ary2)) return _false_value();

  return ary2;
}

static _value
_ary_cmp(_state *mrb, _value ary1)
{
  _value ary2;

  _get_args(mrb, "o", &ary2);
  if (_obj_equal(mrb, ary1, ary2)) return _fixnum_value(0);
  if (!_array_p(ary2)) {
    return _nil_value();
  }

  return ary2;
}

/* internal method to convert multi-value to single value */
static _value
_ary_svalue(_state *mrb, _value ary)
{
  _get_args(mrb, "");
  switch (RARRAY_LEN(ary)) {
  case 0:
    return _nil_value();
  case 1:
    return RARRAY_PTR(ary)[0];
  default:
    return ary;
  }
}

void
_init_array(_state *mrb)
{
  struct RClass *a;

  mrb->array_class = a = _define_class(mrb, "Array", mrb->object_class);            /* 15.2.12 */
  MRB_SET_INSTANCE_TT(a, MRB_TT_ARRAY);

  _define_class_method(mrb, a, "[]",        _ary_s_create,     MRB_ARGS_ANY());  /* 15.2.12.4.1 */

  _define_method(mrb, a, "+",               _ary_plus,         MRB_ARGS_REQ(1)); /* 15.2.12.5.1  */
  _define_method(mrb, a, "*",               _ary_times,        MRB_ARGS_REQ(1)); /* 15.2.12.5.2  */
  _define_method(mrb, a, "<<",              _ary_push_m,       MRB_ARGS_REQ(1)); /* 15.2.12.5.3  */
  _define_method(mrb, a, "[]",              _ary_aget,         MRB_ARGS_ANY());  /* 15.2.12.5.4  */
  _define_method(mrb, a, "[]=",             _ary_aset,         MRB_ARGS_ANY());  /* 15.2.12.5.5  */
  _define_method(mrb, a, "clear",           _ary_clear,        MRB_ARGS_NONE()); /* 15.2.12.5.6  */
  _define_method(mrb, a, "concat",          _ary_concat_m,     MRB_ARGS_REQ(1)); /* 15.2.12.5.8  */
  _define_method(mrb, a, "delete_at",       _ary_delete_at,    MRB_ARGS_REQ(1)); /* 15.2.12.5.9  */
  _define_method(mrb, a, "empty?",          _ary_empty_p,      MRB_ARGS_NONE()); /* 15.2.12.5.12 */
  _define_method(mrb, a, "first",           _ary_first,        MRB_ARGS_OPT(1)); /* 15.2.12.5.13 */
  _define_method(mrb, a, "index",           _ary_index_m,      MRB_ARGS_REQ(1)); /* 15.2.12.5.14 */
  _define_method(mrb, a, "initialize_copy", _ary_replace_m,    MRB_ARGS_REQ(1)); /* 15.2.12.5.16 */
  _define_method(mrb, a, "join",            _ary_join_m,       MRB_ARGS_ANY());  /* 15.2.12.5.17 */
  _define_method(mrb, a, "last",            _ary_last,         MRB_ARGS_ANY());  /* 15.2.12.5.18 */
  _define_method(mrb, a, "length",          _ary_size,         MRB_ARGS_NONE()); /* 15.2.12.5.19 */
  _define_method(mrb, a, "pop",             _ary_pop,          MRB_ARGS_NONE()); /* 15.2.12.5.21 */
  _define_method(mrb, a, "push",            _ary_push_m,       MRB_ARGS_ANY());  /* 15.2.12.5.22 */
  _define_method(mrb, a, "append",          _ary_push_m,       MRB_ARGS_ANY());
  _define_method(mrb, a, "replace",         _ary_replace_m,    MRB_ARGS_REQ(1)); /* 15.2.12.5.23 */
  _define_method(mrb, a, "reverse",         _ary_reverse,      MRB_ARGS_NONE()); /* 15.2.12.5.24 */
  _define_method(mrb, a, "reverse!",        _ary_reverse_bang, MRB_ARGS_NONE()); /* 15.2.12.5.25 */
  _define_method(mrb, a, "rindex",          _ary_rindex_m,     MRB_ARGS_REQ(1)); /* 15.2.12.5.26 */
  _define_method(mrb, a, "shift",           _ary_shift,        MRB_ARGS_NONE()); /* 15.2.12.5.27 */
  _define_method(mrb, a, "size",            _ary_size,         MRB_ARGS_NONE()); /* 15.2.12.5.28 */
  _define_method(mrb, a, "slice",           _ary_aget,         MRB_ARGS_ANY());  /* 15.2.12.5.29 */
  _define_method(mrb, a, "unshift",         _ary_unshift_m,    MRB_ARGS_ANY());  /* 15.2.12.5.30 */
  _define_method(mrb, a, "prepend",         _ary_unshift_m,    MRB_ARGS_ANY());

  _define_method(mrb, a, "__ary_eq",        _ary_eq,           MRB_ARGS_REQ(1));
  _define_method(mrb, a, "__ary_cmp",       _ary_cmp,          MRB_ARGS_REQ(1));
  _define_method(mrb, a, "__ary_index",     _ary_index_m,      MRB_ARGS_REQ(1)); /* kept for mruby-array-ext */
  _define_method(mrb, a, "__svalue",        _ary_svalue,       MRB_ARGS_NONE());
}
