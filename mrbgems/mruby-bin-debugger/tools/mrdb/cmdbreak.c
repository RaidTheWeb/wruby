/*
** cmdbreak.c
**
*/

#include <ctype.h>
#include <string.h>
#include <mruby.h>
#include <mruby/dump.h>
#include <mruby/debug.h>
#include <mruby/string.h>
#include "mrdb.h"
#include "mrdberror.h"
#include "apibreak.h"

#define BREAK_SET_MSG_LINE                  "Breakpoint %d: file %s, line %d.\n"
#define BREAK_SET_MSG_METHOD                "Breakpoint %d: method %s.\n"
#define BREAK_SET_MSG_CLASS_METHOD          "Breakpoint %d: class %s, method %s.\n"
#define BREAK_INFO_MSG_HEADER               "Num     Type           Enb What"
#define BREAK_INFO_MSG_LINEBREAK            "%-8ubreakpoint     %s   at %s:%u\n"
#define BREAK_INFO_MSG_METHODBREAK          "%-8ubreakpoint     %s   in %s:%s\n"
#define BREAK_INFO_MSG_METHODBREAK_NOCLASS  "%-8ubreakpoint     %s   in %s\n"
#define BREAK_INFO_MSG_ENABLE               "y"
#define BREAK_INFO_MSG_DISABLE              "n"

#define BREAK_ERR_MSG_INVALIDARG            "Internal error."
#define BREAK_ERR_MSG_BLANK                 "Try \'help break\' for more information."
#define BREAK_ERR_MSG_RANGEOVER             "The line number range is from 1 to 65535."
#define BREAK_ERR_MSG_NUMOVER               "Exceeded the setable number of breakpoint."
#define BREAK_ERR_MSG_NOOVER                "Breakno is over the available number.Please 'quit' and restart mrdb."
#define BREAK_ERR_MSG_INVALIDSTR            "String \'%s\' is invalid.\n"
#define BREAK_ERR_MSG_INVALIDLINENO         "Line %d in file \"%s\" is unavailable.\n"
#define BREAK_ERR_MSG_INVALIDCLASS          "Class name \'%s\' is invalid.\n"
#define BREAK_ERR_MSG_INVALIDMETHOD         "Method name \'%s\' is invalid.\n"
#define BREAK_ERR_MSG_INVALIDFILE           "Source file named \"%s\" is unavailable.\n"
#define BREAK_ERR_MSG_INVALIDBPNO           "warning: bad breakpoint number at or near '%s'\n"
#define BREAK_ERR_MSG_INVALIDBPNO_INFO      "Args must be numbers variables."
#define BREAK_ERR_MSG_NOBPNO                "No breakpoint number %d.\n"
#define BREAK_ERR_MSG_NOBPNO_INFO           "No breakpoint matching '%d'\n"
#define BREAK_ERR_MSG_NOBPNO_INFOALL        "No breakpoints."

#define LINENO_MAX_DIGIT 6
#define BPNO_LETTER_NUM 9

typedef int32_t (*all_command_func)($state *, $debug_context *);
typedef int32_t (*select_command_func)($state *, $debug_context *, uint32_t);

static void
print_api_common_error(int32_t error)
{
  switch(error) {
    case $DEBUG_INVALID_ARGUMENT:
      puts(BREAK_ERR_MSG_INVALIDARG);
      break;
    default:
      break;
  }
}

#undef STRTOUL
#define STRTOUL(ul,s) { \
    int i; \
    ul = 0; \
    for(i=0; ISDIGIT(s[i]); i++) ul = 10*ul + (s[i] -'0'); \
}

static int32_t
parse_breakpoint_no(char* args)
{
  char* ps = args;
  uint32_t l;

  if ((*ps == '0')||(strlen(ps) >= BPNO_LETTER_NUM)) {
    return 0;
  }

  while (!(ISBLANK(*ps)||ISCNTRL(*ps))) {
    if (!ISDIGIT(*ps)) {
      return 0;
    }
    ps++;
  }

  STRTOUL(l, args);
  return l;
}

static $bool
exe_set_command_all($state *mrb, mrdb_state *mrdb, all_command_func func)
{
  int32_t ret = $DEBUG_OK;

  if (mrdb->wcnt == 1) {
    ret = func(mrb, mrdb->dbg);
    print_api_common_error(ret);
    return TRUE;
  }
  return FALSE;
}

static void
exe_set_command_select($state *mrb, mrdb_state *mrdb, select_command_func func)
{
  char* ps;
  int32_t ret = $DEBUG_OK;
  int32_t bpno = 0;
  int32_t i;

  for(i=1; i<mrdb->wcnt; i++) {
    ps = mrdb->words[i];
    bpno = parse_breakpoint_no(ps);
    if (bpno == 0) {
      printf(BREAK_ERR_MSG_INVALIDBPNO, ps);
      break;
    }
    ret = func(mrb, mrdb->dbg, (uint32_t)bpno);
    if (ret == $DEBUG_BREAK_INVALID_NO) {
      printf(BREAK_ERR_MSG_NOBPNO, bpno);
    }
    else if (ret != $DEBUG_OK) {
      print_api_common_error(ret);
    }
  }
}

$debug_bptype
check_bptype(char* args)
{
  char* ps = args;

  if (ISBLANK(*ps)||ISCNTRL(*ps)) {
    puts(BREAK_ERR_MSG_BLANK);
    return $DEBUG_BPTYPE_NONE;
  }

  if (!ISDIGIT(*ps)) {
    return $DEBUG_BPTYPE_METHOD;
  }

  while (!(ISBLANK(*ps)||ISCNTRL(*ps))) {
    if (!ISDIGIT(*ps)) {
      printf(BREAK_ERR_MSG_INVALIDSTR, args);
      return $DEBUG_BPTYPE_NONE;
    }
    ps++;
  }

  if ((*args == '0')||(strlen(args) >= LINENO_MAX_DIGIT)) {
    puts(BREAK_ERR_MSG_RANGEOVER);
    return $DEBUG_BPTYPE_NONE;
  }

  return $DEBUG_BPTYPE_LINE;
}

static void
print_breakpoint($debug_breakpoint *bp)
{
  const char* enable_letter[] = {BREAK_INFO_MSG_DISABLE, BREAK_INFO_MSG_ENABLE};

  if (bp->type == $DEBUG_BPTYPE_LINE) {
    printf(BREAK_INFO_MSG_LINEBREAK,
      bp->bpno, enable_letter[bp->enable], bp->point.linepoint.file, bp->point.linepoint.lineno);
  }
  else {
    if (bp->point.methodpoint.class_name == NULL) {
      printf(BREAK_INFO_MSG_METHODBREAK_NOCLASS,
        bp->bpno, enable_letter[bp->enable], bp->point.methodpoint.method_name);
    }
    else {
      printf(BREAK_INFO_MSG_METHODBREAK,
        bp->bpno, enable_letter[bp->enable], bp->point.methodpoint.class_name, bp->point.methodpoint.method_name);
    }
  }
}

static void
info_break_all($state *mrb, mrdb_state *mrdb)
{
  int32_t bpnum = 0;
  int32_t i = 0;
  int32_t ret = $DEBUG_OK;
  $debug_breakpoint *bp_list;

  bpnum = $debug_get_breaknum(mrb, mrdb->dbg);
  if (bpnum < 0) {
    print_api_common_error(bpnum);
    return;
  }
  else if (bpnum == 0) {
    puts(BREAK_ERR_MSG_NOBPNO_INFOALL);
    return;
  }
  bp_list = ($debug_breakpoint*)$malloc(mrb, bpnum * sizeof($debug_breakpoint));

  ret = $debug_get_break_all(mrb, mrdb->dbg, (uint32_t)bpnum, bp_list);
  if (ret < 0) {
    print_api_common_error(ret);
    return;
  }
  puts(BREAK_INFO_MSG_HEADER);
  for(i = 0 ; i < bpnum ; i++) {
    print_breakpoint(&bp_list[i]);
  }

  $free(mrb, bp_list);
}

static void
info_break_select($state *mrb, mrdb_state *mrdb)
{
  int32_t ret = $DEBUG_OK;
  int32_t bpno = 0;
  char* ps = mrdb->command;
  $debug_breakpoint bp;
  $bool isFirst = TRUE;
  int32_t i;

  for(i=2; i<mrdb->wcnt; i++) {
    ps = mrdb->words[i];
    bpno = parse_breakpoint_no(ps);
    if (bpno == 0) {
      puts(BREAK_ERR_MSG_INVALIDBPNO_INFO);
      break;
    }

    ret = $debug_get_break(mrb, mrdb->dbg, bpno, &bp);
    if (ret == $DEBUG_BREAK_INVALID_NO) {
      printf(BREAK_ERR_MSG_NOBPNO_INFO, bpno);
      break;
    }
    else if (ret != $DEBUG_OK) {
      print_api_common_error(ret);
      break;
    }
    else if (isFirst == TRUE) {
      isFirst = FALSE;
      puts(BREAK_INFO_MSG_HEADER);
    }
    print_breakpoint(&bp);
  }
}

$debug_bptype
parse_breakcommand(mrdb_state *mrdb, const char **file, uint32_t *line, char **cname, char **method)
{
  $debug_context *dbg = mrdb->dbg;
  char *args;
  char *body;
  $debug_bptype type;
  uint32_t l;

  if (mrdb->wcnt <= 1) {
    puts(BREAK_ERR_MSG_BLANK);
    return $DEBUG_BPTYPE_NONE;
  }

  args = mrdb->words[1];
  if ((body = strrchr(args, ':')) == NULL) {
    body = args;
    type = check_bptype(body);
  }
  else {
    if (body == args) {
      printf(BREAK_ERR_MSG_INVALIDSTR, args);
      return $DEBUG_BPTYPE_NONE;
    }
    *body = '\0';
    type = check_bptype(++body);
  }

  switch(type) {
    case $DEBUG_BPTYPE_LINE:
      STRTOUL(l, body);
      if (l <= 65535) {
        *line = l;
        *file = (body == args)? $debug_get_filename(dbg->irep, dbg->pc - dbg->irep->iseq): args;
      }
      else {
        puts(BREAK_ERR_MSG_RANGEOVER);
        type = $DEBUG_BPTYPE_NONE;
      }
      break;
    case $DEBUG_BPTYPE_METHOD:
      if (body == args) {
        /* method only */
        if (ISUPPER(*body)||ISLOWER(*body)||(*body == '_')) {
          *method = body;
          *cname = NULL;
        }
        else {
          printf(BREAK_ERR_MSG_INVALIDMETHOD, args);
          type = $DEBUG_BPTYPE_NONE;
        }
      }
      else {
        if (ISUPPER(*args)) {
          switch(*body) {
            case '@': case '$': case '?': case '.': case ',': case ':':
            case ';': case '#': case '\\': case '\'': case '\"':
            printf(BREAK_ERR_MSG_INVALIDMETHOD, body);
            type = $DEBUG_BPTYPE_NONE;
            break;
          default:
            *method = body;
            *cname = args;
            break;
          }
        }
        else {
          printf(BREAK_ERR_MSG_INVALIDCLASS, args);
          type = $DEBUG_BPTYPE_NONE;
        }
      }
      break;
    case $DEBUG_BPTYPE_NONE:
    default:
      break;
  }

  return type;
}

dbgcmd_state
dbgcmd_break($state *mrb, mrdb_state *mrdb)
{
  $debug_bptype type;
  $debug_context *dbg = mrdb->dbg;
  const char *file = NULL;
  uint32_t line = 0;
  char *cname = NULL;
  char *method = NULL;
  int32_t ret;

  type = parse_breakcommand(mrdb, &file, &line, &cname, &method);
  switch (type) {
    case $DEBUG_BPTYPE_LINE:
      ret = $debug_set_break_line(mrb, dbg, file, line);
      break;
    case $DEBUG_BPTYPE_METHOD:
      ret = $debug_set_break_method(mrb, dbg, cname, method);
      break;
    case $DEBUG_BPTYPE_NONE:
    default:
      return DBGST_PROMPT;
  }

  if (ret >= 0) {
    if (type == $DEBUG_BPTYPE_LINE) {
      printf(BREAK_SET_MSG_LINE, ret, file, line);
    }
    else if ((type == $DEBUG_BPTYPE_METHOD)&&(cname == NULL)) {
      printf(BREAK_SET_MSG_METHOD, ret, method);
    }
    else {
      printf(BREAK_SET_MSG_CLASS_METHOD, ret, cname, method);
    }
  }
  else {
    switch (ret) {
      case $DEBUG_BREAK_INVALID_LINENO:
        printf(BREAK_ERR_MSG_INVALIDLINENO, line, file);
        break;
      case $DEBUG_BREAK_INVALID_FILE:
        printf(BREAK_ERR_MSG_INVALIDFILE, file);
        break;
      case $DEBUG_BREAK_NUM_OVER:
        puts(BREAK_ERR_MSG_NUMOVER);
        break;
      case $DEBUG_BREAK_NO_OVER:
        puts(BREAK_ERR_MSG_NOOVER);
        break;
      case $DEBUG_INVALID_ARGUMENT:
        puts(BREAK_ERR_MSG_INVALIDARG);
        break;
      case $DEBUG_NOBUF:
        puts("T.B.D.");
        break;
      default:
        break;
    }
  }

  return DBGST_PROMPT;
}

dbgcmd_state
dbgcmd_info_break($state *mrb, mrdb_state *mrdb)
{
  if (mrdb->wcnt == 2) {
    info_break_all(mrb, mrdb);
  }
  else {
    info_break_select(mrb, mrdb);
  }

  return DBGST_PROMPT;
}

dbgcmd_state
dbgcmd_delete($state *mrb, mrdb_state *mrdb)
{
  $bool ret = FALSE;

  ret = exe_set_command_all(mrb, mrdb, $debug_delete_break_all);
  if (ret != TRUE) {
    exe_set_command_select(mrb, mrdb, $debug_delete_break);
  }

  return DBGST_PROMPT;
}

dbgcmd_state
dbgcmd_enable($state *mrb, mrdb_state *mrdb)
{
  $bool ret = FALSE;

  ret = exe_set_command_all(mrb, mrdb, $debug_enable_break_all);
  if (ret != TRUE) {
    exe_set_command_select(mrb, mrdb, $debug_enable_break);
  }

  return DBGST_PROMPT;
}

dbgcmd_state
dbgcmd_disable($state *mrb, mrdb_state *mrdb)
{
  $bool ret = FALSE;

  ret = exe_set_command_all(mrb, mrdb, $debug_disable_break_all);
  if (ret != TRUE) {
    exe_set_command_select(mrb, mrdb, $debug_disable_break);
  }
  return DBGST_PROMPT;
}
