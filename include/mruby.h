/*
** mruby - An embeddable Ruby implementation
**
** Copyright (c) mruby developers 2010-2018
**
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**
** [ MIT license: http://www.opensource.org/licenses/mit-license.php ]
*/

#ifndef MRUBY_H
#define MRUBY_H

#ifdef __cplusplus
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#ifdef __cplusplus
#ifndef SIZE_MAX
#ifdef __SIZE_MAX__
#define SIZE_MAX __SIZE_MAX__
#else
#define SIZE_MAX std::numeric_limits<size_t>::max()
#endif
#endif
#endif

#ifdef MRB_DEBUG
#include <assert.h>
#define _assert(p) assert(p)
#define _assert_int_fit(t1,n,t2,max) assert((n)>=0 && ((sizeof(n)<=sizeof(t2))||(n<=(t1)(max))))
#else
#define _assert(p) ((void)0)
#define _assert_int_fit(t1,n,t2,max) ((void)0)
#endif

#if __STDC_VERSION__ >= 201112L
#define _static_assert(exp, str) _Static_assert(exp, str)
#else
#define _static_assert(exp, str) _assert(exp)
#endif

#include "mrbconf.h"

#ifndef MRB_WITHOUT_FLOAT
#ifndef FLT_EPSILON
#define FLT_EPSILON (1.19209290e-07f)
#endif
#ifndef DBL_EPSILON
#define DBL_EPSILON ((double)2.22044604925031308085e-16L)
#endif
#ifndef LDBL_EPSILON
#define LDBL_EPSILON (1.08420217248550443401e-19L)
#endif

#ifdef MRB_USE_FLOAT
#define MRB_FLOAT_EPSILON FLT_EPSILON
#else
#define MRB_FLOAT_EPSILON DBL_EPSILON
#endif
#endif

#include "mruby/common.h"
#include <mruby/value.h>
#include <mruby/gc.h>
#include <mruby/version.h>

/**
 * MRuby C API entry point
 */
MRB_BEGIN_DECL

typedef uint8_t _code;

/**
 * Required arguments signature type.
 */
typedef uint32_t _aspec;


struct _irep;
struct state;

/**
 * Function pointer type of custom allocator used in @see _open_allocf.
 *
 * The function pointing it must behave similarly as realloc except:
 * - If ptr is NULL it must allocate new space.
 * - If s is NULL, ptr must be freed.
 *
 * See @see _default_allocf for the default implementation.
 */
typedef void* (*_allocf) (struct state *mrb, void*, size_t, void *ud);

#ifndef MRB_FIXED_STATE_ATEXIT_STACK_SIZE
#define MRB_FIXED_STATE_ATEXIT_STACK_SIZE 5
#endif

typedef struct {
  _sym mid;
  struct RProc *proc;
  value *stackent;
  uint16_t ridx;
  uint16_t epos;
  struct REnv *env;
  _code *pc;                 /* return address */
  _code *err;                /* error position */
  int argc;
  int acc;
  struct RClass *target_class;
} _callinfo;

enum _fiber_state {
  MRB_FIBER_CREATED = 0,
  MRB_FIBER_RUNNING,
  MRB_FIBER_RESUMED,
  MRB_FIBER_SUSPENDED,
  MRB_FIBER_TRANSFERRED,
  MRB_FIBER_TERMINATED,
};

struct _context {
  struct _context *prev;

  value *stack;                       /* stack of virtual machine */
  value *stbase, *stend;

  _callinfo *ci;
  _callinfo *cibase, *ciend;

  uint16_t *rescue;                       /* exception handler stack */
  uint16_t rsize;
  struct RProc **ensure;                  /* ensure handler stack */
  uint16_t esize, eidx;

  enum _fiber_state status;
  _bool vmexec;
  struct RFiber *fib;
};

#ifdef MRB_METHOD_CACHE_SIZE
# define MRB_METHOD_CACHE
#else
/* default method cache size: 128 */
/* cache size needs to be power of 2 */
# define MRB_METHOD_CACHE_SIZE (1<<7)
#endif

typedef value (*_func_t)(struct state *mrb, value);

#ifdef MRB_METHOD_TABLE_INLINE
typedef uintptr_t _method_t;
#else
typedef struct {
  _bool func_p;
  union {
    struct RProc *proc;
    _func_t func;
  };
} _method_t;
#endif

#ifdef MRB_METHOD_CACHE
struct _cache_entry {
  struct RClass *c, *c0;
  _sym mid;
  _method_t m;
};
#endif

struct _jmpbuf;

typedef void (*_atexit_func)(struct state*);

#define MRB_STATE_NO_REGEXP 1
#define MRB_STATE_REGEXP    2

typedef struct state {
  struct _jmpbuf *jmp;

  uint32_t flags;
  _allocf allocf;                      /* memory allocation function */
  void *allocf_ud;                        /* auxiliary data of allocf */

  struct _context *c;
  struct _context *root_c;
  struct iv_tbl *globals;                 /* global variable table */

  struct RObject *exc;                    /* exception */

  struct RObject *top_self;
  struct RClass *object_class;            /* Object class */
  struct RClass *class_class;
  struct RClass *module_class;
  struct RClass *proc_class;
  struct RClass *string_class;
  struct RClass *array_class;
  struct RClass *hash_class;
  struct RClass *range_class;

#ifndef MRB_WITHOUT_FLOAT
  struct RClass *float_class;
#endif
  struct RClass *fixnum_class;
  struct RClass *true_class;
  struct RClass *false_class;
  struct RClass *nil_class;
  struct RClass *symbol_class;
  struct RClass *kernel_module;

  struct alloca_header *mems;
  _gc gc;

#ifdef MRB_METHOD_CACHE
  struct _cache_entry cache[MRB_METHOD_CACHE_SIZE];
#endif

  _sym symidx;
  struct kh_n2s *name2sym;      /* symbol hash */
  struct symbol_name *symtbl;   /* symbol table */
  size_t symcapa;

#ifdef MRB_ENABLE_DEBUG_HOOK
  void (*code_fetch_hook)(struct state* mrb, struct _irep *irep, _code *pc, value *regs);
  void (*debug_op_hook)(struct state* mrb, struct _irep *irep, _code *pc, value *regs);
#endif

#ifdef MRB_BYTECODE_DECODE_OPTION
  _code (*bytecode_decoder)(struct state* mrb, _code code);
#endif

  struct RClass *eException_class;
  struct RClass *eStandardError_class;
  struct RObject *nomem_err;              /* pre-allocated NoMemoryError */
  struct RObject *stack_err;              /* pre-allocated SysStackError */
#ifdef MRB_GC_FIXED_ARENA
  struct RObject *arena_err;              /* pre-allocated arena overfow error */
#endif

  void *ud; /* auxiliary data */

#ifdef MRB_FIXED_STATE_ATEXIT_STACK
  _atexit_func atexit_stack[MRB_FIXED_STATE_ATEXIT_STACK_SIZE];
#else
  _atexit_func *atexit_stack;
#endif
  _int atexit_stack_len;
} state;

/**
 * Defines a new class.
 *
 * If you're creating a gem it may look something like this:
 *
 *      !!!c
 *      void _example_gem_init(state* mrb) {
 *          struct RClass *example_class;
 *          example_class = _define_class(mrb, "Example_Class", mrb->object_class);
 *      }
 *
 *      void _example_gem_final(state* mrb) {
 *          //free(TheAnimals);
 *      }
 *
 * @param [state *] mrb The current mruby state.
 * @param [const char *] name The name of the defined class.
 * @param [struct RClass *] super The new class parent.
 * @return [struct RClass *] Reference to the newly defined class.
 * @see _define_class_under
 */
MRB_API struct RClass *_define_class(state *mrb, const char *name, struct RClass *super);

/**
 * Defines a new module.
 *
 * @param [state *] state* The current mruby state.
 * @param [const char *] char* The name of the module.
 * @return [struct RClass *] Reference to the newly defined module.
 */
MRB_API struct RClass *_define_module(state *, const char*);
MRB_API value _singleton_class(state*, value);

/**
 * Include a module in another class or module.
 * Equivalent to:
 *
 *   module B
 *     include A
 *   end
 * @param [state *] state* The current mruby state.
 * @param [struct RClass *] RClass* A reference to module or a class.
 * @param [struct RClass *] RClass* A reference to the module to be included.
 */
MRB_API void _include_module(state*, struct RClass*, struct RClass*);

/**
 * Prepends a module in another class or module.
 *
 * Equivalent to:
 *  module B
 *    prepend A
 *  end
 * @param [state *] state* The current mruby state.
 * @param [struct RClass *] RClass* A reference to module or a class.
 * @param [struct RClass *] RClass* A reference to the module to be prepended.
 */
MRB_API void _prepend_module(state*, struct RClass*, struct RClass*);

/**
 * Defines a global function in ruby.
 *
 * If you're creating a gem it may look something like this
 *
 * Example:
 *
 *     !!!c
 *     value example_method(state* mrb, value self)
 *     {
 *          puts("Executing example command!");
 *          return self;
 *     }
 *
 *     void _example_gem_init(state* mrb)
 *     {
 *           _define_method(mrb, mrb->kernel_module, "example_method", example_method, MRB_ARGS_NONE());
 *     }
 *
 * @param [state *] mrb The MRuby state reference.
 * @param [struct RClass *] cla The class pointer where the method will be defined.
 * @param [const char *] name The name of the method being defined.
 * @param [_func_t] func The function pointer to the method definition.
 * @param [_aspec] aspec The method parameters declaration.
 */
MRB_API void _define_method(state *mrb, struct RClass *cla, const char *name, _func_t func, _aspec aspec);

/**
 * Defines a class method.
 *
 * Example:
 *
 *     # Ruby style
 *     class Foo
 *       def Foo.bar
 *       end
 *     end
 *     // C style
 *     value bar_method(state* mrb, value self){
 *       return _nil_value();
 *     }
 *     void _example_gem_init(state* mrb){
 *       struct RClass *foo;
 *       foo = _define_class(mrb, "Foo", mrb->object_class);
 *       _define_class_method(mrb, foo, "bar", bar_method, MRB_ARGS_NONE());
 *     }
 * @param [state *] state* The MRuby state reference.
 * @param [struct RClass *] RClass* The class where the class method will be defined.
 * @param [const char *] char* The name of the class method being defined.
 * @param [_func_t] _func_t The function pointer to the class method definition.
 * @param [_aspec] _aspec The method parameters declaration.
 */
MRB_API void _define_class_method(state *, struct RClass *, const char *, _func_t, _aspec);
MRB_API void _define_singleton_method(state*, struct RObject*, const char*, _func_t, _aspec);

/**
 *  Defines a module function.
 *
 * Example:
 *
 *        # Ruby style
 *        module Foo
 *          def Foo.bar
 *          end
 *        end
 *        // C style
 *        value bar_method(state* mrb, value self){
 *          return _nil_value();
 *        }
 *        void _example_gem_init(state* mrb){
 *          struct RClass *foo;
 *          foo = _define_module(mrb, "Foo");
 *          _define_module_function(mrb, foo, "bar", bar_method, MRB_ARGS_NONE());
 *        }
 *  @param [state *] state* The MRuby state reference.
 *  @param [struct RClass *] RClass* The module where the module function will be defined.
 *  @param [const char *] char* The name of the module function being defined.
 *  @param [_func_t] _func_t The function pointer to the module function definition.
 *  @param [_aspec] _aspec The method parameters declaration.
 */
MRB_API void _define_module_function(state*, struct RClass*, const char*, _func_t, _aspec);

/**
 *  Defines a constant.
 *
 * Example:
 *
 *          # Ruby style
 *          class ExampleClass
 *            AGE = 22
 *          end
 *          // C style
 *          #include <stdio.h>
 *          #include <mruby.h>
 *
 *          void
 *          _example_gem_init(state* mrb){
 *            _define_const(mrb, mrb->kernel_module, "AGE", _fixnum_value(22));
 *          }
 *
 *          value
 *          _example_gem_final(state* mrb){
 *          }
 *  @param [state *] state* The MRuby state reference.
 *  @param [struct RClass *] RClass* A class or module the constant is defined in.
 *  @param [const char *] name The name of the constant being defined.
 *  @param [value] value The value for the constant.
 */
MRB_API void _define_const(state*, struct RClass*, const char *name, value);

/**
 * Undefines a method.
 *
 * Example:
 *
 *     # Ruby style
 *
 *     class ExampleClassA
 *       def example_method
 *         "example"
 *       end
 *     end
 *     ExampleClassA.new.example_method # => example
 *
 *     class ExampleClassB < ExampleClassA
 *       undef_method :example_method
 *     end
 *
 *     ExampleClassB.new.example_method # => undefined method 'example_method' for ExampleClassB (NoMethodError)
 *
 *     // C style
 *     #include <stdio.h>
 *     #include <mruby.h>
 *
 *     value
 *     _example_method(state *mrb){
 *       return _str_new_lit(mrb, "example");
 *     }
 *
 *     void
 *     _example_gem_init(state* mrb){
 *       struct RClass *example_class_a;
 *       struct RClass *example_class_b;
 *       struct RClass *example_class_c;
 *
 *       example_class_a = _define_class(mrb, "ExampleClassA", mrb->object_class);
 *       _define_method(mrb, example_class_a, "example_method", _example_method, MRB_ARGS_NONE());
 *       example_class_b = _define_class(mrb, "ExampleClassB", example_class_a);
 *       example_class_c = _define_class(mrb, "ExampleClassC", example_class_b);
 *       _undef_method(mrb, example_class_c, "example_method");
 *     }
 *
 *     _example_gem_final(state* mrb){
 *     }
 * @param [state*] state* The mruby state reference.
 * @param [struct RClass*] RClass* A class the method will be undefined from.
 * @param [const char] const char* The name of the method to be undefined.
 */
MRB_API void _undef_method(state*, struct RClass*, const char*);
MRB_API void _undef_method_id(state*, struct RClass*, _sym);

/**
 * Undefine a class method.
 * Example:
 *
 *      # Ruby style
 *      class ExampleClass
 *        def self.example_method
 *          "example"
 *        end
 *      end
 *
 *     ExampleClass.example_method
 *
 *     // C style
 *     #include <stdio.h>
 *     #include <mruby.h>
 *
 *     value
 *     _example_method(state *mrb){
 *       return _str_new_lit(mrb, "example");
 *     }
 *
 *     void
 *     _example_gem_init(state* mrb){
 *       struct RClass *example_class;
 *       example_class = _define_class(mrb, "ExampleClass", mrb->object_class);
 *       _define_class_method(mrb, example_class, "example_method", _example_method, MRB_ARGS_NONE());
 *       _undef_class_method(mrb, example_class, "example_method");
 *      }
 *
 *      void
 *      _example_gem_final(state* mrb){
 *      }
 * @param [state*] state* The mruby state reference.
 * @param [RClass*] RClass* A class the class method will be undefined from.
 * @param [constchar*] constchar* The name of the class method to be undefined.
 */
MRB_API void _undef_class_method(state*, struct RClass*, const char*);

/**
 * Initialize a new object instance of c class.
 *
 * Example:
 *
 *     # Ruby style
 *     class ExampleClass
 *     end
 *
 *     p ExampleClass # => #<ExampleClass:0x9958588>
 *     // C style
 *     #include <stdio.h>
 *     #include <mruby.h>
 *
 *     void
 *     _example_gem_init(state* mrb) {
 *       struct RClass *example_class;
 *       value obj;
 *       example_class = _define_class(mrb, "ExampleClass", mrb->object_class); # => class ExampleClass; end
 *       obj = _obj_new(mrb, example_class, 0, NULL); # => ExampleClass.new
 *       _p(mrb, obj); // => Kernel#p
 *      }
 * @param [state*] mrb The current mruby state.
 * @param [RClass*] c Reference to the class of the new object.
 * @param [_int] argc Number of arguments in argv
 * @param [const value *] argv Array of value to initialize the object
 * @return [value] The newly initialized object
 */
MRB_API value _obj_new(state *mrb, struct RClass *c, _int argc, const value *argv);

/** @see _obj_new */
MRB_INLINE value _class_new_instance(state *mrb, _int argc, const value *argv, struct RClass *c)
{
  return _obj_new(mrb,c,argc,argv);
}

MRB_API value _instance_new(state *mrb, value cv);

/**
 * Creates a new instance of Class, Class.
 *
 * Example:
 *
 *      void
 *      _example_gem_init(state* mrb) {
 *        struct RClass *example_class;
 *
 *        value obj;
 *        example_class = _class_new(mrb, mrb->object_class);
 *        obj = _obj_new(mrb, example_class, 0, NULL); // => #<#<Class:0x9a945b8>:0x9a94588>
 *        _p(mrb, obj); // => Kernel#p
 *       }
 *
 * @param [state*] mrb The current mruby state.
 * @param [struct RClass *] super The super class or parent.
 * @return [struct RClass *] Reference to the new class.
 */
MRB_API struct RClass * _class_new(state *mrb, struct RClass *super);

/**
 * Creates a new module, Module.
 *
 * Example:
 *      void
 *      _example_gem_init(state* mrb) {
 *        struct RClass *example_module;
 *
 *        example_module = _module_new(mrb);
 *      }
 *
 * @param [state*] mrb The current mruby state.
 * @return [struct RClass *] Reference to the new module.
 */
MRB_API struct RClass * _module_new(state *mrb);

/**
 * Returns an _bool. True if class was defined, and false if the class was not defined.
 *
 * Example:
 *     void
 *     _example_gem_init(state* mrb) {
 *       struct RClass *example_class;
 *       _bool cd;
 *
 *       example_class = _define_class(mrb, "ExampleClass", mrb->object_class);
 *       cd = _class_defined(mrb, "ExampleClass");
 *
 *       // If _class_defined returns 1 then puts "True"
 *       // If _class_defined returns 0 then puts "False"
 *       if (cd == 1){
 *         puts("True");
 *       }
 *       else {
 *         puts("False");
 *       }
 *      }
 *
 * @param [state*] mrb The current mruby state.
 * @param [const char *] name A string representing the name of the class.
 * @return [_bool] A boolean value.
 */
MRB_API _bool _class_defined(state *mrb, const char *name);

/**
 * Gets a class.
 * @param [state*] mrb The current mruby state.
 * @param [const char *] name The name of the class.
 * @return [struct RClass *] A reference to the class.
*/
MRB_API struct RClass * _class_get(state *mrb, const char *name);

/**
 * Gets a exception class.
 * @param [state*] mrb The current mruby state.
 * @param [const char *] name The name of the class.
 * @return [struct RClass *] A reference to the class.
*/
MRB_API struct RClass * _exc_get(state *mrb, const char *name);

/**
 * Returns an _bool. True if inner class was defined, and false if the inner class was not defined.
 *
 * Example:
 *     void
 *     _example_gem_init(state* mrb) {
 *       struct RClass *example_outer, *example_inner;
 *       _bool cd;
 *
 *       example_outer = _define_module(mrb, "ExampleOuter");
 *
 *       example_inner = _define_class_under(mrb, example_outer, "ExampleInner", mrb->object_class);
 *       cd = _class_defined_under(mrb, example_outer, "ExampleInner");
 *
 *       // If _class_defined_under returns 1 then puts "True"
 *       // If _class_defined_under returns 0 then puts "False"
 *       if (cd == 1){
 *         puts("True");
 *       }
 *       else {
 *         puts("False");
 *       }
 *      }
 *
 * @param [state*] mrb The current mruby state.
 * @param [struct RClass *] outer The name of the outer class.
 * @param [const char *] name A string representing the name of the inner class.
 * @return [_bool] A boolean value.
 */
MRB_API _bool _class_defined_under(state *mrb, struct RClass *outer, const char *name);

/**
 * Gets a child class.
 * @param [state*] mrb The current mruby state.
 * @param [struct RClass *] outer The name of the parent class.
 * @param [const char *] name The name of the class.
 * @return [struct RClass *] A reference to the class.
*/
MRB_API struct RClass * _class_get_under(state *mrb, struct RClass *outer, const char *name);

/**
 * Gets a module.
 * @param [state*] mrb The current mruby state.
 * @param [const char *] name The name of the module.
 * @return [struct RClass *] A reference to the module.
*/
MRB_API struct RClass * _module_get(state *mrb, const char *name);

/**
 * Gets a module defined under another module.
 * @param [state*] mrb The current mruby state.
 * @param [struct RClass *] outer The name of the outer module.
 * @param [const char *] name The name of the module.
 * @return [struct RClass *] A reference to the module.
*/
MRB_API struct RClass * _module_get_under(state *mrb, struct RClass *outer, const char *name);
MRB_API value _notimplement_m(state*, value);

/**
 * Duplicate an object.
 *
 * Equivalent to:
 *   Object#dup
 * @param [state*] mrb The current mruby state.
 * @param [value] obj Object to be duplicate.
 * @return [value] The newly duplicated object.
 */
MRB_API value _obj_dup(state *mrb, value obj);
MRB_API value _check_to_integer(state *mrb, value val, const char *method);

/**
 * Returns true if obj responds to the given method. If the method was defined for that
 * class it returns true, it returns false otherwise.
 *
 *      Example:
 *      # Ruby style
 *      class ExampleClass
 *        def example_method
 *        end
 *      end
 *
 *      ExampleClass.new.respond_to?(:example_method) # => true
 *
 *      // C style
 *      void
 *      _example_gem_init(state* mrb) {
 *        struct RClass *example_class;
 *        _sym mid;
 *        _bool obj_resp;
 *
 *        example_class = _define_class(mrb, "ExampleClass", mrb->object_class);
 *        _define_method(mrb, example_class, "example_method", exampleMethod, MRB_ARGS_NONE());
 *        mid = _intern_str(mrb, _str_new_lit(mrb, "example_method" ));
 *        obj_resp = _obj_respond_to(mrb, example_class, mid); // => 1(true in Ruby world)
 *
 *        // If _obj_respond_to returns 1 then puts "True"
 *        // If _obj_respond_to returns 0 then puts "False"
 *        if (obj_resp == 1) {
 *          puts("True");
 *        }
 *        else if (obj_resp == 0) {
 *          puts("False");
 *        }
 *      }
 *
 * @param [state*] mrb The current mruby state.
 * @param [struct RClass *] c A reference to a class.
 * @param [_sym] mid A symbol referencing a method id.
 * @return [_bool] A boolean value.
 */
MRB_API _bool _obj_respond_to(state *mrb, struct RClass* c, _sym mid);

/**
 * Defines a new class under a given module
 *
 * @param [state*] mrb The current mruby state.
 * @param [struct RClass *] outer Reference to the module under which the new class will be defined
 * @param [const char *] name The name of the defined class
 * @param [struct RClass *] super The new class parent
 * @return [struct RClass *] Reference to the newly defined class
 * @see _define_class
 */
MRB_API struct RClass * _define_class_under(state *mrb, struct RClass *outer, const char *name, struct RClass *super);

MRB_API struct RClass * _define_module_under(state *mrb, struct RClass *outer, const char *name);

/**
 * Function requires n arguments.
 *
 * @param n
 *      The number of required arguments.
 */
#define MRB_ARGS_REQ(n)     ((_aspec)((n)&0x1f) << 18)

/**
 * Function takes n optional arguments
 *
 * @param n
 *      The number of optional arguments.
 */
#define MRB_ARGS_OPT(n)     ((_aspec)((n)&0x1f) << 13)

/**
 * Function takes n1 mandatory arguments and n2 optional arguments
 *
 * @param n1
 *      The number of required arguments.
 * @param n2
 *      The number of optional arguments.
 */
#define MRB_ARGS_ARG(n1,n2)   (MRB_ARGS_REQ(n1)|MRB_ARGS_OPT(n2))

/** rest argument */
#define MRB_ARGS_REST()     ((_aspec)(1 << 12))

/** required arguments after rest */
#define MRB_ARGS_POST(n)    ((_aspec)((n)&0x1f) << 7)

/** keyword arguments (n of keys, kdict) */
#define MRB_ARGS_KEY(n1,n2) ((_aspec)((((n1)&0x1f) << 2) | ((n2)?(1<<1):0)))

/**
 * Function takes a block argument
 */
#define MRB_ARGS_BLOCK()    ((_aspec)1)

/**
 * Function accepts any number of arguments
 */
#define MRB_ARGS_ANY()      MRB_ARGS_REST()

/**
 * Function accepts no arguments
 */
#define MRB_ARGS_NONE()     ((_aspec)0)

/**
 * Format specifiers for {_get_args} function
 *
 * Must be a C string composed of the following format specifiers:
 *
 * | char | Ruby type      | C types           | Notes                                              |
 * |:----:|----------------|-------------------|----------------------------------------------------|
 * | `o`  | {Object}       | {value}       | Could be used to retrieve any type of argument     |
 * | `C`  | {Class}/{Module} | {value}     |                                                    |
 * | `S`  | {String}       | {value}       | when `!` follows, the value may be `nil`           |
 * | `A`  | {Array}        | {value}       | when `!` follows, the value may be `nil`           |
 * | `H`  | {Hash}         | {value}       | when `!` follows, the value may be `nil`           |
 * | `s`  | {String}       | char *, {_int} | Receive two arguments; `s!` gives (`NULL`,`0`) for `nil`       |
 * | `z`  | {String}       | char *            | `NULL` terminated string; `z!` gives `NULL` for `nil`           |
 * | `a`  | {Array}        | {value} *, {_int} | Receive two arguments; `a!` gives (`NULL`,`0`) for `nil` |
 * | `f`  | {Float}        | {_float}       |                                                    |
 * | `i`  | {Integer}      | {_int}         |                                                    |
 * | `b`  | boolean        | {_bool}        |                                                    |
 * | `n`  | {Symbol}       | {_sym}         |                                                    |
 * | `&`  | block          | {value}       | &! raises exception if no block given.             |
 * | `*`  | rest arguments | {value} *, {_int} | Receive the rest of arguments as an array; *! avoid copy of the stack.  |
 * | &vert; | optional     |                   | After this spec following specs would be optional. |
 * | `?`  | optional given | {_bool}        | `TRUE` if preceding argument is given. Used to check optional argument is given. |
 *
 * @see _get_args
 */
typedef const char *_args_format;

/**
 * Retrieve arguments from state.
 *
 * When applicable, implicit conversions (such as `to_str`, `to_ary`, `to_hash`) are
 * applied to received arguments.
 * Used inside a function of _func_t type.
 *
 * @param mrb The current MRuby state.
 * @param format [_args_format] is a list of format specifiers
 * @param ... The passing variadic arguments must be a pointer of retrieving type.
 * @return the number of arguments retrieved.
 * @see _args_format
 */
MRB_API _int _get_args(state *mrb, _args_format format, ...);

static inline _sym
_get_mid(state *mrb) /* get method symbol */
{
  return mrb->c->ci->mid;
}

/**
 * Retrieve number of arguments from state.
 *
 * Correctly handles *splat arguments.
 */
MRB_API _int _get_argc(state *mrb);

MRB_API value* _get_argv(state *mrb);

/* `strlen` for character string literals (use with caution or `strlen` instead)
    Adjacent string literals are concatenated in C/C++ in translation phase 6.
    If `lit` is not one, the compiler will report a syntax error:
     MSVC: "error C2143: syntax error : missing ')' before 'string'"
     GCC:  "error: expected ')' before string constant"
*/
#define _strlen_lit(lit) (sizeof(lit "") - 1)

/**
 * Call existing ruby functions.
 *
 *      #include <stdio.h>
 *      #include <mruby.h>
 *      #include "mruby/compile.h"
 *
 *      int
 *      main()
 *      {
 *        _int i = 99;
 *        state *mrb = _open();
 *
 *        if (!mrb) { }
 *        FILE *fp = fopen("test.rb","r");
 *        value obj = _load_file(mrb,fp);
 *        _funcall(mrb, obj, "method_name", 1, _fixnum_value(i));
 *        fclose(fp);
 *        _close(mrb);
 *       }
 * @param [state*] state* The current mruby state.
 * @param [value] value A reference to an mruby value.
 * @param [const char*] const char* The name of the method.
 * @param [_int] _int The number of arguments the method has.
 * @param [...] ... Variadic values(not type safe!).
 * @return [value] value mruby function value.
 */
MRB_API value _funcall(state*, value, const char*, _int,...);
/**
 * Call existing ruby functions. This is basically the type safe version of _funcall.
 *
 *      #include <stdio.h>
 *      #include <mruby.h>
 *      #include "mruby/compile.h"
 *      int
 *      main()
 *      {
 *        _int i = 99;
 *        state *mrb = _open();
 *
 *        if (!mrb) { }
 *        _sym m_sym = _intern_lit(mrb, "method_name"); // Symbol for method.
 *
 *        FILE *fp = fopen("test.rb","r");
 *        value obj = _load_file(mrb,fp);
 *        _funcall_argv(mrb, obj, m_sym, 1, &obj); // Calling ruby function from test.rb.
 *        fclose(fp);
 *        _close(mrb);
 *       }
 * @param [state*] state* The current mruby state.
 * @param [value] value A reference to an mruby value.
 * @param [_sym] _sym The symbol representing the method.
 * @param [_int] _int The number of arguments the method has.
 * @param [const value*] value* Pointer to the object.
 * @return [value] value mruby function value.
 * @see _funcall
 */
MRB_API value _funcall_argv(state*, value, _sym, _int, const value*);
/**
 * Call existing ruby functions with a block.
 */
MRB_API value _funcall_with_block(state*, value, _sym, _int, const value*, value);
/**
 * Create a symbol
 *
 *     # Ruby style:
 *     :pizza # => :pizza
 *
 *     // C style:
 *     _sym m_sym = _intern_lit(mrb, "pizza"); //  => :pizza
 * @param [state*] state* The current mruby state.
 * @param [const char*] const char* The name of the method.
 * @return [_sym] _sym A symbol.
 */
MRB_API _sym _intern_cstr(state*,const char*);
MRB_API _sym _intern(state*,const char*,size_t);
MRB_API _sym _intern_static(state*,const char*,size_t);
#define _intern_lit(mrb, lit) _intern_static(mrb, lit, _strlen_lit(lit))
MRB_API _sym _intern_str(state*,value);
MRB_API value _check_intern_cstr(state*,const char*);
MRB_API value _check_intern(state*,const char*,size_t);
MRB_API value _check_intern_str(state*,value);
MRB_API const char *_sym2name(state*,_sym);
MRB_API const char *_sym2name_len(state*,_sym,_int*);
MRB_API value _sym2str(state*,_sym);

MRB_API void *_malloc(state*, size_t);         /* raise RuntimeError if no mem */
MRB_API void *_calloc(state*, size_t, size_t); /* ditto */
MRB_API void *_realloc(state*, void*, size_t); /* ditto */
MRB_API void *_realloc_simple(state*, void*, size_t); /* return NULL if no memory available */
MRB_API void *_malloc_simple(state*, size_t);  /* return NULL if no memory available */
MRB_API struct RBasic *_obj_alloc(state*, enum _vtype, struct RClass*);
MRB_API void _free(state*, void*);

MRB_API value _str_new(state *mrb, const char *p, size_t len);

/**
 * Turns a C string into a Ruby string value.
 */
MRB_API value _str_new_cstr(state*, const char*);
MRB_API value _str_new_static(state *mrb, const char *p, size_t len);
#define _str_new_lit(mrb, lit) _str_new_static(mrb, (lit), _strlen_lit(lit))

#ifdef _WIN32
MRB_API char* _utf8_from_locale(const char *p, int len);
MRB_API char* _locale_from_utf8(const char *p, int len);
#define _locale_free(p) free(p)
#define _utf8_free(p) free(p)
#else
#define _utf8_from_locale(p, l) ((char*)p)
#define _locale_from_utf8(p, l) ((char*)p)
#define _locale_free(p)
#define _utf8_free(p)
#endif

/**
 * Creates new state.
 *
 * @return
 *      Pointer to the newly created state.
 */
MRB_API state* _open(void);

/**
 * Create new state with custom allocators.
 *
 * @param f
 *      Reference to the allocation function.
 * @param ud
 *      User data will be passed to custom allocator f.
 *      If user data isn't required just pass NULL.
 * @return
 *      Pointer to the newly created state.
 */
MRB_API state* _open_allocf(_allocf f, void *ud);

/**
 * Create new state with just the MRuby core
 *
 * @param f
 *      Reference to the allocation function.
 *      Use _default_allocf for the default
 * @param ud
 *      User data will be passed to custom allocator f.
 *      If user data isn't required just pass NULL.
 * @return
 *      Pointer to the newly created state.
 */
MRB_API state* _open_core(_allocf f, void *ud);

/**
 * Closes and frees a state.
 *
 * @param mrb
 *      Pointer to the state to be closed.
 */
MRB_API void _close(state *mrb);

/**
 * The default allocation function.
 *
 * @see _allocf
 */
MRB_API void* _default_allocf(state*, void*, size_t, void*);

MRB_API value _top_self(state *);
MRB_API value _run(state*, struct RProc*, value);
MRB_API value _top_run(state*, struct RProc*, value, unsigned int);
MRB_API value _vm_run(state*, struct RProc*, value, unsigned int);
MRB_API value _vm_exec(state*, struct RProc*, _code*);
/* compatibility macros */
#define _toplevel_run_keep(m,p,k) _top_run((m),(p),_top_self(m),(k))
#define _toplevel_run(m,p) _toplevel_run_keep((m),(p),0)
#define _context_run(m,p,s,k) _vm_run((m),(p),(s),(k))

MRB_API void _p(state*, value);
MRB_API _int _obj_id(value obj);
MRB_API _sym _obj_to_sym(state *mrb, value name);

MRB_API _bool _obj_eq(state*, value, value);
MRB_API _bool _obj_equal(state*, value, value);
MRB_API _bool _equal(state *mrb, value obj1, value obj2);
MRB_API value _convert_to_integer(state *mrb, value val, _int base);
MRB_API value _Integer(state *mrb, value val);
#ifndef MRB_WITHOUT_FLOAT
MRB_API value _Float(state *mrb, value val);
#endif
MRB_API value _inspect(state *mrb, value obj);
MRB_API _bool _eql(state *mrb, value obj1, value obj2);

static inline int _gc_arena_save(state*);
static inline void _gc_arena_restore(state*,int);

static inline int
_gc_arena_save(state *mrb)
{
  return mrb->gc.arena_idx;
}

static inline void
_gc_arena_restore(state *mrb, int idx)
{
  mrb->gc.arena_idx = idx;
}

MRB_API void _garbage_collect(state*);
MRB_API void _full_gc(state*);
MRB_API void _incremental_gc(state *);
MRB_API void _gc_mark(state*,struct RBasic*);
#define _gc_mark_value(mrb,val) do {\
  if (!_immediate_p(val)) _gc_mark((mrb), _basic_ptr(val)); \
} while (0)
MRB_API void _field_write_barrier(state *, struct RBasic*, struct RBasic*);
#define _field_write_barrier_value(mrb, obj, val) do{\
  if (!_immediate_p(val)) _field_write_barrier((mrb), (obj), _basic_ptr(val)); \
} while (0)
MRB_API void _write_barrier(state *, struct RBasic*);

MRB_API value _check_convert_type(state *mrb, value val, enum _vtype type, const char *tname, const char *method);
MRB_API value _any_to_s(state *mrb, value obj);
MRB_API const char * _obj_classname(state *mrb, value obj);
MRB_API struct RClass* _obj_class(state *mrb, value obj);
MRB_API value _class_path(state *mrb, struct RClass *c);
MRB_API value _convert_type(state *mrb, value val, enum _vtype type, const char *tname, const char *method);
MRB_API _bool _obj_is_kind_of(state *mrb, value obj, struct RClass *c);
MRB_API value _obj_inspect(state *mrb, value self);
MRB_API value _obj_clone(state *mrb, value self);

#ifndef ISPRINT
#define ISASCII(c) ((unsigned)(c) <= 0x7f)
#define ISPRINT(c) (((unsigned)(c) - 0x20) < 0x5f)
#define ISSPACE(c) ((c) == ' ' || (unsigned)(c) - '\t' < 5)
#define ISUPPER(c) (((unsigned)(c) - 'A') < 26)
#define ISLOWER(c) (((unsigned)(c) - 'a') < 26)
#define ISALPHA(c) ((((unsigned)(c) | 0x20) - 'a') < 26)
#define ISDIGIT(c) (((unsigned)(c) - '0') < 10)
#define ISXDIGIT(c) (ISDIGIT(c) || ((unsigned)(c) | 0x20) - 'a' < 6)
#define ISALNUM(c) (ISALPHA(c) || ISDIGIT(c))
#define ISBLANK(c) ((c) == ' ' || (c) == '\t')
#define ISCNTRL(c) ((unsigned)(c) < 0x20 || (c) == 0x7f)
#define TOUPPER(c) (ISLOWER(c) ? ((c) & 0x5f) : (c))
#define TOLOWER(c) (ISUPPER(c) ? ((c) | 0x20) : (c))
#endif

MRB_API value _exc_new(state *mrb, struct RClass *c, const char *ptr, size_t len);
MRB_API _noreturn void _exc_raise(state *mrb, value exc);

MRB_API _noreturn void _raise(state *mrb, struct RClass *c, const char *msg);
MRB_API _noreturn void _raisef(state *mrb, struct RClass *c, const char *fmt, ...);
MRB_API _noreturn void _name_error(state *mrb, _sym id, const char *fmt, ...);
MRB_API void _warn(state *mrb, const char *fmt, ...);
MRB_API _noreturn void _bug(state *mrb, const char *fmt, ...);
MRB_API void _print_backtrace(state *mrb);
MRB_API void _print_error(state *mrb);

/* macros to get typical exception objects
   note:
   + those E_* macros requires state* variable named mrb.
   + exception objects obtained from those macros are local to mrb
*/
#define E_RUNTIME_ERROR             (_exc_get(mrb, "RuntimeError"))
#define E_TYPE_ERROR                (_exc_get(mrb, "TypeError"))
#define E_ARGUMENT_ERROR            (_exc_get(mrb, "ArgumentError"))
#define E_INDEX_ERROR               (_exc_get(mrb, "IndexError"))
#define E_RANGE_ERROR               (_exc_get(mrb, "RangeError"))
#define E_NAME_ERROR                (_exc_get(mrb, "NameError"))
#define E_NOMETHOD_ERROR            (_exc_get(mrb, "NoMethodError"))
#define E_SCRIPT_ERROR              (_exc_get(mrb, "ScriptError"))
#define E_SYNTAX_ERROR              (_exc_get(mrb, "SyntaxError"))
#define E_LOCALJUMP_ERROR           (_exc_get(mrb, "LocalJumpError"))
#define E_REGEXP_ERROR              (_exc_get(mrb, "RegexpError"))
#define E_FROZEN_ERROR              (_exc_get(mrb, "FrozenError"))

#define E_NOTIMP_ERROR              (_exc_get(mrb, "NotImplementedError"))
#ifndef MRB_WITHOUT_FLOAT
#define E_FLOATDOMAIN_ERROR         (_exc_get(mrb, "FloatDomainError"))
#endif

#define E_KEY_ERROR                 (_exc_get(mrb, "KeyError"))

MRB_API value _yield(state *mrb, value b, value arg);
MRB_API value _yield_argv(state *mrb, value b, _int argc, const value *argv);
MRB_API value _yield_with_class(state *mrb, value b, _int argc, const value *argv, value self, struct RClass *c);

/* continue execution to the proc */
/* this function should always be called as the last function of a method */
/* e.g. return _yield_cont(mrb, proc, self, argc, argv); */
value _yield_cont(state *mrb, value b, value self, _int argc, const value *argv);

/* _gc_protect() leaves the object in the arena */
MRB_API void _gc_protect(state *mrb, value obj);
/* _gc_register() keeps the object from GC. */
MRB_API void _gc_register(state *mrb, value obj);
/* _gc_unregister() removes the object from GC root. */
MRB_API void _gc_unregister(state *mrb, value obj);

MRB_API value _to_int(state *mrb, value val);
#define _int(mrb, val) _fixnum(_to_int(mrb, val))
MRB_API void _check_type(state *mrb, value x, enum _vtype t);

typedef enum call_type {
  CALL_PUBLIC,
  CALL_FCALL,
  CALL_VCALL,
  CALL_TYPE_MAX
} call_type;

MRB_API void _define_alias(state *mrb, struct RClass *c, const char *a, const char *b);
MRB_API const char *_class_name(state *mrb, struct RClass* klass);
MRB_API void _define_global_const(state *mrb, const char *name, value val);

MRB_API value _attr_get(state *mrb, value obj, _sym id);

MRB_API _bool _respond_to(state *mrb, value obj, _sym mid);
MRB_API _bool _obj_is_instance_of(state *mrb, value obj, struct RClass* c);
MRB_API _bool _func_basic_p(state *mrb, value obj, _sym mid, _func_t func);


/*
 * Resume a Fiber
 *
 * @mrbgem mruby-fiber
 */
MRB_API value _fiber_resume(state *mrb, value fib, _int argc, const value *argv);

/*
 * Yield a Fiber
 *
 * @mrbgem mruby-fiber
 */
MRB_API value _fiber_yield(state *mrb, _int argc, const value *argv);

/*
 * Check if a Fiber is alive
 *
 * @mrbgem mruby-fiber
 */
MRB_API value _fiber_alive_p(state *mrb, value fib);

/*
 * FiberError reference
 *
 * @mrbgem mruby-fiber
 */
#define E_FIBER_ERROR (_exc_get(mrb, "FiberError"))
MRB_API void _stack_extend(state*, _int);

/* memory pool implementation */
typedef struct _pool _pool;
MRB_API struct _pool* _pool_open(state*);
MRB_API void _pool_close(struct _pool*);
MRB_API void* _pool_alloc(struct _pool*, size_t);
MRB_API void* _pool_realloc(struct _pool*, void*, size_t oldlen, size_t newlen);
MRB_API _bool _pool_can_realloc(struct _pool*, void*, size_t);
MRB_API void* _alloca(state *mrb, size_t);

MRB_API void _state_atexit(state *mrb, _atexit_func func);

MRB_API void _show_version(state *mrb);
MRB_API void _show_copyright(state *mrb);

MRB_API value _format(state *mrb, const char *format, ...);

#if 0
/* memcpy and memset does not work with gdb reverse-next on my box */
/* use naive memcpy and memset instead */
#undef memcpy
#undef memset
static void*
mrbmemcpy(void *dst, const void *src, size_t n)
{
  char *d = (char*)dst;
  const char *s = (const char*)src;
  while (n--)
    *d++ = *s++;
  return d;
}
#define memcpy(a,b,c) mrbmemcpy(a,b,c)

static void*
mrbmemset(void *s, int c, size_t n)
{
  char *t = (char*)s;
  while (n--)
    *t++ = c;
  return s;
}
#define memset(a,b,c) mrbmemset(a,b,c)
#endif

MRB_END_DECL

#endif  /* MRUBY_H */

//int main();