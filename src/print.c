/*
** print.c - Kernel.#p
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/variable.h>

#ifndef $DISABLE_STDIO
static void
printstr($value obj, FILE *stream)
{
  if ($string_p(obj)) {
    fwrite(RSTRING_PTR(obj), RSTRING_LEN(obj), 1, stream);
    putc('\n', stream);
  }
}
#else
# define printstr(obj, stream) (void)0
#endif

$API void
$p($state *mrb, $value obj)
{
  printstr($inspect(mrb, obj), stdout);
}

$API void
$print_error($state *mrb)
{
  $print_backtrace(mrb);
  printstr($funcall(mrb, $obj_value(mrb->exc), "inspect", 0), stderr);
}

$API void
$show_version($state *mrb)
{
  printstr($const_get(mrb, $obj_value(mrb->object_class), $intern_lit(mrb, "MRUBY_DESCRIPTION")), stdout);
}

$API void
$show_copyright($state *mrb)
{
  printstr($const_get(mrb, $obj_value(mrb->object_class), $intern_lit(mrb, "MRUBY_COPYRIGHT")), stdout);
}
