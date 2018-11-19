/*
** mruby/variable.h - mruby variables
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_VARIABLE_H
#define MRUBY_VARIABLE_H

#include "common.h"

/**
 * Functions to access mruby variables.
 */
MRB_BEGIN_DECL

typedef struct global_variable {
  int   counter;
  _value *data;
  _value (*getter)(void);
  void  (*setter)(void);
  /* void  (*marker)(); */
  /* int block_trace; */
  /* struct trace_var *trace; */
} global_variable;

struct global_entry {
  global_variable *var;
  _sym id;
};

_value _vm_special_get(_state*, _sym);
void _vm_special_set(_state*, _sym, _value);
_value _vm_cv_get(_state*, _sym);
void _vm_cv_set(_state*, _sym, _value);
_value _vm_const_get(_state*, _sym);
void _vm_const_set(_state*, _sym, _value);
MRB_API _value _const_get(_state*, _value, _sym);
MRB_API void _const_set(_state*, _value, _sym, _value);
MRB_API _bool _const_defined(_state*, _value, _sym);
MRB_API void _const_remove(_state*, _value, _sym);

MRB_API _bool _iv_name_sym_p(_state *mrb, _sym sym);
MRB_API void _iv_name_sym_check(_state *mrb, _sym sym);
MRB_API _value _obj_iv_get(_state *mrb, struct RObject *obj, _sym sym);
MRB_API void _obj_iv_set(_state *mrb, struct RObject *obj, _sym sym, _value v);
MRB_API _bool _obj_iv_defined(_state *mrb, struct RObject *obj, _sym sym);
MRB_API _value _iv_get(_state *mrb, _value obj, _sym sym);
MRB_API void _iv_set(_state *mrb, _value obj, _sym sym, _value v);
MRB_API _bool _iv_defined(_state*, _value, _sym);
MRB_API _value _iv_remove(_state *mrb, _value obj, _sym sym);
MRB_API void _iv_copy(_state *mrb, _value dst, _value src);
MRB_API _bool _const_defined_at(_state *mrb, _value mod, _sym id);

/**
 * Get a global variable. Will return nil if the var does not exist
 *
 * Example:
 *
 *     !!!ruby
 *     # Ruby style
 *     var = $value
 *
 *     !!!c
 *     // C style
 *     _sym sym = _intern_lit(mrb, "$value");
 *     _value var = _gv_get(mrb, sym);
 *
 * @param mrb The mruby state reference
 * @param sym The name of the global variable
 * @return The value of that global variable. May be nil
 */
MRB_API _value _gv_get(_state *mrb, _sym sym);

/**
 * Set a global variable
 *
 * Example:
 *
 *     !!!ruby
 *     # Ruby style
 *     $value = "foo"
 *
 *     !!!c
 *     // C style
 *     _sym sym = _intern_lit(mrb, "$value");
 *     _gv_set(mrb, sym, _str_new_lit("foo"));
 *
 * @param mrb The mruby state reference
 * @param sym The name of the global variable
 * @param val The value of the global variable
 */
MRB_API void _gv_set(_state *mrb, _sym sym, _value val);

/**
 * Remove a global variable.
 *
 * Example:
 *
 *     !!!ruby
 *     # Ruby style
 *     $value = nil
 *
 *     !!!c
 *     // C style
 *     _sym sym = _intern_lit(mrb, "$value");
 *     _gv_remove(mrb, sym);
 *
 * @param mrb The mruby state reference
 * @param sym The name of the global variable
 * @param val The value of the global variable
 */
MRB_API void _gv_remove(_state *mrb, _sym sym);

MRB_API _value _cv_get(_state *mrb, _value mod, _sym sym);
MRB_API void _mod_cv_set(_state *mrb, struct RClass * c, _sym sym, _value v);
MRB_API void _cv_set(_state *mrb, _value mod, _sym sym, _value v);
MRB_API _bool _cv_defined(_state *mrb, _value mod, _sym sym);
_value _obj_iv_inspect(_state*, struct RObject*);
_value _mod_constants(_state *mrb, _value mod);
_value _f_global_variables(_state *mrb, _value self);
_value _obj_instance_variables(_state*, _value);
_value _mod_class_variables(_state*, _value);
_value _mod_cv_get(_state *mrb, struct RClass * c, _sym sym);
_bool _mod_cv_defined(_state *mrb, struct RClass * c, _sym sym);

/* GC functions */
void _gc_mark_gv(_state*);
void _gc_free_gv(_state*);
void _gc_mark_iv(_state*, struct RObject*);
size_t _gc_mark_iv_size(_state*, struct RObject*);
void _gc_free_iv(_state*, struct RObject*);

MRB_END_DECL

#endif  /* MRUBY_VARIABLE_H */
