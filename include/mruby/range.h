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
BEGIN_DECL

typedef struct range_edges {
  value beg;
  value end;
} range_edges;

struct RRange {
  OBJECT_HEADER;
  range_edges *edges;
  bool excl : 1;
};

API struct RRange* range_ptr(state *mrb, value v);
#define range_raw_ptr(v) ((struct RRange*)ptr(v))
#define range_value(p)  obj_value((void*)(p))

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
API value range_new(state *mrb, value start, value end, bool exclude);

API int range_beg_len(state *mrb, value range, int *begp, int *lenp, int len, bool trunc);
value get_values_at(state *mrb, value obj, int olen, int argc, const value *argv, value (*func)(state*, value, int));

END_DECL

#endif  /* MRUBY_RANGE_H */
