#include <mruby.h>
#include <mruby/gc.h>
#include <mruby/hash.h>
#include <mruby/class.h>

struct os_count_struct {
  _int total;
  _int freed;
  _int counts[MRB_TT_MAXDEFINE+1];
};

static int
os_count_object_type(_state *mrb, struct RBasic *obj, void *data)
{
  struct os_count_struct *obj_count;
  obj_count = (struct os_count_struct*)data;

  obj_count->total++;

  if (_object_dead_p(mrb, obj)) {
    obj_count->freed++;
  }
  else {
    obj_count->counts[obj->tt]++;
  }
  return MRB_EACH_OBJ_OK;
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

static _value
os_count_objects(_state *mrb, _value self)
{
  struct os_count_struct obj_count = { 0 };
  _int i;
  _value hash;

  if (_get_args(mrb, "|H", &hash) == 0) {
    hash = _hash_new(mrb);
  }

  if (!_hash_empty_p(mrb, hash)) {
    _hash_clear(mrb, hash);
  }

  _objspace_each_objects(mrb, os_count_object_type, &obj_count);

  _hash_set(mrb, hash, _symbol_value(_intern_lit(mrb, "TOTAL")), _fixnum_value(obj_count.total));
  _hash_set(mrb, hash, _symbol_value(_intern_lit(mrb, "FREE")), _fixnum_value(obj_count.freed));

  for (i = MRB_TT_FALSE; i < MRB_TT_MAXDEFINE; i++) {
    _value type;
    switch (i) {
#define COUNT_TYPE(t) case (MRB_T ## t): type = _symbol_value(_intern_lit(mrb, #t)); break;
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
      type = _fixnum_value(i); break;
    }
    if (obj_count.counts[i])
      _hash_set(mrb, hash, type, _fixnum_value(obj_count.counts[i]));
  }

  return hash;
}

struct os_each_object_data {
  _value block;
  struct RClass *target_module;
  _int count;
};

static int
os_each_object_cb(_state *mrb, struct RBasic *obj, void *ud)
{
  struct os_each_object_data *d = (struct os_each_object_data*)ud;

  /* filter dead objects */
  if (_object_dead_p(mrb, obj)) {
    return MRB_EACH_OBJ_OK;
  }

  /* filter internal objects */
  switch (obj->tt) {
  case MRB_TT_ENV:
  case MRB_TT_ICLASS:
    return MRB_EACH_OBJ_OK;
  default:
    break;
  }

  /* filter half baked (or internal) objects */
  if (!obj->c) return MRB_EACH_OBJ_OK;

  /* filter class kind if target module defined */
  if (d->target_module && !_obj_is_kind_of(mrb, _obj_value(obj), d->target_module)) {
    return MRB_EACH_OBJ_OK;
  }

  _yield(mrb, d->block, _obj_value(obj));
  ++d->count;
  return MRB_EACH_OBJ_OK;
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

static _value
os_each_object(_state *mrb, _value self)
{
  _value cls = _nil_value();
  struct os_each_object_data d;
  _get_args(mrb, "&|C", &d.block, &cls);

  if (_nil_p(d.block)) {
    _raise(mrb, E_ARGUMENT_ERROR, "Expected block in ObjectSpace.each_object.");
  }

  d.target_module = _nil_p(cls) ? NULL : _class_ptr(cls);
  d.count = 0;
  _objspace_each_objects(mrb, os_each_object_cb, &d);
  return _fixnum_value(d.count);
}

void
_mruby_objectspace_gem_init(_state *mrb)
{
  struct RClass *os = _define_module(mrb, "ObjectSpace");
  _define_class_method(mrb, os, "count_objects", os_count_objects, MRB_ARGS_OPT(1));
  _define_class_method(mrb, os, "each_object", os_each_object, MRB_ARGS_OPT(1));
}

void
_mruby_objectspace_gem_final(_state *mrb)
{
}
