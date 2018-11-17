/*
** socket.c - Socket module
**
** See Copyright Notice in mruby.h
*/

#ifdef _WIN32
  #define _WIN32_WINNT 0x0501

  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>

  #define SHUT_RDWR SD_BOTH
  #ifndef _SSIZE_T_DEFINED
  typedef int ssize_t;
  #endif
  typedef int fsize_t;
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/param.h>
  #include <sys/un.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <netdb.h>
  #include <unistd.h>
  typedef size_t fsize_t;
#endif

#include <stddef.h>
#include <string.h>

#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "error.h"

#include "mruby/ext/io.h"

#if !defined(HAVE_SA_LEN)
#if (defined(BSD) && (BSD >= 199006))
#define HAVE_SA_LEN  1
#else
#define HAVE_SA_LEN  0
#endif
#endif

#define E_SOCKET_ERROR             (class_get(mrb, "SocketError"))

#if !defined(cptr)
#define cptr_value(m,p) voidp_value((m),(p))
#define cptr(o) voidp(o)
#define cptr_p(o) voidp_p(o)
#endif

#ifdef _WIN32
static const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{
    if (af == AF_INET)
    {
	struct sockaddr_in in;
	memset(&in, 0, sizeof(in));
	in.sin_family = AF_INET;
	memcpy(&in.sin_addr, src, sizeof(struct in_addr));
	getnameinfo((struct sockaddr *)&in, sizeof(struct
		    sockaddr_in), dst, cnt, NULL, 0, NI_NUMERICHOST);
	return dst;
    }
    else if (af == AF_INET6)
    {
	struct sockaddr_in6 in;
	memset(&in, 0, sizeof(in));
	in.sin6_family = AF_INET6;
	memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
	getnameinfo((struct sockaddr *)&in, sizeof(struct
		    sockaddr_in6), dst, cnt, NULL, 0, NI_NUMERICHOST);
	return dst;
    }
    return NULL;
}

static int inet_pton(int af, const char *src, void *dst)
{
    struct addrinfo hints, *res, *ressave;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = af;

    if (getaddrinfo(src, NULL, &hints, &res) != 0)
    {
	printf("Couldn't resolve host %s\n", src);
	return -1;
    }

    ressave = res;

    while (res)
    {
	memcpy(dst, res->ai_addr, res->ai_addrlen);
	res = res->ai_next;
    }

    freeaddrinfo(ressave);
    return 0;
}

#endif

static value
addrinfo_getaddrinfo(state *mrb, value klass)
{
  struct addrinfo hints, *res0, *res;
  value ai, ary, family, lastai, nodename, protocol, sa, service, socktype;
  int flags;
  int arena_idx, error;
  const char *hostname = NULL, *servname = NULL;

  ary = ary_new(mrb);
  arena_idx = gc_arena_save(mrb);  /* ary must be on arena! */

  family = socktype = protocol = nil_value();
  flags = 0;
  get_args(mrb, "oo|oooi", &nodename, &service, &family, &socktype, &protocol, &flags);

  if (string_p(nodename)) {
    hostname = str_to_cstr(mrb, nodename);
  } else if (nil_p(nodename)) {
    hostname = NULL;
  } else {
    raise(mrb, E_TYPE_ERROR, "nodename must be String or nil");
  }

  if (string_p(service)) {
    servname = str_to_cstr(mrb, service);
  } else if (fixnum_p(service)) {
    servname = str_to_cstr(mrb, funcall(mrb, service, "to_s", 0));
  } else if (nil_p(service)) {
    servname = NULL;
  } else {
    raise(mrb, E_TYPE_ERROR, "service must be String, Fixnum, or nil");
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = (int)flags;

  if (fixnum_p(family)) {
    hints.ai_family = (int)fixnum(family);
  }

  if (fixnum_p(socktype)) {
    hints.ai_socktype = (int)fixnum(socktype);
  }

  if (fixnum_p(protocol)) {
    hints.ai_protocol = (int)fixnum(protocol);
  }

  lastai = cv_get(mrb, klass, intern_lit(mrb, "_lastai"));
  if (cptr_p(lastai)) {
    freeaddrinfo((struct addrinfo*)cptr(lastai));
    cv_set(mrb, klass, intern_lit(mrb, "_lastai"), nil_value());
  }

  error = getaddrinfo(hostname, servname, &hints, &res0);
  if (error) {
    raisef(mrb, E_SOCKET_ERROR, "getaddrinfo: %S", str_new_cstr(mrb, gai_strerror(error)));
  }
  cv_set(mrb, klass, intern_lit(mrb, "_lastai"), cptr_value(mrb, res0));

  for (res = res0; res != NULL; res = res->ai_next) {
    sa = str_new(mrb, (char*)res->ai_addr, res->ai_addrlen);
    ai = funcall(mrb, klass, "new", 4, sa, fixnum_value(res->ai_family), fixnum_value(res->ai_socktype), fixnum_value(res->ai_protocol));
    ary_push(mrb, ary, ai);
    gc_arena_restore(mrb, arena_idx);
  }

  freeaddrinfo(res0);
  cv_set(mrb, klass, intern_lit(mrb, "_lastai"), nil_value());

  return ary;
}

static value
addrinfo_getnameinfo(state *mrb, value self)
{
  int flags;
  value ary, host, sastr, serv;
  int error;

  flags = 0;
  get_args(mrb, "|i", &flags);
  host = str_buf_new(mrb, NI_MAXHOST);
  serv = str_buf_new(mrb, NI_MAXSERV);

  sastr = iv_get(mrb, self, intern_lit(mrb, "@sockaddr"));
  if (!string_p(sastr)) {
    raise(mrb, E_SOCKET_ERROR, "invalid sockaddr");
  }
  error = getnameinfo((struct sockaddr *)RSTRING_PTR(sastr), (socklen_t)RSTRING_LEN(sastr), RSTRING_PTR(host), NI_MAXHOST, RSTRING_PTR(serv), NI_MAXSERV, (int)flags);
  if (error) {
    raisef(mrb, E_SOCKET_ERROR, "getnameinfo: %S", str_new_cstr(mrb, gai_strerror(error)));
  }
  ary = ary_new_capa(mrb, 2);
  str_resize(mrb, host, strlen(RSTRING_PTR(host)));
  ary_push(mrb, ary, host);
  str_resize(mrb, serv, strlen(RSTRING_PTR(serv)));
  ary_push(mrb, ary, serv);
  return ary;
}

#ifndef _WIN32
static value
addrinfo_unix_path(state *mrb, value self)
{
  value sastr;

  sastr = iv_get(mrb, self, intern_lit(mrb, "@sockaddr"));
  if (((struct sockaddr *)RSTRING_PTR(sastr))->sa_family != AF_UNIX)
    raise(mrb, E_SOCKET_ERROR, "need AF_UNIX address");
  if (RSTRING_LEN(sastr) < (int)offsetof(struct sockaddr_un, sun_path) + 1) {
    return str_new(mrb, "", 0);
  } else {
    return str_new_cstr(mrb, ((struct sockaddr_un *)RSTRING_PTR(sastr))->sun_path);
  }
}
#endif

static value
sa2addrlist(state *mrb, const struct sockaddr *sa, socklen_t salen)
{
  value ary, host;
  unsigned short port;
  const char *afstr;

  switch (sa->sa_family) {
  case AF_INET:
    afstr = "AF_INET";
    port = ((struct sockaddr_in *)sa)->sin_port;
    break;
  case AF_INET6:
    afstr = "AF_INET6";
    port = ((struct sockaddr_in6 *)sa)->sin6_port;
    break;
  default:
    raise(mrb, E_ARGUMENT_ERROR, "bad af");
    return nil_value();
  }
  port = ntohs(port);
  host = str_buf_new(mrb, NI_MAXHOST);
  if (getnameinfo(sa, salen, RSTRING_PTR(host), NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == -1)
    sys_fail(mrb, "getnameinfo");
  str_resize(mrb, host, strlen(RSTRING_PTR(host)));
  ary = ary_new_capa(mrb, 4);
  ary_push(mrb, ary, str_new_cstr(mrb, afstr));
  ary_push(mrb, ary, fixnum_value(port));
  ary_push(mrb, ary, host);
  ary_push(mrb, ary, host);
  return ary;
}

static int
socket_fd(state *mrb, value sock)
{
  return (int)fixnum(funcall(mrb, sock, "fileno", 0));
}

static int
socket_family(int s)
{
  struct sockaddr_storage ss;
  socklen_t salen;

  salen = sizeof(ss);
  if (getsockname(s, (struct sockaddr *)&ss, &salen) == -1)
    return AF_UNSPEC;
  return ss.ss_family;
}

static value
basicsocket_getpeereid(state *mrb, value self)
{
#ifdef HAVE_GETPEEREID
  value ary;
  gid_t egid;
  uid_t euid;
  int s;

  s = socket_fd(mrb, self);
  if (getpeereid(s, &euid, &egid) != 0)
    sys_fail(mrb, "getpeereid");

  ary = ary_new_capa(mrb, 2);
  ary_push(mrb, ary, fixnum_value((int)euid));
  ary_push(mrb, ary, fixnum_value((int)egid));
  return ary;
#else
  raise(mrb, E_RUNTIME_ERROR, "getpeereid is not available on this system");
  return nil_value();
#endif
}

static value
basicsocket_getpeername(state *mrb, value self)
{
  struct sockaddr_storage ss;
  socklen_t salen;

  salen = sizeof(ss);
  if (getpeername(socket_fd(mrb, self), (struct sockaddr *)&ss, &salen) != 0)
    sys_fail(mrb, "getpeername");

  return str_new(mrb, (char*)&ss, salen);
}

static value
basicsocket_getsockname(state *mrb, value self)
{
  struct sockaddr_storage ss;
  socklen_t salen;

  salen = sizeof(ss);
  if (getsockname(socket_fd(mrb, self), (struct sockaddr *)&ss, &salen) != 0)
    sys_fail(mrb, "getsockname");

  return str_new(mrb, (char*)&ss, salen);
}

static value
basicsocket_getsockopt(state *mrb, value self)
{
  char opt[8];
  int s;
  int family, level, optname;
  value c, data;
  socklen_t optlen;

  get_args(mrb, "ii", &level, &optname);
  s = socket_fd(mrb, self);
  optlen = sizeof(opt);
  if (getsockopt(s, (int)level, (int)optname, opt, &optlen) == -1)
    sys_fail(mrb, "getsockopt");
  c = const_get(mrb, obj_value(class_get(mrb, "Socket")), intern_lit(mrb, "Option"));
  family = socket_family(s);
  data = str_new(mrb, opt, optlen);
  return funcall(mrb, c, "new", 4, fixnum_value(family), fixnum_value(level), fixnum_value(optname), data);
}

static value
basicsocket_recv(state *mrb, value self)
{
  ssize_t n;
  int maxlen, flags = 0;
  value buf;

  get_args(mrb, "i|i", &maxlen, &flags);
  buf = str_buf_new(mrb, maxlen);
  n = recv(socket_fd(mrb, self), RSTRING_PTR(buf), (fsize_t)maxlen, (int)flags);
  if (n == -1)
    sys_fail(mrb, "recv");
  str_resize(mrb, buf, (int)n);
  return buf;
}

static value
basicsocket_recvfrom(state *mrb, value self)
{
  ssize_t n;
  int maxlen, flags = 0;
  value ary, buf, sa;
  socklen_t socklen;

  get_args(mrb, "i|i", &maxlen, &flags);
  buf = str_buf_new(mrb, maxlen);
  socklen = sizeof(struct sockaddr_storage);
  sa = str_buf_new(mrb, socklen);
  n = recvfrom(socket_fd(mrb, self), RSTRING_PTR(buf), (fsize_t)maxlen, (int)flags, (struct sockaddr *)RSTRING_PTR(sa), &socklen);
  if (n == -1)
    sys_fail(mrb, "recvfrom");
  str_resize(mrb, buf, (int)n);
  str_resize(mrb, sa, (int)socklen);
  ary = ary_new_capa(mrb, 2);
  ary_push(mrb, ary, buf);
  ary_push(mrb, ary, sa);
  return ary;
}

static value
basicsocket_send(state *mrb, value self)
{
  ssize_t n;
  int flags;
  value dest, mesg;

  dest = nil_value();
  get_args(mrb, "Si|S", &mesg, &flags, &dest);
  if (nil_p(dest)) {
    n = send(socket_fd(mrb, self), RSTRING_PTR(mesg), (fsize_t)RSTRING_LEN(mesg), (int)flags);
  } else {
    n = sendto(socket_fd(mrb, self), RSTRING_PTR(mesg), (fsize_t)RSTRING_LEN(mesg), (int)flags, (const struct sockaddr*)RSTRING_PTR(dest), (fsize_t)RSTRING_LEN(dest));
  }
  if (n == -1)
    sys_fail(mrb, "send");
  return fixnum_value((int)n);
}

static value
basicsocket_setnonblock(state *mrb, value self)
{
  int fd, flags;
  bool nonblocking;
#ifdef _WIN32
  u_long mode = 1;
#endif

  get_args(mrb, "b", &nonblocking);
  fd = socket_fd(mrb, self);
#ifdef _WIN32
  flags = ioctlsocket(fd, FIONBIO, &mode);
  if (flags != NO_ERROR)
    sys_fail(mrb, "ioctlsocket");
#else
  flags = fcntl(fd, F_GETFL, 0);
  if (flags == 1)
    sys_fail(mrb, "fcntl");
  if (nonblocking)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) == -1)
    sys_fail(mrb, "fcntl");
#endif
  return nil_value();
}

static value
basicsocket_setsockopt(state *mrb, value self)
{
  int s;
  int argc, level = 0, optname;
  value optval, so;

  argc = get_args(mrb, "o|io", &so, &optname, &optval);
  if (argc == 3) {
    if (!fixnum_p(so)) {
      raise(mrb, E_ARGUMENT_ERROR, "level is not an integer");
    }
    level = fixnum(so);
    if (string_p(optval)) {
      /* that's good */
    } else if (type(optval) == TT_TRUE || type(optval) == TT_FALSE) {
      int i = test(optval) ? 1 : 0;
      optval = str_new(mrb, (char*)&i, sizeof(i));
    } else if (fixnum_p(optval)) {
      if (optname == IP_MULTICAST_TTL || optname == IP_MULTICAST_LOOP) {
        char uc = (char)fixnum(optval);
        optval = str_new(mrb, &uc, sizeof(uc));
      } else {
        int i = fixnum(optval);
        optval = str_new(mrb, (char*)&i, sizeof(i));
      }
    } else {
      raise(mrb, E_ARGUMENT_ERROR, "optval should be true, false, an integer, or a string");
    }
  } else if (argc == 1) {
    if (strcmp(obj_classname(mrb, so), "Socket::Option") != 0)
      raise(mrb, E_ARGUMENT_ERROR, "not an instance of Socket::Option");
    level = fixnum(funcall(mrb, so, "level", 0));
    optname = fixnum(funcall(mrb, so, "optname", 0));
    optval = funcall(mrb, so, "data", 0);
  } else {
    raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%S for 3)", fixnum_value(argc));
  }

  s = socket_fd(mrb, self);
  if (setsockopt(s, (int)level, (int)optname, RSTRING_PTR(optval), (socklen_t)RSTRING_LEN(optval)) == -1)
    sys_fail(mrb, "setsockopt");
  return fixnum_value(0);
}

static value
basicsocket_shutdown(state *mrb, value self)
{
  int how = SHUT_RDWR;

  get_args(mrb, "|i", &how);
  if (shutdown(socket_fd(mrb, self), (int)how) != 0)
    sys_fail(mrb, "shutdown");
  return fixnum_value(0);
}

static value
basicsocket_set_is_socket(state *mrb, value self)
{
  bool b;
  struct io *io_p;
  get_args(mrb, "b", &b);

  io_p = (struct io*)DATA_PTR(self);
  if (io_p) {
    io_p->is_socket = b;
  }

  return bool_value(b);
}

static value
ipsocket_ntop(state *mrb, value klass)
{
  int af, n;
  char *addr, buf[50];

  get_args(mrb, "is", &af, &addr, &n);
  if ((af == AF_INET && n != 4) || (af == AF_INET6 && n != 16))
    raise(mrb, E_ARGUMENT_ERROR, "invalid address");
  if (inet_ntop((int)af, addr, buf, sizeof(buf)) == NULL)
    raise(mrb, E_ARGUMENT_ERROR, "invalid address");
  return str_new_cstr(mrb, buf);
}

static value
ipsocket_pton(state *mrb, value klass)
{
  int af, n;
  char *bp, buf[50];

  get_args(mrb, "is", &af, &bp, &n);
  if ((size_t)n > sizeof(buf) - 1)
    raise(mrb, E_ARGUMENT_ERROR, "invalid address");
  memcpy(buf, bp, n);
  buf[n] = '\0';

  if (af == AF_INET) {
    struct in_addr in;
    if (inet_pton(AF_INET, buf, (void *)&in.s_addr) != 1)
      goto invalid;
    return str_new(mrb, (char*)&in.s_addr, 4);
  } else if (af == AF_INET6) {
    struct in6_addr in6;
    if (inet_pton(AF_INET6, buf, (void *)&in6.s6_addr) != 1)
      goto invalid;
    return str_new(mrb, (char*)&in6.s6_addr, 16);
  } else
    raise(mrb, E_ARGUMENT_ERROR, "unsupported address family");

invalid:
  raise(mrb, E_ARGUMENT_ERROR, "invalid address");
  return nil_value(); /* dummy */
}

static value
ipsocket_recvfrom(state *mrb, value self)
{
  struct sockaddr_storage ss;
  socklen_t socklen;
  value a, buf, pair;
  int flags, maxlen;
  ssize_t n;
  int fd;

  fd = socket_fd(mrb, self);
  flags = 0;
  get_args(mrb, "i|i", &maxlen, &flags);
  buf = str_buf_new(mrb, maxlen);
  socklen = sizeof(ss);
  n = recvfrom(fd, RSTRING_PTR(buf), (fsize_t)maxlen, (int)flags,
  	       (struct sockaddr *)&ss, &socklen);
  if (n == -1) {
    sys_fail(mrb, "recvfrom");
  }
  str_resize(mrb, buf, (int)n);
  a = sa2addrlist(mrb, (struct sockaddr *)&ss, socklen);
  pair = ary_new_capa(mrb, 2);
  ary_push(mrb, pair, buf);
  ary_push(mrb, pair, a);
  return pair;
}

static value
socket_gethostname(state *mrb, value cls)
{
  value buf;
  size_t bufsize;

#ifdef HOST_NAME_MAX
  bufsize = HOST_NAME_MAX + 1;
#else
  bufsize = 256;
#endif
  buf = str_buf_new(mrb, (int)bufsize);
  if (gethostname(RSTRING_PTR(buf), (fsize_t)bufsize) != 0)
    sys_fail(mrb, "gethostname");
  str_resize(mrb, buf, (int)strlen(RSTRING_PTR(buf)));
  return buf;
}

static value
socket_accept(state *mrb, value klass)
{
  int s1;
  int s0;

  get_args(mrb, "i", &s0);
  s1 = (int)accept(s0, NULL, NULL);
  if (s1 == -1) {
    sys_fail(mrb, "accept");
  }
  return fixnum_value(s1);
}

static value
socket_accept2(state *mrb, value klass)
{
  value ary, sastr;
  int s1;
  int s0;
  socklen_t socklen;

  get_args(mrb, "i", &s0);
  socklen = sizeof(struct sockaddr_storage);
  sastr = str_buf_new(mrb, socklen);
  s1 = (int)accept(s0, (struct sockaddr *)RSTRING_PTR(sastr), &socklen);
  if (s1 == -1) {
    sys_fail(mrb, "accept");
  }
  // XXX: possible descriptor leakage here!
  str_resize(mrb, sastr, socklen);
  ary = ary_new_capa(mrb, 2);
  ary_push(mrb, ary, fixnum_value(s1));
  ary_push(mrb, ary, sastr);
  return ary;
}

static value
socket_bind(state *mrb, value klass)
{
  value sastr;
  int s;

  get_args(mrb, "iS", &s, &sastr);
  if (bind((int)s, (struct sockaddr *)RSTRING_PTR(sastr), (socklen_t)RSTRING_LEN(sastr)) == -1) {
    sys_fail(mrb, "bind");
  }
  return nil_value();
}

static value
socket_connect(state *mrb, value klass)
{
  value sastr;
  int s;

  get_args(mrb, "iS", &s, &sastr);
  if (connect((int)s, (struct sockaddr *)RSTRING_PTR(sastr), (socklen_t)RSTRING_LEN(sastr)) == -1) {
    sys_fail(mrb, "connect");
  }
  return nil_value();
}

static value
socket_listen(state *mrb, value klass)
{
  int backlog, s;

  get_args(mrb, "ii", &s, &backlog);
  if (listen((int)s, (int)backlog) == -1) {
    sys_fail(mrb, "listen");
  }
  return nil_value();
}

static value
socket_sockaddr_family(state *mrb, value klass)
{
  const struct sockaddr *sa;
  value str;

  get_args(mrb, "S", &str);
  if ((size_t)RSTRING_LEN(str) < offsetof(struct sockaddr, sa_family) + sizeof(sa->sa_family)) {
    raise(mrb, E_SOCKET_ERROR, "invalid sockaddr (too short)");
  }
  sa = (const struct sockaddr *)RSTRING_PTR(str);
  return fixnum_value(sa->sa_family);
}

static value
socket_sockaddr_un(state *mrb, value klass)
{
#ifdef _WIN32
  raise(mrb, E_NOTIMP_ERROR, "sockaddr_un unsupported on Windows");
  return nil_value();
#else
  struct sockaddr_un *sunp;
  value path, s;

  get_args(mrb, "S", &path);
  if ((size_t)RSTRING_LEN(path) > sizeof(sunp->sun_path) - 1) {
    raisef(mrb, E_ARGUMENT_ERROR, "too long unix socket path (max: %S bytes)", fixnum_value(sizeof(sunp->sun_path) - 1));
  }
  s = str_buf_new(mrb, sizeof(struct sockaddr_un));
  sunp = (struct sockaddr_un *)RSTRING_PTR(s);
#if HAVE_SA_LEN
  sunp->sun_len = sizeof(struct sockaddr_un);
#endif
  sunp->sun_family = AF_UNIX;
  memcpy(sunp->sun_path, RSTRING_PTR(path), RSTRING_LEN(path));
  sunp->sun_path[RSTRING_LEN(path)] = '\0';
  str_resize(mrb, s, sizeof(struct sockaddr_un));
  return s;
#endif
}

static value
socket_socketpair(state *mrb, value klass)
{
#ifdef _WIN32
  raise(mrb, E_NOTIMP_ERROR, "socketpair unsupported on Windows");
  return nil_value();
#else
  value ary;
  int domain, type, protocol;
  int sv[2];

  get_args(mrb, "iii", &domain, &type, &protocol);
  if (socketpair(domain, type, protocol, sv) == -1) {
    sys_fail(mrb, "socketpair");
  }
  // XXX: possible descriptor leakage here!
  ary = ary_new_capa(mrb, 2);
  ary_push(mrb, ary, fixnum_value(sv[0]));
  ary_push(mrb, ary, fixnum_value(sv[1]));
  return ary;
#endif
}

static value
socket_socket(state *mrb, value klass)
{
  int domain, type, protocol;
  int s;

  get_args(mrb, "iii", &domain, &type, &protocol);
  s = (int)socket((int)domain, (int)type, (int)protocol);
  if (s == -1)
    sys_fail(mrb, "socket");
  return fixnum_value(s);
}

static value
tcpsocket_allocate(state *mrb, value klass)
{
  struct RClass *c = class_ptr(klass);
  enum vtype ttype = INSTANCE_TT(c);

  /* copied from instance_alloc() */
  if (ttype == 0) ttype = TT_OBJECT;
  return obj_value((struct RObject*)obj_alloc(mrb, ttype, c));
}

/* Windows overrides for IO methods on BasicSocket objects.
 * This is because sockets on Windows are not the same as file
 * descriptors, and thus functions which operate on file descriptors
 * will break on socket descriptors.
 */
#ifdef _WIN32
static value
win32_basicsocket_close(state *mrb, value self)
{
  if (closesocket(socket_fd(mrb, self)) != NO_ERROR)
    raise(mrb, E_SOCKET_ERROR, "closesocket unsuccessful");
  return nil_value();
}

#define E_EOF_ERROR                (class_get(mrb, "EOFError"))
static value
win32_basicsocket_sysread(state *mrb, value self)
{
  int sd, ret;
  value buf = nil_value();
  int maxlen;

  get_args(mrb, "i|S", &maxlen, &buf);
  if (maxlen < 0) {
    return nil_value();
  }

  if (nil_p(buf)) {
    buf = str_new(mrb, NULL, maxlen);
  }
  if (RSTRING_LEN(buf) != maxlen) {
    buf = str_resize(mrb, buf, maxlen);
  }

  sd = socket_fd(mrb, self);
  ret = recv(sd, RSTRING_PTR(buf), (int)maxlen, 0);

  switch (ret) {
    case 0: /* EOF */
      if (maxlen == 0) {
        buf = str_new_cstr(mrb, "");
      } else {
        raise(mrb, E_EOF_ERROR, "sysread failed: End of File");
      }
      break;
    case SOCKET_ERROR: /* Error */
      sys_fail(mrb, "recv");
      break;
    default:
      if (RSTRING_LEN(buf) != ret) {
        buf = str_resize(mrb, buf, ret);
      }
      break;
  }

  return buf;
}

static value
win32_basicsocket_sysseek(state *mrb, value self)
{
  raise(mrb, E_NOTIMP_ERROR, "sysseek not implemented for windows sockets");
  return nil_value();
}

static value
win32_basicsocket_syswrite(state *mrb, value self)
{
  int n;
  SOCKET sd;
  value str;

  sd = socket_fd(mrb, self);
  get_args(mrb, "S", &str);
  n = send(sd, RSTRING_PTR(str), (int)RSTRING_LEN(str), 0);
  if (n == SOCKET_ERROR)
    sys_fail(mrb, "send");
  return fixnum_value(n);
}

#endif

void
mruby_socket_gem_init(state* mrb)
{
  struct RClass *io, *ai, *sock, *bsock, *ipsock, *tcpsock;
  struct RClass *constants;

#ifdef _WIN32
  WSADATA wsaData;
  int result;
  result = WSAStartup(MAKEWORD(2,2), &wsaData);
  if (result != NO_ERROR)
    raise(mrb, E_RUNTIME_ERROR, "WSAStartup failed");
#endif

  ai = define_class(mrb, "Addrinfo", mrb->object_class);
  mod_cv_set(mrb, ai, intern_lit(mrb, "_lastai"), nil_value());
  define_class_method(mrb, ai, "getaddrinfo", addrinfo_getaddrinfo, ARGS_REQ(2)|ARGS_OPT(4));
  define_method(mrb, ai, "getnameinfo", addrinfo_getnameinfo, ARGS_OPT(1));
#ifndef _WIN32
  define_method(mrb, ai, "unix_path", addrinfo_unix_path, ARGS_NONE());
#endif

  io = class_get(mrb, "IO");

  bsock = define_class(mrb, "BasicSocket", io);
  define_method(mrb, bsock, "_recvfrom", basicsocket_recvfrom, ARGS_REQ(1)|ARGS_OPT(1));
  define_method(mrb, bsock, "_setnonblock", basicsocket_setnonblock, ARGS_REQ(1));
  define_method(mrb, bsock, "getpeereid", basicsocket_getpeereid, ARGS_NONE());
  define_method(mrb, bsock, "getpeername", basicsocket_getpeername, ARGS_NONE());
  define_method(mrb, bsock, "getsockname", basicsocket_getsockname, ARGS_NONE());
  define_method(mrb, bsock, "getsockopt", basicsocket_getsockopt, ARGS_REQ(2));
  define_method(mrb, bsock, "recv", basicsocket_recv, ARGS_REQ(1)|ARGS_OPT(1));
  // #recvmsg(maxlen, flags=0)
  define_method(mrb, bsock, "send", basicsocket_send, ARGS_REQ(2)|ARGS_OPT(1));
  // #sendmsg
  // #sendmsg_nonblock
  define_method(mrb, bsock, "setsockopt", basicsocket_setsockopt, ARGS_REQ(1)|ARGS_OPT(2));
  define_method(mrb, bsock, "shutdown", basicsocket_shutdown, ARGS_OPT(1));
  define_method(mrb, bsock, "_is_socket=", basicsocket_set_is_socket, ARGS_REQ(1));

  ipsock = define_class(mrb, "IPSocket", bsock);
  define_class_method(mrb, ipsock, "ntop", ipsocket_ntop, ARGS_REQ(1));
  define_class_method(mrb, ipsock, "pton", ipsocket_pton, ARGS_REQ(2));
  define_method(mrb, ipsock, "recvfrom", ipsocket_recvfrom, ARGS_REQ(1)|ARGS_OPT(1));

  tcpsock = define_class(mrb, "TCPSocket", ipsock);
  define_class_method(mrb, tcpsock, "_allocate", tcpsocket_allocate, ARGS_NONE());
  define_class(mrb, "TCPServer", tcpsock);

  define_class(mrb, "UDPSocket", ipsock);
  //#recvfrom_nonblock

  sock = define_class(mrb, "Socket", bsock);
  define_class_method(mrb, sock, "_accept", socket_accept, ARGS_REQ(1));
  define_class_method(mrb, sock, "_accept2", socket_accept2, ARGS_REQ(1));
  define_class_method(mrb, sock, "_bind", socket_bind, ARGS_REQ(3));
  define_class_method(mrb, sock, "_connect", socket_connect, ARGS_REQ(3));
  define_class_method(mrb, sock, "_listen", socket_listen, ARGS_REQ(2));
  define_class_method(mrb, sock, "_sockaddr_family", socket_sockaddr_family, ARGS_REQ(1));
  define_class_method(mrb, sock, "_socket", socket_socket, ARGS_REQ(3));
  //define_class_method(mrb, sock, "gethostbyaddr", socket_gethostbyaddr, ARGS_REQ(1)|ARGS_OPT(1));
  //define_class_method(mrb, sock, "gethostbyname", socket_gethostbyname, ARGS_REQ(1)|ARGS_OPT(1));
  define_class_method(mrb, sock, "gethostname", socket_gethostname, ARGS_NONE());
  //define_class_method(mrb, sock, "getservbyname", socket_getservbyname, ARGS_REQ(1)|ARGS_OPT(1));
  //define_class_method(mrb, sock, "getservbyport", socket_getservbyport, ARGS_REQ(1)|ARGS_OPT(1));
  define_class_method(mrb, sock, "sockaddr_un", socket_sockaddr_un, ARGS_REQ(1));
  define_class_method(mrb, sock, "socketpair", socket_socketpair, ARGS_REQ(3));
  //define_method(mrb, sock, "sysaccept", socket_accept, ARGS_NONE());

#ifndef _WIN32
  define_class(mrb, "UNIXSocket", bsock);
  //define_class_method(mrb, usock, "pair", unixsocket_open, ARGS_OPT(2));
  //define_class_method(mrb, usock, "socketpair", unixsocket_open, ARGS_OPT(2));

  //define_method(mrb, usock, "recv_io", unixsocket_peeraddr, ARGS_NONE());
  //define_method(mrb, usock, "recvfrom", unixsocket_peeraddr, ARGS_NONE());
  //define_method(mrb, usock, "send_io", unixsocket_peeraddr, ARGS_NONE());
#endif

  /* Windows IO Method Overrides on BasicSocket */
#ifdef _WIN32
  define_method(mrb, bsock, "close", win32_basicsocket_close, ARGS_NONE());
  define_method(mrb, bsock, "sysread", win32_basicsocket_sysread, ARGS_REQ(1)|ARGS_OPT(1));
  define_method(mrb, bsock, "sysseek", win32_basicsocket_sysseek, ARGS_REQ(1));
  define_method(mrb, bsock, "syswrite", win32_basicsocket_syswrite, ARGS_REQ(1));
#endif

  constants = define_module_under(mrb, sock, "Constants");

#define define_const(SYM) \
  do {								\
    define_const(mrb, constants, #SYM, fixnum_value(SYM));	\
  } while (0)

#include "const.cstub"
}

void
mruby_socket_gem_final(state* mrb)
{
  value ai;
  ai = mod_cv_get(mrb, class_get(mrb, "Addrinfo"), intern_lit(mrb, "_lastai"));
  if (cptr_p(ai)) {
    freeaddrinfo((struct addrinfo*)cptr(ai));
  }
#ifdef _WIN32
  WSACleanup();
#endif
}
