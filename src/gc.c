/*
** gc.c - garbage collector for mruby
**
** See Copyright Notice in mruby.h
*/

#include <string.h>
#include <stdlib.h>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/hash.h>
#include <mruby/proc.h>
#include <mruby/range.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <mruby/gc.h>
#include <mruby/error.h>
#include <mruby/throw.h>

/*
  = Tri-color Incremental Garbage Collection

  mruby's GC is Tri-color Incremental GC with Mark & Sweep.
  Algorithm details are omitted.
  Instead, the implementation part is described below.

  == Object's Color

  Each object can be painted in three colors:

    * White - Unmarked.
    * Gray - Marked, But the child objects are unmarked.
    * Black - Marked, the child objects are also marked.

  == Two White Types

  There're two white color types in a flip-flop fashion: White-A and White-B,
  which respectively represent the Current White color (the newly allocated
  objects in the current GC cycle) and the Sweep Target White color (the
  dead objects to be swept).

  A and B will be switched just at the beginning of the next GC cycle. At
  that time, all the dead objects have been swept, while the newly created
  objects in the current GC cycle which finally remains White are now
  regarded as dead objects. Instead of traversing all the White-A objects and
  painting them as White-B, just switch the meaning of White-A and White-B as
  this will be much cheaper.

  As a result, the objects we sweep in the current GC cycle are always
  left from the previous GC cycle. This allows us to sweep objects
  incrementally, without the disturbance of the newly created objects.

  == Execution Timing

  GC Execution Time and Each step interval are decided by live objects count.
  List of Adjustment API:

    * gc_interval_ratio_set
    * gc_step_ratio_set

  For details, see the comments for each function.

  == Write Barrier

  mruby implementer and C extension library writer must insert a write
  barrier when updating a reference from a field of an object.
  When updating a reference from a field of object A to object B,
  two different types of write barrier are available:

    * _field_write_barrier - target B object for a mark.
    * _write_barrier       - target A object for a mark.

  == Generational Mode

  mruby's GC offers an Generational Mode while re-using the tri-color GC
  infrastructure. It will treat the Black objects as Old objects after each
  sweep phase, instead of painting them White. The key ideas are still the same
  as traditional generational GC:

    * Minor GC - just traverse the Young objects (Gray objects) in the mark
                 phase, then only sweep the newly created objects, and leave
                 the Old objects live.

    * Major GC - same as a full regular GC cycle.

  The difference from "traditional" generational GC is, that the major GC
  in mruby is triggered incrementally in a tri-color manner.


  For details, see the comments for each function.

*/

struct free_obj {
  MRB_OBJECT_HEADER;
  struct RBasic *next;
};

typedef struct {
  union {
    struct free_obj free;
    struct RBasic basic;
    struct RObject object;
    struct RClass klass;
    struct RString string;
    struct RArray array;
    struct RHash hash;
    struct RRange range;
    struct RData data;
    struct RProc proc;
    struct REnv env;
    struct RException exc;
    struct RBreak brk;
#ifdef MRB_WORD_BOXING
#ifndef MRB_WITHOUT_FLOAT
    struct RFloat floatv;
#endif
    struct RCptr cptr;
#endif
  } as;
} RVALUE;

#ifdef GC_PROFILE
#include <stdio.h>
#include <sys/time.h>

static double program_invoke_time = 0;
static double gc_time = 0;
static double gc_total_time = 0;

static double
gettimeofday_time(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec * 1e-6;
}

#define GC_INVOKE_TIME_REPORT(with) do {\
  fprintf(stderr, "%s\n", with);\
  fprintf(stderr, "gc_invoke: %19.3f\n", gettimeofday_time() - program_invoke_time);\
  fprintf(stderr, "is_generational: %d\n", is_generational(gc));\
  fprintf(stderr, "is_major_gc: %d\n", is_major_gc(gc));\
} while(0)

#define GC_TIME_START do {\
  gc_time = gettimeofday_time();\
} while(0)

#define GC_TIME_STOP_AND_REPORT do {\
  gc_time = gettimeofday_time() - gc_time;\
  gc_total_time += gc_time;\
  fprintf(stderr, "gc_state: %d\n", gc->state);\
  fprintf(stderr, "live: %zu\n", gc->live);\
  fprintf(stderr, "majorgc_old_threshold: %zu\n", gc->majorgc_old_threshold);\
  fprintf(stderr, "gc_threshold: %zu\n", gc->threshold);\
  fprintf(stderr, "gc_time: %30.20f\n", gc_time);\
  fprintf(stderr, "gc_total_time: %30.20f\n\n", gc_total_time);\
} while(0)
#else
#define GC_INVOKE_TIME_REPORT(s)
#define GC_TIME_START
#define GC_TIME_STOP_AND_REPORT
#endif

#ifdef GC_DEBUG
#define DEBUG(x) (x)
#else
#define DEBUG(x)
#endif

#ifndef MRB_HEAP_PAGE_SIZE
#define MRB_HEAP_PAGE_SIZE 1024
#endif

#define GC_STEP_SIZE 1024

/* white: 001 or 010, black: 100, gray: 000 */
#define GC_GRAY 0
#define GC_WHITE_A 1
#define GC_WHITE_B (1 << 1)
#define GC_BLACK (1 << 2)
#define GC_WHITES (GC_WHITE_A | GC_WHITE_B)
#define GC_COLOR_MASK 7

#define paint_gray(o) ((o)->color = GC_GRAY)
#define paint_black(o) ((o)->color = GC_BLACK)
#define paint_white(o) ((o)->color = GC_WHITES)
#define paint_partial_white(s, o) ((o)->color = (s)->current_white_part)
#define is_gray(o) ((o)->color == GC_GRAY)
#define is_white(o) ((o)->color & GC_WHITES)
#define is_black(o) ((o)->color & GC_BLACK)
#define flip_white_part(s) ((s)->current_white_part = other_white_part(s))
#define other_white_part(s) ((s)->current_white_part ^ GC_WHITES)
#define is_dead(s, o) (((o)->color & other_white_part(s) & GC_WHITES) || (o)->tt == MRB_TT_FREE)

#define objects(p) ((RVALUE *)p->objects)

MRB_API void*
_realloc_simple(_state *mrb, void *p,  size_t len)
{
  void *p2;

  p2 = (mrb->allocf)(mrb, p, len, mrb->allocf_ud);
  if (!p2 && len > 0 && mrb->gc.heaps) {
    _full_gc(mrb);
    p2 = (mrb->allocf)(mrb, p, len, mrb->allocf_ud);
  }

  return p2;
}

MRB_API void*
_realloc(_state *mrb, void *p, size_t len)
{
  void *p2;

  p2 = _realloc_simple(mrb, p, len);
  if (len == 0) return p2;
  if (p2 == NULL) {
    if (mrb->gc.out_of_memory) {
      _exc_raise(mrb, _obj_value(mrb->nomem_err));
      /* _panic(mrb); */
    }
    else {
      mrb->gc.out_of_memory = TRUE;
      _exc_raise(mrb, _obj_value(mrb->nomem_err));
    }
  }
  else {
    mrb->gc.out_of_memory = FALSE;
  }

  return p2;
}

MRB_API void*
_malloc(_state *mrb, size_t len)
{
  return _realloc(mrb, 0, len);
}

MRB_API void*
_malloc_simple(_state *mrb, size_t len)
{
  return _realloc_simple(mrb, 0, len);
}

MRB_API void*
_calloc(_state *mrb, size_t nelem, size_t len)
{
  void *p;

  if (nelem > 0 && len > 0 &&
      nelem <= SIZE_MAX / len) {
    size_t size;
    size = nelem * len;
    p = _malloc(mrb, size);

    memset(p, 0, size);
  }
  else {
    p = NULL;
  }

  return p;
}

MRB_API void
_free(_state *mrb, void *p)
{
  (mrb->allocf)(mrb, p, 0, mrb->allocf_ud);
}

static _bool
heap_p(_gc *gc, struct RBasic *object)
{
  _heap_page* page;

  page = gc->heaps;
  while (page) {
    RVALUE *p;

    p = objects(page);
    if (&p[0].as.basic <= object && object <= &p[MRB_HEAP_PAGE_SIZE].as.basic) {
      return TRUE;
    }
    page = page->next;
  }
  return FALSE;
}

MRB_API _bool
_object_dead_p(_state *mrb, struct RBasic *object) {
  _gc *gc = &mrb->gc;
  if (!heap_p(gc, object)) return TRUE;
  return is_dead(gc, object);
}

static void
link_heap_page(_gc *gc, _heap_page *page)
{
  page->next = gc->heaps;
  if (gc->heaps)
    gc->heaps->prev = page;
  gc->heaps = page;
}

static void
unlink_heap_page(_gc *gc, _heap_page *page)
{
  if (page->prev)
    page->prev->next = page->next;
  if (page->next)
    page->next->prev = page->prev;
  if (gc->heaps == page)
    gc->heaps = page->next;
  page->prev = NULL;
  page->next = NULL;
}

static void
link_free_heap_page(_gc *gc, _heap_page *page)
{
  page->free_next = gc->free_heaps;
  if (gc->free_heaps) {
    gc->free_heaps->free_prev = page;
  }
  gc->free_heaps = page;
}

static void
unlink_free_heap_page(_gc *gc, _heap_page *page)
{
  if (page->free_prev)
    page->free_prev->free_next = page->free_next;
  if (page->free_next)
    page->free_next->free_prev = page->free_prev;
  if (gc->free_heaps == page)
    gc->free_heaps = page->free_next;
  page->free_prev = NULL;
  page->free_next = NULL;
}

static void
add_heap(_state *mrb, _gc *gc)
{
  _heap_page *page = (_heap_page *)_calloc(mrb, 1, sizeof(_heap_page) + MRB_HEAP_PAGE_SIZE * sizeof(RVALUE));
  RVALUE *p, *e;
  struct RBasic *prev = NULL;

  for (p = objects(page), e=p+MRB_HEAP_PAGE_SIZE; p<e; p++) {
    p->as.free.tt = MRB_TT_FREE;
    p->as.free.next = prev;
    prev = &p->as.basic;
  }
  page->freelist = prev;

  link_heap_page(gc, page);
  link_free_heap_page(gc, page);
}

#define DEFAULT_GC_INTERVAL_RATIO 200
#define DEFAULT_GC_STEP_RATIO 200
#define MAJOR_GC_INC_RATIO 120
#define MAJOR_GC_TOOMANY 10000
#define is_generational(gc) ((gc)->generational)
#define is_major_gc(gc) (is_generational(gc) && (gc)->full)
#define is_minor_gc(gc) (is_generational(gc) && !(gc)->full)

void
_gc_init(_state *mrb, _gc *gc)
{
#ifndef MRB_GC_FIXED_ARENA
  gc->arena = (struct RBasic**)_malloc(mrb, sizeof(struct RBasic*)*MRB_GC_ARENA_SIZE);
  gc->arena_capa = MRB_GC_ARENA_SIZE;
#endif

  gc->current_white_part = GC_WHITE_A;
  gc->heaps = NULL;
  gc->free_heaps = NULL;
  add_heap(mrb, gc);
  gc->interval_ratio = DEFAULT_GC_INTERVAL_RATIO;
  gc->step_ratio = DEFAULT_GC_STEP_RATIO;
#ifndef MRB_GC_TURN_OFF_GENERATIONAL
  gc->generational = TRUE;
  gc->full = TRUE;
#endif

#ifdef GC_PROFILE
  program_invoke_time = gettimeofday_time();
#endif
}

static void obj_free(_state *mrb, struct RBasic *obj, int end);

void
free_heap(_state *mrb, _gc *gc)
{
  _heap_page *page = gc->heaps;
  _heap_page *tmp;
  RVALUE *p, *e;

  while (page) {
    tmp = page;
    page = page->next;
    for (p = objects(tmp), e=p+MRB_HEAP_PAGE_SIZE; p<e; p++) {
      if (p->as.free.tt != MRB_TT_FREE)
        obj_free(mrb, &p->as.basic, TRUE);
    }
    _free(mrb, tmp);
  }
}

void
_gc_destroy(_state *mrb, _gc *gc)
{
  free_heap(mrb, gc);
#ifndef MRB_GC_FIXED_ARENA
  _free(mrb, gc->arena);
#endif
}

static void
gc_protect(_state *mrb, _gc *gc, struct RBasic *p)
{
#ifdef MRB_GC_FIXED_ARENA
  if (gc->arena_idx >= MRB_GC_ARENA_SIZE) {
    /* arena overflow error */
    gc->arena_idx = MRB_GC_ARENA_SIZE - 4; /* force room in arena */
    _exc_raise(mrb, _obj_value(mrb->arena_err));
  }
#else
  if (gc->arena_idx >= gc->arena_capa) {
    /* extend arena */
    gc->arena_capa = (int)(gc->arena_capa * 3 / 2);
    gc->arena = (struct RBasic**)_realloc(mrb, gc->arena, sizeof(struct RBasic*)*gc->arena_capa);
  }
#endif
  gc->arena[gc->arena_idx++] = p;
}

/* _gc_protect() leaves the object in the arena */
MRB_API void
_gc_protect(_state *mrb, _value obj)
{
  if (_immediate_p(obj)) return;
  gc_protect(mrb, &mrb->gc, _basic_ptr(obj));
}

#define GC_ROOT_NAME "_gc_root_"

/* _gc_register() keeps the object from GC.

   Register your object when it's exported to C world,
   without reference from Ruby world, e.g. callback
   arguments.  Don't forget to remove the object using
   _gc_unregister, otherwise your object will leak.
*/

MRB_API void
_gc_register(_state *mrb, _value obj)
{
  _sym root = _intern_lit(mrb, GC_ROOT_NAME);
  _value table = _gv_get(mrb, root);

  if (_nil_p(table) || _type(table) != MRB_TT_ARRAY) {
    table = _ary_new(mrb);
    _gv_set(mrb, root, table);
  }
  _ary_push(mrb, table, obj);
}

/* _gc_unregister() removes the object from GC root. */
MRB_API void
_gc_unregister(_state *mrb, _value obj)
{
  _sym root = _intern_lit(mrb, GC_ROOT_NAME);
  _value table = _gv_get(mrb, root);
  struct RArray *a;
  _int i;

  if (_nil_p(table)) return;
  if (_type(table) != MRB_TT_ARRAY) {
    _gv_set(mrb, root, _nil_value());
    return;
  }
  a = _ary_ptr(table);
  _ary_modify(mrb, a);
  for (i = 0; i < ARY_LEN(a); i++) {
    if (_obj_eq(mrb, ARY_PTR(a)[i], obj)) {
      _int len = ARY_LEN(a)-1;
      _value *ptr = ARY_PTR(a);

      ARY_SET_LEN(a, len);
      memmove(&ptr[i], &ptr[i + 1], (len - i) * sizeof(_value));
      break;
    }
  }
}

MRB_API struct RBasic*
_obj_alloc(_state *mrb, enum _vtype ttype, struct RClass *cls)
{
  struct RBasic *p;
  static const RVALUE RVALUE_zero = { { { MRB_TT_FALSE } } };
  _gc *gc = &mrb->gc;

  if (cls) {
    enum _vtype tt;

    switch (cls->tt) {
    case MRB_TT_CLASS:
    case MRB_TT_SCLASS:
    case MRB_TT_MODULE:
    case MRB_TT_ENV:
      break;
    default:
      _raise(mrb, E_TYPE_ERROR, "allocation failure");
    }
    tt = MRB_INSTANCE_TT(cls);
    if (tt != MRB_TT_FALSE &&
        ttype != MRB_TT_SCLASS &&
        ttype != MRB_TT_ICLASS &&
        ttype != MRB_TT_ENV &&
        ttype != tt) {
      _raisef(mrb, E_TYPE_ERROR, "allocation failure of %S", _obj_value(cls));
    }
  }

#ifdef MRB_GC_STRESS
  _full_gc(mrb);
#endif
  if (gc->threshold < gc->live) {
    _incremental_gc(mrb);
  }
  if (gc->free_heaps == NULL) {
    add_heap(mrb, gc);
  }

  p = gc->free_heaps->freelist;
  gc->free_heaps->freelist = ((struct free_obj*)p)->next;
  if (gc->free_heaps->freelist == NULL) {
    unlink_free_heap_page(gc, gc->free_heaps);
  }

  gc->live++;
  gc_protect(mrb, gc, p);
  *(RVALUE *)p = RVALUE_zero;
  p->tt = ttype;
  p->c = cls;
  paint_partial_white(gc, p);
  return p;
}

static inline void
add_gray_list(_state *mrb, _gc *gc, struct RBasic *obj)
{
#ifdef MRB_GC_STRESS
  if (obj->tt > MRB_TT_MAXDEFINE) {
    abort();
  }
#endif
  paint_gray(obj);
  obj->gcnext = gc->gray_list;
  gc->gray_list = obj;
}

static int
ci_nregs(_callinfo *ci)
{
  struct RProc *p = ci->proc;
  int n = 0;

  if (!p) {
    if (ci->argc < 0) return 3;
    return ci->argc+2;
  }
  if (!MRB_PROC_CFUNC_P(p) && p->body.irep) {
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

static void
mark_context_stack(_state *mrb, struct _context *c)
{
  size_t i;
  size_t e;
  _value nil;

  if (c->stack == NULL) return;
  e = c->stack - c->stbase;
  if (c->ci) {
    e += ci_nregs(c->ci);
  }
  if (c->stbase + e > c->stend) e = c->stend - c->stbase;
  for (i=0; i<e; i++) {
    _value v = c->stbase[i];

    if (!_immediate_p(v)) {
      _gc_mark(mrb, _basic_ptr(v));
    }
  }
  e = c->stend - c->stbase;
  nil = _nil_value();
  for (; i<e; i++) {
    c->stbase[i] = nil;
  }
}

static void
mark_context(_state *mrb, struct _context *c)
{
  int i;
  _callinfo *ci;

 start:
  if (c->status == MRB_FIBER_TERMINATED) return;

  /* mark VM stack */
  mark_context_stack(mrb, c);

  /* mark call stack */
  if (c->cibase) {
    for (ci = c->cibase; ci <= c->ci; ci++) {
      _gc_mark(mrb, (struct RBasic*)ci->env);
      _gc_mark(mrb, (struct RBasic*)ci->proc);
      _gc_mark(mrb, (struct RBasic*)ci->target_class);
    }
  }
  /* mark ensure stack */
  for (i=0; i<c->eidx; i++) {
    _gc_mark(mrb, (struct RBasic*)c->ensure[i]);
  }
  /* mark fibers */
  _gc_mark(mrb, (struct RBasic*)c->fib);
  if (c->prev) {
    c = c->prev;
    goto start;
  }
}

static void
gc_mark_children(_state *mrb, _gc *gc, struct RBasic *obj)
{
  _assert(is_gray(obj));
  paint_black(obj);
  gc->gray_list = obj->gcnext;
  _gc_mark(mrb, (struct RBasic*)obj->c);
  switch (obj->tt) {
  case MRB_TT_ICLASS:
    {
      struct RClass *c = (struct RClass*)obj;
      if (MRB_FLAG_TEST(c, MRB_FL_CLASS_IS_ORIGIN))
        _gc_mark_mt(mrb, c);
      _gc_mark(mrb, (struct RBasic*)((struct RClass*)obj)->super);
    }
    break;

  case MRB_TT_CLASS:
  case MRB_TT_MODULE:
  case MRB_TT_SCLASS:
    {
      struct RClass *c = (struct RClass*)obj;

      _gc_mark_mt(mrb, c);
      _gc_mark(mrb, (struct RBasic*)c->super);
    }
    /* fall through */

  case MRB_TT_OBJECT:
  case MRB_TT_DATA:
  case MRB_TT_EXCEPTION:
    _gc_mark_iv(mrb, (struct RObject*)obj);
    break;

  case MRB_TT_PROC:
    {
      struct RProc *p = (struct RProc*)obj;

      _gc_mark(mrb, (struct RBasic*)p->upper);
      _gc_mark(mrb, (struct RBasic*)p->e.env);
    }
    break;

  case MRB_TT_ENV:
    {
      struct REnv *e = (struct REnv*)obj;
      _int i, len;

      if (MRB_ENV_STACK_SHARED_P(e) && e->cxt && e->cxt->fib) {
        _gc_mark(mrb, (struct RBasic*)e->cxt->fib);
      }
      len = MRB_ENV_STACK_LEN(e);
      for (i=0; i<len; i++) {
        _gc_mark_value(mrb, e->stack[i]);
      }
    }
    break;

  case MRB_TT_FIBER:
    {
      struct _context *c = ((struct RFiber*)obj)->cxt;

      if (c) mark_context(mrb, c);
    }
    break;

  case MRB_TT_ARRAY:
    {
      struct RArray *a = (struct RArray*)obj;
      size_t i, e;

      for (i=0,e=ARY_LEN(a); i<e; i++) {
        _gc_mark_value(mrb, ARY_PTR(a)[i]);
      }
    }
    break;

  case MRB_TT_HASH:
    _gc_mark_iv(mrb, (struct RObject*)obj);
    _gc_mark_hash(mrb, (struct RHash*)obj);
    break;

  case MRB_TT_STRING:
    if (RSTR_FSHARED_P(obj) && !RSTR_NOFREE_P(obj)) {
      struct RString *s = (struct RString*)obj;
      _gc_mark(mrb, (struct RBasic*)s->as.heap.aux.fshared);
    }
    break;

  case MRB_TT_RANGE:
    {
      struct RRange *r = (struct RRange*)obj;

      if (r->edges) {
        _gc_mark_value(mrb, r->edges->beg);
        _gc_mark_value(mrb, r->edges->end);
      }
    }
    break;

  default:
    break;
  }
}

MRB_API void
_gc_mark(_state *mrb, struct RBasic *obj)
{
  if (obj == 0) return;
  if (!is_white(obj)) return;
  _assert((obj)->tt != MRB_TT_FREE);
  add_gray_list(mrb, &mrb->gc, obj);
}

static void
obj_free(_state *mrb, struct RBasic *obj, int end)
{
  DEBUG(fprintf(stderr, "obj_free(%p,tt=%d)\n",obj,obj->tt));
  switch (obj->tt) {
    /* immediate - no mark */
  case MRB_TT_TRUE:
  case MRB_TT_FIXNUM:
  case MRB_TT_SYMBOL:
    /* cannot happen */
    return;

#ifndef MRB_WITHOUT_FLOAT
  case MRB_TT_FLOAT:
#ifdef MRB_WORD_BOXING
    break;
#else
    return;
#endif
#endif

  case MRB_TT_OBJECT:
    _gc_free_iv(mrb, (struct RObject*)obj);
    break;

  case MRB_TT_EXCEPTION:
    _gc_free_iv(mrb, (struct RObject*)obj);
    break;

  case MRB_TT_CLASS:
  case MRB_TT_MODULE:
  case MRB_TT_SCLASS:
    _gc_free_mt(mrb, (struct RClass*)obj);
    _gc_free_iv(mrb, (struct RObject*)obj);
    break;
  case MRB_TT_ICLASS:
    if (MRB_FLAG_TEST(obj, MRB_FL_CLASS_IS_ORIGIN))
      _gc_free_mt(mrb, (struct RClass*)obj);
    break;
  case MRB_TT_ENV:
    {
      struct REnv *e = (struct REnv*)obj;

      if (MRB_ENV_STACK_SHARED_P(e)) {
        /* cannot be freed */
        e->stack = NULL;
        break;
      }
      _free(mrb, e->stack);
      e->stack = NULL;
    }
    break;

  case MRB_TT_FIBER:
    {
      struct _context *c = ((struct RFiber*)obj)->cxt;

      if (c && c != mrb->root_c) {
        if (!end && c->status != MRB_FIBER_TERMINATED) {
          _callinfo *ci = c->ci;
          _callinfo *ce = c->cibase;

          while (ce <= ci) {
            struct REnv *e = ci->env;
            if (e && !_object_dead_p(mrb, (struct RBasic*)e) &&
                e->tt == MRB_TT_ENV && MRB_ENV_STACK_SHARED_P(e)) {
              _env_unshare(mrb, e);
            }
            ci--;
          }
        }
        _free_context(mrb, c);
      }
    }
    break;

  case MRB_TT_ARRAY:
    if (ARY_SHARED_P(obj))
      _ary_decref(mrb, ((struct RArray*)obj)->as.heap.aux.shared);
    else if (!ARY_EMBED_P(obj))
      _free(mrb, ((struct RArray*)obj)->as.heap.ptr);
    break;

  case MRB_TT_HASH:
    _gc_free_iv(mrb, (struct RObject*)obj);
    _gc_free_hash(mrb, (struct RHash*)obj);
    break;

  case MRB_TT_STRING:
    _gc_free_str(mrb, (struct RString*)obj);
    break;

  case MRB_TT_PROC:
    {
      struct RProc *p = (struct RProc*)obj;

      if (!MRB_PROC_CFUNC_P(p) && p->body.irep) {
        _irep *irep = p->body.irep;
        if (end) {
          _irep_cutref(mrb, irep);
        }
        _irep_decref(mrb, irep);
      }
    }
    break;

  case MRB_TT_RANGE:
    _free(mrb, ((struct RRange*)obj)->edges);
    break;

  case MRB_TT_DATA:
    {
      struct RData *d = (struct RData*)obj;
      if (d->type && d->type->dfree) {
        d->type->dfree(mrb, d->data);
      }
      _gc_free_iv(mrb, (struct RObject*)obj);
    }
    break;

  default:
    break;
  }
  obj->tt = MRB_TT_FREE;
}

static void
root_scan_phase(_state *mrb, _gc *gc)
{
  int i, e;

  if (!is_minor_gc(gc)) {
    gc->gray_list = NULL;
    gc->atomic_gray_list = NULL;
  }

  _gc_mark_gv(mrb);
  /* mark arena */
  for (i=0,e=gc->arena_idx; i<e; i++) {
    _gc_mark(mrb, gc->arena[i]);
  }
  /* mark class hierarchy */
  _gc_mark(mrb, (struct RBasic*)mrb->object_class);

  /* mark built-in classes */
  _gc_mark(mrb, (struct RBasic*)mrb->class_class);
  _gc_mark(mrb, (struct RBasic*)mrb->module_class);
  _gc_mark(mrb, (struct RBasic*)mrb->proc_class);
  _gc_mark(mrb, (struct RBasic*)mrb->string_class);
  _gc_mark(mrb, (struct RBasic*)mrb->array_class);
  _gc_mark(mrb, (struct RBasic*)mrb->hash_class);
  _gc_mark(mrb, (struct RBasic*)mrb->range_class);

#ifndef MRB_WITHOUT_FLOAT
  _gc_mark(mrb, (struct RBasic*)mrb->float_class);
#endif
  _gc_mark(mrb, (struct RBasic*)mrb->fixnum_class);
  _gc_mark(mrb, (struct RBasic*)mrb->true_class);
  _gc_mark(mrb, (struct RBasic*)mrb->false_class);
  _gc_mark(mrb, (struct RBasic*)mrb->nil_class);
  _gc_mark(mrb, (struct RBasic*)mrb->symbol_class);
  _gc_mark(mrb, (struct RBasic*)mrb->kernel_module);

  _gc_mark(mrb, (struct RBasic*)mrb->eException_class);
  _gc_mark(mrb, (struct RBasic*)mrb->eStandardError_class);

  /* mark top_self */
  _gc_mark(mrb, (struct RBasic*)mrb->top_self);
  /* mark exception */
  _gc_mark(mrb, (struct RBasic*)mrb->exc);
  /* mark pre-allocated exception */
  _gc_mark(mrb, (struct RBasic*)mrb->nomem_err);
  _gc_mark(mrb, (struct RBasic*)mrb->stack_err);
#ifdef MRB_GC_FIXED_ARENA
  _gc_mark(mrb, (struct RBasic*)mrb->arena_err);
#endif

  mark_context(mrb, mrb->c);
  if (mrb->root_c != mrb->c) {
    mark_context(mrb, mrb->root_c);
  }
}

static size_t
gc_gray_mark(_state *mrb, _gc *gc, struct RBasic *obj)
{
  size_t children = 0;

  gc_mark_children(mrb, gc, obj);

  switch (obj->tt) {
  case MRB_TT_ICLASS:
    children++;
    break;

  case MRB_TT_CLASS:
  case MRB_TT_SCLASS:
  case MRB_TT_MODULE:
    {
      struct RClass *c = (struct RClass*)obj;

      children += _gc_mark_iv_size(mrb, (struct RObject*)obj);
      children += _gc_mark_mt_size(mrb, c);
      children++;
    }
    break;

  case MRB_TT_OBJECT:
  case MRB_TT_DATA:
  case MRB_TT_EXCEPTION:
    children += _gc_mark_iv_size(mrb, (struct RObject*)obj);
    break;

  case MRB_TT_ENV:
    children += MRB_ENV_STACK_LEN(obj);
    break;

  case MRB_TT_FIBER:
    {
      struct _context *c = ((struct RFiber*)obj)->cxt;
      size_t i;
      _callinfo *ci;

      if (!c || c->status == MRB_FIBER_TERMINATED) break;

      /* mark stack */
      i = c->stack - c->stbase;

      if (c->ci) {
        i += ci_nregs(c->ci);
      }
      if (c->stbase + i > c->stend) i = c->stend - c->stbase;
      children += i;

      /* mark ensure stack */
      children += c->eidx;

      /* mark closure */
      if (c->cibase) {
        for (i=0, ci = c->cibase; ci <= c->ci; i++, ci++)
          ;
      }
      children += i;
    }
    break;

  case MRB_TT_ARRAY:
    {
      struct RArray *a = (struct RArray*)obj;
      children += ARY_LEN(a);
    }
    break;

  case MRB_TT_HASH:
    children += _gc_mark_iv_size(mrb, (struct RObject*)obj);
    children += _gc_mark_hash_size(mrb, (struct RHash*)obj);
    break;

  case MRB_TT_PROC:
  case MRB_TT_RANGE:
    children+=2;
    break;

  default:
    break;
  }
  return children;
}


static void
gc_mark_gray_list(_state *mrb, _gc *gc) {
  while (gc->gray_list) {
    if (is_gray(gc->gray_list))
      gc_mark_children(mrb, gc, gc->gray_list);
    else
      gc->gray_list = gc->gray_list->gcnext;
  }
}


static size_t
incremental_marking_phase(_state *mrb, _gc *gc, size_t limit)
{
  size_t tried_marks = 0;

  while (gc->gray_list && tried_marks < limit) {
    tried_marks += gc_gray_mark(mrb, gc, gc->gray_list);
  }

  return tried_marks;
}

static void
final_marking_phase(_state *mrb, _gc *gc)
{
  int i, e;

  /* mark arena */
  for (i=0,e=gc->arena_idx; i<e; i++) {
    _gc_mark(mrb, gc->arena[i]);
  }
  _gc_mark_gv(mrb);
  mark_context(mrb, mrb->c);
  mark_context(mrb, mrb->root_c);
  _gc_mark(mrb, (struct RBasic*)mrb->exc);
  gc_mark_gray_list(mrb, gc);
  _assert(gc->gray_list == NULL);
  gc->gray_list = gc->atomic_gray_list;
  gc->atomic_gray_list = NULL;
  gc_mark_gray_list(mrb, gc);
  _assert(gc->gray_list == NULL);
}

static void
prepare_incremental_sweep(_state *mrb, _gc *gc)
{
  gc->state = MRB_GC_STATE_SWEEP;
  gc->sweeps = gc->heaps;
  gc->live_after_mark = gc->live;
}

static size_t
incremental_sweep_phase(_state *mrb, _gc *gc, size_t limit)
{
  _heap_page *page = gc->sweeps;
  size_t tried_sweep = 0;

  while (page && (tried_sweep < limit)) {
    RVALUE *p = objects(page);
    RVALUE *e = p + MRB_HEAP_PAGE_SIZE;
    size_t freed = 0;
    _bool dead_slot = TRUE;
    _bool full = (page->freelist == NULL);

    if (is_minor_gc(gc) && page->old) {
      /* skip a slot which doesn't contain any young object */
      p = e;
      dead_slot = FALSE;
    }
    while (p<e) {
      if (is_dead(gc, &p->as.basic)) {
        if (p->as.basic.tt != MRB_TT_FREE) {
          obj_free(mrb, &p->as.basic, FALSE);
          if (p->as.basic.tt == MRB_TT_FREE) {
            p->as.free.next = page->freelist;
            page->freelist = (struct RBasic*)p;
            freed++;
          }
          else {
            dead_slot = FALSE;
          }
        }
      }
      else {
        if (!is_generational(gc))
          paint_partial_white(gc, &p->as.basic); /* next gc target */
        dead_slot = FALSE;
      }
      p++;
    }

    /* free dead slot */
    if (dead_slot && freed < MRB_HEAP_PAGE_SIZE) {
      _heap_page *next = page->next;

      unlink_heap_page(gc, page);
      unlink_free_heap_page(gc, page);
      _free(mrb, page);
      page = next;
    }
    else {
      if (full && freed > 0) {
        link_free_heap_page(gc, page);
      }
      if (page->freelist == NULL && is_minor_gc(gc))
        page->old = TRUE;
      else
        page->old = FALSE;
      page = page->next;
    }
    tried_sweep += MRB_HEAP_PAGE_SIZE;
    gc->live -= freed;
    gc->live_after_mark -= freed;
  }
  gc->sweeps = page;
  return tried_sweep;
}

static size_t
incremental_gc(_state *mrb, _gc *gc, size_t limit)
{
  switch (gc->state) {
  case MRB_GC_STATE_ROOT:
    root_scan_phase(mrb, gc);
    gc->state = MRB_GC_STATE_MARK;
    flip_white_part(gc);
    return 0;
  case MRB_GC_STATE_MARK:
    if (gc->gray_list) {
      return incremental_marking_phase(mrb, gc, limit);
    }
    else {
      final_marking_phase(mrb, gc);
      prepare_incremental_sweep(mrb, gc);
      return 0;
    }
  case MRB_GC_STATE_SWEEP: {
     size_t tried_sweep = 0;
     tried_sweep = incremental_sweep_phase(mrb, gc, limit);
     if (tried_sweep == 0)
       gc->state = MRB_GC_STATE_ROOT;
     return tried_sweep;
  }
  default:
    /* unknown state */
    _assert(0);
    return 0;
  }
}

static void
incremental_gc_until(_state *mrb, _gc *gc, _gc_state to_state)
{
  do {
    incremental_gc(mrb, gc, SIZE_MAX);
  } while (gc->state != to_state);
}

static void
incremental_gc_step(_state *mrb, _gc *gc)
{
  size_t limit = 0, result = 0;
  limit = (GC_STEP_SIZE/100) * gc->step_ratio;
  while (result < limit) {
    result += incremental_gc(mrb, gc, limit);
    if (gc->state == MRB_GC_STATE_ROOT)
      break;
  }

  gc->threshold = gc->live + GC_STEP_SIZE;
}

static void
clear_all_old(_state *mrb, _gc *gc)
{
  _bool origin_mode = gc->generational;

  _assert(is_generational(gc));
  if (is_major_gc(gc)) {
    /* finish the half baked GC */
    incremental_gc_until(mrb, gc, MRB_GC_STATE_ROOT);
  }

  /* Sweep the dead objects, then reset all the live objects
   * (including all the old objects, of course) to white. */
  gc->generational = FALSE;
  prepare_incremental_sweep(mrb, gc);
  incremental_gc_until(mrb, gc, MRB_GC_STATE_ROOT);
  gc->generational = origin_mode;

  /* The gray objects have already been painted as white */
  gc->atomic_gray_list = gc->gray_list = NULL;
}

MRB_API void
_incremental_gc(_state *mrb)
{
  _gc *gc = &mrb->gc;

  if (gc->disabled || gc->iterating) return;

  GC_INVOKE_TIME_REPORT("_incremental_gc()");
  GC_TIME_START;

  if (is_minor_gc(gc)) {
    incremental_gc_until(mrb, gc, MRB_GC_STATE_ROOT);
  }
  else {
    incremental_gc_step(mrb, gc);
  }

  if (gc->state == MRB_GC_STATE_ROOT) {
    _assert(gc->live >= gc->live_after_mark);
    gc->threshold = (gc->live_after_mark/100) * gc->interval_ratio;
    if (gc->threshold < GC_STEP_SIZE) {
      gc->threshold = GC_STEP_SIZE;
    }

    if (is_major_gc(gc)) {
      size_t threshold = gc->live_after_mark/100 * MAJOR_GC_INC_RATIO;

      gc->full = FALSE;
      if (threshold < MAJOR_GC_TOOMANY) {
        gc->majorgc_old_threshold = threshold;
      }
      else {
        /* too many objects allocated during incremental GC, */
        /* instead of increasing threshold, invoke full GC. */
        _full_gc(mrb);
      }
    }
    else if (is_minor_gc(gc)) {
      if (gc->live > gc->majorgc_old_threshold) {
        clear_all_old(mrb, gc);
        gc->full = TRUE;
      }
    }
  }

  GC_TIME_STOP_AND_REPORT;
}

/* Perform a full gc cycle */
MRB_API void
_full_gc(_state *mrb)
{
  _gc *gc = &mrb->gc;

  if (gc->disabled || gc->iterating) return;

  GC_INVOKE_TIME_REPORT("_full_gc()");
  GC_TIME_START;

  if (is_generational(gc)) {
    /* clear all the old objects back to young */
    clear_all_old(mrb, gc);
    gc->full = TRUE;
  }
  else if (gc->state != MRB_GC_STATE_ROOT) {
    /* finish half baked GC cycle */
    incremental_gc_until(mrb, gc, MRB_GC_STATE_ROOT);
  }

  incremental_gc_until(mrb, gc, MRB_GC_STATE_ROOT);
  gc->threshold = (gc->live_after_mark/100) * gc->interval_ratio;

  if (is_generational(gc)) {
    gc->majorgc_old_threshold = gc->live_after_mark/100 * MAJOR_GC_INC_RATIO;
    gc->full = FALSE;
  }

  GC_TIME_STOP_AND_REPORT;
}

MRB_API void
_garbage_collect(_state *mrb)
{
  _full_gc(mrb);
}

/*
 * Field write barrier
 *   Paint obj(Black) -> value(White) to obj(Black) -> value(Gray).
 */

MRB_API void
_field_write_barrier(_state *mrb, struct RBasic *obj, struct RBasic *value)
{
  _gc *gc = &mrb->gc;

  if (!is_black(obj)) return;
  if (!is_white(value)) return;

  _assert(gc->state == MRB_GC_STATE_MARK || (!is_dead(gc, value) && !is_dead(gc, obj)));
  _assert(is_generational(gc) || gc->state != MRB_GC_STATE_ROOT);

  if (is_generational(gc) || gc->state == MRB_GC_STATE_MARK) {
    add_gray_list(mrb, gc, value);
  }
  else {
    _assert(gc->state == MRB_GC_STATE_SWEEP);
    paint_partial_white(gc, obj); /* for never write barriers */
  }
}

/*
 * Write barrier
 *   Paint obj(Black) to obj(Gray).
 *
 *   The object that is painted gray will be traversed atomically in final
 *   mark phase. So you use this write barrier if it's frequency written spot.
 *   e.g. Set element on Array.
 */

MRB_API void
_write_barrier(_state *mrb, struct RBasic *obj)
{
  _gc *gc = &mrb->gc;

  if (!is_black(obj)) return;

  _assert(!is_dead(gc, obj));
  _assert(is_generational(gc) || gc->state != MRB_GC_STATE_ROOT);
  paint_gray(obj);
  obj->gcnext = gc->atomic_gray_list;
  gc->atomic_gray_list = obj;
}

/*
 *  call-seq:
 *     GC.start                     -> nil
 *
 *  Initiates full garbage collection.
 *
 */

static _value
gc_start(_state *mrb, _value obj)
{
  _full_gc(mrb);
  return _nil_value();
}

/*
 *  call-seq:
 *     GC.enable    -> true or false
 *
 *  Enables garbage collection, returning <code>true</code> if garbage
 *  collection was previously disabled.
 *
 *     GC.disable   #=> false
 *     GC.enable    #=> true
 *     GC.enable    #=> false
 *
 */

static _value
gc_enable(_state *mrb, _value obj)
{
  _bool old = mrb->gc.disabled;

  mrb->gc.disabled = FALSE;

  return _bool_value(old);
}

/*
 *  call-seq:
 *     GC.disable    -> true or false
 *
 *  Disables garbage collection, returning <code>true</code> if garbage
 *  collection was already disabled.
 *
 *     GC.disable   #=> false
 *     GC.disable   #=> true
 *
 */

static _value
gc_disable(_state *mrb, _value obj)
{
  _bool old = mrb->gc.disabled;

  mrb->gc.disabled = TRUE;

  return _bool_value(old);
}

/*
 *  call-seq:
 *     GC.interval_ratio      -> fixnum
 *
 *  Returns ratio of GC interval. Default value is 200(%).
 *
 */

static _value
gc_interval_ratio_get(_state *mrb, _value obj)
{
  return _fixnum_value(mrb->gc.interval_ratio);
}

/*
 *  call-seq:
 *     GC.interval_ratio = fixnum    -> nil
 *
 *  Updates ratio of GC interval. Default value is 200(%).
 *  GC start as soon as after end all step of GC if you set 100(%).
 *
 */

static _value
gc_interval_ratio_set(_state *mrb, _value obj)
{
  _int ratio;

  _get_args(mrb, "i", &ratio);
  mrb->gc.interval_ratio = (int)ratio;
  return _nil_value();
}

/*
 *  call-seq:
 *     GC.step_ratio    -> fixnum
 *
 *  Returns step span ratio of Incremental GC. Default value is 200(%).
 *
 */

static _value
gc_step_ratio_get(_state *mrb, _value obj)
{
  return _fixnum_value(mrb->gc.step_ratio);
}

/*
 *  call-seq:
 *     GC.step_ratio = fixnum   -> nil
 *
 *  Updates step span ratio of Incremental GC. Default value is 200(%).
 *  1 step of incrementalGC becomes long if a rate is big.
 *
 */

static _value
gc_step_ratio_set(_state *mrb, _value obj)
{
  _int ratio;

  _get_args(mrb, "i", &ratio);
  mrb->gc.step_ratio = (int)ratio;
  return _nil_value();
}

static void
change_gen_gc_mode(_state *mrb, _gc *gc, _bool enable)
{
  if (gc->disabled || gc->iterating) {
    _raise(mrb, E_RUNTIME_ERROR, "generational mode changed when GC disabled");
    return;
  }
  if (is_generational(gc) && !enable) {
    clear_all_old(mrb, gc);
    _assert(gc->state == MRB_GC_STATE_ROOT);
    gc->full = FALSE;
  }
  else if (!is_generational(gc) && enable) {
    incremental_gc_until(mrb, gc, MRB_GC_STATE_ROOT);
    gc->majorgc_old_threshold = gc->live_after_mark/100 * MAJOR_GC_INC_RATIO;
    gc->full = FALSE;
  }
  gc->generational = enable;
}

/*
 *  call-seq:
 *     GC.generational_mode -> true or false
 *
 *  Returns generational or normal gc mode.
 *
 */

static _value
gc_generational_mode_get(_state *mrb, _value self)
{
  return _bool_value(mrb->gc.generational);
}

/*
 *  call-seq:
 *     GC.generational_mode = true or false -> true or false
 *
 *  Changes to generational or normal gc mode.
 *
 */

static _value
gc_generational_mode_set(_state *mrb, _value self)
{
  _bool enable;

  _get_args(mrb, "b", &enable);
  if (mrb->gc.generational != enable)
    change_gen_gc_mode(mrb, &mrb->gc, enable);

  return _bool_value(enable);
}


static void
gc_each_objects(_state *mrb, _gc *gc, _each_object_callback *callback, void *data)
{
  _heap_page* page;

  page = gc->heaps;
  while (page != NULL) {
    RVALUE *p;
    int i;

    p = objects(page);
    for (i=0; i < MRB_HEAP_PAGE_SIZE; i++) {
      if ((*callback)(mrb, &p[i].as.basic, data) == MRB_EACH_OBJ_BREAK)
        return;
    }
    page = page->next;
  }
}

void
_objspace_each_objects(_state *mrb, _each_object_callback *callback, void *data)
{
  _bool iterating = mrb->gc.iterating;

  _full_gc(mrb);
  mrb->gc.iterating = TRUE;
  if (iterating) {
    gc_each_objects(mrb, &mrb->gc, callback, data);
  }
  else {
    struct _jmpbuf *prev_jmp = mrb->jmp;
    struct _jmpbuf c_jmp;

    MRB_TRY(&c_jmp) {
      mrb->jmp = &c_jmp;
      gc_each_objects(mrb, &mrb->gc, callback, data);
      mrb->jmp = prev_jmp;
      mrb->gc.iterating = iterating;
   } MRB_CATCH(&c_jmp) {
      mrb->gc.iterating = iterating;
      mrb->jmp = prev_jmp;
      MRB_THROW(prev_jmp);
    } MRB_END_EXC(&c_jmp);
  }
}

#ifdef GC_TEST
#ifdef GC_DEBUG
static _value gc_test(_state *, _value);
#endif
#endif

void
_init_gc(_state *mrb)
{
  struct RClass *gc;

  gc = _define_module(mrb, "GC");

  _define_class_method(mrb, gc, "start", gc_start, MRB_ARGS_NONE());
  _define_class_method(mrb, gc, "enable", gc_enable, MRB_ARGS_NONE());
  _define_class_method(mrb, gc, "disable", gc_disable, MRB_ARGS_NONE());
  _define_class_method(mrb, gc, "interval_ratio", gc_interval_ratio_get, MRB_ARGS_NONE());
  _define_class_method(mrb, gc, "interval_ratio=", gc_interval_ratio_set, MRB_ARGS_REQ(1));
  _define_class_method(mrb, gc, "step_ratio", gc_step_ratio_get, MRB_ARGS_NONE());
  _define_class_method(mrb, gc, "step_ratio=", gc_step_ratio_set, MRB_ARGS_REQ(1));
  _define_class_method(mrb, gc, "generational_mode=", gc_generational_mode_set, MRB_ARGS_REQ(1));
  _define_class_method(mrb, gc, "generational_mode", gc_generational_mode_get, MRB_ARGS_NONE());
#ifdef GC_TEST
#ifdef GC_DEBUG
  _define_class_method(mrb, gc, "test", gc_test, MRB_ARGS_NONE());
#endif
#endif
}

#ifdef GC_TEST
#ifdef GC_DEBUG
void
test__field_write_barrier(void)
{
  _state *mrb = _open();
  struct RBasic *obj, *value;
  _gc *gc = &mrb->gc;

  puts("test__field_write_barrier");
  gc->generational = FALSE;
  obj = _basic_ptr(_ary_new(mrb));
  value = _basic_ptr(_str_new_lit(mrb, "value"));
  paint_black(obj);
  paint_partial_white(gc, value);


  puts("  in MRB_GC_STATE_MARK");
  gc->state = MRB_GC_STATE_MARK;
  _field_write_barrier(mrb, obj, value);

  _assert(is_gray(value));


  puts("  in MRB_GC_STATE_SWEEP");
  paint_partial_white(gc, value);
  gc->state = MRB_GC_STATE_SWEEP;
  _field_write_barrier(mrb, obj, value);

  _assert(obj->color & gc->current_white_part);
  _assert(value->color & gc->current_white_part);


  puts("  fail with black");
  gc->state = MRB_GC_STATE_MARK;
  paint_white(obj);
  paint_partial_white(gc, value);
  _field_write_barrier(mrb, obj, value);

  _assert(obj->color & gc->current_white_part);


  puts("  fail with gray");
  gc->state = MRB_GC_STATE_MARK;
  paint_black(obj);
  paint_gray(value);
  _field_write_barrier(mrb, obj, value);

  _assert(is_gray(value));


  {
    puts("test__field_write_barrier_value");
    obj = _basic_ptr(_ary_new(mrb));
    _value value = _str_new_lit(mrb, "value");
    paint_black(obj);
    paint_partial_white(gc, _basic_ptr(value));

    gc->state = MRB_GC_STATE_MARK;
    _field_write_barrier_value(mrb, obj, value);

    _assert(is_gray(_basic_ptr(value)));
  }

  _close(mrb);
}

void
test__write_barrier(void)
{
  _state *mrb = _open();
  struct RBasic *obj;
  _gc *gc = &mrb->gc;

  puts("test__write_barrier");
  obj = _basic_ptr(_ary_new(mrb));
  paint_black(obj);

  puts("  in MRB_GC_STATE_MARK");
  gc->state = MRB_GC_STATE_MARK;
  _write_barrier(mrb, obj);

  _assert(is_gray(obj));
  _assert(gc->atomic_gray_list == obj);


  puts("  fail with gray");
  paint_gray(obj);
  _write_barrier(mrb, obj);

  _assert(is_gray(obj));

  _close(mrb);
}

void
test_add_gray_list(void)
{
  _state *mrb = _open();
  struct RBasic *obj1, *obj2;
  _gc *gc = &mrb->gc;

  puts("test_add_gray_list");
  change_gen_gc_mode(mrb, gc, FALSE);
  _assert(gc->gray_list == NULL);
  obj1 = _basic_ptr(_str_new_lit(mrb, "test"));
  add_gray_list(mrb, gc, obj1);
  _assert(gc->gray_list == obj1);
  _assert(is_gray(obj1));

  obj2 = _basic_ptr(_str_new_lit(mrb, "test"));
  add_gray_list(mrb, gc, obj2);
  _assert(gc->gray_list == obj2);
  _assert(gc->gray_list->gcnext == obj1);
  _assert(is_gray(obj2));

  _close(mrb);
}

void
test_gc_gray_mark(void)
{
  _state *mrb = _open();
  _value obj_v, value_v;
  struct RBasic *obj;
  size_t gray_num = 0;
  _gc *gc = &mrb->gc;

  puts("test_gc_gray_mark");

  puts("  in MRB_TT_CLASS");
  obj = (struct RBasic*)mrb->object_class;
  paint_gray(obj);
  gray_num = gc_gray_mark(mrb, gc, obj);
  _assert(is_black(obj));
  _assert(gray_num > 1);

  puts("  in MRB_TT_ARRAY");
  obj_v = _ary_new(mrb);
  value_v = _str_new_lit(mrb, "test");
  paint_gray(_basic_ptr(obj_v));
  paint_partial_white(gc, _basic_ptr(value_v));
  _ary_push(mrb, obj_v, value_v);
  gray_num = gc_gray_mark(mrb, gc, _basic_ptr(obj_v));
  _assert(is_black(_basic_ptr(obj_v)));
  _assert(is_gray(_basic_ptr(value_v)));
  _assert(gray_num == 1);

  _close(mrb);
}

void
test_incremental_gc(void)
{
  _state *mrb = _open();
  size_t max = ~0, live = 0, total = 0, freed = 0;
  RVALUE *free;
  _heap_page *page;
  _gc *gc = &mrb->gc;

  puts("test_incremental_gc");
  change_gen_gc_mode(mrb, gc, FALSE);

  puts("  in _full_gc");
  _full_gc(mrb);

  _assert(gc->state == MRB_GC_STATE_ROOT);
  puts("  in MRB_GC_STATE_ROOT");
  incremental_gc(mrb, gc, max);
  _assert(gc->state == MRB_GC_STATE_MARK);
  puts("  in MRB_GC_STATE_MARK");
  incremental_gc_until(mrb, gc, MRB_GC_STATE_SWEEP);
  _assert(gc->state == MRB_GC_STATE_SWEEP);

  puts("  in MRB_GC_STATE_SWEEP");
  page = gc->heaps;
  while (page) {
    RVALUE *p = objects(page);
    RVALUE *e = p + MRB_HEAP_PAGE_SIZE;
    while (p<e) {
      if (is_black(&p->as.basic)) {
        live++;
      }
      if (is_gray(&p->as.basic) && !is_dead(gc, &p->as.basic)) {
        printf("%p\n", &p->as.basic);
      }
      p++;
    }
    page = page->next;
    total += MRB_HEAP_PAGE_SIZE;
  }

  _assert(gc->gray_list == NULL);

  incremental_gc(mrb, gc, max);
  _assert(gc->state == MRB_GC_STATE_SWEEP);

  incremental_gc(mrb, gc, max);
  _assert(gc->state == MRB_GC_STATE_ROOT);

  free = (RVALUE*)gc->heaps->freelist;
  while (free) {
   freed++;
   free = (RVALUE*)free->as.free.next;
  }

  _assert(gc->live == live);
  _assert(gc->live == total-freed);

  puts("test_incremental_gc(gen)");
  incremental_gc_until(mrb, gc, MRB_GC_STATE_SWEEP);
  change_gen_gc_mode(mrb, gc, TRUE);

  _assert(gc->full == FALSE);
  _assert(gc->state == MRB_GC_STATE_ROOT);

  puts("  in minor");
  _assert(is_minor_gc(gc));
  _assert(gc->majorgc_old_threshold > 0);
  gc->majorgc_old_threshold = 0;
  _incremental_gc(mrb);
  _assert(gc->full == TRUE);
  _assert(gc->state == MRB_GC_STATE_ROOT);

  puts("  in major");
  _assert(is_major_gc(gc));
  do {
    _incremental_gc(mrb);
  } while (gc->state != MRB_GC_STATE_ROOT);
  _assert(gc->full == FALSE);

  _close(mrb);
}

void
test_incremental_sweep_phase(void)
{
  _state *mrb = _open();
  _gc *gc = &mrb->gc;

  puts("test_incremental_sweep_phase");

  add_heap(mrb, gc);
  gc->sweeps = gc->heaps;

  _assert(gc->heaps->next->next == NULL);
  _assert(gc->free_heaps->next->next == NULL);
  incremental_sweep_phase(mrb, gc, MRB_HEAP_PAGE_SIZE * 3);

  _assert(gc->heaps->next == NULL);
  _assert(gc->heaps == gc->free_heaps);

  _close(mrb);
}

static _value
gc_test(_state *mrb, _value self)
{
  test__field_write_barrier();
  test__write_barrier();
  test_add_gray_list();
  test_gc_gray_mark();
  test_incremental_gc();
  test_incremental_sweep_phase();
  return _nil_value();
}
#endif  /* GC_DEBUG */
#endif  /* GC_TEST */
