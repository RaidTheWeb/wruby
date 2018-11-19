/*
** enum.c - Enumerable module
**
** See Copyright Notice in mruby.h
*/

#include <mruby.h>
#include <mruby/proc.h>

/* internal method `__update_hash(oldhash, index, itemhash)` */
static _value
enum_update_hash(_state *mrb, _value self)
{
  _int hash;
  _int index;
  _int hv;
  _value item_hash;

  _get_args(mrb, "iio", &hash, &index, &item_hash);
  if (_fixnum_p(item_hash)) {
    hv = _fixnum(item_hash);
  }
#ifndef MRB_WITHOUT_FLOAT
  else if (_float_p(item_hash)) {
    hv = (_int)_float(item_hash);
  }
#endif
  else {
    _raise(mrb, E_TYPE_ERROR, "can't calculate hash");
    /* not reached */
    hv = 0;
  }
  hash ^= (hv << (index % 16));

  return _fixnum_value(hash);
}

void
_init_enumerable(_state *mrb)
{
  struct RClass *enumerable;
  enumerable = _define_module(mrb, "Enumerable");  /* 15.3.2 */
  _define_module_function(mrb, enumerable, "__update_hash", enum_update_hash, MRB_ARGS_REQ(1));
}
