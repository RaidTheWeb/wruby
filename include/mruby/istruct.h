/*
** mruby/istruct.h - Inline structures
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_ISTRUCT_H
#define MRUBY_ISTRUCT_H

#include "common.h"
#include <string.h>

/**
 * Inline structures that fit in RVALUE
 *
 * They cannot have finalizer, and cannot have instance variables.
 */
MRB_BEGIN_DECL

#define ISTRUCT_DATA_SIZE (sizeof(void*) * 3)

struct RIstruct {
  MRB_OBJECT_HEADER;
  char inline_data[ISTRUCT_DATA_SIZE];
};

#define RISTRUCT(obj)         ((struct RIstruct*)(_ptr(obj)))
#define ISTRUCT_PTR(obj)      (RISTRUCT(obj)->inline_data)

MRB_INLINE _int _istruct_size()
{
  return ISTRUCT_DATA_SIZE;
}

MRB_INLINE void* _istruct_ptr(value object)
{
  return ISTRUCT_PTR(object);
}

MRB_INLINE void _istruct_copy(value dest, value src)
{
  memcpy(ISTRUCT_PTR(dest), ISTRUCT_PTR(src), ISTRUCT_DATA_SIZE);
}

MRB_END_DECL

#endif /* MRUBY_ISTRUCT_H */
