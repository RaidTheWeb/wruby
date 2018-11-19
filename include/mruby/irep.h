/*
** mruby/irep.h - _irep structure
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_IREP_H
#define MRUBY_IREP_H

#include "common.h"
#include <mruby/compile.h>

/**
 * Compiled mruby scripts.
 */
MRB_BEGIN_DECL

enum irep_pool_type {
  IREP_TT_STRING,
  IREP_TT_FIXNUM,
  IREP_TT_FLOAT,
};

struct _locals {
  _sym name;
  uint16_t r;
};

/* Program data array struct */
typedef struct _irep {
  uint16_t nlocals;        /* Number of local variables */
  uint16_t nregs;          /* Number of register variables */
  uint8_t flags;

  _code *iseq;
  _value *pool;
  _sym *syms;
  struct _irep **reps;

  struct _locals *lv;
  /* debug info */
  _bool own_filename;
  const char *filename;
  uint16_t *lines;
  struct _irep_debug_info* debug_info;

  uint16_t ilen, plen, slen, rlen;
  uint32_t refcnt;
} _irep;

#define MRB_ISEQ_NO_FREE 1

MRB_API _irep *_add_irep(_state *mrb);
MRB_API _value _load_irep(_state*, const uint8_t*);
MRB_API _value _load_irep_cxt(_state*, const uint8_t*, mrbc_context*);
void _irep_free(_state*, struct _irep*);
void _irep_incref(_state*, struct _irep*);
void _irep_decref(_state*, struct _irep*);
void _irep_cutref(_state*, struct _irep*);
void _irep_remove_lv(_state *mrb, _irep *irep);

struct _insn_data {
  uint8_t insn;
  uint16_t a;
  uint16_t b;
  uint8_t c;
};

struct _insn_data _decode_insn(_code *pc);

MRB_END_DECL

#endif  /* MRUBY_IREP_H */
