/*
 * apiprint.h
 */

#ifndef APIPRINT_H_
#define APIPRINT_H_

#include <mruby.h>
#include "mrdb.h"

value _debug_eval(state*, _debug_context*, const char*, size_t, _bool*);

#endif /* APIPRINT_H_ */
