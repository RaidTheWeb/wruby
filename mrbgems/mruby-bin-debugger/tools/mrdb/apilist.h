/*
 * apilist.h
 */

#ifndef APILIST_H_
#define APILIST_H_

#include <mruby.h>
#include "mrdb.h"

int32_t _debug_list(state *, _debug_context *, char *, uint16_t, uint16_t);
char* _debug_get_source(state *, mrdb_state *, const char *, const char *);

#endif /* APILIST_H_ */
