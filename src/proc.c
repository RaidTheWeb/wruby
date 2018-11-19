/*
** proc.c - Proc class
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/class.h>
#include <mruby/proc.h>
#include <mruby/opcode.h>

static _code call_iseq[] = {
  OP_CALL,
};

struct RProc*
_proc_new(state *mrb, _irep *irep)
{
  struct RProc *p;
  _callinfo *ci = mrb->c->ci;

  p = (struct RProc*)_obj_alloc(mrb, MRB_TT_PROC, mrb->proc_class);
  if (ci) {
    struct RClass *tc = NULL;

    if (ci->proc) {
      tc = MRB_PROC_TARGET_CLASS(ci->proc);
    }
    if (tc == NULL) {
      tc = ci->target_class;
    }
    p->upper = ci->proc;
    p->e.target_class = tc;
  }
  p->body.irep = irep;
  _irep_incref(mrb, irep);

  return p;
}

static struct REnv*
env_new(state *mrb, _int nlocals)
{
  struct REnv *e;
  _callinfo *ci = mrb->c->ci;
  int bidx;

  e = (struct REnv*)_obj_alloc(mrb, MRB_TT_ENV, NULL);
  MRB_ENV_SET_STACK_LEN(e, nlocals);
  bidx = ci->argc;
  if (ci->argc < 0) bidx = 2;
  else bidx += 1;
  MRB_ENV_SET_BIDX(e, bidx);
  e->mid = ci->mid;
  e->stack = mrb->c->stack;
  e->cxt = mrb->c;

  return e;
}

static void
closure_setup(state *mrb, struct RProc *p)
{
  _callinfo *ci = mrb->c->ci;
  struct RProc *up = p->upper;
  struct REnv *e = NULL;

  if (ci && ci->env) {
    e = ci->env;
  }
  else if (up) {
    struct RClass *tc = MRB_PROC_TARGET_CLASS(p);

    e = env_new(mrb, up->body.irep->nlocals);
    ci->env = e;
    if (tc) {
      e->c = tc;
      _field_write_barrier(mrb, (struct RBasic*)e, (struct RBasic*)tc);
    }
  }
  if (e) {
    p->e.env = e;
    p->flags |= MRB_PROC_ENVSET;
    _field_write_barrier(mrb, (struct RBasic*)p, (struct RBasic*)e);
  }
}

struct RProc*
_closure_new(state *mrb, _irep *irep)
{
  struct RProc *p = _proc_new(mrb, irep);

  closure_setup(mrb, p);
  return p;
}

MRB_API struct RProc*
_proc_new_cfunc(state *mrb, _func_t func)
{
  struct RProc *p;

  p = (struct RProc*)_obj_alloc(mrb, MRB_TT_PROC, mrb->proc_class);
  p->body.func = func;
  p->flags |= MRB_PROC_CFUNC_FL;
  p->upper = 0;
  p->e.target_class = 0;

  return p;
}

MRB_API struct RProc*
_proc_new_cfunc_with_env(state *mrb, _func_t func, _int argc, const value *argv)
{
  struct RProc *p = _proc_new_cfunc(mrb, func);
  struct REnv *e;
  int i;

  p->e.env = e = env_new(mrb, argc);
  p->flags |= MRB_PROC_ENVSET;
  _field_write_barrier(mrb, (struct RBasic*)p, (struct RBasic*)e);
  MRB_ENV_UNSHARE_STACK(e);
  e->stack = (value*)_malloc(mrb, sizeof(value) * argc);
  if (argv) {
    for (i = 0; i < argc; ++i) {
      e->stack[i] = argv[i];
    }
  }
  else {
    for (i = 0; i < argc; ++i) {
      SET_NIL_VALUE(e->stack[i]);
    }
  }
  return p;
}

MRB_API struct RProc*
_closure_new_cfunc(state *mrb, _func_t func, int nlocals)
{
  return _proc_new_cfunc_with_env(mrb, func, nlocals, NULL);
}

MRB_API value
_proc_cfunc_env_get(state *mrb, _int idx)
{
  struct RProc *p = mrb->c->ci->proc;
  struct REnv *e;

  if (!p || !MRB_PROC_CFUNC_P(p)) {
    _raise(mrb, E_TYPE_ERROR, "Can't get cfunc env from non-cfunc proc.");
  }
  e = MRB_PROC_ENV(p);
  if (!e) {
    _raise(mrb, E_TYPE_ERROR, "Can't get cfunc env from cfunc Proc without REnv.");
  }
  if (idx < 0 || MRB_ENV_STACK_LEN(e) <= idx) {
    _raisef(mrb, E_INDEX_ERROR, "Env index out of range: %S (expected: 0 <= index < %S)",
               _fixnum_value(idx), _fixnum_value(MRB_ENV_STACK_LEN(e)));
  }

  return e->stack[idx];
}

void
_proc_copy(struct RProc *a, struct RProc *b)
{
  if (a->body.irep) {
    /* already initialized proc */
    return;
  }
  a->flags = b->flags;
  a->body = b->body;
  if (!MRB_PROC_CFUNC_P(a) && a->body.irep) {
    a->body.irep->refcnt++;
  }
  a->upper = b->upper;
  a->e.env = b->e.env;
  /* a->e.target_class = a->e.target_class; */
}

static value
_proc_s_new(state *mrb, value proc_class)
{
  value blk;
  value proc;
  struct RProc *p;

  _get_args(mrb, "&", &blk);
  if (_nil_p(blk)) {
    /* Calling Proc.new without a block is not implemented yet */
    _raise(mrb, E_ARGUMENT_ERROR, "tried to create Proc object without a block");
  }
  p = (struct RProc *)_obj_alloc(mrb, MRB_TT_PROC, _class_ptr(proc_class));
  _proc_copy(p, _proc_ptr(blk));
  proc = _obj_value(p);
  _funcall_with_block(mrb, proc, _intern_lit(mrb, "initialize"), 0, NULL, proc);
  if (!MRB_PROC_STRICT_P(p) &&
      mrb->c->ci > mrb->c->cibase && MRB_PROC_ENV(p) == mrb->c->ci[-1].env) {
    p->flags |= MRB_PROC_ORPHAN;
  }
  return proc;
}

static value
_proc_init_copy(state *mrb, value self)
{
  value proc;

  _get_args(mrb, "o", &proc);
  if (_type(proc) != MRB_TT_PROC) {
    _raise(mrb, E_ARGUMENT_ERROR, "not a proc");
  }
  _proc_copy(_proc_ptr(self), _proc_ptr(proc));
  return self;
}

int
_proc_cfunc_p(struct RProc *p)
{
  return MRB_PROC_CFUNC_P(p);
}

/* 15.2.17.4.2 */
static value
_proc_arity(state *mrb, value self)
{
  struct RProc *p = _proc_ptr(self);
  struct _irep *irep;
  _code *pc;
  _aspec aspec;
  int ma, op, ra, pa, arity;

  if (MRB_PROC_CFUNC_P(p)) {
    /* TODO cfunc aspec not implemented yet */
    return _fixnum_value(-1);
  }

  irep = p->body.irep;
  if (!irep) {
    return _fixnum_value(0);
  }

  pc = irep->iseq;
  /* arity is depend on OP_ENTER */
  if (*pc != OP_ENTER) {
    return _fixnum_value(0);
  }

  aspec = PEEK_W(pc+1);
  ma = MRB_ASPEC_REQ(aspec);
  op = MRB_ASPEC_OPT(aspec);
  ra = MRB_ASPEC_REST(aspec);
  pa = MRB_ASPEC_POST(aspec);
  arity = ra || (MRB_PROC_STRICT_P(p) && op) ? -(ma + pa + 1) : ma + pa;

  return _fixnum_value(arity);
}

/* 15.3.1.2.6  */
/* 15.3.1.3.27 */
/*
 * call-seq:
 *   lambda { |...| block }  -> a_proc
 *
 * Equivalent to <code>Proc.new</code>, except the resulting Proc objects
 * check the number of parameters passed when called.
 */
static value
proc_lambda(state *mrb, value self)
{
  value blk;
  struct RProc *p;

  _get_args(mrb, "&", &blk);
  if (_nil_p(blk)) {
    _raise(mrb, E_ARGUMENT_ERROR, "tried to create Proc object without a block");
  }
  if (_type(blk) != MRB_TT_PROC) {
    _raise(mrb, E_ARGUMENT_ERROR, "not a proc");
  }
  p = _proc_ptr(blk);
  if (!MRB_PROC_STRICT_P(p)) {
    struct RProc *p2 = (struct RProc*)_obj_alloc(mrb, MRB_TT_PROC, p->c);
    _proc_copy(p2, p);
    p2->flags |= MRB_PROC_STRICT;
    return _obj_value(p2);
  }
  return blk;
}

void
_init_proc(state *mrb)
{
  struct RProc *p;
  _method_t m;
  _irep *call_irep = (_irep *)_malloc(mrb, sizeof(_irep));
  static const _irep _irep_zero = { 0 };

  *call_irep = _irep_zero;
  call_irep->flags = MRB_ISEQ_NO_FREE;
  call_irep->iseq = call_iseq;
  call_irep->ilen = 1;
  call_irep->nregs = 2;         /* receiver and block */

  _define_class_method(mrb, mrb->proc_class, "new", _proc_s_new, MRB_ARGS_ANY());
  _define_method(mrb, mrb->proc_class, "initialize_copy", _proc_init_copy, MRB_ARGS_REQ(1));
  _define_method(mrb, mrb->proc_class, "arity", _proc_arity, MRB_ARGS_NONE());

  p = _proc_new(mrb, call_irep);
  MRB_METHOD_FROM_PROC(m, p);
  _define_method_raw(mrb, mrb->proc_class, _intern_lit(mrb, "call"), m);
  _define_method_raw(mrb, mrb->proc_class, _intern_lit(mrb, "[]"), m);

  _define_class_method(mrb, mrb->kernel_module, "lambda", proc_lambda, MRB_ARGS_NONE()); /* 15.3.1.2.6  */
  _define_method(mrb, mrb->kernel_module,       "lambda", proc_lambda, MRB_ARGS_NONE()); /* 15.3.1.3.27 */
}
