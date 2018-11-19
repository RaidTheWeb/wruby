/*
** state.c - $state open/close functions
**
** See Copyright Notice in mruby.h
*/

#include <stdlib.h>
#include <string.h>
#include <mruby.h>
#include <mruby/irep.h>
#include <mruby/variable.h>
#include <mruby/debug.h>
#include <mruby/string.h>
#include <mruby/class.h>

void $init_core($state*);
void $init_mrbgems($state*);

void $gc_init($state*, $gc *gc);
void $gc_destroy($state*, $gc *gc);

$API $state*
$open_core($allocf f, void *ud)
{
  static const $state $state_zero = { 0 };
  static const struct $context $context_zero = { 0 };
  $state *mrb;

  mrb = ($state *)(f)(NULL, NULL, sizeof($state), ud);
  if (mrb == NULL) return NULL;

  *mrb = $state_zero;
  mrb->allocf_ud = ud;
  mrb->allocf = f;
  mrb->atexit_stack_len = 0;

  $gc_init(mrb, &mrb->gc);
  mrb->c = (struct $context*)$malloc(mrb, sizeof(struct $context));
  *mrb->c = $context_zero;
  mrb->root_c = mrb->c;

  $init_core(mrb);

  return mrb;
}

void*
$default_allocf($state *mrb, void *p, size_t size, void *ud)
{
  if (size == 0) {
    free(p);
    return NULL;
  }
  else {
    return realloc(p, size);
  }
}

struct alloca_header {
  struct alloca_header *next;
  char buf[1];
};

$API void*
$alloca($state *mrb, size_t size)
{
  struct alloca_header *p;

  p = (struct alloca_header*) $malloc(mrb, sizeof(struct alloca_header)+size);
  p->next = mrb->mems;
  mrb->mems = p;
  return (void*)p->buf;
}

static void
$alloca_free($state *mrb)
{
  struct alloca_header *p;
  struct alloca_header *tmp;

  if (mrb == NULL) return;
  p = mrb->mems;

  while (p) {
    tmp = p;
    p = p->next;
    $free(mrb, tmp);
  }
}

$API $state*
$open(void)
{
  $state *mrb = $open_allocf($default_allocf, NULL);

  return mrb;
}

$API $state*
$open_allocf($allocf f, void *ud)
{
  $state *mrb = $open_core(f, ud);

  if (mrb == NULL) {
    return NULL;
  }

#ifndef DISABLE_GEMS
  $init_mrbgems(mrb);
  $gc_arena_restore(mrb, 0);
#endif
  return mrb;
}

void $free_symtbl($state *mrb);

void
$irep_incref($state *mrb, $irep *irep)
{
  irep->refcnt++;
}

void
$irep_decref($state *mrb, $irep *irep)
{
  irep->refcnt--;
  if (irep->refcnt == 0) {
    $irep_free(mrb, irep);
  }
}

void
$irep_cutref($state *mrb, $irep *irep)
{
  $irep *tmp;
  int i;

  for (i=0; i<irep->rlen; i++) {
    tmp = irep->reps[i];
    irep->reps[i] = NULL;
    if (tmp) $irep_decref(mrb, tmp);
  }
}

void
$irep_free($state *mrb, $irep *irep)
{
  int i;

  if (!(irep->flags & $ISEQ_NO_FREE))
    $free(mrb, irep->iseq);
  if (irep->pool) for (i=0; i<irep->plen; i++) {
    if ($type(irep->pool[i]) == $TT_STRING) {
      $gc_free_str(mrb, RSTRING(irep->pool[i]));
      $free(mrb, $obj_ptr(irep->pool[i]));
    }
#if defined($WORD_BOXING) && !defined($WITHOUT_FLOAT)
    else if ($type(irep->pool[i]) == $TT_FLOAT) {
      $free(mrb, $obj_ptr(irep->pool[i]));
    }
#endif
  }
  $free(mrb, irep->pool);
  $free(mrb, irep->syms);
  for (i=0; i<irep->rlen; i++) {
    if (irep->reps[i])
      $irep_decref(mrb, irep->reps[i]);
  }
  $free(mrb, irep->reps);
  $free(mrb, irep->lv);
  if (irep->own_filename) {
    $free(mrb, (void *)irep->filename);
  }
  $free(mrb, irep->lines);
  $debug_info_free(mrb, irep->debug_info);
  $free(mrb, irep);
}

$value
$str_pool($state *mrb, $value str)
{
  struct RString *s = $str_ptr(str);
  struct RString *ns;
  char *ptr;
  $int len;

  ns = (struct RString *)$malloc(mrb, sizeof(struct RString));
  ns->tt = $TT_STRING;
  ns->c = mrb->string_class;

  if (RSTR_NOFREE_P(s)) {
    ns->flags = $STR_NOFREE;
    ns->as.heap.ptr = s->as.heap.ptr;
    ns->as.heap.len = s->as.heap.len;
    ns->as.heap.aux.capa = 0;
  }
  else {
    ns->flags = 0;
    if (RSTR_EMBED_P(s)) {
      ptr = s->as.ary;
      len = RSTR_EMBED_LEN(s);
    }
    else {
      ptr = s->as.heap.ptr;
      len = s->as.heap.len;
    }

    if (len < RSTRING_EMBED_LEN_MAX) {
      RSTR_SET_EMBED_FLAG(ns);
      RSTR_SET_EMBED_LEN(ns, len);
      if (ptr) {
        memcpy(ns->as.ary, ptr, len);
      }
      ns->as.ary[len] = '\0';
    }
    else {
      ns->as.heap.ptr = (char *)$malloc(mrb, (size_t)len+1);
      ns->as.heap.len = len;
      ns->as.heap.aux.capa = len;
      if (ptr) {
        memcpy(ns->as.heap.ptr, ptr, len);
      }
      ns->as.heap.ptr[len] = '\0';
    }
  }
  RSTR_SET_POOL_FLAG(ns);
  $SET_FROZEN_FLAG(ns);
  return $obj_value(ns);
}

void $free_backtrace($state *mrb);

$API void
$free_context($state *mrb, struct $context *c)
{
  if (!c) return;
  $free(mrb, c->stbase);
  $free(mrb, c->cibase);
  $free(mrb, c->rescue);
  $free(mrb, c->ensure);
  $free(mrb, c);
}

$API void
$close($state *mrb)
{
  if (!mrb) return;
  if (mrb->atexit_stack_len > 0) {
    $int i;
    for (i = mrb->atexit_stack_len; i > 0; --i) {
      mrb->atexit_stack[i - 1](mrb);
    }
#ifndef $FIXED_STATE_ATEXIT_STACK
    $free(mrb, mrb->atexit_stack);
#endif
  }

  /* free */
  $gc_free_gv(mrb);
  $free_context(mrb, mrb->root_c);
  $free_symtbl(mrb);
  $alloca_free(mrb);
  $gc_destroy(mrb, &mrb->gc);
  $free(mrb, mrb);
}

$API $irep*
$add_irep($state *mrb)
{
  static const $irep $irep_zero = { 0 };
  $irep *irep;

  irep = ($irep *)$malloc(mrb, sizeof($irep));
  *irep = $irep_zero;
  irep->refcnt = 1;
  irep->own_filename = FALSE;

  return irep;
}

$API $value
$top_self($state *mrb)
{
  return $obj_value(mrb->top_self);
}

$API void
$state_atexit($state *mrb, $atexit_func f)
{
#ifdef $FIXED_STATE_ATEXIT_STACK
  if (mrb->atexit_stack_len + 1 > $FIXED_STATE_ATEXIT_STACK_SIZE) {
    $raise(mrb, E_RUNTIME_ERROR, "exceeded fixed state atexit stack limit");
  }
#else
  size_t stack_size;

  stack_size = sizeof($atexit_func) * (mrb->atexit_stack_len + 1);
  if (mrb->atexit_stack_len == 0) {
    mrb->atexit_stack = ($atexit_func*)$malloc(mrb, stack_size);
  }
  else {
    mrb->atexit_stack = ($atexit_func*)$realloc(mrb, mrb->atexit_stack, stack_size);
  }
#endif

  mrb->atexit_stack[mrb->atexit_stack_len++] = f;
}
