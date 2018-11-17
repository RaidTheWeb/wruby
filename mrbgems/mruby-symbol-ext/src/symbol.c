#include <mruby.h>
#include <mruby/khash.h>
#include <mruby/array.h>

typedef struct symbol_name {
  size_t len;
  const char *name;
} symbol_name;

/*
 *  call-seq:
 *     Symbol.all_symbols    => array
 *
 *  Returns an array of all the symbols currently in Ruby's symbol
 *  table.
 *
 *     Symbol.all_symbols.size    #=> 903
 *     Symbol.all_symbols[1,20]   #=> [:floor, :ARGV, :Binding, :symlink,
 *                                     :chown, :EOFError, :$;, :String,
 *                                     :LOCK_SH, :"setuid?", :$<,
 *                                     :default_proc, :compact, :extend,
 *                                     :Tms, :getwd, :$=, :ThreadGroup,
 *                                     :wait2, :$>]
 */
static value
sym_all_symbols(state *mrb, value self)
{
  sym i, lim;
  value ary = ary_new_capa(mrb, mrb->symidx);

  for (i=1, lim=mrb->symidx+1; i<lim; i++) {
    ary_push(mrb, ary, symbol_value(i));
  }

  return ary;
}

/*
 * call-seq:
 *   sym.length    -> integer
 *
 * Same as <code>sym.to_s.length</code>.
 */
static value
sym_length(state *mrb, value self)
{
  int len;
  sym2name_len(mrb, symbol(self), &len);
  return fixnum_value(len);
}

void
mruby_symbol_ext_gem_init(state* mrb)
{
  struct RClass *s = mrb->symbol_class;
  define_class_method(mrb, s, "all_symbols", sym_all_symbols, ARGS_NONE());
  define_method(mrb, s, "length", sym_length, ARGS_NONE());
  define_method(mrb, s, "size", sym_length, ARGS_NONE());
}

void
mruby_symbol_ext_gem_final(state* mrb)
{
}
