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

#ifdef $DEBUG
#include <assert.h>
#define $assert(p) assert(p)
#define $assert_int_fit(t1,n,t2,max) assert((n)>=0 && ((sizeof(n)<=sizeof(t2))||(n<=(t1)(max))))
#else
#define $assert(p) ((void)0)
#define $assert_int_fit(t1,n,t2,max) ((void)0)
#endif

#if __STDC_VERSION__ >= 201112L
#define $static_assert(exp, str) _Static_assert(exp, str)
#else
#define $static_assert(exp, str) $assert(exp)
#endif

#include "mrbconf.h"

#ifndef $WITHOUT_FLOAT
#ifndef FLT_EPSILON
#define FLT_EPSILON (1.19209290e-07f)
#endif
#ifndef DBL_EPSILON
#define DBL_EPSILON ((double)2.22044604925031308085e-16L)
#endif
#ifndef LDBL_EPSILON
#define LDBL_EPSILON (1.08420217248550443401e-19L)
#endif

#ifdef $USE_FLOAT
#define $FLOAT_EPSILON FLT_EPSILON
#else
#define $FLOAT_EPSILON DBL_EPSILON
#endif
#endif

#include "mruby/common.h"
#include <mruby/value.h>
#include <mruby/gc.h>
#include <mruby/version.h>

/**
 * MRuby C API entry point
 */
$BEGIN_DECL

typedef uint8_t $code;

/**
 * Required arguments signature type.
 */
typedef uint32_t $aspec;


struct $irep;
struct $state;

/**
 * Function pointer type of custom allocator used in @see $open_allocf.
 *
 * The function pointing it must behave similarly as realloc except:
 * - If ptr is NULL it must allocate new space.
 * - If s is NULL, ptr must be freed.
 *
 * See @see $default_allocf for the default implementation.
 */
typedef void* (*$allocf) (struct $state *mrb, void*, size_t, void *ud);

#ifndef $FIXED_STATE_ATEXIT_STACK_SIZE
#define $FIXED_STATE_ATEXIT_STACK_SIZE 5
#endif

typedef struct {
  $sym mid;
  struct RProc *proc;
  $value *stackent;
  uint16_t ridx;
  uint16_t epos;
  struct REnv *env;
  $code *pc;                 /* return address */
  $code *err;                /* error position */
  int argc;
  int acc;
  struct RClass *target_class;
} $callinfo;

enum $fiber_state {
  $FIBER_CREATED = 0,
  $FIBER_RUNNING,
  $FIBER_RESUMED,
  $FIBER_SUSPENDED,
  $FIBER_TRANSFERRED,
  $FIBER_TERMINATED,
};

struct $context {
  struct $context *prev;

  $value *stack;                       /* stack of virtual machine */
  $value *stbase, *stend;

  $callinfo *ci;
  $callinfo *cibase, *ciend;

  uint16_t *rescue;                       /* exception handler stack */
  uint16_t rsize;
  struct RProc **ensure;                  /* ensure handler stack */
  uint16_t esize, eidx;

  enum $fiber_state status;
  $bool vmexec;
  struct RFiber *fib;
};

#ifdef $METHOD_CACHE_SIZE
# define $METHOD_CACHE
#else
/* default method cache size: 128 */
/* cache size needs to be power of 2 */
# define $METHOD_CACHE_SIZE (1<<7)
#endif

typedef $value (*$func_t)(struct $state *mrb, $value);

#ifdef $METHOD_TABLE_INLINE
typedef uintptr_t $method_t;
#else
typedef struct {
  $bool func_p;
  union {
    struct RProc *proc;
    $func_t func;
  };
} $method_t;
#endif

#ifdef $METHOD_CACHE
struct $cache_entry {
  struct RClass *c, *c0;
  $sym mid;
  $method_t m;
};
#endif

struct $jmpbuf;

typedef void (*$atexit_func)(struct $state*);

#define $STATE_NO_REGEXP 1
#define $STATE_REGEXP    2

typedef struct $state {
  struct $jmpbuf *jmp;

  uint32_t flags;
  $allocf allocf;                      /* memory allocation function */
  void *allocf_ud;                        /* auxiliary data of allocf */

  struct $context *c;
  struct $context *root_c;
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

#ifndef $WITHOUT_FLOAT
  struct RClass *float_class;
#endif
  struct RClass *fixnum_class;
  struct RClass *true_class;
  struct RClass *false_class;
  struct RClass *nil_class;
  struct RClass *symbol_class;
  struct RClass *kernel_module;

  struct alloca_header *mems;
  $gc gc;

#ifdef $METHOD_CACHE
  struct $cache_entry cache[$METHOD_CACHE_SIZE];
#endif

  $sym symidx;
  struct kh_n2s *name2sym;      /* symbol hash */
  struct symbol_name *symtbl;   /* symbol table */
  size_t symcapa;

#ifdef $ENABLE_DEBUG_HOOK
  void (*code_fetch_hook)(struct $state* mrb, struct $irep *irep, $code *pc, $value *regs);
  void (*debug_op_hook)(struct $state* mrb, struct $irep *irep, $code *pc, $value *regs);
#endif

#ifdef $BYTECODE_DECODE_OPTION
  $code (*bytecode_decoder)(struct $state* mrb, $code code);
#endif

  struct RClass *eException_class;
  struct RClass *eStandardError_class;
  struct RObject *nomem_err;              /* pre-allocated NoMemoryError */
  struct RObject *stack_err;              /* pre-allocated SysStackError */
#ifdef $GC_FIXED_ARENA
  struct RObject *arena_err;              /* pre-allocated arena overfow error */
#endif

  void *ud; /* auxiliary data */

#ifdef $FIXED_STATE_ATEXIT_STACK
  $atexit_func atexit_stack[$FIXED_STATE_ATEXIT_STACK_SIZE];
#else
  $atexit_func *atexit_stack;
#endif
  $int atexit_stack_len;
} $state;

/**
 * Defines a new class.
 *
 * If you're creating a gem it may look something like this:
 *
 *      !!!c
 *      void $example_gem_init($state* mrb) {
 *          struct RClass *example_class;
 *          example_class = $define_class(mrb, "Example_Class", mrb->object_class);
 *      }
 *
 *      void $example_gem_final($state* mrb) {
 *          //free(TheAnimals);
 *      }
 *
 * @param [$state *] mrb The current mruby state.
 * @param [const char *] name The name of the defined class.
 * @param [struct RClass *] super The new class parent.
 * @return [struct RClass *] Reference to the newly defined class.
 * @see $define_class_under
 */
$API struct RClass *$define_class($state *mrb, const char *name, struct RClass *super);

/**
 * Defines a new module.
 *
 * @param [$state *] $state* The current mruby state.
 * @param [const char *] char* The name of the module.
 * @return [struct RClass *] Reference to the newly defined module.
 */
$API struct RClass *$define_module($state *, const char*);
$API $value $singleton_class($state*, $value);

/**
 * Include a module in another class or module.
 * Equivalent to:
 *
 *   module B
 *     include A
 *   end
 * @param [$state *] $state* The current mruby state.
 * @param [struct RClass *] RClass* A reference to module or a class.
 * @param [struct RClass *] RClass* A reference to the module to be included.
 */
$API void $include_module($state*, struct RClass*, struct RClass*);

/**
 * Prepends a module in another class or module.
 *
 * Equivalent to:
 *  module B
 *    prepend A
 *  end
 * @param [$state *] $state* The current mruby state.
 * @param [struct RClass *] RClass* A reference to module or a class.
 * @param [struct RClass *] RClass* A reference to the module to be prepended.
 */
$API void $prepend_module($state*, struct RClass*, struct RClass*);

/**
 * Defines a global function in ruby.
 *
 * If you're creating a gem it may look something like this
 *
 * Example:
 *
 *     !!!c
 *     $value example_method($state* mrb, $value self)
 *     {
 *          puts("Executing example command!");
 *          return self;
 *     }
 *
 *     void $example_gem_init($state* mrb)
 *     {
 *           $define_method(mrb, mrb->kernel_module, "example_method", example_method, $ARGS_NONE());
 *     }
 *
 * @param [$state *] mrb The MRuby state reference.
 * @param [struct RClass *] cla The class pointer where the method will be defined.
 * @param [const char *] name The name of the method being defined.
 * @param [$func_t] func The function pointer to the method definition.
 * @param [$aspec] aspec The method parameters declaration.
 */
$API void $define_method($state *mrb, struct RClass *cla, const char *name, $func_t func, $aspec aspec);

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
 *     $value bar_method($state* mrb, $value self){
 *       return $nil_value();
 *     }
 *     void $example_gem_init($state* mrb){
 *       struct RClass *foo;
 *       foo = $define_class(mrb, "Foo", mrb->object_class);
 *       $define_class_method(mrb, foo, "bar", bar_method, $ARGS_NONE());
 *     }
 * @param [$state *] $state* The MRuby state reference.
 * @param [struct RClass *] RClass* The class where the class method will be defined.
 * @param [const char *] char* The name of the class method being defined.
 * @param [$func_t] $func_t The function pointer to the class method definition.
 * @param [$aspec] $aspec The method parameters declaration.
 */
$API void $define_class_method($state *, struct RClass *, const char *, $func_t, $aspec);
$API void $define_singleton_method($state*, struct RObject*, const char*, $func_t, $aspec);

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
 *        $value bar_method($state* mrb, $value self){
 *          return $nil_value();
 *        }
 *        void $example_gem_init($state* mrb){
 *          struct RClass *foo;
 *          foo = $define_module(mrb, "Foo");
 *          $define_module_function(mrb, foo, "bar", bar_method, $ARGS_NONE());
 *        }
 *  @param [$state *] $state* The MRuby state reference.
 *  @param [struct RClass *] RClass* The module where the module function will be defined.
 *  @param [const char *] char* The name of the module function being defined.
 *  @param [$func_t] $func_t The function pointer to the module function definition.
 *  @param [$aspec] $aspec The method parameters declaration.
 */
$API void $define_module_function($state*, struct RClass*, const char*, $func_t, $aspec);

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
 *          $example_gem_init($state* mrb){
 *            $define_const(mrb, mrb->kernel_module, "AGE", $fixnum_value(22));
 *          }
 *
 *          $value
 *          $example_gem_final($state* mrb){
 *          }
 *  @param [$state *] $state* The MRuby state reference.
 *  @param [struct RClass *] RClass* A class or module the constant is defined in.
 *  @param [const char *] name The name of the constant being defined.
 *  @param [$value] $value The value for the constant.
 */
$API void $define_const($state*, struct RClass*, const char *name, $value);

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
 *     $value
 *     $example_method($state *mrb){
 *       return $str_new_lit(mrb, "example");
 *     }
 *
 *     void
 *     $example_gem_init($state* mrb){
 *       struct RClass *example_class_a;
 *       struct RClass *example_class_b;
 *       struct RClass *example_class_c;
 *
 *       example_class_a = $define_class(mrb, "ExampleClassA", mrb->object_class);
 *       $define_method(mrb, example_class_a, "example_method", $example_method, $ARGS_NONE());
 *       example_class_b = $define_class(mrb, "ExampleClassB", example_class_a);
 *       example_class_c = $define_class(mrb, "ExampleClassC", example_class_b);
 *       $undef_method(mrb, example_class_c, "example_method");
 *     }
 *
 *     $example_gem_final($state* mrb){
 *     }
 * @param [$state*] $state* The mruby state reference.
 * @param [struct RClass*] RClass* A class the method will be undefined from.
 * @param [const char] const char* The name of the method to be undefined.
 */
$API void $undef_method($state*, struct RClass*, const char*);
$API void $undef_method_id($state*, struct RClass*, $sym);

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
 *     $value
 *     $example_method($state *mrb){
 *       return $str_new_lit(mrb, "example");
 *     }
 *
 *     void
 *     $example_gem_init($state* mrb){
 *       struct RClass *example_class;
 *       example_class = $define_class(mrb, "ExampleClass", mrb->object_class);
 *       $define_class_method(mrb, example_class, "example_method", $example_method, $ARGS_NONE());
 *       $undef_class_method(mrb, example_class, "example_method");
 *      }
 *
 *      void
 *      $example_gem_final($state* mrb){
 *      }
 * @param [$state*] $state* The mruby state reference.
 * @param [RClass*] RClass* A class the class method will be undefined from.
 * @param [constchar*] constchar* The name of the class method to be undefined.
 */
$API void $undef_class_method($state*, struct RClass*, const char*);

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
 *     $example_gem_init($state* mrb) {
 *       struct RClass *example_class;
 *       $value obj;
 *       example_class = $define_class(mrb, "ExampleClass", mrb->object_class); # => class ExampleClass; end
 *       obj = $obj_new(mrb, example_class, 0, NULL); # => ExampleClass.new
 *       $p(mrb, obj); // => Kernel#p
 *      }
 * @param [$state*] mrb The current mruby state.
 * @param [RClass*] c Reference to the class of the new object.
 * @param [$int] argc Number of arguments in argv
 * @param [const $value *] argv Array of $value to initialize the object
 * @return [$value] The newly initialized object
 */
$API $value $obj_new($state *mrb, struct RClass *c, $int argc, const $value *argv);

/** @see $obj_new */
$INLINE $value $class_new_instance($state *mrb, $int argc, const $value *argv, struct RClass *c)
{
  return $obj_new(mrb,c,argc,argv);
}

$API $value $instance_new($state *mrb, $value cv);

/**
 * Creates a new instance of Class, Class.
 *
 * Example:
 *
 *      void
 *      $example_gem_init($state* mrb) {
 *        struct RClass *example_class;
 *
 *        $value obj;
 *        example_class = $class_new(mrb, mrb->object_class);
 *        obj = $obj_new(mrb, example_class, 0, NULL); // => #<#<Class:0x9a945b8>:0x9a94588>
 *        $p(mrb, obj); // => Kernel#p
 *       }
 *
 * @param [$state*] mrb The current mruby state.
 * @param [struct RClass *] super The super class or parent.
 * @return [struct RClass *] Reference to the new class.
 */
$API struct RClass * $class_new($state *mrb, struct RClass *super);

/**
 * Creates a new module, Module.
 *
 * Example:
 *      void
 *      $example_gem_init($state* mrb) {
 *        struct RClass *example_module;
 *
 *        example_module = $module_new(mrb);
 *      }
 *
 * @param [$state*] mrb The current mruby state.
 * @return [struct RClass *] Reference to the new module.
 */
$API struct RClass * $module_new($state *mrb);

/**
 * Returns an $bool. True if class was defined, and false if the class was not defined.
 *
 * Example:
 *     void
 *     $example_gem_init($state* mrb) {
 *       struct RClass *example_class;
 *       $bool cd;
 *
 *       example_class = $define_class(mrb, "ExampleClass", mrb->object_class);
 *       cd = $class_defined(mrb, "ExampleClass");
 *
 *       // If $class_defined returns 1 then puts "True"
 *       // If $class_defined returns 0 then puts "False"
 *       if (cd == 1){
 *         puts("True");
 *       }
 *       else {
 *         puts("False");
 *       }
 *      }
 *
 * @param [$state*] mrb The current mruby state.
 * @param [const char *] name A string representing the name of the class.
 * @return [$bool] A boolean value.
 */
$API $bool $class_defined($state *mrb, const char *name);

/**
 * Gets a class.
 * @param [$state*] mrb The current mruby state.
 * @param [const char *] name The name of the class.
 * @return [struct RClass *] A reference to the class.
*/
$API struct RClass * $class_get($state *mrb, const char *name);

/**
 * Gets a exception class.
 * @param [$state*] mrb The current mruby state.
 * @param [const char *] name The name of the class.
 * @return [struct RClass *] A reference to the class.
*/
$API struct RClass * $exc_get($state *mrb, const char *name);

/**
 * Returns an $bool. True if inner class was defined, and false if the inner class was not defined.
 *
 * Example:
 *     void
 *     $example_gem_init($state* mrb) {
 *       struct RClass *example_outer, *example_inner;
 *       $bool cd;
 *
 *       example_outer = $define_module(mrb, "ExampleOuter");
 *
 *       example_inner = $define_class_under(mrb, example_outer, "ExampleInner", mrb->object_class);
 *       cd = $class_defined_under(mrb, example_outer, "ExampleInner");
 *
 *       // If $class_defined_under returns 1 then puts "True"
 *       // If $class_defined_under returns 0 then puts "False"
 *       if (cd == 1){
 *         puts("True");
 *       }
 *       else {
 *         puts("False");
 *       }
 *      }
 *
 * @param [$state*] mrb The current mruby state.
 * @param [struct RClass *] outer The name of the outer class.
 * @param [const char *] name A string representing the name of the inner class.
 * @return [$bool] A boolean value.
 */
$API $bool $class_defined_under($state *mrb, struct RClass *outer, const char *name);

/**
 * Gets a child class.
 * @param [$state*] mrb The current mruby state.
 * @param [struct RClass *] outer The name of the parent class.
 * @param [const char *] name The name of the class.
 * @return [struct RClass *] A reference to the class.
*/
$API struct RClass * $class_get_under($state *mrb, struct RClass *outer, const char *name);

/**
 * Gets a module.
 * @param [$state*] mrb The current mruby state.
 * @param [const char *] name The name of the module.
 * @return [struct RClass *] A reference to the module.
*/
$API struct RClass * $module_get($state *mrb, const char *name);

/**
 * Gets a module defined under another module.
 * @param [$state*] mrb The current mruby state.
 * @param [struct RClass *] outer The name of the outer module.
 * @param [const char *] name The name of the module.
 * @return [struct RClass *] A reference to the module.
*/
$API struct RClass * $module_get_under($state *mrb, struct RClass *outer, const char *name);
$API $value $notimplement_m($state*, $value);

/**
 * Duplicate an object.
 *
 * Equivalent to:
 *   Object#dup
 * @param [$state*] mrb The current mruby state.
 * @param [$value] obj Object to be duplicate.
 * @return [$value] The newly duplicated object.
 */
$API $value $obj_dup($state *mrb, $value obj);
$API $value $check_to_integer($state *mrb, $value val, const char *method);

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
 *      $example_gem_init($state* mrb) {
 *        struct RClass *example_class;
 *        $sym mid;
 *        $bool obj_resp;
 *
 *        example_class = $define_class(mrb, "ExampleClass", mrb->object_class);
 *        $define_method(mrb, example_class, "example_method", exampleMethod, $ARGS_NONE());
 *        mid = $intern_str(mrb, $str_new_lit(mrb, "example_method" ));
 *        obj_resp = $obj_respond_to(mrb, example_class, mid); // => 1(true in Ruby world)
 *
 *        // If $obj_respond_to returns 1 then puts "True"
 *        // If $obj_respond_to returns 0 then puts "False"
 *        if (obj_resp == 1) {
 *          puts("True");
 *        }
 *        else if (obj_resp == 0) {
 *          puts("False");
 *        }
 *      }
 *
 * @param [$state*] mrb The current mruby state.
 * @param [struct RClass *] c A reference to a class.
 * @param [$sym] mid A symbol referencing a method id.
 * @return [$bool] A boolean value.
 */
$API $bool $obj_respond_to($state *mrb, struct RClass* c, $sym mid);

/**
 * Defines a new class under a given module
 *
 * @param [$state*] mrb The current mruby state.
 * @param [struct RClass *] outer Reference to the module under which the new class will be defined
 * @param [const char *] name The name of the defined class
 * @param [struct RClass *] super The new class parent
 * @return [struct RClass *] Reference to the newly defined class
 * @see $define_class
 */
$API struct RClass * $define_class_under($state *mrb, struct RClass *outer, const char *name, struct RClass *super);

$API struct RClass * $define_module_under($state *mrb, struct RClass *outer, const char *name);

/**
 * Function requires n arguments.
 *
 * @param n
 *      The number of required arguments.
 */
#define $ARGS_REQ(n)     (($aspec)((n)&0x1f) << 18)

/**
 * Function takes n optional arguments
 *
 * @param n
 *      The number of optional arguments.
 */
#define $ARGS_OPT(n)     (($aspec)((n)&0x1f) << 13)

/**
 * Function takes n1 mandatory arguments and n2 optional arguments
 *
 * @param n1
 *      The number of required arguments.
 * @param n2
 *      The number of optional arguments.
 */
#define $ARGS_ARG(n1,n2)   ($ARGS_REQ(n1)|$ARGS_OPT(n2))

/** rest argument */
#define $ARGS_REST()     (($aspec)(1 << 12))

/** required arguments after rest */
#define $ARGS_POST(n)    (($aspec)((n)&0x1f) << 7)

/** keyword arguments (n of keys, kdict) */
#define $ARGS_KEY(n1,n2) (($aspec)((((n1)&0x1f) << 2) | ((n2)?(1<<1):0)))

/**
 * Function takes a block argument
 */
#define $ARGS_BLOCK()    (($aspec)1)

/**
 * Function accepts any number of arguments
 */
#define $ARGS_ANY()      $ARGS_REST()

/**
 * Function accepts no arguments
 */
#define $ARGS_NONE()     (($aspec)0)

/**
 * Format specifiers for {$get_args} function
 *
 * Must be a C string composed of the following format specifiers:
 *
 * | char | Ruby type      | C types           | Notes                                              |
 * |:----:|----------------|-------------------|----------------------------------------------------|
 * | `o`  | {Object}       | {$value}       | Could be used to retrieve any type of argument     |
 * | `C`  | {Class}/{Module} | {$value}     |                                                    |
 * | `S`  | {String}       | {$value}       | when `!` follows, the value may be `nil`           |
 * | `A`  | {Array}        | {$value}       | when `!` follows, the value may be `nil`           |
 * | `H`  | {Hash}         | {$value}       | when `!` follows, the value may be `nil`           |
 * | `s`  | {String}       | char *, {$int} | Receive two arguments; `s!` gives (`NULL`,`0`) for `nil`       |
 * | `z`  | {String}       | char *            | `NULL` terminated string; `z!` gives `NULL` for `nil`           |
 * | `a`  | {Array}        | {$value} *, {$int} | Receive two arguments; `a!` gives (`NULL`,`0`) for `nil` |
 * | `f`  | {Float}        | {$float}       |                                                    |
 * | `i`  | {Integer}      | {$int}         |                                                    |
 * | `b`  | boolean        | {$bool}        |                                                    |
 * | `n`  | {Symbol}       | {$sym}         |                                                    |
 * | `&`  | block          | {$value}       | &! raises exception if no block given.             |
 * | `*`  | rest arguments | {$value} *, {$int} | Receive the rest of arguments as an array; *! avoid copy of the stack.  |
 * | &vert; | optional     |                   | After this spec following specs would be optional. |
 * | `?`  | optional given | {$bool}        | `TRUE` if preceding argument is given. Used to check optional argument is given. |
 *
 * @see $get_args
 */
typedef const char *$args_format;

/**
 * Retrieve arguments from $state.
 *
 * When applicable, implicit conversions (such as `to_str`, `to_ary`, `to_hash`) are
 * applied to received arguments.
 * Used inside a function of $func_t type.
 *
 * @param mrb The current MRuby state.
 * @param format [$args_format] is a list of format specifiers
 * @param ... The passing variadic arguments must be a pointer of retrieving type.
 * @return the number of arguments retrieved.
 * @see $args_format
 */
$API $int $get_args($state *mrb, $args_format format, ...);

static inline $sym
$get_mid($state *mrb) /* get method symbol */
{
  return mrb->c->ci->mid;
}

/**
 * Retrieve number of arguments from $state.
 *
 * Correctly handles *splat arguments.
 */
$API $int $get_argc($state *mrb);

$API $value* $get_argv($state *mrb);

/* `strlen` for character string literals (use with caution or `strlen` instead)
    Adjacent string literals are concatenated in C/C++ in translation phase 6.
    If `lit` is not one, the compiler will report a syntax error:
     MSVC: "error C2143: syntax error : missing ')' before 'string'"
     GCC:  "error: expected ')' before string constant"
*/
#define $strlen_lit(lit) (sizeof(lit "") - 1)

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
 *        $int i = 99;
 *        $state *mrb = $open();
 *
 *        if (!mrb) { }
 *        FILE *fp = fopen("test.rb","r");
 *        $value obj = $load_file(mrb,fp);
 *        $funcall(mrb, obj, "method_name", 1, $fixnum_value(i));
 *        fclose(fp);
 *        $close(mrb);
 *       }
 * @param [$state*] $state* The current mruby state.
 * @param [$value] $value A reference to an mruby value.
 * @param [const char*] const char* The name of the method.
 * @param [$int] $int The number of arguments the method has.
 * @param [...] ... Variadic values(not type safe!).
 * @return [$value] $value mruby function value.
 */
$API $value $funcall($state*, $value, const char*, $int,...);
/**
 * Call existing ruby functions. This is basically the type safe version of $funcall.
 *
 *      #include <stdio.h>
 *      #include <mruby.h>
 *      #include "mruby/compile.h"
 *      int
 *      main()
 *      {
 *        $int i = 99;
 *        $state *mrb = $open();
 *
 *        if (!mrb) { }
 *        $sym m_sym = $intern_lit(mrb, "method_name"); // Symbol for method.
 *
 *        FILE *fp = fopen("test.rb","r");
 *        $value obj = $load_file(mrb,fp);
 *        $funcall_argv(mrb, obj, m_sym, 1, &obj); // Calling ruby function from test.rb.
 *        fclose(fp);
 *        $close(mrb);
 *       }
 * @param [$state*] $state* The current mruby state.
 * @param [$value] $value A reference to an mruby value.
 * @param [$sym] $sym The symbol representing the method.
 * @param [$int] $int The number of arguments the method has.
 * @param [const $value*] $value* Pointer to the object.
 * @return [$value] $value mruby function value.
 * @see $funcall
 */
$API $value $funcall_argv($state*, $value, $sym, $int, const $value*);
/**
 * Call existing ruby functions with a block.
 */
$API $value $funcall_with_block($state*, $value, $sym, $int, const $value*, $value);
/**
 * Create a symbol
 *
 *     # Ruby style:
 *     :pizza # => :pizza
 *
 *     // C style:
 *     $sym m_sym = $intern_lit(mrb, "pizza"); //  => :pizza
 * @param [$state*] $state* The current mruby state.
 * @param [const char*] const char* The name of the method.
 * @return [$sym] $sym A symbol.
 */
$API $sym $intern_cstr($state*,const char*);
$API $sym $intern($state*,const char*,size_t);
$API $sym $intern_static($state*,const char*,size_t);
#define $intern_lit(mrb, lit) $intern_static(mrb, lit, $strlen_lit(lit))
$API $sym $intern_str($state*,$value);
$API $value $check_intern_cstr($state*,const char*);
$API $value $check_intern($state*,const char*,size_t);
$API $value $check_intern_str($state*,$value);
$API const char *$sym2name($state*,$sym);
$API const char *$sym2name_len($state*,$sym,$int*);
$API $value $sym2str($state*,$sym);

$API void *$malloc($state*, size_t);         /* raise RuntimeError if no mem */
$API void *$calloc($state*, size_t, size_t); /* ditto */
$API void *$realloc($state*, void*, size_t); /* ditto */
$API void *$realloc_simple($state*, void*, size_t); /* return NULL if no memory available */
$API void *$malloc_simple($state*, size_t);  /* return NULL if no memory available */
$API struct RBasic *$obj_alloc($state*, enum $vtype, struct RClass*);
$API void $free($state*, void*);

$API $value $str_new($state *mrb, const char *p, size_t len);

/**
 * Turns a C string into a Ruby string value.
 */
$API $value $str_new_cstr($state*, const char*);
$API $value $str_new_static($state *mrb, const char *p, size_t len);
#define $str_new_lit(mrb, lit) $str_new_static(mrb, (lit), $strlen_lit(lit))

#ifdef _WIN32
$API char* $utf8_from_locale(const char *p, int len);
$API char* $locale_from_utf8(const char *p, int len);
#define $locale_free(p) free(p)
#define $utf8_free(p) free(p)
#else
#define $utf8_from_locale(p, l) ((char*)p)
#define $locale_from_utf8(p, l) ((char*)p)
#define $locale_free(p)
#define $utf8_free(p)
#endif

/**
 * Creates new $state.
 *
 * @return
 *      Pointer to the newly created $state.
 */
$API $state* $open(void);

/**
 * Create new $state with custom allocators.
 *
 * @param f
 *      Reference to the allocation function.
 * @param ud
 *      User data will be passed to custom allocator f.
 *      If user data isn't required just pass NULL.
 * @return
 *      Pointer to the newly created $state.
 */
$API $state* $open_allocf($allocf f, void *ud);

/**
 * Create new $state with just the MRuby core
 *
 * @param f
 *      Reference to the allocation function.
 *      Use $default_allocf for the default
 * @param ud
 *      User data will be passed to custom allocator f.
 *      If user data isn't required just pass NULL.
 * @return
 *      Pointer to the newly created $state.
 */
$API $state* $open_core($allocf f, void *ud);

/**
 * Closes and frees a $state.
 *
 * @param mrb
 *      Pointer to the $state to be closed.
 */
$API void $close($state *mrb);

/**
 * The default allocation function.
 *
 * @see $allocf
 */
$API void* $default_allocf($state*, void*, size_t, void*);

$API $value $top_self($state *);
$API $value $run($state*, struct RProc*, $value);
$API $value $top_run($state*, struct RProc*, $value, unsigned int);
$API $value $vm_run($state*, struct RProc*, $value, unsigned int);
$API $value $vm_exec($state*, struct RProc*, $code*);
/* compatibility macros */
#define $toplevel_run_keep(m,p,k) $top_run((m),(p),$top_self(m),(k))
#define $toplevel_run(m,p) $toplevel_run_keep((m),(p),0)
#define $context_run(m,p,s,k) $vm_run((m),(p),(s),(k))

$API void $p($state*, $value);
$API $int $obj_id($value obj);
$API $sym $obj_to_sym($state *mrb, $value name);

$API $bool $obj_eq($state*, $value, $value);
$API $bool $obj_equal($state*, $value, $value);
$API $bool $equal($state *mrb, $value obj1, $value obj2);
$API $value $convert_to_integer($state *mrb, $value val, $int base);
$API $value $Integer($state *mrb, $value val);
#ifndef $WITHOUT_FLOAT
$API $value $Float($state *mrb, $value val);
#endif
$API $value $inspect($state *mrb, $value obj);
$API $bool $eql($state *mrb, $value obj1, $value obj2);

static inline int $gc_arena_save($state*);
static inline void $gc_arena_restore($state*,int);

static inline int
$gc_arena_save($state *mrb)
{
  return mrb->gc.arena_idx;
}

static inline void
$gc_arena_restore($state *mrb, int idx)
{
  mrb->gc.arena_idx = idx;
}

$API void $garbage_collect($state*);
$API void $full_gc($state*);
$API void $incremental_gc($state *);
$API void $gc_mark($state*,struct RBasic*);
#define $gc_mark_value(mrb,val) do {\
  if (!$immediate_p(val)) $gc_mark((mrb), $basic_ptr(val)); \
} while (0)
$API void $field_write_barrier($state *, struct RBasic*, struct RBasic*);
#define $field_write_barrier_value(mrb, obj, val) do{\
  if (!$immediate_p(val)) $field_write_barrier((mrb), (obj), $basic_ptr(val)); \
} while (0)
$API void $write_barrier($state *, struct RBasic*);

$API $value $check_convert_type($state *mrb, $value val, enum $vtype type, const char *tname, const char *method);
$API $value $any_to_s($state *mrb, $value obj);
$API const char * $obj_classname($state *mrb, $value obj);
$API struct RClass* $obj_class($state *mrb, $value obj);
$API $value $class_path($state *mrb, struct RClass *c);
$API $value $convert_type($state *mrb, $value val, enum $vtype type, const char *tname, const char *method);
$API $bool $obj_is_kind_of($state *mrb, $value obj, struct RClass *c);
$API $value $obj_inspect($state *mrb, $value self);
$API $value $obj_clone($state *mrb, $value self);

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

$API $value $exc_new($state *mrb, struct RClass *c, const char *ptr, size_t len);
$API $noreturn void $exc_raise($state *mrb, $value exc);

$API $noreturn void $raise($state *mrb, struct RClass *c, const char *msg);
$API $noreturn void $raisef($state *mrb, struct RClass *c, const char *fmt, ...);
$API $noreturn void $name_error($state *mrb, $sym id, const char *fmt, ...);
$API void $warn($state *mrb, const char *fmt, ...);
$API $noreturn void $bug($state *mrb, const char *fmt, ...);
$API void $print_backtrace($state *mrb);
$API void $print_error($state *mrb);

/* macros to get typical exception objects
   note:
   + those E_* macros requires $state* variable named mrb.
   + exception objects obtained from those macros are local to mrb
*/
#define E_RUNTIME_ERROR             ($exc_get(mrb, "RuntimeError"))
#define E_TYPE_ERROR                ($exc_get(mrb, "TypeError"))
#define E_ARGUMENT_ERROR            ($exc_get(mrb, "ArgumentError"))
#define E_INDEX_ERROR               ($exc_get(mrb, "IndexError"))
#define E_RANGE_ERROR               ($exc_get(mrb, "RangeError"))
#define E_NAME_ERROR                ($exc_get(mrb, "NameError"))
#define E_NOMETHOD_ERROR            ($exc_get(mrb, "NoMethodError"))
#define E_SCRIPT_ERROR              ($exc_get(mrb, "ScriptError"))
#define E_SYNTAX_ERROR              ($exc_get(mrb, "SyntaxError"))
#define E_LOCALJUMP_ERROR           ($exc_get(mrb, "LocalJumpError"))
#define E_REGEXP_ERROR              ($exc_get(mrb, "RegexpError"))
#define E_FROZEN_ERROR              ($exc_get(mrb, "FrozenError"))

#define E_NOTIMP_ERROR              ($exc_get(mrb, "NotImplementedError"))
#ifndef $WITHOUT_FLOAT
#define E_FLOATDOMAIN_ERROR         ($exc_get(mrb, "FloatDomainError"))
#endif

#define E_KEY_ERROR                 ($exc_get(mrb, "KeyError"))

$API $value $yield($state *mrb, $value b, $value arg);
$API $value $yield_argv($state *mrb, $value b, $int argc, const $value *argv);
$API $value $yield_with_class($state *mrb, $value b, $int argc, const $value *argv, $value self, struct RClass *c);

/* continue execution to the proc */
/* this function should always be called as the last function of a method */
/* e.g. return $yield_cont(mrb, proc, self, argc, argv); */
$value $yield_cont($state *mrb, $value b, $value self, $int argc, const $value *argv);

/* $gc_protect() leaves the object in the arena */
$API void $gc_protect($state *mrb, $value obj);
/* $gc_register() keeps the object from GC. */
$API void $gc_register($state *mrb, $value obj);
/* $gc_unregister() removes the object from GC root. */
$API void $gc_unregister($state *mrb, $value obj);

$API $value $to_int($state *mrb, $value val);
#define $int(mrb, val) $fixnum($to_int(mrb, val))
$API void $check_type($state *mrb, $value x, enum $vtype t);

typedef enum call_type {
  CALL_PUBLIC,
  CALL_FCALL,
  CALL_VCALL,
  CALL_TYPE_MAX
} call_type;

$API void $define_alias($state *mrb, struct RClass *c, const char *a, const char *b);
$API const char *$class_name($state *mrb, struct RClass* klass);
$API void $define_global_const($state *mrb, const char *name, $value val);

$API $value $attr_get($state *mrb, $value obj, $sym id);

$API $bool $respond_to($state *mrb, $value obj, $sym mid);
$API $bool $obj_is_instance_of($state *mrb, $value obj, struct RClass* c);
$API $bool $func_basic_p($state *mrb, $value obj, $sym mid, $func_t func);


/*
 * Resume a Fiber
 *
 * @mrbgem mruby-fiber
 */
$API $value $fiber_resume($state *mrb, $value fib, $int argc, const $value *argv);

/*
 * Yield a Fiber
 *
 * @mrbgem mruby-fiber
 */
$API $value $fiber_yield($state *mrb, $int argc, const $value *argv);

/*
 * Check if a Fiber is alive
 *
 * @mrbgem mruby-fiber
 */
$API $value $fiber_alive_p($state *mrb, $value fib);

/*
 * FiberError reference
 *
 * @mrbgem mruby-fiber
 */
#define E_FIBER_ERROR ($exc_get(mrb, "FiberError"))
$API void $stack_extend($state*, $int);

/* memory pool implementation */
typedef struct $pool $pool;
$API struct $pool* $pool_open($state*);
$API void $pool_close(struct $pool*);
$API void* $pool_alloc(struct $pool*, size_t);
$API void* $pool_realloc(struct $pool*, void*, size_t oldlen, size_t newlen);
$API $bool $pool_can_realloc(struct $pool*, void*, size_t);
$API void* $alloca($state *mrb, size_t);

$API void $state_atexit($state *mrb, $atexit_func func);

$API void $show_version($state *mrb);
$API void $show_copyright($state *mrb);

$API $value $format($state *mrb, const char *format, ...);

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

$END_DECL

#endif  /* MRUBY_H */
