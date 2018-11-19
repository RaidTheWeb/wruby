/*
** apibreak.c
**
*/

#include <string.h>
#include <mruby.h>
#include <mruby/irep.h>
#include "mrdb.h"
#include <mruby/debug.h>
#include <mruby/opcode.h>
#include <mruby/class.h>
#include <mruby/proc.h>
#include <mruby/variable.h>
#include "mrdberror.h"
#include "apibreak.h"

#define MAX_BREAKPOINTNO (MAX_BREAKPOINT * 1024)
#define $DEBUG_BP_FILE_OK   (0x0001)
#define $DEBUG_BP_LINENO_OK (0x0002)

static uint16_t
check_lineno($irep_debug_info_file *info_file, uint16_t lineno)
{
  uint32_t count = info_file->line_entry_count;
  uint16_t l_idx;

  if (info_file->line_type == $debug_line_ary) {
    for (l_idx = 0; l_idx < count; ++l_idx) {
      if (lineno == info_file->lines.ary[l_idx]) {
        return lineno;
      }
    }
  }
  else {
    for (l_idx = 0; l_idx < count; ++l_idx) {
      if (lineno == info_file->lines.flat_map[l_idx].line) {
        return lineno;
      }
    }
  }

  return 0;
}

static int32_t
get_break_index($debug_context *dbg, uint32_t bpno)
{
  uint32_t i;
  int32_t index;
  char hit = FALSE;

  for(i = 0 ; i < dbg->bpnum; i++) {
    if (dbg->bp[i].bpno == bpno) {
      hit = TRUE;
      index = i;
      break;
    }
  }

  if (hit == FALSE) {
    return $DEBUG_BREAK_INVALID_NO;
  }

  return index;
}

static void
free_breakpoint($state *mrb, $debug_breakpoint *bp)
{
  switch(bp->type) {
    case $DEBUG_BPTYPE_LINE:
      $free(mrb, (void*)bp->point.linepoint.file);
      break;
    case $DEBUG_BPTYPE_METHOD:
      $free(mrb, (void*)bp->point.methodpoint.method_name);
      if (bp->point.methodpoint.class_name != NULL) {
        $free(mrb, (void*)bp->point.methodpoint.class_name);
      }
      break;
    default:
      break;
  }
}

static uint16_t
check_file_lineno(struct $irep *irep, const char *file, uint16_t lineno)
{
  $irep_debug_info_file *info_file;
  uint16_t result = 0;
  uint16_t f_idx;
  uint16_t fix_lineno;
  uint16_t i;

  for (f_idx = 0; f_idx < irep->debug_info->flen; ++f_idx) {
    info_file = irep->debug_info->files[f_idx];
    if (!strcmp(info_file->filename, file)) {
      result = $DEBUG_BP_FILE_OK;

      fix_lineno = check_lineno(info_file, lineno);
      if (fix_lineno != 0) {
        return result | $DEBUG_BP_LINENO_OK;
      }
    }
    for (i=0; i < irep->rlen; ++i) {
      result  |= check_file_lineno(irep->reps[i], file, lineno);
      if (result == ($DEBUG_BP_FILE_OK | $DEBUG_BP_LINENO_OK)) {
        return result;
      }
    }
  }
  return result;
}

static int32_t
compare_break_method($state *mrb, $debug_breakpoint *bp, struct RClass *class_obj, $sym method_sym, $bool* isCfunc)
{
  const char* class_name;
  const char* method_name;
  $method_t m;
  struct RClass* sc;
  const char* sn;
  $sym ssym;
  $debug_methodpoint *method_p;
  $bool is_defined;

  method_name = $sym2name(mrb, method_sym);

  method_p = &bp->point.methodpoint;
  if (strcmp(method_p->method_name, method_name) == 0) {
    class_name = $class_name(mrb, class_obj);
    if (class_name == NULL) {
      if (method_p->class_name == NULL) {
        return bp->bpno;
      }
    }
    else if (method_p->class_name != NULL) {
      m = $method_search_vm(mrb, &class_obj, method_sym);
      if ($METHOD_UNDEF_P(m)) {
        return $DEBUG_OK;
      }
      if ($METHOD_CFUNC_P(m)) {
        *isCfunc = TRUE;
      }

      is_defined = $class_defined(mrb, method_p->class_name);
      if (is_defined == FALSE) {
        return $DEBUG_OK;
      }

      sc = $class_get(mrb, method_p->class_name);
      ssym = $symbol($check_intern_cstr(mrb, method_p->method_name));
      m = $method_search_vm(mrb, &sc, ssym);
      if ($METHOD_UNDEF_P(m)) {
        return $DEBUG_OK;
      }

      class_name = $class_name(mrb, class_obj);
      sn = $class_name(mrb, sc);
      if (strcmp(sn, class_name) == 0) {
        return bp->bpno;
      }
    }
  }
  return $DEBUG_OK;
}

int32_t
$debug_set_break_line($state *mrb, $debug_context *dbg, const char *file, uint16_t lineno)
{
  int32_t index;
  char* set_file;
  uint16_t result;

  if ((mrb == NULL)||(dbg == NULL)||(file == NULL)) {
    return $DEBUG_INVALID_ARGUMENT;
  }

  if (dbg->bpnum >= MAX_BREAKPOINT) {
    return $DEBUG_BREAK_NUM_OVER;
  }

  if (dbg->next_bpno > MAX_BREAKPOINTNO) {
    return $DEBUG_BREAK_NO_OVER;
  }

  /* file and lineno check (line type $debug_line_ary only.) */
  result = check_file_lineno(dbg->root_irep, file, lineno);
  if (result == 0) {
    return $DEBUG_BREAK_INVALID_FILE;
  }
  else if (result == $DEBUG_BP_FILE_OK) {
    return $DEBUG_BREAK_INVALID_LINENO;
  }

  set_file = (char*)$malloc(mrb, strlen(file) + 1);

  index = dbg->bpnum;
  dbg->bp[index].bpno = dbg->next_bpno;
  dbg->next_bpno++;
  dbg->bp[index].enable = TRUE;
  dbg->bp[index].type = $DEBUG_BPTYPE_LINE;
  dbg->bp[index].point.linepoint.lineno = lineno;
  dbg->bpnum++;

  strncpy(set_file, file, strlen(file) + 1);

  dbg->bp[index].point.linepoint.file = set_file;

  return dbg->bp[index].bpno;
}

int32_t
$debug_set_break_method($state *mrb, $debug_context *dbg, const char *class_name, const char *method_name)
{
  int32_t index;
  char* set_class;
  char* set_method;

  if ((mrb == NULL) || (dbg == NULL) || (method_name == NULL)) {
    return $DEBUG_INVALID_ARGUMENT;
  }

  if (dbg->bpnum >= MAX_BREAKPOINT) {
    return $DEBUG_BREAK_NUM_OVER;
  }

  if (dbg->next_bpno > MAX_BREAKPOINTNO) {
    return $DEBUG_BREAK_NO_OVER;
  }

  if (class_name != NULL) {
    set_class = (char*)$malloc(mrb, strlen(class_name) + 1);
    strncpy(set_class, class_name, strlen(class_name) + 1);
  }
  else {
    set_class = NULL;
  }

  set_method = (char*)$malloc(mrb, strlen(method_name) + 1);

  strncpy(set_method, method_name, strlen(method_name) + 1);

  index = dbg->bpnum;
  dbg->bp[index].bpno = dbg->next_bpno;
  dbg->next_bpno++;
  dbg->bp[index].enable = TRUE;
  dbg->bp[index].type = $DEBUG_BPTYPE_METHOD;
  dbg->bp[index].point.methodpoint.method_name = set_method;
  dbg->bp[index].point.methodpoint.class_name = set_class;
  dbg->bpnum++;

  return dbg->bp[index].bpno;
}

int32_t
$debug_get_breaknum($state *mrb, $debug_context *dbg)
{
  if ((mrb == NULL) || (dbg == NULL)) {
    return $DEBUG_INVALID_ARGUMENT;
  }

  return dbg->bpnum;
}

int32_t
$debug_get_break_all($state *mrb, $debug_context *dbg, uint32_t size, $debug_breakpoint *bp)
{
  uint32_t get_size = 0;

  if ((mrb == NULL) || (dbg == NULL) || (bp == NULL)) {
    return $DEBUG_INVALID_ARGUMENT;
  }

  if (dbg->bpnum >= size) {
    get_size = size;
  }
  else {
    get_size = dbg->bpnum;
  }

  memcpy(bp, dbg->bp, sizeof($debug_breakpoint) * get_size);

  return get_size;
}

int32_t
$debug_get_break($state *mrb, $debug_context *dbg, uint32_t bpno, $debug_breakpoint *bp)
{
  int32_t index;

  if ((mrb == NULL) || (dbg == NULL) || (bp == NULL)) {
    return $DEBUG_INVALID_ARGUMENT;
  }

  index = get_break_index(dbg, bpno);
  if (index == $DEBUG_BREAK_INVALID_NO) {
    return $DEBUG_BREAK_INVALID_NO;
  }

  bp->bpno = dbg->bp[index].bpno;
  bp->enable = dbg->bp[index].enable;
  bp->point = dbg->bp[index].point;
  bp->type = dbg->bp[index].type;

  return 0;
}

int32_t
$debug_delete_break($state *mrb, $debug_context *dbg, uint32_t bpno)
{
  uint32_t i;
  int32_t index;

  if ((mrb == NULL) ||(dbg == NULL)) {
    return $DEBUG_INVALID_ARGUMENT;
  }

  index = get_break_index(dbg, bpno);
  if (index == $DEBUG_BREAK_INVALID_NO) {
    return $DEBUG_BREAK_INVALID_NO;
  }

  free_breakpoint(mrb, &dbg->bp[index]);

  for(i = index ; i < dbg->bpnum; i++) {
    if ((i + 1) == dbg->bpnum) {
      memset(&dbg->bp[i], 0, sizeof($debug_breakpoint));
    }
    else {
      memcpy(&dbg->bp[i], &dbg->bp[i + 1], sizeof($debug_breakpoint));
    }
  }

  dbg->bpnum--;

  return $DEBUG_OK;
}

int32_t
$debug_delete_break_all($state *mrb, $debug_context *dbg)
{
  uint32_t i;

  if ((mrb == NULL) || (dbg == NULL)) {
    return $DEBUG_INVALID_ARGUMENT;
  }

  for(i = 0 ; i < dbg->bpnum ; i++) {
    free_breakpoint(mrb, &dbg->bp[i]);
  }

  dbg->bpnum = 0;

  return $DEBUG_OK;
}

int32_t
$debug_enable_break($state *mrb, $debug_context *dbg, uint32_t bpno)
{
  int32_t index = 0;

  if ((mrb == NULL) || (dbg == NULL)) {
    return $DEBUG_INVALID_ARGUMENT;
  }

  index = get_break_index(dbg, bpno);
  if (index == $DEBUG_BREAK_INVALID_NO) {
    return $DEBUG_BREAK_INVALID_NO;
  }

  dbg->bp[index].enable = TRUE;

  return $DEBUG_OK;
}

int32_t
$debug_enable_break_all($state *mrb, $debug_context *dbg)
{
  uint32_t i;

  if ((mrb == NULL) || (dbg == NULL)) {
    return $DEBUG_INVALID_ARGUMENT;
  }

  for(i = 0 ; i < dbg->bpnum; i++) {
    dbg->bp[i].enable = TRUE;
  }

  return $DEBUG_OK;
}

int32_t
$debug_disable_break($state *mrb, $debug_context *dbg, uint32_t bpno)
{
  int32_t index = 0;

  if ((mrb == NULL) || (dbg == NULL)) {
    return $DEBUG_INVALID_ARGUMENT;
  }

  index = get_break_index(dbg, bpno);
  if (index == $DEBUG_BREAK_INVALID_NO) {
    return $DEBUG_BREAK_INVALID_NO;
  }

  dbg->bp[index].enable = FALSE;

  return $DEBUG_OK;
}

int32_t
$debug_disable_break_all($state *mrb, $debug_context *dbg)
{
  uint32_t i;

  if ((mrb == NULL) || (dbg == NULL)) {
    return $DEBUG_INVALID_ARGUMENT;
  }

  for(i = 0 ; i < dbg->bpnum; i++) {
    dbg->bp[i].enable = FALSE;
  }

  return $DEBUG_OK;
}

static $bool
check_start_pc_for_line($irep *irep, $code *pc, uint16_t line)
{
  if (pc > irep->iseq) {
    if (line == $debug_get_line(irep, pc - irep->iseq - 1)) {
      return FALSE;
    }
  }
  return TRUE;
}

int32_t
$debug_check_breakpoint_line($state *mrb, $debug_context *dbg, const char *file, uint16_t line)
{
  $debug_breakpoint *bp;
  $debug_linepoint *line_p;
  uint32_t i;

  if ((mrb == NULL) || (dbg == NULL) || (file == NULL) || (line <= 0)) {
    return $DEBUG_INVALID_ARGUMENT;
  }

  if (!check_start_pc_for_line(dbg->irep, dbg->pc, line)) {
    return $DEBUG_OK;
  }

  bp = dbg->bp;
  for(i=0; i<dbg->bpnum; i++) {
    switch (bp->type) {
      case $DEBUG_BPTYPE_LINE:
        if (bp->enable == TRUE) {
          line_p = &bp->point.linepoint;
          if ((strcmp(line_p->file, file) == 0) && (line_p->lineno == line)) {
            return bp->bpno;
          }
        }
        break;
      case $DEBUG_BPTYPE_METHOD:
        break;
      case $DEBUG_BPTYPE_NONE:
      default:
        return $DEBUG_OK;
    }
    bp++;
  }
  return $DEBUG_OK;
}


int32_t
$debug_check_breakpoint_method($state *mrb, $debug_context *dbg, struct RClass *class_obj, $sym method_sym, $bool* isCfunc)
{
  $debug_breakpoint *bp;
  int32_t bpno;
  uint32_t i;

  if ((mrb == NULL) || (dbg == NULL) || (class_obj == NULL)) {
    return $DEBUG_INVALID_ARGUMENT;
  }

  bp = dbg->bp;
  for(i=0; i<dbg->bpnum; i++) {
    if (bp->type == $DEBUG_BPTYPE_METHOD) {
      if (bp->enable == TRUE) {
        bpno = compare_break_method(mrb, bp, class_obj, method_sym, isCfunc);
        if (bpno > 0) {
          return bpno;
        }
      }
    }
    else if (bp->type == $DEBUG_BPTYPE_NONE) {
      break;
    }
    bp++;
  }

  return 0;
}
