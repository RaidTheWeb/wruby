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

_value
_file_s_umask(_state *mrb, _value klass)
{
#if defined(_WIN32) || defined(_WIN64)
  /* nothing to do on windows */
  return _fixnum_value(0);

#else
  _int mask, omask;
  if (_get_args(mrb, "|i", &mask) == 0) {
    omask = umask(0);
    umask(omask);
  } else {
    omask = umask(mask);
  }
  return _fixnum_value(omask);
#endif
}

static _value
_file_s_unlink(_state *mrb, _value obj)
{
  _value *argv;
  _value pathv;
  _int argc, i;
  char *path;

  _get_args(mrb, "*", &argv, &argc);
  for (i = 0; i < argc; i++) {
    const char *utf8_path;
    pathv = _convert_type(mrb, argv[i], MRB_TT_STRING, "String", "to_str");
    utf8_path = _string_value_cstr(mrb, &pathv);
    path = _locale_from_utf8(utf8_path, -1);
    if (UNLINK(path) < 0) {
      _locale_free(path);
      _sys_fail(mrb, utf8_path);
    }
    _locale_free(path);
  }
  return _fixnum_value(argc);
}

static _value
_file_s_rename(_state *mrb, _value obj)
{
  _value from, to;
  char *src, *dst;

  _get_args(mrb, "SS", &from, &to);
  src = _locale_from_utf8(_string_value_cstr(mrb, &from), -1);
  dst = _locale_from_utf8(_string_value_cstr(mrb, &to), -1);
  if (rename(src, dst) < 0) {
#if defined(_WIN32) || defined(_WIN64)
    if (CHMOD(dst, 0666) == 0 && UNLINK(dst) == 0 && rename(src, dst) == 0) {
      _locale_free(src);
      _locale_free(dst);
      return _fixnum_value(0);
    }
#endif
    _locale_free(src);
    _locale_free(dst);
    _sys_fail(mrb, _str_to_cstr(mrb, _format(mrb, "(%S, %S)", from, to)));
  }
  _locale_free(src);
  _locale_free(dst);
  return _fixnum_value(0);
}

static _value
_file_dirname(_state *mrb, _value klass)
{
#if defined(_WIN32) || defined(_WIN64)
  char dname[_MAX_DIR], vname[_MAX_DRIVE];
  char buffer[_MAX_DRIVE + _MAX_DIR];
  char *path;
  size_t ridx;
  _value s;
  _get_args(mrb, "S", &s);
  path = _locale_from_utf8(_str_to_cstr(mrb, s), -1);
  _splitpath((const char*)path, vname, dname, NULL, NULL);
  snprintf(buffer, _MAX_DRIVE + _MAX_DIR, "%s%s", vname, dname);
  _locale_free(path);
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
  return _str_new_cstr(mrb, buffer);
#else
  char *dname, *path;
  _value s;
  _get_args(mrb, "S", &s);
  path = _locale_from_utf8(_str_to_cstr(mrb, s), -1);

  if ((dname = dirname(path)) == NULL) {
    _locale_free(path);
    _sys_fail(mrb, "dirname");
  }
  _locale_free(path);
  return _str_new_cstr(mrb, dname);
#endif
}

static _value
_file_basename(_state *mrb, _value klass)
{
  // NOTE: Do not use _locale_from_utf8 here
#if defined(_WIN32) || defined(_WIN64)
  char bname[_MAX_DIR];
  char extname[_MAX_EXT];
  char *path;
  size_t ridx;
  char buffer[_MAX_DIR + _MAX_EXT];
  _value s;

  _get_args(mrb, "S", &s);
  path = _str_to_cstr(mrb, s);
  ridx = strlen(path);
  if (ridx > 0) {
    ridx--;
    while (ridx > 0 && (path[ridx] == '/' || path[ridx] == '\\')) {
      path[ridx] = '\0';
      ridx--;
    }
    if (strncmp(path, "/", 2) == 0) {
      return _str_new_cstr(mrb, path);
    }
  }
  _splitpath((const char*)path, NULL, NULL, bname, extname);
  snprintf(buffer, _MAX_DIR + _MAX_EXT, "%s%s", bname, extname);
  return _str_new_cstr(mrb, buffer);
#else
  char *bname, *path;
  _value s;
  _get_args(mrb, "S", &s);
  path = _str_to_cstr(mrb, s);
  if ((bname = basename(path)) == NULL) {
    _sys_fail(mrb, "basename");
  }
  if (strncmp(bname, "//", 3) == 0) bname[1] = '\0';  /* patch for Cygwin */
  return _str_new_cstr(mrb, bname);
#endif
}

static _value
_file_realpath(_state *mrb, _value klass)
{
  _value pathname, dir_string, s, result;
  _int argc;
  char *cpath;

  argc = _get_args(mrb, "S|S", &pathname, &dir_string);
  if (argc == 2) {
    s = _str_dup(mrb, dir_string);
    s = _str_append(mrb, s, _str_new_cstr(mrb, FILE_SEPARATOR));
    s = _str_append(mrb, s, pathname);
    pathname = s;
  }
  cpath = _locale_from_utf8(_str_to_cstr(mrb, pathname), -1);
  result = _str_buf_new(mrb, PATH_MAX);
  if (realpath(cpath, RSTRING_PTR(result)) == NULL) {
    _locale_free(cpath);
    _sys_fail(mrb, cpath);
  }
  _locale_free(cpath);
  _str_resize(mrb, result, strlen(RSTRING_PTR(result)));
  return result;
}

_value
_file__getwd(_state *mrb, _value klass)
{
  _value path;
  char buf[MAXPATHLEN], *utf8;

  if (GETCWD(buf, MAXPATHLEN) == NULL) {
    _sys_fail(mrb, "getcwd(2)");
  }
  utf8 = _utf8_from_locale(buf, -1);
  path = _str_new_cstr(mrb, utf8);
  _utf8_free(utf8);
  return path;
}

static int
_file_is_absolute_path(const char *path)
{
  return (path[0] == '/');
}

static _value
_file__gethome(_state *mrb, _value klass)
{
  _int argc;
  char *home;
  _value path;

#ifndef _WIN32
  _value username;

  argc = _get_args(mrb, "|S", &username);
  if (argc == 0) {
    home = getenv("HOME");
    if (home == NULL) {
      return _nil_value();
    }
    if (!_file_is_absolute_path(home)) {
      _raise(mrb, E_ARGUMENT_ERROR, "non-absolute home");
    }
  } else {
    const char *cuser = _str_to_cstr(mrb, username);
    struct passwd *pwd = getpwnam(cuser);
    if (pwd == NULL) {
      return _nil_value();
    }
    home = pwd->pw_dir;
    if (!_file_is_absolute_path(home)) {
      _raisef(mrb, E_ARGUMENT_ERROR, "non-absolute home of ~%S", username);
    }
  }
  home = _locale_from_utf8(home, -1);
  path = _str_new_cstr(mrb, home);
  _locale_free(home);
  return path;
#else
  argc = _get_argc(mrb);
  if (argc == 0) {
    home = getenv("USERPROFILE");
    if (home == NULL) {
      return _nil_value();
    }
    if (!_file_is_absolute_path(home)) {
      _raise(mrb, E_ARGUMENT_ERROR, "non-absolute home");
    }
  } else {
    return _nil_value();
  }
  home = _locale_from_utf8(home, -1);
  path = _str_new_cstr(mrb, home);
  _locale_free(home);
  return path;
#endif
}

static _value
_file_mtime(_state *mrb, _value self)
{
  _value obj;
  struct stat st;
  int fd;

  obj = _obj_value(_class_get(mrb, "Time"));
  fd = (int)_fixnum(_io_fileno(mrb, self));
  if (fstat(fd, &st) == -1)
    return _false_value();
  return _funcall(mrb, obj, "at", 1, _fixnum_value(st.st_mtime));
}

_value
_file_flock(_state *mrb, _value self)
{
#if defined(sun)
  _raise(mrb, E_NOTIMP_ERROR, "flock is not supported on Illumos/Solaris/Windows");
#else
  _int operation;
  int fd;

  _get_args(mrb, "i", &operation);
  fd = (int)_fixnum(_io_fileno(mrb, self));

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
          return _false_value();
        }
        /* FALLTHRU - should not happen */
      default:
        _sys_fail(mrb, "flock failed");
        break;
    }
  }
#endif
  return _fixnum_value(0);
}

static _value
_file_s_symlink(_state *mrb, _value klass)
{
#if defined(_WIN32) || defined(_WIN64)
  _raise(mrb, E_NOTIMP_ERROR, "symlink is not supported on this platform");
#else
  _value from, to;
  const char *src, *dst;
  int ai = _gc_arena_save(mrb);

  _get_args(mrb, "SS", &from, &to);
  src = _locale_from_utf8(_str_to_cstr(mrb, from), -1);
  dst = _locale_from_utf8(_str_to_cstr(mrb, to), -1);

  if (symlink(src, dst) == -1) {
    _locale_free(src);
    _locale_free(dst);
    _sys_fail(mrb, _str_to_cstr(mrb, _format(mrb, "(%S, %S)", from, to)));
  }
  _locale_free(src);
  _locale_free(dst);
  _gc_arena_restore(mrb, ai);
#endif
  return _fixnum_value(0);
}

static _value
_file_s_chmod(_state *mrb, _value klass) {
  _int mode;
  _int argc, i;
  _value *filenames;
  int ai = _gc_arena_save(mrb);

  _get_args(mrb, "i*", &mode, &filenames, &argc);
  for (i = 0; i < argc; i++) {
    const char *utf8_path = _str_to_cstr(mrb, filenames[i]);
    char *path = _locale_from_utf8(utf8_path, -1);
    if (CHMOD(path, mode) == -1) {
      _locale_free(path);
      _sys_fail(mrb, utf8_path);
    }
    _locale_free(path);
  }

  _gc_arena_restore(mrb, ai);
  return _fixnum_value(argc);
}

static _value
_file_s_readlink(_state *mrb, _value klass) {
#if defined(_WIN32) || defined(_WIN64)
  _raise(mrb, E_NOTIMP_ERROR, "readlink is not supported on this platform");
  return _nil_value(); // unreachable
#else
  char *path, *buf, *tmp;
  size_t bufsize = 100;
  ssize_t rc;
  _value ret;
  int ai = _gc_arena_save(mrb);

  _get_args(mrb, "z", &path);
  tmp = _locale_from_utf8(path, -1);

  buf = (char *)_malloc(mrb, bufsize);
  while ((rc = readlink(tmp, buf, bufsize)) == (ssize_t)bufsize && rc != -1) {
    bufsize *= 2;
    buf = (char *)_realloc(mrb, buf, bufsize);
  }
  _locale_free(tmp);
  if (rc == -1) {
    _free(mrb, buf);
    _sys_fail(mrb, path);
  }
  tmp = _utf8_from_locale(buf, -1);
  ret = _str_new(mrb, tmp, rc);
  _locale_free(tmp);
  _free(mrb, buf);

  _gc_arena_restore(mrb, ai);
  return ret;
#endif
}

void
_init_file(_state *mrb)
{
  struct RClass *io, *file, *cnst;

  io   = _class_get(mrb, "IO");
  file = _define_class(mrb, "File", io);
  MRB_SET_INSTANCE_TT(file, MRB_TT_DATA);
  _define_class_method(mrb, file, "umask",  _file_s_umask, MRB_ARGS_REQ(1));
  _define_class_method(mrb, file, "delete", _file_s_unlink, MRB_ARGS_ANY());
  _define_class_method(mrb, file, "unlink", _file_s_unlink, MRB_ARGS_ANY());
  _define_class_method(mrb, file, "rename", _file_s_rename, MRB_ARGS_REQ(2));
  _define_class_method(mrb, file, "symlink", _file_s_symlink, MRB_ARGS_REQ(2));
  _define_class_method(mrb, file, "chmod", _file_s_chmod, MRB_ARGS_REQ(1) | MRB_ARGS_REST());
  _define_class_method(mrb, file, "readlink", _file_s_readlink, MRB_ARGS_REQ(1));

  _define_class_method(mrb, file, "dirname",   _file_dirname,    MRB_ARGS_REQ(1));
  _define_class_method(mrb, file, "basename",  _file_basename,   MRB_ARGS_REQ(1));
  _define_class_method(mrb, file, "realpath",  _file_realpath,   MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  _define_class_method(mrb, file, "_getwd",    _file__getwd,     MRB_ARGS_NONE());
  _define_class_method(mrb, file, "_gethome",  _file__gethome,   MRB_ARGS_OPT(1));

  _define_method(mrb, file, "flock", _file_flock, MRB_ARGS_REQ(1));
  _define_method(mrb, file, "mtime", _file_mtime, MRB_ARGS_NONE());

  cnst = _define_module_under(mrb, file, "Constants");
  _define_const(mrb, cnst, "LOCK_SH", _fixnum_value(LOCK_SH));
  _define_const(mrb, cnst, "LOCK_EX", _fixnum_value(LOCK_EX));
  _define_const(mrb, cnst, "LOCK_UN", _fixnum_value(LOCK_UN));
  _define_const(mrb, cnst, "LOCK_NB", _fixnum_value(LOCK_NB));
  _define_const(mrb, cnst, "SEPARATOR", _str_new_cstr(mrb, FILE_SEPARATOR));
  _define_const(mrb, cnst, "PATH_SEPARATOR", _str_new_cstr(mrb, PATH_SEPARATOR));
#if defined(_WIN32) || defined(_WIN64)
  _define_const(mrb, cnst, "ALT_SEPARATOR", _str_new_cstr(mrb, FILE_ALT_SEPARATOR));
#else
  _define_const(mrb, cnst, "ALT_SEPARATOR", _nil_value());
#endif
  _define_const(mrb, cnst, "NULL", _str_new_cstr(mrb, NULL_FILE));

}
