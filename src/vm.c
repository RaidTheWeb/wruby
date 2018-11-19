/*
** vm.c - virtual machine for mruby
**
** See Copyright Notice in mruby.h
*/

#include <stddef.h>
#include <stdarg.h>
#include <math.h>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/hash.h>
#include <mruby/irep.h>
#include <mruby/numeric.h>
#include <mruby/proc.h>
#include <mruby/range.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <mruby/error.h>
#include <mruby/opcode.h>
#include "value_array.h"
#include <mruby/throw.h>

#ifdef DISABLE_STDIO
#if defined(__cplusplus)
extern "C" {
#endif
void abort(void);
#if defined(__cplusplus)
}  /* extern "C" { */
#endif
#endif

#define STACK_INIT_SIZE 128
#define CALLINFO_INIT_SIZE 32

#ifndef ENSURE_STACK_INIT_SIZE
#define ENSURE_STACK_INIT_SIZE 16
#endif

#ifndef RESCUE_STACK_INIT_SIZE
#define RESCUE_STACK_INIT_SIZE 16
#endif

/* Define amount of linear stack growth. */
#ifndef STACK_GROWTH
#define STACK_GROWTH 128
#endif

/* Maximum funcall() depth. Should be set lower on memory constrained systems. */
#ifndef FUNCALL_DEPTH_MAX
#define FUNCALL_DEPTH_MAX 512
#endif

/* Maximum depth of ecall() recursion. */
#ifndef ECALL_DEPTH_MAX
#define ECALL_DEPTH_MAX 32
#endif

/* Maximum stack depth. Should be set lower on memory constrained systems.
The value below allows about 60000 recursive calls in the simplest case. */
#ifndef STACK_MAX
#define STACK_MAX (0x40000 - STACK_GROWTH)
#endif

#ifdef VM_DEBUG
# define DEBUG(x) (x)
#else
# define DEBUG(x)
#endif


#ifndef GC_FIXED_ARENA
static void
gc_arena_shrink(state *mrb, int idx)
{
  gc *gc = &mrb->gc;
  int capa = gc->arena_capa;

  if (idx < capa / 4) {
    capa >>= 2;
    if (capa < GC_ARENA_SIZE) {
      capa = GC_ARENA_SIZE;
    }
    if (capa != gc->arena_capa) {
      gc->arena = (struct RBasic**)realloc(mrb, gc->arena, sizeof(struct RBasic*)*capa);
      gc->arena_capa = capa;
    }
  }
}
#else
#define gc_arena_shrink(mrb,idx)
#endif

#define CALL_MAXARGS 127

void method_missing(state *mrb, sym name, value self, value args);

static inline void
stack_clear(value *from, size_t count)
{
#ifndef NAN_BOXING
  const value value_zero = { 0 };

  while (count-- > 0) {
    *from++ = value_zero;
  }
#else
  while (count-- > 0) {
    SET_NIL_VALUE(*from);
    from++;
  }
#endif
}

static inline void
stack_copy(value *dst, const value *src, size_t size)
{
  while (size-- > 0) {
    *dst++ = *src++;
  }
}

static void
stack_init(state *mrb)
{
  struct context *c = mrb->c;

  /* assert(mrb->stack == NULL); */
  c->stbase = (value *)calloc(mrb, STACK_INIT_SIZE, sizeof(value));
  c->stend = c->stbase + STACK_INIT_SIZE;
  c->stack = c->stbase;

  /* assert(ci == NULL); */
  c->cibase = (callinfo *)calloc(mrb, CALLINFO_INIT_SIZE, sizeof(callinfo));
  c->ciend = c->cibase + CALLINFO_INIT_SIZE;
  c->ci = c->cibase;
  c->ci->target_class = mrb->object_class;
  c->ci->stackent = c->stack;
}

static inline void
envadjust(state *mrb, value *oldbase, value *newbase, size_t oldsize)
{
  callinfo *ci = mrb->c->cibase;

  if (newbase == oldbase) return;
  while (ci <= mrb->c->ci) {
    struct REnv *e = ci->env;
    value *st;

    if (e && ENV_STACK_SHARED_P(e) &&
        (st = e->stack) && oldbase <= st && st < oldbase+oldsize) {
      ptrdiff_t off = e->stack - oldbase;

      e->stack = newbase + off;
    }

    if (ci->proc && PROC_ENV_P(ci->proc) && ci->env != PROC_ENV(ci->proc)) {
      e = PROC_ENV(ci->proc);

      if (e && ENV_STACK_SHARED_P(e) &&
          (st = e->stack) && oldbase <= st && st < oldbase+oldsize) {
        ptrdiff_t off = e->stack - oldbase;

        e->stack = newbase + off;
      }
    }

    ci->stackent = newbase + (ci->stackent - oldbase);
    ci++;
  }
}

/** def rec ; $deep =+ 1 ; if $deep > 1000 ; return 0 ; end ; rec ; end  */

static void
stack_extend_alloc(state *mrb, int room)
{
  value *oldbase = mrb->c->stbase;
  value *newstack;
  size_t oldsize = mrb->c->stend - mrb->c->stbase;
  size_t size = oldsize;
  size_t off = mrb->c->stack - mrb->c->stbase;

  if (off > size) size = off;
#ifdef STACK_EXTEND_DOUBLING
  if ((size_t)room <= size)
    size *= 2;
  else
    size += room;
#else
  /* Use linear stack growth.
     It is slightly slower than doubling the stack space,
     but it saves memory on small devices. */
  if (room <= STACK_GROWTH)
    size += STACK_GROWTH;
  else
    size += room;
#endif

  newstack = (value *)realloc(mrb, mrb->c->stbase, sizeof(value) * size);
  if (newstack == NULL) {
    exc_raise(mrb, obj_value(mrb->stack_err));
  }
  stack_clear(&(newstack[oldsize]), size - oldsize);
  envadjust(mrb, oldbase, newstack, oldsize);
  mrb->c->stbase = newstack;
  mrb->c->stack = mrb->c->stbase + off;
  mrb->c->stend = mrb->c->stbase + size;

  /* Raise an exception if the new stack size will be too large,
     to prevent infinite recursion. However, do this only after resizing the stack, so raise has stack space to work with. */
  if (size > STACK_MAX) {
    exc_raise(mrb, obj_value(mrb->stack_err));
  }
}

API void
stack_extend(state *mrb, int room)
{
  if (mrb->c->stack + room >= mrb->c->stend) {
    stack_extend_alloc(mrb, room);
  }
}

static inline struct REnv*
uvenv(state *mrb, int up)
{
  struct RProc *proc = mrb->c->ci->proc;
  struct REnv *e;

  while (up--) {
    proc = proc->upper;
    if (!proc) return NULL;
  }
  e = PROC_ENV(proc);
  if (e) return e;              /* proc has enclosed env */
  else {
    callinfo *ci = mrb->c->ci;
    callinfo *cb = mrb->c->cibase;

    while (cb <= ci) {
      if (ci->proc == proc) {
        return ci->env;
      }
      ci--;
    }
  }
  return NULL;
}

static inline struct RProc*
top_proc(state *mrb, struct RProc *proc)
{
  while (proc->upper) {
    if (PROC_SCOPE_P(proc) || PROC_STRICT_P(proc))
      return proc;
    proc = proc->upper;
  }
  return proc;
}

#define CI_ACC_SKIP    -1
#define CI_ACC_DIRECT  -2
#define CI_ACC_RESUMED -3

static inline callinfo*
cipush(state *mrb)
{
  struct context *c = mrb->c;
  static const callinfo ci_zero = { 0 };
  callinfo *ci = c->ci;

  int ridx = ci->ridx;

  if (ci + 1 == c->ciend) {
    ptrdiff_t size = ci - c->cibase;

    c->cibase = (callinfo *)realloc(mrb, c->cibase, sizeof(callinfo)*size*2);
    c->ci = c->cibase + size;
    c->ciend = c->cibase + size * 2;
  }
  ci = ++c->ci;
  *ci = ci_zero;
  ci->epos = mrb->c->eidx;
  ci->ridx = ridx;

  return ci;
}

void
env_unshare(state *mrb, struct REnv *e)
{
  if (e == NULL) return;
  else {
    size_t len = (size_t)ENV_STACK_LEN(e);
    value *p;

    if (!ENV_STACK_SHARED_P(e)) return;
    if (e->cxt != mrb->c) return;
    if (e == mrb->c->cibase->env) return; /* for mirb */
    p = (value *)malloc(mrb, sizeof(value)*len);
    if (len > 0) {
      stack_copy(p, e->stack, len);
    }
    e->stack = p;
    ENV_UNSHARE_STACK(e);
    write_barrier(mrb, (struct RBasic *)e);
  }
}

static inline void
cipop(state *mrb)
{
  struct context *c = mrb->c;
  struct REnv *env = c->ci->env;

  c->ci--;
  if (env) env_unshare(mrb, env);
}

void exc_set(state *mrb, value exc);

static void
ecall(state *mrb)
{
  struct RProc *p;
  struct context *c = mrb->c;
  callinfo *ci = c->ci;
  struct RObject *exc;
  struct REnv *env;
  ptrdiff_t cioff;
  int ai = gc_arena_save(mrb);
  uint16_t i = --c->eidx;
  int nregs;

  if (i<0) return;
  if (ci - c->cibase > ECALL_DEPTH_MAX) {
    exc_raise(mrb, obj_value(mrb->stack_err));
  }
  p = c->ensure[i];
  if (!p) return;
  assert(!PROC_CFUNC_P(p));
  c->ensure[i] = NULL;
  nregs = p->upper->body.irep->nregs;
  if (ci->proc && !PROC_CFUNC_P(ci->proc) &&
      ci->proc->body.irep->nregs > nregs) {
    nregs = ci->proc->body.irep->nregs;
  }
  cioff = ci - c->cibase;
  ci = cipush(mrb);
  ci->stackent = mrb->c->stack;
  ci->mid = ci[-1].mid;
  ci->acc = CI_ACC_SKIP;
  ci->argc = 0;
  ci->proc = p;
  ci->target_class = PROC_TARGET_CLASS(p);
  env = PROC_ENV(p);
  assert(env);
  c->stack += nregs;
  exc = mrb->exc; mrb->exc = 0;
  if (exc) {
    gc_protect(mrb, obj_value(exc));
  }
  run(mrb, p, env->stack[0]);
  mrb->c = c;
  c->ci = c->cibase + cioff;
  if (!mrb->exc) mrb->exc = exc;
  gc_arena_restore(mrb, ai);
}

#ifndef FUNCALL_ARGC_MAX
#define FUNCALL_ARGC_MAX 16
#endif

API value
funcall(state *mrb, value self, const char *name, int argc, ...)
{
  value argv[FUNCALL_ARGC_MAX];
  va_list ap;
  int i;
  sym mid = intern_cstr(mrb, name);

  if (argc > FUNCALL_ARGC_MAX) {
    raise(mrb, E_ARGUMENT_ERROR, "Too long arguments. (limit=" STRINGIZE(FUNCALL_ARGC_MAX) ")");
  }

  va_start(ap, argc);
  for (i = 0; i < argc; i++) {
    argv[i] = va_arg(ap, value);
  }
  va_end(ap);
  return funcall_argv(mrb, self, mid, argc, argv);
}

static int
ci_nregs(callinfo *ci)
{
  struct RProc *p;
  int n = 0;

  if (!ci) return 3;
  p = ci->proc;
  if (!p) {
    if (ci->argc < 0) return 3;
    return ci->argc+2;
  }
  if (!PROC_CFUNC_P(p) && p->body.irep) {
    n = p->body.irep->nregs;
  }
  if (ci->argc < 0) {
    if (n < 3) n = 3; /* self + args + blk */
  }
  if (ci->argc > n) {
    n = ci->argc + 2; /* self + blk */
  }
  return n;
}

API value
funcall_with_block(state *mrb, value self, sym mid, int argc, const value *argv, value blk)
{
  value val;

  if (!mrb->jmp) {
    struct jmpbuf c_jmp;
    ptrdiff_t nth_ci = mrb->c->ci - mrb->c->cibase;

    TRY(&c_jmp) {
      mrb->jmp = &c_jmp;
      /* recursive call */
      val = funcall_with_block(mrb, self, mid, argc, argv, blk);
      mrb->jmp = 0;
    }
    CATCH(&c_jmp) { /* error */
      while (nth_ci < (mrb->c->ci - mrb->c->cibase)) {
        mrb->c->stack = mrb->c->ci->stackent;
        cipop(mrb);
      }
      mrb->jmp = 0;
      val = obj_value(mrb->exc);
    }
    END_EXC(&c_jmp);
    mrb->jmp = 0;
  }
  else {
    method_t m;
    struct RClass *c;
    callinfo *ci;
    int n = ci_nregs(mrb->c->ci);
    ptrdiff_t voff = -1;

    if (!mrb->c->stack) {
      stack_init(mrb);
    }
    if (argc < 0) {
      raisef(mrb, E_ARGUMENT_ERROR, "negative argc for funcall (%S)", fixnum_value(argc));
    }
    c = class(mrb, self);
    m = method_search_vm(mrb, &c, mid);
    if (METHOD_UNDEF_P(m)) {
      sym missing = intern_lit(mrb, "method_missing");
      value args = a_ary_new_from_values(mrb, argc, argv);
      m = method_search_vm(mrb, &c, missing);
      if (METHOD_UNDEF_P(m)) {
        method_missing(mrb, mid, self, args);
      }
      ary_unshift(mrb, args, symbol_value(mid));
      stack_extend(mrb, n+2);
      mrb->c->stack[n+1] = args;
      argc = -1;
    }
    if (mrb->c->ci - mrb->c->cibase > FUNCALL_DEPTH_MAX) {
      exc_raise(mrb, obj_value(mrb->stack_err));
    }
    ci = cipush(mrb);
    ci->mid = mid;
    ci->stackent = mrb->c->stack;
    ci->argc = (int)argc;
    ci->target_class = c;
    mrb->c->stack = mrb->c->stack + n;
    if (mrb->c->stbase <= argv && argv < mrb->c->stend) {
      voff = argv - mrb->c->stbase;
    }
    if (METHOD_CFUNC_P(m)) {
      stack_extend(mrb, argc + 2);
    }
    else if (argc >= CALL_MAXARGS) {
      value args = a_ary_new_from_values(mrb, argc, argv);

      stack_extend(mrb, 3);
      mrb->c->stack[1] = args;
      ci->argc = -1;
      argc = 1;
    }
    else {
      struct RProc *p = METHOD_PROC(m);

      ci->proc = p;
      if (argc < 0) argc = 1;
      stack_extend(mrb, p->body.irep->nregs + argc);
    }
    if (voff >= 0) {
      argv = mrb->c->stbase + voff;
    }
    mrb->c->stack[0] = self;
    if (ci->argc > 0) {
      stack_copy(mrb->c->stack+1, argv, argc);
    }
    mrb->c->stack[argc+1] = blk;

    if (METHOD_CFUNC_P(m)) {
      int ai = gc_arena_save(mrb);

      ci->acc = CI_ACC_DIRECT;
      if (METHOD_PROC_P(m)) {
        ci->proc = METHOD_PROC(m);
      }
      val = METHOD_CFUNC(m)(mrb, self);
      mrb->c->stack = mrb->c->ci->stackent;
      cipop(mrb);
      gc_arena_restore(mrb, ai);
    }
    else {
      ci->acc = CI_ACC_SKIP;
      val = run(mrb, METHOD_PROC(m), self);
    }
  }
  gc_protect(mrb, val);
  return val;
}

API value
funcall_argv(state *mrb, value self, sym mid, int argc, const value *argv)
{
  return funcall_with_block(mrb, self, mid, argc, argv, nil_value());
}

value
exec_irep(state *mrb, value self, struct RProc *p)
{
  callinfo *ci = mrb->c->ci;
  int keep, nregs;

  mrb->c->stack[0] = self;
  ci->proc = p;
  if (PROC_CFUNC_P(p)) {
    return PROC_CFUNC(p)(mrb, self);
  }
  nregs = p->body.irep->nregs;
  if (ci->argc < 0) keep = 3;
  else keep = ci->argc + 2;
  if (nregs < keep) {
    stack_extend(mrb, keep);
  }
  else {
    stack_extend(mrb, nregs);
    stack_clear(mrb->c->stack+keep, nregs-keep);
  }

  ci = cipush(mrb);
  ci->target_class = 0;
  ci->pc = p->body.irep->iseq;
  ci->stackent = mrb->c->stack;
  ci->acc = 0;

  return self;
}

/* 15.3.1.3.4  */
/* 15.3.1.3.44 */
/*
 *  call-seq:
 *     obj.send(symbol [, args...])        -> obj
 *     obj.__send__(symbol [, args...])      -> obj
 *
 *  Invokes the method identified by _symbol_, passing it any
 *  arguments specified. You can use <code>__send__</code> if the name
 *  +send+ clashes with an existing method in _obj_.
 *
 *     class Klass
 *       def hello(*args)
 *         "Hello " + args.join(' ')
 *       end
 *     end
 *     k = Klass.new
 *     k.send :hello, "gentle", "readers"   #=> "Hello gentle readers"
 */
API value
f_send(state *mrb, value self)
{
  sym name;
  value block, *argv, *regs;
  int argc, i, len;
  method_t m;
  struct RClass *c;
  callinfo *ci;

  get_args(mrb, "n*&", &name, &argv, &argc, &block);
  ci = mrb->c->ci;
  if (ci->acc < 0) {
  funcall:
    return funcall_with_block(mrb, self, name, argc, argv, block);
  }

  c = class(mrb, self);
  m = method_search_vm(mrb, &c, name);
  if (METHOD_UNDEF_P(m)) {            /* call method_mising */
    goto funcall;
  }

  ci->mid = name;
  ci->target_class = c;
  regs = mrb->c->stack+1;
  /* remove first symbol from arguments */
  if (ci->argc >= 0) {
    for (i=0,len=ci->argc; i<len; i++) {
      regs[i] = regs[i+1];
    }
    ci->argc--;
  }
  else {                     /* variable length arguments */
    ary_shift(mrb, regs[0]);
  }

  if (METHOD_CFUNC_P(m)) {
    if (METHOD_PROC_P(m)) {
      ci->proc = METHOD_PROC(m);
    }
    return METHOD_CFUNC(m)(mrb, self);
  }
  return exec_irep(mrb, self, METHOD_PROC(m));
}

static value
eval_under(state *mrb, value self, value blk, struct RClass *c)
{
  struct RProc *p;
  callinfo *ci;
  int nregs;

  if (nil_p(blk)) {
    raise(mrb, E_ARGUMENT_ERROR, "no block given");
  }
  ci = mrb->c->ci;
  if (ci->acc == CI_ACC_DIRECT) {
    ci->target_class = c;
    return yield_cont(mrb, blk, self, 1, &self);
  }
  ci->target_class = c;
  p = proc_ptr(blk);
  ci->proc = p;
  ci->argc = 1;
  ci->mid = ci[-1].mid;
  if (PROC_CFUNC_P(p)) {
    stack_extend(mrb, 3);
    mrb->c->stack[0] = self;
    mrb->c->stack[1] = self;
    mrb->c->stack[2] = nil_value();
    return PROC_CFUNC(p)(mrb, self);
  }
  nregs = p->body.irep->nregs;
  stack_extend(mrb, (nregs < 3) ? 3 : nregs);
  mrb->c->stack[0] = self;
  mrb->c->stack[1] = self;
  mrb->c->stack[2] = nil_value();
  ci = cipush(mrb);
  ci->target_class = 0;
  ci->pc = p->body.irep->iseq;
  ci->stackent = mrb->c->stack;
  ci->acc = 0;

  return self;
}

/* 15.2.2.4.35 */
/*
 *  call-seq:
 *     mod.class_eval {| | block }  -> obj
 *     mod.module_eval {| | block } -> obj
 *
 *  Evaluates block in the context of _mod_. This can
 *  be used to add methods to a class. <code>module_eval</code> returns
 *  the result of evaluating its argument.
 */
value
mod_module_eval(state *mrb, value mod)
{
  value a, b;

  if (get_args(mrb, "|S&", &a, &b) == 1) {
    raise(mrb, E_NOTIMP_ERROR, "module_eval/class_eval with string not implemented");
  }
  return eval_under(mrb, mod, b, class_ptr(mod));
}

/* 15.3.1.3.18 */
/*
 *  call-seq:
 *     obj.instance_eval {| | block }                       -> obj
 *
 *  Evaluates the given block,within  the context of the receiver (_obj_).
 *  In order to set the context, the variable +self+ is set to _obj_ while
 *  the code is executing, giving the code access to _obj_'s
 *  instance variables. In the version of <code>instance_eval</code>
 *  that takes a +String+, the optional second and third
 *  parameters supply a filename and starting line number that are used
 *  when reporting compilation errors.
 *
 *     class KlassWithSecret
 *       def initialize
 *         @secret = 99
 *       end
 *     end
 *     k = KlassWithSecret.new
 *     k.instance_eval { @secret }   #=> 99
 */
value
obj_instance_eval(state *mrb, value self)
{
  value a, b;
  value cv;
  struct RClass *c;

  if (get_args(mrb, "|S&", &a, &b) == 1) {
    raise(mrb, E_NOTIMP_ERROR, "instance_eval with string not implemented");
  }
  switch (type(self)) {
  case TT_SYMBOL:
  case TT_FIXNUM:
#ifndef WITHOUT_FLOAT
  case TT_FLOAT:
#endif
    c = 0;
    break;
  default:
    cv = singleton_class(mrb, self);
    c = class_ptr(cv);
    break;
  }
  return eval_under(mrb, self, b, c);
}

API value
yield_with_class(state *mrb, value b, int argc, const value *argv, value self, struct RClass *c)
{
  struct RProc *p;
  sym mid = mrb->c->ci->mid;
  callinfo *ci;
  value val;
  int n;

  if (nil_p(b)) {
    raise(mrb, E_ARGUMENT_ERROR, "no block given");
  }
  ci = mrb->c->ci;
  n = ci_nregs(ci);
  if (ci - mrb->c->cibase > FUNCALL_DEPTH_MAX) {
    exc_raise(mrb, obj_value(mrb->stack_err));
  }
  p = proc_ptr(b);
  ci = cipush(mrb);
  ci->mid = mid;
  ci->proc = p;
  ci->stackent = mrb->c->stack;
  ci->argc = (int)argc;
  ci->target_class = c;
  ci->acc = CI_ACC_SKIP;
  n = PROC_CFUNC_P(p) ? (int)(argc+2) : p->body.irep->nregs;
  mrb->c->stack = mrb->c->stack + n;
  stack_extend(mrb, n);

  mrb->c->stack[0] = self;
  if (argc > 0) {
    stack_copy(mrb->c->stack+1, argv, argc);
  }
  mrb->c->stack[argc+1] = nil_value();

  if (PROC_CFUNC_P(p)) {
    val = PROC_CFUNC(p)(mrb, self);
    mrb->c->stack = mrb->c->ci->stackent;
    cipop(mrb);
  }
  else {
    val = run(mrb, p, self);
  }
  return val;
}

API value
yield_argv(state *mrb, value b, int argc, const value *argv)
{
  struct RProc *p = proc_ptr(b);

  return yield_with_class(mrb, b, argc, argv, PROC_ENV(p)->stack[0], PROC_TARGET_CLASS(p));
}

API value
yield(state *mrb, value b, value arg)
{
  struct RProc *p = proc_ptr(b);

  return yield_with_class(mrb, b, 1, &arg, PROC_ENV(p)->stack[0], PROC_TARGET_CLASS(p));
}

value
yield_cont(state *mrb, value b, value self, int argc, const value *argv)
{
  struct RProc *p;
  callinfo *ci;

  if (nil_p(b)) {
    raise(mrb, E_ARGUMENT_ERROR, "no block given");
  }
  if (type(b) != TT_PROC) {
    raise(mrb, E_TYPE_ERROR, "not a block");
  }

  p = proc_ptr(b);
  ci = mrb->c->ci;

  stack_extend(mrb, 3);
  mrb->c->stack[1] = a_ary_new_from_values(mrb, argc, argv);
  mrb->c->stack[2] = nil_value();
  ci->argc = -1;
  return exec_irep(mrb, self, p);
}

value
mod_s_nesting(state *mrb, value mod)
{
  struct RProc *proc;
  value ary;
  struct RClass *c = NULL;

  get_args(mrb, "");
  ary = ary_new(mrb);
  proc = mrb->c->ci[-1].proc;   /* callee proc */
  assert(!PROC_CFUNC_P(proc));
  while (proc) {
    if (PROC_SCOPE_P(proc)) {
      struct RClass *c2 = PROC_TARGET_CLASS(proc);

      if (c2 != c) {
        c = c2;
        ary_push(mrb, ary, obj_value(c));
      }
    }
    proc = proc->upper;
  }
  return ary;
}

static struct RBreak*
break_new(state *mrb, struct RProc *p, value val)
{
  struct RBreak *brk;

  brk = (struct RBreak*)obj_alloc(mrb, TT_BREAK, NULL);
  brk->proc = p;
  brk->val = val;

  return brk;
}

typedef enum {
  LOCALJUMP_ERROR_RETURN = 0,
  LOCALJUMP_ERROR_BREAK = 1,
  LOCALJUMP_ERROR_YIELD = 2
} localjump_error_kind;

static void
localjump_error(state *mrb, localjump_error_kind kind)
{
  char kind_str[3][7] = { "return", "break", "yield" };
  char kind_str_len[] = { 6, 5, 5 };
  static const char lead[] = "unexpected ";
  value msg;
  value exc;

  msg = str_new_capa(mrb, sizeof(lead) + 7);
  str_cat(mrb, msg, lead, sizeof(lead) - 1);
  str_cat(mrb, msg, kind_str[kind], kind_str_len[kind]);
  exc = exc_new_str(mrb, E_LOCALJUMP_ERROR, msg);
  exc_set(mrb, exc);
}

static void
argnum_error(state *mrb, int num)
{
  value exc;
  value str;
  int argc = mrb->c->ci->argc;

  if (argc < 0) {
    value args = mrb->c->stack[1];
    if (array_p(args)) {
      argc = RARRAY_LEN(args);
    }
  }
  if (mrb->c->ci->mid) {
    str = format(mrb, "'%S': wrong number of arguments (%S for %S)",
                  sym2str(mrb, mrb->c->ci->mid),
                  fixnum_value(argc), fixnum_value(num));
  }
  else {
    str = format(mrb, "wrong number of arguments (%S for %S)",
                     fixnum_value(argc), fixnum_value(num));
  }
  exc = exc_new_str(mrb, E_ARGUMENT_ERROR, str);
  exc_set(mrb, exc);
}

#define ERR_PC_SET(mrb, pc) mrb->c->ci->err = pc;
#define ERR_PC_CLR(mrb)     mrb->c->ci->err = 0;
#ifdef ENABLE_DEBUG_HOOK
#define CODE_FETCH_HOOK(mrb, irep, pc, regs) if ((mrb)->code_fetch_hook) (mrb)->code_fetch_hook((mrb), (irep), (pc), (regs));
#else
#define CODE_FETCH_HOOK(mrb, irep, pc, regs)
#endif

#ifdef BYTECODE_DECODE_OPTION
#define BYTECODE_DECODER(x) ((mrb)->bytecode_decoder)?(mrb)->bytecode_decoder((mrb), (x)):(x)
#else
#define BYTECODE_DECODER(x) (x)
#endif

#ifndef DISABLE_DIRECT_THREADING
#if defined __GNUC__ || defined __clang__ || defined __INTEL_COMPILER
#define DIRECT_THREADED
#endif
#endif /* ifndef DISABLE_DIRECT_THREADING */

#ifndef DIRECT_THREADED

#define INIT_DISPATCH for (;;) { insn = BYTECODE_DECODER(*pc); CODE_FETCH_HOOK(mrb, irep, pc, regs); switch (insn) {
#define CASE(insn,ops) case insn: pc++; FETCH_ ## ops ();; L_ ## insn ## _BODY:
#define NEXT break
#define JUMP NEXT
#define END_DISPATCH }}

#else

#define INIT_DISPATCH JUMP; return nil_value();
#define CASE(insn,ops) L_ ## insn: pc++; FETCH_ ## ops (); L_ ## insn ## _BODY:
#define NEXT insn=BYTECODE_DECODER(*pc); CODE_FETCH_HOOK(mrb, irep, pc, regs); goto *optable[insn]
#define JUMP NEXT

#define END_DISPATCH

#endif

API value
vm_run(state *mrb, struct RProc *proc, value self, unsigned int stack_keep)
{
  irep *irep = proc->body.irep;
  value result;
  struct context *c = mrb->c;
  ptrdiff_t cioff = c->ci - c->cibase;
  unsigned int nregs = irep->nregs;

  if (!c->stack) {
    stack_init(mrb);
  }
  if (stack_keep > nregs)
    nregs = stack_keep;
  stack_extend(mrb, nregs);
  stack_clear(c->stack + stack_keep, nregs - stack_keep);
  c->stack[0] = self;
  result = vm_exec(mrb, proc, irep->iseq);
  if (c->ci - c->cibase > cioff) {
    c->ci = c->cibase + cioff;
  }
  if (mrb->c != c) {
    if (mrb->c->fib) {
      write_barrier(mrb, (struct RBasic*)mrb->c->fib);
    }
    mrb->c = c;
  }
  return result;
}

static bool
check_target_class(state *mrb)
{
  if (!mrb->c->ci->target_class) {
    value exc = exc_new_str_lit(mrb, E_TYPE_ERROR, "no target class or module");
    exc_set(mrb, exc);
    return FALSE;
  }
  return TRUE;
}

void hash_check_kdict(state *mrb, value self);

API value
vm_exec(state *mrb, struct RProc *proc, code *pc)
{
  /* assert(proc_cfunc_p(proc)) */
  irep *irep = proc->body.irep;
  value *pool = irep->pool;
  sym *syms = irep->syms;
  code insn;
  int ai = gc_arena_save(mrb);
  struct jmpbuf *prev_jmp = mrb->jmp;
  struct jmpbuf c_jmp;
  uint32_t a;
  uint16_t b;
  uint8_t c;
  sym mid;

#ifdef DIRECT_THREADED
  static void *optable[] = {
#define OPCODE(x,_) &&L_OP_ ## x,
#include "mruby/ops.h"
#undef OPCODE
  };
#endif

  bool exc_catched = FALSE;
RETRY_TRY_BLOCK:

  TRY(&c_jmp) {

  if (exc_catched) {
    exc_catched = FALSE;
    gc_arena_restore(mrb, ai);
    if (mrb->exc && mrb->exc->tt == TT_BREAK)
      goto L_BREAK;
    goto L_RAISE;
  }
  mrb->jmp = &c_jmp;
  mrb->c->ci->proc = proc;

#define regs (mrb->c->stack)
  INIT_DISPATCH {
    CASE(OP_NOP, Z) {
      /* do nothing */
      NEXT;
    }

    CASE(OP_MOVE, BB) {
      regs[a] = regs[b];
      NEXT;
    }

    CASE(OP_LOADL, BB) {
#ifdef WORD_BOXING
      value val = pool[b];
#ifndef WITHOUT_FLOAT
      if (float_p(val)) {
        val = float_value(mrb, float(val));
      }
#endif
      regs[a] = val;
#else
      regs[a] = pool[b];
#endif
      NEXT;
    }

    CASE(OP_LOADI, BB) {
      SET_INT_VALUE(regs[a], b);
      NEXT;
    }

    CASE(OP_LOADINEG, BB) {
      SET_INT_VALUE(regs[a], -b);
      NEXT;
    }

    CASE(OP_LOADI__1,B) goto L_LOADI;
    CASE(OP_LOADI_0,B) goto L_LOADI;
    CASE(OP_LOADI_1,B) goto L_LOADI;
    CASE(OP_LOADI_2,B) goto L_LOADI;
    CASE(OP_LOADI_3,B) goto L_LOADI;
    CASE(OP_LOADI_4,B) goto L_LOADI;
    CASE(OP_LOADI_5,B) goto L_LOADI;
    CASE(OP_LOADI_6,B) goto L_LOADI;
    CASE(OP_LOADI_7, B) {
    L_LOADI:
      SET_INT_VALUE(regs[a], (int)insn - (int)OP_LOADI_0);
      NEXT;
    }

    CASE(OP_LOADSYM, BB) {
      SET_SYM_VALUE(regs[a], syms[b]);
      NEXT;
    }

    CASE(OP_LOADNIL, B) {
      SET_NIL_VALUE(regs[a]);
      NEXT;
    }

    CASE(OP_LOADSELF, B) {
      regs[a] = regs[0];
      NEXT;
    }

    CASE(OP_LOADT, B) {
      SET_TRUE_VALUE(regs[a]);
      NEXT;
    }

    CASE(OP_LOADF, B) {
      SET_FALSE_VALUE(regs[a]);
      NEXT;
    }

    CASE(OP_GETGV, BB) {
      value val = gv_get(mrb, syms[b]);
      regs[a] = val;
      NEXT;
    }

    CASE(OP_SETGV, BB) {
      gv_set(mrb, syms[b], regs[a]);
      NEXT;
    }

    CASE(OP_GETSV, BB) {
      value val = vm_special_get(mrb, b);
      regs[a] = val;
      NEXT;
    }

    CASE(OP_SETSV, BB) {
      vm_special_set(mrb, b, regs[a]);
      NEXT;
    }

    CASE(OP_GETIV, BB) {
      regs[a] = iv_get(mrb, regs[0], syms[b]);
      NEXT;
    }

    CASE(OP_SETIV, BB) {
      iv_set(mrb, regs[0], syms[b], regs[a]);
      NEXT;
    }

    CASE(OP_GETCV, BB) {
      value val;
      ERR_PC_SET(mrb, pc);
      val = vm_cv_get(mrb, syms[b]);
      ERR_PC_CLR(mrb);
      regs[a] = val;
      NEXT;
    }

    CASE(OP_SETCV, BB) {
      vm_cv_set(mrb, syms[b], regs[a]);
      NEXT;
    }

    CASE(OP_GETCONST, BB) {
      value val;
      sym sym = syms[b];

      ERR_PC_SET(mrb, pc);
      val = vm_const_get(mrb, sym);
      ERR_PC_CLR(mrb);
      regs[a] = val;
      NEXT;
    }

    CASE(OP_SETCONST, BB) {
      vm_const_set(mrb, syms[b], regs[a]);
      NEXT;
    }

    CASE(OP_GETMCNST, BB) {
      value val;

      ERR_PC_SET(mrb, pc);
      val = const_get(mrb, regs[a], syms[b]);
      ERR_PC_CLR(mrb);
      regs[a] = val;
      NEXT;
    }

    CASE(OP_SETMCNST, BB) {
      const_set(mrb, regs[a+1], syms[b], regs[a]);
      NEXT;
    }

    CASE(OP_GETUPVAR, BBB) {
      value *regs_a = regs + a;
      struct REnv *e = uvenv(mrb, c);

      if (e && b < ENV_STACK_LEN(e)) {
        *regs_a = e->stack[b];
      }
      else {
        *regs_a = nil_value();
      }
      NEXT;
    }

    CASE(OP_SETUPVAR, BBB) {
      struct REnv *e = uvenv(mrb, c);

      if (e) {
        value *regs_a = regs + a;

        if (b < ENV_STACK_LEN(e)) {
          e->stack[b] = *regs_a;
          write_barrier(mrb, (struct RBasic*)e);
        }
      }
      NEXT;
    }

    CASE(OP_JMP, S) {
      pc = irep->iseq+a;
      JUMP;
    }
    CASE(OP_JMPIF, BS) {
      if (test(regs[a])) {
        pc = irep->iseq+b;
        JUMP;
      }
      NEXT;
    }
    CASE(OP_JMPNOT, BS) {
      if (!test(regs[a])) {
        pc = irep->iseq+b;
        JUMP;
      }
      NEXT;
    }
    CASE(OP_JMPNIL, BS) {
      if (nil_p(regs[a])) {
        pc = irep->iseq+b;
        JUMP;
      }
      NEXT;
    }

    CASE(OP_ONERR, S) {
      /* check rescue stack */
      if (mrb->c->ci->ridx == UINT16_MAX-1) {
        value exc = exc_new_str_lit(mrb, E_RUNTIME_ERROR, "too many nested rescues");
        exc_set(mrb, exc);
        goto L_RAISE;
      }
      /* expand rescue stack */
      if (mrb->c->rsize <= mrb->c->ci->ridx) {
        if (mrb->c->rsize == 0) mrb->c->rsize = RESCUE_STACK_INIT_SIZE;
        else {
          mrb->c->rsize *= 2;
          if (mrb->c->rsize <= mrb->c->ci->ridx) {
            mrb->c->rsize = UINT16_MAX;
          }
        }
        mrb->c->rescue = (uint16_t*)realloc(mrb, mrb->c->rescue, sizeof(uint16_t)*mrb->c->rsize);
      }
      /* push rescue stack */
      mrb->c->rescue[mrb->c->ci->ridx++] = a;
      NEXT;
    }

    CASE(OP_EXCEPT, B) {
      value exc = obj_value(mrb->exc);
      mrb->exc = 0;
      regs[a] = exc;
      NEXT;
    }
    CASE(OP_RESCUE, BB) {
      value exc = regs[a];  /* exc on stack */
      value e = regs[b];
      struct RClass *ec;

      switch (type(e)) {
      case TT_CLASS:
      case TT_MODULE:
        break;
      default:
        {
          value exc;

          exc = exc_new_str_lit(mrb, E_TYPE_ERROR,
                                    "class or module required for rescue clause");
          exc_set(mrb, exc);
          goto L_RAISE;
        }
      }
      ec = class_ptr(e);
      regs[b] = bool_value(obj_is_kind_of(mrb, exc, ec));
      NEXT;
    }

    CASE(OP_POPERR, B) {
      mrb->c->ci->ridx -= a;
      NEXT;
    }

    CASE(OP_RAISE, B) {
      exc_set(mrb, regs[a]);
      goto L_RAISE;
    }

    CASE(OP_EPUSH, B) {
      struct RProc *p;

      p = closure_new(mrb, irep->reps[a]);
      /* check ensure stack */
      if (mrb->c->eidx == UINT16_MAX-1) {
        value exc = exc_new_str_lit(mrb, E_RUNTIME_ERROR, "too many nested ensures");
        exc_set(mrb, exc);
        goto L_RAISE;
      }
      /* expand ensure stack */
      if (mrb->c->esize <= mrb->c->eidx+1) {
        if (mrb->c->esize == 0) mrb->c->esize = ENSURE_STACK_INIT_SIZE;
        else {
          mrb->c->esize *= 2;
          if (mrb->c->esize <= mrb->c->eidx) {
            mrb->c->esize = UINT16_MAX;
          }
        }
        mrb->c->ensure = (struct RProc**)realloc(mrb, mrb->c->ensure, sizeof(struct RProc*)*mrb->c->esize);
      }
      /* push ensure stack */
      mrb->c->ensure[mrb->c->eidx++] = p;
      mrb->c->ensure[mrb->c->eidx] = NULL;
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_EPOP, B) {
      callinfo *ci = mrb->c->ci;
      unsigned int n, epos = ci->epos;
      value self = regs[0];
      struct RClass *target_class = ci->target_class;

      if (mrb->c->eidx <= epos) {
        NEXT;
      }

      if (a > (int)mrb->c->eidx - epos)
        a = mrb->c->eidx - epos;
      for (n=0; n<a; n++) {
        int nregs = irep->nregs;

        proc = mrb->c->ensure[epos+n];
        mrb->c->ensure[epos+n] = NULL;
        if (proc == NULL) continue;
        irep = proc->body.irep;
        ci = cipush(mrb);
        ci->mid = ci[-1].mid;
        ci->argc = 0;
        ci->proc = proc;
        ci->stackent = mrb->c->stack;
        ci->target_class = target_class;
        ci->pc = pc;
        ci->acc = nregs;
        mrb->c->stack += ci->acc;
        stack_extend(mrb, irep->nregs);
        regs[0] = self;
        pc = irep->iseq;
      }
      pool = irep->pool;
      syms = irep->syms;
      mrb->c->eidx = epos;
      JUMP;
    }

    CASE(OP_SENDV, BB) {
      c = CALL_MAXARGS;
      goto L_SEND;
    };

    CASE(OP_SENDVB, BB) {
      c = CALL_MAXARGS;
      goto L_SENDB;
    };

    CASE(OP_SEND, BBB)
    L_SEND:
    {
      /* push nil after arguments */
      int bidx = (c == CALL_MAXARGS) ? a+2 : a+c+1;
      SET_NIL_VALUE(regs[bidx]);
      goto L_SENDB;
    };
    L_SEND_SYM:
    {
      /* push nil after arguments */
      int bidx = (c == CALL_MAXARGS) ? a+2 : a+c+1;
      SET_NIL_VALUE(regs[bidx]);
      goto L_SENDB_SYM;
    };

    CASE(OP_SENDB, BBB)
    L_SENDB:
    mid = syms[b];
    L_SENDB_SYM:
    {
      int argc = (c == CALL_MAXARGS) ? -1 : c;
      int bidx = (argc < 0) ? a+2 : a+c+1;
      method_t m;
      struct RClass *cls;
      callinfo *ci = mrb->c->ci;
      value recv, blk;

      assert(bidx < irep->nregs);

      recv = regs[a];
      blk = regs[bidx];
      if (!nil_p(blk) && type(blk) != TT_PROC) {
        blk = convert_type(mrb, blk, TT_PROC, "Proc", "to_proc");
        /* The stack might have been reallocated during convert_type(),
           see #3622 */
        regs[bidx] = blk;
      }
      cls = class(mrb, recv);
      m = method_search_vm(mrb, &cls, mid);
      if (METHOD_UNDEF_P(m)) {
        sym missing = intern_lit(mrb, "method_missing");
        m = method_search_vm(mrb, &cls, missing);
        if (METHOD_UNDEF_P(m) || (missing == mrb->c->ci->mid && obj_eq(mrb, regs[0], recv))) {
          value args = (argc < 0) ? regs[a+1] : a_ary_new_from_values(mrb, c, regs + a + 1);
          ERR_PC_SET(mrb, pc);
          method_missing(mrb, mid, recv, args);
        }
        if (argc >= 0) {
          if (a+2 >= irep->nregs) {
            stack_extend(mrb, a+3);
          }
          regs[a+1] = a_ary_new_from_values(mrb, c, regs + a + 1);
          regs[a+2] = blk;
          argc = -1;
        }
        ary_unshift(mrb, regs[a+1], symbol_value(mid));
        mid = missing;
      }

      /* push callinfo */
      ci = cipush(mrb);
      ci->mid = mid;
      ci->stackent = mrb->c->stack;
      ci->target_class = cls;
      ci->argc = argc;

      ci->pc = pc;
      ci->acc = a;

      /* prepare stack */
      mrb->c->stack += a;

      if (METHOD_CFUNC_P(m)) {
        if (METHOD_PROC_P(m)) {
          struct RProc *p = METHOD_PROC(m);

          ci->proc = p;
          recv = p->body.func(mrb, recv);
        }
        else {
          recv = METHOD_FUNC(m)(mrb, recv);
        }
        gc_arena_restore(mrb, ai);
        gc_arena_shrink(mrb, ai);
        if (mrb->exc) goto L_RAISE;
        ci = mrb->c->ci;
        if (type(blk) == TT_PROC) {
          struct RProc *p = proc_ptr(blk);
          if (p && !PROC_STRICT_P(p) && PROC_ENV(p) == ci[-1].env) {
            p->flags |= PROC_ORPHAN;
          }
        }
        if (!ci->target_class) { /* return from context modifying method (resume/yield) */
          if (ci->acc == CI_ACC_RESUMED) {
            mrb->jmp = prev_jmp;
            return recv;
          }
          else {
            assert(!PROC_CFUNC_P(ci[-1].proc));
            proc = ci[-1].proc;
            irep = proc->body.irep;
            pool = irep->pool;
            syms = irep->syms;
          }
        }
        mrb->c->stack[0] = recv;
        /* pop stackpos */
        mrb->c->stack = ci->stackent;
        pc = ci->pc;
        cipop(mrb);
        JUMP;
      }
      else {
        /* setup environment for calling method */
        proc = ci->proc = METHOD_PROC(m);
        irep = proc->body.irep;
        pool = irep->pool;
        syms = irep->syms;
        stack_extend(mrb, (argc < 0 && irep->nregs < 3) ? 3 : irep->nregs);
        pc = irep->iseq;
        JUMP;
      }
    }

    CASE(OP_CALL, Z) {
      callinfo *ci;
      value recv = mrb->c->stack[0];
      struct RProc *m = proc_ptr(recv);

      /* replace callinfo */
      ci = mrb->c->ci;
      ci->target_class = PROC_TARGET_CLASS(m);
      ci->proc = m;
      if (PROC_ENV_P(m)) {
        sym mid;
        struct REnv *e = PROC_ENV(m);

        mid = e->mid;
        if (mid) ci->mid = mid;
        if (!e->stack) {
          e->stack = mrb->c->stack;
        }
      }

      /* prepare stack */
      if (PROC_CFUNC_P(m)) {
        recv = PROC_CFUNC(m)(mrb, recv);
        gc_arena_restore(mrb, ai);
        gc_arena_shrink(mrb, ai);
        if (mrb->exc) goto L_RAISE;
        /* pop stackpos */
        ci = mrb->c->ci;
        mrb->c->stack = ci->stackent;
        regs[ci->acc] = recv;
        pc = ci->pc;
        cipop(mrb);
        irep = mrb->c->ci->proc->body.irep;
        pool = irep->pool;
        syms = irep->syms;
        JUMP;
      }
      else {
        /* setup environment for calling method */
        proc = m;
        irep = m->body.irep;
        if (!irep) {
          mrb->c->stack[0] = nil_value();
          a = 0;
          c = OP_R_NORMAL;
          goto L_OP_RETURN_BODY;
        }
        pool = irep->pool;
        syms = irep->syms;
        stack_extend(mrb, irep->nregs);
        if (ci->argc < 0) {
          if (irep->nregs > 3) {
            stack_clear(regs+3, irep->nregs-3);
          }
        }
        else if (ci->argc+2 < irep->nregs) {
          stack_clear(regs+ci->argc+2, irep->nregs-ci->argc-2);
        }
        if (PROC_ENV_P(m)) {
          regs[0] = PROC_ENV(m)->stack[0];
        }
        pc = irep->iseq;
        JUMP;
      }
    }

    CASE(OP_SUPER, BB) {
      int argc = (b == CALL_MAXARGS) ? -1 : b;
      int bidx = (argc < 0) ? a+2 : a+b+1;
      method_t m;
      struct RClass *cls;
      callinfo *ci = mrb->c->ci;
      value recv, blk;
      sym mid = ci->mid;
      struct RClass* target_class = PROC_TARGET_CLASS(ci->proc);

      assert(bidx < irep->nregs);

      if (mid == 0 || !target_class) {
        value exc = exc_new_str_lit(mrb, E_NOMETHOD_ERROR, "super called outside of method");
        exc_set(mrb, exc);
        goto L_RAISE;
      }
      if (target_class->tt == TT_MODULE) {
        target_class = ci->target_class;
        if (target_class->tt != TT_ICLASS) {
          value exc = exc_new_str_lit(mrb, E_RUNTIME_ERROR, "superclass info lost [mruby limitations]");
          exc_set(mrb, exc);
          goto L_RAISE;
        }
      }
      recv = regs[0];
      if (!obj_is_kind_of(mrb, recv, target_class)) {
        value exc = exc_new_str_lit(mrb, E_TYPE_ERROR,
                                            "self has wrong type to call super in this context");
        exc_set(mrb, exc);
        goto L_RAISE;
      }
      blk = regs[bidx];
      if (!nil_p(blk) && type(blk) != TT_PROC) {
        blk = convert_type(mrb, blk, TT_PROC, "Proc", "to_proc");
        /* The stack or ci stack might have been reallocated during
           convert_type(), see #3622 and #3784 */
        regs[bidx] = blk;
        ci = mrb->c->ci;
      }
      cls = target_class->super;
      m = method_search_vm(mrb, &cls, mid);
      if (METHOD_UNDEF_P(m)) {
        sym missing = intern_lit(mrb, "method_missing");

        if (mid != missing) {
          cls = class(mrb, recv);
        }
        m = method_search_vm(mrb, &cls, missing);
        if (METHOD_UNDEF_P(m)) {
          value args = (argc < 0) ? regs[a+1] : a_ary_new_from_values(mrb, b, regs + a + 1);
          ERR_PC_SET(mrb, pc);
          method_missing(mrb, mid, recv, args);
        }
        mid = missing;
        if (argc >= 0) {
          if (a+2 >= irep->nregs) {
            stack_extend(mrb, a+3);
          }
          regs[a+1] = a_ary_new_from_values(mrb, b, regs + a + 1);
          regs[a+2] = blk;
          argc = -1;
        }
        ary_unshift(mrb, regs[a+1], symbol_value(ci->mid));
      }

      /* push callinfo */
      ci = cipush(mrb);
      ci->mid = mid;
      ci->stackent = mrb->c->stack;
      ci->target_class = cls;
      ci->pc = pc;
      ci->argc = argc;

      /* prepare stack */
      mrb->c->stack += a;
      mrb->c->stack[0] = recv;

      if (METHOD_CFUNC_P(m)) {
        value v;

        if (METHOD_PROC_P(m)) {
          ci->proc = METHOD_PROC(m);
        }
        v = METHOD_CFUNC(m)(mrb, recv);
        gc_arena_restore(mrb, ai);
        if (mrb->exc) goto L_RAISE;
        ci = mrb->c->ci;
        if (!ci->target_class) { /* return from context modifying method (resume/yield) */
          if (ci->acc == CI_ACC_RESUMED) {
            mrb->jmp = prev_jmp;
            return v;
          }
          else {
            assert(!PROC_CFUNC_P(ci[-1].proc));
            proc = ci[-1].proc;
            irep = proc->body.irep;
            pool = irep->pool;
            syms = irep->syms;
          }
        }
        mrb->c->stack[0] = v;
        /* pop stackpos */
        mrb->c->stack = ci->stackent;
        pc = ci->pc;
        cipop(mrb);
        JUMP;
      }
      else {
        /* fill callinfo */
        ci->acc = a;

        /* setup environment for calling method */
        proc = ci->proc = METHOD_PROC(m);
        irep = proc->body.irep;
        pool = irep->pool;
        syms = irep->syms;
        stack_extend(mrb, (argc < 0 && irep->nregs < 3) ? 3 : irep->nregs);
        pc = irep->iseq;
        JUMP;
      }
    }

    CASE(OP_ARGARY, BS) {
      int m1 = (b>>11)&0x3f;
      int r  = (b>>10)&0x1;
      int m2 = (b>>5)&0x1f;
      int kd = (b>>4)&0x1;
      int lv = (b>>0)&0xf;
      value *stack;

      if (mrb->c->ci->mid == 0 || mrb->c->ci->target_class == NULL) {
        value exc;

      L_NOSUPER:
        exc = exc_new_str_lit(mrb, E_NOMETHOD_ERROR, "super called outside of method");
        exc_set(mrb, exc);
        goto L_RAISE;
      }
      if (lv == 0) stack = regs + 1;
      else {
        struct REnv *e = uvenv(mrb, lv-1);
        if (!e) goto L_NOSUPER;
        if (ENV_STACK_LEN(e) <= m1+r+m2+kd+1)
          goto L_NOSUPER;
        stack = e->stack + 1;
      }
      if (r == 0) {
        regs[a] = a_ary_new_from_values(mrb, m1 + m2 + kd, stack);
      }
      else {
        value *pp = NULL;
        struct RArray *rest;
        int len = 0;

        if (array_p(stack[m1])) {
          struct RArray *ary = ary_ptr(stack[m1]);

          pp = ARY_PTR(ary);
          len = (int)ARY_LEN(ary);
        }
        regs[a] = a_ary_new_capa(mrb, m1 + len + m2 + kd);
        rest = ary_ptr(regs[a]);
        if (m1 > 0) {
          stack_copy(ARY_PTR(rest), stack, m1);
        }
        if (len > 0) {
          stack_copy(ARY_PTR(rest)+m1, pp, len);
        }
        if (m2 > 0) {
          stack_copy(ARY_PTR(rest)+m1+len, stack+m1+1, m2);
        }
        if (kd) {
          stack_copy(ARY_PTR(rest)+m1+len+m2, stack+m1+m2+1, kd);
        }
        ARY_SET_LEN(rest, m1+len+m2+kd);
      }
      regs[a+1] = stack[m1+r+m2];
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_ENTER, W) {
      int m1 = ASPEC_REQ(a);
      int o  = ASPEC_OPT(a);
      int r  = ASPEC_REST(a);
      int m2 = ASPEC_POST(a);
      int kd = (ASPEC_KEY(a) > 0 || ASPEC_KDICT(a))? 1 : 0;
      /* unused
      int b  = ASPEC_BLOCK(a);
      */
      int argc = mrb->c->ci->argc;
      value *argv = regs+1;
      value * const argv0 = argv;
      int const len = m1 + o + r + m2;
      int const blk_pos = len + kd + 1;
      value *blk = &argv[argc < 0 ? 1 : argc];
      value kdict;
      int kargs = kd;

      /* arguments is passed with Array */
      if (argc < 0) {
        struct RArray *ary = ary_ptr(regs[1]);
        argv = ARY_PTR(ary);
        argc = (int)ARY_LEN(ary);
        gc_protect(mrb, regs[1]);
      }

      /* strict argument check */
      if (mrb->c->ci->proc && PROC_STRICT_P(mrb->c->ci->proc)) {
        if (argc < m1 + m2 || (r == 0 && argc > len + kd)) {
          argnum_error(mrb, m1+m2);
          goto L_RAISE;
        }
      }
      /* extract first argument array to arguments */
      else if (len > 1 && argc == 1 && array_p(argv[0])) {
        gc_protect(mrb, argv[0]);
        argc = (int)RARRAY_LEN(argv[0]);
        argv = RARRAY_PTR(argv[0]);
      }

      if (kd) {
        /* check last arguments is hash if method takes keyword arguments */
        if (argc == m1+m2) {
          kdict = hash_new(mrb);
          kargs = 0;
        }
        else {
          if (argv && argc > 0 && hash_p(argv[argc-1])) {
            kdict = argv[argc-1];
            hash_check_kdict(mrb, kdict);
          }
          else if (r || argc <= m1+m2+o) {
            kdict = hash_new(mrb);
            kargs = 0;
          }
          else {
            argnum_error(mrb, m1+m2);
            goto L_RAISE;
          }
          if (ASPEC_KEY(a) > 0) {
            kdict = hash_dup(mrb, kdict);
          }
        }
      }

      /* no rest arguments */
      if (argc-kargs < len) {
        int mlen = m2;
        if (argc < m1+m2) {
          mlen = m1 < argc ? argc - m1 : 0;
        }
        regs[blk_pos] = *blk; /* move block */
        if (kd) regs[len + 1] = kdict;

        /* copy mandatory and optional arguments */
        if (argv0 != argv) {
          value_move(&regs[1], argv, argc-mlen); /* m1 + o */
        }
        if (argc < m1) {
          stack_clear(&regs[argc+1], m1-argc);
        }
        /* copy post mandatory arguments */
        if (mlen) {
          value_move(&regs[len-m2+1], &argv[argc-mlen], mlen);
        }
        if (mlen < m2) {
          stack_clear(&regs[len-m2+mlen+1], m2-mlen);
        }
        /* initalize rest arguments with empty Array */
        if (r) {
          regs[m1+o+1] = a_ary_new_capa(mrb, 0);
        }
        /* skip initailizer of passed arguments */
        if (o > 0 && argc-kargs > m1+m2)
          pc += (argc - kargs - m1 - m2)*3;
      }
      else {
        int rnum = 0;
        if (argv0 != argv) {
          regs[blk_pos] = *blk; /* move block */
          if (kd) regs[len + 1] = kdict;
          value_move(&regs[1], argv, m1+o);
        }
        if (r) {
          value ary;

          rnum = argc-m1-o-m2-kargs;
          ary = a_ary_new_from_values(mrb, rnum, argv + m1 + o);
          regs[m1+o+1] = ary;
        }
        if (m2) {
          if (argc-m2 > m1) {
            value_move(&regs[m1+o+r+1], &argv[m1+o+rnum], m2);
          }
        }
        if (argv0 == argv) {
          regs[blk_pos] = *blk; /* move block */
          if (kd) regs[len + 1] = kdict;
        }
        pc += o*3;
      }

      /* format arguments for generated code */
      mrb->c->ci->argc = len + kd;

      /* clear local (but non-argument) variables */
      if (irep->nlocals-blk_pos-1 > 0) {
        stack_clear(&regs[blk_pos+1], irep->nlocals-blk_pos-1);
      }
      JUMP;
    }

    CASE(OP_KARG, BB) {
      value k = symbol_value(syms[b]);
      value kdict = regs[mrb->c->ci->argc];

      if (!hash_key_p(mrb, kdict, k)) {
        value str = format(mrb, "missing keyword: %S", k);
        exc_set(mrb, exc_new_str(mrb, E_ARGUMENT_ERROR, str));
        goto L_RAISE;
      }
      regs[a] = hash_get(mrb, kdict, k);
      hash_delete_key(mrb, kdict, k);
      NEXT;
    }

    CASE(OP_KEY_P, BB) {
      value k = symbol_value(syms[b]);
      value kdict = regs[mrb->c->ci->argc];
      bool key_p = hash_key_p(mrb, kdict, k);

      regs[a] = bool_value(key_p);
      NEXT;
    }

    CASE(OP_KEYEND, Z) {
      value kdict = regs[mrb->c->ci->argc];

      if (hash_p(kdict) && !hash_empty_p(mrb, kdict)) {
        value keys = hash_keys(mrb, kdict);
        value key1 = RARRAY_PTR(keys)[0];
        value str = format(mrb, "unknown keyword: %S", key1);
        exc_set(mrb, exc_new_str(mrb, E_ARGUMENT_ERROR, str));
        goto L_RAISE;
      }
      NEXT;
    }

    CASE(OP_BREAK, B) {
      c = OP_R_BREAK;
      goto L_RETURN;
    }
    CASE(OP_RETURN_BLK, B) {
      c = OP_R_RETURN;
      goto L_RETURN;
    }
    CASE(OP_RETURN, B)
    c = OP_R_NORMAL;
    L_RETURN:
    {
       callinfo *ci;

#define ecall_adjust() do {\
  ptrdiff_t cioff = ci - mrb->c->cibase;\
  ecall(mrb);\
  ci = mrb->c->cibase + cioff;\
} while (0)

      ci = mrb->c->ci;
      if (ci->mid) {
        value blk;

        if (ci->argc < 0) {
          blk = regs[2];
        }
        else {
          blk = regs[ci->argc+1];
        }
        if (type(blk) == TT_PROC) {
          struct RProc *p = proc_ptr(blk);

          if (!PROC_STRICT_P(p) &&
              ci > mrb->c->cibase && PROC_ENV(p) == ci[-1].env) {
            p->flags |= PROC_ORPHAN;
          }
        }
      }

      if (mrb->exc) {
        callinfo *ci0;

      L_RAISE:
        ci0 = ci = mrb->c->ci;
        if (ci == mrb->c->cibase) {
          if (ci->ridx == 0) goto L_FTOP;
          goto L_RESCUE;
        }
        while (ci[0].ridx == ci[-1].ridx) {
          cipop(mrb);
          mrb->c->stack = ci->stackent;
          if (ci->acc == CI_ACC_SKIP && prev_jmp) {
            mrb->jmp = prev_jmp;
            THROW(prev_jmp);
          }
          ci = mrb->c->ci;
          if (ci == mrb->c->cibase) {
            if (ci->ridx == 0) {
            L_FTOP:             /* fiber top */
              if (mrb->c == mrb->root_c) {
                mrb->c->stack = mrb->c->stbase;
                goto L_STOP;
              }
              else {
                struct context *c = mrb->c;

                while (c->eidx > ci->epos) {
                  ecall_adjust();
                }
                c->status = FIBER_TERMINATED;
                mrb->c = c->prev;
                c->prev = NULL;
                goto L_RAISE;
              }
            }
            break;
          }
          /* call ensure only when we skip this callinfo */
          if (ci[0].ridx == ci[-1].ridx) {
            while (mrb->c->eidx > ci->epos) {
              ecall_adjust();
            }
          }
        }
      L_RESCUE:
        if (ci->ridx == 0) goto L_STOP;
        proc = ci->proc;
        irep = proc->body.irep;
        pool = irep->pool;
        syms = irep->syms;
        if (ci < ci0) {
          mrb->c->stack = ci[1].stackent;
        }
        stack_extend(mrb, irep->nregs);
        pc = irep->iseq+mrb->c->rescue[--ci->ridx];
      }
      else {
        int acc;
        value v;
        struct RProc *dst;

        ci = mrb->c->ci;
        v = regs[a];
        gc_protect(mrb, v);
        switch (c) {
        case OP_R_RETURN:
          /* Fall through to OP_R_NORMAL otherwise */
          if (ci->acc >=0 && PROC_ENV_P(proc) && !PROC_STRICT_P(proc)) {
            callinfo *cibase = mrb->c->cibase;
            dst = top_proc(mrb, proc);

            if (PROC_ENV_P(dst)) {
              struct REnv *e = PROC_ENV(dst);

              if (!ENV_STACK_SHARED_P(e) || e->cxt != mrb->c) {
                localjump_error(mrb, LOCALJUMP_ERROR_RETURN);
                goto L_RAISE;
              }
            }
            while (cibase <= ci && ci->proc != dst) {
              if (ci->acc < 0) {
                localjump_error(mrb, LOCALJUMP_ERROR_RETURN);
                goto L_RAISE;
              }
              ci--;
            }
            if (ci <= cibase) {
              localjump_error(mrb, LOCALJUMP_ERROR_RETURN);
              goto L_RAISE;
            }
            break;
          }
          /* fallthrough */
        case OP_R_NORMAL:
        NORMAL_RETURN:
          if (ci == mrb->c->cibase) {
            struct context *c = mrb->c;

            if (!c->prev) { /* toplevel return */
              regs[irep->nlocals] = v;
              goto L_STOP;
            }
            if (c->prev->ci == c->prev->cibase) {
              value exc = exc_new_str_lit(mrb, E_FIBER_ERROR, "double resume");
              exc_set(mrb, exc);
              goto L_RAISE;
            }
            while (c->eidx > 0) {
              ecall(mrb);
            }
            /* automatic yield at the end */
            c->status = FIBER_TERMINATED;
            mrb->c = c->prev;
            c->prev = NULL;
            mrb->c->status = FIBER_RUNNING;
            ci = mrb->c->ci;
          }
          break;
        case OP_R_BREAK:
          if (PROC_STRICT_P(proc)) goto NORMAL_RETURN;
          if (PROC_ORPHAN_P(proc)) {
            value exc;

          L_BREAK_ERROR:
            exc = exc_new_str_lit(mrb, E_LOCALJUMP_ERROR,
                                      "break from proc-closure");
            exc_set(mrb, exc);
            goto L_RAISE;
          }
          if (!PROC_ENV_P(proc) || !ENV_STACK_SHARED_P(PROC_ENV(proc))) {
            goto L_BREAK_ERROR;
          }
          else {
            struct REnv *e = PROC_ENV(proc);

            if (e->cxt != mrb->c) {
              goto L_BREAK_ERROR;
            }
          }
          while (mrb->c->eidx > mrb->c->ci->epos) {
            ecall_adjust();
          }
          /* break from fiber block */
          if (ci == mrb->c->cibase && ci->pc) {
            struct context *c = mrb->c;

            mrb->c = c->prev;
            c->prev = NULL;
            ci = mrb->c->ci;
          }
          if (ci->acc < 0) {
            gc_arena_restore(mrb, ai);
            mrb->c->vmexec = FALSE;
            mrb->exc = (struct RObject*)break_new(mrb, proc, v);
            mrb->jmp = prev_jmp;
            THROW(prev_jmp);
          }
          if (FALSE) {
          L_BREAK:
            v = ((struct RBreak*)mrb->exc)->val;
            proc = ((struct RBreak*)mrb->exc)->proc;
            mrb->exc = NULL;
            ci = mrb->c->ci;
          }
          mrb->c->stack = ci->stackent;
          proc = proc->upper;
          while (mrb->c->cibase < ci &&  ci[-1].proc != proc) {
            if (ci[-1].acc == CI_ACC_SKIP) {
              while (ci < mrb->c->ci) {
                cipop(mrb);
              }
              goto L_BREAK_ERROR;
            }
            ci--;
          }
          if (ci == mrb->c->cibase) {
            goto L_BREAK_ERROR;
          }
          break;
        default:
          /* cannot happen */
          break;
        }
        while (ci < mrb->c->ci) {
          cipop(mrb);
        }
        ci[0].ridx = ci[-1].ridx;
        while (mrb->c->eidx > ci->epos) {
          ecall_adjust();
        }
        if (mrb->c->vmexec && !ci->target_class) {
          gc_arena_restore(mrb, ai);
          mrb->c->vmexec = FALSE;
          mrb->jmp = prev_jmp;
          return v;
        }
        acc = ci->acc;
        mrb->c->stack = ci->stackent;
        cipop(mrb);
        if (acc == CI_ACC_SKIP || acc == CI_ACC_DIRECT) {
          gc_arena_restore(mrb, ai);
          mrb->jmp = prev_jmp;
          return v;
        }
        pc = ci->pc;
        ci = mrb->c->ci;
        DEBUG(fprintf(stderr, "from :%s\n", sym2name(mrb, ci->mid)));
        proc = mrb->c->ci->proc;
        irep = proc->body.irep;
        pool = irep->pool;
        syms = irep->syms;

        regs[acc] = v;
        gc_arena_restore(mrb, ai);
      }
      JUMP;
    }

    CASE(OP_BLKPUSH, BS) {
      int m1 = (b>>11)&0x3f;
      int r  = (b>>10)&0x1;
      int m2 = (b>>5)&0x1f;
      int kd = (b>>4)&0x1;
      int lv = (b>>0)&0xf;
      value *stack;

      if (lv == 0) stack = regs + 1;
      else {
        struct REnv *e = uvenv(mrb, lv-1);
        if (!e || (!ENV_STACK_SHARED_P(e) && e->mid == 0) ||
            ENV_STACK_LEN(e) <= m1+r+m2+1) {
          localjump_error(mrb, LOCALJUMP_ERROR_YIELD);
          goto L_RAISE;
        }
        stack = e->stack + 1;
      }
      if (nil_p(stack[m1+r+m2])) {
        localjump_error(mrb, LOCALJUMP_ERROR_YIELD);
        goto L_RAISE;
      }
      regs[a] = stack[m1+r+m2+kd];
      NEXT;
    }

#define TYPES2(a,b) ((((uint16_t)(a))<<8)|(((uint16_t)(b))&0xff))
#define OP_MATH_BODY(op,v1,v2) do {\
  v1(regs[a]) = v1(regs[a]) op v2(regs[a+1]);\
} while(0)

    CASE(OP_ADD, B) {
      /* need to check if op is overridden */
      switch (TYPES2(type(regs[a]),type(regs[a+1]))) {
      case TYPES2(TT_FIXNUM,TT_FIXNUM):
        {
          int x, y, z;
          value *regs_a = regs + a;

          x = fixnum(regs_a[0]);
          y = fixnum(regs_a[1]);
          if (int_add_overflow(x, y, &z)) {
#ifndef WITHOUT_FLOAT
            SET_FLOAT_VALUE(mrb, regs_a[0], (float)x + (float)y);
            break;
#endif
          }
          SET_INT_VALUE(regs[a], z);
        }
        break;
#ifndef WITHOUT_FLOAT
      case TYPES2(TT_FIXNUM,TT_FLOAT):
        {
          int x = fixnum(regs[a]);
          float y = float(regs[a+1]);
          SET_FLOAT_VALUE(mrb, regs[a], (float)x + y);
        }
        break;
      case TYPES2(TT_FLOAT,TT_FIXNUM):
#ifdef WORD_BOXING
        {
          float x = float(regs[a]);
          int y = fixnum(regs[a+1]);
          SET_FLOAT_VALUE(mrb, regs[a], x + y);
        }
#else
        OP_MATH_BODY(+,float,fixnum);
#endif
        break;
      case TYPES2(TT_FLOAT,TT_FLOAT):
#ifdef WORD_BOXING
        {
          float x = float(regs[a]);
          float y = float(regs[a+1]);
          SET_FLOAT_VALUE(mrb, regs[a], x + y);
        }
#else
        OP_MATH_BODY(+,float,float);
#endif
        break;
#endif
      case TYPES2(TT_STRING,TT_STRING):
        regs[a] = str_plus(mrb, regs[a], regs[a+1]);
        break;
      default:
        c = 1;
        mid = intern_lit(mrb, "+");
        goto L_SEND_SYM;
      }
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_SUB, B) {
      /* need to check if op is overridden */
      switch (TYPES2(type(regs[a]),type(regs[a+1]))) {
      case TYPES2(TT_FIXNUM,TT_FIXNUM):
        {
          int x, y, z;

          x = fixnum(regs[a]);
          y = fixnum(regs[a+1]);
          if (int_sub_overflow(x, y, &z)) {
#ifndef WITHOUT_FLOAT
            SET_FLOAT_VALUE(mrb, regs[a], (float)x - (float)y);
            break;
#endif
          }
          SET_INT_VALUE(regs[a], z);
        }
        break;
#ifndef WITHOUT_FLOAT
      case TYPES2(TT_FIXNUM,TT_FLOAT):
        {
          int x = fixnum(regs[a]);
          float y = float(regs[a+1]);
          SET_FLOAT_VALUE(mrb, regs[a], (float)x - y);
        }
        break;
      case TYPES2(TT_FLOAT,TT_FIXNUM):
#ifdef WORD_BOXING
        {
          float x = float(regs[a]);
          int y = fixnum(regs[a+1]);
          SET_FLOAT_VALUE(mrb, regs[a], x - y);
        }
#else
        OP_MATH_BODY(-,float,fixnum);
#endif
        break;
      case TYPES2(TT_FLOAT,TT_FLOAT):
#ifdef WORD_BOXING
        {
          float x = float(regs[a]);
          float y = float(regs[a+1]);
          SET_FLOAT_VALUE(mrb, regs[a], x - y);
        }
#else
        OP_MATH_BODY(-,float,float);
#endif
        break;
#endif
      default:
        c = 1;
        mid = intern_lit(mrb, "-");
        goto L_SEND_SYM;
      }
      NEXT;
    }

    CASE(OP_MUL, B) {
      /* need to check if op is overridden */
      switch (TYPES2(type(regs[a]),type(regs[a+1]))) {
      case TYPES2(TT_FIXNUM,TT_FIXNUM):
        {
          int x, y, z;

          x = fixnum(regs[a]);
          y = fixnum(regs[a+1]);
          if (int_mul_overflow(x, y, &z)) {
#ifndef WITHOUT_FLOAT
            SET_FLOAT_VALUE(mrb, regs[a], (float)x * (float)y);
            break;
#endif
          }
          SET_INT_VALUE(regs[a], z);
        }
        break;
#ifndef WITHOUT_FLOAT
      case TYPES2(TT_FIXNUM,TT_FLOAT):
        {
          int x = fixnum(regs[a]);
          float y = float(regs[a+1]);
          SET_FLOAT_VALUE(mrb, regs[a], (float)x * y);
        }
        break;
      case TYPES2(TT_FLOAT,TT_FIXNUM):
#ifdef WORD_BOXING
        {
          float x = float(regs[a]);
          int y = fixnum(regs[a+1]);
          SET_FLOAT_VALUE(mrb, regs[a], x * y);
        }
#else
        OP_MATH_BODY(*,float,fixnum);
#endif
        break;
      case TYPES2(TT_FLOAT,TT_FLOAT):
#ifdef WORD_BOXING
        {
          float x = float(regs[a]);
          float y = float(regs[a+1]);
          SET_FLOAT_VALUE(mrb, regs[a], x * y);
        }
#else
        OP_MATH_BODY(*,float,float);
#endif
        break;
#endif
      default:
        c = 1;
        mid = intern_lit(mrb, "*");
        goto L_SEND_SYM;
      }
      NEXT;
    }

    CASE(OP_DIV, B) {
#ifndef WITHOUT_FLOAT
      double x, y, f;
#endif

      /* need to check if op is overridden */
      switch (TYPES2(type(regs[a]),type(regs[a+1]))) {
      case TYPES2(TT_FIXNUM,TT_FIXNUM):
#ifdef WITHOUT_FLOAT
        {
          int x = fixnum(regs[a]);
          int y = fixnum(regs[a+1]);
          SET_INT_VALUE(regs[a], y ? x / y : 0);
        }
        break;
#else
        x = (float)fixnum(regs[a]);
        y = (float)fixnum(regs[a+1]);
        break;
      case TYPES2(TT_FIXNUM,TT_FLOAT):
        x = (float)fixnum(regs[a]);
        y = float(regs[a+1]);
        break;
      case TYPES2(TT_FLOAT,TT_FIXNUM):
        x = float(regs[a]);
        y = (float)fixnum(regs[a+1]);
        break;
      case TYPES2(TT_FLOAT,TT_FLOAT):
        x = float(regs[a]);
        y = float(regs[a+1]);
        break;
#endif
      default:
        c = 1;
        mid = intern_lit(mrb, "/");
        goto L_SEND_SYM;
      }

#ifndef WITHOUT_FLOAT
      if (y == 0) {
        if (x > 0) f = INFINITY;
        else if (x < 0) f = -INFINITY;
        else /* if (x == 0) */ f = NAN;
      }
      else {
        f = x / y;
      }
      SET_FLOAT_VALUE(mrb, regs[a], f);
#endif
      NEXT;
    }

    CASE(OP_ADDI, BB) {
      /* need to check if + is overridden */
      switch (type(regs[a])) {
      case TT_FIXNUM:
        {
          int x = fixnum(regs[a]);
          int y = (int)b;
          int z;

          if (int_add_overflow(x, y, &z)) {
#ifndef WITHOUT_FLOAT
            SET_FLOAT_VALUE(mrb, regs[a], (float)x + (float)y);
            break;
#endif
          }
          SET_INT_VALUE(regs[a], z);
        }
        break;
#ifndef WITHOUT_FLOAT
      case TT_FLOAT:
#ifdef WORD_BOXING
        {
          float x = float(regs[a]);
          SET_FLOAT_VALUE(mrb, regs[a], x + b);
        }
#else
        float(regs[a]) += b;
#endif
        break;
#endif
      default:
        SET_INT_VALUE(regs[a+1], b);
        c = 1;
        mid = intern_lit(mrb, "+");
        goto L_SEND_SYM;
      }
      NEXT;
    }

    CASE(OP_SUBI, BB) {
      value *regs_a = regs + a;

      /* need to check if + is overridden */
      switch (type(regs_a[0])) {
      case TT_FIXNUM:
        {
          int x = fixnum(regs_a[0]);
          int y = (int)b;
          int z;

          if (int_sub_overflow(x, y, &z)) {
#ifndef WITHOUT_FLOAT
            SET_FLOAT_VALUE(mrb, regs_a[0], (float)x - (float)y);
            break;
#endif
          }
          SET_INT_VALUE(regs_a[0], z);
        }
        break;
#ifndef WITHOUT_FLOAT
      case TT_FLOAT:
#ifdef WORD_BOXING
        {
          float x = float(regs[a]);
          SET_FLOAT_VALUE(mrb, regs[a], (float)x - (float)b);
        }
#else
        float(regs_a[0]) -= b;
#endif
        break;
#endif
      default:
        SET_INT_VALUE(regs_a[1], b);
        c = 1;
        mid = intern_lit(mrb, "-");
        goto L_SEND_SYM;
      }
      NEXT;
    }

#define OP_CMP_BODY(op,v1,v2) (v1(regs[a]) op v2(regs[a+1]))

#ifdef WITHOUT_FLOAT
#define OP_CMP(op) do {\
  int result;\
  /* need to check if - is overridden */\
  switch (TYPES2(type(regs[a]),type(regs[a+1]))) {\
  case TYPES2(TT_FIXNUM,TT_FIXNUM):\
    result = OP_CMP_BODY(op,fixnum,fixnum);\
    break;\
  default:\
    c = 1;\
    mid = intern_lit(mrb, # op);\
    goto L_SEND_SYM;\
  }\
  if (result) {\
    SET_TRUE_VALUE(regs[a]);\
  }\
  else {\
    SET_FALSE_VALUE(regs[a]);\
  }\
} while(0)
#else
#define OP_CMP(op) do {\
  int result;\
  /* need to check if - is overridden */\
  switch (TYPES2(type(regs[a]),type(regs[a+1]))) {\
  case TYPES2(TT_FIXNUM,TT_FIXNUM):\
    result = OP_CMP_BODY(op,fixnum,fixnum);\
    break;\
  case TYPES2(TT_FIXNUM,TT_FLOAT):\
    result = OP_CMP_BODY(op,fixnum,float);\
    break;\
  case TYPES2(TT_FLOAT,TT_FIXNUM):\
    result = OP_CMP_BODY(op,float,fixnum);\
    break;\
  case TYPES2(TT_FLOAT,TT_FLOAT):\
    result = OP_CMP_BODY(op,float,float);\
    break;\
  default:\
    c = 1;\
    mid = intern_lit(mrb, # op);\
    goto L_SEND_SYM;\
  }\
  if (result) {\
    SET_TRUE_VALUE(regs[a]);\
  }\
  else {\
    SET_FALSE_VALUE(regs[a]);\
  }\
} while(0)
#endif

    CASE(OP_EQ, B) {
      if (obj_eq(mrb, regs[a], regs[a+1])) {
        SET_TRUE_VALUE(regs[a]);
      }
      else {
        OP_CMP(==);
      }
      NEXT;
    }

    CASE(OP_LT, B) {
      OP_CMP(<);
      NEXT;
    }

    CASE(OP_LE, B) {
      OP_CMP(<=);
      NEXT;
    }

    CASE(OP_GT, B) {
      OP_CMP(>);
      NEXT;
    }

    CASE(OP_GE, B) {
      OP_CMP(>=);
      NEXT;
    }

    CASE(OP_ARRAY, BB) {
      value v = a_ary_new_from_values(mrb, b, &regs[a]);
      regs[a] = v;
      gc_arena_restore(mrb, ai);
      NEXT;
    }
    CASE(OP_ARRAY2, BBB) {
      value v = a_ary_new_from_values(mrb, c, &regs[b]);
      regs[a] = v;
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_ARYCAT, B) {
      value splat = ary_splat(mrb, regs[a+1]);
      ary_concat(mrb, regs[a], splat);
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_ARYPUSH, B) {
      ary_push(mrb, regs[a], regs[a+1]);
      NEXT;
    }

    CASE(OP_ARYDUP, B) {
      value ary = regs[a];
      if (array_p(ary)) {
        ary = a_ary_new_from_values(mrb, RARRAY_LEN(ary), RARRAY_PTR(ary));
      }
      else {
        ary = a_ary_new_from_values(mrb, 1, &ary);
      }
      regs[a] = ary;
      NEXT;
    }

    CASE(OP_AREF, BBB) {
      value v = regs[b];

      if (!array_p(v)) {
        if (c == 0) {
          regs[a] = v;
        }
        else {
          SET_NIL_VALUE(regs[a]);
        }
      }
      else {
        v = ary_ref(mrb, v, c);
        regs[a] = v;
      }
      NEXT;
    }

    CASE(OP_ASET, BBB) {
      ary_set(mrb, regs[b], c, regs[a]);
      NEXT;
    }

    CASE(OP_APOST, BBB) {
      value v = regs[a];
      int pre  = b;
      int post = c;
      struct RArray *ary;
      int len, idx;

      if (!array_p(v)) {
        v = a_ary_new_from_values(mrb, 1, &regs[a]);
      }
      ary = ary_ptr(v);
      len = (int)ARY_LEN(ary);
      if (len > pre + post) {
        v = a_ary_new_from_values(mrb, len - pre - post, ARY_PTR(ary) + pre);
        regs[a++] = v;
        while (post--) {
          regs[a++] = ARY_PTR(ary)[len-post-1];
        }
      }
      else {
        v = a_ary_new_capa(mrb, 0);
        regs[a++] = v;
        for (idx=0; idx+pre<len; idx++) {
          regs[a+idx] = ARY_PTR(ary)[pre+idx];
        }
        while (idx < post) {
          SET_NIL_VALUE(regs[a+idx]);
          idx++;
        }
      }
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_INTERN, B) {
      sym sym = intern_str(mrb, regs[a]);

      regs[a] = symbol_value(sym);
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_STRING, BB) {
      value str = str_dup(mrb, pool[b]);

      regs[a] = str;
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_STRCAT, B) {
      str_concat(mrb, regs[a], regs[a+1]);
      NEXT;
    }

    CASE(OP_HASH, BB) {
      value hash = hash_new_capa(mrb, b);
      int i;
      int lim = a+b*2;

      for (i=a; i<lim; i+=2) {
        hash_set(mrb, hash, regs[i], regs[i+1]);
      }
      regs[a] = hash;
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_HASHADD, BB) {
      value hash;
      int i;
      int lim = a+b*2+1;

      hash = ensure_hash_type(mrb, regs[a]);
      for (i=a+1; i<lim; i+=2) {
        hash_set(mrb, hash, regs[i], regs[i+1]);
      }
      gc_arena_restore(mrb, ai);
      NEXT;
    }
    CASE(OP_HASHCAT, B) {
      value hash = ensure_hash_type(mrb, regs[a]);

      hash_merge(mrb, hash, regs[a+1]);
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_LAMBDA, BB)
    c = OP_L_LAMBDA;
    L_MAKE_LAMBDA:
    {
      struct RProc *p;
      irep *nirep = irep->reps[b];

      if (c & OP_L_CAPTURE) {
        p = closure_new(mrb, nirep);
      }
      else {
        p = proc_new(mrb, nirep);
        p->flags |= PROC_SCOPE;
      }
      if (c & OP_L_STRICT) p->flags |= PROC_STRICT;
      regs[a] = obj_value(p);
      gc_arena_restore(mrb, ai);
      NEXT;
    }
    CASE(OP_BLOCK, BB) {
      c = OP_L_BLOCK;
      goto L_MAKE_LAMBDA;
    }
    CASE(OP_METHOD, BB) {
      c = OP_L_METHOD;
      goto L_MAKE_LAMBDA;
    }

    CASE(OP_RANGE_INC, B) {
      value val = range_new(mrb, regs[a], regs[a+1], FALSE);
      regs[a] = val;
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_RANGE_EXC, B) {
      value val = range_new(mrb, regs[a], regs[a+1], TRUE);
      regs[a] = val;
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_OCLASS, B) {
      regs[a] = obj_value(mrb->object_class);
      NEXT;
    }

    CASE(OP_CLASS, BB) {
      struct RClass *c = 0, *baseclass;
      value base, super;
      sym id = syms[b];

      base = regs[a];
      super = regs[a+1];
      if (nil_p(base)) {
        baseclass = PROC_TARGET_CLASS(mrb->c->ci->proc);
        base = obj_value(baseclass);
      }
      c = vm_define_class(mrb, base, super, id);
      regs[a] = obj_value(c);
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_MODULE, BB) {
      struct RClass *cls = 0, *baseclass;
      value base;
      sym id = syms[b];

      base = regs[a];
      if (nil_p(base)) {
        baseclass = PROC_TARGET_CLASS(mrb->c->ci->proc);
        base = obj_value(baseclass);
      }
      cls = vm_define_module(mrb, base, id);
      regs[a] = obj_value(cls);
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_EXEC, BB) {
      callinfo *ci;
      value recv = regs[a];
      struct RProc *p;
      irep *nirep = irep->reps[b];

      /* prepare closure */
      p = proc_new(mrb, nirep);
      p->c = NULL;
      field_write_barrier(mrb, (struct RBasic*)p, (struct RBasic*)proc);
      PROC_SET_TARGET_CLASS(p, class_ptr(recv));
      p->flags |= PROC_SCOPE;

      /* prepare call stack */
      ci = cipush(mrb);
      ci->pc = pc;
      ci->acc = a;
      ci->mid = 0;
      ci->stackent = mrb->c->stack;
      ci->argc = 0;
      ci->target_class = class_ptr(recv);

      /* prepare stack */
      mrb->c->stack += a;

      /* setup block to call */
      ci->proc = p;

      irep = p->body.irep;
      pool = irep->pool;
      syms = irep->syms;
      stack_extend(mrb, irep->nregs);
      stack_clear(regs+1, irep->nregs-1);
      pc = irep->iseq;
      JUMP;
    }

    CASE(OP_DEF, BB) {
      struct RClass *target = class_ptr(regs[a]);
      struct RProc *p = proc_ptr(regs[a+1]);
      method_t m;

      METHOD_FROM_PROC(m, p);
      define_method_raw(mrb, target, syms[b], m);
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_SCLASS, B) {
      regs[a] = singleton_class(mrb, regs[a]);
      gc_arena_restore(mrb, ai);
      NEXT;
    }

    CASE(OP_TCLASS, B) {
      if (!check_target_class(mrb)) goto L_RAISE;
      regs[a] = obj_value(mrb->c->ci->target_class);
      NEXT;
    }

    CASE(OP_ALIAS, BB) {
      struct RClass *target;

      if (!check_target_class(mrb)) goto L_RAISE;
      target = mrb->c->ci->target_class;
      alias_method(mrb, target, syms[a], syms[b]);
      NEXT;
    }
    CASE(OP_UNDEF, B) {
      struct RClass *target;

      if (!check_target_class(mrb)) goto L_RAISE;
      target = mrb->c->ci->target_class;
      undef_method_id(mrb, target, syms[a]);
      NEXT;
    }

    CASE(OP_DEBUG, Z) {
      FETCH_BBB();
#ifdef ENABLE_DEBUG_HOOK
      mrb->debug_op_hook(mrb, irep, pc, regs);
#else
#ifndef DISABLE_STDIO
      printf("OP_DEBUG %d %d %d\n", a, b, c);
#else
      abort();
#endif
#endif
      NEXT;
    }

    CASE(OP_ERR, B) {
      value msg = str_dup(mrb, pool[a]);
      value exc;

      exc = exc_new_str(mrb, E_LOCALJUMP_ERROR, msg);
      ERR_PC_SET(mrb, pc);
      exc_set(mrb, exc);
      goto L_RAISE;
    }

    CASE(OP_EXT1, Z) {
      insn = READ_B();
      switch (insn) {
#define OPCODE(insn,ops) case OP_ ## insn: FETCH_ ## ops ## _1(); goto L_OP_ ## insn ## _BODY;
#include "mruby/ops.h"
#undef OPCODE
      }
      pc--;
      NEXT;
    }
    CASE(OP_EXT2, Z) {
      insn = READ_B();
      switch (insn) {
#define OPCODE(insn,ops) case OP_ ## insn: FETCH_ ## ops ## _2(); goto L_OP_ ## insn ## _BODY;
#include "mruby/ops.h"
#undef OPCODE
      }
      pc--;
      NEXT;
    }
    CASE(OP_EXT3, Z) {
      uint8_t insn = READ_B();
      switch (insn) {
#define OPCODE(insn,ops) case OP_ ## insn: FETCH_ ## ops ## _3(); goto L_OP_ ## insn ## _BODY;
#include "mruby/ops.h"
#undef OPCODE
      }
      pc--;
      NEXT;
    }

    CASE(OP_STOP, Z) {
      /*        stop VM */
    L_STOP:
      while (mrb->c->eidx > 0) {
        ecall(mrb);
      }
      mrb->c->cibase->ridx = 0;
      ERR_PC_CLR(mrb);
      mrb->jmp = prev_jmp;
      if (mrb->exc) {
        return obj_value(mrb->exc);
      }
      return regs[irep->nlocals];
    }
  }
  END_DISPATCH;
#undef regs
  }
  CATCH(&c_jmp) {
    exc_catched = TRUE;
    goto RETRY_TRY_BLOCK;
  }
  END_EXC(&c_jmp);
}

API value
run(state *mrb, struct RProc *proc, value self)
{
  if (mrb->c->ci->argc < 0) {
    return vm_run(mrb, proc, self, 3); /* receiver, args and block) */
  }
  else {
    return vm_run(mrb, proc, self, mrb->c->ci->argc + 2); /* argc + 2 (receiver and block) */
  }
}

API value
top_run(state *mrb, struct RProc *proc, value self, unsigned int stack_keep)
{
  callinfo *ci;
  value v;

  if (!mrb->c->cibase) {
    return vm_run(mrb, proc, self, stack_keep);
  }
  if (mrb->c->ci == mrb->c->cibase) {
    return vm_run(mrb, proc, self, stack_keep);
  }
  ci = cipush(mrb);
  ci->mid = 0;
  ci->acc = CI_ACC_SKIP;
  ci->target_class = mrb->object_class;
  v = vm_run(mrb, proc, self, stack_keep);
  cipop(mrb);

  return v;
}

#if defined(ENABLE_CXX_EXCEPTION) && defined(__cplusplus)
# if !defined(ENABLE_CXX_ABI)
} /* end of extern "C" */
# endif
int jmpbuf::jmpbuf_id = 0;
# if !defined(ENABLE_CXX_ABI)
extern "C" {
# endif
#endif
