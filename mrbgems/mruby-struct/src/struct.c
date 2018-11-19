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
struct_class(state *mrb)
{
  return _class_get(mrb, "Struct");
}

static inline value
struct_ivar_get(state *mrb, value cls, _sym id)
{
  struct RClass* c = _class_ptr(cls);
  struct RClass* sclass = struct_class(mrb);
  value ans;

  for (;;) {
    ans = _iv_get(mrb, _obj_value(c), id);
    if (!_nil_p(ans)) return ans;
    c = c->super;
    if (c == sclass || c == 0)
      return _nil_value();
  }
}

static value
struct_s_members(state *mrb, struct RClass *klass)
{
  value members = struct_ivar_get(mrb, _obj_value(klass), _intern_lit(mrb, "__members__"));

  if (_nil_p(members)) {
    _raise(mrb, E_TYPE_ERROR, "uninitialized struct");
  }
  if (!_array_p(members)) {
    _raise(mrb, E_TYPE_ERROR, "corrupted struct");
  }
  return members;
}

static value
struct_members(state *mrb, value s)
{
  value members = struct_s_members(mrb, _obj_class(mrb, s));
  if (!_array_p(s)) {
    _raise(mrb, E_TYPE_ERROR, "corrupted struct");
  }
  if (RSTRUCT_LEN(s) != RARRAY_LEN(members)) {
    if (RSTRUCT_LEN(s) == 0) {  /* probably uninitialized */
      _ary_resize(mrb, s, RARRAY_LEN(members));
    }
    else {
      _raisef(mrb, E_TYPE_ERROR,
                 "struct size differs (%S required %S given)",
                 _fixnum_value(RARRAY_LEN(members)), _fixnum_value(RSTRUCT_LEN(s)));
    }
  }
  return members;
}

static value
_struct_s_members_m(state *mrb, value klass)
{
  value members, ary;

  members = struct_s_members(mrb, _class_ptr(klass));
  ary = _ary_new_capa(mrb, RARRAY_LEN(members));
  _ary_replace(mrb, ary, members);
  return ary;
}

static void
_struct_modify(state *mrb, value strct)
{
  if (MRB_FROZEN_P(_basic_ptr(strct))) {
    _raise(mrb, E_FROZEN_ERROR, "can't modify frozen struct");
  }

  _write_barrier(mrb, _basic_ptr(strct));
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

static value
_struct_members(state *mrb, value obj)
{
  return _struct_s_members_m(mrb, _obj_value(_obj_class(mrb, obj)));
}

static value
_struct_ref(state *mrb, value obj)
{
  _int i = _fixnum(_proc_cfunc_env_get(mrb, 0));
  value *ptr = RSTRUCT_PTR(obj);

  if (!ptr) return _nil_value();
  return ptr[i];
}

static _sym
_id_attrset(state *mrb, _sym id)
{
  const char *name;
  char *buf;
  _int len;
  _sym mid;

  name = _sym2name_len(mrb, id, &len);
  buf = (char *)_malloc(mrb, (size_t)len+2);
  memcpy(buf, name, (size_t)len);
  buf[len] = '=';
  buf[len+1] = '\0';

  mid = _intern(mrb, buf, len+1);
  _free(mrb, buf);
  return mid;
}

static value
_struct_set_m(state *mrb, value obj)
{
  _int i = _fixnum(_proc_cfunc_env_get(mrb, 0));
  value *ptr;
  value val;

  _get_args(mrb, "o", &val);
  _struct_modify(mrb, obj);
  ptr = RSTRUCT_PTR(obj);
  if (ptr == NULL || i >= RSTRUCT_LEN(obj)) {
    _ary_set(mrb, obj, i, val);
  }
  else {
    ptr[i] = val;
  }
  return val;
}

static _bool
is_local_id(state *mrb, const char *name)
{
  if (!name) return FALSE;
  return !ISUPPER(name[0]);
}

static _bool
is_const_id(state *mrb, const char *name)
{
  if (!name) return FALSE;
  return ISUPPER(name[0]);
}

static void
make_struct_define_accessors(state *mrb, value members, struct RClass *c)
{
  const value *ptr_members = RARRAY_PTR(members);
  _int i;
  _int len = RARRAY_LEN(members);
  int ai = _gc_arena_save(mrb);

  for (i=0; i<len; i++) {
    _sym id = _symbol(ptr_members[i]);
    const char *name = _sym2name_len(mrb, id, NULL);

    if (is_local_id(mrb, name) || is_const_id(mrb, name)) {
      _method_t m;
      value at = _fixnum_value(i);
      struct RProc *aref = _proc_new_cfunc_with_env(mrb, _struct_ref, 1, &at);
      struct RProc *aset = _proc_new_cfunc_with_env(mrb, _struct_set_m, 1, &at);
      MRB_METHOD_FROM_PROC(m, aref);
      _define_method_raw(mrb, c, id, m);
      MRB_METHOD_FROM_PROC(m, aset);
      _define_method_raw(mrb, c, _id_attrset(mrb, id), m);
      _gc_arena_restore(mrb, ai);
    }
  }
}

static value
make_struct(state *mrb, value name, value members, struct RClass *klass)
{
  value nstr;
  _sym id;
  struct RClass *c;

  if (_nil_p(name)) {
    c = _class_new(mrb, klass);
  }
  else {
    /* old style: should we warn? */
    name = _str_to_str(mrb, name);
    id = _obj_to_sym(mrb, name);
    if (!is_const_id(mrb, _sym2name_len(mrb, id, NULL))) {
      _name_error(mrb, id, "identifier %S needs to be constant", name);
    }
    if (_const_defined_at(mrb, _obj_value(klass), id)) {
      _warn(mrb, "redefining constant Struct::%S", name);
      _const_remove(mrb, _obj_value(klass), id);
    }
    c = _define_class_under(mrb, klass, RSTRING_PTR(name), klass);
  }
  MRB_SET_INSTANCE_TT(c, MRB_TT_ARRAY);
  nstr = _obj_value(c);
  _iv_set(mrb, nstr, _intern_lit(mrb, "__members__"), members);

  _define_class_method(mrb, c, "new", _instance_new, MRB_ARGS_ANY());
  _define_class_method(mrb, c, "[]", _instance_new, MRB_ARGS_ANY());
  _define_class_method(mrb, c, "members", _struct_s_members_m, MRB_ARGS_NONE());
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
static value
_struct_s_def(state *mrb, value klass)
{
  value name, rest;
  value *pargv;
  _int argcnt;
  _int i;
  value b, st;
  _sym id;
  value *argv;
  _int argc;

  name = _nil_value();
  _get_args(mrb, "*&", &argv, &argc, &b);
  if (argc == 0) { /* special case to avoid crash */
    _raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
  }
  else {
    pargv = argv;
    argcnt = argc;
    if (argc > 0) {
      name = argv[0];
      if (_symbol_p(name)) {
        /* 1stArgument:symbol -> name=nil rest=argv[0..n] */
        name = _nil_value();
      }
      else {
        pargv++;
        argcnt--;
      }
    }
    rest = _ary_new_from_values(mrb, argcnt, pargv);
    for (i=0; i<argcnt; i++) {
      id = _obj_to_sym(mrb, RARRAY_PTR(rest)[i]);
      _ary_set(mrb, rest, i, _symbol_value(id));
    }
    st = make_struct(mrb, name, rest, _class_ptr(klass));
    if (!_nil_p(b)) {
      _yield_with_class(mrb, b, 1, &st, st, _class_ptr(st));
    }

    return st;
  }
  /* not reached */
  return _nil_value();
}

static _int
num_members(state *mrb, struct RClass *klass)
{
  value members;

  members = struct_ivar_get(mrb, _obj_value(klass), _intern_lit(mrb, "__members__"));
  if (!_array_p(members)) {
    _raise(mrb, E_TYPE_ERROR, "broken members");
  }
  return RARRAY_LEN(members);
}

/* 15.2.18.4.8  */
/*
 */
static value
_struct_initialize_withArg(state *mrb, _int argc, value *argv, value self)
{
  struct RClass *klass = _obj_class(mrb, self);
  _int i, n;

  n = num_members(mrb, klass);
  if (n < argc) {
    _raise(mrb, E_ARGUMENT_ERROR, "struct size differs");
  }

  for (i = 0; i < argc; i++) {
    _ary_set(mrb, self, i, argv[i]);
  }
  for (i = argc; i < n; i++) {
    _ary_set(mrb, self, i, _nil_value());
  }
  return self;
}

static value
_struct_initialize(state *mrb, value self)
{
  value *argv;
  _int argc;

  _get_args(mrb, "*!", &argv, &argc);
  return _struct_initialize_withArg(mrb, argc, argv, self);
}

/* 15.2.18.4.9  */
/* :nodoc: */
static value
_struct_init_copy(state *mrb, value copy)
{
  value s;

  _get_args(mrb, "o", &s);

  if (_obj_equal(mrb, copy, s)) return copy;
  if (!_obj_is_instance_of(mrb, s, _obj_class(mrb, copy))) {
    _raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }
  if (!_array_p(s)) {
    _raise(mrb, E_TYPE_ERROR, "corrupted struct");
  }
  _ary_replace(mrb, copy, s);
  return copy;
}

static value
struct_aref_sym(state *mrb, value obj, _sym id)
{
  value members, *ptr;
  const value *ptr_members;
  _int i, len;

  members = struct_members(mrb, obj);
  ptr_members = RARRAY_PTR(members);
  len = RARRAY_LEN(members);
  ptr = RSTRUCT_PTR(obj);
  for (i=0; i<len; i++) {
    value slot = ptr_members[i];
    if (_symbol_p(slot) && _symbol(slot) == id) {
      return ptr[i];
    }
  }
  _raisef(mrb, E_INDEX_ERROR, "'%S' is not a struct member", _sym2str(mrb, id));
  return _nil_value();       /* not reached */
}

static value
struct_aref_int(state *mrb, value s, _int i)
{
  if (i < 0) i = RSTRUCT_LEN(s) + i;
  if (i < 0)
      _raisef(mrb, E_INDEX_ERROR,
                 "offset %S too small for struct(size:%S)",
                 _fixnum_value(i), _fixnum_value(RSTRUCT_LEN(s)));
  if (RSTRUCT_LEN(s) <= i)
    _raisef(mrb, E_INDEX_ERROR,
               "offset %S too large for struct(size:%S)",
               _fixnum_value(i), _fixnum_value(RSTRUCT_LEN(s)));
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
static value
_struct_aref(state *mrb, value s)
{
  value idx;

  _get_args(mrb, "o", &idx);
  if (_string_p(idx)) {
    value sym = _check_intern_str(mrb, idx);

    if (_nil_p(sym)) {
      _name_error(mrb, _intern_str(mrb, idx), "no member '%S' in struct", idx);
    }
    idx = sym;
  }
  if (_symbol_p(idx)) {
    return struct_aref_sym(mrb, s, _symbol(idx));
  }
  return struct_aref_int(mrb, s, _int(mrb, idx));
}

static value
_struct_aset_sym(state *mrb, value s, _sym id, value val)
{
  value members, *ptr;
  const value *ptr_members;
  _int i, len;

  members = struct_members(mrb, s);
  len = RARRAY_LEN(members);
  ptr = RSTRUCT_PTR(s);
  ptr_members = RARRAY_PTR(members);
  for (i=0; i<len; i++) {
    if (_symbol(ptr_members[i]) == id) {
      _struct_modify(mrb, s);
      ptr[i] = val;
      return val;
    }
  }
  _name_error(mrb, id, "no member '%S' in struct", _sym2str(mrb, id));
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

static value
_struct_aset(state *mrb, value s)
{
  _int i;
  value idx;
  value val;

  _get_args(mrb, "oo", &idx, &val);

  if (_string_p(idx)) {
    value sym = _check_intern_str(mrb, idx);

    if (_nil_p(sym)) {
      _name_error(mrb, _intern_str(mrb, idx), "no member '%S' in struct", idx);
    }
    idx = sym;
  }
  if (_symbol_p(idx)) {
    return _struct_aset_sym(mrb, s, _symbol(idx), val);
  }

  i = _int(mrb, idx);
  if (i < 0) i = RSTRUCT_LEN(s) + i;
  if (i < 0) {
    _raisef(mrb, E_INDEX_ERROR,
               "offset %S too small for struct(size:%S)",
               _fixnum_value(i), _fixnum_value(RSTRUCT_LEN(s)));
  }
  if (RSTRUCT_LEN(s) <= i) {
    _raisef(mrb, E_INDEX_ERROR,
               "offset %S too large for struct(size:%S)",
               _fixnum_value(i), _fixnum_value(RSTRUCT_LEN(s)));
  }
  _struct_modify(mrb, s);
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

static value
_struct_equal(state *mrb, value s)
{
  value s2;
  value *ptr, *ptr2;
  _int i, len;

  _get_args(mrb, "o", &s2);
  if (_obj_equal(mrb, s, s2)) {
    return _true_value();
  }
  if (_obj_class(mrb, s) != _obj_class(mrb, s2)) {
    return _false_value();
  }
  if (RSTRUCT_LEN(s) != RSTRUCT_LEN(s2)) {
    _bug(mrb, "inconsistent struct"); /* should never happen */
  }
  ptr = RSTRUCT_PTR(s);
  ptr2 = RSTRUCT_PTR(s2);
  len = RSTRUCT_LEN(s);
  for (i=0; i<len; i++) {
    if (!_equal(mrb, ptr[i], ptr2[i])) {
      return _false_value();
    }
  }

  return _true_value();
}

/* 15.2.18.4.12(x)  */
/*
 * code-seq:
 *   struct.eql?(other)   -> true or false
 *
 * Two structures are equal if they are the same object, or if all their
 * fields are equal (using <code>eql?</code>).
 */
static value
_struct_eql(state *mrb, value s)
{
  value s2;
  value *ptr, *ptr2;
  _int i, len;

  _get_args(mrb, "o", &s2);
  if (_obj_equal(mrb, s, s2)) {
    return _true_value();
  }
  if (_obj_class(mrb, s) != _obj_class(mrb, s2)) {
    return _false_value();
  }
  if (RSTRUCT_LEN(s) != RSTRUCT_LEN(s2)) {
    _bug(mrb, "inconsistent struct"); /* should never happen */
  }
  ptr = RSTRUCT_PTR(s);
  ptr2 = RSTRUCT_PTR(s2);
  len = RSTRUCT_LEN(s);
  for (i=0; i<len; i++) {
    if (!_eql(mrb, ptr[i], ptr2[i])) {
      return _false_value();
    }
  }

  return _true_value();
}

/*
 * call-seq:
 *    struct.length   -> Fixnum
 *    struct.size     -> Fixnum
 *
 * Returns number of struct members.
 */
static value
_struct_len(state *mrb, value self)
{
  return _fixnum_value(RSTRUCT_LEN(self));
}

/*
 * call-seq:
 *    struct.to_a    -> array
 *    struct.values  -> array
 *
 * Create an array from struct values.
 */
static value
_struct_to_a(state *mrb, value self)
{
  return _ary_new_from_values(mrb, RSTRUCT_LEN(self), RSTRUCT_PTR(self));
}

/*
 * call-seq:
 *    struct.to_h -> hash
 *
 * Create a hash from member names and struct values.
 */
static value
_struct_to_h(state *mrb, value self)
{
  value members, ret;
  _int i;

  members = struct_members(mrb, self);
  ret = _hash_new_capa(mrb, RARRAY_LEN(members));

  for (i = 0; i < RARRAY_LEN(members); ++i) {
    _hash_set(mrb, ret, RARRAY_PTR(members)[i], RSTRUCT_PTR(self)[i]);
  }

  return ret;
}

static value
_struct_values_at(state *mrb, value self)
{
  _int argc;
  value *argv;

  _get_args(mrb, "*", &argv, &argc);

  return _get_values_at(mrb, self, RSTRUCT_LEN(self), argc, argv, struct_aref_int);
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
_mruby_struct_gem_init(state* mrb)
{
  struct RClass *st;
  st = _define_class(mrb, "Struct",  mrb->object_class);
  MRB_SET_INSTANCE_TT(st, MRB_TT_ARRAY);

  _define_class_method(mrb, st, "new",             _struct_s_def,       MRB_ARGS_ANY());  /* 15.2.18.3.1  */

  _define_method(mrb, st,       "==",              _struct_equal,       MRB_ARGS_REQ(1)); /* 15.2.18.4.1  */
  _define_method(mrb, st,       "[]",              _struct_aref,        MRB_ARGS_REQ(1)); /* 15.2.18.4.2  */
  _define_method(mrb, st,       "[]=",             _struct_aset,        MRB_ARGS_REQ(2)); /* 15.2.18.4.3  */
  _define_method(mrb, st,       "members",         _struct_members,     MRB_ARGS_NONE()); /* 15.2.18.4.6  */
  _define_method(mrb, st,       "initialize",      _struct_initialize,  MRB_ARGS_ANY());  /* 15.2.18.4.8  */
  _define_method(mrb, st,       "initialize_copy", _struct_init_copy,   MRB_ARGS_REQ(1)); /* 15.2.18.4.9  */
  _define_method(mrb, st,       "eql?",            _struct_eql,         MRB_ARGS_REQ(1)); /* 15.2.18.4.12(x)  */

  _define_method(mrb, st,        "size",           _struct_len,         MRB_ARGS_NONE());
  _define_method(mrb, st,        "length",         _struct_len,         MRB_ARGS_NONE());
  _define_method(mrb, st,        "to_a",           _struct_to_a,        MRB_ARGS_NONE());
  _define_method(mrb, st,        "values",         _struct_to_a,        MRB_ARGS_NONE());
  _define_method(mrb, st,        "to_h",           _struct_to_h,        MRB_ARGS_NONE());
  _define_method(mrb, st,        "values_at",      _struct_values_at,   MRB_ARGS_ANY());
}

void
_mruby_struct_gem_final(state* mrb)
{
}
