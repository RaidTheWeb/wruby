#include <sys/types.h>
#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)

#include <winsock.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#if (!defined __MINGW64__) && (!defined __MINGW32__)
typedef int mode_t;
#endif

#define open _open
#define close _close

#ifdef _MSC_VER
#include <sys/stat.h>

static int
mkstemp(char *p)
{
  int fd;
  char* fname = _mktemp(p);
  if (fname == NULL)
    return -1;
  fd = open(fname, O_RDWR | O_CREAT | O_EXCL, _S_IREAD | _S_IWRITE);
  if (fd >= 0)
    return fd;
  return -1;
}
#endif

static char*
mkdtemp(char *temp)
{
  char *path = _mktemp(temp);
  if (path[0] == 0) return NULL;
  if (_mkdir(path) < 0) return NULL;
  return path;
}

#define umask(mode) _umask(mode)
#define rmdir(path) _rmdir(path)
#else
  #include <sys/socket.h>
  #include <unistd.h>
  #include <sys/un.h>
#endif

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include "mruby.h"
#include "mruby/array.h"
#include "mruby/error.h"
#include "mruby/string.h"
#include "mruby/variable.h"

static value
_io_test_io_setup(state *mrb, value self)
{
  char rfname[]      = "tmp.mruby-io-test-r.XXXXXXXX";
  char wfname[]      = "tmp.mruby-io-test-w.XXXXXXXX";
  char symlinkname[] = "tmp.mruby-io-test-l.XXXXXXXX";
  char socketname[]  = "tmp.mruby-io-test-s.XXXXXXXX";
  char msg[] = "mruby io test\n";
  mode_t mask;
  int fd0, fd1;
  FILE *fp;

#if !defined(_WIN32) && !defined(_WIN64)
  int fd2, fd3;
  struct sockaddr_un sun0;
#endif

  mask = umask(077);
  fd0 = mkstemp(rfname);
  fd1 = mkstemp(wfname);
  if (fd0 == -1 || fd1 == -1) {
    _raise(mrb, E_RUNTIME_ERROR, "can't create temporary file");
    return _nil_value();
  }
  close(fd0);
  close(fd1);

#if !defined(_WIN32) && !defined(_WIN64)
  fd2 = mkstemp(symlinkname);
  fd3 = mkstemp(socketname);
  if (fd2 == -1 || fd3 == -1) {
    _raise(mrb, E_RUNTIME_ERROR, "can't create temporary file");
    return _nil_value();
  }
#endif
  umask(mask);

  _gv_set(mrb, _intern_cstr(mrb, "$mrbtest_io_rfname"), _str_new_cstr(mrb, rfname));
  _gv_set(mrb, _intern_cstr(mrb, "$mrbtest_io_wfname"), _str_new_cstr(mrb, wfname));
  _gv_set(mrb, _intern_cstr(mrb, "$mrbtest_io_symlinkname"), _str_new_cstr(mrb, symlinkname));
  _gv_set(mrb, _intern_cstr(mrb, "$mrbtest_io_socketname"), _str_new_cstr(mrb, socketname));
  _gv_set(mrb, _intern_cstr(mrb, "$mrbtest_io_msg"), _str_new_cstr(mrb, msg));

  fp = fopen(rfname, "wb");
  if (fp == NULL) {
    _raise(mrb, E_RUNTIME_ERROR, "can't open temporary file");
    return _nil_value();
  }
  fputs(msg, fp);
  fclose(fp);

  fp = fopen(wfname, "wb");
  if (fp == NULL) {
    _raise(mrb, E_RUNTIME_ERROR, "can't open temporary file");
    return _nil_value();
  }
  fclose(fp);

#if !defined(_WIN32) && !defined(_WIN64)
  unlink(symlinkname);
  close(fd2);
  if (symlink(rfname, symlinkname) == -1) {
    _raise(mrb, E_RUNTIME_ERROR, "can't make a symbolic link");
  }

  unlink(socketname);
  close(fd3);
  fd3 = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd3 == -1) {
    _raise(mrb, E_RUNTIME_ERROR, "can't make a socket");
  }
  sun0.sun_family = AF_UNIX;
  snprintf(sun0.sun_path, sizeof(sun0.sun_path), "%s", socketname);
  if (bind(fd3, (struct sockaddr *)&sun0, sizeof(sun0)) == -1) {
    _raisef(mrb, E_RUNTIME_ERROR, "can't bind AF_UNIX socket to %S: %S",
               _str_new_cstr(mrb, sun0.sun_path),
               _fixnum_value(errno));
  }
  close(fd3);
#endif

  return _true_value();
}

static value
_io_test_io_cleanup(state *mrb, value self)
{
  value rfname = _gv_get(mrb, _intern_cstr(mrb, "$mrbtest_io_rfname"));
  value wfname = _gv_get(mrb, _intern_cstr(mrb, "$mrbtest_io_wfname"));
  value symlinkname = _gv_get(mrb, _intern_cstr(mrb, "$mrbtest_io_symlinkname"));
  value socketname = _gv_get(mrb, _intern_cstr(mrb, "$mrbtest_io_socketname"));

  if (_type(rfname) == MRB_TT_STRING) {
    remove(RSTRING_PTR(rfname));
  }
  if (_type(wfname) == MRB_TT_STRING) {
    remove(RSTRING_PTR(wfname));
  }
  if (_type(symlinkname) == MRB_TT_STRING) {
    remove(RSTRING_PTR(symlinkname));
  }
  if (_type(socketname) == MRB_TT_STRING) {
    remove(RSTRING_PTR(socketname));
  }

  _gv_set(mrb, _intern_cstr(mrb, "$mrbtest_io_rfname"), _nil_value());
  _gv_set(mrb, _intern_cstr(mrb, "$mrbtest_io_wfname"), _nil_value());
  _gv_set(mrb, _intern_cstr(mrb, "$mrbtest_io_symlinkname"), _nil_value());
  _gv_set(mrb, _intern_cstr(mrb, "$mrbtest_io_socketname"), _nil_value());
  _gv_set(mrb, _intern_cstr(mrb, "$mrbtest_io_msg"), _nil_value());

  return _nil_value();
}

static value
_io_test_file_setup(state *mrb, value self)
{
  value ary = _io_test_io_setup(mrb, self);
#if !defined(_WIN32) && !defined(_WIN64)
  if (symlink("/usr/bin", "test-bin") == -1) {
    _raise(mrb, E_RUNTIME_ERROR, "can't make a symbolic link");
  }
#endif

  return ary;
}

static value
_io_test_file_cleanup(state *mrb, value self)
{
  _io_test_io_cleanup(mrb, self);
  remove("test-bin");

  return _nil_value();
}

static value
_io_test_mkdtemp(state *mrb, value klass)
{
  value str;
  char *cp;

  _get_args(mrb, "S", &str);
  cp = _str_to_cstr(mrb, str);
  if (mkdtemp(cp) == NULL) {
    _sys_fail(mrb, "mkdtemp");
  }
  return _str_new_cstr(mrb, cp);
}

static value
_io_test_rmdir(state *mrb, value klass)
{
  value str;
  char *cp;

  _get_args(mrb, "S", &str);
  cp = _str_to_cstr(mrb, str);
  if (rmdir(cp) == -1) {
    _sys_fail(mrb, "rmdir");
  }
  return _true_value();
}

value
_io_win_p(state *mrb, value klass)
{
#if defined(_WIN32) || defined(_WIN64)
# if defined(__CYGWIN__) || defined(__CYGWIN32__)
  return _false_value();
# else
  return _true_value();
# endif
#else
  return _false_value();
#endif
}

void
_mruby_io_gem_test(state* mrb)
{
  struct RClass *io_test = _define_module(mrb, "MRubyIOTestUtil");
  _define_class_method(mrb, io_test, "io_test_setup", _io_test_io_setup, MRB_ARGS_NONE());
  _define_class_method(mrb, io_test, "io_test_cleanup", _io_test_io_cleanup, MRB_ARGS_NONE());

  _define_class_method(mrb, io_test, "file_test_setup", _io_test_file_setup, MRB_ARGS_NONE());
  _define_class_method(mrb, io_test, "file_test_cleanup", _io_test_file_cleanup, MRB_ARGS_NONE());

  _define_class_method(mrb, io_test, "mkdtemp", _io_test_mkdtemp, MRB_ARGS_REQ(1));
  _define_class_method(mrb, io_test, "rmdir", _io_test_rmdir, MRB_ARGS_REQ(1));
  _define_class_method(mrb, io_test, "win?", _io_win_p, MRB_ARGS_NONE());
}
