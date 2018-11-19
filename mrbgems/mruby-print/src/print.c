#include <mruby.h>
#include <mruby/string.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(_WIN32)
# include <windows.h>
# include <io.h>
#ifdef _MSC_VER
# define isatty(x) _isatty(x)
# define fileno(x) _fileno(x)
#endif
#endif

static void
printstr(state *mrb, value obj)
{
  if (_string_p(obj)) {
#if defined(_WIN32)
    if (isatty(fileno(stdout))) {
      DWORD written;
      int mlen = (int)RSTRING_LEN(obj);
      char* utf8 = RSTRING_PTR(obj);
      int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, mlen, NULL, 0);
      wchar_t* utf16 = (wchar_t*)_malloc(mrb, (wlen+1) * sizeof(wchar_t));
      if (utf16 == NULL) return;
      if (MultiByteToWideChar(CP_UTF8, 0, utf8, mlen, utf16, wlen) > 0) {
        utf16[wlen] = 0;
        WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),
          utf16, wlen, &written, NULL);
      }
      _free(mrb, utf16);
    } else
#endif
      fwrite(RSTRING_PTR(obj), RSTRING_LEN(obj), 1, stdout);
    fflush(stdout);
  }
}

/* 15.3.1.2.9  */
/* 15.3.1.3.34 */
value
_printstr(state *mrb, value self)
{
  value argv;

  _get_args(mrb, "o", &argv);
  printstr(mrb, argv);

  return argv;
}

void
_mruby_print_gem_init(state* mrb)
{
  struct RClass *krn;
  krn = mrb->kernel_module;
  _define_method(mrb, krn, "__printstr__", _printstr, MRB_ARGS_REQ(1));
}

void
_mruby_print_gem_final(state* mrb)
{
}
