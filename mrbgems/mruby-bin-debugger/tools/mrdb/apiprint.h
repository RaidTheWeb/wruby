/*
 * apiprint.h
 */

#ifndef APIPRINT_H_
#define APIPRINT_H_

#include <mruby.h>
#include "mrdb.h"

value debug_eval(state*, debug_context*, const char*, size_t, bool*);

#endif /* APIPRINT_H_ */
