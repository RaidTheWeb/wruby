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
  value *data;
  value (*getter)(void);
  void  (*setter)(void);
  /* void  (*marker)(); */
  /* int block_trace; */
  /* struct trace_var *trace; */
} global_variable;

struct global_entry {
  global_variable *var;
  _sym id;
};

value _vm_special_get(state*, _sym);
void _vm_special_set(state*, _sym, value);
value _vm_cv_get(state*, _sym);
void _vm_cv_set(state*, _sym, value);
value _vm_const_get(state*, _sym);
void _vm_const_set(state*, _sym, value);
MRB_API value _const_get(state*, value, _sym);
MRB_API void _const_set(state*, value, _sym, value);
MRB_API _bool _const_defined(state*, value, _sym);
MRB_API void _const_remove(state*, value, _sym);

MRB_API _bool _iv_name_sym_p(state *mrb, _sym sym);
MRB_API void _iv_name_sym_check(state *mrb, _sym sym);
MRB_API value _obj_iv_get(state *mrb, struct RObject *obj, _sym sym);
MRB_API void _obj_iv_set(state *mrb, struct RObject *obj, _sym sym, value v);
MRB_API _bool _obj_iv_defined(state *mrb, struct RObject *obj, _sym sym);
MRB_API value _iv_get(state *mrb, value obj, _sym sym);
MRB_API void _iv_set(state *mrb, value obj, _sym sym, value v);
MRB_API _bool _iv_defined(state*, value, _sym);
MRB_API value _iv_remove(state *mrb, value obj, _sym sym);
MRB_API void _iv_copy(state *mrb, value dst, value src);
MRB_API _bool _const_defined_at(state *mrb, value mod, _sym id);

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
 *     value var = _gv_get(mrb, sym);
 *
 * @param mrb The mruby state reference
 * @param sym The name of the global variable
 * @return The value of that global variable. May be nil
 */
MRB_API value _gv_get(state *mrb, _sym sym);

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
MRB_API void _gv_set(state *mrb, _sym sym, value val);

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
MRB_API void _gv_remove(state *mrb, _sym sym);

MRB_API value _cv_get(state *mrb, value mod, _sym sym);
MRB_API void _mod_cv_set(state *mrb, struct RClass * c, _sym sym, value v);
MRB_API void _cv_set(state *mrb, value mod, _sym sym, value v);
MRB_API _bool _cv_defined(state *mrb, value mod, _sym sym);
value _obj_iv_inspect(state*, struct RObject*);
value _mod_constants(state *mrb, value mod);
value _f_global_variables(state *mrb, value self);
value _obj_instance_variables(state*, value);
value _mod_class_variables(state*, value);
value _mod_cv_get(state *mrb, struct RClass * c, _sym sym);
_bool _mod_cv_defined(state *mrb, struct RClass * c, _sym sym);

/* GC functions */
void _gc_mark_gv(state*);
void _gc_free_gv(state*);
void _gc_mark_iv(state*, struct RObject*);
size_t _gc_mark_iv_size(state*, struct RObject*);
void _gc_free_iv(state*, struct RObject*);

MRB_END_DECL

#endif  /* MRUBY_VARIABLE_H */
