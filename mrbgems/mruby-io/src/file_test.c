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

#if defined(_WIN32) || defined(_WIN64)
  #define LSTAT stat
  #include <winsock.h>
#else
  #define LSTAT lstat
  #include <sys/file.h>
  #include <sys/param.h>
  #include <sys/wait.h>
  #include <libgen.h>
  #include <pwd.h>
  #include <unistd.h>
#endif

#include <fcntl.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct _data_type _io_type;

static int
_stat0(_state *mrb, _value obj, struct stat *st, int do_lstat)
{
  _value tmp;
  _value io_klass, str_klass;

  io_klass  = _obj_value(_class_get(mrb, "IO"));
  str_klass = _obj_value(_class_get(mrb, "String"));

  tmp = _funcall(mrb, obj, "is_a?", 1, io_klass);
  if (_test(tmp)) {
    struct _io *fptr;
    fptr = (struct _io *)_get_datatype(mrb, obj, &_io_type);

    if (fptr && fptr->fd >= 0) {
      return fstat(fptr->fd, st);
    }

    _raise(mrb, E_IO_ERROR, "closed stream");
    return -1;
  }

  tmp = _funcall(mrb, obj, "is_a?", 1, str_klass);
  if (_test(tmp)) {
    char *path = _locale_from_utf8(_str_to_cstr(mrb, obj), -1);
    int ret;
    if (do_lstat) {
      ret = LSTAT(path, st);
    } else {
      ret = stat(path, st);
    }
    _locale_free(path);
    return ret;
  }

  return -1;
}

static int
_stat(_state *mrb, _value obj, struct stat *st)
{
  return _stat0(mrb, obj, st, 0);
}

static int
_lstat(_state *mrb, _value obj, struct stat *st)
{
  return _stat0(mrb, obj, st, 1);
}

/*
 * Document-method: directory?
 *
 * call-seq:
 *   File.directory?(file_name)   ->  true or false
 *
 * Returns <code>true</code> if the named file is a directory,
 * or a symlink that points at a directory, and <code>false</code>
 * otherwise.
 *
 *    File.directory?(".")
 */

_value
_filetest_s_directory_p(_state *mrb, _value klass)
{
#ifndef S_ISDIR
#   define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

  struct stat st;
  _value obj;

  _get_args(mrb, "o", &obj);

  if (_stat(mrb, obj, &st) < 0)
    return _false_value();
  if (S_ISDIR(st.st_mode))
    return _true_value();

  return _false_value();
}

/*
 * call-seq:
 *   File.pipe?(file_name)   ->  true or false
 *
 * Returns <code>true</code> if the named file is a pipe.
 */

_value
_filetest_s_pipe_p(_state *mrb, _value klass)
{
#if defined(_WIN32) || defined(_WIN64)
  _raise(mrb, E_NOTIMP_ERROR, "pipe is not supported on this platform");
#else
#ifdef S_IFIFO
#  ifndef S_ISFIFO
#    define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#  endif

  struct stat st;
  _value obj;

  _get_args(mrb, "o", &obj);

  if (_stat(mrb, obj, &st) < 0)
    return _false_value();
  if (S_ISFIFO(st.st_mode))
    return _true_value();

#endif
  return _false_value();
#endif
}

/*
 * call-seq:
 *   File.symlink?(file_name)   ->  true or false
 *
 * Returns <code>true</code> if the named file is a symbolic link.
 */

_value
_filetest_s_symlink_p(_state *mrb, _value klass)
{
#if defined(_WIN32) || defined(_WIN64)
  _raise(mrb, E_NOTIMP_ERROR, "symlink is not supported on this platform");
#else
#ifndef S_ISLNK
#  ifdef _S_ISLNK
#    define S_ISLNK(m) _S_ISLNK(m)
#  else
#    ifdef _S_IFLNK
#      define S_ISLNK(m) (((m) & S_IFMT) == _S_IFLNK)
#    else
#      ifdef S_IFLNK
#        define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#      endif
#    endif
#  endif
#endif

#ifdef S_ISLNK
  struct stat st;
  _value obj;

  _get_args(mrb, "o", &obj);

  if (_lstat(mrb, obj, &st) == -1)
    return _false_value();
  if (S_ISLNK(st.st_mode))
    return _true_value();
#endif

  return _false_value();
#endif
}

/*
 * call-seq:
 *   File.socket?(file_name)   ->  true or false
 *
 * Returns <code>true</code> if the named file is a socket.
 */

_value
_filetest_s_socket_p(_state *mrb, _value klass)
{
#if defined(_WIN32) || defined(_WIN64)
  _raise(mrb, E_NOTIMP_ERROR, "socket is not supported on this platform");
#else
#ifndef S_ISSOCK
#  ifdef _S_ISSOCK
#    define S_ISSOCK(m) _S_ISSOCK(m)
#  else
#    ifdef _S_IFSOCK
#      define S_ISSOCK(m) (((m) & S_IFMT) == _S_IFSOCK)
#    else
#      ifdef S_IFSOCK
#        define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#      endif
#    endif
#  endif
#endif

#ifdef S_ISSOCK
  struct stat st;
  _value obj;

  _get_args(mrb, "o", &obj);

  if (_stat(mrb, obj, &st) < 0)
    return _false_value();
  if (S_ISSOCK(st.st_mode))
    return _true_value();
#endif

  return _false_value();
#endif
}

/*
 * call-seq:
 *    File.exist?(file_name)    ->  true or false
 *    File.exists?(file_name)   ->  true or false
 *
 * Return <code>true</code> if the named file exists.
 */

_value
_filetest_s_exist_p(_state *mrb, _value klass)
{
  struct stat st;
  _value obj;

  _get_args(mrb, "o", &obj);
  if (_stat(mrb, obj, &st) < 0)
    return _false_value();

  return _true_value();
}

/*
 * call-seq:
 *    File.file?(file_name)   -> true or false
 *
 * Returns <code>true</code> if the named file exists and is a
 * regular file.
 */

_value
_filetest_s_file_p(_state *mrb, _value klass)
{
#ifndef S_ISREG
#   define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

  struct stat st;
  _value obj;

  _get_args(mrb, "o", &obj);

  if (_stat(mrb, obj, &st) < 0)
    return _false_value();
  if (S_ISREG(st.st_mode))
    return _true_value();

  return _false_value();
}

/*
 * call-seq:
 *    File.zero?(file_name)   -> true or false
 *
 * Returns <code>true</code> if the named file exists and has
 * a zero size.
 */

_value
_filetest_s_zero_p(_state *mrb, _value klass)
{
  struct stat st;
  _value obj;

  _get_args(mrb, "o", &obj);

  if (_stat(mrb, obj, &st) < 0)
    return _false_value();
  if (st.st_size == 0)
    return _true_value();

  return _false_value();
}

/*
 * call-seq:
 *    File.size(file_name)   -> integer
 *
 * Returns the size of <code>file_name</code>.
 *
 * _file_name_ can be an IO object.
 */

_value
_filetest_s_size(_state *mrb, _value klass)
{
  struct stat st;
  _value obj;

  _get_args(mrb, "o", &obj);

  if (_stat(mrb, obj, &st) < 0)
    _sys_fail(mrb, "_stat");

  return _fixnum_value(st.st_size);
}

/*
 * call-seq:
 *    File.size?(file_name)   -> Integer or nil
 *
 * Returns +nil+ if +file_name+ doesn't exist or has zero size, the size of the
 * file otherwise.
 */

_value
_filetest_s_size_p(_state *mrb, _value klass)
{
  struct stat st;
  _value obj;

  _get_args(mrb, "o", &obj);

  if (_stat(mrb, obj, &st) < 0)
    return _nil_value();
  if (st.st_size == 0)
    return _nil_value();

  return _fixnum_value(st.st_size);
}

void
_init_file_test(_state *mrb)
{
  struct RClass *f;

  f = _define_class(mrb, "FileTest", mrb->object_class);

  _define_class_method(mrb, f, "directory?", _filetest_s_directory_p, MRB_ARGS_REQ(1));
  _define_class_method(mrb, f, "exist?",     _filetest_s_exist_p,     MRB_ARGS_REQ(1));
  _define_class_method(mrb, f, "exists?",    _filetest_s_exist_p,     MRB_ARGS_REQ(1));
  _define_class_method(mrb, f, "file?",      _filetest_s_file_p,      MRB_ARGS_REQ(1));
  _define_class_method(mrb, f, "pipe?",      _filetest_s_pipe_p,      MRB_ARGS_REQ(1));
  _define_class_method(mrb, f, "size",       _filetest_s_size,        MRB_ARGS_REQ(1));
  _define_class_method(mrb, f, "size?",      _filetest_s_size_p,      MRB_ARGS_REQ(1));
  _define_class_method(mrb, f, "socket?",    _filetest_s_socket_p,    MRB_ARGS_REQ(1));
  _define_class_method(mrb, f, "symlink?",   _filetest_s_symlink_p,   MRB_ARGS_REQ(1));
  _define_class_method(mrb, f, "zero?",      _filetest_s_zero_p,      MRB_ARGS_REQ(1));
}
