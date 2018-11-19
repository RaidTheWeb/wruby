/*
** io.h - IO class
*/

#ifndef MRUBY_IO_H
#define MRUBY_IO_H

#if defined(__cplusplus)
extern "C" {
#endif

struct _io {
  int fd;   /* file descriptor, or -1 */
  int fd2;  /* file descriptor to write if it's different from fd, or -1 */
  int pid;  /* child's pid (for pipes)  */
  unsigned int readable:1,
               writable:1,
               sync:1,
               is_socket:1;
};

#define FMODE_READABLE             0x00000001
#define FMODE_WRITABLE             0x00000002
#define FMODE_READWRITE            (FMODE_READABLE|FMODE_WRITABLE)
#define FMODE_BINMODE              0x00000004
#define FMODE_APPEND               0x00000040
#define FMODE_CREATE               0x00000080
#define FMODE_TRUNC                0x00000800

#define E_IO_ERROR                 (_class_get(mrb, "IOError"))
#define E_EOF_ERROR                (_class_get(mrb, "EOFError"))

value _io_fileno(state *mrb, value io);

#if defined(__cplusplus)
} /* extern "C" { */
#endif
#endif /* MRUBY_IO_H */
