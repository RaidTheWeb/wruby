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

static char const MT_STATE_KEY[] = "$$i_mt_state";

static const struct $data_type mt_state_type = {
  MT_STATE_KEY, $free,
};

static $value $random_rand($state *mrb, $value self);
static $value $random_srand($state *mrb, $value self);

static void
mt_srand(mt_state *t, unsigned long seed)
{
  $random_init_genrand(t, seed);
}

static unsigned long
mt_rand(mt_state *t)
{
  return $random_genrand_int32(t);
}

static double
mt_rand_real(mt_state *t)
{
  return $random_genrand_real1(t);
}

static $value
$random_mt_srand($state *mrb, mt_state *t, $value seed)
{
  if ($nil_p(seed)) {
    seed = $fixnum_value(($int)(time(NULL) + mt_rand(t)));
    if ($fixnum(seed) < 0) {
      seed = $fixnum_value(0 - $fixnum(seed));
    }
  }

  mt_srand(t, (unsigned) $fixnum(seed));

  return seed;
}

static $value
$random_mt_rand($state *mrb, mt_state *t, $value max)
{
  $value value;

  if ($fixnum(max) == 0) {
    value = $float_value(mrb, mt_rand_real(t));
  }
  else {
    value = $fixnum_value(mt_rand(t) % $fixnum(max));
  }

  return value;
}

static $value
get_opt($state* mrb)
{
  $value arg;

  arg = $nil_value();
  $get_args(mrb, "|o", &arg);

  if (!$nil_p(arg)) {
    arg = $check_convert_type(mrb, arg, $TT_FIXNUM, "Fixnum", "to_int");
    if ($nil_p(arg)) {
      $raise(mrb, E_ARGUMENT_ERROR, "invalid argument type");
    }
    if ($fixnum(arg) < 0) {
      arg = $fixnum_value(0 - $fixnum(arg));
    }
  }
  return arg;
}

static $value
get_random($state *mrb) {
  return $const_get(mrb,
             $obj_value($class_get(mrb, "Random")),
             $intern_lit(mrb, "DEFAULT"));
}

static mt_state *
get_random_state($state *mrb)
{
  $value random_val = get_random(mrb);
  return DATA_GET_PTR(mrb, random_val, &mt_state_type, mt_state);
}

static $value
$random_g_rand($state *mrb, $value self)
{
  $value random = get_random(mrb);
  return $random_rand(mrb, random);
}

static $value
$random_g_srand($state *mrb, $value self)
{
  $value random = get_random(mrb);
  return $random_srand(mrb, random);
}

static $value
$random_init($state *mrb, $value self)
{
  $value seed;
  mt_state *t;

  seed = get_opt(mrb);

  /* avoid memory leaks */
  t = (mt_state*)DATA_PTR(self);
  if (t) {
    $free(mrb, t);
  }
  $data_init(self, NULL, &mt_state_type);

  t = (mt_state *)$malloc(mrb, sizeof(mt_state));
  t->mti = N + 1;

  seed = $random_mt_srand(mrb, t, seed);
  if ($nil_p(seed)) {
    t->has_seed = FALSE;
  }
  else {
    $assert($fixnum_p(seed));
    t->has_seed = TRUE;
    t->seed = $fixnum(seed);
  }

  $data_init(self, t, &mt_state_type);

  return self;
}

static void
$random_rand_seed($state *mrb, mt_state *t)
{
  if (!t->has_seed) {
    $random_mt_srand(mrb, t, $nil_value());
  }
}

static $value
$random_rand($state *mrb, $value self)
{
  $value max;
  mt_state *t = DATA_GET_PTR(mrb, self, &mt_state_type, mt_state);

  max = get_opt(mrb);
  $random_rand_seed(mrb, t);
  return $random_mt_rand(mrb, t, max);
}

static $value
$random_srand($state *mrb, $value self)
{
  $value seed;
  $value old_seed;
  mt_state *t = DATA_GET_PTR(mrb, self, &mt_state_type, mt_state);

  seed = get_opt(mrb);
  seed = $random_mt_srand(mrb, t, seed);
  old_seed = t->has_seed? $fixnum_value(t->seed) : $nil_value();
  if ($nil_p(seed)) {
    t->has_seed = FALSE;
  }
  else {
    $assert($fixnum_p(seed));
    t->has_seed = TRUE;
    t->seed = $fixnum(seed);
  }

  return old_seed;
}

/*
 *  call-seq:
 *     ary.shuffle!   ->   ary
 *
 *  Shuffles elements in self in place.
 */

static $value
$ary_shuffle_bang($state *mrb, $value ary)
{
  $int i;
  mt_state *random = NULL;

  if (RARRAY_LEN(ary) > 1) {
    $get_args(mrb, "|d", &random, &mt_state_type);

    if (random == NULL) {
      random = get_random_state(mrb);
    }
    $random_rand_seed(mrb, random);

    $ary_modify(mrb, $ary_ptr(ary));

    for (i = RARRAY_LEN(ary) - 1; i > 0; i--)  {
      $int j;
      $value *ptr = RARRAY_PTR(ary);
      $value tmp;


      j = $fixnum($random_mt_rand(mrb, random, $fixnum_value(RARRAY_LEN(ary))));

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

static $value
$ary_shuffle($state *mrb, $value ary)
{
  $value new_ary = $ary_new_from_values(mrb, RARRAY_LEN(ary), RARRAY_PTR(ary));
  $ary_shuffle_bang(mrb, new_ary);

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

static $value
$ary_sample($state *mrb, $value ary)
{
  $int n = 0;
  $bool given;
  mt_state *random = NULL;
  $int len;

  $get_args(mrb, "|i?d", &n, &given, &random, &mt_state_type);
  if (random == NULL) {
    random = get_random_state(mrb);
  }
  $random_rand_seed(mrb, random);
  mt_rand(random);
  len = RARRAY_LEN(ary);
  if (!given) {                 /* pick one element */
    switch (len) {
    case 0:
      return $nil_value();
    case 1:
      return RARRAY_PTR(ary)[0];
    default:
      return RARRAY_PTR(ary)[mt_rand(random) % len];
    }
  }
  else {
    $value result;
    $int i, j;

    if (n < 0) $raise(mrb, E_ARGUMENT_ERROR, "negative sample number");
    if (n > len) n = len;
    result = $ary_new_capa(mrb, n);
    for (i=0; i<n; i++) {
      $int r;

      for (;;) {
      retry:
        r = mt_rand(random) % len;

        for (j=0; j<i; j++) {
          if ($fixnum(RARRAY_PTR(result)[j]) == r) {
            goto retry;         /* retry if duplicate */
          }
        }
        break;
      }
      $ary_push(mrb, result, $fixnum_value(r));
    }
    for (i=0; i<n; i++) {
      $ary_set(mrb, result, i, RARRAY_PTR(ary)[$fixnum(RARRAY_PTR(result)[i])]);
    }
    return result;
  }
}


void $mruby_random_gem_init($state *mrb)
{
  struct RClass *random;
  struct RClass *array = mrb->array_class;

  $define_method(mrb, mrb->kernel_module, "rand", $random_g_rand, $ARGS_OPT(1));
  $define_method(mrb, mrb->kernel_module, "srand", $random_g_srand, $ARGS_OPT(1));

  random = $define_class(mrb, "Random", mrb->object_class);
  $SET_INSTANCE_TT(random, $TT_DATA);
  $define_class_method(mrb, random, "rand", $random_g_rand, $ARGS_OPT(1));
  $define_class_method(mrb, random, "srand", $random_g_srand, $ARGS_OPT(1));

  $define_method(mrb, random, "initialize", $random_init, $ARGS_OPT(1));
  $define_method(mrb, random, "rand", $random_rand, $ARGS_OPT(1));
  $define_method(mrb, random, "srand", $random_srand, $ARGS_OPT(1));

  $define_method(mrb, array, "shuffle", $ary_shuffle, $ARGS_OPT(1));
  $define_method(mrb, array, "shuffle!", $ary_shuffle_bang, $ARGS_OPT(1));
  $define_method(mrb, array, "sample", $ary_sample, $ARGS_OPT(2));

  $const_set(mrb, $obj_value(random), $intern_lit(mrb, "DEFAULT"),
          $obj_new(mrb, random, 0, NULL));
}

void $mruby_random_gem_final($state *mrb)
{
}
