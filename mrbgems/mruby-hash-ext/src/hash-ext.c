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

static $value
hash_values_at($state *mrb, $value hash)
{
  $value *argv, result;
  $int argc, i;
  int ai;

  $get_args(mrb, "*", &argv, &argc);
  result = $ary_new_capa(mrb, argc);
  ai = $gc_arena_save(mrb);
  for (i = 0; i < argc; i++) {
    $ary_push(mrb, result, $hash_get(mrb, hash, argv[i]));
    $gc_arena_restore(mrb, ai);
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
static $value
hash_slice($state *mrb, $value hash)
{
  $value *argv, result;
  $int argc, i;

  $get_args(mrb, "*", &argv, &argc);
  if (argc == 0) {
    return $hash_new_capa(mrb, argc);
  }
  result = $hash_new_capa(mrb, argc);
  for (i = 0; i < argc; i++) {
    $value key = argv[i];
    $value val;

    val = $hash_fetch(mrb, hash, key, $undef_value());
    if (!$undef_p(val)) {
      $hash_set(mrb, result, key, val);
    }
  }
  return result;
}

void
$mruby_hash_ext_gem_init($state *mrb)
{
  struct RClass *h;

  h = mrb->hash_class;
  $define_method(mrb, h, "values_at", hash_values_at, $ARGS_ANY());
  $define_method(mrb, h, "slice",     hash_slice, $ARGS_ANY());
}

void
$mruby_hash_ext_gem_final($state *mrb)
{
}
