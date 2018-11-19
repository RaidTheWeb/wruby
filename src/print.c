/*
** print.c - Kernel.#p
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#ifndef MRB_DISABLE_STDIO
static void
printstr(_value obj, FILE *stream)
{
  if (_string_p(obj)) {
    fwrite(RSTRING_PTR(obj), RSTRING_LEN(obj), 1, stream);
    putc('\n', stream);
  }
}
#else
# define printstr(obj, stream) (void)0
#endif

MRB_API void
_p(_state *mrb, _value obj)
{
  printstr(_inspect(mrb, obj), stdout);
}

MRB_API void
_print_error(_state *mrb)
{
  _print_backtrace(mrb);
  printstr(_funcall(mrb, _obj_value(mrb->exc), "inspect", 0), stderr);
}

MRB_API void
_show_version(_state *mrb)
{
  printstr(_const_get(mrb, _obj_value(mrb->object_class), _intern_lit(mrb, "MRUBY_DESCRIPTION")), stdout);
}

MRB_API void
_show_copyright(_state *mrb)
{
  printstr(_const_get(mrb, _obj_value(mrb->object_class), _intern_lit(mrb, "MRUBY_COPYRIGHT")), stdout);
}
