#include <mruby.h>
#include <mruby/proc.h>
#include <mruby/opcode.h>
#include <mruby/array.h>
#include <mruby/string.h>
#include <mruby/debug.h>

static _value
_proc_lambda(_state *mrb, _value self)
{
  struct RProc *p = _proc_ptr(self);
  return _bool_value(MRB_PROC_STRICT_P(p));
}

static _value
_proc_source_location(_state *mrb, _value self)
{
  struct RProc *p = _proc_ptr(self);

  if (MRB_PROC_CFUNC_P(p)) {
    return _nil_value();
  }
  else {
    _irep *irep = p->body.irep;
    int32_t line;
    const char *filename;

    filename = _debug_get_filename(irep, 0);
    line = _debug_get_line(irep, 0);

    return (!filename && line == -1)? _nil_value()
        : _assoc_new(mrb, _str_new_cstr(mrb, filename), _fixnum_value(line));
  }
}

static _value
_proc_inspect(_state *mrb, _value self)
{
  struct RProc *p = _proc_ptr(self);
  _value str = _str_new_lit(mrb, "#<Proc:");
  _str_concat(mrb, str, _ptr_to_str(mrb, _cptr(self)));

  if (!MRB_PROC_CFUNC_P(p)) {
    _irep *irep = p->body.irep;
    const char *filename;
    int32_t line;
    _str_cat_lit(mrb, str, "@");

    filename = _debug_get_filename(irep, 0);
    _str_cat_cstr(mrb, str, filename ? filename : "-");
    _str_cat_lit(mrb, str, ":");

    line = _debug_get_line(irep, 0);
    if (line != -1) {
      str = _format(mrb, "%S:%S", str, _fixnum_value(line));
    }
    else {
      _str_cat_lit(mrb, str, "-");
    }
  }

  if (MRB_PROC_STRICT_P(p)) {
    _str_cat_lit(mrb, str, " (lambda)");
  }

  _str_cat_lit(mrb, str, ">");
  return str;
}

static _value
_kernel_proc(_state *mrb, _value self)
{
  _value blk;

  _get_args(mrb, "&", &blk);
  if (_nil_p(blk)) {
    _raise(mrb, E_ARGUMENT_ERROR, "tried to create Proc object without a block");
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

static _value
_proc_parameters(_state *mrb, _value self)
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
  const struct RProc *proc = _proc_ptr(self);
  const struct _irep *irep = proc->body.irep;
  _aspec aspec;
  _value sname, parameters;
  int i, j;
  int max = -1;

  if (MRB_PROC_CFUNC_P(proc)) {
    // TODO cfunc aspec is not implemented yet
    return _ary_new(mrb);
  }
  if (!irep) {
    return _ary_new(mrb);
  }
  if (!irep->lv) {
    return _ary_new(mrb);
  }
  if (*irep->iseq != OP_ENTER) {
    return _ary_new(mrb);
  }

  if (!MRB_PROC_STRICT_P(proc)) {
    parameters_list[0].name = "opt";
    parameters_list[3].name = "opt";
  }

  aspec = PEEK_W(irep->iseq+1);
  parameters_list[0].size = MRB_ASPEC_REQ(aspec);
  parameters_list[1].size = MRB_ASPEC_OPT(aspec);
  parameters_list[2].size = MRB_ASPEC_REST(aspec);
  parameters_list[3].size = MRB_ASPEC_POST(aspec);
  parameters_list[4].size = MRB_ASPEC_BLOCK(aspec);

  parameters = _ary_new_capa(mrb, irep->nlocals-1);

  max = irep->nlocals-1;
  for (i = 0, p = parameters_list; p->name; p++) {
    if (p->size <= 0) continue;
    sname = _symbol_value(_intern_cstr(mrb, p->name));
    for (j = 0; j < p->size; i++, j++) {
      _value a;

      a = _ary_new(mrb);
      _ary_push(mrb, a, sname);
      if (i < max && irep->lv[i].name) {
        _sym sym = irep->lv[i].name;
        const char *name = _sym2name(mrb, sym);
        switch (name[0]) {
        case '*': case '&':
          break;
        default:
          _ary_push(mrb, a, _symbol_value(sym));
          break;
        }
      }
      _ary_push(mrb, parameters, a);
    }
  }
  return parameters;
}

void
_mruby_proc_ext_gem_init(_state* mrb)
{
  struct RClass *p = mrb->proc_class;
  _define_method(mrb, p, "lambda?",         _proc_lambda,          MRB_ARGS_NONE());
  _define_method(mrb, p, "source_location", _proc_source_location, MRB_ARGS_NONE());
  _define_method(mrb, p, "to_s",            _proc_inspect,         MRB_ARGS_NONE());
  _define_method(mrb, p, "inspect",         _proc_inspect,         MRB_ARGS_NONE());
  _define_method(mrb, p, "parameters",      _proc_parameters,      MRB_ARGS_NONE());

  _define_class_method(mrb, mrb->kernel_module, "proc", _kernel_proc, MRB_ARGS_NONE());
  _define_method(mrb, mrb->kernel_module,       "proc", _kernel_proc, MRB_ARGS_NONE());
}

void
_mruby_proc_ext_gem_final(_state* mrb)
{
}
