/*
** mruby/irep.h - irep structure
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
BEGIN_DECL

enum irep_pool_type {
  IREP_TT_STRING,
  IREP_TT_FIXNUM,
  IREP_TT_FLOAT,
};

struct locals {
  sym name;
  uint16_t r;
};

/* Program data array struct */
typedef struct irep {
  uint16_t nlocals;        /* Number of local variables */
  uint16_t nregs;          /* Number of register variables */
  uint8_t flags;

  code *iseq;
  value *pool;
  sym *syms;
  struct irep **reps;

  struct locals *lv;
  /* debug info */
  bool own_filename;
  const char *filename;
  uint16_t *lines;
  struct irep_debug_info* debug_info;

  uint16_t ilen, plen, slen, rlen;
  uint32_t refcnt;
} irep;

#define ISEQ_NO_FREE 1

API irep *add_irep(state *mrb);
API value load_irep(state*, const uint8_t*);
API value load_irep_cxt(state*, const uint8_t*, mrbc_context*);
void irep_free(state*, struct irep*);
void irep_incref(state*, struct irep*);
void irep_decref(state*, struct irep*);
void irep_cutref(state*, struct irep*);
void irep_remove_lv(state *mrb, irep *irep);

struct insn_data {
  uint8_t insn;
  uint16_t a;
  uint16_t b;
  uint8_t c;
};

struct insn_data decode_insn(code *pc);

END_DECL

#endif  /* MRUBY_IREP_H */
