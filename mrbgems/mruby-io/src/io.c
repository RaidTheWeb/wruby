/*
** io.c - IO class
*/

#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/hash.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "mruby/ext/io.h"

#if MRUBY_RELEASE_NO < 10000
#include "error.h"
#else
#include "mruby/error.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
  #include <winsock.h>
  #include <io.h>
  #define open  _open
  #define close _close
  #define dup _dup
  #define dup2 _dup2
  #define read  _read
  #define write _write
  #define lseek _lseek
  #define isatty _isatty
  #define WEXITSTATUS(x) (x)
  typedef int fsize_t;
  typedef long ftime_t;
  typedef long fsuseconds_t;
  typedef int fmode_t;

#else
  #include <sys/wait.h>
  #include <unistd.h>
  typedef size_t fsize_t;
  typedef time_t ftime_t;
  typedef suseconds_t fsuseconds_t;
  typedef mode_t fmode_t;
#endif

#ifdef _MSC_VER
typedef _int pid_t;
#endif

#include <fcntl.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>


static void _io_free(_state *mrb, void *ptr);
struct _data_type _io_type = { "IO", _io_free };


static struct _io *io_get_open_fptr(_state *mrb, _value self);
static int _io_modestr_to_flags(_state *mrb, const char *modestr);
static int _io_flags_to_modenum(_state *mrb, int flags);
static void fptr_finalize(_state *mrb, struct _io *fptr, int quiet);

#if MRUBY_RELEASE_NO < 10000
static struct RClass *
_module_get(_state *mrb, const char *name)
{
  return _class_get(mrb, name);
}
#endif

static struct _io *
io_get_open_fptr(_state *mrb, _value self)
{
  struct _io *fptr;

  fptr = (struct _io *)_get_datatype(mrb, self, &_io_type);
  if (fptr == NULL) {
    _raise(mrb, E_IO_ERROR, "uninitialized stream.");
  }
  if (fptr->fd < 0) {
    _raise(mrb, E_IO_ERROR, "closed stream.");
  }
  return fptr;
}

static void
io_set_process_status(_state *mrb, pid_t pid, int status)
{
  struct RClass *c_process, *c_status;
  _value v;

  c_status = NULL;
  if (_class_defined(mrb, "Process")) {
    c_process = _module_get(mrb, "Process");
    if (_const_defined(mrb, _obj_value(c_process), _intern_cstr(mrb, "Status"))) {
      c_status = _class_get_under(mrb, c_process, "Status");
    }
  }
  if (c_status != NULL) {
    v = _funcall(mrb, _obj_value(c_status), "new", 2, _fixnum_value(pid), _fixnum_value(status));
  } else {
    v = _fixnum_value(WEXITSTATUS(status));
  }
  _gv_set(mrb, _intern_cstr(mrb, "$?"), v);
}

static int
_io_modestr_to_flags(_state *mrb, const char *mode)
{
  int flags = 0;
  const char *m = mode;

  switch (*m++) {
    case 'r':
      flags |= FMODE_READABLE;
      break;
    case 'w':
      flags |= FMODE_WRITABLE | FMODE_CREATE | FMODE_TRUNC;
      break;
    case 'a':
      flags |= FMODE_WRITABLE | FMODE_APPEND | FMODE_CREATE;
      break;
    default:
      _raisef(mrb, E_ARGUMENT_ERROR, "illegal access mode %S", _str_new_cstr(mrb, mode));
  }

  while (*m) {
    switch (*m++) {
      case 'b':
        flags |= FMODE_BINMODE;
        break;
      case '+':
        flags |= FMODE_READWRITE;
        break;
      case ':':
        /* XXX: PASSTHROUGH*/
      default:
        _raisef(mrb, E_ARGUMENT_ERROR, "illegal access mode %S", _str_new_cstr(mrb, mode));
    }
  }

  return flags;
}

static int
_io_flags_to_modenum(_state *mrb, int flags)
{
  int modenum = 0;

  switch(flags & (FMODE_READABLE|FMODE_WRITABLE|FMODE_READWRITE)) {
    case FMODE_READABLE:
      modenum = O_RDONLY;
      break;
    case FMODE_WRITABLE:
      modenum = O_WRONLY;
      break;
    case FMODE_READWRITE:
      modenum = O_RDWR;
      break;
  }

  if (flags & FMODE_APPEND) {
    modenum |= O_APPEND;
  }
  if (flags & FMODE_TRUNC) {
    modenum |= O_TRUNC;
  }
  if (flags & FMODE_CREATE) {
    modenum |= O_CREAT;
  }
#ifdef O_BINARY
  if (flags & FMODE_BINMODE) {
    modenum |= O_BINARY;
  }
#endif

  return modenum;
}

static void
_fd_cloexec(_state *mrb, int fd)
{
#if defined(F_GETFD) && defined(F_SETFD) && defined(FD_CLOEXEC)
  int flags, flags2;

  flags = fcntl(fd, F_GETFD);
  if (flags == -1) {
    _bug(mrb, "_fd_cloexec: fcntl(%S, F_GETFD) failed: %S",
      _fixnum_value(fd), _fixnum_value(errno));
  }
  if (fd <= 2) {
    flags2 = flags & ~FD_CLOEXEC; /* Clear CLOEXEC for standard file descriptors: 0, 1, 2. */
  }
  else {
    flags2 = flags | FD_CLOEXEC; /* Set CLOEXEC for non-standard file descriptors: 3, 4, 5, ... */
  }
  if (flags != flags2) {
    if (fcntl(fd, F_SETFD, flags2) == -1) {
      _bug(mrb, "_fd_cloexec: fcntl(%S, F_SETFD, %S) failed: %S",
        _fixnum_value(fd), _fixnum_value(flags2), _fixnum_value(errno));
    }
  }
#endif
}

#if !defined(_WIN32) && !TARGET_OS_IPHONE
static int
_cloexec_pipe(_state *mrb, int fildes[2])
{
  int ret;
  ret = pipe(fildes);
  if (ret == -1)
    return -1;
  _fd_cloexec(mrb, fildes[0]);
  _fd_cloexec(mrb, fildes[1]);
  return ret;
}

static int
_pipe(_state *mrb, int pipes[2])
{
  int ret;
  ret = _cloexec_pipe(mrb, pipes);
  if (ret == -1) {
    if (errno == EMFILE || errno == ENFILE) {
      _garbage_collect(mrb);
      ret = _cloexec_pipe(mrb, pipes);
    }
  }
  return ret;
}

static int
_proc_exec(const char *pname)
{
  const char *s;
  s = pname;

  while (*s == ' ' || *s == '\t' || *s == '\n')
    s++;

  if (!*s) {
    errno = ENOENT;
    return -1;
  }

  execl("/bin/sh", "sh", "-c", pname, (char *)NULL);
  return -1;
}
#endif

static void
_io_free(_state *mrb, void *ptr)
{
  struct _io *io = (struct _io *)ptr;
  if (io != NULL) {
    fptr_finalize(mrb, io, TRUE);
    _free(mrb, io);
  }
}

static struct _io *
_io_alloc(_state *mrb)
{
  struct _io *fptr;

  fptr = (struct _io *)_malloc(mrb, sizeof(struct _io));
  fptr->fd = -1;
  fptr->fd2 = -1;
  fptr->pid = 0;
  fptr->readable = 0;
  fptr->writable = 0;
  fptr->sync = 0;
  fptr->is_socket = 0;
  return fptr;
}

#ifndef NOFILE
#define NOFILE 64
#endif

static int
option_to_fd(_state *mrb, _value obj, const char *key)
{
  _value opt = _funcall(mrb, obj, "[]", 1, _symbol_value(_intern_static(mrb, key, strlen(key))));
  if (_nil_p(opt)) {
    return -1;
  }

  switch (_type(opt)) {
    case MRB_TT_DATA: /* IO */
      return (int)_fixnum(_io_fileno(mrb, opt));
    case MRB_TT_FIXNUM:
      return (int)_fixnum(opt);
    default:
      _raise(mrb, E_ARGUMENT_ERROR, "wrong exec redirect action");
      break;
  }
  return -1; /* never reached */
}

#ifdef _WIN32
_value
_io_s_popen(_state *mrb, _value klass)
{
  _value cmd, io;
  _value mode = _str_new_cstr(mrb, "r");
  _value opt  = _hash_new(mrb);

  struct _io *fptr;
  const char *pname;
  int pid = 0, flags;
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  SECURITY_ATTRIBUTES saAttr;

  HANDLE ifd[2];
  HANDLE ofd[2];

  int doexec;
  int opt_in, opt_out, opt_err;

  ifd[0] = INVALID_HANDLE_VALUE;
  ifd[1] = INVALID_HANDLE_VALUE;
  ofd[0] = INVALID_HANDLE_VALUE;
  ofd[1] = INVALID_HANDLE_VALUE;

  _get_args(mrb, "S|SH", &cmd, &mode, &opt);
  io = _obj_value(_data_object_alloc(mrb, _class_ptr(klass), NULL, &_io_type));

  pname = _string_value_cstr(mrb, &cmd);
  flags = _io_modestr_to_flags(mrb, _string_value_cstr(mrb, &mode));

  doexec = (strcmp("-", pname) != 0);
  opt_in = option_to_fd(mrb, opt, "in");
  opt_out = option_to_fd(mrb, opt, "out");
  opt_err = option_to_fd(mrb, opt, "err");

  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  if (flags & FMODE_READABLE) {
    if (!CreatePipe(&ofd[0], &ofd[1], &saAttr, 0)
        || !SetHandleInformation(ofd[0], HANDLE_FLAG_INHERIT, 0)) {
      _sys_fail(mrb, "pipe");
    }
  }

  if (flags & FMODE_WRITABLE) {
    if (!CreatePipe(&ifd[0], &ifd[1], &saAttr, 0)
        || !SetHandleInformation(ifd[1], HANDLE_FLAG_INHERIT, 0)) {
      _sys_fail(mrb, "pipe");
    }
  }

  if (doexec) {
    ZeroMemory(&pi, sizeof(pi));
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.dwFlags |= STARTF_USESTDHANDLES;
    if (flags & FMODE_READABLE) {
      si.hStdOutput = ofd[1];
      si.hStdError = ofd[1];
    }
    if (flags & FMODE_WRITABLE) {
      si.hStdInput = ifd[0];
    }
    if (!CreateProcess(
        NULL, (char*)pname, NULL, NULL,
        TRUE, CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi)) {
      CloseHandle(ifd[0]);
      CloseHandle(ifd[1]);
      CloseHandle(ofd[0]);
      CloseHandle(ofd[1]);
      _raisef(mrb, E_IO_ERROR, "command not found: %S", cmd);
    }
    CloseHandle(pi.hThread);
    CloseHandle(ifd[0]);
    CloseHandle(ofd[1]);
    pid = pi.dwProcessId;
  }

  _iv_set(mrb, io, _intern_cstr(mrb, "@buf"), _str_new_cstr(mrb, ""));

  fptr = _io_alloc(mrb);
  fptr->fd = _open_osfhandle((intptr_t)ofd[0], 0);
  fptr->fd2 = _open_osfhandle((intptr_t)ifd[1], 0);
  fptr->pid = pid;
  fptr->readable = ((flags & FMODE_READABLE) != 0);
  fptr->writable = ((flags & FMODE_WRITABLE) != 0);
  fptr->sync = 0;

  DATA_TYPE(io) = &_io_type;
  DATA_PTR(io)  = fptr;
  return io;
}
#elif TARGET_OS_IPHONE
_value
_io_s_popen(_state *mrb, _value klass)
{
  _raise(mrb, E_NOTIMP_ERROR, "IO#popen is not supported on the platform");
  return _false_value();
}
#else
_value
_io_s_popen(_state *mrb, _value klass)
{
  _value cmd, io, result;
  _value mode = _str_new_cstr(mrb, "r");
  _value opt  = _hash_new(mrb);

  struct _io *fptr;
  const char *pname;
  int pid, flags, fd, write_fd = -1;
  int pr[2] = { -1, -1 };
  int pw[2] = { -1, -1 };
  int doexec;
  int saved_errno;
  int opt_in, opt_out, opt_err;

  _get_args(mrb, "S|SH", &cmd, &mode, &opt);
  io = _obj_value(_data_object_alloc(mrb, _class_ptr(klass), NULL, &_io_type));

  pname = _string_value_cstr(mrb, &cmd);
  flags = _io_modestr_to_flags(mrb, _string_value_cstr(mrb, &mode));

  doexec = (strcmp("-", pname) != 0);
  opt_in = option_to_fd(mrb, opt, "in");
  opt_out = option_to_fd(mrb, opt, "out");
  opt_err = option_to_fd(mrb, opt, "err");

  if (flags & FMODE_READABLE) {
    if (pipe(pr) == -1) {
      _sys_fail(mrb, "pipe");
    }
    _fd_cloexec(mrb, pr[0]);
    _fd_cloexec(mrb, pr[1]);
  }

  if (flags & FMODE_WRITABLE) {
    if (pipe(pw) == -1) {
      if (pr[0] != -1) close(pr[0]);
      if (pr[1] != -1) close(pr[1]);
      _sys_fail(mrb, "pipe");
    }
    _fd_cloexec(mrb, pw[0]);
    _fd_cloexec(mrb, pw[1]);
  }

  if (!doexec) {
    // XXX
    fflush(stdin);
    fflush(stdout);
    fflush(stderr);
  }

  result = _nil_value();
  switch (pid = fork()) {
    case 0: /* child */
      if (opt_in != -1) {
        dup2(opt_in, 0);
      }
      if (opt_out != -1) {
        dup2(opt_out, 1);
      }
      if (opt_err != -1) {
        dup2(opt_err, 2);
      }
      if (flags & FMODE_READABLE) {
        close(pr[0]);
        if (pr[1] != 1) {
          dup2(pr[1], 1);
          close(pr[1]);
        }
      }
      if (flags & FMODE_WRITABLE) {
        close(pw[1]);
        if (pw[0] != 0) {
          dup2(pw[0], 0);
          close(pw[0]);
        }
      }
      if (doexec) {
        for (fd = 3; fd < NOFILE; fd++) {
          close(fd);
        }
        _proc_exec(pname);
        _raisef(mrb, E_IO_ERROR, "command not found: %S", cmd);
        _exit(127);
      }
      result = _nil_value();
      break;

    default: /* parent */
      if ((flags & FMODE_READABLE) && (flags & FMODE_WRITABLE)) {
        close(pr[1]);
        fd = pr[0];
        close(pw[0]);
        write_fd = pw[1];
      } else if (flags & FMODE_READABLE) {
        close(pr[1]);
        fd = pr[0];
      } else {
        close(pw[0]);
        fd = pw[1];
      }

      _iv_set(mrb, io, _intern_cstr(mrb, "@buf"), _str_new_cstr(mrb, ""));

      fptr = _io_alloc(mrb);
      fptr->fd = fd;
      fptr->fd2 = write_fd;
      fptr->pid = pid;
      fptr->readable = ((flags & FMODE_READABLE) != 0);
      fptr->writable = ((flags & FMODE_WRITABLE) != 0);
      fptr->sync = 0;

      DATA_TYPE(io) = &_io_type;
      DATA_PTR(io)  = fptr;
      result = io;
      break;

    case -1: /* error */
      saved_errno = errno;
      if (flags & FMODE_READABLE) {
        close(pr[0]);
        close(pr[1]);
      }
      if (flags & FMODE_WRITABLE) {
        close(pw[0]);
        close(pw[1]);
      }
      errno = saved_errno;
      _sys_fail(mrb, "pipe_open failed.");
      break;
  }
  return result;
}
#endif

static int
_dup(_state *mrb, int fd, _bool *failed)
{
  int new_fd;

  *failed = TRUE;
  if (fd < 0)
    return fd;

  new_fd = dup(fd);
  if (new_fd > 0) *failed = FALSE;
  return new_fd;
}

_value
_io_initialize_copy(_state *mrb, _value copy)
{
  _value orig;
  _value buf;
  struct _io *fptr_copy;
  struct _io *fptr_orig;
  _bool failed = TRUE;

  _get_args(mrb, "o", &orig);
  fptr_orig = io_get_open_fptr(mrb, orig);
  fptr_copy = (struct _io *)DATA_PTR(copy);
  if (fptr_orig == fptr_copy) return copy;
  if (fptr_copy != NULL) {
    fptr_finalize(mrb, fptr_copy, FALSE);
    _free(mrb, fptr_copy);
  }
  fptr_copy = (struct _io *)_io_alloc(mrb);

  DATA_TYPE(copy) = &_io_type;
  DATA_PTR(copy) = fptr_copy;

  buf = _iv_get(mrb, orig, _intern_cstr(mrb, "@buf"));
  _iv_set(mrb, copy, _intern_cstr(mrb, "@buf"), buf);

  fptr_copy->fd = _dup(mrb, fptr_orig->fd, &failed);
  if (failed) {
    _sys_fail(mrb, 0);
  }
  _fd_cloexec(mrb, fptr_copy->fd);

  if (fptr_orig->fd2 != -1) {
    fptr_copy->fd2 = _dup(mrb, fptr_orig->fd2, &failed);
    if (failed) {
      close(fptr_copy->fd);
      _sys_fail(mrb, 0);
    }
    _fd_cloexec(mrb, fptr_copy->fd2);
  }

  fptr_copy->pid = fptr_orig->pid;
  fptr_copy->readable = fptr_orig->readable;
  fptr_copy->writable = fptr_orig->writable;
  fptr_copy->sync = fptr_orig->sync;
  fptr_copy->is_socket = fptr_orig->is_socket;

  return copy;
}

_value
_io_initialize(_state *mrb, _value io)
{
  struct _io *fptr;
  _int fd;
  _value mode, opt;
  int flags;

  mode = opt = _nil_value();

  _get_args(mrb, "i|So", &fd, &mode, &opt);
  if (_nil_p(mode)) {
    mode = _str_new_cstr(mrb, "r");
  }
  if (_nil_p(opt)) {
    opt = _hash_new(mrb);
  }

  flags = _io_modestr_to_flags(mrb, _string_value_cstr(mrb, &mode));

  _iv_set(mrb, io, _intern_cstr(mrb, "@buf"), _str_new_cstr(mrb, ""));

  fptr = (struct _io *)DATA_PTR(io);
  if (fptr != NULL) {
    fptr_finalize(mrb, fptr, TRUE);
    _free(mrb, fptr);
  }
  fptr = _io_alloc(mrb);

  DATA_TYPE(io) = &_io_type;
  DATA_PTR(io) = fptr;

  fptr->fd = (int)fd;
  fptr->readable = ((flags & FMODE_READABLE) != 0);
  fptr->writable = ((flags & FMODE_WRITABLE) != 0);
  fptr->sync = 0;
  return io;
}

static void
fptr_finalize(_state *mrb, struct _io *fptr, int quiet)
{
  int saved_errno = 0;

  if (fptr == NULL) {
    return;
  }

  if (fptr->fd > 2) {
#ifdef _WIN32
    if (fptr->is_socket) {
      if (closesocket(fptr->fd) != 0) {
        saved_errno = WSAGetLastError();
      }
      fptr->fd = -1;
    }
#endif
    if (fptr->fd != -1) {
      if (close(fptr->fd) == -1) {
        saved_errno = errno;
      }
    }
    fptr->fd = -1;
  }

  if (fptr->fd2 > 2) {
    if (close(fptr->fd2) == -1) {
      if (saved_errno == 0) {
        saved_errno = errno;
      }
    }
    fptr->fd2 = -1;
  }

  if (fptr->pid != 0) {
#if !defined(_WIN32) && !defined(_WIN64)
    pid_t pid;
    int status;
    do {
      pid = waitpid(fptr->pid, &status, 0);
    } while (pid == -1 && errno == EINTR);
    if (!quiet && pid == fptr->pid) {
      io_set_process_status(mrb, pid, status);
    }
#else
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, fptr->pid);
    DWORD status;
    if (WaitForSingleObject(h, INFINITE) && GetExitCodeProcess(h, &status))
      if (!quiet)
        io_set_process_status(mrb, fptr->pid, (int)status);
    CloseHandle(h);
#endif
    fptr->pid = 0;
    /* Note: we don't raise an exception when waitpid(3) fails */
  }

  if (!quiet && saved_errno != 0) {
    errno = saved_errno;
    _sys_fail(mrb, "fptr_finalize failed.");
  }
}

_value
_io_check_readable(_state *mrb, _value self)
{
  struct _io *fptr = io_get_open_fptr(mrb, self);
  if (! fptr->readable) {
    _raise(mrb, E_IO_ERROR, "not opened for reading");
  }
  return _nil_value();
}

_value
_io_isatty(_state *mrb, _value self)
{
  struct _io *fptr;

  fptr = io_get_open_fptr(mrb, self);
  if (isatty(fptr->fd) == 0)
    return _false_value();
  return _true_value();
}

_value
_io_s_for_fd(_state *mrb, _value klass)
{
  struct RClass *c = _class_ptr(klass);
  enum _vtype ttype = MRB_INSTANCE_TT(c);
  _value obj;

  /* copied from _instance_alloc() */
  if (ttype == 0) ttype = MRB_TT_OBJECT;
  obj = _obj_value((struct RObject*)_obj_alloc(mrb, ttype, c));
  return _io_initialize(mrb, obj);
}

_value
_io_s_sysclose(_state *mrb, _value klass)
{
  _int fd;
  _get_args(mrb, "i", &fd);
  if (close((int)fd) == -1) {
    _sys_fail(mrb, "close");
  }
  return _fixnum_value(0);
}

int
_cloexec_open(_state *mrb, const char *pathname, _int flags, _int mode)
{
  _value emsg;
  int fd, retry = FALSE;
  char* fname = _locale_from_utf8(pathname, -1);

#ifdef O_CLOEXEC
  /* O_CLOEXEC is available since Linux 2.6.23.  Linux 2.6.18 silently ignore it. */
  flags |= O_CLOEXEC;
#elif defined O_NOINHERIT
  flags |= O_NOINHERIT;
#endif
reopen:
  fd = open(fname, (int)flags, (fmode_t)mode);
  if (fd == -1) {
    if (!retry) {
      switch (errno) {
        case ENFILE:
        case EMFILE:
        _garbage_collect(mrb);
        retry = TRUE;
        goto reopen;
      }
    }

    emsg = _format(mrb, "open %S", _str_new_cstr(mrb, pathname));
    _str_modify(mrb, _str_ptr(emsg));
    _sys_fail(mrb, RSTRING_PTR(emsg));
  }
  _locale_free(fname);

  if (fd <= 2) {
    _fd_cloexec(mrb, fd);
  }
  return fd;
}

_value
_io_s_sysopen(_state *mrb, _value klass)
{
  _value path = _nil_value();
  _value mode = _nil_value();
  _int fd, perm = -1;
  const char *pat;
  int flags, modenum;

  _get_args(mrb, "S|Si", &path, &mode, &perm);
  if (_nil_p(mode)) {
    mode = _str_new_cstr(mrb, "r");
  }
  if (perm < 0) {
    perm = 0666;
  }

  pat = _string_value_cstr(mrb, &path);
  flags = _io_modestr_to_flags(mrb, _string_value_cstr(mrb, &mode));
  modenum = _io_flags_to_modenum(mrb, flags);
  fd = _cloexec_open(mrb, pat, modenum, perm);
  return _fixnum_value(fd);
}

_value
_io_sysread(_state *mrb, _value io)
{
  struct _io *fptr;
  _value buf = _nil_value();
  _int maxlen;
  int ret;

  _get_args(mrb, "i|S", &maxlen, &buf);
  if (maxlen < 0) {
    _raise(mrb, E_ARGUMENT_ERROR, "negative expanding string size");
  }
  else if (maxlen == 0) {
    return _str_new(mrb, NULL, maxlen);
  }

  if (_nil_p(buf)) {
    buf = _str_new(mrb, NULL, maxlen);
  }

  if (RSTRING_LEN(buf) != maxlen) {
    buf = _str_resize(mrb, buf, maxlen);
  } else {
    _str_modify(mrb, RSTRING(buf));
  }

  fptr = (struct _io *)io_get_open_fptr(mrb, io);
  if (!fptr->readable) {
    _raise(mrb, E_IO_ERROR, "not opened for reading");
  }
  ret = read(fptr->fd, RSTRING_PTR(buf), (fsize_t)maxlen);
  switch (ret) {
    case 0: /* EOF */
      if (maxlen == 0) {
        buf = _str_new_cstr(mrb, "");
      } else {
        _raise(mrb, E_EOF_ERROR, "sysread failed: End of File");
      }
      break;
    case -1: /* Error */
      _sys_fail(mrb, "sysread failed");
      break;
    default:
      if (RSTRING_LEN(buf) != ret) {
        buf = _str_resize(mrb, buf, ret);
      }
      break;
  }

  return buf;
}

_value
_io_sysseek(_state *mrb, _value io)
{
  struct _io *fptr;
  off_t pos;
  _int offset, whence = -1;

  _get_args(mrb, "i|i", &offset, &whence);
  if (whence < 0) {
    whence = 0;
  }

  fptr = io_get_open_fptr(mrb, io);
  pos = lseek(fptr->fd, (off_t)offset, (int)whence);
  if (pos == -1) {
    _sys_fail(mrb, "sysseek");
  }
  if (pos > MRB_INT_MAX) {
#ifndef MRB_WITHOUT_FLOAT
    return _float_value(mrb, (_float)pos);
#else
    _raise(mrb, E_IO_ERROR, "sysseek reached too far for MRB_WITHOUT_FLOAT");
#endif
  } else {
    return _fixnum_value(pos);
  }
}

_value
_io_syswrite(_state *mrb, _value io)
{
  struct _io *fptr;
  _value str, buf;
  int fd, length;

  fptr = io_get_open_fptr(mrb, io);
  if (! fptr->writable) {
    _raise(mrb, E_IO_ERROR, "not opened for writing");
  }

  _get_args(mrb, "S", &str);
  if (_type(str) != MRB_TT_STRING) {
    buf = _funcall(mrb, str, "to_s", 0);
  } else {
    buf = str;
  }

  if (fptr->fd2 == -1) {
    fd = fptr->fd;
  } else {
    fd = fptr->fd2;
  }
  length = write(fd, RSTRING_PTR(buf), (fsize_t)RSTRING_LEN(buf));
  if (length == -1) {
    _sys_fail(mrb, 0);
  }

  return _fixnum_value(length);
}

_value
_io_close(_state *mrb, _value self)
{
  struct _io *fptr;
  fptr = io_get_open_fptr(mrb, self);
  fptr_finalize(mrb, fptr, FALSE);
  return _nil_value();
}

_value
_io_close_write(_state *mrb, _value self)
{
  struct _io *fptr;
  fptr = io_get_open_fptr(mrb, self);
  if (close((int)fptr->fd2) == -1) {
    _sys_fail(mrb, "close");
  }
  return _nil_value();
}

_value
_io_closed(_state *mrb, _value io)
{
  struct _io *fptr;
  fptr = (struct _io *)_get_datatype(mrb, io, &_io_type);
  if (fptr == NULL || fptr->fd >= 0) {
    return _false_value();
  }

  return _true_value();
}

_value
_io_pid(_state *mrb, _value io)
{
  struct _io *fptr;
  fptr = io_get_open_fptr(mrb, io);

  if (fptr->pid > 0) {
    return _fixnum_value(fptr->pid);
  }

  return _nil_value();
}

static struct timeval
time2timeval(_state *mrb, _value time)
{
  struct timeval t = { 0, 0 };

  switch (_type(time)) {
    case MRB_TT_FIXNUM:
      t.tv_sec = (ftime_t)_fixnum(time);
      t.tv_usec = 0;
      break;

#ifndef MRB_WITHOUT_FLOAT
    case MRB_TT_FLOAT:
      t.tv_sec = (ftime_t)_float(time);
      t.tv_usec = (fsuseconds_t)((_float(time) - t.tv_sec) * 1000000.0);
      break;
#endif

    default:
      _raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }

  return t;
}

static int
_io_read_data_pending(_state *mrb, _value io)
{
  _value buf = _iv_get(mrb, io, _intern_cstr(mrb, "@buf"));
  if (_type(buf) == MRB_TT_STRING && RSTRING_LEN(buf) > 0) {
    return 1;
  }
  return 0;
}

#if !defined(_WIN32) && !TARGET_OS_IPHONE
static _value
_io_s_pipe(_state *mrb, _value klass)
{
  _value r = _nil_value();
  _value w = _nil_value();
  struct _io *fptr_r;
  struct _io *fptr_w;
  int pipes[2];

  if (_pipe(mrb, pipes) == -1) {
    _sys_fail(mrb, "pipe");
  }

  r = _obj_value(_data_object_alloc(mrb, _class_ptr(klass), NULL, &_io_type));
  _iv_set(mrb, r, _intern_cstr(mrb, "@buf"), _str_new_cstr(mrb, ""));
  fptr_r = _io_alloc(mrb);
  fptr_r->fd = pipes[0];
  fptr_r->readable = 1;
  fptr_r->writable = 0;
  fptr_r->sync = 0;
  DATA_TYPE(r) = &_io_type;
  DATA_PTR(r)  = fptr_r;

  w = _obj_value(_data_object_alloc(mrb, _class_ptr(klass), NULL, &_io_type));
  _iv_set(mrb, w, _intern_cstr(mrb, "@buf"), _str_new_cstr(mrb, ""));
  fptr_w = _io_alloc(mrb);
  fptr_w->fd = pipes[1];
  fptr_w->readable = 0;
  fptr_w->writable = 1;
  fptr_w->sync = 1;
  DATA_TYPE(w) = &_io_type;
  DATA_PTR(w)  = fptr_w;

  return _assoc_new(mrb, r, w);
}
#endif

static _value
_io_s_select(_state *mrb, _value klass)
{
  _value *argv;
  _int argc;
  _value read, read_io, write, except, timeout, list;
  struct timeval *tp, timerec;
  fd_set pset, rset, wset, eset;
  fd_set *rp, *wp, *ep;
  struct _io *fptr;
  int pending = 0;
  _value result;
  int max = 0;
  int interrupt_flag = 0;
  int i, n;

  _get_args(mrb, "*", &argv, &argc);

  if (argc < 1 || argc > 4) {
    _raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%S for 1..4)", _fixnum_value(argc));
  }

  timeout = _nil_value();
  except = _nil_value();
  write = _nil_value();
  if (argc > 3)
    timeout = argv[3];
  if (argc > 2)
    except = argv[2];
  if (argc > 1)
    write = argv[1];
  read = argv[0];

  if (_nil_p(timeout)) {
    tp = NULL;
  } else {
    timerec = time2timeval(mrb, timeout);
    tp = &timerec;
  }

  FD_ZERO(&pset);
  if (!_nil_p(read)) {
    _check_type(mrb, read, MRB_TT_ARRAY);
    rp = &rset;
    FD_ZERO(rp);
    for (i = 0; i < RARRAY_LEN(read); i++) {
      read_io = RARRAY_PTR(read)[i];
      fptr = io_get_open_fptr(mrb, read_io);
      FD_SET(fptr->fd, rp);
      if (_io_read_data_pending(mrb, read_io)) {
        pending++;
        FD_SET(fptr->fd, &pset);
      }
      if (max < fptr->fd)
        max = fptr->fd;
    }
    if (pending) {
      timerec.tv_sec = timerec.tv_usec = 0;
      tp = &timerec;
    }
  } else {
    rp = NULL;
  }

  if (!_nil_p(write)) {
    _check_type(mrb, write, MRB_TT_ARRAY);
    wp = &wset;
    FD_ZERO(wp);
    for (i = 0; i < RARRAY_LEN(write); i++) {
      fptr = io_get_open_fptr(mrb, RARRAY_PTR(write)[i]);
      FD_SET(fptr->fd, wp);
      if (max < fptr->fd)
        max = fptr->fd;
      if (fptr->fd2 >= 0) {
        FD_SET(fptr->fd2, wp);
        if (max < fptr->fd2)
          max = fptr->fd2;
      }
    }
  } else {
    wp = NULL;
  }

  if (!_nil_p(except)) {
    _check_type(mrb, except, MRB_TT_ARRAY);
    ep = &eset;
    FD_ZERO(ep);
    for (i = 0; i < RARRAY_LEN(except); i++) {
      fptr = io_get_open_fptr(mrb, RARRAY_PTR(except)[i]);
      FD_SET(fptr->fd, ep);
      if (max < fptr->fd)
        max = fptr->fd;
      if (fptr->fd2 >= 0) {
        FD_SET(fptr->fd2, ep);
        if (max < fptr->fd2)
          max = fptr->fd2;
      }
    }
  } else {
    ep = NULL;
  }

  max++;

retry:
  n = select(max, rp, wp, ep, tp);
  if (n < 0) {
    if (errno != EINTR)
      _sys_fail(mrb, "select failed");
    if (tp == NULL)
      goto retry;
    interrupt_flag = 1;
  }

  if (!pending && n == 0)
    return _nil_value();

  result = _ary_new_capa(mrb, 3);
  _ary_push(mrb, result, rp? _ary_new(mrb) : _ary_new_capa(mrb, 0));
  _ary_push(mrb, result, wp? _ary_new(mrb) : _ary_new_capa(mrb, 0));
  _ary_push(mrb, result, ep? _ary_new(mrb) : _ary_new_capa(mrb, 0));

  if (interrupt_flag == 0) {
    if (rp) {
      list = RARRAY_PTR(result)[0];
      for (i = 0; i < RARRAY_LEN(read); i++) {
        fptr = io_get_open_fptr(mrb, RARRAY_PTR(read)[i]);
        if (FD_ISSET(fptr->fd, rp) ||
            FD_ISSET(fptr->fd, &pset)) {
          _ary_push(mrb, list, RARRAY_PTR(read)[i]);
        }
      }
    }

    if (wp) {
      list = RARRAY_PTR(result)[1];
      for (i = 0; i < RARRAY_LEN(write); i++) {
        fptr = io_get_open_fptr(mrb, RARRAY_PTR(write)[i]);
        if (FD_ISSET(fptr->fd, wp)) {
          _ary_push(mrb, list, RARRAY_PTR(write)[i]);
        } else if (fptr->fd2 >= 0 && FD_ISSET(fptr->fd2, wp)) {
          _ary_push(mrb, list, RARRAY_PTR(write)[i]);
        }
      }
    }

    if (ep) {
      list = RARRAY_PTR(result)[2];
      for (i = 0; i < RARRAY_LEN(except); i++) {
        fptr = io_get_open_fptr(mrb, RARRAY_PTR(except)[i]);
        if (FD_ISSET(fptr->fd, ep)) {
          _ary_push(mrb, list, RARRAY_PTR(except)[i]);
        } else if (fptr->fd2 >= 0 && FD_ISSET(fptr->fd2, ep)) {
          _ary_push(mrb, list, RARRAY_PTR(except)[i]);
        }
      }
    }
  }

  return result;
}

_value
_io_fileno(_state *mrb, _value io)
{
  struct _io *fptr;
  fptr = io_get_open_fptr(mrb, io);
  return _fixnum_value(fptr->fd);
}

_value
_io_close_on_exec_p(_state *mrb, _value self)
{
#if defined(F_GETFD) && defined(F_SETFD) && defined(FD_CLOEXEC)
  struct _io *fptr;
  int ret;

  fptr = io_get_open_fptr(mrb, self);

  if (fptr->fd2 >= 0) {
    if ((ret = fcntl(fptr->fd2, F_GETFD)) == -1) _sys_fail(mrb, "F_GETFD failed");
    if (!(ret & FD_CLOEXEC)) return _false_value();
  }

  if ((ret = fcntl(fptr->fd, F_GETFD)) == -1) _sys_fail(mrb, "F_GETFD failed");
  if (!(ret & FD_CLOEXEC)) return _false_value();
  return _true_value();

#else
  _raise(mrb, E_NOTIMP_ERROR, "IO#close_on_exec? is not supported on the platform");
  return _false_value();
#endif
}

_value
_io_set_close_on_exec(_state *mrb, _value self)
{
#if defined(F_GETFD) && defined(F_SETFD) && defined(FD_CLOEXEC)
  struct _io *fptr;
  int flag, ret;
  _bool b;

  fptr = io_get_open_fptr(mrb, self);
  _get_args(mrb, "b", &b);
  flag = b ? FD_CLOEXEC : 0;

  if (fptr->fd2 >= 0) {
    if ((ret = fcntl(fptr->fd2, F_GETFD)) == -1) _sys_fail(mrb, "F_GETFD failed");
    if ((ret & FD_CLOEXEC) != flag) {
      ret = (ret & ~FD_CLOEXEC) | flag;
      ret = fcntl(fptr->fd2, F_SETFD, ret);

      if (ret == -1) _sys_fail(mrb, "F_SETFD failed");
    }
  }

  if ((ret = fcntl(fptr->fd, F_GETFD)) == -1) _sys_fail(mrb, "F_GETFD failed");
  if ((ret & FD_CLOEXEC) != flag) {
    ret = (ret & ~FD_CLOEXEC) | flag;
    ret = fcntl(fptr->fd, F_SETFD, ret);
    if (ret == -1) _sys_fail(mrb, "F_SETFD failed");
  }

  return _bool_value(b);
#else
  _raise(mrb, E_NOTIMP_ERROR, "IO#close_on_exec= is not supported on the platform");
  return _nil_value();
#endif
}

_value
_io_set_sync(_state *mrb, _value self)
{
  struct _io *fptr;
  _bool b;

  fptr = io_get_open_fptr(mrb, self);
  _get_args(mrb, "b", &b);
  fptr->sync = b;
  return _bool_value(b);
}

_value
_io_sync(_state *mrb, _value self)
{
  struct _io *fptr;
  fptr = io_get_open_fptr(mrb, self);
  return _bool_value(fptr->sync);
}

void
_init_io(_state *mrb)
{
  struct RClass *io;

  io      = _define_class(mrb, "IO", mrb->object_class);
  MRB_SET_INSTANCE_TT(io, MRB_TT_DATA);

  _include_module(mrb, io, _module_get(mrb, "Enumerable")); /* 15.2.20.3 */
  _define_class_method(mrb, io, "_popen",  _io_s_popen,   MRB_ARGS_ANY());
  _define_class_method(mrb, io, "_sysclose",  _io_s_sysclose, MRB_ARGS_REQ(1));
  _define_class_method(mrb, io, "for_fd",  _io_s_for_fd,   MRB_ARGS_ANY());
  _define_class_method(mrb, io, "select",  _io_s_select,  MRB_ARGS_ANY());
  _define_class_method(mrb, io, "sysopen", _io_s_sysopen, MRB_ARGS_ANY());
#if !defined(_WIN32) && !TARGET_OS_IPHONE
  _define_class_method(mrb, io, "_pipe", _io_s_pipe, MRB_ARGS_NONE());
#endif

  _define_method(mrb, io, "initialize", _io_initialize, MRB_ARGS_ANY());    /* 15.2.20.5.21 (x)*/
  _define_method(mrb, io, "initialize_copy", _io_initialize_copy, MRB_ARGS_REQ(1));
  _define_method(mrb, io, "_check_readable", _io_check_readable, MRB_ARGS_NONE());
  _define_method(mrb, io, "isatty",     _io_isatty,     MRB_ARGS_NONE());
  _define_method(mrb, io, "sync",       _io_sync,       MRB_ARGS_NONE());
  _define_method(mrb, io, "sync=",      _io_set_sync,   MRB_ARGS_REQ(1));
  _define_method(mrb, io, "sysread",    _io_sysread,    MRB_ARGS_ANY());
  _define_method(mrb, io, "sysseek",    _io_sysseek,    MRB_ARGS_REQ(1));
  _define_method(mrb, io, "syswrite",   _io_syswrite,   MRB_ARGS_REQ(1));
  _define_method(mrb, io, "close",      _io_close,      MRB_ARGS_NONE());   /* 15.2.20.5.1 */
  _define_method(mrb, io, "close_write",    _io_close_write,       MRB_ARGS_NONE());
  _define_method(mrb, io, "close_on_exec=", _io_set_close_on_exec, MRB_ARGS_REQ(1));
  _define_method(mrb, io, "close_on_exec?", _io_close_on_exec_p,   MRB_ARGS_NONE());
  _define_method(mrb, io, "closed?",    _io_closed,     MRB_ARGS_NONE());   /* 15.2.20.5.2 */
  _define_method(mrb, io, "pid",        _io_pid,        MRB_ARGS_NONE());   /* 15.2.20.5.2 */
  _define_method(mrb, io, "fileno",     _io_fileno,     MRB_ARGS_NONE());


  _gv_set(mrb, _intern_cstr(mrb, "$/"), _str_new_cstr(mrb, "\n"));
}
