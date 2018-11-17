#include <mruby.h>
#include <mruby/proc.h>
#include <mruby/opcode.h>
#include <mruby/array.h>
#include <mruby/string.h>
#include <mruby/debug.h>

static value
proc_lambda(state *mrb, value self)
{
  struct RProc *p = proc_ptr(self);
  return bool_value(PROC_STRICT_P(p));
}

static value
proc_source_location(state *mrb, value self)
{
  struct RProc *p = proc_ptr(self);

  if (PROC_CFUNC_P(p)) {
    return nil_value();
  }
  else {
    irep *irep = p->body.irep;
    int32_t line;
    const char *filename;

    filename = debug_get_filename(irep, 0);
    line = debug_get_line(irep, 0);

    return (!filename && line == -1)? nil_value()
        : assoc_new(mrb, str_new_cstr(mrb, filename), fixnum_value(line));
  }
}

static value
proc_inspect(state *mrb, value self)
{
  struct RProc *p = proc_ptr(self);
  value str = str_new_lit(mrb, "#<Proc:");
  str_concat(mrb, str, ptr_to_str(mrb, cptr(self)));

  if (!PROC_CFUNC_P(p)) {
    irep *irep = p->body.irep;
    const char *filename;
    int32_t line;
    str_cat_lit(mrb, str, "@");

    filename = debug_get_filename(irep, 0);
    str_cat_cstr(mrb, str, filename ? filename : "-");
    str_cat_lit(mrb, str, ":");

    line = debug_get_line(irep, 0);
    if (line != -1) {
      str = format(mrb, "%S:%S", str, fixnum_value(line));
    }
    else {
      str_cat_lit(mrb, str, "-");
    }
  }

  if (PROC_STRICT_P(p)) {
    str_cat_lit(mrb, str, " (lambda)");
  }

  str_cat_lit(mrb, str, ">");
  return str;
}

static value
kernel_proc(state *mrb, value self)
{
  value blk;

  get_args(mrb, "&", &blk);
  if (nil_p(blk)) {
    raise(mrb, E_ARGUMENT_ERROR, "tried to create Proc object without a block");
  }

  return blk;
}

/*
 * call-seq:
 *    prc.parameters  -> array
 *
 * Returns the parameter information of this proc.
 *
 *    prc = lambda{|x, y=42, *other|}
 *    prc.parameters  #=> [[:req, :x], [:opt, :y], [:rest, :other]]
 */

static value
proc_parameters(state *mrb, value self)
{
  struct parameters_type {
    int size;
    const char *name;
  } *p, parameters_list [] = {
    {0, "req"},
    {0, "opt"},
    {0, "rest"},
    {0, "req"},
    {0, "block"},
    {0, NULL}
  };
  const struct RProc *proc = proc_ptr(self);
  const struct irep *irep = proc->body.irep;
  aspec aspec;
  value sname, parameters;
  int i, j;
  int max = -1;

  if (PROC_CFUNC_P(proc)) {
    // TODO cfunc aspec is not implemented yet
    return ary_new(mrb);
  }
  if (!irep) {
    return ary_new(mrb);
  }
  if (!irep->lv) {
    return ary_new(mrb);
  }
  if (*irep->iseq != OP_ENTER) {
    return ary_new(mrb);
  }

  if (!PROC_STRICT_P(proc)) {
    parameters_list[0].name = "opt";
    parameters_list[3].name = "opt";
  }

  aspec = PEEK_W(irep->iseq+1);
  parameters_list[0].size = ASPEC_REQ(aspec);
  parameters_list[1].size = ASPEC_OPT(aspec);
  parameters_list[2].size = ASPEC_REST(aspec);
  parameters_list[3].size = ASPEC_POST(aspec);
  parameters_list[4].size = ASPEC_BLOCK(aspec);

  parameters = ary_new_capa(mrb, irep->nlocals-1);

  max = irep->nlocals-1;
  for (i = 0, p = parameters_list; p->name; p++) {
    if (p->size <= 0) continue;
    sname = symbol_value(intern_cstr(mrb, p->name));
    for (j = 0; j < p->size; i++, j++) {
      value a;

      a = ary_new(mrb);
      ary_push(mrb, a, sname);
      if (i < max && irep->lv[i].name) {
        sym sym = irep->lv[i].name;
        const char *name = sym2name(mrb, sym);
        switch (name[0]) {
        case '*': case '&':
          break;
        default:
          ary_push(mrb, a, symbol_value(sym));
          break;
        }
      }
      ary_push(mrb, parameters, a);
    }
  }
  return parameters;
}

void
mruby_proc_ext_gem_init(state* mrb)
{
  struct RClass *p = mrb->proc_class;
  define_method(mrb, p, "lambda?",         proc_lambda,          ARGS_NONE());
  define_method(mrb, p, "source_location", proc_source_location, ARGS_NONE());
  define_method(mrb, p, "to_s",            proc_inspect,         ARGS_NONE());
  define_method(mrb, p, "inspect",         proc_inspect,         ARGS_NONE());
  define_method(mrb, p, "parameters",      proc_parameters,      ARGS_NONE());

  define_class_method(mrb, mrb->kernel_module, "proc", kernel_proc, ARGS_NONE());
  define_method(mrb, mrb->kernel_module,       "proc", kernel_proc, ARGS_NONE());
}

void
mruby_proc_ext_gem_final(state* mrb)
{
}
