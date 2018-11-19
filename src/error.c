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

API value
exc_new(state *mrb, struct RClass *c, const char *ptr, size_t len)
{
  value arg = str_new(mrb, ptr, len);
  return obj_new(mrb, c, 1, &arg);
}

API value
exc_new_str(state *mrb, struct RClass* c, value str)
{
  str = str_to_str(mrb, str);
  return obj_new(mrb, c, 1, &str);
}

/*
 * call-seq:
 *    Exception.new(msg = nil)   ->  exception
 *
 *  Construct a new Exception object, optionally passing in
 *  a message.
 */

static value
exc_initialize(state *mrb, value exc)
{
  value mesg;
  int argc;
  value *argv;

  if (get_args(mrb, "|o*!", &mesg, &argv, &argc) >= 1) {
    iv_set(mrb, exc, intern_lit(mrb, "mesg"), mesg);
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

static value
exc_exception(state *mrb, value self)
{
  value exc;
  value a;
  int argc;

  argc = get_args(mrb, "|o", &a);
  if (argc == 0) return self;
  if (obj_equal(mrb, self, a)) return self;
  exc = obj_clone(mrb, self);
  iv_set(mrb, exc, intern_lit(mrb, "mesg"), a);

  return exc;
}

/*
 * call-seq:
 *   exception.to_s   ->  string
 *
 * Returns exception's message (or the name of the exception if
 * no message is set).
 */

static value
exc_to_s(state *mrb, value exc)
{
  value mesg = attr_get(mrb, exc, intern_lit(mrb, "mesg"));
  struct RObject *p;

  if (!string_p(mesg)) {
    return str_new_cstr(mrb, obj_classname(mrb, exc));
  }
  p = obj_ptr(mesg);
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

static value
exc_message(state *mrb, value exc)
{
  return funcall(mrb, exc, "to_s", 0);
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

static value
exc_inspect(state *mrb, value exc)
{
  value str, mesg, file, line;
  bool append_mesg;
  const char *cname;

  mesg = attr_get(mrb, exc, intern_lit(mrb, "mesg"));
  file = attr_get(mrb, exc, intern_lit(mrb, "file"));
  line = attr_get(mrb, exc, intern_lit(mrb, "line"));

  append_mesg = !nil_p(mesg);
  if (append_mesg) {
    mesg = obj_as_string(mrb, mesg);
    append_mesg = RSTRING_LEN(mesg) > 0;
  }

  cname = obj_classname(mrb, exc);
  str = str_new_cstr(mrb, cname);
  if (string_p(file) && fixnum_p(line)) {
    if (append_mesg) {
      str = format(mrb, "%S:%S: %S (%S)", file, line, mesg, str);
    }
    else {
      str = format(mrb, "%S:%S: %S", file, line, str);
    }
  }
  else if (append_mesg) {
    str = format(mrb, "%S: %S", str, mesg);
  }
  return str;
}

void keep_backtrace(state *mrb, value exc);

static void
set_backtrace(state *mrb, value exc, value backtrace)
{
  if (!array_p(backtrace)) {
  type_err:
    raise(mrb, E_TYPE_ERROR, "backtrace must be Array of String");
  }
  else {
    const value *p = RARRAY_PTR(backtrace);
    const value *pend = p + RARRAY_LEN(backtrace);

    while (p < pend) {
      if (!string_p(*p)) goto type_err;
      p++;
    }
  }
  iv_set(mrb, exc, intern_lit(mrb, "backtrace"), backtrace);
}

static value
exc_set_backtrace(state *mrb, value exc)
{
  value backtrace;

  get_args(mrb, "o", &backtrace);
  set_backtrace(mrb, exc, backtrace);
  return backtrace;
}

static void
exc_debug_info(state *mrb, struct RObject *exc)
{
  callinfo *ci = mrb->c->ci;
  code *pc = ci->pc;

  if (obj_iv_defined(mrb, exc, intern_lit(mrb, "file"))) return;
  while (ci >= mrb->c->cibase) {
    code *err = ci->err;

    if (!err && pc) err = pc - 1;
    if (err && ci->proc && !PROC_CFUNC_P(ci->proc)) {
      irep *irep = ci->proc->body.irep;

      int32_t const line = debug_get_line(irep, err - irep->iseq);
      char const* file = debug_get_filename(irep, err - irep->iseq);
      if (line != -1 && file) {
        obj_iv_set(mrb, exc, intern_lit(mrb, "file"), str_new_cstr(mrb, file));
        obj_iv_set(mrb, exc, intern_lit(mrb, "line"), fixnum_value(line));
        return;
      }
    }
    pc = ci->pc;
    ci--;
  }
}

void
exc_set(state *mrb, value exc)
{
  if (nil_p(exc)) {
    mrb->exc = 0;
  }
  else {
    mrb->exc = obj_ptr(exc);
    if (mrb->gc.arena_idx > 0 &&
        (struct RBasic*)mrb->exc == mrb->gc.arena[mrb->gc.arena_idx-1]) {
      mrb->gc.arena_idx--;
    }
    if (!mrb->gc.out_of_memory && !FROZEN_P(mrb->exc)) {
      exc_debug_info(mrb, mrb->exc);
      keep_backtrace(mrb, exc);
    }
  }
}

API noreturn void
exc_raise(state *mrb, value exc)
{
  if (!obj_is_kind_of(mrb, exc, mrb->eException_class)) {
    raise(mrb, E_TYPE_ERROR, "exception object expected");
  }
  exc_set(mrb, exc);
  if (!mrb->jmp) {
    p(mrb, exc);
    abort();
  }
  THROW(mrb->jmp);
}

API noreturn void
raise(state *mrb, struct RClass *c, const char *msg)
{
  exc_raise(mrb, exc_new_str(mrb, c, str_new_cstr(mrb, msg)));
}

API value
vformat(state *mrb, const char *format, va_list ap)
{
  const char *p = format;
  const char *b = p;
  ptrdiff_t size;
  int ai0 = gc_arena_save(mrb);
  value ary = a_ary_new_capa(mrb, 4);
  int ai = gc_arena_save(mrb);

  while (*p) {
    const char c = *p++;

    if (c == '%') {
      if (*p == 'S') {
        value val;

        size = p - b - 1;
        ary_push(mrb, ary, str_new(mrb, b, size));
        val = va_arg(ap, value);
        ary_push(mrb, ary, obj_as_string(mrb, val));
        b = p + 1;
      }
    }
    else if (c == '\\') {
      if (*p) {
        size = p - b - 1;
        ary_push(mrb, ary, str_new(mrb, b, size));
        ary_push(mrb, ary, str_new(mrb, p, 1));
        b = ++p;
      }
      else {
        break;
      }
    }
    gc_arena_restore(mrb, ai);
  }
  if (b == format) {
    gc_arena_restore(mrb, ai0);
    return str_new_cstr(mrb, format);
  }
  else {
    value val;

    size = p - b;
    if (size > 0) {
      ary_push(mrb, ary, str_new(mrb, b, size));
    }
    val = ary_join(mrb, ary, nil_value());
    gc_arena_restore(mrb, ai0);
    gc_protect(mrb, val);
    return val;
  }
}

API value
format(state *mrb, const char *format, ...)
{
  va_list ap;
  value str;

  va_start(ap, format);
  str = vformat(mrb, format, ap);
  va_end(ap);

  return str;
}

static noreturn void
raise_va(state *mrb, struct RClass *c, const char *fmt, va_list ap, int argc, value *argv)
{
  value mesg;

  mesg = vformat(mrb, fmt, ap);
  if (argv == NULL) {
    argv = &mesg;
  }
  else {
    argv[0] = mesg;
  }
  exc_raise(mrb, obj_new(mrb, c, argc+1, argv));
}

API noreturn void
raisef(state *mrb, struct RClass *c, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  raise_va(mrb, c, fmt, args, 0, NULL);
  va_end(args);
}

API noreturn void
name_error(state *mrb, sym id, const char *fmt, ...)
{
  value argv[2];
  va_list args;

  va_start(args, fmt);
  argv[1] = symbol_value(id);
  raise_va(mrb, E_NAME_ERROR, fmt, args, 1, argv);
  va_end(args);
}

API void
warn(state *mrb, const char *fmt, ...)
{
#ifndef DISABLE_STDIO
  va_list ap;
  value str;

  va_start(ap, fmt);
  str = vformat(mrb, fmt, ap);
  fputs("warning: ", stderr);
  fwrite(RSTRING_PTR(str), RSTRING_LEN(str), 1, stderr);
  va_end(ap);
#endif
}

API noreturn void
bug(state *mrb, const char *fmt, ...)
{
#ifndef DISABLE_STDIO
  va_list ap;
  value str;

  va_start(ap, fmt);
  str = vformat(mrb, fmt, ap);
  fputs("bug: ", stderr);
  fwrite(RSTRING_PTR(str), RSTRING_LEN(str), 1, stderr);
  va_end(ap);
#endif
  exit(EXIT_FAILURE);
}

API value
make_exception(state *mrb, int argc, const value *argv)
{
  value mesg;
  int n;

  mesg = nil_value();
  switch (argc) {
    case 0:
    break;
    case 1:
      if (nil_p(argv[0]))
        break;
      if (string_p(argv[0])) {
        mesg = exc_new_str(mrb, E_RUNTIME_ERROR, argv[0]);
        break;
      }
      n = 0;
      goto exception_call;

    case 2:
    case 3:
      n = 1;
exception_call:
      {
        sym exc = intern_lit(mrb, "exception");
        if (respond_to(mrb, argv[0], exc)) {
          mesg = funcall_argv(mrb, argv[0], exc, n, argv+1);
        }
        else {
          /* undef */
          raise(mrb, E_TYPE_ERROR, "exception class/object expected");
        }
      }

      break;
    default:
      raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%S for 0..3)", fixnum_value(argc));
      break;
  }
  if (argc > 0) {
    if (!obj_is_kind_of(mrb, mesg, mrb->eException_class))
      raise(mrb, mrb->eException_class, "exception object expected");
    if (argc > 2)
      set_backtrace(mrb, mesg, argv[2]);
  }

  return mesg;
}

API void
sys_fail(state *mrb, const char *mesg)
{
  struct RClass *sce;
  int no;

  no = (int)errno;
  if (class_defined(mrb, "SystemCallError")) {
    sce = class_get(mrb, "SystemCallError");
    if (mesg != NULL) {
      funcall(mrb, obj_value(sce), "_sys_fail", 2, fixnum_value(no), str_new_cstr(mrb, mesg));
    }
    else {
      funcall(mrb, obj_value(sce), "_sys_fail", 1, fixnum_value(no));
    }
  }
  else {
    raise(mrb, E_RUNTIME_ERROR, mesg);
  }
}

API noreturn void
no_method_error(state *mrb, sym id, value args, char const* fmt, ...)
{
  value exc;
  value argv[3];
  va_list ap;

  va_start(ap, fmt);
  argv[0] = vformat(mrb, fmt, ap);
  argv[1] = symbol_value(id);
  argv[2] = args;
  va_end(ap);
  exc = obj_new(mrb, E_NOMETHOD_ERROR, 3, argv);
  exc_raise(mrb, exc);
}

void
init_exception(state *mrb)
{
  struct RClass *exception, *script_error, *stack_error, *nomem_error;

  mrb->eException_class = exception = define_class(mrb, "Exception", mrb->object_class); /* 15.2.22 */
  SET_INSTANCE_TT(exception, TT_EXCEPTION);
  define_class_method(mrb, exception, "exception", instance_new,  ARGS_ANY());
  define_method(mrb, exception, "exception",       exc_exception,     ARGS_ANY());
  define_method(mrb, exception, "initialize",      exc_initialize,    ARGS_ANY());
  define_method(mrb, exception, "to_s",            exc_to_s,          ARGS_NONE());
  define_method(mrb, exception, "message",         exc_message,       ARGS_NONE());
  define_method(mrb, exception, "inspect",         exc_inspect,       ARGS_NONE());
  define_method(mrb, exception, "backtrace",       exc_backtrace, ARGS_NONE());
  define_method(mrb, exception, "set_backtrace",   exc_set_backtrace, ARGS_REQ(1));

  mrb->eStandardError_class = define_class(mrb, "StandardError", mrb->eException_class); /* 15.2.23 */
  define_class(mrb, "RuntimeError", mrb->eStandardError_class);          /* 15.2.28 */
  script_error = define_class(mrb, "ScriptError", mrb->eException_class);                /* 15.2.37 */
  define_class(mrb, "SyntaxError", script_error);                                        /* 15.2.38 */
  stack_error = define_class(mrb, "SystemStackError", exception);
  mrb->stack_err = obj_ptr(exc_new_str_lit(mrb, stack_error, "stack level too deep"));

  nomem_error = define_class(mrb, "NoMemoryError", exception);
  mrb->nomem_err = obj_ptr(exc_new_str_lit(mrb, nomem_error, "Out of memory"));
#ifdef GC_FIXED_ARENA
  mrb->arena_err = obj_ptr(exc_new_str_lit(mrb, nomem_error, "arena overflow error"));
#endif
}
