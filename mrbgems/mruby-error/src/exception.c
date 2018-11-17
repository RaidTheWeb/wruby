#include <mruby.h>
#include <mruby/throw.h>
#include <mruby/error.h>

API value
protect(state *mrb, func_t body, value data, bool *state)
{
  struct jmpbuf *prev_jmp = mrb->jmp;
  struct jmpbuf c_jmp;
  value result = nil_value();

  if (state) { *state = FALSE; }

  TRY(&c_jmp) {
    mrb->jmp = &c_jmp;
    result = body(mrb, data);
    mrb->jmp = prev_jmp;
  } CATCH(&c_jmp) {
    mrb->jmp = prev_jmp;
    result = obj_value(mrb->exc);
    mrb->exc = NULL;
    if (state) { *state = TRUE; }
  } END_EXC(&c_jmp);

  gc_protect(mrb, result);
  return result;
}

API value
ensure(state *mrb, func_t body, value b_data, func_t ensure, value e_data)
{
  struct jmpbuf *prev_jmp = mrb->jmp;
  struct jmpbuf c_jmp;
  value result;

  TRY(&c_jmp) {
    mrb->jmp = &c_jmp;
    result = body(mrb, b_data);
    mrb->jmp = prev_jmp;
  } CATCH(&c_jmp) {
    mrb->jmp = prev_jmp;
    ensure(mrb, e_data);
    THROW(mrb->jmp); /* rethrow catched exceptions */
  } END_EXC(&c_jmp);

  ensure(mrb, e_data);
  gc_protect(mrb, result);
  return result;
}

API value
rescue(state *mrb, func_t body, value b_data,
           func_t rescue, value r_data)
{
  return rescue_exceptions(mrb, body, b_data, rescue, r_data, 1, &mrb->eStandardError_class);
}

API value
rescue_exceptions(state *mrb, func_t body, value b_data, func_t rescue, value r_data,
                      int len, struct RClass **classes)
{
  struct jmpbuf *prev_jmp = mrb->jmp;
  struct jmpbuf c_jmp;
  value result;
  bool error_matched = FALSE;
  int i;

  TRY(&c_jmp) {
    mrb->jmp = &c_jmp;
    result = body(mrb, b_data);
    mrb->jmp = prev_jmp;
  } CATCH(&c_jmp) {
    mrb->jmp = prev_jmp;

    for (i = 0; i < len; ++i) {
      if (obj_is_kind_of(mrb, obj_value(mrb->exc), classes[i])) {
        error_matched = TRUE;
        break;
      }
    }

    if (!error_matched) { THROW(mrb->jmp); }

    mrb->exc = NULL;
    result = rescue(mrb, r_data);
  } END_EXC(&c_jmp);

  gc_protect(mrb, result);
  return result;
}

void
mruby_error_gem_init(state *mrb)
{
}

void
mruby_error_gem_final(state *mrb)
{
}
