/*
** mruby/gc.h - garbage collector for mruby
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_GC_H
#define MRUBY_GC_H

#include "common.h"

/**
 * Uncommon memory management stuffs.
 */
BEGIN_DECL


struct state;

#define EACH_OBJ_OK 0
#define EACH_OBJ_BREAK 1
typedef int (each_object_callback)(struct state *mrb, struct RBasic *obj, void *data);
void objspace_each_objects(struct state *mrb, each_object_callback *callback, void *data);
API void free_context(struct state *mrb, struct context *c);

#ifndef GC_ARENA_SIZE
#define GC_ARENA_SIZE 100
#endif

typedef enum {
  GC_STATE_ROOT = 0,
  GC_STATE_MARK,
  GC_STATE_SWEEP
} gc_state;

/* Disable MSVC warning "C4200: nonstandard extension used: zero-sized array
 * in struct/union" when in C++ mode */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif

typedef struct heap_page {
  struct RBasic *freelist;
  struct heap_page *prev;
  struct heap_page *next;
  struct heap_page *free_next;
  struct heap_page *free_prev;
  bool old:1;
  void *objects[];
} heap_page;

#ifdef _MSC_VER
#pragma warning(pop)
#endif

typedef struct gc {
  heap_page *heaps;                /* heaps for GC */
  heap_page *sweeps;
  heap_page *free_heaps;
  size_t live; /* count of live objects */
#ifdef GC_FIXED_ARENA
  struct RBasic *arena[GC_ARENA_SIZE]; /* GC protection array */
#else
  struct RBasic **arena;                   /* GC protection array */
  int arena_capa;
#endif
  int arena_idx;

  gc_state state; /* state of gc */
  int current_white_part; /* make white object by white_part */
  struct RBasic *gray_list; /* list of gray objects to be traversed incrementally */
  struct RBasic *atomic_gray_list; /* list of objects to be traversed atomically */
  size_t live_after_mark;
  size_t threshold;
  int interval_ratio;
  int step_ratio;
  bool iterating     :1;
  bool disabled      :1;
  bool full          :1;
  bool generational  :1;
  bool out_of_memory :1;
  size_t majorgc_old_threshold;
} gc;

API bool
object_dead_p(struct state *mrb, struct RBasic *object);

END_DECL

#endif  /* MRUBY_GC_H */
