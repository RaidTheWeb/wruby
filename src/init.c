/*
** init.c - initialize mruby core
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>

void _init_symtbl(_state*);
void _init_class(_state*);
void _init_object(_state*);
void _init_kernel(_state*);
void _init_comparable(_state*);
void _init_enumerable(_state*);
void _init_symbol(_state*);
void _init_string(_state*);
void _init_exception(_state*);
void _init_proc(_state*);
void _init_array(_state*);
void _init_hash(_state*);
void _init_numeric(_state*);
void _init_range(_state*);
void _init_gc(_state*);
void _init_math(_state*);
void _init_version(_state*);
void _init_mrblib(_state*);

#define DONE _gc_arena_restore(mrb, 0);
void
_init_core(_state *mrb)
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
//  _init_mrblib(mrb); DONE;
}
