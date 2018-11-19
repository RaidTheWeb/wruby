/*
** compar.c - Comparable module
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>

void
_init_comparable(state *mrb)
{
  _define_module(mrb, "Comparable");  /* 15.3.3 */
}
