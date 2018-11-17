/*
** mruby/array.h - Array class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_ARRAY_H
#define MRUBY_ARRAY_H

#include "common.h"

/*
 * Array class
 */
BEGIN_DECL


typedef struct shared_array {
  int refcnt;
  int len;
  value *ptr;
} shared_array;

#define ARY_EMBED_LEN_MAX ((int)(sizeof(void*)*3/sizeof(value)))
struct RArray {
  OBJECT_HEADER;
  union {
    struct {
      int len;
      union {
        int capa;
        shared_array *shared;
      } aux;
      value *ptr;
    } heap;
    value embed[ARY_EMBED_LEN_MAX];
  } as;
};

#define ary_ptr(v)    ((struct RArray*)(ptr(v)))
#define ary_value(p)  obj_value((void*)(p))
#define RARRAY(v)  ((struct RArray*)(ptr(v)))

#define ARY_EMBED_MASK  7
#define ARY_EMBED_P(a) ((a)->flags & ARY_EMBED_MASK)
#define ARY_UNSET_EMBED_FLAG(a) ((a)->flags &= ~(ARY_EMBED_MASK))
#define ARY_EMBED_LEN(a) ((int)(((a)->flags & ARY_EMBED_MASK) - 1))
#define ARY_SET_EMBED_LEN(a,len) ((a)->flags = ((a)->flags&~ARY_EMBED_MASK) | ((uint32_t)(len) + 1))
#define ARY_EMBED_PTR(a) (&((a)->as.embed[0]))

#define ARY_LEN(a) (ARY_EMBED_P(a)?ARY_EMBED_LEN(a):(a)->as.heap.len)
#define ARY_PTR(a) (ARY_EMBED_P(a)?ARY_EMBED_PTR(a):(a)->as.heap.ptr)
#define RARRAY_LEN(a) ARY_LEN(RARRAY(a))
#define RARRAY_PTR(a) ARY_PTR(RARRAY(a))
#define ARY_SET_LEN(a,n) do {\
  if (ARY_EMBED_P(a)) {\
    assert((n) <= ARY_EMBED_LEN_MAX); \
    ARY_SET_EMBED_LEN(a,n);\
  }\
  else\
    (a)->as.heap.len = (n);\
} while (0)
#define ARY_CAPA(a) (ARY_EMBED_P(a)?ARY_EMBED_LEN_MAX:(a)->as.heap.aux.capa)
#define ARY_SHARED      256
#define ARY_SHARED_P(a) ((a)->flags & ARY_SHARED)
#define ARY_SET_SHARED_FLAG(a) ((a)->flags |= ARY_SHARED)
#define ARY_UNSET_SHARED_FLAG(a) ((a)->flags &= ~ARY_SHARED)

void ary_decref(state*, shared_array*);
API void ary_modify(state*, struct RArray*);
API value ary_new_capa(state*, int);

/*
 * Initializes a new array.
 *
 * Equivalent to:
 *
 *      Array.new
 *
 * @param mrb The mruby state reference.
 * @return The initialized array.
 */
API value ary_new(state *mrb);

/*
 * Initializes a new array with initial values
 *
 * Equivalent to:
 *
 *      Array[value1, value2, ...]
 *
 * @param mrb The mruby state reference.
 * @param size The numer of values.
 * @param vals The actual values.
 * @return The initialized array.
 */
API value ary_new_from_values(state *mrb, int size, const value *vals);

/*
 * Initializes a new array with two initial values
 *
 * Equivalent to:
 *
 *      Array[car, cdr]
 *
 * @param mrb The mruby state reference.
 * @param car The first value.
 * @param cdr The second value.
 * @return The initialized array.
 */
API value assoc_new(state *mrb, value car, value cdr);

/*
 * Concatenate two arrays. The target array will be modified
 *
 * Equivalent to:
 *      ary.concat(other)
 *
 * @param mrb The mruby state reference.
 * @param self The target array.
 * @param other The array that will be concatenated to self.
 */
API void ary_concat(state *mrb, value self, value other);

/*
 * Create an array from the input. It tries calling to_a on the
 * value. If value does not respond to that, it creates a new
 * array with just this value.
 *
 * @param mrb The mruby state reference.
 * @param value The value to change into an array.
 * @return An array representation of value.
 */
API value ary_splat(state *mrb, value value);

/*
 * Pushes value into array.
 *
 * Equivalent to:
 *
 *      ary << value
 *
 * @param mrb The mruby state reference.
 * @param ary The array in which the value will be pushed
 * @param value The value to be pushed into array
 */
API void ary_push(state *mrb, value array, value value);

/*
 * Pops the last element from the array.
 *
 * Equivalent to:
 *
 *      ary.pop
 *
 * @param mrb The mruby state reference.
 * @param ary The array from which the value will be popped.
 * @return The popped value.
 */
API value ary_pop(state *mrb, value ary);

/*
 * Returns a reference to an element of the array on the given index.
 *
 * Equivalent to:
 *
 *      ary[n]
 *
 * @param mrb The mruby state reference.
 * @param ary The target array.
 * @param n The array index being referenced
 * @return The referenced value.
 */
API value ary_ref(state *mrb, value ary, int n);

/*
 * Sets a value on an array at the given index
 *
 * Equivalent to:
 *
 *      ary[n] = val
 *
 * @param mrb The mruby state reference.
 * @param ary The target array.
 * @param n The array index being referenced.
 * @param val The value being setted.
 */
API void ary_set(state *mrb, value ary, int n, value val);

/*
 * Replace the array with another array
 *
 * Equivalent to:
 *
 *      ary.replace(other)
 *
 * @param mrb The mruby state reference
 * @param self The target array.
 * @param other The array to replace it with.
 */
API void ary_replace(state *mrb, value self, value other);
API value check_array_type(state *mrb, value self);

/*
 * Unshift an element into the array
 *
 * Equivalent to:
 *
 *     ary.unshift(item)
 *
 * @param mrb The mruby state reference.
 * @param self The target array.
 * @param item The item to unshift.
 */
API value ary_unshift(state *mrb, value self, value item);

/*
 * Get nth element in the array
 *
 * Equivalent to:
 *
 *     ary[offset]
 *
 * @param ary The target array.
 * @param offset The element position (negative counts from the tail).
 */
API value ary_entry(value ary, int offset);

/*
 * Shifts the first element from the array.
 *
 * Equivalent to:
 *
 *      ary.shift
 *
 * @param mrb The mruby state reference.
 * @param self The array from which the value will be shifted.
 * @return The shifted value.
 */
API value ary_shift(state *mrb, value self);

/*
 * Removes all elements from the array
 *
 * Equivalent to:
 *
 *      ary.clear
 *
 * @param mrb The mruby state reference.
 * @param self The target array.
 * @return self
 */
API value ary_clear(state *mrb, value self);

/*
 * Join the array elements together in a string
 *
 * Equivalent to:
 *
 *      ary.join(sep="")
 *
 * @param mrb The mruby state reference.
 * @param ary The target array
 * @param sep The separater, can be NULL
 */
API value ary_join(state *mrb, value ary, value sep);

/*
 * Update the capacity of the array
 *
 * @param mrb The mruby state reference.
 * @param ary The target array.
 * @param new_len The new capacity of the array
 */
API value ary_resize(state *mrb, value ary, int new_len);

END_DECL

#endif  /* MRUBY_ARRAY_H */
