/*
** random.c - Random module
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/variable.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/array.h>
#include "mt19937ar.h"

#include <time.h>

static char const MT_STATE_KEY[] = "$_i_mt_state";

static const struct _data_type mt_state_type = {
  MT_STATE_KEY, _free,
};

static value _random_rand(state *mrb, value self);
static value _random_srand(state *mrb, value self);

static void
mt_srand(mt_state *t, unsigned long seed)
{
  _random_init_genrand(t, seed);
}

static unsigned long
mt_rand(mt_state *t)
{
  return _random_genrand_int32(t);
}

static double
mt_rand_real(mt_state *t)
{
  return _random_genrand_real1(t);
}

static value
_random_mt_srand(state *mrb, mt_state *t, value seed)
{
  if (_nil_p(seed)) {
    seed = _fixnum_value((_int)(time(NULL) + mt_rand(t)));
    if (_fixnum(seed) < 0) {
      seed = _fixnum_value(0 - _fixnum(seed));
    }
  }

  mt_srand(t, (unsigned) _fixnum(seed));

  return seed;
}

static value
_random_mt_rand(state *mrb, mt_state *t, value max)
{
  value value;

  if (_fixnum(max) == 0) {
    value = _float_value(mrb, mt_rand_real(t));
  }
  else {
    value = _fixnum_value(mt_rand(t) % _fixnum(max));
  }

  return value;
}

static value
get_opt(state* mrb)
{
  value arg;

  arg = _nil_value();
  _get_args(mrb, "|o", &arg);

  if (!_nil_p(arg)) {
    arg = _check_convert_type(mrb, arg, MRB_TT_FIXNUM, "Fixnum", "to_int");
    if (_nil_p(arg)) {
      _raise(mrb, E_ARGUMENT_ERROR, "invalid argument type");
    }
    if (_fixnum(arg) < 0) {
      arg = _fixnum_value(0 - _fixnum(arg));
    }
  }
  return arg;
}

static value
get_random(state *mrb) {
  return _const_get(mrb,
             _obj_value(_class_get(mrb, "Random")),
             _intern_lit(mrb, "DEFAULT"));
}

static mt_state *
get_random_state(state *mrb)
{
  value random_val = get_random(mrb);
  return DATA_GET_PTR(mrb, random_val, &mt_state_type, mt_state);
}

static value
_random_g_rand(state *mrb, value self)
{
  value random = get_random(mrb);
  return _random_rand(mrb, random);
}

static value
_random_g_srand(state *mrb, value self)
{
  value random = get_random(mrb);
  return _random_srand(mrb, random);
}

static value
_random_init(state *mrb, value self)
{
  value seed;
  mt_state *t;

  seed = get_opt(mrb);

  /* avoid memory leaks */
  t = (mt_state*)DATA_PTR(self);
  if (t) {
    _free(mrb, t);
  }
  _data_init(self, NULL, &mt_state_type);

  t = (mt_state *)_malloc(mrb, sizeof(mt_state));
  t->mti = N + 1;

  seed = _random_mt_srand(mrb, t, seed);
  if (_nil_p(seed)) {
    t->has_seed = FALSE;
  }
  else {
    _assert(_fixnum_p(seed));
    t->has_seed = TRUE;
    t->seed = _fixnum(seed);
  }

  _data_init(self, t, &mt_state_type);

  return self;
}

static void
_random_rand_seed(state *mrb, mt_state *t)
{
  if (!t->has_seed) {
    _random_mt_srand(mrb, t, _nil_value());
  }
}

static value
_random_rand(state *mrb, value self)
{
  value max;
  mt_state *t = DATA_GET_PTR(mrb, self, &mt_state_type, mt_state);

  max = get_opt(mrb);
  _random_rand_seed(mrb, t);
  return _random_mt_rand(mrb, t, max);
}

static value
_random_srand(state *mrb, value self)
{
  value seed;
  value old_seed;
  mt_state *t = DATA_GET_PTR(mrb, self, &mt_state_type, mt_state);

  seed = get_opt(mrb);
  seed = _random_mt_srand(mrb, t, seed);
  old_seed = t->has_seed? _fixnum_value(t->seed) : _nil_value();
  if (_nil_p(seed)) {
    t->has_seed = FALSE;
  }
  else {
    _assert(_fixnum_p(seed));
    t->has_seed = TRUE;
    t->seed = _fixnum(seed);
  }

  return old_seed;
}

/*
 *  call-seq:
 *     ary.shuffle!   ->   ary
 *
 *  Shuffles elements in self in place.
 */

static value
_ary_shuffle_bang(state *mrb, value ary)
{
  _int i;
  mt_state *random = NULL;

  if (RARRAY_LEN(ary) > 1) {
    _get_args(mrb, "|d", &random, &mt_state_type);

    if (random == NULL) {
      random = get_random_state(mrb);
    }
    _random_rand_seed(mrb, random);

    _ary_modify(mrb, _ary_ptr(ary));

    for (i = RARRAY_LEN(ary) - 1; i > 0; i--)  {
      _int j;
      value *ptr = RARRAY_PTR(ary);
      value tmp;


      j = _fixnum(_random_mt_rand(mrb, random, _fixnum_value(RARRAY_LEN(ary))));

      tmp = ptr[i];
      ptr[i] = ptr[j];
      ptr[j] = tmp;
    }
  }

  return ary;
}

/*
 *  call-seq:
 *     ary.shuffle   ->   new_ary
 *
 *  Returns a new array with elements of self shuffled.
 */

static value
_ary_shuffle(state *mrb, value ary)
{
  value new_ary = _ary_new_from_values(mrb, RARRAY_LEN(ary), RARRAY_PTR(ary));
  _ary_shuffle_bang(mrb, new_ary);

  return new_ary;
}

/*
 *  call-seq:
 *     ary.sample      ->   obj
 *     ary.sample(n)   ->   new_ary
 *
 *  Choose a random element or +n+ random elements from the array.
 *
 *  The elements are chosen by using random and unique indices into the array
 *  in order to ensure that an element doesn't repeat itself unless the array
 *  already contained duplicate elements.
 *
 *  If the array is empty the first form returns +nil+ and the second form
 *  returns an empty array.
 */

static value
_ary_sample(state *mrb, value ary)
{
  _int n = 0;
  _bool given;
  mt_state *random = NULL;
  _int len;

  _get_args(mrb, "|i?d", &n, &given, &random, &mt_state_type);
  if (random == NULL) {
    random = get_random_state(mrb);
  }
  _random_rand_seed(mrb, random);
  mt_rand(random);
  len = RARRAY_LEN(ary);
  if (!given) {                 /* pick one element */
    switch (len) {
    case 0:
      return _nil_value();
    case 1:
      return RARRAY_PTR(ary)[0];
    default:
      return RARRAY_PTR(ary)[mt_rand(random) % len];
    }
  }
  else {
    value result;
    _int i, j;

    if (n < 0) _raise(mrb, E_ARGUMENT_ERROR, "negative sample number");
    if (n > len) n = len;
    result = _ary_new_capa(mrb, n);
    for (i=0; i<n; i++) {
      _int r;

      for (;;) {
      retry:
        r = mt_rand(random) % len;

        for (j=0; j<i; j++) {
          if (_fixnum(RARRAY_PTR(result)[j]) == r) {
            goto retry;         /* retry if duplicate */
          }
        }
        break;
      }
      _ary_push(mrb, result, _fixnum_value(r));
    }
    for (i=0; i<n; i++) {
      _ary_set(mrb, result, i, RARRAY_PTR(ary)[_fixnum(RARRAY_PTR(result)[i])]);
    }
    return result;
  }
}


void _mruby_random_gem_init(state *mrb)
{
  struct RClass *random;
  struct RClass *array = mrb->array_class;

  _define_method(mrb, mrb->kernel_module, "rand", _random_g_rand, MRB_ARGS_OPT(1));
  _define_method(mrb, mrb->kernel_module, "srand", _random_g_srand, MRB_ARGS_OPT(1));

  random = _define_class(mrb, "Random", mrb->object_class);
  MRB_SET_INSTANCE_TT(random, MRB_TT_DATA);
  _define_class_method(mrb, random, "rand", _random_g_rand, MRB_ARGS_OPT(1));
  _define_class_method(mrb, random, "srand", _random_g_srand, MRB_ARGS_OPT(1));

  _define_method(mrb, random, "initialize", _random_init, MRB_ARGS_OPT(1));
  _define_method(mrb, random, "rand", _random_rand, MRB_ARGS_OPT(1));
  _define_method(mrb, random, "srand", _random_srand, MRB_ARGS_OPT(1));

  _define_method(mrb, array, "shuffle", _ary_shuffle, MRB_ARGS_OPT(1));
  _define_method(mrb, array, "shuffle!", _ary_shuffle_bang, MRB_ARGS_OPT(1));
  _define_method(mrb, array, "sample", _ary_sample, MRB_ARGS_OPT(2));

  _const_set(mrb, _obj_value(random), _intern_lit(mrb, "DEFAULT"),
          _obj_new(mrb, random, 0, NULL));
}

void _mruby_random_gem_final(state *mrb)
{
}
