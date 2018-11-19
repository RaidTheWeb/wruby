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
$BEGIN_DECL

struct REnv {
  $OBJECT_HEADER;
  $value *stack;
  struct $context *cxt;
  $sym mid;
};

/* flags (21bits): 1(shared flag):10(cioff/bidx):10(stack_len) */
#define $ENV_SET_STACK_LEN(e,len) (e)->flags = (((e)->flags & ~0x3ff)|((unsigned int)(len) & 0x3ff))
#define $ENV_STACK_LEN(e) (($int)((e)->flags & 0x3ff))
#define $ENV_STACK_UNSHARED (1<<20)
#define $ENV_UNSHARE_STACK(e) (e)->flags |= $ENV_STACK_UNSHARED
#define $ENV_STACK_SHARED_P(e) (((e)->flags & $ENV_STACK_UNSHARED) == 0)
#define $ENV_BIDX(e) (((e)->flags >> 10) & 0x3ff)
#define $ENV_SET_BIDX(e,idx) (e)->flags = (((e)->flags & ~(0x3ff<<10))|((unsigned int)(idx) & 0x3ff)<<10)

void $env_unshare($state*, struct REnv*);

struct RProc {
  $OBJECT_HEADER;
  union {
    $irep *irep;
    $func_t func;
  } body;
  struct RProc *upper;
  union {
    struct RClass *target_class;
    struct REnv *env;
  } e;
};

/* aspec access */
#define $ASPEC_REQ(a)          (((a) >> 18) & 0x1f)
#define $ASPEC_OPT(a)          (((a) >> 13) & 0x1f)
#define $ASPEC_REST(a)         (((a) >> 12) & 0x1)
#define $ASPEC_POST(a)         (((a) >> 7) & 0x1f)
#define $ASPEC_KEY(a)          (((a) >> 2) & 0x1f)
#define $ASPEC_KDICT(a)        ((a) & (1<<1))
#define $ASPEC_BLOCK(a)        ((a) & 1)

#define $PROC_CFUNC_FL 128
#define $PROC_CFUNC_P(p) (((p)->flags & $PROC_CFUNC_FL) != 0)
#define $PROC_CFUNC(p) (p)->body.func
#define $PROC_STRICT 256
#define $PROC_STRICT_P(p) (((p)->flags & $PROC_STRICT) != 0)
#define $PROC_ORPHAN 512
#define $PROC_ORPHAN_P(p) (((p)->flags & $PROC_ORPHAN) != 0)
#define $PROC_ENVSET 1024
#define $PROC_ENV_P(p) (((p)->flags & $PROC_ENVSET) != 0)
#define $PROC_ENV(p) ($PROC_ENV_P(p) ? (p)->e.env : NULL)
#define $PROC_TARGET_CLASS(p) ($PROC_ENV_P(p) ? (p)->e.env->c : (p)->e.target_class)
#define $PROC_SET_TARGET_CLASS(p,tc) do {\
  if ($PROC_ENV_P(p)) {\
    (p)->e.env->c = (tc);\
    $field_write_barrier(mrb, (struct RBasic*)(p)->e.env, (struct RBasic*)tc);\
  }\
  else {\
    (p)->e.target_class = (tc);\
    $field_write_barrier(mrb, (struct RBasic*)p, (struct RBasic*)tc);\
  }\
} while (0)
#define $PROC_SCOPE 2048
#define $PROC_SCOPE_P(p) (((p)->flags & $PROC_SCOPE) != 0)

#define $proc_ptr(v)    ((struct RProc*)($ptr(v)))

struct RProc *$proc_new($state*, $irep*);
struct RProc *$closure_new($state*, $irep*);
$API struct RProc *$proc_new_cfunc($state*, $func_t);
$API struct RProc *$closure_new_cfunc($state *mrb, $func_t func, int nlocals);
void $proc_copy(struct RProc *a, struct RProc *b);

/* implementation of #send method */
$value $f_send($state *mrb, $value self);

/* following functions are defined in mruby-proc-ext so please include it when using */
$API struct RProc *$proc_new_cfunc_with_env($state*, $func_t, $int, const $value*);
$API $value $proc_cfunc_env_get($state*, $int);
/* old name */
#define $cfunc_env_get(mrb, idx) $proc_cfunc_env_get(mrb, idx)

#ifdef $METHOD_TABLE_INLINE

#define $METHOD_FUNC_FL ((uintptr_t)1)
#define $METHOD_FUNC_P(m) (((uintptr_t)(m))&$METHOD_FUNC_FL)
#define $METHOD_FUNC(m) (($func_t)((uintptr_t)(m)&(~$METHOD_FUNC_FL)))
#define $METHOD_FROM_FUNC(m,fn) m=($method_t)((struct RProc*)((uintptr_t)(fn)|$METHOD_FUNC_FL))
#define $METHOD_FROM_PROC(m,pr) m=($method_t)(struct RProc*)(pr)
#define $METHOD_PROC_P(m) (!$METHOD_FUNC_P(m))
#define $METHOD_PROC(m) ((struct RProc*)(m))
#define $METHOD_UNDEF_P(m) ((m)==0)

#else

#define $METHOD_FUNC_P(m) ((m).func_p)
#define $METHOD_FUNC(m) ((m).func)
#define $METHOD_FROM_FUNC(m,fn) do{(m).func_p=TRUE;(m).func=(fn);}while(0)
#define $METHOD_FROM_PROC(m,pr) do{(m).func_p=FALSE;(m).proc=(pr);}while(0)
#define $METHOD_PROC_P(m) (!$METHOD_FUNC_P(m))
#define $METHOD_PROC(m) ((m).proc)
#define $METHOD_UNDEF_P(m) ((m).proc==NULL)

#endif /* $METHOD_TABLE_INLINE */

#define $METHOD_CFUNC_P(m) ($METHOD_FUNC_P(m)?TRUE:($METHOD_PROC(m)?($PROC_CFUNC_P($METHOD_PROC(m))):FALSE))
#define $METHOD_CFUNC(m) ($METHOD_FUNC_P(m)?$METHOD_FUNC(m):(($METHOD_PROC(m)&&$PROC_CFUNC_P($METHOD_PROC(m)))?$PROC_CFUNC($METHOD_PROC(m)):NULL))


#include <mruby/khash.h>
KHASH_DECLARE(mt, $sym, $method_t, TRUE)

$END_DECL

#endif  /* MRUBY_PROC_H */
