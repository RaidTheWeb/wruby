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
static _value
_sym_all_symbols(_state *mrb, _value self)
{
  _sym i, lim;
  _value ary = _ary_new_capa(mrb, mrb->symidx);

  for (i=1, lim=mrb->symidx+1; i<lim; i++) {
    _ary_push(mrb, ary, _symbol_value(i));
  }

  return ary;
}

/*
 * call-seq:
 *   sym.length    -> integer
 *
 * Same as <code>sym.to_s.length</code>.
 */
static _value
_sym_length(_state *mrb, _value self)
{
  _int len;
  _sym2name_len(mrb, _symbol(self), &len);
  return _fixnum_value(len);
}

void
_mruby_symbol_ext_gem_init(_state* mrb)
{
  struct RClass *s = mrb->symbol_class;
  _define_class_method(mrb, s, "all_symbols", _sym_all_symbols, MRB_ARGS_NONE());
  _define_method(mrb, s, "length", _sym_length, MRB_ARGS_NONE());
  _define_method(mrb, s, "size", _sym_length, MRB_ARGS_NONE());
}

void
_mruby_symbol_ext_gem_final(_state* mrb)
{
}
