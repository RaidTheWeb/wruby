/*
** mruby/proc.h - Proc class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_PROC_H
#define MRUBY_PROC_H

#include "common.h"
#include <mruby/irep.h>

/**
 * Proc class
 */
MRB_BEGIN_DECL

struct REnv {
  MRB_OBJECT_HEADER;
  _value *stack;
  struct _context *cxt;
  _sym mid;
};

/* flags (21bits): 1(shared flag):10(cioff/bidx):10(stack_len) */
#define MRB_ENV_SET_STACK_LEN(e,len) (e)->flags = (((e)->flags & ~0x3ff)|((unsigned int)(len) & 0x3ff))
#define MRB_ENV_STACK_LEN(e) ((_int)((e)->flags & 0x3ff))
#define MRB_ENV_STACK_UNSHARED (1<<20)
#define MRB_ENV_UNSHARE_STACK(e) (e)->flags |= MRB_ENV_STACK_UNSHARED
#define MRB_ENV_STACK_SHARED_P(e) (((e)->flags & MRB_ENV_STACK_UNSHARED) == 0)
#define MRB_ENV_BIDX(e) (((e)->flags >> 10) & 0x3ff)
#define MRB_ENV_SET_BIDX(e,idx) (e)->flags = (((e)->flags & ~(0x3ff<<10))|((unsigned int)(idx) & 0x3ff)<<10)

void _env_unshare(_state*, struct REnv*);

struct RProc {
  MRB_OBJECT_HEADER;
  union {
    _irep *irep;
    _func_t func;
  } body;
  struct RProc *upper;
  union {
    struct RClass *target_class;
    struct REnv *env;
  } e;
};

/* aspec access */
#define MRB_ASPEC_REQ(a)          (((a) >> 18) & 0x1f)
#define MRB_ASPEC_OPT(a)          (((a) >> 13) & 0x1f)
#define MRB_ASPEC_REST(a)         (((a) >> 12) & 0x1)
#define MRB_ASPEC_POST(a)         (((a) >> 7) & 0x1f)
#define MRB_ASPEC_KEY(a)          (((a) >> 2) & 0x1f)
#define MRB_ASPEC_KDICT(a)        ((a) & (1<<1))
#define MRB_ASPEC_BLOCK(a)        ((a) & 1)

#define MRB_PROC_CFUNC_FL 128
#define MRB_PROC_CFUNC_P(p) (((p)->flags & MRB_PROC_CFUNC_FL) != 0)
#define MRB_PROC_CFUNC(p) (p)->body.func
#define MRB_PROC_STRICT 256
#define MRB_PROC_STRICT_P(p) (((p)->flags & MRB_PROC_STRICT) != 0)
#define MRB_PROC_ORPHAN 512
#define MRB_PROC_ORPHAN_P(p) (((p)->flags & MRB_PROC_ORPHAN) != 0)
#define MRB_PROC_ENVSET 1024
#define MRB_PROC_ENV_P(p) (((p)->flags & MRB_PROC_ENVSET) != 0)
#define MRB_PROC_ENV(p) (MRB_PROC_ENV_P(p) ? (p)->e.env : NULL)
#define MRB_PROC_TARGET_CLASS(p) (MRB_PROC_ENV_P(p) ? (p)->e.env->c : (p)->e.target_class)
#define MRB_PROC_SET_TARGET_CLASS(p,tc) do {\
  if (MRB_PROC_ENV_P(p)) {\
    (p)->e.env->c = (tc);\
    _field_write_barrier(mrb, (struct RBasic*)(p)->e.env, (struct RBasic*)tc);\
  }\
  else {\
    (p)->e.target_class = (tc);\
    _field_write_barrier(mrb, (struct RBasic*)p, (struct RBasic*)tc);\
  }\
} while (0)
#define MRB_PROC_SCOPE 2048
#define MRB_PROC_SCOPE_P(p) (((p)->flags & MRB_PROC_SCOPE) != 0)

#define _proc_ptr(v)    ((struct RProc*)(_ptr(v)))

struct RProc *_proc_new(_state*, _irep*);
struct RProc *_closure_new(_state*, _irep*);
MRB_API struct RProc *_proc_new_cfunc(_state*, _func_t);
MRB_API struct RProc *_closure_new_cfunc(_state *mrb, _func_t func, int nlocals);
void _proc_copy(struct RProc *a, struct RProc *b);

/* implementation of #send method */
_value _f_send(_state *mrb, _value self);

/* following functions are defined in mruby-proc-ext so please include it when using */
MRB_API struct RProc *_proc_new_cfunc_with_env(_state*, _func_t, _int, const _value*);
MRB_API _value _proc_cfunc_env_get(_state*, _int);
/* old name */
#define _cfunc_env_get(mrb, idx) _proc_cfunc_env_get(mrb, idx)

#ifdef MRB_METHOD_TABLE_INLINE

#define MRB_METHOD_FUNC_FL ((uintptr_t)1)
#define MRB_METHOD_FUNC_P(m) (((uintptr_t)(m))&MRB_METHOD_FUNC_FL)
#define MRB_METHOD_FUNC(m) ((_func_t)((uintptr_t)(m)&(~MRB_METHOD_FUNC_FL)))
#define MRB_METHOD_FROM_FUNC(m,fn) m=(_method_t)((struct RProc*)((uintptr_t)(fn)|MRB_METHOD_FUNC_FL))
#define MRB_METHOD_FROM_PROC(m,pr) m=(_method_t)(struct RProc*)(pr)
#define MRB_METHOD_PROC_P(m) (!MRB_METHOD_FUNC_P(m))
#define MRB_METHOD_PROC(m) ((struct RProc*)(m))
#define MRB_METHOD_UNDEF_P(m) ((m)==0)

#else

#define MRB_METHOD_FUNC_P(m) ((m).func_p)
#define MRB_METHOD_FUNC(m) ((m).func)
#define MRB_METHOD_FROM_FUNC(m,fn) do{(m).func_p=TRUE;(m).func=(fn);}while(0)
#define MRB_METHOD_FROM_PROC(m,pr) do{(m).func_p=FALSE;(m).proc=(pr);}while(0)
#define MRB_METHOD_PROC_P(m) (!MRB_METHOD_FUNC_P(m))
#define MRB_METHOD_PROC(m) ((m).proc)
#define MRB_METHOD_UNDEF_P(m) ((m).proc==NULL)

#endif /* MRB_METHOD_TABLE_INLINE */

#define MRB_METHOD_CFUNC_P(m) (MRB_METHOD_FUNC_P(m)?TRUE:(MRB_METHOD_PROC(m)?(MRB_PROC_CFUNC_P(MRB_METHOD_PROC(m))):FALSE))
#define MRB_METHOD_CFUNC(m) (MRB_METHOD_FUNC_P(m)?MRB_METHOD_FUNC(m):((MRB_METHOD_PROC(m)&&MRB_PROC_CFUNC_P(MRB_METHOD_PROC(m)))?MRB_PROC_CFUNC(MRB_METHOD_PROC(m)):NULL))


#include <mruby/khash.h>
KHASH_DECLARE(mt, _sym, _method_t, TRUE)

MRB_END_DECL

#endif  /* MRUBY_PROC_H */
