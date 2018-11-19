#include <mruby.h>
#include <mruby/class.h>
#include <mruby/string.h>
#include <mruby/istruct.h>

static _value
istruct_test_initialize(_state *mrb, _value self)
{
  char *string = (char*)_istruct_ptr(self);
  _int size = _istruct_size();
  _value object;
  _get_args(mrb, "o", &object);

  if (_float_p(object)) {
    strncpy(string, "float", size-1);
  }
  else if (_fixnum_p(object)) {
    strncpy(string, "fixnum", size-1);
  }
  else if (_string_p(object)) {
    strncpy(string, "string", size-1);
  }
  else {
    strncpy(string, "anything", size-1);
  }

  string[size - 1] = 0; // force NULL at the end
  return self;
}

static _value
istruct_test_to_s(_state *mrb, _value self)
{
  return _str_new_cstr(mrb, (const char*)_istruct_ptr(self));
}

static _value
istruct_test_length(_state *mrb, _value self)
{
  return _fixnum_value(_istruct_size());
}

static _value
istruct_test_test_receive(_state *mrb, _value self)
{
  _value object;
  _get_args(mrb, "o", &object);
  if (_obj_class(mrb, object) != _class_get(mrb, "InlineStructTest"))
  {
    _raise(mrb, E_TYPE_ERROR, "Expected InlineStructTest");
  }
  return _bool_value(((char*)_istruct_ptr(object))[0] == 's');
}

static _value
istruct_test_test_receive_direct(_state *mrb, _value self)
{
  char *ptr;
  _get_args(mrb, "I", &ptr);
  return _bool_value(ptr[0] == 's');
}

static _value
istruct_test_mutate(_state *mrb, _value self)
{
  char *ptr = (char*)_istruct_ptr(self);
  memcpy(ptr, "mutate", 6);
  return _nil_value();
}

void _mruby_inline_struct_gem_test(_state *mrb)
{
  struct RClass *cls;

  cls = _define_class(mrb, "InlineStructTest", mrb->object_class);
  MRB_SET_INSTANCE_TT(cls, MRB_TT_ISTRUCT);
  _define_method(mrb, cls, "initialize", istruct_test_initialize, MRB_ARGS_REQ(1));
  _define_method(mrb, cls, "to_s", istruct_test_to_s, MRB_ARGS_NONE());
  _define_method(mrb, cls, "mutate", istruct_test_mutate, MRB_ARGS_NONE());
  _define_class_method(mrb, cls, "length", istruct_test_length, MRB_ARGS_NONE());
  _define_class_method(mrb, cls, "test_receive", istruct_test_test_receive, MRB_ARGS_REQ(1));
  _define_class_method(mrb, cls, "test_receive_direct", istruct_test_test_receive_direct, MRB_ARGS_REQ(1));
}
