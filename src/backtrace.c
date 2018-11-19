/*
** backtrace.c -
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/variable.h>
#include <mruby/proc.h>
#include <mruby/array.h>
#include <mruby/string.h>
#include <mruby/class.h>
#include <mruby/debug.h>
#include <mruby/error.h>
#include <mruby/numeric.h>
#include <mruby/data.h>

struct backtrace_location {
  int lineno;
  const char *filename;
  _sym method_id;
};

typedef void (*each_backtrace_func)(_state*, struct backtrace_location*, void*);

static const _data_type bt_type = { "Backtrace", _free };

static void
each_backtrace(_state *mrb, ptrdiff_t ciidx, _code *pc0, each_backtrace_func func, void *data)
{
  ptrdiff_t i, j;

  if (ciidx >= mrb->c->ciend - mrb->c->cibase)
    ciidx = 10; /* ciidx is broken... */

  for (i=ciidx, j=0; i >= 0; i--,j++) {
    struct backtrace_location loc;
    _callinfo *ci;
    _irep *irep;
    _code *pc;

    ci = &mrb->c->cibase[i];

    if (!ci->proc) continue;
    if (MRB_PROC_CFUNC_P(ci->proc)) continue;

    irep = ci->proc->body.irep;
    if (!irep) continue;

    if (mrb->c->cibase[i].err) {
      pc = mrb->c->cibase[i].err;
    }
    else if (i+1 <= ciidx) {
      if (!mrb->c->cibase[i + 1].pc) continue;
      pc = &mrb->c->cibase[i+1].pc[-1];
    }
    else {
      pc = pc0;
    }
    loc.filename = _debug_get_filename(irep, pc - irep->iseq);
    loc.lineno = _debug_get_line(irep, pc - irep->iseq);

    if (loc.lineno == -1) continue;

    if (!loc.filename) {
      loc.filename = "(unknown)";
    }

    loc.method_id = ci->mid;
    func(mrb, &loc, data);
  }
}

#ifndef MRB_DISABLE_STDIO

static void
print_backtrace(_state *mrb, _value backtrace)
{
  int i;
  _int n;
  FILE *stream = stderr;

  if (!_array_p(backtrace)) return;

  n = RARRAY_LEN(backtrace) - 1;
  if (n == 0) return;

  fprintf(stream, "trace (most recent call last):\n");
  for (i=0; i<n; i++) {
    _value entry = RARRAY_PTR(backtrace)[n-i-1];

    if (_string_p(entry)) {
      fprintf(stream, "\t[%d] %.*s\n", i, (int)RSTRING_LEN(entry), RSTRING_PTR(entry));
    }
  }
}

static int
packed_bt_len(struct backtrace_location *bt, int n)
{
  int len = 0;
  int i;

  for (i=0; i<n; i++) {
    if (!bt[i].filename && !bt[i].lineno && !bt[i].method_id)
      continue;
    len++;
  }
  return len;
}

static void
print_packed_backtrace(_state *mrb, _value packed)
{
  FILE *stream = stderr;
  struct backtrace_location *bt;
  int n, i;
  int ai = _gc_arena_save(mrb);

  bt = (struct backtrace_location*)_data_check_get_ptr(mrb, packed, &bt_type);
  if (bt == NULL) return;
  n = (_int)RDATA(packed)->flags;

  if (packed_bt_len(bt, n) == 0) return;
  fprintf(stream, "trace (most recent call last):\n");
  for (i = 0; i<n; i++) {
    struct backtrace_location *entry = &bt[n-i-1];
    if (entry->filename == NULL) continue;
    fprintf(stream, "\t[%d] %s:%d", i, entry->filename, entry->lineno);
    if (entry->method_id != 0) {
      const char *method_name;

      method_name = _sym2name(mrb, entry->method_id);
      fprintf(stream, ":in %s", method_name);
      _gc_arena_restore(mrb, ai);
    }
    fprintf(stream, "\n");
  }
}

/* _print_backtrace

   function to retrieve backtrace information from the last exception.
*/

MRB_API void
_print_backtrace(_state *mrb)
{
  _value backtrace;

  if (!mrb->exc) {
    return;
  }

  backtrace = _obj_iv_get(mrb, mrb->exc, _intern_lit(mrb, "backtrace"));
  if (_nil_p(backtrace)) return;
  if (_array_p(backtrace)) {
    print_backtrace(mrb, backtrace);
  }
  else {
    print_packed_backtrace(mrb, backtrace);
  }
}
#else

MRB_API void
_print_backtrace(_state *mrb)
{
}

#endif

static void
count_backtrace_i(_state *mrb,
                 struct backtrace_location *loc,
                 void *data)
{
  int *lenp = (int*)data;

  if (loc->filename == NULL) return;
  (*lenp)++;
}

static void
pack_backtrace_i(_state *mrb,
                 struct backtrace_location *loc,
                 void *data)
{
  struct backtrace_location **pptr = (struct backtrace_location**)data;
  struct backtrace_location *ptr = *pptr;

  if (loc->filename == NULL) return;
  *ptr = *loc;
  *pptr = ptr+1;
}

static _value
packed_backtrace(_state *mrb)
{
  struct RData *backtrace;
  ptrdiff_t ciidx = mrb->c->ci - mrb->c->cibase;
  int len = 0;
  int size;
  void *ptr;

  each_backtrace(mrb, ciidx, mrb->c->ci->pc, count_backtrace_i, &len);
  size = len * sizeof(struct backtrace_location);
  ptr = _malloc(mrb, size);
  if (ptr) memset(ptr, 0, size);
  backtrace = _data_object_alloc(mrb, NULL, ptr, &bt_type);
  backtrace->flags = (unsigned int)len;
  each_backtrace(mrb, ciidx, mrb->c->ci->pc, pack_backtrace_i, &ptr);
  return _obj_value(backtrace);
}

void
_keep_backtrace(_state *mrb, _value exc)
{
  _sym sym = _intern_lit(mrb, "backtrace");
  _value backtrace;
  int ai;

  if (_iv_defined(mrb, exc, sym)) return;
  ai = _gc_arena_save(mrb);
  backtrace = packed_backtrace(mrb);
  _iv_set(mrb, exc, sym, backtrace);
  _gc_arena_restore(mrb, ai);
}

_value
_unpack_backtrace(_state *mrb, _value backtrace)
{
  struct backtrace_location *bt;
  _int n, i;
  int ai;

  if (_nil_p(backtrace)) {
  empty_backtrace:
    return _ary_new_capa(mrb, 0);
  }
  if (_array_p(backtrace)) return backtrace;
  bt = (struct backtrace_location*)_data_check_get_ptr(mrb, backtrace, &bt_type);
  if (bt == NULL) goto empty_backtrace;
  n = (_int)RDATA(backtrace)->flags;
  backtrace = _ary_new_capa(mrb, n);
  ai = _gc_arena_save(mrb);
  for (i = 0; i < n; i++) {
    struct backtrace_location *entry = &bt[i];
    _value btline;

    if (entry->filename == NULL) continue;
    btline = _format(mrb, "%S:%S",
                              _str_new_cstr(mrb, entry->filename),
                              _fixnum_value(entry->lineno));
    if (entry->method_id != 0) {
      _str_cat_lit(mrb, btline, ":in ");
      _str_cat_cstr(mrb, btline, _sym2name(mrb, entry->method_id));
    }
    _ary_push(mrb, backtrace, btline);
    _gc_arena_restore(mrb, ai);
  }

  return backtrace;
}

MRB_API _value
_exc_backtrace(_state *mrb, _value exc)
{
  _sym attr_name;
  _value backtrace;

  attr_name = _intern_lit(mrb, "backtrace");
  backtrace = _iv_get(mrb, exc, attr_name);
  if (_nil_p(backtrace) || _array_p(backtrace)) {
    return backtrace;
  }
  backtrace = _unpack_backtrace(mrb, backtrace);
  _iv_set(mrb, exc, attr_name, backtrace);
  return backtrace;
}

MRB_API _value
_get_backtrace(_state *mrb)
{
  return _unpack_backtrace(mrb, packed_backtrace(mrb));
}
