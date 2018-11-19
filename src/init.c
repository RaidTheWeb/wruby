/*
** init.c - initialize mruby core
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>

void _init_symtbl(state*);
void _init_class(state*);
void _init_object(state*);
void _init_kernel(state*);
void _init_comparable(state*);
void _init_enumerable(state*);
void _init_symbol(state*);
void _init_string(state*);
void _init_exception(state*);
void _init_proc(state*);
void _init_array(state*);
void _init_hash(state*);
void _init_numeric(state*);
void _init_range(state*);
void _init_gc(state*);
void _init_math(state*);
void _init_version(state*);
void _init_mrblib(state*);

#define DONE _gc_arena_restore(mrb, 0);
void
_init_core(state *mrb)
{
  _init_symtbl(mrb); DONE;

  _init_class(mrb); DONE;
  _init_object(mrb); DONE;
  _init_kernel(mrb); DONE;
  _init_comparable(mrb); DONE;
  _init_enumerable(mrb); DONE;

  _init_symbol(mrb); DONE;
  _init_string(mrb); DONE;
  _init_exception(mrb); DONE;
  _init_proc(mrb); DONE;
  _init_array(mrb); DONE;
  _init_hash(mrb); DONE;
  _init_numeric(mrb); DONE;
  _init_range(mrb); DONE;
  _init_gc(mrb); DONE;
  _init_version(mrb); DONE;
  _init_mrblib(mrb); DONE;
}
