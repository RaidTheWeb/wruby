/*
** error.c - Exception class
**
** See Copyright Notice in mruby.h
*/

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/irep.h>
#include <mruby/proc.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <mruby/debug.h>
#include <mruby/error.h>
#include <mruby/class.h>
#include <mruby/throw.h>

MRB_API _value
_exc_new(_state *mrb, struct RClass *c, const char *ptr, size_t len)
{
  _value arg = _str_new(mrb, ptr, len);
  return _obj_new(mrb, c, 1, &arg);
}

MRB_API _value
_exc_new_str(_state *mrb, struct RClass* c, _value str)
{
  str = _str_to_str(mrb, str);
  return _obj_new(mrb, c, 1, &str);
}

/*
 * call-seq:
 *    Exception.new(msg = nil)   ->  exception
 *
 *  Construct a new Exception object, optionally passing in
 *  a message.
 */

static _value
exc_initialize(_state *mrb, _value exc)
{
  _value mesg;
  _int argc;
  _value *argv;

  if (_get_args(mrb, "|o*!", &mesg, &argv, &argc) >= 1) {
    _iv_set(mrb, exc, _intern_lit(mrb, "mesg"), mesg);
  }
  return exc;
}

/*
 *  Document-method: exception
 *
 *  call-seq:
 *     exc.exception(string)  ->  an_exception or exc
 *
 *  With no argument, or if the argument is the same as the receiver,
 *  return the receiver. Otherwise, create a new
 *  exception object of the same class as the receiver, but with a
 *  message equal to <code>string</code>.
 *
 */

static _value
exc_exception(_state *mrb, _value self)
{
  _value exc;
  _value a;
  _int argc;

  argc = _get_args(mrb, "|o", &a);
  if (argc == 0) return self;
  if (_obj_equal(mrb, self, a)) return self;
  exc = _obj_clone(mrb, self);
  _iv_set(mrb, exc, _intern_lit(mrb, "mesg"), a);

  return exc;
}

/*
 * call-seq:
 *   exception.to_s   ->  string
 *
 * Returns exception's message (or the name of the exception if
 * no message is set).
 */

static _value
exc_to_s(_state *mrb, _value exc)
{
  _value mesg = _attr_get(mrb, exc, _intern_lit(mrb, "mesg"));
  struct RObject *p;

  if (!_string_p(mesg)) {
    return _str_new_cstr(mrb, _obj_classname(mrb, exc));
  }
  p = _obj_ptr(mesg);
  if (!p->c) {
    p->c = mrb->string_class;
  }
  return mesg;
}

/*
 * call-seq:
 *   exception.message   ->  string
 *
 * Returns the result of invoking <code>exception.to_s</code>.
 * Normally this returns the exception's message or name.
 */

static _value
exc_message(_state *mrb, _value exc)
{
  return _funcall(mrb, exc, "to_s", 0);
}

/*
 * call-seq:
 *   exception.inspect   -> string
 *
 * Returns this exception's file name, line number,
 * message and class name.
 * If file name or line number is not set,
 * returns message and class name.
 */

static _value
exc_inspect(_state *mrb, _value exc)
{
  _value str, mesg, file, line;
  _bool append_mesg;
  const char *cname;

  mesg = _attr_get(mrb, exc, _intern_lit(mrb, "mesg"));
  file = _attr_get(mrb, exc, _intern_lit(mrb, "file"));
  line = _attr_get(mrb, exc, _intern_lit(mrb, "line"));

  append_mesg = !_nil_p(mesg);
  if (append_mesg) {
    mesg = _obj_as_string(mrb, mesg);
    append_mesg = RSTRING_LEN(mesg) > 0;
  }

  cname = _obj_classname(mrb, exc);
  str = _str_new_cstr(mrb, cname);
  if (_string_p(file) && _fixnum_p(line)) {
    if (append_mesg) {
      str = _format(mrb, "%S:%S: %S (%S)", file, line, mesg, str);
    }
    else {
      str = _format(mrb, "%S:%S: %S", file, line, str);
    }
  }
  else if (append_mesg) {
    str = _format(mrb, "%S: %S", str, mesg);
  }
  return str;
}

void _keep_backtrace(_state *mrb, _value exc);

static void
set_backtrace(_state *mrb, _value exc, _value backtrace)
{
  if (!_array_p(backtrace)) {
  type_err:
    _raise(mrb, E_TYPE_ERROR, "backtrace must be Array of String");
  }
  else {
    const _value *p = RARRAY_PTR(backtrace);
    const _value *pend = p + RARRAY_LEN(backtrace);

    while (p < pend) {
      if (!_string_p(*p)) goto type_err;
      p++;
    }
  }
  _iv_set(mrb, exc, _intern_lit(mrb, "backtrace"), backtrace);
}

static _value
exc_set_backtrace(_state *mrb, _value exc)
{
  _value backtrace;

  _get_args(mrb, "o", &backtrace);
  set_backtrace(mrb, exc, backtrace);
  return backtrace;
}

static void
exc_debug_info(_state *mrb, struct RObject *exc)
{
  _callinfo *ci = mrb->c->ci;
  _code *pc = ci->pc;

  if (_obj_iv_defined(mrb, exc, _intern_lit(mrb, "file"))) return;
  while (ci >= mrb->c->cibase) {
    _code *err = ci->err;

    if (!err && pc) err = pc - 1;
    if (err && ci->proc && !MRB_PROC_CFUNC_P(ci->proc)) {
      _irep *irep = ci->proc->body.irep;

      int32_t const line = _debug_get_line(irep, err - irep->iseq);
      char const* file = _debug_get_filename(irep, err - irep->iseq);
      if (line != -1 && file) {
        _obj_iv_set(mrb, exc, _intern_lit(mrb, "file"), _str_new_cstr(mrb, file));
        _obj_iv_set(mrb, exc, _intern_lit(mrb, "line"), _fixnum_value(line));
        return;
      }
    }
    pc = ci->pc;
    ci--;
  }
}

void
_exc_set(_state *mrb, _value exc)
{
  if (_nil_p(exc)) {
    mrb->exc = 0;
  }
  else {
    mrb->exc = _obj_ptr(exc);
    if (mrb->gc.arena_idx > 0 &&
        (struct RBasic*)mrb->exc == mrb->gc.arena[mrb->gc.arena_idx-1]) {
      mrb->gc.arena_idx--;
    }
    if (!mrb->gc.out_of_memory && !MRB_FROZEN_P(mrb->exc)) {
      exc_debug_info(mrb, mrb->exc);
      _keep_backtrace(mrb, exc);
    }
  }
}

MRB_API _noreturn void
_exc_raise(_state *mrb, _value exc)
{
  if (!_obj_is_kind_of(mrb, exc, mrb->eException_class)) {
    _raise(mrb, E_TYPE_ERROR, "exception object expected");
  }
  _exc_set(mrb, exc);
  if (!mrb->jmp) {
    _p(mrb, exc);
    abort();
  }
  MRB_THROW(mrb->jmp);
}

MRB_API _noreturn void
_raise(_state *mrb, struct RClass *c, const char *msg)
{
  _exc_raise(mrb, _exc_new_str(mrb, c, _str_new_cstr(mrb, msg)));
}

MRB_API _value
_vformat(_state *mrb, const char *format, va_list ap)
{
  const char *p = format;
  const char *b = p;
  ptrdiff_t size;
  int ai0 = _gc_arena_save(mrb);
  _value ary = _ary_new_capa(mrb, 4);
  int ai = _gc_arena_save(mrb);

  while (*p) {
    const char c = *p++;

    if (c == '%') {
      if (*p == 'S') {
        _value val;

        size = p - b - 1;
        _ary_push(mrb, ary, _str_new(mrb, b, size));
        val = va_arg(ap, _value);
        _ary_push(mrb, ary, _obj_as_string(mrb, val));
        b = p + 1;
      }
    }
    else if (c == '\\') {
      if (*p) {
        size = p - b - 1;
        _ary_push(mrb, ary, _str_new(mrb, b, size));
        _ary_push(mrb, ary, _str_new(mrb, p, 1));
        b = ++p;
      }
      else {
        break;
      }
    }
    _gc_arena_restore(mrb, ai);
  }
  if (b == format) {
    _gc_arena_restore(mrb, ai0);
    return _str_new_cstr(mrb, format);
  }
  else {
    _value val;

    size = p - b;
    if (size > 0) {
      _ary_push(mrb, ary, _str_new(mrb, b, size));
    }
    val = _ary_join(mrb, ary, _nil_value());
    _gc_arena_restore(mrb, ai0);
    _gc_protect(mrb, val);
    return val;
  }
}

MRB_API _value
_format(_state *mrb, const char *format, ...)
{
  va_list ap;
  _value str;

  va_start(ap, format);
  str = _vformat(mrb, format, ap);
  va_end(ap);

  return str;
}

static _noreturn void
raise_va(_state *mrb, struct RClass *c, const char *fmt, va_list ap, int argc, _value *argv)
{
  _value mesg;

  mesg = _vformat(mrb, fmt, ap);
  if (argv == NULL) {
    argv = &mesg;
  }
  else {
    argv[0] = mesg;
  }
  _exc_raise(mrb, _obj_new(mrb, c, argc+1, argv));
}

MRB_API _noreturn void
_raisef(_state *mrb, struct RClass *c, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  raise_va(mrb, c, fmt, args, 0, NULL);
  va_end(args);
}

MRB_API _noreturn void
_name_error(_state *mrb, _sym id, const char *fmt, ...)
{
  _value argv[2];
  va_list args;

  va_start(args, fmt);
  argv[1] = _symbol_value(id);
  raise_va(mrb, E_NAME_ERROR, fmt, args, 1, argv);
  va_end(args);
}

MRB_API void
_warn(_state *mrb, const char *fmt, ...)
{
#ifndef MRB_DISABLE_STDIO
  va_list ap;
  _value str;

  va_start(ap, fmt);
  str = _vformat(mrb, fmt, ap);
  fputs("warning: ", stderr);
  fwrite(RSTRING_PTR(str), RSTRING_LEN(str), 1, stderr);
  va_end(ap);
#endif
}

MRB_API _noreturn void
_bug(_state *mrb, const char *fmt, ...)
{
#ifndef MRB_DISABLE_STDIO
  va_list ap;
  _value str;

  va_start(ap, fmt);
  str = _vformat(mrb, fmt, ap);
  fputs("bug: ", stderr);
  fwrite(RSTRING_PTR(str), RSTRING_LEN(str), 1, stderr);
  va_end(ap);
#endif
  exit(EXIT_FAILURE);
}

MRB_API _value
_make_exception(_state *mrb, _int argc, const _value *argv)
{
  _value mesg;
  int n;

  mesg = _nil_value();
  switch (argc) {
    case 0:
    break;
    case 1:
      if (_nil_p(argv[0]))
        break;
      if (_string_p(argv[0])) {
        mesg = _exc_new_str(mrb, E_RUNTIME_ERROR, argv[0]);
        break;
      }
      n = 0;
      goto exception_call;

    case 2:
    case 3:
      n = 1;
exception_call:
      {
        _sym exc = _intern_lit(mrb, "exception");
        if (_respond_to(mrb, argv[0], exc)) {
          mesg = _funcall_argv(mrb, argv[0], exc, n, argv+1);
        }
        else {
          /* undef */
          _raise(mrb, E_TYPE_ERROR, "exception class/object expected");
        }
      }

      break;
    default:
      _raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%S for 0..3)", _fixnum_value(argc));
      break;
  }
  if (argc > 0) {
    if (!_obj_is_kind_of(mrb, mesg, mrb->eException_class))
      _raise(mrb, mrb->eException_class, "exception object expected");
    if (argc > 2)
      set_backtrace(mrb, mesg, argv[2]);
  }

  return mesg;
}

MRB_API void
_sys_fail(_state *mrb, const char *mesg)
{
  struct RClass *sce;
  _int no;

  no = (_int)errno;
  if (_class_defined(mrb, "SystemCallError")) {
    sce = _class_get(mrb, "SystemCallError");
    if (mesg != NULL) {
      _funcall(mrb, _obj_value(sce), "_sys_fail", 2, _fixnum_value(no), _str_new_cstr(mrb, mesg));
    }
    else {
      _funcall(mrb, _obj_value(sce), "_sys_fail", 1, _fixnum_value(no));
    }
  }
  else {
    _raise(mrb, E_RUNTIME_ERROR, mesg);
  }
}

MRB_API _noreturn void
_no_method_error(_state *mrb, _sym id, _value args, char const* fmt, ...)
{
  _value exc;
  _value argv[3];
  va_list ap;

  va_start(ap, fmt);
  argv[0] = _vformat(mrb, fmt, ap);
  argv[1] = _symbol_value(id);
  argv[2] = args;
  va_end(ap);
  exc = _obj_new(mrb, E_NOMETHOD_ERROR, 3, argv);
  _exc_raise(mrb, exc);
}

void
_init_exception(_state *mrb)
{
  struct RClass *exception, *script_error, *stack_error, *nomem_error;

  mrb->eException_class = exception = _define_class(mrb, "Exception", mrb->object_class); /* 15.2.22 */
  MRB_SET_INSTANCE_TT(exception, MRB_TT_EXCEPTION);
  _define_class_method(mrb, exception, "exception", _instance_new,  MRB_ARGS_ANY());
  _define_method(mrb, exception, "exception",       exc_exception,     MRB_ARGS_ANY());
  _define_method(mrb, exception, "initialize",      exc_initialize,    MRB_ARGS_ANY());
  _define_method(mrb, exception, "to_s",            exc_to_s,          MRB_ARGS_NONE());
  _define_method(mrb, exception, "message",         exc_message,       MRB_ARGS_NONE());
  _define_method(mrb, exception, "inspect",         exc_inspect,       MRB_ARGS_NONE());
  _define_method(mrb, exception, "backtrace",       _exc_backtrace, MRB_ARGS_NONE());
  _define_method(mrb, exception, "set_backtrace",   exc_set_backtrace, MRB_ARGS_REQ(1));

  mrb->eStandardError_class = _define_class(mrb, "StandardError", mrb->eException_class); /* 15.2.23 */
  _define_class(mrb, "RuntimeError", mrb->eStandardError_class);          /* 15.2.28 */
  script_error = _define_class(mrb, "ScriptError", mrb->eException_class);                /* 15.2.37 */
  _define_class(mrb, "SyntaxError", script_error);                                        /* 15.2.38 */
  stack_error = _define_class(mrb, "SystemStackError", exception);
  mrb->stack_err = _obj_ptr(_exc_new_str_lit(mrb, stack_error, "stack level too deep"));

  nomem_error = _define_class(mrb, "NoMemoryError", exception);
  mrb->nomem_err = _obj_ptr(_exc_new_str_lit(mrb, nomem_error, "Out of memory"));
#ifdef MRB_GC_FIXED_ARENA
  mrb->arena_err = _obj_ptr(_exc_new_str_lit(mrb, nomem_error, "arena overflow error"));
#endif
}
