#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/proc.h>

#define fiber_ptr(o) ((struct RFiber*)ptr(o))

#define FIBER_STACK_INIT_SIZE 64
#define FIBER_CI_INIT_SIZE 8
#define CI_ACC_RESUMED -3

/*
 *  call-seq:
 *     Fiber.new{...} -> obj
 *
 *  Creates a fiber, whose execution is suspend until it is explicitly
 *  resumed using <code>Fiber#resume</code> method.
 *  The code running inside the fiber can give up control by calling
 *  <code>Fiber.yield</code> in which case it yields control back to caller
 *  (the caller of the <code>Fiber#resume</code>).
 *
 *  Upon yielding or termination the Fiber returns the value of the last
 *  executed expression
 *
 *  For instance:
 *
 *    fiber = Fiber.new do
 *      Fiber.yield 1
 *      2
 *    end
 *
 *    puts fiber.resume
 *    puts fiber.resume
 *    puts fiber.resume
 *
 *  <em>produces</em>
 *
 *    1
 *    2
 *    resuming dead fiber (FiberError)
 *
 *  The <code>Fiber#resume</code> method accepts an arbitrary number of
 *  parameters, if it is the first call to <code>resume</code> then they
 *  will be passed as block arguments. Otherwise they will be the return
 *  value of the call to <code>Fiber.yield</code>
 *
 *  Example:
 *
 *    fiber = Fiber.new do |first|
 *      second = Fiber.yield first + 2
 *    end
 *
 *    puts fiber.resume 10
 *    puts fiber.resume 14
 *    puts fiber.resume 18
 *
 *  <em>produces</em>
 *
 *    12
 *    14
 *    resuming dead fiber (FiberError)
 *
 */
static value
fiber_init(state *mrb, value self)
{
  static const struct context context_zero = { 0 };
  struct RFiber *f = fiber_ptr(self);
  struct context *c;
  struct RProc *p;
  callinfo *ci;
  value blk;
  size_t slen;

  get_args(mrb, "&", &blk);

  if (f->cxt) {
    raise(mrb, E_RUNTIME_ERROR, "cannot initialize twice");
  }
  if (nil_p(blk)) {
    raise(mrb, E_ARGUMENT_ERROR, "tried to create Fiber object without a block");
  }
  p = proc_ptr(blk);
  if (PROC_CFUNC_P(p)) {
    raise(mrb, E_FIBER_ERROR, "tried to create Fiber from C defined method");
  }

  c = (struct context*)malloc(mrb, sizeof(struct context));
  *c = context_zero;
  f->cxt = c;

  /* initialize VM stack */
  slen = FIBER_STACK_INIT_SIZE;
  if (p->body.irep->nregs > slen) {
    slen += p->body.irep->nregs;
  }
  c->stbase = (value *)malloc(mrb, slen*sizeof(value));
  c->stend = c->stbase + slen;
  c->stack = c->stbase;

#ifdef NAN_BOXING
  {
    value *p = c->stbase;
    value *pend = c->stend;

    while (p < pend) {
      SET_NIL_VALUE(*p);
      p++;
    }
  }
#else
  memset(c->stbase, 0, slen * sizeof(value));
#endif

  /* copy receiver from a block */
  c->stack[0] = mrb->c->stack[0];

  /* initialize callinfo stack */
  c->cibase = (callinfo *)calloc(mrb, FIBER_CI_INIT_SIZE, sizeof(callinfo));
  c->ciend = c->cibase + FIBER_CI_INIT_SIZE;
  c->ci = c->cibase;
  c->ci->stackent = c->stack;

  /* adjust return callinfo */
  ci = c->ci;
  ci->target_class = PROC_TARGET_CLASS(p);
  ci->proc = p;
  field_write_barrier(mrb, (struct RBasic*)obj_ptr(self), (struct RBasic*)p);
  ci->pc = p->body.irep->iseq;
  ci[1] = ci[0];
  c->ci++;                      /* push dummy callinfo */

  c->fib = f;
  c->status = FIBER_CREATED;

  return self;
}

static struct context*
fiber_check(state *mrb, value fib)
{
  struct RFiber *f = fiber_ptr(fib);

  assert(f->tt == TT_FIBER);
  if (!f->cxt) {
    raise(mrb, E_FIBER_ERROR, "uninitialized Fiber");
  }
  return f->cxt;
}

static value
fiber_result(state *mrb, const value *a, int len)
{
  if (len == 0) return nil_value();
  if (len == 1) return a[0];
  return ary_new_from_values(mrb, len, a);
}

/* mark return from context modifying method */
#define MARK_CONTEXT_MODIFY(c) (c)->ci->target_class = NULL

static void
fiber_check_cfunc(state *mrb, struct context *c)
{
  callinfo *ci;

  for (ci = c->ci; ci >= c->cibase; ci--) {
    if (ci->acc < 0) {
      raise(mrb, E_FIBER_ERROR, "can't cross C function boundary");
    }
  }
}

static void
fiber_switch_context(state *mrb, struct context *c)
{
  if (mrb->c->fib) {
    write_barrier(mrb, (struct RBasic*)mrb->c->fib);
  }
  c->status = FIBER_RUNNING;
  mrb->c = c;
}

static value
fiber_switch(state *mrb, value self, int len, const value *a, bool resume, bool vmexec)
{
  struct context *c = fiber_check(mrb, self);
  struct context *old_c = mrb->c;
  enum fiber_state status;
  value value;

  fiber_check_cfunc(mrb, c);
  status = c->status;
  if (resume && status == FIBER_TRANSFERRED) {
    raise(mrb, E_FIBER_ERROR, "resuming transferred fiber");
  }
  if (status == FIBER_RUNNING || status == FIBER_RESUMED) {
    raise(mrb, E_FIBER_ERROR, "double resume");
  }
  if (status == FIBER_TERMINATED) {
    raise(mrb, E_FIBER_ERROR, "resuming dead fiber");
  }
  old_c->status = resume ? FIBER_RESUMED : FIBER_TRANSFERRED;
  c->prev = resume ? mrb->c : (c->prev ? c->prev : mrb->root_c);
  fiber_switch_context(mrb, c);
  if (status == FIBER_CREATED) {
    value *b, *e;

    if (!c->ci->proc) {
      raise(mrb, E_FIBER_ERROR, "double resume (current)");
    }
    stack_extend(mrb, len+2); /* for receiver and (optional) block */
    b = c->stack+1;
    e = b + len;
    while (b<e) {
      *b++ = *a++;
    }
    c->cibase->argc = (int)len;
    value = c->stack[0] = PROC_ENV(c->ci->proc)->stack[0];
  }
  else {
    value = fiber_result(mrb, a, len);
  }

  if (vmexec) {
    c->vmexec = TRUE;
    value = vm_exec(mrb, c->ci[-1].proc, c->ci->pc);
    mrb->c = old_c;
  }
  else {
    MARK_CONTEXT_MODIFY(c);
  }
  return value;
}

/*
 *  call-seq:
 *     fiber.resume(args, ...) -> obj
 *
 *  Resumes the fiber from the point at which the last <code>Fiber.yield</code>
 *  was called, or starts running it if it is the first call to
 *  <code>resume</code>. Arguments passed to resume will be the value of
 *  the <code>Fiber.yield</code> expression or will be passed as block
 *  parameters to the fiber's block if this is the first <code>resume</code>.
 *
 *  Alternatively, when resume is called it evaluates to the arguments passed
 *  to the next <code>Fiber.yield</code> statement inside the fiber's block
 *  or to the block value if it runs to completion without any
 *  <code>Fiber.yield</code>
 */
static value
fiber_resume(state *mrb, value self)
{
  value *a;
  int len;
  bool vmexec = FALSE;

  get_args(mrb, "*!", &a, &len);
  if (mrb->c->ci->acc < 0) {
    vmexec = TRUE;
  }
  return fiber_switch(mrb, self, len, a, TRUE, vmexec);
}

/* resume thread with given arguments */
API value
fiber_resume(state *mrb, value fib, int len, const value *a)
{
  return fiber_switch(mrb, fib, len, a, TRUE, TRUE);
}

/*
 *  call-seq:
 *     fiber.alive? -> true or false
 *
 *  Returns true if the fiber can still be resumed. After finishing
 *  execution of the fiber block this method will always return false.
 */
API value
fiber_alive_p(state *mrb, value self)
{
  struct context *c = fiber_check(mrb, self);
  return bool_value(c->status != FIBER_TERMINATED);
}
#define fiber_alive_p fiber_alive_p

static value
fiber_eq(state *mrb, value self)
{
  value other;
  get_args(mrb, "o", &other);

  if (type(other) != TT_FIBER) {
    return false_value();
  }
  return bool_value(fiber_ptr(self) == fiber_ptr(other));
}

/*
 *  call-seq:
 *     fiber.transfer(args, ...) -> obj
 *
 *  Transfers control to receiver fiber of the method call.
 *  Unlike <code>resume</code> the receiver wouldn't be pushed to call
 * stack of fibers. Instead it will switch to the call stack of
 * transferring fiber.
 *  When resuming a fiber that was transferred to another fiber it would
 * cause double resume error. Though when the fiber is re-transferred
 * and <code>Fiber.yield</code> is called, the fiber would be resumable.
 */
static value
fiber_transfer(state *mrb, value self)
{
  struct context *c = fiber_check(mrb, self);
  value* a;
  int len;

  fiber_check_cfunc(mrb, mrb->c);
  get_args(mrb, "*!", &a, &len);

  if (c == mrb->root_c) {
    mrb->c->status = FIBER_TRANSFERRED;
    fiber_switch_context(mrb, c);
    MARK_CONTEXT_MODIFY(c);
    return fiber_result(mrb, a, len);
  }

  if (c == mrb->c) {
    return fiber_result(mrb, a, len);
  }

  return fiber_switch(mrb, self, len, a, FALSE, FALSE);
}

/* yield values to the caller fiber */
/* fiber_yield() must be called as `return fiber_yield(...)` */
API value
fiber_yield(state *mrb, int len, const value *a)
{
  struct context *c = mrb->c;

  if (!c->prev) {
    raise(mrb, E_FIBER_ERROR, "can't yield from root fiber");
  }

  fiber_check_cfunc(mrb, c);
  c->prev->status = FIBER_RUNNING;
  c->status = FIBER_SUSPENDED;
  fiber_switch_context(mrb, c->prev);
  c->prev = NULL;
  if (c->vmexec) {
    c->vmexec = FALSE;
    mrb->c->ci->acc = CI_ACC_RESUMED;
  }
  MARK_CONTEXT_MODIFY(mrb->c);
  return fiber_result(mrb, a, len);
}

/*
 *  call-seq:
 *     Fiber.yield(args, ...) -> obj
 *
 *  Yields control back to the context that resumed the fiber, passing
 *  along any arguments that were passed to it. The fiber will resume
 *  processing at this point when <code>resume</code> is called next.
 *  Any arguments passed to the next <code>resume</code> will be the
 *
 *  mruby limitation: Fiber resume/yield cannot cross C function boundary.
 *  thus you cannot yield from #initialize which is called by funcall().
 */
static value
fiber_yield(state *mrb, value self)
{
  value *a;
  int len;

  get_args(mrb, "*!", &a, &len);
  return fiber_yield(mrb, len, a);
}

/*
 *  call-seq:
 *     Fiber.current() -> fiber
 *
 *  Returns the current fiber. If you are not running in the context of
 *  a fiber this method will return the root fiber.
 */
static value
fiber_current(state *mrb, value self)
{
  if (!mrb->c->fib) {
    struct RFiber *f = (struct RFiber*)obj_alloc(mrb, TT_FIBER, class_ptr(self));

    f->cxt = mrb->c;
    mrb->c->fib = f;
  }
  return obj_value(mrb->c->fib);
}

void
mruby_fiber_gem_init(state* mrb)
{
  struct RClass *c;

  c = define_class(mrb, "Fiber", mrb->object_class);
  SET_INSTANCE_TT(c, TT_FIBER);

  define_method(mrb, c, "initialize", fiber_init,    ARGS_NONE());
  define_method(mrb, c, "resume",     fiber_resume,  ARGS_ANY());
  define_method(mrb, c, "transfer",   fiber_transfer, ARGS_ANY());
  define_method(mrb, c, "alive?",     fiber_alive_p, ARGS_NONE());
  define_method(mrb, c, "==",         fiber_eq,      ARGS_REQ(1));

  define_class_method(mrb, c, "yield", fiber_yield, ARGS_ANY());
  define_class_method(mrb, c, "current", fiber_current, ARGS_NONE());

  define_class(mrb, "FiberError", mrb->eStandardError_class);
}

void
mruby_fiber_gem_final(state* mrb)
{
}
