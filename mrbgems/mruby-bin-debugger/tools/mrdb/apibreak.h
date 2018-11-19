/*
** apibreak.h
**
*/

#ifndef APIBREAK_H_
#define APIBREAK_H_

#include <mruby.h>
#include "mrdb.h"

int32_t $debug_set_break_line($state *, $debug_context *, const char *, uint16_t);
int32_t $debug_set_break_method($state *, $debug_context *, const char *, const char *);
int32_t $debug_get_breaknum($state *, $debug_context *);
int32_t $debug_get_break_all($state *, $debug_context *, uint32_t, $debug_breakpoint bp[]);
int32_t $debug_get_break($state *, $debug_context *, uint32_t, $debug_breakpoint *);
int32_t $debug_delete_break($state *, $debug_context *, uint32_t);
int32_t $debug_delete_break_all($state *, $debug_context *);
int32_t $debug_enable_break($state *, $debug_context *, uint32_t);
int32_t $debug_enable_break_all($state *, $debug_context *);
int32_t $debug_disable_break($state *, $debug_context *, uint32_t);
int32_t $debug_disable_break_all($state *, $debug_context *);
int32_t $debug_check_breakpoint_line($state *, $debug_context *, const char *, uint16_t);
int32_t $debug_check_breakpoint_method($state *, $debug_context *, struct RClass *, $sym, $bool*);

#endif /* APIBREAK_H_ */
