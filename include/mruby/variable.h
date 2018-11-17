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
BEGIN_DECL

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
  sym id;
};

value vm_special_get(state*, sym);
void vm_special_set(state*, sym, value);
value vm_cv_get(state*, sym);
void vm_cv_set(state*, sym, value);
value vm_const_get(state*, sym);
void vm_const_set(state*, sym, value);
API value const_get(state*, value, sym);
API void const_set(state*, value, sym, value);
API bool const_defined(state*, value, sym);
API void const_remove(state*, value, sym);

API bool iv_name_sym_p(state *mrb, sym sym);
API void iv_name_sym_check(state *mrb, sym sym);
API value obj_iv_get(state *mrb, struct RObject *obj, sym sym);
API void obj_iv_set(state *mrb, struct RObject *obj, sym sym, value v);
API bool obj_iv_defined(state *mrb, struct RObject *obj, sym sym);
API value iv_get(state *mrb, value obj, sym sym);
API void iv_set(state *mrb, value obj, sym sym, value v);
API bool iv_defined(state*, value, sym);
API value iv_remove(state *mrb, value obj, sym sym);
API void iv_copy(state *mrb, value dst, value src);
API bool const_defined_at(state *mrb, value mod, sym id);

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
 *     sym sym = intern_lit(mrb, "$value");
 *     value var = gv_get(mrb, sym);
 *
 * @param mrb The mruby state reference
 * @param sym The name of the global variable
 * @return The value of that global variable. May be nil
 */
API value gv_get(state *mrb, sym sym);

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
 *     sym sym = intern_lit(mrb, "$value");
 *     gv_set(mrb, sym, str_new_lit("foo"));
 *
 * @param mrb The mruby state reference
 * @param sym The name of the global variable
 * @param val The value of the global variable
 */
API void gv_set(state *mrb, sym sym, value val);

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
 *     sym sym = intern_lit(mrb, "$value");
 *     gv_remove(mrb, sym);
 *
 * @param mrb The mruby state reference
 * @param sym The name of the global variable
 * @param val The value of the global variable
 */
API void gv_remove(state *mrb, sym sym);

API value cv_get(state *mrb, value mod, sym sym);
API void mod_cv_set(state *mrb, struct RClass * c, sym sym, value v);
API void cv_set(state *mrb, value mod, sym sym, value v);
API bool cv_defined(state *mrb, value mod, sym sym);
value obj_iv_inspect(state*, struct RObject*);
value mod_constants(state *mrb, value mod);
value f_global_variables(state *mrb, value self);
value obj_instance_variables(state*, value);
value mod_class_variables(state*, value);
value mod_cv_get(state *mrb, struct RClass * c, sym sym);
bool mod_cv_defined(state *mrb, struct RClass * c, sym sym);

/* GC functions */
void gc_mark_gv(state*);
void gc_free_gv(state*);
void gc_mark_iv(state*, struct RObject*);
size_t gc_mark_iv_size(state*, struct RObject*);
void gc_free_iv(state*, struct RObject*);

END_DECL

#endif  /* MRUBY_VARIABLE_H */
