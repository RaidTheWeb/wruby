/*
** mrdb.h - mruby debugger
**
*/

#ifndef MRDB_H
#define MRDB_H

#include <mruby.h>

#include "mrdbconf.h"

#ifdef _MSC_VER
# define __func__ __FUNCTION__
#endif

#define MAX_COMMAND_WORD (16)

typedef enum debug_command_id {
  DBGCMD_RUN,
  DBGCMD_CONTINUE,
  DBGCMD_NEXT,
  DBGCMD_STEP,
  DBGCMD_BREAK,
  DBGCMD_INFO_BREAK,
  DBGCMD_WATCH,
  DBGCMD_INFO_WATCH,
  DBGCMD_ENABLE,
  DBGCMD_DISABLE,
  DBGCMD_DELETE,
  DBGCMD_PRINT,
  DBGCMD_DISPLAY,
  DBGCMD_INFO_DISPLAY,
  DBGCMD_DELETE_DISPLAY,
  DBGCMD_EVAL,
  DBGCMD_BACKTRACE,
  DBGCMD_LIST,
  DBGCMD_HELP,
  DBGCMD_QUIT,
  DBGCMD_UNKNOWN
} debug_command_id;

typedef enum dbgcmd_state {
  DBGST_CONTINUE,
  DBGST_PROMPT,
  DBGST_COMMAND_ERROR,
  DBGST_MAX,
  DBGST_RESTART
} dbgcmd_state;

typedef enum mrdb_exemode {
  DBG_INIT,
  DBG_RUN,
  DBG_STEP,
  DBG_NEXT,
  DBG_QUIT,
} mrdb_exemode;

typedef enum mrdb_exephase {
  DBG_PHASE_BEFORE_RUN,
  DBG_PHASE_RUNNING,
  DBG_PHASE_AFTER_RUN,
  DBG_PHASE_RESTART,
} mrdb_exephase;

typedef enum mrdb_brkmode {
  BRK_INIT,
  BRK_BREAK,
  BRK_STEP,
  BRK_NEXT,
  BRK_QUIT,
} mrdb_brkmode;

typedef enum {
  DEBUG_BPTYPE_NONE,
  DEBUG_BPTYPE_LINE,
  DEBUG_BPTYPE_METHOD,
} debug_bptype;

struct irep;
struct mrbc_context;
struct debug_context;

typedef struct debug_linepoint {
  const char *file;
  uint16_t lineno;
} debug_linepoint;

typedef struct debug_methodpoint {
  const char *class_name;
  const char *method_name;
} debug_methodpoint;

typedef struct debug_breakpoint {
  uint32_t bpno;
  uint8_t enable;
  debug_bptype type;
  union point {
    debug_linepoint linepoint;
    debug_methodpoint methodpoint;
  } point;
} debug_breakpoint;

typedef struct debug_context {
  struct irep *root_irep;
  struct irep *irep;
  code *pc;
  value *regs;

  const char *prvfile;
  int32_t prvline;
  callinfo *prvci;

  mrdb_exemode xm;
  mrdb_exephase xphase;
  mrdb_brkmode bm;
  int16_t bmi;

  uint16_t ccnt;
  uint16_t scnt;

  debug_breakpoint bp[MAX_BREAKPOINT];
  uint32_t bpnum;
  int32_t next_bpno;
  int32_t method_bpno;
  int32_t stopped_bpno;
  bool isCfunc;

  mrdb_exemode (*break_hook)(state *mrb, struct debug_context *dbg);

} debug_context;

typedef struct mrdb_state {
  char *command;
  uint8_t wcnt;
  uint8_t pi;
  char *words[MAX_COMMAND_WORD];
  const char *srcpath;
  uint32_t print_no;

  debug_context *dbg;
} mrdb_state;

typedef dbgcmd_state (*debug_command_func)(state*, mrdb_state*);

/* cmdrun.c */
dbgcmd_state dbgcmd_run(state*, mrdb_state*);
dbgcmd_state dbgcmd_continue(state*, mrdb_state*);
dbgcmd_state dbgcmd_step(state*, mrdb_state*);
dbgcmd_state dbgcmd_next(state*, mrdb_state*);
/* cmdbreak.c */
dbgcmd_state dbgcmd_break(state*, mrdb_state*);
dbgcmd_state dbgcmd_info_break(state*, mrdb_state*);
dbgcmd_state dbgcmd_delete(state*, mrdb_state*);
dbgcmd_state dbgcmd_enable(state*, mrdb_state*);
dbgcmd_state dbgcmd_disable(state*, mrdb_state*);
/* cmdprint.c */
dbgcmd_state dbgcmd_print(state*, mrdb_state*);
dbgcmd_state dbgcmd_eval(state*, mrdb_state*);
/* cmdmisc.c */
dbgcmd_state dbgcmd_list(state*, mrdb_state*);
dbgcmd_state dbgcmd_help(state*, mrdb_state*);
dbgcmd_state dbgcmd_quit(state*, mrdb_state*);

#endif
