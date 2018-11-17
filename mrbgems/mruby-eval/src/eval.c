#include <mruby.h>
#include <mruby/class.h>
#include <mruby/compile.h>
#include <mruby/irep.h>
#include <mruby/proc.h>
#include <mruby/opcode.h>
#include <mruby/error.h>

value exec_irep(state *mrb, value self, struct RProc *p);
value obj_instance_eval(state *mrb, value self);

static struct irep *
get_closure_irep(state *mrb, int level)
{
  struct RProc *proc = mrb->c->ci[-1].proc;

  while (level--) {
    if (!proc) return NULL;
    proc = proc->upper;
  }
  if (!proc) return NULL;
  if (PROC_CFUNC_P(proc)) {
    return NULL;
  }
  return proc->body.irep;
}

/* search for irep lev above the bottom */
static irep*
search_irep(irep *top, int bnest, int lev, irep *bottom)
{
  int i;

  for (i=0; i<top->rlen; i++) {
    irep* tmp = top->reps[i];

    if (tmp == bottom) return top;
    tmp = search_irep(tmp, bnest-1, lev, bottom);
    if (tmp) {
      if (bnest == lev) return top;
      return tmp;
    }
  }
  return NULL;
}

static uint16_t
search_variable(state *mrb, sym vsym, int bnest)
{
  irep *virep;
  int level;
  int pos;

  for (level = 0; (virep = get_closure_irep(mrb, level)); level++) {
    if (virep->lv == NULL) {
      continue;
    }
    for (pos = 0; pos < virep->nlocals - 1; pos++) {
      if (vsym == virep->lv[pos].name) {
        return (pos+1)<<8 | (level+bnest);
      }
    }
  }

  return 0;
}

static int
irep_argc(irep *irep)
{
  code c;

  c = irep->iseq[0];
  if (c == OP_ENTER) {
    aspec ax = PEEK_W(irep->iseq+1);
    /* extra 1 means a slot for block */
    return ASPEC_REQ(ax)+ASPEC_OPT(ax)+ASPEC_REST(ax)+ASPEC_POST(ax)+1;
  }
  return 0;
}

static bool
potential_upvar_p(struct locals *lv, uint16_t v, int argc, uint16_t nlocals)
{
  if (v >= nlocals) return FALSE;
  /* skip arguments  */
  if (v < argc+1) return FALSE;
  return TRUE;
}

extern uint8_t insn_size[];
extern uint8_t insn_size1[];
extern uint8_t insn_size2[];
extern uint8_t insn_size3[];

static void
patch_irep(state *mrb, irep *irep, int bnest, irep *top)
{
  int i;
  uint32_t a;
  uint16_t b;
  uint8_t c;
  code insn;
  int argc = irep_argc(irep);

  for (i = 0; i < irep->ilen; ) {
    insn = irep->iseq[i];
    switch(insn){
    case OP_EPUSH:
      b = PEEK_S(irep->iseq+i+1);
      patch_irep(mrb, irep->reps[b], bnest + 1, top);
      break;

    case OP_LAMBDA:
    case OP_BLOCK:
      a = PEEK_B(irep->iseq+i+1);
      b = PEEK_B(irep->iseq+i+2);
      patch_irep(mrb, irep->reps[b], bnest + 1, top);
      break;

    case OP_SEND:
      b = PEEK_B(irep->iseq+i+2);
      c = PEEK_B(irep->iseq+i+3);
      if (c != 0) {
        break;
      }
      else {
        uint16_t arg = search_variable(mrb, irep->syms[b], bnest);
        if (arg != 0) {
          /* must replace */
          irep->iseq[i] = OP_GETUPVAR;
          irep->iseq[i+2] = arg >> 8;
          irep->iseq[i+3] = arg & 0xff;
        }
      }
      break;

    case OP_MOVE:
      a = PEEK_B(irep->iseq+i+1);
      b = PEEK_B(irep->iseq+i+2);
      /* src part */
      if (potential_upvar_p(irep->lv, b, argc, irep->nlocals)) {
        uint16_t arg = search_variable(mrb, irep->lv[b - 1].name, bnest);
        if (arg != 0) {
          /* must replace */
          irep->iseq[i] = insn = OP_GETUPVAR;
          irep->iseq[i+2] = arg >> 8;
          irep->iseq[i+3] = arg & 0xff;
        }
      }
      /* dst part */
      if (potential_upvar_p(irep->lv, a, argc, irep->nlocals)) {
        uint16_t arg = search_variable(mrb, irep->lv[a - 1].name, bnest);
        if (arg != 0) {
          /* must replace */
          irep->iseq[i] = insn = OP_SETUPVAR;
          irep->iseq[i+1] = (code)b;
          irep->iseq[i+2] = arg >> 8;
          irep->iseq[i+3] = arg & 0xff;
        }
      }
      break;

    case OP_GETUPVAR:
      a = PEEK_B(irep->iseq+i+1);
      b = PEEK_B(irep->iseq+i+2);
      c = PEEK_B(irep->iseq+i+3);
      {
        int lev = c+1;
        irep *tmp = search_irep(top, bnest, lev, irep);
        if (potential_upvar_p(tmp->lv, b, irep_argc(tmp), tmp->nlocals)) {
          uint16_t arg = search_variable(mrb, tmp->lv[b-1].name, bnest);
          if (arg != 0) {
            /* must replace */
            irep->iseq[i] = OP_GETUPVAR;
            irep->iseq[i+2] = arg >> 8;
            irep->iseq[i+3] = arg & 0xff;
          }
        }
      }
      break;

    case OP_SETUPVAR:
      a = PEEK_B(irep->iseq+i+1);
      b = PEEK_B(irep->iseq+i+2);
      c = PEEK_B(irep->iseq+i+3);
      {
        int lev = c+1;
        irep *tmp = search_irep(top, bnest, lev, irep);
        if (potential_upvar_p(tmp->lv, b, irep_argc(tmp), tmp->nlocals)) {
          uint16_t arg = search_variable(mrb, tmp->lv[b-1].name, bnest);
          if (arg != 0) {
            /* must replace */
            irep->iseq[i] = OP_SETUPVAR;
            irep->iseq[i+1] = a;
            irep->iseq[i+2] = arg >> 8;
            irep->iseq[i+3] = arg & 0xff;
          }
        }
      }
      break;

    case OP_EXT1:
      insn = PEEK_B(irep->iseq+i+1);
      i += insn_size1[insn]+1;
      continue;
    case OP_EXT2:
      insn = PEEK_B(irep->iseq+i+1);
      i += insn_size2[insn]+1;
      continue;
    case OP_EXT3:
      insn = PEEK_B(irep->iseq+i+1);
      i += insn_size3[insn]+1;
      continue;
    }
    i+=insn_size[insn];
  }
}

void codedump_all(state*, struct RProc*);

static struct RProc*
create_proc_from_string(state *mrb, char *s, int len, value binding, const char *file, int line)
{
  mrbc_context *cxt;
  struct parser_state *p;
  struct RProc *proc;
  struct REnv *e;
  callinfo *ci; /* callinfo of eval caller */
  struct RClass *target_class = NULL;
  int bidx;

  if (!nil_p(binding)) {
    raise(mrb, E_ARGUMENT_ERROR, "Binding of eval must be nil.");
  }

  cxt = mrbc_context_new(mrb);
  cxt->lineno = (short)line;

  mrbc_filename(mrb, cxt, file ? file : "(eval)");
  cxt->capture_errors = TRUE;
  cxt->no_optimize = TRUE;
  cxt->on_eval = TRUE;

  p = parse_nstring(mrb, s, len, cxt);

  /* only occur when memory ran out */
  if (!p) {
    raise(mrb, E_RUNTIME_ERROR, "Failed to create parser state.");
  }

  if (0 < p->nerr) {
    /* parse error */
    value str;

    if (file) {
      str = format(mrb, " file %S line %S: %S",
                       str_new_cstr(mrb, file),
                       fixnum_value(p->error_buffer[0].lineno),
                       str_new_cstr(mrb, p->error_buffer[0].message));
    }
    else {
      str = format(mrb, " line %S: %S",
                       fixnum_value(p->error_buffer[0].lineno),
                       str_new_cstr(mrb, p->error_buffer[0].message));
    }
    parser_free(p);
    mrbc_context_free(mrb, cxt);
    exc_raise(mrb, exc_new_str(mrb, E_SYNTAX_ERROR, str));
  }

  proc = generate_code(mrb, p);
  if (proc == NULL) {
    /* codegen error */
    parser_free(p);
    mrbc_context_free(mrb, cxt);
    raise(mrb, E_SCRIPT_ERROR, "codegen error");
  }
  if (mrb->c->ci > mrb->c->cibase) {
    ci = &mrb->c->ci[-1];
  }
  else {
    ci = mrb->c->cibase;
  }
  if (ci->proc) {
    target_class = PROC_TARGET_CLASS(ci->proc);
  }
  if (ci->proc && !PROC_CFUNC_P(ci->proc)) {
    if (ci->env) {
      e = ci->env;
    }
    else {
      e = (struct REnv*)obj_alloc(mrb, TT_ENV,
                                      (struct RClass*)target_class);
      e->mid = ci->mid;
      e->stack = ci[1].stackent;
      e->cxt = mrb->c;
      ENV_SET_STACK_LEN(e, ci->proc->body.irep->nlocals);
      bidx = ci->argc;
      if (ci->argc < 0) bidx = 2;
      else bidx += 1;
      ENV_SET_BIDX(e, bidx);
      ci->env = e;
    }
    proc->e.env = e;
    proc->flags |= PROC_ENVSET;
    field_write_barrier(mrb, (struct RBasic*)proc, (struct RBasic*)e);
  }
  proc->upper = ci->proc;
  mrb->c->ci->target_class = target_class;
  patch_irep(mrb, proc->body.irep, 0, proc->body.irep);
  /* codedump_all(mrb, proc); */

  parser_free(p);
  mrbc_context_free(mrb, cxt);

  return proc;
}

static value
exec_irep(state *mrb, value self, struct RProc *proc)
{
  /* no argument passed from eval() */
  mrb->c->ci->argc = 0;
  if (mrb->c->ci->acc < 0) {
    ptrdiff_t cioff = mrb->c->ci - mrb->c->cibase;
    value ret = top_run(mrb, proc, self, 0);
    if (mrb->exc) {
      exc_raise(mrb, obj_value(mrb->exc));
    }
    mrb->c->ci = mrb->c->cibase + cioff;
    return ret;
  }
  /* clear block */
  mrb->c->stack[1] = nil_value();
  return exec_irep(mrb, self, proc);
}

static value
f_eval(state *mrb, value self)
{
  char *s;
  int len;
  value binding = nil_value();
  char *file = NULL;
  int line = 1;
  struct RProc *proc;

  get_args(mrb, "s|ozi", &s, &len, &binding, &file, &line);

  proc = create_proc_from_string(mrb, s, len, binding, file, line);
  assert(!PROC_CFUNC_P(proc));
  return exec_irep(mrb, self, proc);
}

static value
f_instance_eval(state *mrb, value self)
{
  value b;
  int argc; value *argv;

  get_args(mrb, "*!&", &argv, &argc, &b);

  if (nil_p(b)) {
    char *s;
    int len;
    char *file = NULL;
    int line = 1;
    value cv;
    struct RProc *proc;

    get_args(mrb, "s|zi", &s, &len, &file, &line);
    cv = singleton_class(mrb, self);
    proc = create_proc_from_string(mrb, s, len, nil_value(), file, line);
    PROC_SET_TARGET_CLASS(proc, class_ptr(cv));
    assert(!PROC_CFUNC_P(proc));
    mrb->c->ci->target_class = class_ptr(cv);
    return exec_irep(mrb, self, proc);
  }
  else {
    get_args(mrb, "&", &b);
    return obj_instance_eval(mrb, self);
  }
}

void
mruby_eval_gem_init(state* mrb)
{
  define_module_function(mrb, mrb->kernel_module, "eval", f_eval, ARGS_ARG(1, 3));
  define_method(mrb, mrb->kernel_module, "instance_eval", f_instance_eval, ARGS_ARG(1, 2));
}

void
mruby_eval_gem_final(state* mrb)
{
}
