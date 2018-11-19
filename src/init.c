/*
** init.c - initialize mruby core
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>

void $init_symtbl($state*);
void $init_class($state*);
void $init_object($state*);
void $init_kernel($state*);
void $init_comparable($state*);
void $init_enumerable($state*);
void $init_symbol($state*);
void $init_string($state*);
void $init_exception($state*);
void $init_proc($state*);
void $init_array($state*);
void $init_hash($state*);
void $init_numeric($state*);
void $init_range($state*);
void $init_gc($state*);
void $init_math($state*);
void $init_version($state*);
void $init_mrblib($state*);

#define DONE $gc_arena_restore(mrb, 0);
void
$init_core($state *mrb)
{
  $init_symtbl(mrb); DONE;

  $init_class(mrb); DONE;
  $init_object(mrb); DONE;
  $init_kernel(mrb); DONE;
  $init_comparable(mrb); DONE;
  $init_enumerable(mrb); DONE;

  $init_symbol(mrb); DONE;
  $init_string(mrb); DONE;
  $init_exception(mrb); DONE;
  $init_proc(mrb); DONE;
  $init_array(mrb); DONE;
  $init_hash(mrb); DONE;
  $init_numeric(mrb); DONE;
  $init_range(mrb); DONE;
  $init_gc(mrb); DONE;
  $init_version(mrb); DONE;
  $init_mrblib(mrb); DONE;
}
