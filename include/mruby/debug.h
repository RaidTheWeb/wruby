/*
** mruby/debug.h - mruby debug info
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_DEBUG_H
#define MRUBY_DEBUG_H

#include "common.h"

/**
 * MRuby Debugging.
 */
MRB_BEGIN_DECL

typedef enum _debug_line_type {
  _debug_line_ary = 0,
  _debug_line_flat_map = 1
} _debug_line_type;

typedef struct _irep_debug_info_line {
  uint32_t start_pos;
  uint16_t line;
} _irep_debug_info_line;

typedef struct _irep_debug_info_file {
  uint32_t start_pos;
  const char *filename;
  _sym filename_sym;
  uint32_t line_entry_count;
  _debug_line_type line_type;
  union {
    void *ptr;
    _irep_debug_info_line *flat_map;
    uint16_t *ary;
  } lines;
} _irep_debug_info_file;

typedef struct _irep_debug_info {
  uint32_t pc_count;
  uint16_t flen;
  _irep_debug_info_file **files;
} _irep_debug_info;

/*
 * get line from irep's debug info and program counter
 * @return returns NULL if not found
 */
MRB_API const char *_debug_get_filename(_irep *irep, ptrdiff_t pc);

/*
 * get line from irep's debug info and program counter
 * @return returns -1 if not found
 */
MRB_API int32_t _debug_get_line(_irep *irep, ptrdiff_t pc);

MRB_API _irep_debug_info_file *_debug_info_append_file(
    _state *mrb, _irep *irep,
    uint32_t start_pos, uint32_t end_pos);
MRB_API _irep_debug_info *_debug_info_alloc(_state *mrb, _irep *irep);
MRB_API void _debug_info_free(_state *mrb, _irep_debug_info *d);

MRB_END_DECL

#endif /* MRUBY_DEBUG_H */
