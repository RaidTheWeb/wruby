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

static $value
$io_test_io_setup($state *mrb, $value self)
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
    $raise(mrb, E_RUNTIME_ERROR, "can't create temporary file");
    return $nil_value();
  }
  close(fd0);
  close(fd1);

#if !defined(_WIN32) && !defined(_WIN64)
  fd2 = mkstemp(symlinkname);
  fd3 = mkstemp(socketname);
  if (fd2 == -1 || fd3 == -1) {
    $raise(mrb, E_RUNTIME_ERROR, "can't create temporary file");
    return $nil_value();
  }
#endif
  umask(mask);

  $gv_set(mrb, $intern_cstr(mrb, "$mrbtest_io_rfname"), $str_new_cstr(mrb, rfname));
  $gv_set(mrb, $intern_cstr(mrb, "$mrbtest_io_wfname"), $str_new_cstr(mrb, wfname));
  $gv_set(mrb, $intern_cstr(mrb, "$mrbtest_io_symlinkname"), $str_new_cstr(mrb, symlinkname));
  $gv_set(mrb, $intern_cstr(mrb, "$mrbtest_io_socketname"), $str_new_cstr(mrb, socketname));
  $gv_set(mrb, $intern_cstr(mrb, "$mrbtest_io_msg"), $str_new_cstr(mrb, msg));

  fp = fopen(rfname, "wb");
  if (fp == NULL) {
    $raise(mrb, E_RUNTIME_ERROR, "can't open temporary file");
    return $nil_value();
  }
  fputs(msg, fp);
  fclose(fp);

  fp = fopen(wfname, "wb");
  if (fp == NULL) {
    $raise(mrb, E_RUNTIME_ERROR, "can't open temporary file");
    return $nil_value();
  }
  fclose(fp);

#if !defined(_WIN32) && !defined(_WIN64)
  unlink(symlinkname);
  close(fd2);
  if (symlink(rfname, symlinkname) == -1) {
    $raise(mrb, E_RUNTIME_ERROR, "can't make a symbolic link");
  }

  unlink(socketname);
  close(fd3);
  fd3 = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd3 == -1) {
    $raise(mrb, E_RUNTIME_ERROR, "can't make a socket");
  }
  sun0.sun_family = AF_UNIX;
  snprintf(sun0.sun_path, sizeof(sun0.sun_path), "%s", socketname);
  if (bind(fd3, (struct sockaddr *)&sun0, sizeof(sun0)) == -1) {
    $raisef(mrb, E_RUNTIME_ERROR, "can't bind AF_UNIX socket to %S: %S",
               $str_new_cstr(mrb, sun0.sun_path),
               $fixnum_value(errno));
  }
  close(fd3);
#endif

  return $true_value();
}

static $value
$io_test_io_cleanup($state *mrb, $value self)
{
  $value rfname = $gv_get(mrb, $intern_cstr(mrb, "$mrbtest_io_rfname"));
  $value wfname = $gv_get(mrb, $intern_cstr(mrb, "$mrbtest_io_wfname"));
  $value symlinkname = $gv_get(mrb, $intern_cstr(mrb, "$mrbtest_io_symlinkname"));
  $value socketname = $gv_get(mrb, $intern_cstr(mrb, "$mrbtest_io_socketname"));

  if ($type(rfname) == $TT_STRING) {
    remove(RSTRING_PTR(rfname));
  }
  if ($type(wfname) == $TT_STRING) {
    remove(RSTRING_PTR(wfname));
  }
  if ($type(symlinkname) == $TT_STRING) {
    remove(RSTRING_PTR(symlinkname));
  }
  if ($type(socketname) == $TT_STRING) {
    remove(RSTRING_PTR(socketname));
  }

  $gv_set(mrb, $intern_cstr(mrb, "$mrbtest_io_rfname"), $nil_value());
  $gv_set(mrb, $intern_cstr(mrb, "$mrbtest_io_wfname"), $nil_value());
  $gv_set(mrb, $intern_cstr(mrb, "$mrbtest_io_symlinkname"), $nil_value());
  $gv_set(mrb, $intern_cstr(mrb, "$mrbtest_io_socketname"), $nil_value());
  $gv_set(mrb, $intern_cstr(mrb, "$mrbtest_io_msg"), $nil_value());

  return $nil_value();
}

static $value
$io_test_file_setup($state *mrb, $value self)
{
  $value ary = $io_test_io_setup(mrb, self);
#if !defined(_WIN32) && !defined(_WIN64)
  if (symlink("/usr/bin", "test-bin") == -1) {
    $raise(mrb, E_RUNTIME_ERROR, "can't make a symbolic link");
  }
#endif

  return ary;
}

static $value
$io_test_file_cleanup($state *mrb, $value self)
{
  $io_test_io_cleanup(mrb, self);
  remove("test-bin");

  return $nil_value();
}

static $value
$io_test_mkdtemp($state *mrb, $value klass)
{
  $value str;
  char *cp;

  $get_args(mrb, "S", &str);
  cp = $str_to_cstr(mrb, str);
  if (mkdtemp(cp) == NULL) {
    $sys_fail(mrb, "mkdtemp");
  }
  return $str_new_cstr(mrb, cp);
}

static $value
$io_test_rmdir($state *mrb, $value klass)
{
  $value str;
  char *cp;

  $get_args(mrb, "S", &str);
  cp = $str_to_cstr(mrb, str);
  if (rmdir(cp) == -1) {
    $sys_fail(mrb, "rmdir");
  }
  return $true_value();
}

$value
$io_win_p($state *mrb, $value klass)
{
#if defined(_WIN32) || defined(_WIN64)
# if defined(__CYGWIN__) || defined(__CYGWIN32__)
  return $false_value();
# else
  return $true_value();
# endif
#else
  return $false_value();
#endif
}

void
$mruby_io_gem_test($state* mrb)
{
  struct RClass *io_test = $define_module(mrb, "MRubyIOTestUtil");
  $define_class_method(mrb, io_test, "io_test_setup", $io_test_io_setup, $ARGS_NONE());
  $define_class_method(mrb, io_test, "io_test_cleanup", $io_test_io_cleanup, $ARGS_NONE());

  $define_class_method(mrb, io_test, "file_test_setup", $io_test_file_setup, $ARGS_NONE());
  $define_class_method(mrb, io_test, "file_test_cleanup", $io_test_file_cleanup, $ARGS_NONE());

  $define_class_method(mrb, io_test, "mkdtemp", $io_test_mkdtemp, $ARGS_REQ(1));
  $define_class_method(mrb, io_test, "rmdir", $io_test_rmdir, $ARGS_REQ(1));
  $define_class_method(mrb, io_test, "win?", $io_win_p, $ARGS_NONE());
}
