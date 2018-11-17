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
BEGIN_DECL

#include <mruby.h>

struct jmpbuf;

struct parser_state;
/* load context */
typedef struct mrbc_context {
  sym *syms;
  int slen;
  char *filename;
  short lineno;
  int (*partial_hook)(struct parser_state*);
  void *partial_data;
  struct RClass *target_class;
  bool capture_errors:1;
  bool dump_result:1;
  bool no_exec:1;
  bool keep_lv:1;
  bool no_optimize:1;
  bool on_eval:1;

  size_t parser_nerr;
} mrbc_context;

API mrbc_context* mrbc_context_new(state *mrb);
API void mrbc_context_free(state *mrb, mrbc_context *cxt);
API const char *mrbc_filename(state *mrb, mrbc_context *c, const char *s);
API void mrbc_partial_hook(state *mrb, mrbc_context *c, int (*partial_hook)(struct parser_state*), void*data);

/* AST node structure */
typedef struct ast_node {
  struct ast_node *car, *cdr;
  uint16_t lineno, filename_index;
} ast_node;

/* lexer states */
enum lex_state_enum {
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
struct parser_message {
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

enum string_type {
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
struct parser_heredoc_info {
  bool allow_indent:1;
  bool line_head:1;
  enum string_type type;
  const char *term;
  int term_len;
  ast_node *doc;
};

#define PARSER_TOKBUF_MAX 65536
#define PARSER_TOKBUF_SIZE 256

/* parser structure */
struct parser_state {
  state *mrb;
  struct pool *pool;
  ast_node *cells;
  const char *s, *send;
#ifndef DISABLE_STDIO
  FILE *f;
#endif
  mrbc_context *cxt;
  char const *filename;
  int lineno;
  int column;

  enum lex_state_enum lstate;
  ast_node *lex_strterm; /* (type nest_level beg . end) */

  unsigned int cond_stack;
  unsigned int cmdarg_stack;
  int paren_nest;
  int lpar_beg;
  int in_def, in_single;
  bool cmd_start:1;
  ast_node *locals;

  ast_node *pb;
  char *tokbuf;
  char buf[PARSER_TOKBUF_SIZE];
  int tidx;
  int tsiz;

  ast_node *all_heredocs; /* list of parser_heredoc_info* */
  ast_node *heredocs_from_nextline;
  ast_node *parsing_heredoc;
  ast_node *lex_strterm_before_heredoc;
  bool heredoc_end_now:1; /* for mirb */

  void *ylval;

  size_t nerr;
  size_t nwarn;
  ast_node *tree;

  bool no_optimize:1;
  bool on_eval:1;
  bool capture_errors:1;
  struct parser_message error_buffer[10];
  struct parser_message warn_buffer[10];

  sym* filename_table;
  size_t filename_table_length;
  int current_filename_index;

  struct jmpbuf* jmp;
};

API struct parser_state* parser_new(state*);
API void parser_free(struct parser_state*);
API void parser_parse(struct parser_state*,mrbc_context*);

API void parser_set_filename(struct parser_state*, char const*);
API char const* parser_get_filename(struct parser_state*, uint16_t idx);

/* utility functions */
#ifndef DISABLE_STDIO
API struct parser_state* parse_file(state*,FILE*,mrbc_context*);
#endif
API struct parser_state* parse_string(state*,const char*,mrbc_context*);
API struct parser_state* parse_nstring(state*,const char*,size_t,mrbc_context*);
API struct RProc* generate_code(state*, struct parser_state*);
API value load_exec(state *mrb, struct parser_state *p, mrbc_context *c);

/* program load functions */
#ifndef DISABLE_STDIO
API value load_file(state*,FILE*);
API value load_file_cxt(state*,FILE*, mrbc_context *cxt);
#endif
API value load_string(state *mrb, const char *s);
API value load_nstring(state *mrb, const char *s, size_t len);
API value load_string_cxt(state *mrb, const char *s, mrbc_context *cxt);
API value load_nstring_cxt(state *mrb, const char *s, size_t len, mrbc_context *cxt);

/** @} */
END_DECL

#endif /* MRUBY_COMPILE_H */
