#include <mruby.h>
#include <mruby/gc.h>
#include <mruby/hash.h>
#include <mruby/class.h>

struct os_count_struct {
  $int total;
  $int freed;
  $int counts[$TT_MAXDEFINE+1];
};

static int
os_count_object_type($state *mrb, struct RBasic *obj, void *data)
{
  struct os_count_struct *obj_count;
  obj_count = (struct os_count_struct*)data;

  obj_count->total++;

  if ($object_dead_p(mrb, obj)) {
    obj_count->freed++;
  }
  else {
    obj_count->counts[obj->tt]++;
  }
  return $EACH_OBJ_OK;
}

/*
 *  call-seq:
 *     ObjectSpace.count_objects([result_hash]) -> hash
 *
 *  Counts objects for each type.
 *
 *  It returns a hash, such as:
 *  {
 *    :TOTAL=>10000,
 *    :FREE=>3011,
 *    :T_OBJECT=>6,
 *    :T_CLASS=>404,
 *    # ...
 *  }
 *
 *  If the optional argument +result_hash+ is given,
 *  it is overwritten and returned. This is intended to avoid probe effect.
 *
 */

static $value
os_count_objects($state *mrb, $value self)
{
  struct os_count_struct obj_count = { 0 };
  $int i;
  $value hash;

  if ($get_args(mrb, "|H", &hash) == 0) {
    hash = $hash_new(mrb);
  }

  if (!$hash_empty_p(mrb, hash)) {
    $hash_clear(mrb, hash);
  }

  $objspace_each_objects(mrb, os_count_object_type, &obj_count);

  $hash_set(mrb, hash, $symbol_value($intern_lit(mrb, "TOTAL")), $fixnum_value(obj_count.total));
  $hash_set(mrb, hash, $symbol_value($intern_lit(mrb, "FREE")), $fixnum_value(obj_count.freed));

  for (i = $TT_FALSE; i < $TT_MAXDEFINE; i++) {
    $value type;
    switch (i) {
#define COUNT_TYPE(t) case ($T ## t): type = $symbol_value($intern_lit(mrb, #t)); break;
      COUNT_TYPE(T_FALSE);
      COUNT_TYPE(T_FREE);
      COUNT_TYPE(T_TRUE);
      COUNT_TYPE(T_FIXNUM);
      COUNT_TYPE(T_SYMBOL);
      COUNT_TYPE(T_UNDEF);
      COUNT_TYPE(T_FLOAT);
      COUNT_TYPE(T_CPTR);
      COUNT_TYPE(T_OBJECT);
      COUNT_TYPE(T_CLASS);
      COUNT_TYPE(T_MODULE);
      COUNT_TYPE(T_ICLASS);
      COUNT_TYPE(T_SCLASS);
      COUNT_TYPE(T_PROC);
      COUNT_TYPE(T_ARRAY);
      COUNT_TYPE(T_HASH);
      COUNT_TYPE(T_STRING);
      COUNT_TYPE(T_RANGE);
      COUNT_TYPE(T_EXCEPTION);
      COUNT_TYPE(T_FILE);
      COUNT_TYPE(T_ENV);
      COUNT_TYPE(T_DATA);
      COUNT_TYPE(T_FIBER);
#undef COUNT_TYPE
    default:
      type = $fixnum_value(i); break;
    }
    if (obj_count.counts[i])
      $hash_set(mrb, hash, type, $fixnum_value(obj_count.counts[i]));
  }

  return hash;
}

struct os_each_object_data {
  $value block;
  struct RClass *target_module;
  $int count;
};

static int
os_each_object_cb($state *mrb, struct RBasic *obj, void *ud)
{
  struct os_each_object_data *d = (struct os_each_object_data*)ud;

  /* filter dead objects */
  if ($object_dead_p(mrb, obj)) {
    return $EACH_OBJ_OK;
  }

  /* filter internal objects */
  switch (obj->tt) {
  case $TT_ENV:
  case $TT_ICLASS:
    return $EACH_OBJ_OK;
  default:
    break;
  }

  /* filter half baked (or internal) objects */
  if (!obj->c) return $EACH_OBJ_OK;

  /* filter class kind if target module defined */
  if (d->target_module && !$obj_is_kind_of(mrb, $obj_value(obj), d->target_module)) {
    return $EACH_OBJ_OK;
  }

  $yield(mrb, d->block, $obj_value(obj));
  ++d->count;
  return $EACH_OBJ_OK;
}

/*
 *  call-seq:
 *     ObjectSpace.each_object([module]) {|obj| ... } -> fixnum
 *
 *  Calls the block once for each object in this Ruby process.
 *  Returns the number of objects found.
 *  If the optional argument +module+ is given,
 *  calls the block for only those classes or modules
 *  that match (or are a subclass of) +module+.
 *
 *  If no block is given, ArgumentError is raised.
 *
 */

static $value
os_each_object($state *mrb, $value self)
{
  $value cls = $nil_value();
  struct os_each_object_data d;
  $get_args(mrb, "&|C", &d.block, &cls);

  if ($nil_p(d.block)) {
    $raise(mrb, E_ARGUMENT_ERROR, "Expected block in ObjectSpace.each_object.");
  }

  d.target_module = $nil_p(cls) ? NULL : $class_ptr(cls);
  d.count = 0;
  $objspace_each_objects(mrb, os_each_object_cb, &d);
  return $fixnum_value(d.count);
}

void
$mruby_objectspace_gem_init($state *mrb)
{
  struct RClass *os = $define_module(mrb, "ObjectSpace");
  $define_class_method(mrb, os, "count_objects", os_count_objects, $ARGS_OPT(1));
  $define_class_method(mrb, os, "each_object", os_each_object, $ARGS_OPT(1));
}

void
$mruby_objectspace_gem_final($state *mrb)
{
}
