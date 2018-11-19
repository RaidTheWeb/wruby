/*
** class.c - Class class
**
** See Copyright Notice in mruby.h
*/

#include <stdarg.h>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/numeric.h>
#include <mruby/proc.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <mruby/error.h>
#include <mruby/data.h>
#include <mruby/istruct.h>

KHASH_DEFINE(mt, $sym, $method_t, TRUE, kh_int_hash_func, kh_int_hash_equal)

void
$gc_mark_mt($state *mrb, struct RClass *c)
{
  khiter_t k;
  khash_t(mt) *h = c->mt;

  if (!h) return;
  for (k = kh_begin(h); k != kh_end(h); k++) {
    if (kh_exist(h, k)) {
      $method_t m = kh_value(h, k);

      if ($METHOD_PROC_P(m)) {
        struct RProc *p = $METHOD_PROC(m);
        $gc_mark(mrb, (struct RBasic*)p);
      }
    }
  }
}

size_t
$gc_mark_mt_size($state *mrb, struct RClass *c)
{
  khash_t(mt) *h = c->mt;

  if (!h) return 0;
  return kh_size(h);
}

void
$gc_free_mt($state *mrb, struct RClass *c)
{
  kh_destroy(mt, mrb, c->mt);
}

void
$class_name_class($state *mrb, struct RClass *outer, struct RClass *c, $sym id)
{
  $value name;
  $sym nsym = $intern_lit(mrb, "__classname__");

  if ($obj_iv_defined(mrb, (struct RObject*)c, nsym)) return;
  if (outer == NULL || outer == mrb->object_class) {
    name = $symbol_value(id);
  }
  else {
    name = $class_path(mrb, outer);
    if ($nil_p(name)) {      /* unnamed outer class */
      if (outer != mrb->object_class && outer != c) {
        $obj_iv_set(mrb, (struct RObject*)c, $intern_lit(mrb, "__outer__"),
                       $obj_value(outer));
      }
      return;
    }
    $str_cat_cstr(mrb, name, "::");
    $str_cat_cstr(mrb, name, $sym2name(mrb, id));
  }
  $obj_iv_set(mrb, (struct RObject*)c, nsym, name);
}

static void
setup_class($state *mrb, struct RClass *outer, struct RClass *c, $sym id)
{
  $class_name_class(mrb, outer, c, id);
  $obj_iv_set(mrb, (struct RObject*)outer, id, $obj_value(c));
}

#define make_metaclass(mrb, c) prepare_singleton_class((mrb), (struct RBasic*)(c))

static void
prepare_singleton_class($state *mrb, struct RBasic *o)
{
  struct RClass *sc, *c;

  if (o->c->tt == $TT_SCLASS) return;
  sc = (struct RClass*)$obj_alloc(mrb, $TT_SCLASS, mrb->class_class);
  sc->flags |= $FL_CLASS_IS_INHERITED;
  sc->mt = kh_init(mt, mrb);
  sc->iv = 0;
  if (o->tt == $TT_CLASS) {
    c = (struct RClass*)o;
    if (!c->super) {
      sc->super = mrb->class_class;
    }
    else {
      sc->super = c->super->c;
    }
  }
  else if (o->tt == $TT_SCLASS) {
    c = (struct RClass*)o;
    while (c->super->tt == $TT_ICLASS)
      c = c->super;
    make_metaclass(mrb, c->super);
    sc->super = c->super->c;
  }
  else {
    sc->super = o->c;
    prepare_singleton_class(mrb, (struct RBasic*)sc);
  }
  o->c = sc;
  $field_write_barrier(mrb, (struct RBasic*)o, (struct RBasic*)sc);
  $field_write_barrier(mrb, (struct RBasic*)sc, (struct RBasic*)o);
  $obj_iv_set(mrb, (struct RObject*)sc, $intern_lit(mrb, "__attached__"), $obj_value(o));
}

static struct RClass*
class_from_sym($state *mrb, struct RClass *klass, $sym id)
{
  $value c = $const_get(mrb, $obj_value(klass), id);

  $check_type(mrb, c, $TT_CLASS);
  return $class_ptr(c);
}

static struct RClass*
module_from_sym($state *mrb, struct RClass *klass, $sym id)
{
  $value c = $const_get(mrb, $obj_value(klass), id);

  $check_type(mrb, c, $TT_MODULE);
  return $class_ptr(c);
}

static $bool
class_ptr_p($value obj)
{
  switch ($type(obj)) {
  case $TT_CLASS:
  case $TT_SCLASS:
  case $TT_MODULE:
    return TRUE;
  default:
    return FALSE;
  }
}

static void
check_if_class_or_module($state *mrb, $value obj)
{
  if (!class_ptr_p(obj)) {
    $raisef(mrb, E_TYPE_ERROR, "%S is not a class/module", $inspect(mrb, obj));
  }
}

static struct RClass*
define_module($state *mrb, $sym name, struct RClass *outer)
{
  struct RClass *m;

  if ($const_defined_at(mrb, $obj_value(outer), name)) {
    return module_from_sym(mrb, outer, name);
  }
  m = $module_new(mrb);
  setup_class(mrb, outer, m, name);

  return m;
}

$API struct RClass*
$define_module_id($state *mrb, $sym name)
{
  return define_module(mrb, name, mrb->object_class);
}

$API struct RClass*
$define_module($state *mrb, const char *name)
{
  return define_module(mrb, $intern_cstr(mrb, name), mrb->object_class);
}

$API struct RClass*
$vm_define_module($state *mrb, $value outer, $sym id)
{
  check_if_class_or_module(mrb, outer);
  if ($const_defined_at(mrb, outer, id)) {
    $value old = $const_get(mrb, outer, id);

    if ($type(old) != $TT_MODULE) {
      $raisef(mrb, E_TYPE_ERROR, "%S is not a module", $inspect(mrb, old));
    }
    return $class_ptr(old);
  }
  return define_module(mrb, id, $class_ptr(outer));
}

$API struct RClass*
$define_module_under($state *mrb, struct RClass *outer, const char *name)
{
  $sym id = $intern_cstr(mrb, name);
  struct RClass * c = define_module(mrb, id, outer);

  setup_class(mrb, outer, c, id);
  return c;
}

static struct RClass*
find_origin(struct RClass *c)
{
  $CLASS_ORIGIN(c);
  return c;
}

static struct RClass*
define_class($state *mrb, $sym name, struct RClass *super, struct RClass *outer)
{
  struct RClass * c;

  if ($const_defined_at(mrb, $obj_value(outer), name)) {
    c = class_from_sym(mrb, outer, name);
    $CLASS_ORIGIN(c);
    if (super && $class_real(c->super) != super) {
      $raisef(mrb, E_TYPE_ERROR, "superclass mismatch for Class %S (%S not %S)",
                 $sym2str(mrb, name),
                 $obj_value(c->super), $obj_value(super));
    }
    return c;
  }

  c = $class_new(mrb, super);
  setup_class(mrb, outer, c, name);

  return c;
}

$API struct RClass*
$define_class_id($state *mrb, $sym name, struct RClass *super)
{
  if (!super) {
    $warn(mrb, "no super class for '%S', Object assumed", $sym2str(mrb, name));
  }
  return define_class(mrb, name, super, mrb->object_class);
}

$API struct RClass*
$define_class($state *mrb, const char *name, struct RClass *super)
{
  return $define_class_id(mrb, $intern_cstr(mrb, name), super);
}

static $value $bob_init($state *mrb, $value);
#ifdef $METHOD_CACHE
static void mc_clear_all($state *mrb);
static void mc_clear_by_class($state *mrb, struct RClass*);
static void mc_clear_by_id($state *mrb, struct RClass*, $sym);
#else
#define mc_clear_all(mrb)
#define mc_clear_by_class(mrb,c)
#define mc_clear_by_id(mrb,c,s)
#endif

static void
$class_inherited($state *mrb, struct RClass *super, struct RClass *klass)
{
  $value s;
  $sym mid;

  if (!super)
    super = mrb->object_class;
  super->flags |= $FL_CLASS_IS_INHERITED;
  s = $obj_value(super);
  mc_clear_by_class(mrb, klass);
  mid = $intern_lit(mrb, "inherited");
  if (!$func_basic_p(mrb, s, mid, $bob_init)) {
    $value c = $obj_value(klass);
    $funcall_argv(mrb, s, mid, 1, &c);
  }
}

$API struct RClass*
$vm_define_class($state *mrb, $value outer, $value super, $sym id)
{
  struct RClass *s;
  struct RClass *c;

  if (!$nil_p(super)) {
    if ($type(super) != $TT_CLASS) {
      $raisef(mrb, E_TYPE_ERROR, "superclass must be a Class (%S given)",
                 $inspect(mrb, super));
    }
    s = $class_ptr(super);
  }
  else {
    s = 0;
  }
  check_if_class_or_module(mrb, outer);
  if ($const_defined_at(mrb, outer, id)) {
    $value old = $const_get(mrb, outer, id);

    if ($type(old) != $TT_CLASS) {
      $raisef(mrb, E_TYPE_ERROR, "%S is not a class", $inspect(mrb, old));
    }
    c = $class_ptr(old);
    if (s) {
      /* check super class */
      if ($class_real(c->super) != s) {
        $raisef(mrb, E_TYPE_ERROR, "superclass mismatch for class %S", old);
      }
    }
    return c;
  }
  c = define_class(mrb, id, s, $class_ptr(outer));
  $class_inherited(mrb, $class_real(c->super), c);

  return c;
}

$API $bool
$class_defined($state *mrb, const char *name)
{
  $value sym = $check_intern_cstr(mrb, name);
  if ($nil_p(sym)) {
    return FALSE;
  }
  return $const_defined(mrb, $obj_value(mrb->object_class), $symbol(sym));
}

$API $bool
$class_defined_under($state *mrb, struct RClass *outer, const char *name)
{
  $value sym = $check_intern_cstr(mrb, name);
  if ($nil_p(sym)) {
    return FALSE;
  }
  return $const_defined_at(mrb, $obj_value(outer), $symbol(sym));
}

$API struct RClass*
$class_get_under($state *mrb, struct RClass *outer, const char *name)
{
  return class_from_sym(mrb, outer, $intern_cstr(mrb, name));
}

$API struct RClass*
$class_get($state *mrb, const char *name)
{
  return $class_get_under(mrb, mrb->object_class, name);
}

$API struct RClass*
$exc_get($state *mrb, const char *name)
{
  struct RClass *exc, *e;
  $value c = $const_get(mrb, $obj_value(mrb->object_class),
                              $intern_cstr(mrb, name));

  if ($type(c) != $TT_CLASS) {
    $raise(mrb, mrb->eException_class, "exception corrupted");
  }
  exc = e = $class_ptr(c);

  while (e) {
    if (e == mrb->eException_class)
      return exc;
    e = e->super;
  }
  return mrb->eException_class;
}

$API struct RClass*
$module_get_under($state *mrb, struct RClass *outer, const char *name)
{
  return module_from_sym(mrb, outer, $intern_cstr(mrb, name));
}

$API struct RClass*
$module_get($state *mrb, const char *name)
{
  return $module_get_under(mrb, mrb->object_class, name);
}

/*!
 * Defines a class under the namespace of \a outer.
 * \param outer  a class which contains the new class.
 * \param id     name of the new class
 * \param super  a class from which the new class will derive.
 *               NULL means \c Object class.
 * \return the created class
 * \throw TypeError if the constant name \a name is already taken but
 *                  the constant is not a \c Class.
 * \throw NameError if the class is already defined but the class can not
 *                  be reopened because its superclass is not \a super.
 * \post top-level constant named \a name refers the returned class.
 *
 * \note if a class named \a name is already defined and its superclass is
 *       \a super, the function just returns the defined class.
 */
$API struct RClass*
$define_class_under($state *mrb, struct RClass *outer, const char *name, struct RClass *super)
{
  $sym id = $intern_cstr(mrb, name);
  struct RClass * c;

#if 0
  if (!super) {
    $warn(mrb, "no super class for '%S::%S', Object assumed",
             $obj_value(outer), $sym2str(mrb, id));
  }
#endif
  c = define_class(mrb, id, super, outer);
  setup_class(mrb, outer, c, id);
  return c;
}

$API void
$define_method_raw($state *mrb, struct RClass *c, $sym mid, $method_t m)
{
  khash_t(mt) *h;
  khiter_t k;
  $CLASS_ORIGIN(c);
  h = c->mt;

  if ($FROZEN_P(c)) {
    if (c->tt == $TT_MODULE)
      $raise(mrb, E_FROZEN_ERROR, "can't modify frozen module");
    else
      $raise(mrb, E_FROZEN_ERROR, "can't modify frozen class");
  }
  if (!h) h = c->mt = kh_init(mt, mrb);
  k = kh_put(mt, mrb, h, mid);
  kh_value(h, k) = m;
  if ($METHOD_PROC_P(m) && !$METHOD_UNDEF_P(m)) {
    struct RProc *p = $METHOD_PROC(m);

    p->flags |= $PROC_SCOPE;
    p->c = NULL;
    $field_write_barrier(mrb, (struct RBasic*)c, (struct RBasic*)p);
    if (!$PROC_ENV_P(p)) {
      $PROC_SET_TARGET_CLASS(p, c);
    }
  }
  mc_clear_by_id(mrb, c, mid);
}

$API void
$define_method_id($state *mrb, struct RClass *c, $sym mid, $func_t func, $aspec aspec)
{
  $method_t m;
  int ai = $gc_arena_save(mrb);

  $METHOD_FROM_FUNC(m, func);
  $define_method_raw(mrb, c, mid, m);
  $gc_arena_restore(mrb, ai);
}

$API void
$define_method($state *mrb, struct RClass *c, const char *name, $func_t func, $aspec aspec)
{
  $define_method_id(mrb, c, $intern_cstr(mrb, name), func, aspec);
}

/* a function to raise NotImplementedError with current method name */
$API void
$notimplement($state *mrb)
{
  const char *str;
  $int len;
  $callinfo *ci = mrb->c->ci;

  if (ci->mid) {
    str = $sym2name_len(mrb, ci->mid, &len);
    $raisef(mrb, E_NOTIMP_ERROR,
      "%S() function is unimplemented on this machine",
      $str_new_static(mrb, str, (size_t)len));
  }
}

/* a function to be replacement of unimplemented method */
$API $value
$notimplement_m($state *mrb, $value self)
{
  $notimplement(mrb);
  /* not reached */
  return $nil_value();
}

static $value
check_type($state *mrb, $value val, enum $vtype t, const char *c, const char *m)
{
  $value tmp;

  tmp = $check_convert_type(mrb, val, t, c, m);
  if ($nil_p(tmp)) {
    $raisef(mrb, E_TYPE_ERROR, "expected %S", $str_new_cstr(mrb, c));
  }
  return tmp;
}

static $value
to_str($state *mrb, $value val)
{
  return check_type(mrb, val, $TT_STRING, "String", "to_str");
}

static $value
to_ary($state *mrb, $value val)
{
  return check_type(mrb, val, $TT_ARRAY, "Array", "to_ary");
}

static $value
to_hash($state *mrb, $value val)
{
  return check_type(mrb, val, $TT_HASH, "Hash", "to_hash");
}

#define to_sym(mrb, ss) $obj_to_sym(mrb, ss)

$API $int
$get_argc($state *mrb)
{
  $int argc = mrb->c->ci->argc;

  if (argc < 0) {
    struct RArray *a = $ary_ptr(mrb->c->stack[1]);

    argc = ARY_LEN(a);
  }
  return argc;
}

$API $value*
$get_argv($state *mrb)
{
  $int argc = mrb->c->ci->argc;
  $value *array_argv;
  if (argc < 0) {
    struct RArray *a = $ary_ptr(mrb->c->stack[1]);

    array_argv = ARY_PTR(a);
  }
  else {
    array_argv = NULL;
  }
  return array_argv;
}

/*
  retrieve arguments from $state.

  $get_args(mrb, format, ...)

  returns number of arguments parsed.

  format specifiers:

    string  mruby type     C type                 note
    ----------------------------------------------------------------------------------------------
    o:      Object         [$value]
    C:      class/module   [$value]
    S:      String         [$value]            when ! follows, the value may be nil
    A:      Array          [$value]            when ! follows, the value may be nil
    H:      Hash           [$value]            when ! follows, the value may be nil
    s:      String         [char*,$int]        Receive two arguments; s! gives (NULL,0) for nil
    z:      String         [char*]                NUL terminated string; z! gives NULL for nil
    a:      Array          [$value*,$int]   Receive two arguments; a! gives (NULL,0) for nil
    f:      Float          [$float]
    i:      Integer        [$int]
    b:      Boolean        [$bool]
    n:      Symbol         [$sym]
    d:      Data           [void*,$data_type const] 2nd argument will be used to check data type so it won't be modified
    I:      Inline struct  [void*]
    &:      Block          [$value]            &! raises exception if no block given
    *:      rest argument  [$value*,$int]   The rest of the arguments as an array; *! avoid copy of the stack
    |:      optional                              Following arguments are optional
    ?:      optional given [$bool]             true if preceding argument (optional) is given
 */
$API $int
$get_args($state *mrb, const char *format, ...)
{
  const char *fmt = format;
  char c;
  $int i = 0;
  va_list ap;
  $int argc = $get_argc(mrb);
  $int arg_i = 0;
  $value *array_argv = $get_argv(mrb);
  $bool opt = FALSE;
  $bool opt_skip = TRUE;
  $bool given = TRUE;

  va_start(ap, format);

#define ARGV \
  (array_argv ? array_argv : (mrb->c->stack + 1))

  while ((c = *fmt++)) {
    switch (c) {
    case '|':
      opt = TRUE;
      break;
    case '*':
      opt_skip = FALSE;
      goto check_exit;
    case '!':
      break;
    case '&': case '?':
      if (opt) opt_skip = FALSE;
      break;
    default:
      break;
    }
  }

 check_exit:
  opt = FALSE;
  i = 0;
  while ((c = *format++)) {
    switch (c) {
    case '|': case '*': case '&': case '?':
      break;
    default:
      if (argc <= i) {
        if (opt) {
          given = FALSE;
        }
        else {
          $raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
        }
      }
      break;
    }

    switch (c) {
    case 'o':
      {
        $value *p;

        p = va_arg(ap, $value*);
        if (i < argc) {
          *p = ARGV[arg_i++];
          i++;
        }
      }
      break;
    case 'C':
      {
        $value *p;

        p = va_arg(ap, $value*);
        if (i < argc) {
          $value ss;

          ss = ARGV[arg_i++];
          if (!class_ptr_p(ss)) {
            $raisef(mrb, E_TYPE_ERROR, "%S is not class/module", ss);
          }
          *p = ss;
          i++;
        }
      }
      break;
    case 'S':
      {
        $value *p;

        p = va_arg(ap, $value*);
        if (*format == '!') {
          format++;
          if (i < argc && $nil_p(ARGV[arg_i])) {
            *p = ARGV[arg_i++];
            i++;
            break;
          }
        }
        if (i < argc) {
          *p = to_str(mrb, ARGV[arg_i++]);
          i++;
        }
      }
      break;
    case 'A':
      {
        $value *p;

        p = va_arg(ap, $value*);
        if (*format == '!') {
          format++;
          if (i < argc && $nil_p(ARGV[arg_i])) {
            *p = ARGV[arg_i++];
            i++;
            break;
          }
        }
        if (i < argc) {
          *p = to_ary(mrb, ARGV[arg_i++]);
          i++;
        }
      }
      break;
    case 'H':
      {
        $value *p;

        p = va_arg(ap, $value*);
        if (*format == '!') {
          format++;
          if (i < argc && $nil_p(ARGV[arg_i])) {
            *p = ARGV[arg_i++];
            i++;
            break;
          }
        }
        if (i < argc) {
          *p = to_hash(mrb, ARGV[arg_i++]);
          i++;
        }
      }
      break;
    case 's':
      {
        $value ss;
        char **ps = 0;
        $int *pl = 0;

        ps = va_arg(ap, char**);
        pl = va_arg(ap, $int*);
        if (*format == '!') {
          format++;
          if (i < argc && $nil_p(ARGV[arg_i])) {
            *ps = NULL;
            *pl = 0;
            i++; arg_i++;
            break;
          }
        }
        if (i < argc) {
          ss = to_str(mrb, ARGV[arg_i++]);
          *ps = RSTRING_PTR(ss);
          *pl = RSTRING_LEN(ss);
          i++;
        }
      }
      break;
    case 'z':
      {
        $value ss;
        const char **ps;

        ps = va_arg(ap, const char**);
        if (*format == '!') {
          format++;
          if (i < argc && $nil_p(ARGV[arg_i])) {
            *ps = NULL;
            i++; arg_i++;
            break;
          }
        }
        if (i < argc) {
          ss = to_str(mrb, ARGV[arg_i++]);
          *ps = $string_value_cstr(mrb, &ss);
          i++;
        }
      }
      break;
    case 'a':
      {
        $value aa;
        struct RArray *a;
        $value **pb;
        $int *pl;

        pb = va_arg(ap, $value**);
        pl = va_arg(ap, $int*);
        if (*format == '!') {
          format++;
          if (i < argc && $nil_p(ARGV[arg_i])) {
            *pb = 0;
            *pl = 0;
            i++; arg_i++;
            break;
          }
        }
        if (i < argc) {
          aa = to_ary(mrb, ARGV[arg_i++]);
          a = $ary_ptr(aa);
          *pb = ARY_PTR(a);
          *pl = ARY_LEN(a);
          i++;
        }
      }
      break;
    case 'I':
      {
        void* *p;
        $value ss;

        p = va_arg(ap, void**);
        if (i < argc) {
          ss = ARGV[arg_i];
          if ($type(ss) != $TT_ISTRUCT)
          {
            $raisef(mrb, E_TYPE_ERROR, "%S is not inline struct", ss);
          }
          *p = $istruct_ptr(ss);
          arg_i++;
          i++;
        }
      }
      break;
#ifndef $WITHOUT_FLOAT
    case 'f':
      {
        $float *p;

        p = va_arg(ap, $float*);
        if (i < argc) {
          *p = $to_flo(mrb, ARGV[arg_i]);
          arg_i++;
          i++;
        }
      }
      break;
#endif
    case 'i':
      {
        $int *p;

        p = va_arg(ap, $int*);
        if (i < argc) {
          switch ($type(ARGV[arg_i])) {
            case $TT_FIXNUM:
              *p = $fixnum(ARGV[arg_i]);
              break;
#ifndef $WITHOUT_FLOAT
            case $TT_FLOAT:
              {
                $float f = $float(ARGV[arg_i]);

                if (!FIXABLE_FLOAT(f)) {
                  $raise(mrb, E_RANGE_ERROR, "float too big for int");
                }
                *p = ($int)f;
              }
              break;
#endif
            case $TT_STRING:
              $raise(mrb, E_TYPE_ERROR, "no implicit conversion of String into Integer");
              break;
            default:
              *p = $fixnum($Integer(mrb, ARGV[arg_i]));
              break;
          }
          arg_i++;
          i++;
        }
      }
      break;
    case 'b':
      {
        $bool *boolp = va_arg(ap, $bool*);

        if (i < argc) {
          $value b = ARGV[arg_i++];
          *boolp = $test(b);
          i++;
        }
      }
      break;
    case 'n':
      {
        $sym *symp;

        symp = va_arg(ap, $sym*);
        if (i < argc) {
          $value ss;

          ss = ARGV[arg_i++];
          *symp = to_sym(mrb, ss);
          i++;
        }
      }
      break;
    case 'd':
      {
        void** datap;
        struct $data_type const* type;

        datap = va_arg(ap, void**);
        type = va_arg(ap, struct $data_type const*);
        if (*format == '!') {
          format++;
          if (i < argc && $nil_p(ARGV[arg_i])) {
            *datap = 0;
            i++; arg_i++;
            break;
          }
        }
        if (i < argc) {
          *datap = $data_get_ptr(mrb, ARGV[arg_i++], type);
          ++i;
        }
      }
      break;

    case '&':
      {
        $value *p, *bp;

        p = va_arg(ap, $value*);
        if (mrb->c->ci->argc < 0) {
          bp = mrb->c->stack + 2;
        }
        else {
          bp = mrb->c->stack + mrb->c->ci->argc + 1;
        }
        if (*format == '!') {
          format ++;
          if ($nil_p(*bp)) {
            $raise(mrb, E_ARGUMENT_ERROR, "no block given");
          }
        }
        *p = *bp;
      }
      break;
    case '|':
      if (opt_skip && i == argc) return argc;
      opt = TRUE;
      break;
    case '?':
      {
        $bool *p;

        p = va_arg(ap, $bool*);
        *p = given;
      }
      break;

    case '*':
      {
        $value **var;
        $int *pl;
        $bool nocopy = array_argv ? TRUE : FALSE;

        if (*format == '!') {
          format++;
          nocopy = TRUE;
        }
        var = va_arg(ap, $value**);
        pl = va_arg(ap, $int*);
        if (argc > i) {
          *pl = argc-i;
          if (*pl > 0) {
            if (nocopy) {
              *var = ARGV+arg_i;
            }
            else {
              $value args = $ary_new_from_values(mrb, *pl, ARGV+arg_i);
              RARRAY(args)->c = NULL;
              *var = RARRAY_PTR(args);
            }
          }
          i = argc;
          arg_i += *pl;
        }
        else {
          *pl = 0;
          *var = NULL;
        }
      }
      break;
    default:
      $raisef(mrb, E_ARGUMENT_ERROR, "invalid argument specifier %S", $str_new(mrb, &c, 1));
      break;
    }
  }

#undef ARGV

  if (!c && argc > i) {
    $raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
  }
  va_end(ap);
  return i;
}

static struct RClass*
boot_defclass($state *mrb, struct RClass *super)
{
  struct RClass *c;

  c = (struct RClass*)$obj_alloc(mrb, $TT_CLASS, mrb->class_class);
  if (super) {
    c->super = super;
    $field_write_barrier(mrb, (struct RBasic*)c, (struct RBasic*)super);
  }
  else {
    c->super = mrb->object_class;
  }
  c->mt = kh_init(mt, mrb);
  return c;
}

static void
boot_initmod($state *mrb, struct RClass *mod)
{
  if (!mod->mt) {
    mod->mt = kh_init(mt, mrb);
  }
}

static struct RClass*
include_class_new($state *mrb, struct RClass *m, struct RClass *super)
{
  struct RClass *ic = (struct RClass*)$obj_alloc(mrb, $TT_ICLASS, mrb->class_class);
  if (m->tt == $TT_ICLASS) {
    m = m->c;
  }
  $CLASS_ORIGIN(m);
  ic->iv = m->iv;
  ic->mt = m->mt;
  ic->super = super;
  if (m->tt == $TT_ICLASS) {
    ic->c = m->c;
  }
  else {
    ic->c = m;
  }
  return ic;
}

static int
include_module_at($state *mrb, struct RClass *c, struct RClass *ins_pos, struct RClass *m, int search_super)
{
  struct RClass *p, *ic;
  void *klass_mt = find_origin(c)->mt;

  while (m) {
    int superclass_seen = 0;

    if (m->flags & $FL_CLASS_IS_PREPENDED)
      goto skip;

    if (klass_mt && klass_mt == m->mt)
      return -1;

    p = c->super;
    while (p) {
      if (p->tt == $TT_ICLASS) {
        if (p->mt == m->mt) {
          if (!superclass_seen) {
            ins_pos = p; /* move insert point */
          }
          goto skip;
        }
      } else if (p->tt == $TT_CLASS) {
        if (!search_super) break;
        superclass_seen = 1;
      }
      p = p->super;
    }

    ic = include_class_new(mrb, m, ins_pos->super);
    m->flags |= $FL_CLASS_IS_INHERITED;
    ins_pos->super = ic;
    $field_write_barrier(mrb, (struct RBasic*)ins_pos, (struct RBasic*)ic);
    mc_clear_by_class(mrb, ins_pos);
    ins_pos = ic;
  skip:
    m = m->super;
  }
  mc_clear_all(mrb);
  return 0;
}

$API void
$include_module($state *mrb, struct RClass *c, struct RClass *m)
{
  int changed = include_module_at(mrb, c, find_origin(c), m, 1);
  if (changed < 0) {
    $raise(mrb, E_ARGUMENT_ERROR, "cyclic include detected");
  }
}

$API void
$prepend_module($state *mrb, struct RClass *c, struct RClass *m)
{
  struct RClass *origin;
  int changed = 0;

  if (!(c->flags & $FL_CLASS_IS_PREPENDED)) {
    origin = (struct RClass*)$obj_alloc(mrb, $TT_ICLASS, c);
    origin->flags |= $FL_CLASS_IS_ORIGIN | $FL_CLASS_IS_INHERITED;
    origin->super = c->super;
    c->super = origin;
    origin->mt = c->mt;
    c->mt = kh_init(mt, mrb);
    $field_write_barrier(mrb, (struct RBasic*)c, (struct RBasic*)origin);
    c->flags |= $FL_CLASS_IS_PREPENDED;
  }
  changed = include_module_at(mrb, c, c, m, 0);
  if (changed < 0) {
    $raise(mrb, E_ARGUMENT_ERROR, "cyclic prepend detected");
  }
}

static $value
$mod_prepend_features($state *mrb, $value mod)
{
  $value klass;

  $check_type(mrb, mod, $TT_MODULE);
  $get_args(mrb, "C", &klass);
  $prepend_module(mrb, $class_ptr(klass), $class_ptr(mod));
  return mod;
}

static $value
$mod_append_features($state *mrb, $value mod)
{
  $value klass;

  $check_type(mrb, mod, $TT_MODULE);
  $get_args(mrb, "C", &klass);
  $include_module(mrb, $class_ptr(klass), $class_ptr(mod));
  return mod;
}

/* 15.2.2.4.28 */
/*
 *  call-seq:
 *     mod.include?(module)    -> true or false
 *
 *  Returns <code>true</code> if <i>module</i> is included in
 *  <i>mod</i> or one of <i>mod</i>'s ancestors.
 *
 *     module A
 *     end
 *     class B
 *       include A
 *     end
 *     class C < B
 *     end
 *     B.include?(A)   #=> true
 *     C.include?(A)   #=> true
 *     A.include?(A)   #=> false
 */
static $value
$mod_include_p($state *mrb, $value mod)
{
  $value mod2;
  struct RClass *c = $class_ptr(mod);

  $get_args(mrb, "C", &mod2);
  $check_type(mrb, mod2, $TT_MODULE);

  while (c) {
    if (c->tt == $TT_ICLASS) {
      if (c->c == $class_ptr(mod2)) return $true_value();
    }
    c = c->super;
  }
  return $false_value();
}

static $value
$mod_ancestors($state *mrb, $value self)
{
  $value result;
  struct RClass *c = $class_ptr(self);
  result = $ary_new(mrb);
  while (c) {
    if (c->tt == $TT_ICLASS) {
      $ary_push(mrb, result, $obj_value(c->c));
    }
    else if (!(c->flags & $FL_CLASS_IS_PREPENDED)) {
      $ary_push(mrb, result, $obj_value(c));
    }
    c = c->super;
  }

  return result;
}

static $value
$mod_extend_object($state *mrb, $value mod)
{
  $value obj;

  $check_type(mrb, mod, $TT_MODULE);
  $get_args(mrb, "o", &obj);
  $include_module(mrb, $class_ptr($singleton_class(mrb, obj)), $class_ptr(mod));
  return mod;
}

static $value
$mod_initialize($state *mrb, $value mod)
{
  $value b;
  struct RClass *m = $class_ptr(mod);
  boot_initmod(mrb, m); /* bootstrap a newly initialized module */
  $get_args(mrb, "|&", &b);
  if (!$nil_p(b)) {
    $yield_with_class(mrb, b, 1, &mod, mod, m);
  }
  return mod;
}

/* implementation of module_eval/class_eval */
$value $mod_module_eval($state*, $value);

static $value
$mod_dummy_visibility($state *mrb, $value mod)
{
  return mod;
}

$API $value
$singleton_class($state *mrb, $value v)
{
  struct RBasic *obj;

  switch ($type(v)) {
  case $TT_FALSE:
    if ($nil_p(v))
      return $obj_value(mrb->nil_class);
    return $obj_value(mrb->false_class);
  case $TT_TRUE:
    return $obj_value(mrb->true_class);
  case $TT_CPTR:
    return $obj_value(mrb->object_class);
  case $TT_SYMBOL:
  case $TT_FIXNUM:
#ifndef $WITHOUT_FLOAT
  case $TT_FLOAT:
#endif
    $raise(mrb, E_TYPE_ERROR, "can't define singleton");
    return $nil_value();    /* not reached */
  default:
    break;
  }
  obj = $basic_ptr(v);
  prepare_singleton_class(mrb, obj);
  return $obj_value(obj->c);
}

$API void
$define_singleton_method($state *mrb, struct RObject *o, const char *name, $func_t func, $aspec aspec)
{
  prepare_singleton_class(mrb, (struct RBasic*)o);
  $define_method_id(mrb, o->c, $intern_cstr(mrb, name), func, aspec);
}

$API void
$define_class_method($state *mrb, struct RClass *c, const char *name, $func_t func, $aspec aspec)
{
  $define_singleton_method(mrb, (struct RObject*)c, name, func, aspec);
}

$API void
$define_module_function($state *mrb, struct RClass *c, const char *name, $func_t func, $aspec aspec)
{
  $define_class_method(mrb, c, name, func, aspec);
  $define_method(mrb, c, name, func, aspec);
}

#ifdef $METHOD_CACHE
static void
mc_clear_all($state *mrb)
{
  struct $cache_entry *mc = mrb->cache;
  int i;

  for (i=0; i<$METHOD_CACHE_SIZE; i++) {
    mc[i].c = 0;
  }
}

static void
mc_clear_by_class($state *mrb, struct RClass *c)
{
  struct $cache_entry *mc = mrb->cache;
  int i;

  if (c->flags & $FL_CLASS_IS_INHERITED) {
    mc_clear_all(mrb);
    c->flags &= ~$FL_CLASS_IS_INHERITED;
    return;
  }
  for (i=0; i<$METHOD_CACHE_SIZE; i++) {
    if (mc[i].c == c) mc[i].c = 0;
  }
}

static void
mc_clear_by_id($state *mrb, struct RClass *c, $sym mid)
{
  struct $cache_entry *mc = mrb->cache;
  int i;

  if (c->flags & $FL_CLASS_IS_INHERITED) {
    mc_clear_all(mrb);
    c->flags &= ~$FL_CLASS_IS_INHERITED;
    return;
  }
  for (i=0; i<$METHOD_CACHE_SIZE; i++) {
    if (mc[i].c == c || mc[i].mid == mid)
      mc[i].c = 0;
  }
}
#endif

$API $method_t
$method_search_vm($state *mrb, struct RClass **cp, $sym mid)
{
  khiter_t k;
  $method_t m;
  struct RClass *c = *cp;
#ifdef $METHOD_CACHE
  struct RClass *oc = c;
  int h = kh_int_hash_func(mrb, ((intptr_t)oc) ^ mid) & ($METHOD_CACHE_SIZE-1);
  struct $cache_entry *mc = &mrb->cache[h];

  if (mc->c == c && mc->mid == mid) {
    *cp = mc->c0;
    return mc->m;
  }
#endif

  while (c) {
    khash_t(mt) *h = c->mt;

    if (h) {
      k = kh_get(mt, mrb, h, mid);
      if (k != kh_end(h)) {
        m = kh_value(h, k);
        if ($METHOD_UNDEF_P(m)) break;
        *cp = c;
#ifdef $METHOD_CACHE
        mc->c = oc;
        mc->c0 = c;
        mc->mid = mid;
        mc->m = m;
#endif
        return m;
      }
    }
    c = c->super;
  }
  $METHOD_FROM_PROC(m, NULL);
  return m;                  /* no method */
}

$API $method_t
$method_search($state *mrb, struct RClass* c, $sym mid)
{
  $method_t m;

  m = $method_search_vm(mrb, &c, mid);
  if ($METHOD_UNDEF_P(m)) {
    $value inspect = $funcall(mrb, $obj_value(c), "inspect", 0);
    if ($string_p(inspect) && RSTRING_LEN(inspect) > 64) {
      inspect = $any_to_s(mrb, $obj_value(c));
    }
    $name_error(mrb, mid, "undefined method '%S' for class %S",
               $sym2str(mrb, mid), inspect);
  }
  return m;
}

static $value
attr_reader($state *mrb, $value obj)
{
  $value name = $proc_cfunc_env_get(mrb, 0);
  return $iv_get(mrb, obj, to_sym(mrb, name));
}

static $value
$mod_attr_reader($state *mrb, $value mod)
{
  struct RClass *c = $class_ptr(mod);
  $value *argv;
  $int argc, i;
  int ai;

  $get_args(mrb, "*", &argv, &argc);
  ai = $gc_arena_save(mrb);
  for (i=0; i<argc; i++) {
    $value name, str;
    $sym method, sym;
    struct RProc *p;
    $method_t m;

    method = to_sym(mrb, argv[i]);
    name = $sym2str(mrb, method);
    str = $str_new_capa(mrb, RSTRING_LEN(name)+1);
    $str_cat_lit(mrb, str, "@");
    $str_cat_str(mrb, str, name);
    sym = $intern_str(mrb, str);
    $iv_name_sym_check(mrb, sym);
    name = $symbol_value(sym);
    p = $proc_new_cfunc_with_env(mrb, attr_reader, 1, &name);
    $METHOD_FROM_PROC(m, p);
    $define_method_raw(mrb, c, method, m);
    $gc_arena_restore(mrb, ai);
  }
  return $nil_value();
}

static $value
attr_writer($state *mrb, $value obj)
{
  $value name = $proc_cfunc_env_get(mrb, 0);
  $value val;

  $get_args(mrb, "o", &val);
  $iv_set(mrb, obj, to_sym(mrb, name), val);
  return val;
}

static $value
$mod_attr_writer($state *mrb, $value mod)
{
  struct RClass *c = $class_ptr(mod);
  $value *argv;
  $int argc, i;
  int ai;

  $get_args(mrb, "*", &argv, &argc);
  ai = $gc_arena_save(mrb);
  for (i=0; i<argc; i++) {
    $value name, str, attr;
    $sym method, sym;
    struct RProc *p;
    $method_t m;

    method = to_sym(mrb, argv[i]);

    /* prepare iv name (@name) */
    name = $sym2str(mrb, method);
    str = $str_new_capa(mrb, RSTRING_LEN(name)+1);
    $str_cat_lit(mrb, str, "@");
    $str_cat_str(mrb, str, name);
    sym = $intern_str(mrb, str);
    $iv_name_sym_check(mrb, sym);
    attr = $symbol_value(sym);

    /* prepare method name (name=) */
    str = $str_new_capa(mrb, RSTRING_LEN(str));
    $str_cat_str(mrb, str, name);
    $str_cat_lit(mrb, str, "=");
    method = $intern_str(mrb, str);

    p = $proc_new_cfunc_with_env(mrb, attr_writer, 1, &attr);
    $METHOD_FROM_PROC(m, p);
    $define_method_raw(mrb, c, method, m);
    $gc_arena_restore(mrb, ai);
  }
  return $nil_value();
}

static $value
$instance_alloc($state *mrb, $value cv)
{
  struct RClass *c = $class_ptr(cv);
  struct RObject *o;
  enum $vtype ttype = $INSTANCE_TT(c);

  if (c->tt == $TT_SCLASS)
    $raise(mrb, E_TYPE_ERROR, "can't create instance of singleton class");

  if (ttype == 0) ttype = $TT_OBJECT;
  if (ttype <= $TT_CPTR) {
    $raisef(mrb, E_TYPE_ERROR, "can't create instance of %S", cv);
  }
  o = (struct RObject*)$obj_alloc(mrb, ttype, c);
  return $obj_value(o);
}

/*
 *  call-seq:
 *     class.new(args, ...)    ->  obj
 *
 *  Creates a new object of <i>class</i>'s class, then
 *  invokes that object's <code>initialize</code> method,
 *  passing it <i>args</i>. This is the method that ends
 *  up getting called whenever an object is constructed using
 *  `.new`.
 *
 */

$API $value
$instance_new($state *mrb, $value cv)
{
  $value obj, blk;
  $value *argv;
  $int argc;
  $sym init;
  $method_t m;

  $get_args(mrb, "*&", &argv, &argc, &blk);
  obj = $instance_alloc(mrb, cv);
  init = $intern_lit(mrb, "initialize");
  m = $method_search(mrb, $class(mrb, obj), init);
  if ($METHOD_CFUNC_P(m)) {
    $func_t f = $METHOD_CFUNC(m);
    if (f != $bob_init) {
      f(mrb, obj);
    }
  }
  else {
    $funcall_with_block(mrb, obj, init, argc, argv, blk);
  }

  return obj;
}

$API $value
$obj_new($state *mrb, struct RClass *c, $int argc, const $value *argv)
{
  $value obj;
  $sym mid;

  obj = $instance_alloc(mrb, $obj_value(c));
  mid = $intern_lit(mrb, "initialize");
  if (!$func_basic_p(mrb, obj, mid, $bob_init)) {
    $funcall_argv(mrb, obj, mid, argc, argv);
  }
  return obj;
}

static $value
$class_initialize($state *mrb, $value c)
{
  $value a, b;

  $get_args(mrb, "|C&", &a, &b);
  if (!$nil_p(b)) {
    $yield_with_class(mrb, b, 1, &c, c, $class_ptr(c));
  }
  return c;
}

static $value
$class_new_class($state *mrb, $value cv)
{
  $int n;
  $value super, blk;
  $value new_class;
  $sym mid;

  n = $get_args(mrb, "|C&", &super, &blk);
  if (n == 0) {
    super = $obj_value(mrb->object_class);
  }
  new_class = $obj_value($class_new(mrb, $class_ptr(super)));
  mid = $intern_lit(mrb, "initialize");
  if (!$func_basic_p(mrb, new_class, mid, $bob_init)) {
    $funcall_with_block(mrb, new_class, mid, n, &super, blk);
  }
  $class_inherited(mrb, $class_ptr(super), $class_ptr(new_class));
  return new_class;
}

static $value
$class_superclass($state *mrb, $value klass)
{
  struct RClass *c;

  c = $class_ptr(klass);
  c = find_origin(c)->super;
  while (c && c->tt == $TT_ICLASS) {
    c = find_origin(c)->super;
  }
  if (!c) return $nil_value();
  return $obj_value(c);
}

static $value
$bob_init($state *mrb, $value cv)
{
  return $nil_value();
}

static $value
$bob_not($state *mrb, $value cv)
{
  return $bool_value(!$test(cv));
}

/* 15.3.1.3.1  */
/* 15.3.1.3.10 */
/* 15.3.1.3.11 */
/*
 *  call-seq:
 *     obj == other        -> true or false
 *     obj.equal?(other)   -> true or false
 *     obj.eql?(other)     -> true or false
 *
 *  Equality---At the <code>Object</code> level, <code>==</code> returns
 *  <code>true</code> only if <i>obj</i> and <i>other</i> are the
 *  same object. Typically, this method is overridden in descendant
 *  classes to provide class-specific meaning.
 *
 *  Unlike <code>==</code>, the <code>equal?</code> method should never be
 *  overridden by subclasses: it is used to determine object identity
 *  (that is, <code>a.equal?(b)</code> iff <code>a</code> is the same
 *  object as <code>b</code>).
 *
 *  The <code>eql?</code> method returns <code>true</code> if
 *  <i>obj</i> and <i>anObject</i> have the same value. Used by
 *  <code>Hash</code> to test members for equality.  For objects of
 *  class <code>Object</code>, <code>eql?</code> is synonymous with
 *  <code>==</code>. Subclasses normally continue this tradition, but
 *  there are exceptions. <code>Numeric</code> types, for example,
 *  perform type conversion across <code>==</code>, but not across
 *  <code>eql?</code>, so:
 *
 *     1 == 1.0     #=> true
 *     1.eql? 1.0   #=> false
 */
$value
$obj_equal_m($state *mrb, $value self)
{
  $value arg;

  $get_args(mrb, "o", &arg);
  return $bool_value($obj_equal(mrb, self, arg));
}

static $value
$obj_not_equal_m($state *mrb, $value self)
{
  $value arg;

  $get_args(mrb, "o", &arg);
  return $bool_value(!$equal(mrb, self, arg));
}

$API $bool
$obj_respond_to($state *mrb, struct RClass* c, $sym mid)
{
  $method_t m;

  m = $method_search_vm(mrb, &c, mid);
  if ($METHOD_UNDEF_P(m)) {
    return FALSE;
  }
  return TRUE;
}

$API $bool
$respond_to($state *mrb, $value obj, $sym mid)
{
  return $obj_respond_to(mrb, $class(mrb, obj), mid);
}

$API $value
$class_path($state *mrb, struct RClass *c)
{
  $value path;
  $sym nsym = $intern_lit(mrb, "__classname__");

  path = $obj_iv_get(mrb, (struct RObject*)c, nsym);
  if ($nil_p(path)) {
    /* no name (yet) */
    return $class_find_path(mrb, c);
  }
  else if ($symbol_p(path)) {
    /* toplevel class/module */
    const char *str;
    $int len;

    str = $sym2name_len(mrb, $symbol(path), &len);
    return $str_new(mrb, str, len);
  }
  return $str_dup(mrb, path);
}

$API struct RClass*
$class_real(struct RClass* cl)
{
  if (cl == 0) return NULL;
  while ((cl->tt == $TT_SCLASS) || (cl->tt == $TT_ICLASS)) {
    cl = cl->super;
    if (cl == 0) return NULL;
  }
  return cl;
}

$API const char*
$class_name($state *mrb, struct RClass* c)
{
  $value path = $class_path(mrb, c);
  if ($nil_p(path)) {
    path = c->tt == $TT_MODULE ? $str_new_lit(mrb, "#<Module:") :
                                    $str_new_lit(mrb, "#<Class:");
    $str_concat(mrb, path, $ptr_to_str(mrb, c));
    $str_cat_lit(mrb, path, ">");
  }
  return RSTRING_PTR(path);
}

$API const char*
$obj_classname($state *mrb, $value obj)
{
  return $class_name(mrb, $obj_class(mrb, obj));
}

/*!
 * Ensures a class can be derived from super.
 *
 * \param super a reference to an object.
 * \exception TypeError if \a super is not a Class or \a super is a singleton class.
 */
static void
$check_inheritable($state *mrb, struct RClass *super)
{
  if (super->tt != $TT_CLASS) {
    $raisef(mrb, E_TYPE_ERROR, "superclass must be a Class (%S given)", $obj_value(super));
  }
  if (super->tt == $TT_SCLASS) {
    $raise(mrb, E_TYPE_ERROR, "can't make subclass of singleton class");
  }
  if (super == mrb->class_class) {
    $raise(mrb, E_TYPE_ERROR, "can't make subclass of Class");
  }
}

/*!
 * Creates a new class.
 * \param super     a class from which the new class derives.
 * \exception TypeError \a super is not inheritable.
 * \exception TypeError \a super is the Class class.
 */
$API struct RClass*
$class_new($state *mrb, struct RClass *super)
{
  struct RClass *c;

  if (super) {
    $check_inheritable(mrb, super);
  }
  c = boot_defclass(mrb, super);
  if (super) {
    $SET_INSTANCE_TT(c, $INSTANCE_TT(super));
  }
  make_metaclass(mrb, c);

  return c;
}

/*!
 * Creates a new module.
 */
$API struct RClass*
$module_new($state *mrb)
{
  struct RClass *m = (struct RClass*)$obj_alloc(mrb, $TT_MODULE, mrb->module_class);
  boot_initmod(mrb, m);
  return m;
}

/*
 *  call-seq:
 *     obj.class    => class
 *
 *  Returns the class of <i>obj</i>, now preferred over
 *  <code>Object#type</code>, as an object's type in Ruby is only
 *  loosely tied to that object's class. This method must always be
 *  called with an explicit receiver, as <code>class</code> is also a
 *  reserved word in Ruby.
 *
 *     1.class      #=> Fixnum
 *     self.class   #=> Object
 */

$API struct RClass*
$obj_class($state *mrb, $value obj)
{
  return $class_real($class(mrb, obj));
}

$API void
$alias_method($state *mrb, struct RClass *c, $sym a, $sym b)
{
  $method_t m = $method_search(mrb, c, b);

  $define_method_raw(mrb, c, a, m);
}

/*!
 * Defines an alias of a method.
 * \param klass  the class which the original method belongs to
 * \param name1  a new name for the method
 * \param name2  the original name of the method
 */
$API void
$define_alias($state *mrb, struct RClass *klass, const char *name1, const char *name2)
{
  $alias_method(mrb, klass, $intern_cstr(mrb, name1), $intern_cstr(mrb, name2));
}

/*
 * call-seq:
 *   mod.to_s   -> string
 *
 * Return a string representing this module or class. For basic
 * classes and modules, this is the name. For singletons, we
 * show information on the thing we're attached to as well.
 */

static $value
$mod_to_s($state *mrb, $value klass)
{
  $value str;

  if ($type(klass) == $TT_SCLASS) {
    $value v = $iv_get(mrb, klass, $intern_lit(mrb, "__attached__"));

    str = $str_new_lit(mrb, "#<Class:");

    if (class_ptr_p(v)) {
      $str_cat_str(mrb, str, $inspect(mrb, v));
    }
    else {
      $str_cat_str(mrb, str, $any_to_s(mrb, v));
    }
    return $str_cat_lit(mrb, str, ">");
  }
  else {
    struct RClass *c;
    $value path;

    str = $str_new_capa(mrb, 32);
    c = $class_ptr(klass);
    path = $class_path(mrb, c);

    if ($nil_p(path)) {
      switch ($type(klass)) {
        case $TT_CLASS:
          $str_cat_lit(mrb, str, "#<Class:");
          break;

        case $TT_MODULE:
          $str_cat_lit(mrb, str, "#<Module:");
          break;

        default:
          /* Shouldn't be happened? */
          $str_cat_lit(mrb, str, "#<??????:");
          break;
      }
      $str_concat(mrb, str, $ptr_to_str(mrb, c));
      return $str_cat_lit(mrb, str, ">");
    }
    else {
      return path;
    }
  }
}

static $value
$mod_alias($state *mrb, $value mod)
{
  struct RClass *c = $class_ptr(mod);
  $sym new_name, old_name;

  $get_args(mrb, "nn", &new_name, &old_name);
  $alias_method(mrb, c, new_name, old_name);
  return $nil_value();
}

void
$undef_method_id($state *mrb, struct RClass *c, $sym a)
{
  if (!$obj_respond_to(mrb, c, a)) {
    $name_error(mrb, a, "undefined method '%S' for class '%S'", $sym2str(mrb, a), $obj_value(c));
  }
  else {
    $method_t m;

    $METHOD_FROM_PROC(m, NULL);
    $define_method_raw(mrb, c, a, m);
  }
}

$API void
$undef_method($state *mrb, struct RClass *c, const char *name)
{
  $undef_method_id(mrb, c, $intern_cstr(mrb, name));
}

$API void
$undef_class_method($state *mrb, struct RClass *c, const char *name)
{
  $undef_method(mrb,  $class_ptr($singleton_class(mrb, $obj_value(c))), name);
}

static $value
$mod_undef($state *mrb, $value mod)
{
  struct RClass *c = $class_ptr(mod);
  $int argc;
  $value *argv;

  $get_args(mrb, "*", &argv, &argc);
  while (argc--) {
    $undef_method_id(mrb, c, to_sym(mrb, *argv));
    argv++;
  }
  return $nil_value();
}

static void
check_const_name_str($state *mrb, $value str)
{
  if (RSTRING_LEN(str) < 1 || !ISUPPER(*RSTRING_PTR(str))) {
    $name_error(mrb, $intern_str(mrb, str), "wrong constant name %S", str);
  }
}

static void
check_const_name_sym($state *mrb, $sym id)
{
  check_const_name_str(mrb, $sym2str(mrb, id));
}

static $value
$mod_const_defined($state *mrb, $value mod)
{
  $sym id;
  $bool inherit = TRUE;

  $get_args(mrb, "n|b", &id, &inherit);
  check_const_name_sym(mrb, id);
  if (inherit) {
    return $bool_value($const_defined(mrb, mod, id));
  }
  return $bool_value($const_defined_at(mrb, mod, id));
}

static $value
$const_get_sym($state *mrb, $value mod, $sym id)
{
  check_const_name_sym(mrb, id);
  return $const_get(mrb, mod, id);
}

static $value
$mod_const_get($state *mrb, $value mod)
{
  $value path;
  $sym id;
  char *ptr;
  $int off, end, len;

  $get_args(mrb, "o", &path);

  if ($symbol_p(path)) {
    /* const get with symbol */
    id = $symbol(path);
    return $const_get_sym(mrb, mod, id);
  }

  /* const get with class path string */
  path = $string_type(mrb, path);
  ptr = RSTRING_PTR(path);
  len = RSTRING_LEN(path);
  off = 0;

  while (off < len) {
    end = $str_index_lit(mrb, path, "::", off);
    end = (end == -1) ? len : end;
    id = $intern(mrb, ptr+off, end-off);
    mod = $const_get_sym(mrb, mod, id);
    if (end == len)
      off = end;
    else {
      off = end + 2;
      if (off == len) {         /* trailing "::" */
        $name_error(mrb, id, "wrong constant name '%S'", path);
      }
    }
  }

  return mod;
}

static $value
$mod_const_set($state *mrb, $value mod)
{
  $sym id;
  $value value;

  $get_args(mrb, "no", &id, &value);
  check_const_name_sym(mrb, id);
  $const_set(mrb, mod, id, value);
  return value;
}

static $value
$mod_remove_const($state *mrb, $value mod)
{
  $sym id;
  $value val;

  $get_args(mrb, "n", &id);
  check_const_name_sym(mrb, id);
  val = $iv_remove(mrb, mod, id);
  if ($undef_p(val)) {
    $name_error(mrb, id, "constant %S not defined", $sym2str(mrb, id));
  }
  return val;
}

static $value
$mod_const_missing($state *mrb, $value mod)
{
  $sym sym;

  $get_args(mrb, "n", &sym);

  if ($class_real($class_ptr(mod)) != mrb->object_class) {
    $name_error(mrb, sym, "uninitialized constant %S::%S",
                   mod,
                   $sym2str(mrb, sym));
  }
  else {
    $name_error(mrb, sym, "uninitialized constant %S",
                   $sym2str(mrb, sym));
  }
  /* not reached */
  return $nil_value();
}

/* 15.2.2.4.34 */
/*
 *  call-seq:
 *     mod.method_defined?(symbol)    -> true or false
 *
 *  Returns +true+ if the named method is defined by
 *  _mod_ (or its included modules and, if _mod_ is a class,
 *  its ancestors). Public and protected methods are matched.
 *
 *     module A
 *       def method1()  end
 *     end
 *     class B
 *       def method2()  end
 *     end
 *     class C < B
 *       include A
 *       def method3()  end
 *     end
 *
 *     A.method_defined? :method1    #=> true
 *     C.method_defined? "method1"   #=> true
 *     C.method_defined? "method2"   #=> true
 *     C.method_defined? "method3"   #=> true
 *     C.method_defined? "method4"   #=> false
 */

static $value
$mod_method_defined($state *mrb, $value mod)
{
  $sym id;

  $get_args(mrb, "n", &id);
  return $bool_value($obj_respond_to(mrb, $class_ptr(mod), id));
}

static $value
mod_define_method($state *mrb, $value self)
{
  struct RClass *c = $class_ptr(self);
  struct RProc *p;
  $method_t m;
  $sym mid;
  $value proc = $undef_value();
  $value blk;

  $get_args(mrb, "n|o&", &mid, &proc, &blk);
  switch ($type(proc)) {
    case $TT_PROC:
      blk = proc;
      break;
    case $TT_UNDEF:
      /* ignored */
      break;
    default:
      $raisef(mrb, E_TYPE_ERROR, "wrong argument type %S (expected Proc)", $obj_value($obj_class(mrb, proc)));
      break;
  }
  if ($nil_p(blk)) {
    $raise(mrb, E_ARGUMENT_ERROR, "no block given");
  }
  p = (struct RProc*)$obj_alloc(mrb, $TT_PROC, mrb->proc_class);
  $proc_copy(p, $proc_ptr(blk));
  p->flags |= $PROC_STRICT;
  $METHOD_FROM_PROC(m, p);
  $define_method_raw(mrb, c, mid, m);
  return $symbol_value(mid);
}

static $value
top_define_method($state *mrb, $value self)
{
  return mod_define_method(mrb, $obj_value(mrb->object_class));
}

static $value
$mod_eqq($state *mrb, $value mod)
{
  $value obj;
  $bool eqq;

  $get_args(mrb, "o", &obj);
  eqq = $obj_is_kind_of(mrb, obj, $class_ptr(mod));

  return $bool_value(eqq);
}

$API $value
$mod_module_function($state *mrb, $value mod)
{
  $value *argv;
  $int argc, i;
  $sym mid;
  $method_t m;
  struct RClass *rclass;
  int ai;

  $check_type(mrb, mod, $TT_MODULE);

  $get_args(mrb, "*", &argv, &argc);
  if (argc == 0) {
    /* set MODFUNC SCOPE if implemented */
    return mod;
  }

  /* set PRIVATE method visibility if implemented */
  /* $mod_dummy_visibility(mrb, mod); */

  for (i=0; i<argc; i++) {
    $check_type(mrb, argv[i], $TT_SYMBOL);

    mid = $symbol(argv[i]);
    rclass = $class_ptr(mod);
    m = $method_search(mrb, rclass, mid);

    prepare_singleton_class(mrb, (struct RBasic*)rclass);
    ai = $gc_arena_save(mrb);
    $define_method_raw(mrb, rclass->c, mid, m);
    $gc_arena_restore(mrb, ai);
  }

  return mod;
}

/* implementation of __id__ */
$value $obj_id_m($state *mrb, $value self);
/* implementation of instance_eval */
$value $obj_instance_eval($state*, $value);

static $value
inspect_main($state *mrb, $value mod)
{
  return $str_new_lit(mrb, "main");
}

void
$init_class($state *mrb)
{
  struct RClass *bob;           /* BasicObject */
  struct RClass *obj;           /* Object */
  struct RClass *mod;           /* Module */
  struct RClass *cls;           /* Class */

  /* boot class hierarchy */
  bob = boot_defclass(mrb, 0);
  obj = boot_defclass(mrb, bob); mrb->object_class = obj;
  mod = boot_defclass(mrb, obj); mrb->module_class = mod;/* obj -> mod */
  cls = boot_defclass(mrb, mod); mrb->class_class = cls; /* obj -> cls */
  /* fix-up loose ends */
  bob->c = obj->c = mod->c = cls->c = cls;
  make_metaclass(mrb, bob);
  make_metaclass(mrb, obj);
  make_metaclass(mrb, mod);
  make_metaclass(mrb, cls);

  /* name basic classes */
  $define_const(mrb, bob, "BasicObject", $obj_value(bob));
  $define_const(mrb, obj, "BasicObject", $obj_value(bob));
  $define_const(mrb, obj, "Object",      $obj_value(obj));
  $define_const(mrb, obj, "Module",      $obj_value(mod));
  $define_const(mrb, obj, "Class",       $obj_value(cls));

  /* name each classes */
  $class_name_class(mrb, NULL, bob, $intern_lit(mrb, "BasicObject"));
  $class_name_class(mrb, NULL, obj, $intern_lit(mrb, "Object")); /* 15.2.1 */
  $class_name_class(mrb, NULL, mod, $intern_lit(mrb, "Module")); /* 15.2.2 */
  $class_name_class(mrb, NULL, cls, $intern_lit(mrb, "Class"));  /* 15.2.3 */

  mrb->proc_class = $define_class(mrb, "Proc", mrb->object_class);  /* 15.2.17 */
  $SET_INSTANCE_TT(mrb->proc_class, $TT_PROC);

  $SET_INSTANCE_TT(cls, $TT_CLASS);
  $define_method(mrb, bob, "initialize",              $bob_init,             $ARGS_NONE());
  $define_method(mrb, bob, "!",                       $bob_not,              $ARGS_NONE());
  $define_method(mrb, bob, "==",                      $obj_equal_m,          $ARGS_REQ(1)); /* 15.3.1.3.1  */
  $define_method(mrb, bob, "!=",                      $obj_not_equal_m,      $ARGS_REQ(1));
  $define_method(mrb, bob, "__id__",                  $obj_id_m,             $ARGS_NONE()); /* 15.3.1.3.4  */
  $define_method(mrb, bob, "__send__",                $f_send,               $ARGS_ANY());  /* 15.3.1.3.5  */
  $define_method(mrb, bob, "instance_eval",           $obj_instance_eval,    $ARGS_ANY());  /* 15.3.1.3.18 */

  $define_class_method(mrb, cls, "new",               $class_new_class,      $ARGS_OPT(1));
  $define_method(mrb, cls, "superclass",              $class_superclass,     $ARGS_NONE()); /* 15.2.3.3.4 */
  $define_method(mrb, cls, "new",                     $instance_new,         $ARGS_ANY());  /* 15.2.3.3.3 */
  $define_method(mrb, cls, "initialize",              $class_initialize,     $ARGS_OPT(1)); /* 15.2.3.3.1 */
  $define_method(mrb, cls, "inherited",               $bob_init,             $ARGS_REQ(1));

  $SET_INSTANCE_TT(mod, $TT_MODULE);
  $define_method(mrb, mod, "extend_object",           $mod_extend_object,    $ARGS_REQ(1)); /* 15.2.2.4.25 */
  $define_method(mrb, mod, "extended",                $bob_init,             $ARGS_REQ(1)); /* 15.2.2.4.26 */
  $define_method(mrb, mod, "prepended",               $bob_init,             $ARGS_REQ(1));
  $define_method(mrb, mod, "prepend_features",        $mod_prepend_features, $ARGS_REQ(1));
  $define_method(mrb, mod, "include?",                $mod_include_p,        $ARGS_REQ(1)); /* 15.2.2.4.28 */
  $define_method(mrb, mod, "append_features",         $mod_append_features,  $ARGS_REQ(1)); /* 15.2.2.4.10 */
  $define_method(mrb, mod, "class_eval",              $mod_module_eval,      $ARGS_ANY());  /* 15.2.2.4.15 */
  $define_method(mrb, mod, "included",                $bob_init,             $ARGS_REQ(1)); /* 15.2.2.4.29 */
  $define_method(mrb, mod, "initialize",              $mod_initialize,       $ARGS_NONE()); /* 15.2.2.4.31 */
  $define_method(mrb, mod, "module_eval",             $mod_module_eval,      $ARGS_ANY());  /* 15.2.2.4.35 */
  $define_method(mrb, mod, "module_function",         $mod_module_function,  $ARGS_ANY());
  $define_method(mrb, mod, "private",                 $mod_dummy_visibility, $ARGS_ANY());  /* 15.2.2.4.36 */
  $define_method(mrb, mod, "protected",               $mod_dummy_visibility, $ARGS_ANY());  /* 15.2.2.4.37 */
  $define_method(mrb, mod, "public",                  $mod_dummy_visibility, $ARGS_ANY());  /* 15.2.2.4.38 */
  $define_method(mrb, mod, "attr_reader",             $mod_attr_reader,      $ARGS_ANY());  /* 15.2.2.4.13 */
  $define_method(mrb, mod, "attr_writer",             $mod_attr_writer,      $ARGS_ANY());  /* 15.2.2.4.14 */
  $define_method(mrb, mod, "to_s",                    $mod_to_s,             $ARGS_NONE());
  $define_method(mrb, mod, "inspect",                 $mod_to_s,             $ARGS_NONE());
  $define_method(mrb, mod, "alias_method",            $mod_alias,            $ARGS_ANY());  /* 15.2.2.4.8 */
  $define_method(mrb, mod, "ancestors",               $mod_ancestors,        $ARGS_NONE()); /* 15.2.2.4.9 */
  $define_method(mrb, mod, "undef_method",            $mod_undef,            $ARGS_ANY());  /* 15.2.2.4.41 */
  $define_method(mrb, mod, "const_defined?",          $mod_const_defined,    $ARGS_ARG(1,1)); /* 15.2.2.4.20 */
  $define_method(mrb, mod, "const_get",               $mod_const_get,        $ARGS_REQ(1)); /* 15.2.2.4.21 */
  $define_method(mrb, mod, "const_set",               $mod_const_set,        $ARGS_REQ(2)); /* 15.2.2.4.23 */
  $define_method(mrb, mod, "remove_const",            $mod_remove_const,     $ARGS_REQ(1)); /* 15.2.2.4.40 */
  $define_method(mrb, mod, "const_missing",           $mod_const_missing,    $ARGS_REQ(1));
  $define_method(mrb, mod, "method_defined?",         $mod_method_defined,   $ARGS_REQ(1)); /* 15.2.2.4.34 */
  $define_method(mrb, mod, "define_method",           mod_define_method,        $ARGS_ARG(1,1));
  $define_method(mrb, mod, "===",                     $mod_eqq,              $ARGS_REQ(1));

  $undef_method(mrb, cls, "append_features");
  $undef_method(mrb, cls, "extend_object");

  mrb->top_self = (struct RObject*)$obj_alloc(mrb, $TT_OBJECT, mrb->object_class);
  $define_singleton_method(mrb, mrb->top_self, "inspect", inspect_main, $ARGS_NONE());
  $define_singleton_method(mrb, mrb->top_self, "to_s", inspect_main, $ARGS_NONE());
  $define_singleton_method(mrb, mrb->top_self, "define_method", top_define_method, $ARGS_ARG(1,1));
}
