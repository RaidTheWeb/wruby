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
  sym method_id;
};

typedef void (*each_backtrace_func)(state*, struct backtrace_location*, void*);

API const data_type bt_type = { "Backtrace", free };

API void
each_backtrace(state *mrb, ptrdiff_t ciidx, code *pc0, each_backtrace_func func, void *data)
{
  ptrdiff_t i, j;

  if (ciidx >= mrb->c->ciend - mrb->c->cibase)
    ciidx = 10; /* ciidx is broken... */

  for (i=ciidx, j=0; i >= 0; i--,j++) {
    struct backtrace_location loc;
    callinfo *ci;
    irep *irep;
    code *pc;

    ci = &mrb->c->cibase[i];

    if (!ci->proc) continue;
    if (PROC_CFUNC_P(ci->proc)) continue;

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
    loc.filename = debug_get_filename(irep, pc - irep->iseq);
    loc.lineno = debug_get_line(irep, pc - irep->iseq);

    if (loc.lineno == -1) continue;

    if (!loc.filename) {
      loc.filename = "(unknown)";
    }

    loc.method_id = ci->mid;
    func(mrb, &loc, data);
  }
}

#ifndef DISABLE_STDIO

API void print_backtrace(state *mrb, value backtrace)
{
  int i;
  int n;
  FILE *stream = stderr;

  if (!array_p(backtrace)) return;

  n = RARRAY_LEN(backtrace) - 1;
  if (n == 0) return;

  fprintf(stream, "trace (most recent call last):\n");
  for (i=0; i<n; i++) {
    value entry = RARRAY_PTR(backtrace)[n-i-1];

    if (string_p(entry)) {
      fprintf(stream, "\t[%d] %.*s\n", i, (int)RSTRING_LEN(entry), RSTRING_PTR(entry));
    }
  }
}

API int
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

API void
print_packed_backtrace(state *mrb, value packed)
{
  FILE *stream = stderr;
  struct backtrace_location *bt;
  int n, i;
  int ai = gc_arena_save(mrb);

  bt = (struct backtrace_location*)data_check_get_ptr(mrb, packed, &bt_type);
  if (bt == NULL) return;
  n = (int)RDATA(packed)->flags;

  if (packed_bt_len(bt, n) == 0) return;
  fprintf(stream, "trace (most recent call last):\n");
  for (i = 0; i<n; i++) {
    struct backtrace_location *entry = &bt[n-i-1];
    if (entry->filename == NULL) continue;
    fprintf(stream, "\t[%d] %s:%d", i, entry->filename, entry->lineno);
    if (entry->method_id != 0) {
      const char *method_name;

      method_name = sym2name(mrb, entry->method_id);
      fprintf(stream, ":in %s", method_name);
      gc_arena_restore(mrb, ai);
    }
    fprintf(stream, "\n");
  }
}

/* print_backtrace

   function to retrieve backtrace information from the last exception.
*/

API void
print_backtrace_(state *mrb)
{
  value backtrace;

  if (!mrb->exc) {
    return;
  }

  backtrace = obj_iv_get(mrb, mrb->exc, intern_lit(mrb, "backtrace"));
  if (nil_p(backtrace)) return;
  if (array_p(backtrace)) {
    print_backtrace(mrb, backtrace);
  }
  else {
    print_packed_backtrace(mrb, backtrace);
  }
}
#else

API void
print_backtrace(state *mrb)
{
}

#endif

API void
count_backtrace_i(state *mrb,
                 struct backtrace_location *loc,
                 void *data)
{
  int *lenp = (int*)data;

  if (loc->filename == NULL) return;
  (*lenp)++;
}

API void
pack_backtrace_i(state *mrb,
                 struct backtrace_location *loc,
                 void *data)
{
  struct backtrace_location **pptr = (struct backtrace_location**)data;
  struct backtrace_location *ptr = *pptr;

  if (loc->filename == NULL) return;
  *ptr = *loc;
  *pptr = ptr+1;
}

API value
packed_backtrace(state *mrb)
{
  struct RData *backtrace;
  ptrdiff_t ciidx = mrb->c->ci - mrb->c->cibase;
  int len = 0;
  int size;
  void *ptr;

  each_backtrace(mrb, ciidx, mrb->c->ci->pc, count_backtrace_i, &len);
  size = len * sizeof(struct backtrace_location);
  ptr = malloc(mrb, size);
  if (ptr) memset(ptr, 0, size);
  backtrace = data_object_alloc(mrb, NULL, ptr, &bt_type);
  backtrace->flags = (unsigned int)len;
  each_backtrace(mrb, ciidx, mrb->c->ci->pc, pack_backtrace_i, &ptr);
  return obj_value(backtrace);
}

void
keep_backtrace(state *mrb, value exc)
{
  sym sym = intern_lit(mrb, "backtrace");
  value backtrace;
  int ai;

  if (iv_defined(mrb, exc, sym)) return;
  ai = gc_arena_save(mrb);
  backtrace = packed_backtrace(mrb);
  iv_set(mrb, exc, sym, backtrace);
  gc_arena_restore(mrb, ai);
}

value
unpack_backtrace(state *mrb, value backtrace)
{
  struct backtrace_location *bt;
  int n, i;
  int ai;

  if (nil_p(backtrace)) {
  empty_backtrace:
    return ary_new_capa(mrb, 0);
  }
  if (array_p(backtrace)) return backtrace;
  bt = (struct backtrace_location*)data_check_get_ptr(mrb, backtrace, &bt_type);
  if (bt == NULL) goto empty_backtrace;
  n = (int)RDATA(backtrace)->flags;
  backtrace = ary_new_capa(mrb, n);
  ai = gc_arena_save(mrb);
  for (i = 0; i < n; i++) {
    struct backtrace_location *entry = &bt[i];
    value btline;

    if (entry->filename == NULL) continue;
    btline = format(mrb, "%S:%S",
                              str_new_cstr(mrb, entry->filename),
                              fixnum_value(entry->lineno));
    if (entry->method_id != 0) {
      str_cat_lit(mrb, btline, ":in ");
      str_cat_cstr(mrb, btline, sym2name(mrb, entry->method_id));
    }
    ary_push(mrb, backtrace, btline);
    gc_arena_restore(mrb, ai);
  }

  return backtrace;
}

API value
exc_backtrace(state *mrb, value exc)
{
  sym attr_name;
  value backtrace;

  attr_name = intern_lit(mrb, "backtrace");
  backtrace = iv_get(mrb, exc, attr_name);
  if (nil_p(backtrace) || array_p(backtrace)) {
    return backtrace;
  }
  backtrace = unpack_backtrace(mrb, backtrace);
  iv_set(mrb, exc, attr_name, backtrace);
  return backtrace;
}

API value
get_backtrace(state *mrb)
{
  return unpack_backtrace(mrb, packed_backtrace(mrb));
}
