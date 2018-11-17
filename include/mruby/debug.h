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
BEGIN_DECL

typedef enum debug_line_type {
  debug_line_ary = 0,
  debug_line_flat_map = 1
} debug_line_type;

typedef struct irep_debug_info_line {
  uint32_t start_pos;
  uint16_t line;
} irep_debug_info_line;

typedef struct irep_debug_info_file {
  uint32_t start_pos;
  const char *filename;
  sym filename_sym;
  uint32_t line_entry_count;
  debug_line_type line_type;
  union {
    void *ptr;
    irep_debug_info_line *flat_map;
    uint16_t *ary;
  } lines;
} irep_debug_info_file;

typedef struct irep_debug_info {
  uint32_t pc_count;
  uint16_t flen;
  irep_debug_info_file **files;
} irep_debug_info;

/*
 * get line from irep's debug info and program counter
 * @return returns NULL if not found
 */
API const char *debug_get_filename(irep *irep, ptrdiff_t pc);

/*
 * get line from irep's debug info and program counter
 * @return returns -1 if not found
 */
API int32_t debug_get_line(irep *irep, ptrdiff_t pc);

API irep_debug_info_file *debug_info_append_file(
    state *mrb, irep *irep,
    uint32_t start_pos, uint32_t end_pos);
API irep_debug_info *debug_info_alloc(state *mrb, irep *irep);
API void debug_info_free(state *mrb, irep_debug_info *d);

END_DECL

#endif /* MRUBY_DEBUG_H */
