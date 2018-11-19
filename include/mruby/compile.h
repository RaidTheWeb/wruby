/*
** mruby/compile.h - mruby parser
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_COMPILE_H
#define MRUBY_COMPILE_H

#include "common.h"

/**
 * MRuby Compiler
 */
MRB_BEGIN_DECL

#include <mruby.h>

struct _jmpbuf;

struct _parser_state;
/* load context */
typedef struct mrbc_context {
  _sym *syms;
  int slen;
  char *filename;
  short lineno;
  int (*partial_hook)(struct _parser_state*);
  void *partial_data;
  struct RClass *target_class;
  _bool capture_errors:1;
  _bool dump_result:1;
  _bool no_exec:1;
  _bool keep_lv:1;
  _bool no_optimize:1;
  _bool on_eval:1;

  size_t parser_nerr;
} mrbc_context;

MRB_API mrbc_context* mrbc_context_new(state *mrb);
MRB_API void mrbc_context_free(state *mrb, mrbc_context *cxt);
MRB_API const char *mrbc_filename(state *mrb, mrbc_context *c, const char *s);
MRB_API void mrbc_partial_hook(state *mrb, mrbc_context *c, int (*partial_hook)(struct _parser_state*), void*data);

/* AST node structure */
typedef struct _ast_node {
  struct _ast_node *car, *cdr;
  uint16_t lineno, filename_index;
} _ast_node;

/* lexer states */
enum _lex_state_enum {
  EXPR_BEG,                   /* ignore newline, +/- is a sign. */
  EXPR_END,                   /* newline significant, +/- is an operator. */
  EXPR_ENDARG,                /* ditto, and unbound braces. */
  EXPR_ENDFN,                 /* ditto, and unbound braces. */
  EXPR_ARG,                   /* newline significant, +/- is an operator. */
  EXPR_CMDARG,                /* newline significant, +/- is an operator. */
  EXPR_MID,                   /* newline significant, +/- is an operator. */
  EXPR_FNAME,                 /* ignore newline, no reserved words. */
  EXPR_DOT,                   /* right after '.' or '::', no reserved words. */
  EXPR_CLASS,                 /* immediate after 'class', no here document. */
  EXPR_VALUE,                 /* alike EXPR_BEG but label is disallowed. */
  EXPR_MAX_STATE
};

/* saved error message */
struct _parser_message {
  int lineno;
  int column;
  char* message;
};

#define STR_FUNC_PARSING 0x01
#define STR_FUNC_EXPAND  0x02
#define STR_FUNC_REGEXP  0x04
#define STR_FUNC_WORD    0x08
#define STR_FUNC_SYMBOL  0x10
#define STR_FUNC_ARRAY   0x20
#define STR_FUNC_HEREDOC 0x40
#define STR_FUNC_XQUOTE  0x80

enum _string_type {
  str_not_parsing  = (0),
  str_squote   = (STR_FUNC_PARSING),
  str_dquote   = (STR_FUNC_PARSING|STR_FUNC_EXPAND),
  str_regexp   = (STR_FUNC_PARSING|STR_FUNC_REGEXP|STR_FUNC_EXPAND),
  str_sword    = (STR_FUNC_PARSING|STR_FUNC_WORD|STR_FUNC_ARRAY),
  str_dword    = (STR_FUNC_PARSING|STR_FUNC_WORD|STR_FUNC_ARRAY|STR_FUNC_EXPAND),
  str_ssym     = (STR_FUNC_PARSING|STR_FUNC_SYMBOL),
  str_ssymbols = (STR_FUNC_PARSING|STR_FUNC_SYMBOL|STR_FUNC_ARRAY),
  str_dsymbols = (STR_FUNC_PARSING|STR_FUNC_SYMBOL|STR_FUNC_ARRAY|STR_FUNC_EXPAND),
  str_heredoc  = (STR_FUNC_PARSING|STR_FUNC_HEREDOC),
  str_xquote   = (STR_FUNC_PARSING|STR_FUNC_XQUOTE|STR_FUNC_EXPAND),
};

/* heredoc structure */
struct _parser_heredoc_info {
  _bool allow_indent:1;
  _bool line_head:1;
  enum _string_type type;
  const char *term;
  int term_len;
  _ast_node *doc;
};

#define MRB_PARSER_TOKBUF_MAX 65536
#define MRB_PARSER_TOKBUF_SIZE 256

/* parser structure */
struct _parser_state {
  state *mrb;
  struct _pool *pool;
  _ast_node *cells;
  const char *s, *send;
#ifndef MRB_DISABLE_STDIO
  FILE *f;
#endif
  mrbc_context *cxt;
  char const *filename;
  int lineno;
  int column;

  enum _lex_state_enum lstate;
  _ast_node *lex_strterm; /* (type nest_level beg . end) */

  unsigned int cond_stack;
  unsigned int cmdarg_stack;
  int paren_nest;
  int lpar_beg;
  int in_def, in_single;
  _bool cmd_start:1;
  _ast_node *locals;

  _ast_node *pb;
  char *tokbuf;
  char buf[MRB_PARSER_TOKBUF_SIZE];
  int tidx;
  int tsiz;

  _ast_node *all_heredocs; /* list of _parser_heredoc_info* */
  _ast_node *heredocs_from_nextline;
  _ast_node *parsing_heredoc;
  _ast_node *lex_strterm_before_heredoc;
  _bool heredoc_end_now:1; /* for mirb */

  void *ylval;

  size_t nerr;
  size_t nwarn;
  _ast_node *tree;

  _bool no_optimize:1;
  _bool on_eval:1;
  _bool capture_errors:1;
  struct _parser_message error_buffer[10];
  struct _parser_message warn_buffer[10];

  _sym* filename_table;
  size_t filename_table_length;
  int current_filename_index;

  struct _jmpbuf* jmp;
};

MRB_API struct _parser_state* _parser_new(state*);
MRB_API void _parser_free(struct _parser_state*);
MRB_API void _parser_parse(struct _parser_state*,mrbc_context*);

MRB_API void _parser_set_filename(struct _parser_state*, char const*);
MRB_API char const* _parser_get_filename(struct _parser_state*, uint16_t idx);

/* utility functions */
#ifndef MRB_DISABLE_STDIO
MRB_API struct _parser_state* _parse_file(state*,FILE*,mrbc_context*);
#endif
MRB_API struct _parser_state* _parse_string(state*,const char*,mrbc_context*);
MRB_API struct _parser_state* _parse_nstring(state*,const char*,size_t,mrbc_context*);
MRB_API struct RProc* _generate_code(state*, struct _parser_state*);
MRB_API value _load_exec(state *mrb, struct _parser_state *p, mrbc_context *c);

/* program load functions */
#ifndef MRB_DISABLE_STDIO
MRB_API value _load_file(state*,FILE*);
MRB_API value _load_file_cxt(state*,FILE*, mrbc_context *cxt);
#endif
MRB_API value _load_string(state *mrb, const char *s);
MRB_API value _load_nstring(state *mrb, const char *s, size_t len);
MRB_API value _load_string_cxt(state *mrb, const char *s, mrbc_context *cxt);
MRB_API value _load_nstring_cxt(state *mrb, const char *s, size_t len, mrbc_context *cxt);

/** @} */
MRB_END_DECL

#endif /* MRUBY_COMPILE_H */
