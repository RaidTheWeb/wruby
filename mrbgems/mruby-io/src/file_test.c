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

extern struct $data_type $io_type;

static int
$stat0($state *mrb, $value obj, struct stat *st, int do_lstat)
{
  $value tmp;
  $value io_klass, str_klass;

  io_klass  = $obj_value($class_get(mrb, "IO"));
  str_klass = $obj_value($class_get(mrb, "String"));

  tmp = $funcall(mrb, obj, "is_a?", 1, io_klass);
  if ($test(tmp)) {
    struct $io *fptr;
    fptr = (struct $io *)$get_datatype(mrb, obj, &$io_type);

    if (fptr && fptr->fd >= 0) {
      return fstat(fptr->fd, st);
    }

    $raise(mrb, E_IO_ERROR, "closed stream");
    return -1;
  }

  tmp = $funcall(mrb, obj, "is_a?", 1, str_klass);
  if ($test(tmp)) {
    char *path = $locale_from_utf8($str_to_cstr(mrb, obj), -1);
    int ret;
    if (do_lstat) {
      ret = LSTAT(path, st);
    } else {
      ret = stat(path, st);
    }
    $locale_free(path);
    return ret;
  }

  return -1;
}

static int
$stat($state *mrb, $value obj, struct stat *st)
{
  return $stat0(mrb, obj, st, 0);
}

static int
$lstat($state *mrb, $value obj, struct stat *st)
{
  return $stat0(mrb, obj, st, 1);
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

$value
$filetest_s_directory_p($state *mrb, $value klass)
{
#ifndef S_ISDIR
#   define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

  struct stat st;
  $value obj;

  $get_args(mrb, "o", &obj);

  if ($stat(mrb, obj, &st) < 0)
    return $false_value();
  if (S_ISDIR(st.st_mode))
    return $true_value();

  return $false_value();
}

/*
 * call-seq:
 *   File.pipe?(file_name)   ->  true or false
 *
 * Returns <code>true</code> if the named file is a pipe.
 */

$value
$filetest_s_pipe_p($state *mrb, $value klass)
{
#if defined(_WIN32) || defined(_WIN64)
  $raise(mrb, E_NOTIMP_ERROR, "pipe is not supported on this platform");
#else
#ifdef S_IFIFO
#  ifndef S_ISFIFO
#    define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#  endif

  struct stat st;
  $value obj;

  $get_args(mrb, "o", &obj);

  if ($stat(mrb, obj, &st) < 0)
    return $false_value();
  if (S_ISFIFO(st.st_mode))
    return $true_value();

#endif
  return $false_value();
#endif
}

/*
 * call-seq:
 *   File.symlink?(file_name)   ->  true or false
 *
 * Returns <code>true</code> if the named file is a symbolic link.
 */

$value
$filetest_s_symlink_p($state *mrb, $value klass)
{
#if defined(_WIN32) || defined(_WIN64)
  $raise(mrb, E_NOTIMP_ERROR, "symlink is not supported on this platform");
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
  $value obj;

  $get_args(mrb, "o", &obj);

  if ($lstat(mrb, obj, &st) == -1)
    return $false_value();
  if (S_ISLNK(st.st_mode))
    return $true_value();
#endif

  return $false_value();
#endif
}

/*
 * call-seq:
 *   File.socket?(file_name)   ->  true or false
 *
 * Returns <code>true</code> if the named file is a socket.
 */

$value
$filetest_s_socket_p($state *mrb, $value klass)
{
#if defined(_WIN32) || defined(_WIN64)
  $raise(mrb, E_NOTIMP_ERROR, "socket is not supported on this platform");
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
  $value obj;

  $get_args(mrb, "o", &obj);

  if ($stat(mrb, obj, &st) < 0)
    return $false_value();
  if (S_ISSOCK(st.st_mode))
    return $true_value();
#endif

  return $false_value();
#endif
}

/*
 * call-seq:
 *    File.exist?(file_name)    ->  true or false
 *    File.exists?(file_name)   ->  true or false
 *
 * Return <code>true</code> if the named file exists.
 */

$value
$filetest_s_exist_p($state *mrb, $value klass)
{
  struct stat st;
  $value obj;

  $get_args(mrb, "o", &obj);
  if ($stat(mrb, obj, &st) < 0)
    return $false_value();

  return $true_value();
}

/*
 * call-seq:
 *    File.file?(file_name)   -> true or false
 *
 * Returns <code>true</code> if the named file exists and is a
 * regular file.
 */

$value
$filetest_s_file_p($state *mrb, $value klass)
{
#ifndef S_ISREG
#   define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

  struct stat st;
  $value obj;

  $get_args(mrb, "o", &obj);

  if ($stat(mrb, obj, &st) < 0)
    return $false_value();
  if (S_ISREG(st.st_mode))
    return $true_value();

  return $false_value();
}

/*
 * call-seq:
 *    File.zero?(file_name)   -> true or false
 *
 * Returns <code>true</code> if the named file exists and has
 * a zero size.
 */

$value
$filetest_s_zero_p($state *mrb, $value klass)
{
  struct stat st;
  $value obj;

  $get_args(mrb, "o", &obj);

  if ($stat(mrb, obj, &st) < 0)
    return $false_value();
  if (st.st_size == 0)
    return $true_value();

  return $false_value();
}

/*
 * call-seq:
 *    File.size(file_name)   -> integer
 *
 * Returns the size of <code>file_name</code>.
 *
 * _file_name_ can be an IO object.
 */

$value
$filetest_s_size($state *mrb, $value klass)
{
  struct stat st;
  $value obj;

  $get_args(mrb, "o", &obj);

  if ($stat(mrb, obj, &st) < 0)
    $sys_fail(mrb, "$stat");

  return $fixnum_value(st.st_size);
}

/*
 * call-seq:
 *    File.size?(file_name)   -> Integer or nil
 *
 * Returns +nil+ if +file_name+ doesn't exist or has zero size, the size of the
 * file otherwise.
 */

$value
$filetest_s_size_p($state *mrb, $value klass)
{
  struct stat st;
  $value obj;

  $get_args(mrb, "o", &obj);

  if ($stat(mrb, obj, &st) < 0)
    return $nil_value();
  if (st.st_size == 0)
    return $nil_value();

  return $fixnum_value(st.st_size);
}

void
$init_file_test($state *mrb)
{
  struct RClass *f;

  f = $define_class(mrb, "FileTest", mrb->object_class);

  $define_class_method(mrb, f, "directory?", $filetest_s_directory_p, $ARGS_REQ(1));
  $define_class_method(mrb, f, "exist?",     $filetest_s_exist_p,     $ARGS_REQ(1));
  $define_class_method(mrb, f, "exists?",    $filetest_s_exist_p,     $ARGS_REQ(1));
  $define_class_method(mrb, f, "file?",      $filetest_s_file_p,      $ARGS_REQ(1));
  $define_class_method(mrb, f, "pipe?",      $filetest_s_pipe_p,      $ARGS_REQ(1));
  $define_class_method(mrb, f, "size",       $filetest_s_size,        $ARGS_REQ(1));
  $define_class_method(mrb, f, "size?",      $filetest_s_size_p,      $ARGS_REQ(1));
  $define_class_method(mrb, f, "socket?",    $filetest_s_socket_p,    $ARGS_REQ(1));
  $define_class_method(mrb, f, "symlink?",   $filetest_s_symlink_p,   $ARGS_REQ(1));
  $define_class_method(mrb, f, "zero?",      $filetest_s_zero_p,      $ARGS_REQ(1));
}
