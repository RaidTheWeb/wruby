/*
** state.c - state open/close functions
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

void _init_core(state*);
void _init_mrbgems(state*);

void _gc_init(state*, _gc *gc);
void _gc_destroy(state*, _gc *gc);

MRB_API state*
_open_core(_allocf f, void *ud)
{
  static const state _state_zero = { 0 };
  static const struct _context _context_zero = { 0 };
  state *mrb;

  mrb = (state *)(f)(NULL, NULL, sizeof(state), ud);
  if (mrb == NULL) return NULL;

  *mrb = _state_zero;
  mrb->allocf_ud = ud;
  mrb->allocf = f;
  mrb->atexit_stack_len = 0;

  _gc_init(mrb, &mrb->gc);
  mrb->c = (struct _context*)_malloc(mrb, sizeof(struct _context));
  *mrb->c = _context_zero;
  mrb->root_c = mrb->c;

  _init_core(mrb);

  return mrb;
}

void*
_default_allocf(state *mrb, void *p, size_t size, void *ud)
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

MRB_API void*
_alloca(state *mrb, size_t size)
{
  struct alloca_header *p;

  p = (struct alloca_header*) _malloc(mrb, sizeof(struct alloca_header)+size);
  p->next = mrb->mems;
  mrb->mems = p;
  return (void*)p->buf;
}

static void
_alloca_free(state *mrb)
{
  struct alloca_header *p;
  struct alloca_header *tmp;

  if (mrb == NULL) return;
  p = mrb->mems;

  while (p) {
    tmp = p;
    p = p->next;
    _free(mrb, tmp);
  }
}

MRB_API state*
_open(void)
{
  state *mrb = _open_allocf(_default_allocf, NULL);

  return mrb;
}

MRB_API state*
_open_allocf(_allocf f, void *ud)
{
  state *mrb = _open_core(f, ud);

  if (mrb == NULL) {
    return NULL;
  }

#ifndef DISABLE_GEMS
//  _init_mrbgems(mrb);
  _gc_arena_restore(mrb, 0);
#endif
  return mrb;
}

void _free_symtbl(state *mrb);

void
_irep_incref(state *mrb, _irep *irep)
{
  irep->refcnt++;
}

void
_irep_decref(state *mrb, _irep *irep)
{
  irep->refcnt--;
  if (irep->refcnt == 0) {
    _irep_free(mrb, irep);
  }
}

void
_irep_cutref(state *mrb, _irep *irep)
{
  _irep *tmp;
  int i;

  for (i=0; i<irep->rlen; i++) {
    tmp = irep->reps[i];
    irep->reps[i] = NULL;
    if (tmp) _irep_decref(mrb, tmp);
  }
}

void
_irep_free(state *mrb, _irep *irep)
{
  int i;

  if (!(irep->flags & MRB_ISEQ_NO_FREE))
    _free(mrb, irep->iseq);
  if (irep->pool) for (i=0; i<irep->plen; i++) {
    if (_type(irep->pool[i]) == MRB_TT_STRING) {
      _gc_free_str(mrb, RSTRING(irep->pool[i]));
      _free(mrb, _obj_ptr(irep->pool[i]));
    }
#if defined(MRB_WORD_BOXING) && !defined(MRB_WITHOUT_FLOAT)
    else if (_type(irep->pool[i]) == MRB_TT_FLOAT) {
      _free(mrb, _obj_ptr(irep->pool[i]));
    }
#endif
  }
  _free(mrb, irep->pool);
  _free(mrb, irep->syms);
  for (i=0; i<irep->rlen; i++) {
    if (irep->reps[i])
      _irep_decref(mrb, irep->reps[i]);
  }
  _free(mrb, irep->reps);
  _free(mrb, irep->lv);
  if (irep->own_filename) {
    _free(mrb, (void *)irep->filename);
  }
  _free(mrb, irep->lines);
  _debug_info_free(mrb, irep->debug_info);
  _free(mrb, irep);
}

value
_str_pool(state *mrb, value str)
{
  struct RString *s = _str_ptr(str);
  struct RString *ns;
  char *ptr;
  _int len;

  ns = (struct RString *)_malloc(mrb, sizeof(struct RString));
  ns->tt = MRB_TT_STRING;
  ns->c = mrb->string_class;

  if (RSTR_NOFREE_P(s)) {
    ns->flags = MRB_STR_NOFREE;
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
      ns->as.heap.ptr = (char *)_malloc(mrb, (size_t)len+1);
      ns->as.heap.len = len;
      ns->as.heap.aux.capa = len;
      if (ptr) {
        memcpy(ns->as.heap.ptr, ptr, len);
      }
      ns->as.heap.ptr[len] = '\0';
    }
  }
  RSTR_SET_POOL_FLAG(ns);
  MRB_SET_FROZEN_FLAG(ns);
  return _obj_value(ns);
}

void _free_backtrace(state *mrb);

MRB_API void
_free_context(state *mrb, struct _context *c)
{
  if (!c) return;
  _free(mrb, c->stbase);
  _free(mrb, c->cibase);
  _free(mrb, c->rescue);
  _free(mrb, c->ensure);
  _free(mrb, c);
}

MRB_API void
_close(state *mrb)
{
  if (!mrb) return;
  if (mrb->atexit_stack_len > 0) {
    _int i;
    for (i = mrb->atexit_stack_len; i > 0; --i) {
      mrb->atexit_stack[i - 1](mrb);
    }
#ifndef MRB_FIXED_STATE_ATEXIT_STACK
    _free(mrb, mrb->atexit_stack);
#endif
  }

  /* free */
  _gc_free_gv(mrb);
  _free_context(mrb, mrb->root_c);
  _free_symtbl(mrb);
  _alloca_free(mrb);
  _gc_destroy(mrb, &mrb->gc);
  _free(mrb, mrb);
}

MRB_API _irep*
_add_irep(state *mrb)
{
  static const _irep _irep_zero = { 0 };
  _irep *irep;

  irep = (_irep *)_malloc(mrb, sizeof(_irep));
  *irep = _irep_zero;
  irep->refcnt = 1;
  irep->own_filename = FALSE;

  return irep;
}

MRB_API value
_top_self(state *mrb)
{
  return _obj_value(mrb->top_self);
}

MRB_API void
_state_atexit(state *mrb, _atexit_func f)
{
#ifdef MRB_FIXED_STATE_ATEXIT_STACK
  if (mrb->atexit_stack_len + 1 > MRB_FIXED_STATE_ATEXIT_STACK_SIZE) {
    _raise(mrb, E_RUNTIME_ERROR, "exceeded fixed state atexit stack limit");
  }
#else
  size_t stack_size;

  stack_size = sizeof(_atexit_func) * (mrb->atexit_stack_len + 1);
  if (mrb->atexit_stack_len == 0) {
    mrb->atexit_stack = (_atexit_func*)_malloc(mrb, stack_size);
  }
  else {
    mrb->atexit_stack = (_atexit_func*)_realloc(mrb, mrb->atexit_stack, stack_size);
  }
#endif

  mrb->atexit_stack[mrb->atexit_stack_len++] = f;
}
