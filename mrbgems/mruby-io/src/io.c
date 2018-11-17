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
typedef int pid_t;
#endif

#include <fcntl.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>


static void io_free(state *mrb, void *ptr);
struct data_type io_type = { "IO", io_free };


static struct io *io_get_open_fptr(state *mrb, value self);
static int io_modestr_to_flags(state *mrb, const char *modestr);
static int io_flags_to_modenum(state *mrb, int flags);
static void fptr_finalize(state *mrb, struct io *fptr, int quiet);

#if MRUBY_RELEASE_NO < 10000
static struct RClass *
module_get(state *mrb, const char *name)
{
  return class_get(mrb, name);
}
#endif

static struct io *
io_get_open_fptr(state *mrb, value self)
{
  struct io *fptr;

  fptr = (struct io *)get_datatype(mrb, self, &io_type);
  if (fptr == NULL) {
    raise(mrb, E_IO_ERROR, "uninitialized stream.");
  }
  if (fptr->fd < 0) {
    raise(mrb, E_IO_ERROR, "closed stream.");
  }
  return fptr;
}

static void
io_set_process_status(state *mrb, pid_t pid, int status)
{
  struct RClass *c_process, *c_status;
  value v;

  c_status = NULL;
  if (class_defined(mrb, "Process")) {
    c_process = module_get(mrb, "Process");
    if (const_defined(mrb, obj_value(c_process), intern_cstr(mrb, "Status"))) {
      c_status = class_get_under(mrb, c_process, "Status");
    }
  }
  if (c_status != NULL) {
    v = funcall(mrb, obj_value(c_status), "new", 2, fixnum_value(pid), fixnum_value(status));
  } else {
    v = fixnum_value(WEXITSTATUS(status));
  }
  gv_set(mrb, intern_cstr(mrb, "$?"), v);
}

static int
io_modestr_to_flags(state *mrb, const char *mode)
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
      raisef(mrb, E_ARGUMENT_ERROR, "illegal access mode %S", str_new_cstr(mrb, mode));
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
        raisef(mrb, E_ARGUMENT_ERROR, "illegal access mode %S", str_new_cstr(mrb, mode));
    }
  }

  return flags;
}

static int
io_flags_to_modenum(state *mrb, int flags)
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
fd_cloexec(state *mrb, int fd)
{
#if defined(F_GETFD) && defined(F_SETFD) && defined(FD_CLOEXEC)
  int flags, flags2;

  flags = fcntl(fd, F_GETFD);
  if (flags == -1) {
    bug(mrb, "fd_cloexec: fcntl(%S, F_GETFD) failed: %S",
      fixnum_value(fd), fixnum_value(errno));
  }
  if (fd <= 2) {
    flags2 = flags & ~FD_CLOEXEC; /* Clear CLOEXEC for standard file descriptors: 0, 1, 2. */
  }
  else {
    flags2 = flags | FD_CLOEXEC; /* Set CLOEXEC for non-standard file descriptors: 3, 4, 5, ... */
  }
  if (flags != flags2) {
    if (fcntl(fd, F_SETFD, flags2) == -1) {
      bug(mrb, "fd_cloexec: fcntl(%S, F_SETFD, %S) failed: %S",
        fixnum_value(fd), fixnum_value(flags2), fixnum_value(errno));
    }
  }
#endif
}

#if !defined(_WIN32) && !TARGET_OS_IPHONE
static int
cloexec_pipe(state *mrb, int fildes[2])
{
  int ret;
  ret = pipe(fildes);
  if (ret == -1)
    return -1;
  fd_cloexec(mrb, fildes[0]);
  fd_cloexec(mrb, fildes[1]);
  return ret;
}

static int
pipe(state *mrb, int pipes[2])
{
  int ret;
  ret = cloexec_pipe(mrb, pipes);
  if (ret == -1) {
    if (errno == EMFILE || errno == ENFILE) {
      garbage_collect(mrb);
      ret = cloexec_pipe(mrb, pipes);
    }
  }
  return ret;
}

static int
proc_exec(const char *pname)
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
io_free(state *mrb, void *ptr)
{
  struct io *io = (struct io *)ptr;
  if (io != NULL) {
    fptr_finalize(mrb, io, TRUE);
    free(mrb, io);
  }
}

static struct io *
io_alloc(state *mrb)
{
  struct io *fptr;

  fptr = (struct io *)malloc(mrb, sizeof(struct io));
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
option_to_fd(state *mrb, value obj, const char *key)
{
  value opt = funcall(mrb, obj, "[]", 1, symbol_value(intern_static(mrb, key, strlen(key))));
  if (nil_p(opt)) {
    return -1;
  }

  switch (type(opt)) {
    case TT_DATA: /* IO */
      return (int)fixnum(io_fileno(mrb, opt));
    case TT_FIXNUM:
      return (int)fixnum(opt);
    default:
      raise(mrb, E_ARGUMENT_ERROR, "wrong exec redirect action");
      break;
  }
  return -1; /* never reached */
}

#ifdef _WIN32
value
io_s_popen(state *mrb, value klass)
{
  value cmd, io;
  value mode = str_new_cstr(mrb, "r");
  value opt  = hash_new(mrb);

  struct io *fptr;
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

  get_args(mrb, "S|SH", &cmd, &mode, &opt);
  io = obj_value(data_object_alloc(mrb, class_ptr(klass), NULL, &io_type));

  pname = string_value_cstr(mrb, &cmd);
  flags = io_modestr_to_flags(mrb, string_value_cstr(mrb, &mode));

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
      sys_fail(mrb, "pipe");
    }
  }

  if (flags & FMODE_WRITABLE) {
    if (!CreatePipe(&ifd[0], &ifd[1], &saAttr, 0)
        || !SetHandleInformation(ifd[1], HANDLE_FLAG_INHERIT, 0)) {
      sys_fail(mrb, "pipe");
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
      raisef(mrb, E_IO_ERROR, "command not found: %S", cmd);
    }
    CloseHandle(pi.hThread);
    CloseHandle(ifd[0]);
    CloseHandle(ofd[1]);
    pid = pi.dwProcessId;
  }

  iv_set(mrb, io, intern_cstr(mrb, "@buf"), str_new_cstr(mrb, ""));

  fptr = io_alloc(mrb);
  fptr->fd = _open_osfhandle((intptr_t)ofd[0], 0);
  fptr->fd2 = _open_osfhandle((intptr_t)ifd[1], 0);
  fptr->pid = pid;
  fptr->readable = ((flags & FMODE_READABLE) != 0);
  fptr->writable = ((flags & FMODE_WRITABLE) != 0);
  fptr->sync = 0;

  DATA_TYPE(io) = &io_type;
  DATA_PTR(io)  = fptr;
  return io;
}
#elif TARGET_OS_IPHONE
value
io_s_popen(state *mrb, value klass)
{
  raise(mrb, E_NOTIMP_ERROR, "IO#popen is not supported on the platform");
  return false_value();
}
#else
value
io_s_popen(state *mrb, value klass)
{
  value cmd, io, result;
  value mode = str_new_cstr(mrb, "r");
  value opt  = hash_new(mrb);

  struct io *fptr;
  const char *pname;
  int pid, flags, fd, write_fd = -1;
  int pr[2] = { -1, -1 };
  int pw[2] = { -1, -1 };
  int doexec;
  int saved_errno;
  int opt_in, opt_out, opt_err;

  get_args(mrb, "S|SH", &cmd, &mode, &opt);
  io = obj_value(data_object_alloc(mrb, class_ptr(klass), NULL, &io_type));

  pname = string_value_cstr(mrb, &cmd);
  flags = io_modestr_to_flags(mrb, string_value_cstr(mrb, &mode));

  doexec = (strcmp("-", pname) != 0);
  opt_in = option_to_fd(mrb, opt, "in");
  opt_out = option_to_fd(mrb, opt, "out");
  opt_err = option_to_fd(mrb, opt, "err");

  if (flags & FMODE_READABLE) {
    if (pipe(pr) == -1) {
      sys_fail(mrb, "pipe");
    }
    fd_cloexec(mrb, pr[0]);
    fd_cloexec(mrb, pr[1]);
  }

  if (flags & FMODE_WRITABLE) {
    if (pipe(pw) == -1) {
      if (pr[0] != -1) close(pr[0]);
      if (pr[1] != -1) close(pr[1]);
      sys_fail(mrb, "pipe");
    }
    fd_cloexec(mrb, pw[0]);
    fd_cloexec(mrb, pw[1]);
  }

  if (!doexec) {
    // XXX
    fflush(stdin);
    fflush(stdout);
    fflush(stderr);
  }

  result = nil_value();
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
        proc_exec(pname);
        raisef(mrb, E_IO_ERROR, "command not found: %S", cmd);
        _exit(127);
      }
      result = nil_value();
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

      iv_set(mrb, io, intern_cstr(mrb, "@buf"), str_new_cstr(mrb, ""));

      fptr = io_alloc(mrb);
      fptr->fd = fd;
      fptr->fd2 = write_fd;
      fptr->pid = pid;
      fptr->readable = ((flags & FMODE_READABLE) != 0);
      fptr->writable = ((flags & FMODE_WRITABLE) != 0);
      fptr->sync = 0;

      DATA_TYPE(io) = &io_type;
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
      sys_fail(mrb, "pipe_open failed.");
      break;
  }
  return result;
}
#endif

static int
dup(state *mrb, int fd, bool *failed)
{
  int new_fd;

  *failed = TRUE;
  if (fd < 0)
    return fd;

  new_fd = dup(fd);
  if (new_fd > 0) *failed = FALSE;
  return new_fd;
}

value
io_initialize_copy(state *mrb, value copy)
{
  value orig;
  value buf;
  struct io *fptr_copy;
  struct io *fptr_orig;
  bool failed = TRUE;

  get_args(mrb, "o", &orig);
  fptr_orig = io_get_open_fptr(mrb, orig);
  fptr_copy = (struct io *)DATA_PTR(copy);
  if (fptr_orig == fptr_copy) return copy;
  if (fptr_copy != NULL) {
    fptr_finalize(mrb, fptr_copy, FALSE);
    free(mrb, fptr_copy);
  }
  fptr_copy = (struct io *)io_alloc(mrb);

  DATA_TYPE(copy) = &io_type;
  DATA_PTR(copy) = fptr_copy;

  buf = iv_get(mrb, orig, intern_cstr(mrb, "@buf"));
  iv_set(mrb, copy, intern_cstr(mrb, "@buf"), buf);

  fptr_copy->fd = dup(mrb, fptr_orig->fd, &failed);
  if (failed) {
    sys_fail(mrb, 0);
  }
  fd_cloexec(mrb, fptr_copy->fd);

  if (fptr_orig->fd2 != -1) {
    fptr_copy->fd2 = dup(mrb, fptr_orig->fd2, &failed);
    if (failed) {
      close(fptr_copy->fd);
      sys_fail(mrb, 0);
    }
    fd_cloexec(mrb, fptr_copy->fd2);
  }

  fptr_copy->pid = fptr_orig->pid;
  fptr_copy->readable = fptr_orig->readable;
  fptr_copy->writable = fptr_orig->writable;
  fptr_copy->sync = fptr_orig->sync;
  fptr_copy->is_socket = fptr_orig->is_socket;

  return copy;
}

value
io_initialize(state *mrb, value io)
{
  struct io *fptr;
  int fd;
  value mode, opt;
  int flags;

  mode = opt = nil_value();

  get_args(mrb, "i|So", &fd, &mode, &opt);
  if (nil_p(mode)) {
    mode = str_new_cstr(mrb, "r");
  }
  if (nil_p(opt)) {
    opt = hash_new(mrb);
  }

  flags = io_modestr_to_flags(mrb, string_value_cstr(mrb, &mode));

  iv_set(mrb, io, intern_cstr(mrb, "@buf"), str_new_cstr(mrb, ""));

  fptr = (struct io *)DATA_PTR(io);
  if (fptr != NULL) {
    fptr_finalize(mrb, fptr, TRUE);
    free(mrb, fptr);
  }
  fptr = io_alloc(mrb);

  DATA_TYPE(io) = &io_type;
  DATA_PTR(io) = fptr;

  fptr->fd = (int)fd;
  fptr->readable = ((flags & FMODE_READABLE) != 0);
  fptr->writable = ((flags & FMODE_WRITABLE) != 0);
  fptr->sync = 0;
  return io;
}

static void
fptr_finalize(state *mrb, struct io *fptr, int quiet)
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
    sys_fail(mrb, "fptr_finalize failed.");
  }
}

value
io_check_readable(state *mrb, value self)
{
  struct io *fptr = io_get_open_fptr(mrb, self);
  if (! fptr->readable) {
    raise(mrb, E_IO_ERROR, "not opened for reading");
  }
  return nil_value();
}

value
io_isatty(state *mrb, value self)
{
  struct io *fptr;

  fptr = io_get_open_fptr(mrb, self);
  if (isatty(fptr->fd) == 0)
    return false_value();
  return true_value();
}

value
io_s_for_fd(state *mrb, value klass)
{
  struct RClass *c = class_ptr(klass);
  enum vtype ttype = INSTANCE_TT(c);
  value obj;

  /* copied from instance_alloc() */
  if (ttype == 0) ttype = TT_OBJECT;
  obj = obj_value((struct RObject*)obj_alloc(mrb, ttype, c));
  return io_initialize(mrb, obj);
}

value
io_s_sysclose(state *mrb, value klass)
{
  int fd;
  get_args(mrb, "i", &fd);
  if (close((int)fd) == -1) {
    sys_fail(mrb, "close");
  }
  return fixnum_value(0);
}

int
cloexec_open(state *mrb, const char *pathname, int flags, int mode)
{
  value emsg;
  int fd, retry = FALSE;
  char* fname = locale_from_utf8(pathname, -1);

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
        garbage_collect(mrb);
        retry = TRUE;
        goto reopen;
      }
    }

    emsg = format(mrb, "open %S", str_new_cstr(mrb, pathname));
    str_modify(mrb, str_ptr(emsg));
    sys_fail(mrb, RSTRING_PTR(emsg));
  }
  locale_free(fname);

  if (fd <= 2) {
    fd_cloexec(mrb, fd);
  }
  return fd;
}

value
io_s_sysopen(state *mrb, value klass)
{
  value path = nil_value();
  value mode = nil_value();
  int fd, perm = -1;
  const char *pat;
  int flags, modenum;

  get_args(mrb, "S|Si", &path, &mode, &perm);
  if (nil_p(mode)) {
    mode = str_new_cstr(mrb, "r");
  }
  if (perm < 0) {
    perm = 0666;
  }

  pat = string_value_cstr(mrb, &path);
  flags = io_modestr_to_flags(mrb, string_value_cstr(mrb, &mode));
  modenum = io_flags_to_modenum(mrb, flags);
  fd = cloexec_open(mrb, pat, modenum, perm);
  return fixnum_value(fd);
}

value
io_sysread(state *mrb, value io)
{
  struct io *fptr;
  value buf = nil_value();
  int maxlen;
  int ret;

  get_args(mrb, "i|S", &maxlen, &buf);
  if (maxlen < 0) {
    raise(mrb, E_ARGUMENT_ERROR, "negative expanding string size");
  }
  else if (maxlen == 0) {
    return str_new(mrb, NULL, maxlen);
  }

  if (nil_p(buf)) {
    buf = str_new(mrb, NULL, maxlen);
  }

  if (RSTRING_LEN(buf) != maxlen) {
    buf = str_resize(mrb, buf, maxlen);
  } else {
    str_modify(mrb, RSTRING(buf));
  }

  fptr = (struct io *)io_get_open_fptr(mrb, io);
  if (!fptr->readable) {
    raise(mrb, E_IO_ERROR, "not opened for reading");
  }
  ret = read(fptr->fd, RSTRING_PTR(buf), (fsize_t)maxlen);
  switch (ret) {
    case 0: /* EOF */
      if (maxlen == 0) {
        buf = str_new_cstr(mrb, "");
      } else {
        raise(mrb, E_EOF_ERROR, "sysread failed: End of File");
      }
      break;
    case -1: /* Error */
      sys_fail(mrb, "sysread failed");
      break;
    default:
      if (RSTRING_LEN(buf) != ret) {
        buf = str_resize(mrb, buf, ret);
      }
      break;
  }

  return buf;
}

value
io_sysseek(state *mrb, value io)
{
  struct io *fptr;
  off_t pos;
  int offset, whence = -1;

  get_args(mrb, "i|i", &offset, &whence);
  if (whence < 0) {
    whence = 0;
  }

  fptr = io_get_open_fptr(mrb, io);
  pos = lseek(fptr->fd, (off_t)offset, (int)whence);
  if (pos == -1) {
    sys_fail(mrb, "sysseek");
  }
  if (pos > INT_MAX) {
#ifndef WITHOUT_FLOAT
    return float_value(mrb, (float)pos);
#else
    raise(mrb, E_IO_ERROR, "sysseek reached too far for WITHOUT_FLOAT");
#endif
  } else {
    return fixnum_value(pos);
  }
}

value
io_syswrite(state *mrb, value io)
{
  struct io *fptr;
  value str, buf;
  int fd, length;

  fptr = io_get_open_fptr(mrb, io);
  if (! fptr->writable) {
    raise(mrb, E_IO_ERROR, "not opened for writing");
  }

  get_args(mrb, "S", &str);
  if (type(str) != TT_STRING) {
    buf = funcall(mrb, str, "to_s", 0);
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
    sys_fail(mrb, 0);
  }

  return fixnum_value(length);
}

value
io_close(state *mrb, value self)
{
  struct io *fptr;
  fptr = io_get_open_fptr(mrb, self);
  fptr_finalize(mrb, fptr, FALSE);
  return nil_value();
}

value
io_close_write(state *mrb, value self)
{
  struct io *fptr;
  fptr = io_get_open_fptr(mrb, self);
  if (close((int)fptr->fd2) == -1) {
    sys_fail(mrb, "close");
  }
  return nil_value();
}

value
io_closed(state *mrb, value io)
{
  struct io *fptr;
  fptr = (struct io *)get_datatype(mrb, io, &io_type);
  if (fptr == NULL || fptr->fd >= 0) {
    return false_value();
  }

  return true_value();
}

value
io_pid(state *mrb, value io)
{
  struct io *fptr;
  fptr = io_get_open_fptr(mrb, io);

  if (fptr->pid > 0) {
    return fixnum_value(fptr->pid);
  }

  return nil_value();
}

static struct timeval
time2timeval(state *mrb, value time)
{
  struct timeval t = { 0, 0 };

  switch (type(time)) {
    case TT_FIXNUM:
      t.tv_sec = (ftime_t)fixnum(time);
      t.tv_usec = 0;
      break;

#ifndef WITHOUT_FLOAT
    case TT_FLOAT:
      t.tv_sec = (ftime_t)float(time);
      t.tv_usec = (fsuseconds_t)((float(time) - t.tv_sec) * 1000000.0);
      break;
#endif

    default:
      raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }

  return t;
}

static int
io_read_data_pending(state *mrb, value io)
{
  value buf = iv_get(mrb, io, intern_cstr(mrb, "@buf"));
  if (type(buf) == TT_STRING && RSTRING_LEN(buf) > 0) {
    return 1;
  }
  return 0;
}

#if !defined(_WIN32) && !TARGET_OS_IPHONE
static value
io_s_pipe(state *mrb, value klass)
{
  value r = nil_value();
  value w = nil_value();
  struct io *fptr_r;
  struct io *fptr_w;
  int pipes[2];

  if (pipe(mrb, pipes) == -1) {
    sys_fail(mrb, "pipe");
  }

  r = obj_value(data_object_alloc(mrb, class_ptr(klass), NULL, &io_type));
  iv_set(mrb, r, intern_cstr(mrb, "@buf"), str_new_cstr(mrb, ""));
  fptr_r = io_alloc(mrb);
  fptr_r->fd = pipes[0];
  fptr_r->readable = 1;
  fptr_r->writable = 0;
  fptr_r->sync = 0;
  DATA_TYPE(r) = &io_type;
  DATA_PTR(r)  = fptr_r;

  w = obj_value(data_object_alloc(mrb, class_ptr(klass), NULL, &io_type));
  iv_set(mrb, w, intern_cstr(mrb, "@buf"), str_new_cstr(mrb, ""));
  fptr_w = io_alloc(mrb);
  fptr_w->fd = pipes[1];
  fptr_w->readable = 0;
  fptr_w->writable = 1;
  fptr_w->sync = 1;
  DATA_TYPE(w) = &io_type;
  DATA_PTR(w)  = fptr_w;

  return assoc_new(mrb, r, w);
}
#endif

static value
io_s_select(state *mrb, value klass)
{
  value *argv;
  int argc;
  value read, read_io, write, except, timeout, list;
  struct timeval *tp, timerec;
  fd_set pset, rset, wset, eset;
  fd_set *rp, *wp, *ep;
  struct io *fptr;
  int pending = 0;
  value result;
  int max = 0;
  int interrupt_flag = 0;
  int i, n;

  get_args(mrb, "*", &argv, &argc);

  if (argc < 1 || argc > 4) {
    raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%S for 1..4)", fixnum_value(argc));
  }

  timeout = nil_value();
  except = nil_value();
  write = nil_value();
  if (argc > 3)
    timeout = argv[3];
  if (argc > 2)
    except = argv[2];
  if (argc > 1)
    write = argv[1];
  read = argv[0];

  if (nil_p(timeout)) {
    tp = NULL;
  } else {
    timerec = time2timeval(mrb, timeout);
    tp = &timerec;
  }

  FD_ZERO(&pset);
  if (!nil_p(read)) {
    check_type(mrb, read, TT_ARRAY);
    rp = &rset;
    FD_ZERO(rp);
    for (i = 0; i < RARRAY_LEN(read); i++) {
      read_io = RARRAY_PTR(read)[i];
      fptr = io_get_open_fptr(mrb, read_io);
      FD_SET(fptr->fd, rp);
      if (io_read_data_pending(mrb, read_io)) {
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

  if (!nil_p(write)) {
    check_type(mrb, write, TT_ARRAY);
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

  if (!nil_p(except)) {
    check_type(mrb, except, TT_ARRAY);
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
      sys_fail(mrb, "select failed");
    if (tp == NULL)
      goto retry;
    interrupt_flag = 1;
  }

  if (!pending && n == 0)
    return nil_value();

  result = ary_new_capa(mrb, 3);
  ary_push(mrb, result, rp? ary_new(mrb) : ary_new_capa(mrb, 0));
  ary_push(mrb, result, wp? ary_new(mrb) : ary_new_capa(mrb, 0));
  ary_push(mrb, result, ep? ary_new(mrb) : ary_new_capa(mrb, 0));

  if (interrupt_flag == 0) {
    if (rp) {
      list = RARRAY_PTR(result)[0];
      for (i = 0; i < RARRAY_LEN(read); i++) {
        fptr = io_get_open_fptr(mrb, RARRAY_PTR(read)[i]);
        if (FD_ISSET(fptr->fd, rp) ||
            FD_ISSET(fptr->fd, &pset)) {
          ary_push(mrb, list, RARRAY_PTR(read)[i]);
        }
      }
    }

    if (wp) {
      list = RARRAY_PTR(result)[1];
      for (i = 0; i < RARRAY_LEN(write); i++) {
        fptr = io_get_open_fptr(mrb, RARRAY_PTR(write)[i]);
        if (FD_ISSET(fptr->fd, wp)) {
          ary_push(mrb, list, RARRAY_PTR(write)[i]);
        } else if (fptr->fd2 >= 0 && FD_ISSET(fptr->fd2, wp)) {
          ary_push(mrb, list, RARRAY_PTR(write)[i]);
        }
      }
    }

    if (ep) {
      list = RARRAY_PTR(result)[2];
      for (i = 0; i < RARRAY_LEN(except); i++) {
        fptr = io_get_open_fptr(mrb, RARRAY_PTR(except)[i]);
        if (FD_ISSET(fptr->fd, ep)) {
          ary_push(mrb, list, RARRAY_PTR(except)[i]);
        } else if (fptr->fd2 >= 0 && FD_ISSET(fptr->fd2, ep)) {
          ary_push(mrb, list, RARRAY_PTR(except)[i]);
        }
      }
    }
  }

  return result;
}

value
io_fileno(state *mrb, value io)
{
  struct io *fptr;
  fptr = io_get_open_fptr(mrb, io);
  return fixnum_value(fptr->fd);
}

value
io_close_on_exec_p(state *mrb, value self)
{
#if defined(F_GETFD) && defined(F_SETFD) && defined(FD_CLOEXEC)
  struct io *fptr;
  int ret;

  fptr = io_get_open_fptr(mrb, self);

  if (fptr->fd2 >= 0) {
    if ((ret = fcntl(fptr->fd2, F_GETFD)) == -1) sys_fail(mrb, "F_GETFD failed");
    if (!(ret & FD_CLOEXEC)) return false_value();
  }

  if ((ret = fcntl(fptr->fd, F_GETFD)) == -1) sys_fail(mrb, "F_GETFD failed");
  if (!(ret & FD_CLOEXEC)) return false_value();
  return true_value();

#else
  raise(mrb, E_NOTIMP_ERROR, "IO#close_on_exec? is not supported on the platform");
  return false_value();
#endif
}

value
io_set_close_on_exec(state *mrb, value self)
{
#if defined(F_GETFD) && defined(F_SETFD) && defined(FD_CLOEXEC)
  struct io *fptr;
  int flag, ret;
  bool b;

  fptr = io_get_open_fptr(mrb, self);
  get_args(mrb, "b", &b);
  flag = b ? FD_CLOEXEC : 0;

  if (fptr->fd2 >= 0) {
    if ((ret = fcntl(fptr->fd2, F_GETFD)) == -1) sys_fail(mrb, "F_GETFD failed");
    if ((ret & FD_CLOEXEC) != flag) {
      ret = (ret & ~FD_CLOEXEC) | flag;
      ret = fcntl(fptr->fd2, F_SETFD, ret);

      if (ret == -1) sys_fail(mrb, "F_SETFD failed");
    }
  }

  if ((ret = fcntl(fptr->fd, F_GETFD)) == -1) sys_fail(mrb, "F_GETFD failed");
  if ((ret & FD_CLOEXEC) != flag) {
    ret = (ret & ~FD_CLOEXEC) | flag;
    ret = fcntl(fptr->fd, F_SETFD, ret);
    if (ret == -1) sys_fail(mrb, "F_SETFD failed");
  }

  return bool_value(b);
#else
  raise(mrb, E_NOTIMP_ERROR, "IO#close_on_exec= is not supported on the platform");
  return nil_value();
#endif
}

value
io_set_sync(state *mrb, value self)
{
  struct io *fptr;
  bool b;

  fptr = io_get_open_fptr(mrb, self);
  get_args(mrb, "b", &b);
  fptr->sync = b;
  return bool_value(b);
}

value
io_sync(state *mrb, value self)
{
  struct io *fptr;
  fptr = io_get_open_fptr(mrb, self);
  return bool_value(fptr->sync);
}

void
init_io(state *mrb)
{
  struct RClass *io;

  io      = define_class(mrb, "IO", mrb->object_class);
  SET_INSTANCE_TT(io, TT_DATA);

  include_module(mrb, io, module_get(mrb, "Enumerable")); /* 15.2.20.3 */
  define_class_method(mrb, io, "_popen",  io_s_popen,   ARGS_ANY());
  define_class_method(mrb, io, "_sysclose",  io_s_sysclose, ARGS_REQ(1));
  define_class_method(mrb, io, "for_fd",  io_s_for_fd,   ARGS_ANY());
  define_class_method(mrb, io, "select",  io_s_select,  ARGS_ANY());
  define_class_method(mrb, io, "sysopen", io_s_sysopen, ARGS_ANY());
#if !defined(_WIN32) && !TARGET_OS_IPHONE
  define_class_method(mrb, io, "_pipe", io_s_pipe, ARGS_NONE());
#endif

  define_method(mrb, io, "initialize", io_initialize, ARGS_ANY());    /* 15.2.20.5.21 (x)*/
  define_method(mrb, io, "initialize_copy", io_initialize_copy, ARGS_REQ(1));
  define_method(mrb, io, "_check_readable", io_check_readable, ARGS_NONE());
  define_method(mrb, io, "isatty",     io_isatty,     ARGS_NONE());
  define_method(mrb, io, "sync",       io_sync,       ARGS_NONE());
  define_method(mrb, io, "sync=",      io_set_sync,   ARGS_REQ(1));
  define_method(mrb, io, "sysread",    io_sysread,    ARGS_ANY());
  define_method(mrb, io, "sysseek",    io_sysseek,    ARGS_REQ(1));
  define_method(mrb, io, "syswrite",   io_syswrite,   ARGS_REQ(1));
  define_method(mrb, io, "close",      io_close,      ARGS_NONE());   /* 15.2.20.5.1 */
  define_method(mrb, io, "close_write",    io_close_write,       ARGS_NONE());
  define_method(mrb, io, "close_on_exec=", io_set_close_on_exec, ARGS_REQ(1));
  define_method(mrb, io, "close_on_exec?", io_close_on_exec_p,   ARGS_NONE());
  define_method(mrb, io, "closed?",    io_closed,     ARGS_NONE());   /* 15.2.20.5.2 */
  define_method(mrb, io, "pid",        io_pid,        ARGS_NONE());   /* 15.2.20.5.2 */
  define_method(mrb, io, "fileno",     io_fileno,     ARGS_NONE());


  gv_set(mrb, intern_cstr(mrb, "$/"), str_new_cstr(mrb, "\n"));
}
