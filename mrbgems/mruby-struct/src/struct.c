/*
** struct.c - Struct class
**
** See Copyright Notice in mruby.h
*/

#include <string.h>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/string.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <mruby/hash.h>
#include <mruby/range.h>
#include <mruby/proc.h>

#define RSTRUCT_LEN(st) RARRAY_LEN(st)
#define RSTRUCT_PTR(st) RARRAY_PTR(st)

static struct RClass *
struct_class($state *mrb)
{
  return $class_get(mrb, "Struct");
}

static inline $value
struct_ivar_get($state *mrb, $value cls, $sym id)
{
  struct RClass* c = $class_ptr(cls);
  struct RClass* sclass = struct_class(mrb);
  $value ans;

  for (;;) {
    ans = $iv_get(mrb, $obj_value(c), id);
    if (!$nil_p(ans)) return ans;
    c = c->super;
    if (c == sclass || c == 0)
      return $nil_value();
  }
}

static $value
struct_s_members($state *mrb, struct RClass *klass)
{
  $value members = struct_ivar_get(mrb, $obj_value(klass), $intern_lit(mrb, "__members__"));

  if ($nil_p(members)) {
    $raise(mrb, E_TYPE_ERROR, "uninitialized struct");
  }
  if (!$array_p(members)) {
    $raise(mrb, E_TYPE_ERROR, "corrupted struct");
  }
  return members;
}

static $value
struct_members($state *mrb, $value s)
{
  $value members = struct_s_members(mrb, $obj_class(mrb, s));
  if (!$array_p(s)) {
    $raise(mrb, E_TYPE_ERROR, "corrupted struct");
  }
  if (RSTRUCT_LEN(s) != RARRAY_LEN(members)) {
    if (RSTRUCT_LEN(s) == 0) {  /* probably uninitialized */
      $ary_resize(mrb, s, RARRAY_LEN(members));
    }
    else {
      $raisef(mrb, E_TYPE_ERROR,
                 "struct size differs (%S required %S given)",
                 $fixnum_value(RARRAY_LEN(members)), $fixnum_value(RSTRUCT_LEN(s)));
    }
  }
  return members;
}

static $value
$struct_s_members_m($state *mrb, $value klass)
{
  $value members, ary;

  members = struct_s_members(mrb, $class_ptr(klass));
  ary = $ary_new_capa(mrb, RARRAY_LEN(members));
  $ary_replace(mrb, ary, members);
  return ary;
}

static void
$struct_modify($state *mrb, $value strct)
{
  if ($FROZEN_P($basic_ptr(strct))) {
    $raise(mrb, E_FROZEN_ERROR, "can't modify frozen struct");
  }

  $write_barrier(mrb, $basic_ptr(strct));
}

/* 15.2.18.4.6  */
/*
 *  call-seq:
 *     struct.members    -> array
 *
 *  Returns an array of strings representing the names of the instance
 *  variables.
 *
 *     Customer = Struct.new(:name, :address, :zip)
 *     joe = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *     joe.members   #=> [:name, :address, :zip]
 */

static $value
$struct_members($state *mrb, $value obj)
{
  return $struct_s_members_m(mrb, $obj_value($obj_class(mrb, obj)));
}

static $value
$struct_ref($state *mrb, $value obj)
{
  $int i = $fixnum($proc_cfunc_env_get(mrb, 0));
  $value *ptr = RSTRUCT_PTR(obj);

  if (!ptr) return $nil_value();
  return ptr[i];
}

static $sym
$id_attrset($state *mrb, $sym id)
{
  const char *name;
  char *buf;
  $int len;
  $sym mid;

  name = $sym2name_len(mrb, id, &len);
  buf = (char *)$malloc(mrb, (size_t)len+2);
  memcpy(buf, name, (size_t)len);
  buf[len] = '=';
  buf[len+1] = '\0';

  mid = $intern(mrb, buf, len+1);
  $free(mrb, buf);
  return mid;
}

static $value
$struct_set_m($state *mrb, $value obj)
{
  $int i = $fixnum($proc_cfunc_env_get(mrb, 0));
  $value *ptr;
  $value val;

  $get_args(mrb, "o", &val);
  $struct_modify(mrb, obj);
  ptr = RSTRUCT_PTR(obj);
  if (ptr == NULL || i >= RSTRUCT_LEN(obj)) {
    $ary_set(mrb, obj, i, val);
  }
  else {
    ptr[i] = val;
  }
  return val;
}

static $bool
is_local_id($state *mrb, const char *name)
{
  if (!name) return FALSE;
  return !ISUPPER(name[0]);
}

static $bool
is_const_id($state *mrb, const char *name)
{
  if (!name) return FALSE;
  return ISUPPER(name[0]);
}

static void
make_struct_define_accessors($state *mrb, $value members, struct RClass *c)
{
  const $value *ptr_members = RARRAY_PTR(members);
  $int i;
  $int len = RARRAY_LEN(members);
  int ai = $gc_arena_save(mrb);

  for (i=0; i<len; i++) {
    $sym id = $symbol(ptr_members[i]);
    const char *name = $sym2name_len(mrb, id, NULL);

    if (is_local_id(mrb, name) || is_const_id(mrb, name)) {
      $method_t m;
      $value at = $fixnum_value(i);
      struct RProc *aref = $proc_new_cfunc_with_env(mrb, $struct_ref, 1, &at);
      struct RProc *aset = $proc_new_cfunc_with_env(mrb, $struct_set_m, 1, &at);
      $METHOD_FROM_PROC(m, aref);
      $define_method_raw(mrb, c, id, m);
      $METHOD_FROM_PROC(m, aset);
      $define_method_raw(mrb, c, $id_attrset(mrb, id), m);
      $gc_arena_restore(mrb, ai);
    }
  }
}

static $value
make_struct($state *mrb, $value name, $value members, struct RClass *klass)
{
  $value nstr;
  $sym id;
  struct RClass *c;

  if ($nil_p(name)) {
    c = $class_new(mrb, klass);
  }
  else {
    /* old style: should we warn? */
    name = $str_to_str(mrb, name);
    id = $obj_to_sym(mrb, name);
    if (!is_const_id(mrb, $sym2name_len(mrb, id, NULL))) {
      $name_error(mrb, id, "identifier %S needs to be constant", name);
    }
    if ($const_defined_at(mrb, $obj_value(klass), id)) {
      $warn(mrb, "redefining constant Struct::%S", name);
      $const_remove(mrb, $obj_value(klass), id);
    }
    c = $define_class_under(mrb, klass, RSTRING_PTR(name), klass);
  }
  $SET_INSTANCE_TT(c, $TT_ARRAY);
  nstr = $obj_value(c);
  $iv_set(mrb, nstr, $intern_lit(mrb, "__members__"), members);

  $define_class_method(mrb, c, "new", $instance_new, $ARGS_ANY());
  $define_class_method(mrb, c, "[]", $instance_new, $ARGS_ANY());
  $define_class_method(mrb, c, "members", $struct_s_members_m, $ARGS_NONE());
  /* RSTRUCT(nstr)->basic.c->super = c->c; */
  make_struct_define_accessors(mrb, members, c);
  return nstr;
}

/* 15.2.18.3.1  */
/*
 *  call-seq:
 *     Struct.new( [aString] [, aSym]+> )    -> StructClass
 *     StructClass.new(arg, ...)             -> obj
 *     StructClass[arg, ...]                 -> obj
 *
 *  Creates a new class, named by <i>aString</i>, containing accessor
 *  methods for the given symbols. If the name <i>aString</i> is
 *  omitted, an anonymous structure class will be created. Otherwise,
 *  the name of this struct will appear as a constant in class
 *  <code>Struct</code>, so it must be unique for all
 *  <code>Struct</code>s in the system and should start with a capital
 *  letter. Assigning a structure class to a constant effectively gives
 *  the class the name of the constant.
 *
 *  <code>Struct::new</code> returns a new <code>Class</code> object,
 *  which can then be used to create specific instances of the new
 *  structure. The number of actual parameters must be
 *  less than or equal to the number of attributes defined for this
 *  class; unset parameters default to <code>nil</code>.  Passing too many
 *  parameters will raise an <code>ArgumentError</code>.
 *
 *  The remaining methods listed in this section (class and instance)
 *  are defined for this generated class.
 *
 *     # Create a structure with a name in Struct
 *     Struct.new("Customer", :name, :address)    #=> Struct::Customer
 *     Struct::Customer.new("Dave", "123 Main")   #=> #<struct Struct::Customer name="Dave", address="123 Main">
 *
 *     # Create a structure named by its constant
 *     Customer = Struct.new(:name, :address)     #=> Customer
 *     Customer.new("Dave", "123 Main")           #=> #<struct Customer name="Dave", address="123 Main">
 */
static $value
$struct_s_def($state *mrb, $value klass)
{
  $value name, rest;
  $value *pargv;
  $int argcnt;
  $int i;
  $value b, st;
  $sym id;
  $value *argv;
  $int argc;

  name = $nil_value();
  $get_args(mrb, "*&", &argv, &argc, &b);
  if (argc == 0) { /* special case to avoid crash */
    $raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
  }
  else {
    pargv = argv;
    argcnt = argc;
    if (argc > 0) {
      name = argv[0];
      if ($symbol_p(name)) {
        /* 1stArgument:symbol -> name=nil rest=argv[0..n] */
        name = $nil_value();
      }
      else {
        pargv++;
        argcnt--;
      }
    }
    rest = $ary_new_from_values(mrb, argcnt, pargv);
    for (i=0; i<argcnt; i++) {
      id = $obj_to_sym(mrb, RARRAY_PTR(rest)[i]);
      $ary_set(mrb, rest, i, $symbol_value(id));
    }
    st = make_struct(mrb, name, rest, $class_ptr(klass));
    if (!$nil_p(b)) {
      $yield_with_class(mrb, b, 1, &st, st, $class_ptr(st));
    }

    return st;
  }
  /* not reached */
  return $nil_value();
}

static $int
num_members($state *mrb, struct RClass *klass)
{
  $value members;

  members = struct_ivar_get(mrb, $obj_value(klass), $intern_lit(mrb, "__members__"));
  if (!$array_p(members)) {
    $raise(mrb, E_TYPE_ERROR, "broken members");
  }
  return RARRAY_LEN(members);
}

/* 15.2.18.4.8  */
/*
 */
static $value
$struct_initialize_withArg($state *mrb, $int argc, $value *argv, $value self)
{
  struct RClass *klass = $obj_class(mrb, self);
  $int i, n;

  n = num_members(mrb, klass);
  if (n < argc) {
    $raise(mrb, E_ARGUMENT_ERROR, "struct size differs");
  }

  for (i = 0; i < argc; i++) {
    $ary_set(mrb, self, i, argv[i]);
  }
  for (i = argc; i < n; i++) {
    $ary_set(mrb, self, i, $nil_value());
  }
  return self;
}

static $value
$struct_initialize($state *mrb, $value self)
{
  $value *argv;
  $int argc;

  $get_args(mrb, "*!", &argv, &argc);
  return $struct_initialize_withArg(mrb, argc, argv, self);
}

/* 15.2.18.4.9  */
/* :nodoc: */
static $value
$struct_init_copy($state *mrb, $value copy)
{
  $value s;

  $get_args(mrb, "o", &s);

  if ($obj_equal(mrb, copy, s)) return copy;
  if (!$obj_is_instance_of(mrb, s, $obj_class(mrb, copy))) {
    $raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }
  if (!$array_p(s)) {
    $raise(mrb, E_TYPE_ERROR, "corrupted struct");
  }
  $ary_replace(mrb, copy, s);
  return copy;
}

static $value
struct_aref_sym($state *mrb, $value obj, $sym id)
{
  $value members, *ptr;
  const $value *ptr_members;
  $int i, len;

  members = struct_members(mrb, obj);
  ptr_members = RARRAY_PTR(members);
  len = RARRAY_LEN(members);
  ptr = RSTRUCT_PTR(obj);
  for (i=0; i<len; i++) {
    $value slot = ptr_members[i];
    if ($symbol_p(slot) && $symbol(slot) == id) {
      return ptr[i];
    }
  }
  $raisef(mrb, E_INDEX_ERROR, "'%S' is not a struct member", $sym2str(mrb, id));
  return $nil_value();       /* not reached */
}

static $value
struct_aref_int($state *mrb, $value s, $int i)
{
  if (i < 0) i = RSTRUCT_LEN(s) + i;
  if (i < 0)
      $raisef(mrb, E_INDEX_ERROR,
                 "offset %S too small for struct(size:%S)",
                 $fixnum_value(i), $fixnum_value(RSTRUCT_LEN(s)));
  if (RSTRUCT_LEN(s) <= i)
    $raisef(mrb, E_INDEX_ERROR,
               "offset %S too large for struct(size:%S)",
               $fixnum_value(i), $fixnum_value(RSTRUCT_LEN(s)));
  return RSTRUCT_PTR(s)[i];
}

/* 15.2.18.4.2  */
/*
 *  call-seq:
 *     struct[symbol]    -> anObject
 *     struct[fixnum]    -> anObject
 *
 *  Attribute Reference---Returns the value of the instance variable
 *  named by <i>symbol</i>, or indexed (0..length-1) by
 *  <i>fixnum</i>. Will raise <code>NameError</code> if the named
 *  variable does not exist, or <code>IndexError</code> if the index is
 *  out of range.
 *
 *     Customer = Struct.new(:name, :address, :zip)
 *     joe = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *
 *     joe["name"]   #=> "Joe Smith"
 *     joe[:name]    #=> "Joe Smith"
 *     joe[0]        #=> "Joe Smith"
 */
static $value
$struct_aref($state *mrb, $value s)
{
  $value idx;

  $get_args(mrb, "o", &idx);
  if ($string_p(idx)) {
    $value sym = $check_intern_str(mrb, idx);

    if ($nil_p(sym)) {
      $name_error(mrb, $intern_str(mrb, idx), "no member '%S' in struct", idx);
    }
    idx = sym;
  }
  if ($symbol_p(idx)) {
    return struct_aref_sym(mrb, s, $symbol(idx));
  }
  return struct_aref_int(mrb, s, $int(mrb, idx));
}

static $value
$struct_aset_sym($state *mrb, $value s, $sym id, $value val)
{
  $value members, *ptr;
  const $value *ptr_members;
  $int i, len;

  members = struct_members(mrb, s);
  len = RARRAY_LEN(members);
  ptr = RSTRUCT_PTR(s);
  ptr_members = RARRAY_PTR(members);
  for (i=0; i<len; i++) {
    if ($symbol(ptr_members[i]) == id) {
      $struct_modify(mrb, s);
      ptr[i] = val;
      return val;
    }
  }
  $name_error(mrb, id, "no member '%S' in struct", $sym2str(mrb, id));
  return val;                   /* not reach */
}

/* 15.2.18.4.3  */
/*
 *  call-seq:
 *     struct[symbol] = obj    -> obj
 *     struct[fixnum] = obj    -> obj
 *
 *  Attribute Assignment---Assigns to the instance variable named by
 *  <i>symbol</i> or <i>fixnum</i> the value <i>obj</i> and
 *  returns it. Will raise a <code>NameError</code> if the named
 *  variable does not exist, or an <code>IndexError</code> if the index
 *  is out of range.
 *
 *     Customer = Struct.new(:name, :address, :zip)
 *     joe = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *
 *     joe["name"] = "Luke"
 *     joe[:zip]   = "90210"
 *
 *     joe.name   #=> "Luke"
 *     joe.zip    #=> "90210"
 */

static $value
$struct_aset($state *mrb, $value s)
{
  $int i;
  $value idx;
  $value val;

  $get_args(mrb, "oo", &idx, &val);

  if ($string_p(idx)) {
    $value sym = $check_intern_str(mrb, idx);

    if ($nil_p(sym)) {
      $name_error(mrb, $intern_str(mrb, idx), "no member '%S' in struct", idx);
    }
    idx = sym;
  }
  if ($symbol_p(idx)) {
    return $struct_aset_sym(mrb, s, $symbol(idx), val);
  }

  i = $int(mrb, idx);
  if (i < 0) i = RSTRUCT_LEN(s) + i;
  if (i < 0) {
    $raisef(mrb, E_INDEX_ERROR,
               "offset %S too small for struct(size:%S)",
               $fixnum_value(i), $fixnum_value(RSTRUCT_LEN(s)));
  }
  if (RSTRUCT_LEN(s) <= i) {
    $raisef(mrb, E_INDEX_ERROR,
               "offset %S too large for struct(size:%S)",
               $fixnum_value(i), $fixnum_value(RSTRUCT_LEN(s)));
  }
  $struct_modify(mrb, s);
  return RSTRUCT_PTR(s)[i] = val;
}

/* 15.2.18.4.1  */
/*
 *  call-seq:
 *     struct == other_struct     -> true or false
 *
 *  Equality---Returns <code>true</code> if <i>other_struct</i> is
 *  equal to this one: they must be of the same class as generated by
 *  <code>Struct::new</code>, and the values of all instance variables
 *  must be equal (according to <code>Object#==</code>).
 *
 *     Customer = Struct.new(:name, :address, :zip)
 *     joe   = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *     joejr = Customer.new("Joe Smith", "123 Maple, Anytown NC", 12345)
 *     jane  = Customer.new("Jane Doe", "456 Elm, Anytown NC", 12345)
 *     joe == joejr   #=> true
 *     joe == jane    #=> false
 */

static $value
$struct_equal($state *mrb, $value s)
{
  $value s2;
  $value *ptr, *ptr2;
  $int i, len;

  $get_args(mrb, "o", &s2);
  if ($obj_equal(mrb, s, s2)) {
    return $true_value();
  }
  if ($obj_class(mrb, s) != $obj_class(mrb, s2)) {
    return $false_value();
  }
  if (RSTRUCT_LEN(s) != RSTRUCT_LEN(s2)) {
    $bug(mrb, "inconsistent struct"); /* should never happen */
  }
  ptr = RSTRUCT_PTR(s);
  ptr2 = RSTRUCT_PTR(s2);
  len = RSTRUCT_LEN(s);
  for (i=0; i<len; i++) {
    if (!$equal(mrb, ptr[i], ptr2[i])) {
      return $false_value();
    }
  }

  return $true_value();
}

/* 15.2.18.4.12(x)  */
/*
 * code-seq:
 *   struct.eql?(other)   -> true or false
 *
 * Two structures are equal if they are the same object, or if all their
 * fields are equal (using <code>eql?</code>).
 */
static $value
$struct_eql($state *mrb, $value s)
{
  $value s2;
  $value *ptr, *ptr2;
  $int i, len;

  $get_args(mrb, "o", &s2);
  if ($obj_equal(mrb, s, s2)) {
    return $true_value();
  }
  if ($obj_class(mrb, s) != $obj_class(mrb, s2)) {
    return $false_value();
  }
  if (RSTRUCT_LEN(s) != RSTRUCT_LEN(s2)) {
    $bug(mrb, "inconsistent struct"); /* should never happen */
  }
  ptr = RSTRUCT_PTR(s);
  ptr2 = RSTRUCT_PTR(s2);
  len = RSTRUCT_LEN(s);
  for (i=0; i<len; i++) {
    if (!$eql(mrb, ptr[i], ptr2[i])) {
      return $false_value();
    }
  }

  return $true_value();
}

/*
 * call-seq:
 *    struct.length   -> Fixnum
 *    struct.size     -> Fixnum
 *
 * Returns number of struct members.
 */
static $value
$struct_len($state *mrb, $value self)
{
  return $fixnum_value(RSTRUCT_LEN(self));
}

/*
 * call-seq:
 *    struct.to_a    -> array
 *    struct.values  -> array
 *
 * Create an array from struct values.
 */
static $value
$struct_to_a($state *mrb, $value self)
{
  return $ary_new_from_values(mrb, RSTRUCT_LEN(self), RSTRUCT_PTR(self));
}

/*
 * call-seq:
 *    struct.to_h -> hash
 *
 * Create a hash from member names and struct values.
 */
static $value
$struct_to_h($state *mrb, $value self)
{
  $value members, ret;
  $int i;

  members = struct_members(mrb, self);
  ret = $hash_new_capa(mrb, RARRAY_LEN(members));

  for (i = 0; i < RARRAY_LEN(members); ++i) {
    $hash_set(mrb, ret, RARRAY_PTR(members)[i], RSTRUCT_PTR(self)[i]);
  }

  return ret;
}

static $value
$struct_values_at($state *mrb, $value self)
{
  $int argc;
  $value *argv;

  $get_args(mrb, "*", &argv, &argc);

  return $get_values_at(mrb, self, RSTRUCT_LEN(self), argc, argv, struct_aref_int);
}

/*
 *  A <code>Struct</code> is a convenient way to bundle a number of
 *  attributes together, using accessor methods, without having to write
 *  an explicit class.
 *
 *  The <code>Struct</code> class is a generator of specific classes,
 *  each one of which is defined to hold a set of variables and their
 *  accessors. In these examples, we'll call the generated class
 *  "<i>Customer</i>Class," and we'll show an example instance of that
 *  class as "<i>Customer</i>Inst."
 *
 *  In the descriptions that follow, the parameter <i>symbol</i> refers
 *  to a symbol, which is either a quoted string or a
 *  <code>Symbol</code> (such as <code>:name</code>).
 */
void
$mruby_struct_gem_init($state* mrb)
{
  struct RClass *st;
  st = $define_class(mrb, "Struct",  mrb->object_class);
  $SET_INSTANCE_TT(st, $TT_ARRAY);

  $define_class_method(mrb, st, "new",             $struct_s_def,       $ARGS_ANY());  /* 15.2.18.3.1  */

  $define_method(mrb, st,       "==",              $struct_equal,       $ARGS_REQ(1)); /* 15.2.18.4.1  */
  $define_method(mrb, st,       "[]",              $struct_aref,        $ARGS_REQ(1)); /* 15.2.18.4.2  */
  $define_method(mrb, st,       "[]=",             $struct_aset,        $ARGS_REQ(2)); /* 15.2.18.4.3  */
  $define_method(mrb, st,       "members",         $struct_members,     $ARGS_NONE()); /* 15.2.18.4.6  */
  $define_method(mrb, st,       "initialize",      $struct_initialize,  $ARGS_ANY());  /* 15.2.18.4.8  */
  $define_method(mrb, st,       "initialize_copy", $struct_init_copy,   $ARGS_REQ(1)); /* 15.2.18.4.9  */
  $define_method(mrb, st,       "eql?",            $struct_eql,         $ARGS_REQ(1)); /* 15.2.18.4.12(x)  */

  $define_method(mrb, st,        "size",           $struct_len,         $ARGS_NONE());
  $define_method(mrb, st,        "length",         $struct_len,         $ARGS_NONE());
  $define_method(mrb, st,        "to_a",           $struct_to_a,        $ARGS_NONE());
  $define_method(mrb, st,        "values",         $struct_to_a,        $ARGS_NONE());
  $define_method(mrb, st,        "to_h",           $struct_to_h,        $ARGS_NONE());
  $define_method(mrb, st,        "values_at",      $struct_values_at,   $ARGS_ANY());
}

void
$mruby_struct_gem_final($state* mrb)
{
}
