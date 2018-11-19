/*
** proc.c - Proc class
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/class.h>
#include <mruby/proc.h>
#include <mruby/opcode.h>

static $code call_iseq[] = {
  OP_CALL,
};

struct RProc*
$proc_new($state *mrb, $irep *irep)
{
  struct RProc *p;
  $callinfo *ci = mrb->c->ci;

  p = (struct RProc*)$obj_alloc(mrb, $TT_PROC, mrb->proc_class);
  if (ci) {
    struct RClass *tc = NULL;

    if (ci->proc) {
      tc = $PROC_TARGET_CLASS(ci->proc);
    }
    if (tc == NULL) {
      tc = ci->target_class;
    }
    p->upper = ci->proc;
    p->e.target_class = tc;
  }
  p->body.irep = irep;
  $irep_incref(mrb, irep);

  return p;
}

static struct REnv*
env_new($state *mrb, $int nlocals)
{
  struct REnv *e;
  $callinfo *ci = mrb->c->ci;
  int bidx;

  e = (struct REnv*)$obj_alloc(mrb, $TT_ENV, NULL);
  $ENV_SET_STACK_LEN(e, nlocals);
  bidx = ci->argc;
  if (ci->argc < 0) bidx = 2;
  else bidx += 1;
  $ENV_SET_BIDX(e, bidx);
  e->mid = ci->mid;
  e->stack = mrb->c->stack;
  e->cxt = mrb->c;

  return e;
}

static void
closure_setup($state *mrb, struct RProc *p)
{
  $callinfo *ci = mrb->c->ci;
  struct RProc *up = p->upper;
  struct REnv *e = NULL;

  if (ci && ci->env) {
    e = ci->env;
  }
  else if (up) {
    struct RClass *tc = $PROC_TARGET_CLASS(p);

    e = env_new(mrb, up->body.irep->nlocals);
    ci->env = e;
    if (tc) {
      e->c = tc;
      $field_write_barrier(mrb, (struct RBasic*)e, (struct RBasic*)tc);
    }
  }
  if (e) {
    p->e.env = e;
    p->flags |= $PROC_ENVSET;
    $field_write_barrier(mrb, (struct RBasic*)p, (struct RBasic*)e);
  }
}

struct RProc*
$closure_new($state *mrb, $irep *irep)
{
  struct RProc *p = $proc_new(mrb, irep);

  closure_setup(mrb, p);
  return p;
}

$API struct RProc*
$proc_new_cfunc($state *mrb, $func_t func)
{
  struct RProc *p;

  p = (struct RProc*)$obj_alloc(mrb, $TT_PROC, mrb->proc_class);
  p->body.func = func;
  p->flags |= $PROC_CFUNC_FL;
  p->upper = 0;
  p->e.target_class = 0;

  return p;
}

$API struct RProc*
$proc_new_cfunc_with_env($state *mrb, $func_t func, $int argc, const $value *argv)
{
  struct RProc *p = $proc_new_cfunc(mrb, func);
  struct REnv *e;
  int i;

  p->e.env = e = env_new(mrb, argc);
  p->flags |= $PROC_ENVSET;
  $field_write_barrier(mrb, (struct RBasic*)p, (struct RBasic*)e);
  $ENV_UNSHARE_STACK(e);
  e->stack = ($value*)$malloc(mrb, sizeof($value) * argc);
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

$API struct RProc*
$closure_new_cfunc($state *mrb, $func_t func, int nlocals)
{
  return $proc_new_cfunc_with_env(mrb, func, nlocals, NULL);
}

$API $value
$proc_cfunc_env_get($state *mrb, $int idx)
{
  struct RProc *p = mrb->c->ci->proc;
  struct REnv *e;

  if (!p || !$PROC_CFUNC_P(p)) {
    $raise(mrb, E_TYPE_ERROR, "Can't get cfunc env from non-cfunc proc.");
  }
  e = $PROC_ENV(p);
  if (!e) {
    $raise(mrb, E_TYPE_ERROR, "Can't get cfunc env from cfunc Proc without REnv.");
  }
  if (idx < 0 || $ENV_STACK_LEN(e) <= idx) {
    $raisef(mrb, E_INDEX_ERROR, "Env index out of range: %S (expected: 0 <= index < %S)",
               $fixnum_value(idx), $fixnum_value($ENV_STACK_LEN(e)));
  }

  return e->stack[idx];
}

void
$proc_copy(struct RProc *a, struct RProc *b)
{
  if (a->body.irep) {
    /* already initialized proc */
    return;
  }
  a->flags = b->flags;
  a->body = b->body;
  if (!$PROC_CFUNC_P(a) && a->body.irep) {
    a->body.irep->refcnt++;
  }
  a->upper = b->upper;
  a->e.env = b->e.env;
  /* a->e.target_class = a->e.target_class; */
}

static $value
$proc_s_new($state *mrb, $value proc_class)
{
  $value blk;
  $value proc;
  struct RProc *p;

  $get_args(mrb, "&", &blk);
  if ($nil_p(blk)) {
    /* Calling Proc.new without a block is not implemented yet */
    $raise(mrb, E_ARGUMENT_ERROR, "tried to create Proc object without a block");
  }
  p = (struct RProc *)$obj_alloc(mrb, $TT_PROC, $class_ptr(proc_class));
  $proc_copy(p, $proc_ptr(blk));
  proc = $obj_value(p);
  $funcall_with_block(mrb, proc, $intern_lit(mrb, "initialize"), 0, NULL, proc);
  if (!$PROC_STRICT_P(p) &&
      mrb->c->ci > mrb->c->cibase && $PROC_ENV(p) == mrb->c->ci[-1].env) {
    p->flags |= $PROC_ORPHAN;
  }
  return proc;
}

static $value
$proc_init_copy($state *mrb, $value self)
{
  $value proc;

  $get_args(mrb, "o", &proc);
  if ($type(proc) != $TT_PROC) {
    $raise(mrb, E_ARGUMENT_ERROR, "not a proc");
  }
  $proc_copy($proc_ptr(self), $proc_ptr(proc));
  return self;
}

int
$proc_cfunc_p(struct RProc *p)
{
  return $PROC_CFUNC_P(p);
}

/* 15.2.17.4.2 */
static $value
$proc_arity($state *mrb, $value self)
{
  struct RProc *p = $proc_ptr(self);
  struct $irep *irep;
  $code *pc;
  $aspec aspec;
  int ma, op, ra, pa, arity;

  if ($PROC_CFUNC_P(p)) {
    /* TODO cfunc aspec not implemented yet */
    return $fixnum_value(-1);
  }

  irep = p->body.irep;
  if (!irep) {
    return $fixnum_value(0);
  }

  pc = irep->iseq;
  /* arity is depend on OP_ENTER */
  if (*pc != OP_ENTER) {
    return $fixnum_value(0);
  }

  aspec = PEEK_W(pc+1);
  ma = $ASPEC_REQ(aspec);
  op = $ASPEC_OPT(aspec);
  ra = $ASPEC_REST(aspec);
  pa = $ASPEC_POST(aspec);
  arity = ra || ($PROC_STRICT_P(p) && op) ? -(ma + pa + 1) : ma + pa;

  return $fixnum_value(arity);
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
static $value
proc_lambda($state *mrb, $value self)
{
  $value blk;
  struct RProc *p;

  $get_args(mrb, "&", &blk);
  if ($nil_p(blk)) {
    $raise(mrb, E_ARGUMENT_ERROR, "tried to create Proc object without a block");
  }
  if ($type(blk) != $TT_PROC) {
    $raise(mrb, E_ARGUMENT_ERROR, "not a proc");
  }
  p = $proc_ptr(blk);
  if (!$PROC_STRICT_P(p)) {
    struct RProc *p2 = (struct RProc*)$obj_alloc(mrb, $TT_PROC, p->c);
    $proc_copy(p2, p);
    p2->flags |= $PROC_STRICT;
    return $obj_value(p2);
  }
  return blk;
}

void
$init_proc($state *mrb)
{
  struct RProc *p;
  $method_t m;
  $irep *call_irep = ($irep *)$malloc(mrb, sizeof($irep));
  static const $irep $irep_zero = { 0 };

  *call_irep = $irep_zero;
  call_irep->flags = $ISEQ_NO_FREE;
  call_irep->iseq = call_iseq;
  call_irep->ilen = 1;
  call_irep->nregs = 2;         /* receiver and block */

  $define_class_method(mrb, mrb->proc_class, "new", $proc_s_new, $ARGS_ANY());
  $define_method(mrb, mrb->proc_class, "initialize_copy", $proc_init_copy, $ARGS_REQ(1));
  $define_method(mrb, mrb->proc_class, "arity", $proc_arity, $ARGS_NONE());

  p = $proc_new(mrb, call_irep);
  $METHOD_FROM_PROC(m, p);
  $define_method_raw(mrb, mrb->proc_class, $intern_lit(mrb, "call"), m);
  $define_method_raw(mrb, mrb->proc_class, $intern_lit(mrb, "[]"), m);

  $define_class_method(mrb, mrb->kernel_module, "lambda", proc_lambda, $ARGS_NONE()); /* 15.3.1.2.6  */
  $define_method(mrb, mrb->kernel_module,       "lambda", proc_lambda, $ARGS_NONE()); /* 15.3.1.3.27 */
}
