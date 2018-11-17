/*
** compar.c - Comparable module
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>

void
init_comparable(state *mrb)
{
  define_module(mrb, "Comparable");  /* 15.3.3 */
}
