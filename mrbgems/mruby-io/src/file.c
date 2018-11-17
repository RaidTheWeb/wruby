/*
** file.c - File class
*/

#include "mruby.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/string.h"
#include "mruby/ext/io.h"

#if MRUBY_RELEASE_NO < 10000
#include "error.h"
#else
#include "mruby/error.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <limits.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
  #include <io.h>
  #define NULL_FILE "NUL"
  #define UNLINK _unlink
  #define GETCWD _getcwd
  #define CHMOD(a, b) 0
  #define MAXPATHLEN 1024
 #if !defined(PATH_MAX)
  #define PATH_MAX _MAX_PATH
 #endif
  #define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
  #include <direct.h>
#else
  #define NULL_FILE "/dev/null"
  #include <unistd.h>
  #define UNLINK unlink
  #define GETCWD getcwd
  #define CHMOD(a, b) chmod(a,b)
  #include <sys/file.h>
  #include <libgen.h>
  #include <sys/param.h>
  #include <pwd.h>
#endif

#define FILE_SEPARATOR "/"

#if defined(_WIN32) || defined(_WIN64)
  #define PATH_SEPARATOR ";"
  #define FILE_ALT_SEPARATOR "\\"
#else
  #define PATH_SEPARATOR ":"
#endif

#ifndef LOCK_SH
#define LOCK_SH 1
#endif
#ifndef LOCK_EX
#define LOCK_EX 2
#endif
#ifndef LOCK_NB
#define LOCK_NB 4
#endif
#ifndef LOCK_UN
#define LOCK_UN 8
#endif

#define STAT(p, s)        stat(p, s)

#ifdef _WIN32
static int
flock(int fd, int operation) {
  OVERLAPPED ov;
  HANDLE h = (HANDLE)_get_osfhandle(fd);
  DWORD flags;
  flags = ((operation & LOCK_NB) ? LOCKFILE_FAIL_IMMEDIATELY : 0)
          | ((operation & LOCK_SH) ? LOCKFILE_EXCLUSIVE_LOCK : 0);
  memset(&ov, 0, sizeof(ov));
  return LockFileEx(h, flags, 0, 0xffffffff, 0xffffffff, &ov) ? 0 : -1;
}
#endif

value
file_s_umask(state *mrb, value klass)
{
#if defined(_WIN32) || defined(_WIN64)
  /* nothing to do on windows */
  return fixnum_value(0);

#else
  int mask, omask;
  if (get_args(mrb, "|i", &mask) == 0) {
    omask = umask(0);
    umask(omask);
  } else {
    omask = umask(mask);
  }
  return fixnum_value(omask);
#endif
}

static value
file_s_unlink(state *mrb, value obj)
{
  value *argv;
  value pathv;
  int argc, i;
  char *path;

  get_args(mrb, "*", &argv, &argc);
  for (i = 0; i < argc; i++) {
    const char *utf8_path;
    pathv = convert_type(mrb, argv[i], TT_STRING, "String", "to_str");
    utf8_path = string_value_cstr(mrb, &pathv);
    path = locale_from_utf8(utf8_path, -1);
    if (UNLINK(path) < 0) {
      locale_free(path);
      sys_fail(mrb, utf8_path);
    }
    locale_free(path);
  }
  return fixnum_value(argc);
}

static value
file_s_rename(state *mrb, value obj)
{
  value from, to;
  char *src, *dst;

  get_args(mrb, "SS", &from, &to);
  src = locale_from_utf8(string_value_cstr(mrb, &from), -1);
  dst = locale_from_utf8(string_value_cstr(mrb, &to), -1);
  if (rename(src, dst) < 0) {
#if defined(_WIN32) || defined(_WIN64)
    if (CHMOD(dst, 0666) == 0 && UNLINK(dst) == 0 && rename(src, dst) == 0) {
      locale_free(src);
      locale_free(dst);
      return fixnum_value(0);
    }
#endif
    locale_free(src);
    locale_free(dst);
    sys_fail(mrb, str_to_cstr(mrb, format(mrb, "(%S, %S)", from, to)));
  }
  locale_free(src);
  locale_free(dst);
  return fixnum_value(0);
}

static value
file_dirname(state *mrb, value klass)
{
#if defined(_WIN32) || defined(_WIN64)
  char dname[_MAX_DIR], vname[_MAX_DRIVE];
  char buffer[_MAX_DRIVE + _MAX_DIR];
  char *path;
  size_t ridx;
  value s;
  get_args(mrb, "S", &s);
  path = locale_from_utf8(str_to_cstr(mrb, s), -1);
  _splitpath((const char*)path, vname, dname, NULL, NULL);
  snprintf(buffer, _MAX_DRIVE + _MAX_DIR, "%s%s", vname, dname);
  locale_free(path);
  ridx = strlen(buffer);
  if (ridx == 0) {
    strncpy(buffer, ".", 2);  /* null terminated */
  } else if (ridx > 1) {
    ridx--;
    while (ridx > 0 && (buffer[ridx] == '/' || buffer[ridx] == '\\')) {
      buffer[ridx] = '\0';  /* remove last char */
      ridx--;
    }
  }
  return str_new_cstr(mrb, buffer);
#else
  char *dname, *path;
  value s;
  get_args(mrb, "S", &s);
  path = locale_from_utf8(str_to_cstr(mrb, s), -1);

  if ((dname = dirname(path)) == NULL) {
    locale_free(path);
    sys_fail(mrb, "dirname");
  }
  locale_free(path);
  return str_new_cstr(mrb, dname);
#endif
}

static value
file_basename(state *mrb, value klass)
{
  // NOTE: Do not use locale_from_utf8 here
#if defined(_WIN32) || defined(_WIN64)
  char bname[_MAX_DIR];
  char extname[_MAX_EXT];
  char *path;
  size_t ridx;
  char buffer[_MAX_DIR + _MAX_EXT];
  value s;

  get_args(mrb, "S", &s);
  path = str_to_cstr(mrb, s);
  ridx = strlen(path);
  if (ridx > 0) {
    ridx--;
    while (ridx > 0 && (path[ridx] == '/' || path[ridx] == '\\')) {
      path[ridx] = '\0';
      ridx--;
    }
    if (strncmp(path, "/", 2) == 0) {
      return str_new_cstr(mrb, path);
    }
  }
  _splitpath((const char*)path, NULL, NULL, bname, extname);
  snprintf(buffer, _MAX_DIR + _MAX_EXT, "%s%s", bname, extname);
  return str_new_cstr(mrb, buffer);
#else
  char *bname, *path;
  value s;
  get_args(mrb, "S", &s);
  path = str_to_cstr(mrb, s);
  if ((bname = basename(path)) == NULL) {
    sys_fail(mrb, "basename");
  }
  if (strncmp(bname, "//", 3) == 0) bname[1] = '\0';  /* patch for Cygwin */
  return str_new_cstr(mrb, bname);
#endif
}

static value
file_realpath(state *mrb, value klass)
{
  value pathname, dir_string, s, result;
  int argc;
  char *cpath;

  argc = get_args(mrb, "S|S", &pathname, &dir_string);
  if (argc == 2) {
    s = str_dup(mrb, dir_string);
    s = str_append(mrb, s, str_new_cstr(mrb, FILE_SEPARATOR));
    s = str_append(mrb, s, pathname);
    pathname = s;
  }
  cpath = locale_from_utf8(str_to_cstr(mrb, pathname), -1);
  result = str_buf_new(mrb, PATH_MAX);
  if (realpath(cpath, RSTRING_PTR(result)) == NULL) {
    locale_free(cpath);
    sys_fail(mrb, cpath);
  }
  locale_free(cpath);
  str_resize(mrb, result, strlen(RSTRING_PTR(result)));
  return result;
}

value
file__getwd(state *mrb, value klass)
{
  value path;
  char buf[MAXPATHLEN], *utf8;

  if (GETCWD(buf, MAXPATHLEN) == NULL) {
    sys_fail(mrb, "getcwd(2)");
  }
  utf8 = utf8_from_locale(buf, -1);
  path = str_new_cstr(mrb, utf8);
  utf8_free(utf8);
  return path;
}

static int
file_is_absolute_path(const char *path)
{
  return (path[0] == '/');
}

static value
file__gethome(state *mrb, value klass)
{
  int argc;
  char *home;
  value path;

#ifndef _WIN32
  value username;

  argc = get_args(mrb, "|S", &username);
  if (argc == 0) {
    home = getenv("HOME");
    if (home == NULL) {
      return nil_value();
    }
    if (!file_is_absolute_path(home)) {
      raise(mrb, E_ARGUMENT_ERROR, "non-absolute home");
    }
  } else {
    const char *cuser = str_to_cstr(mrb, username);
    struct passwd *pwd = getpwnam(cuser);
    if (pwd == NULL) {
      return nil_value();
    }
    home = pwd->pw_dir;
    if (!file_is_absolute_path(home)) {
      raisef(mrb, E_ARGUMENT_ERROR, "non-absolute home of ~%S", username);
    }
  }
  home = locale_from_utf8(home, -1);
  path = str_new_cstr(mrb, home);
  locale_free(home);
  return path;
#else
  argc = get_argc(mrb);
  if (argc == 0) {
    home = getenv("USERPROFILE");
    if (home == NULL) {
      return nil_value();
    }
    if (!file_is_absolute_path(home)) {
      raise(mrb, E_ARGUMENT_ERROR, "non-absolute home");
    }
  } else {
    return nil_value();
  }
  home = locale_from_utf8(home, -1);
  path = str_new_cstr(mrb, home);
  locale_free(home);
  return path;
#endif
}

static value
file_mtime(state *mrb, value self)
{
  value obj;
  struct stat st;
  int fd;

  obj = obj_value(class_get(mrb, "Time"));
  fd = (int)fixnum(io_fileno(mrb, self));
  if (fstat(fd, &st) == -1)
    return false_value();
  return funcall(mrb, obj, "at", 1, fixnum_value(st.st_mtime));
}

value
file_flock(state *mrb, value self)
{
#if defined(sun)
  raise(mrb, E_NOTIMP_ERROR, "flock is not supported on Illumos/Solaris/Windows");
#else
  int operation;
  int fd;

  get_args(mrb, "i", &operation);
  fd = (int)fixnum(io_fileno(mrb, self));

  while (flock(fd, (int)operation) == -1) {
    switch (errno) {
      case EINTR:
        /* retry */
        break;
      case EAGAIN:      /* NetBSD */
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
      case EWOULDBLOCK: /* FreeBSD OpenBSD Linux */
#endif
        if (operation & LOCK_NB) {
          return false_value();
        }
        /* FALLTHRU - should not happen */
      default:
        sys_fail(mrb, "flock failed");
        break;
    }
  }
#endif
  return fixnum_value(0);
}

static value
file_s_symlink(state *mrb, value klass)
{
#if defined(_WIN32) || defined(_WIN64)
  raise(mrb, E_NOTIMP_ERROR, "symlink is not supported on this platform");
#else
  value from, to;
  const char *src, *dst;
  int ai = gc_arena_save(mrb);

  get_args(mrb, "SS", &from, &to);
  src = locale_from_utf8(str_to_cstr(mrb, from), -1);
  dst = locale_from_utf8(str_to_cstr(mrb, to), -1);

  if (symlink(src, dst) == -1) {
    locale_free(src);
    locale_free(dst);
    sys_fail(mrb, str_to_cstr(mrb, format(mrb, "(%S, %S)", from, to)));
  }
  locale_free(src);
  locale_free(dst);
  gc_arena_restore(mrb, ai);
#endif
  return fixnum_value(0);
}

static value
file_s_chmod(state *mrb, value klass) {
  int mode;
  int argc, i;
  value *filenames;
  int ai = gc_arena_save(mrb);

  get_args(mrb, "i*", &mode, &filenames, &argc);
  for (i = 0; i < argc; i++) {
    const char *utf8_path = str_to_cstr(mrb, filenames[i]);
    char *path = locale_from_utf8(utf8_path, -1);
    if (CHMOD(path, mode) == -1) {
      locale_free(path);
      sys_fail(mrb, utf8_path);
    }
    locale_free(path);
  }

  gc_arena_restore(mrb, ai);
  return fixnum_value(argc);
}

static value
file_s_readlink(state *mrb, value klass) {
#if defined(_WIN32) || defined(_WIN64)
  raise(mrb, E_NOTIMP_ERROR, "readlink is not supported on this platform");
  return nil_value(); // unreachable
#else
  char *path, *buf, *tmp;
  size_t bufsize = 100;
  ssize_t rc;
  value ret;
  int ai = gc_arena_save(mrb);

  get_args(mrb, "z", &path);
  tmp = locale_from_utf8(path, -1);

  buf = (char *)malloc(mrb, bufsize);
  while ((rc = readlink(tmp, buf, bufsize)) == (ssize_t)bufsize && rc != -1) {
    bufsize *= 2;
    buf = (char *)realloc(mrb, buf, bufsize);
  }
  locale_free(tmp);
  if (rc == -1) {
    free(mrb, buf);
    sys_fail(mrb, path);
  }
  tmp = utf8_from_locale(buf, -1);
  ret = str_new(mrb, tmp, rc);
  locale_free(tmp);
  free(mrb, buf);

  gc_arena_restore(mrb, ai);
  return ret;
#endif
}

void
init_file(state *mrb)
{
  struct RClass *io, *file, *cnst;

  io   = class_get(mrb, "IO");
  file = define_class(mrb, "File", io);
  SET_INSTANCE_TT(file, TT_DATA);
  define_class_method(mrb, file, "umask",  file_s_umask, ARGS_REQ(1));
  define_class_method(mrb, file, "delete", file_s_unlink, ARGS_ANY());
  define_class_method(mrb, file, "unlink", file_s_unlink, ARGS_ANY());
  define_class_method(mrb, file, "rename", file_s_rename, ARGS_REQ(2));
  define_class_method(mrb, file, "symlink", file_s_symlink, ARGS_REQ(2));
  define_class_method(mrb, file, "chmod", file_s_chmod, ARGS_REQ(1) | ARGS_REST());
  define_class_method(mrb, file, "readlink", file_s_readlink, ARGS_REQ(1));

  define_class_method(mrb, file, "dirname",   file_dirname,    ARGS_REQ(1));
  define_class_method(mrb, file, "basename",  file_basename,   ARGS_REQ(1));
  define_class_method(mrb, file, "realpath",  file_realpath,   ARGS_REQ(1)|ARGS_OPT(1));
  define_class_method(mrb, file, "_getwd",    file__getwd,     ARGS_NONE());
  define_class_method(mrb, file, "_gethome",  file__gethome,   ARGS_OPT(1));

  define_method(mrb, file, "flock", file_flock, ARGS_REQ(1));
  define_method(mrb, file, "mtime", file_mtime, ARGS_NONE());

  cnst = define_module_under(mrb, file, "Constants");
  define_const(mrb, cnst, "LOCK_SH", fixnum_value(LOCK_SH));
  define_const(mrb, cnst, "LOCK_EX", fixnum_value(LOCK_EX));
  define_const(mrb, cnst, "LOCK_UN", fixnum_value(LOCK_UN));
  define_const(mrb, cnst, "LOCK_NB", fixnum_value(LOCK_NB));
  define_const(mrb, cnst, "SEPARATOR", str_new_cstr(mrb, FILE_SEPARATOR));
  define_const(mrb, cnst, "PATH_SEPARATOR", str_new_cstr(mrb, PATH_SEPARATOR));
#if defined(_WIN32) || defined(_WIN64)
  define_const(mrb, cnst, "ALT_SEPARATOR", str_new_cstr(mrb, FILE_ALT_SEPARATOR));
#else
  define_const(mrb, cnst, "ALT_SEPARATOR", nil_value());
#endif
  define_const(mrb, cnst, "NULL", str_new_cstr(mrb, NULL_FILE));

}
