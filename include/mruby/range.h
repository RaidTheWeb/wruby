/*
** mruby/range.h - Range class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_RANGE_H
#define MRUBY_RANGE_H

#include "common.h"

/**
 * Range class
 */
MRB_BEGIN_DECL

typedef struct _range_edges {
  _value beg;
  _value end;
} _range_edges;

struct RRange {
  MRB_OBJECT_HEADER;
  _range_edges *edges;
  _bool excl : 1;
};

MRB_API struct RRange* _range_ptr(_state *mrb, _value v);
#define _range_raw_ptr(v) ((struct RRange*)_ptr(v))
#define _range_value(p)  _obj_value((void*)(p))

/*
 * Initializes a Range.
 *
 * If the third parameter is FALSE then it includes the last value in the range.
 * If the third parameter is TRUE then it excludes the last value in the range.
 *
 * @param start the beginning value.
 * @param end the ending value.
 * @param exclude represents the inclusion or exclusion of the last value.
 */
MRB_API _value _range_new(_state *mrb, _value start, _value end, _bool exclude);

MRB_API _int _range_beg_len(_state *mrb, _value range, _int *begp, _int *lenp, _int len, _bool trunc);
_value _get_values_at(_state *mrb, _value obj, _int olen, _int argc, const _value *argv, _value (*func)(_state*, _value, _int));

MRB_END_DECL

#endif  /* MRUBY_RANGE_H */
