/*
** apibreak.h
**
*/

#ifndef APIBREAK_H_
#define APIBREAK_H_

#include <mruby.h>
#include "mrdb.h"

int32_t _debug_set_break_line(state *, _debug_context *, const char *, uint16_t);
int32_t _debug_set_break_method(state *, _debug_context *, const char *, const char *);
int32_t _debug_get_breaknum(state *, _debug_context *);
int32_t _debug_get_break_all(state *, _debug_context *, uint32_t, _debug_breakpoint bp[]);
int32_t _debug_get_break(state *, _debug_context *, uint32_t, _debug_breakpoint *);
int32_t _debug_delete_break(state *, _debug_context *, uint32_t);
int32_t _debug_delete_break_all(state *, _debug_context *);
int32_t _debug_enable_break(state *, _debug_context *, uint32_t);
int32_t _debug_enable_break_all(state *, _debug_context *);
int32_t _debug_disable_break(state *, _debug_context *, uint32_t);
int32_t _debug_disable_break_all(state *, _debug_context *);
int32_t _debug_check_breakpoint_line(state *, _debug_context *, const char *, uint16_t);
int32_t _debug_check_breakpoint_method(state *, _debug_context *, struct RClass *, _sym, _bool*);

#endif /* APIBREAK_H_ */
