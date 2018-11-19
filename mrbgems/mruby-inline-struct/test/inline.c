#include <mruby.h>
#include <mruby/class.h>
#include <mruby/string.h>
#include <mruby/istruct.h>

static $value
istruct_test_initialize($state *mrb, $value self)
{
  char *string = (char*)$istruct_ptr(self);
  $int size = $istruct_size();
  $value object;
  $get_args(mrb, "o", &object);

  if ($float_p(object)) {
    strncpy(string, "float", size-1);
  }
  else if ($fixnum_p(object)) {
    strncpy(string, "fixnum", size-1);
  }
  else if ($string_p(object)) {
    strncpy(string, "string", size-1);
  }
  else {
    strncpy(string, "anything", size-1);
  }

  string[size - 1] = 0; // force NULL at the end
  return self;
}

static $value
istruct_test_to_s($state *mrb, $value self)
{
  return $str_new_cstr(mrb, (const char*)$istruct_ptr(self));
}

static $value
istruct_test_length($state *mrb, $value self)
{
  return $fixnum_value($istruct_size());
}

static $value
istruct_test_test_receive($state *mrb, $value self)
{
  $value object;
  $get_args(mrb, "o", &object);
  if ($obj_class(mrb, object) != $class_get(mrb, "InlineStructTest"))
  {
    $raise(mrb, E_TYPE_ERROR, "Expected InlineStructTest");
  }
  return $bool_value(((char*)$istruct_ptr(object))[0] == 's');
}

static $value
istruct_test_test_receive_direct($state *mrb, $value self)
{
  char *ptr;
  $get_args(mrb, "I", &ptr);
  return $bool_value(ptr[0] == 's');
}

static $value
istruct_test_mutate($state *mrb, $value self)
{
  char *ptr = (char*)$istruct_ptr(self);
  memcpy(ptr, "mutate", 6);
  return $nil_value();
}

void $mruby_inline_struct_gem_test($state *mrb)
{
  struct RClass *cls;

  cls = $define_class(mrb, "InlineStructTest", mrb->object_class);
  $SET_INSTANCE_TT(cls, $TT_ISTRUCT);
  $define_method(mrb, cls, "initialize", istruct_test_initialize, $ARGS_REQ(1));
  $define_method(mrb, cls, "to_s", istruct_test_to_s, $ARGS_NONE());
  $define_method(mrb, cls, "mutate", istruct_test_mutate, $ARGS_NONE());
  $define_class_method(mrb, cls, "length", istruct_test_length, $ARGS_NONE());
  $define_class_method(mrb, cls, "test_receive", istruct_test_test_receive, $ARGS_REQ(1));
  $define_class_method(mrb, cls, "test_receive_direct", istruct_test_test_receive_direct, $ARGS_REQ(1));
}
