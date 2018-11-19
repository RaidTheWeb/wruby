/*
** enum.c - Enumerable module
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/proc.h>

/* internal method `__update_hash(oldhash, index, itemhash)` */
static value
enum_update_hash(state *mrb, value self)
{
  int hash;
  int index;
  int hv;
  value item_hash;

  get_args(mrb, "iio", &hash, &index, &item_hash);
  if (fixnum_p(item_hash)) {
    hv = fixnum(item_hash);
  }
#ifndef WITHOUT_FLOAT
  else if (float_p(item_hash)) {
    hv = (int)float(item_hash);
  }
#endif
  else {
    raise(mrb, E_TYPE_ERROR, "can't calculate hash");
    /* not reached */
    hv = 0;
  }
  hash ^= (hv << (index % 16));

  return fixnum_value(hash);
}

void
init_enumerable(state *mrb)
{
  struct RClass *enumerable;
  enumerable = define_module_(mrb, "Enumerable");  /* 15.3.2 */
  define_module_function(mrb, enumerable, "__update_hash", enum_update_hash, ARGS_REQ(1));
}
