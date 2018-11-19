/*
** apiprint.c
**
*/

#include <string.h>
#include "mrdb.h"
#include <mruby/value.h>
#include <mruby/class.h>
#include <mruby/compile.h>
#include <mruby/error.h>
#include <mruby/numeric.h>
#include <mruby/string.h>
#include "apiprint.h"

static void
mrdb_check_syntax(_state *mrb, _debug_context *dbg, const char *expr, size_t len)
{
  mrbc_context *c;

  c = mrbc_context_new(mrb);
  c->no_exec = TRUE;
  c->capture_errors = TRUE;
  mrbc_filename(mrb, c, (const char*)dbg->prvfile);
  c->lineno = dbg->prvline;

  /* Load program */
  _load_nstring_cxt(mrb, expr, len, c);

  mrbc_context_free(mrb, c);
}

_value
_debug_eval(_state *mrb, _debug_context *dbg, const char *expr, size_t len, _bool *exc)
{
  void (*tmp)(struct _state *, struct _irep *, _code *, _value *);
  _value ruby_code;
  _value s;
  _value v;
  _value recv;

  /* disable code_fetch_hook */
  tmp = mrb->code_fetch_hook;
  mrb->code_fetch_hook = NULL;

  mrdb_check_syntax(mrb, dbg, expr, len);
  if (mrb->exc) {
    v = _obj_value(mrb->exc);
    mrb->exc = 0;
  }
  else {
    /*
     * begin
     *   expr
     * rescue => e
     *   e
     * end
     */
    ruby_code = _str_new_lit(mrb, "begin\n");
    ruby_code = _str_cat(mrb, ruby_code, expr, len);
    ruby_code = _str_cat_lit(mrb, ruby_code, "\nrescue => e\ne\nend");

    recv = dbg->regs[0];

    v =  _funcall(mrb, recv, "instance_eval", 1, ruby_code);
  }

  if (exc) {
    *exc = _obj_is_kind_of(mrb, v, mrb->eException_class);
  }

  s = _funcall(mrb, v, "inspect", 0);

  /* enable code_fetch_hook */
  mrb->code_fetch_hook = tmp;

  return s;
}
