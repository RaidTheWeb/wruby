/*
** hash.c - Hash class
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>

/*
 * call-seq:
 *   hsh.values_at(key, ...)   -> array
 *
 * Return an array containing the values associated with the given keys.
 * Also see <code>Hash.select</code>.
 *
 *   h = { "cat" => "feline", "dog" => "canine", "cow" => "bovine" }
 *   h.values_at("cow", "cat")  #=> ["bovine", "feline"]
 */

static _value
hash_values_at(_state *mrb, _value hash)
{
  _value *argv, result;
  _int argc, i;
  int ai;

  _get_args(mrb, "*", &argv, &argc);
  result = _ary_new_capa(mrb, argc);
  ai = _gc_arena_save(mrb);
  for (i = 0; i < argc; i++) {
    _ary_push(mrb, result, _hash_get(mrb, hash, argv[i]));
    _gc_arena_restore(mrb, ai);
  }
  return result;
}

/*
 *  call-seq:
 *     hsh.slice(*keys) -> a_hash
 *
 *  Returns a hash containing only the given keys and their values.
 *
 *     h = { a: 100, b: 200, c: 300 }
 *     h.slice(:a)           #=> {:a=>100}
 *     h.slice(:b, :c, :d)   #=> {:b=>200, :c=>300}
 */
static _value
hash_slice(_state *mrb, _value hash)
{
  _value *argv, result;
  _int argc, i;

  _get_args(mrb, "*", &argv, &argc);
  if (argc == 0) {
    return _hash_new_capa(mrb, argc);
  }
  result = _hash_new_capa(mrb, argc);
  for (i = 0; i < argc; i++) {
    _value key = argv[i];
    _value val;

    val = _hash_fetch(mrb, hash, key, _undef_value());
    if (!_undef_p(val)) {
      _hash_set(mrb, result, key, val);
    }
  }
  return result;
}

void
_mruby_hash_ext_gem_init(_state *mrb)
{
  struct RClass *h;

  h = mrb->hash_class;
  _define_method(mrb, h, "values_at", hash_values_at, MRB_ARGS_ANY());
  _define_method(mrb, h, "slice",     hash_slice, MRB_ARGS_ANY());
}

void
_mruby_hash_ext_gem_final(_state *mrb)
{
}
