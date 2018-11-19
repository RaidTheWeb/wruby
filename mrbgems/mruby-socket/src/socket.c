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

#define E_SOCKET_ERROR             (_class_get(mrb, "SocketError"))

#if !defined(_cptr)
#define _cptr_value(m,p) _voidp_value((m),(p))
#define _cptr(o) _voidp(o)
#define _cptr_p(o) _voidp_p(o)
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
_addrinfo_getaddrinfo(state *mrb, value klass)
{
  struct addrinfo hints, *res0, *res;
  value ai, ary, family, lastai, nodename, protocol, sa, service, socktype;
  _int flags;
  int arena_idx, error;
  const char *hostname = NULL, *servname = NULL;

  ary = _ary_new(mrb);
  arena_idx = _gc_arena_save(mrb);  /* ary must be on arena! */

  family = socktype = protocol = _nil_value();
  flags = 0;
  _get_args(mrb, "oo|oooi", &nodename, &service, &family, &socktype, &protocol, &flags);

  if (_string_p(nodename)) {
    hostname = _str_to_cstr(mrb, nodename);
  } else if (_nil_p(nodename)) {
    hostname = NULL;
  } else {
    _raise(mrb, E_TYPE_ERROR, "nodename must be String or nil");
  }

  if (_string_p(service)) {
    servname = _str_to_cstr(mrb, service);
  } else if (_fixnum_p(service)) {
    servname = _str_to_cstr(mrb, _funcall(mrb, service, "to_s", 0));
  } else if (_nil_p(service)) {
    servname = NULL;
  } else {
    _raise(mrb, E_TYPE_ERROR, "service must be String, Fixnum, or nil");
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = (int)flags;

  if (_fixnum_p(family)) {
    hints.ai_family = (int)_fixnum(family);
  }

  if (_fixnum_p(socktype)) {
    hints.ai_socktype = (int)_fixnum(socktype);
  }

  if (_fixnum_p(protocol)) {
    hints.ai_protocol = (int)_fixnum(protocol);
  }

  lastai = _cv_get(mrb, klass, _intern_lit(mrb, "_lastai"));
  if (_cptr_p(lastai)) {
    freeaddrinfo((struct addrinfo*)_cptr(lastai));
    _cv_set(mrb, klass, _intern_lit(mrb, "_lastai"), _nil_value());
  }

  error = getaddrinfo(hostname, servname, &hints, &res0);
  if (error) {
    _raisef(mrb, E_SOCKET_ERROR, "getaddrinfo: %S", _str_new_cstr(mrb, gai_strerror(error)));
  }
  _cv_set(mrb, klass, _intern_lit(mrb, "_lastai"), _cptr_value(mrb, res0));

  for (res = res0; res != NULL; res = res->ai_next) {
    sa = _str_new(mrb, (char*)res->ai_addr, res->ai_addrlen);
    ai = _funcall(mrb, klass, "new", 4, sa, _fixnum_value(res->ai_family), _fixnum_value(res->ai_socktype), _fixnum_value(res->ai_protocol));
    _ary_push(mrb, ary, ai);
    _gc_arena_restore(mrb, arena_idx);
  }

  freeaddrinfo(res0);
  _cv_set(mrb, klass, _intern_lit(mrb, "_lastai"), _nil_value());

  return ary;
}

static value
_addrinfo_getnameinfo(state *mrb, value self)
{
  _int flags;
  value ary, host, sastr, serv;
  int error;

  flags = 0;
  _get_args(mrb, "|i", &flags);
  host = _str_buf_new(mrb, NI_MAXHOST);
  serv = _str_buf_new(mrb, NI_MAXSERV);

  sastr = _iv_get(mrb, self, _intern_lit(mrb, "@sockaddr"));
  if (!_string_p(sastr)) {
    _raise(mrb, E_SOCKET_ERROR, "invalid sockaddr");
  }
  error = getnameinfo((struct sockaddr *)RSTRING_PTR(sastr), (socklen_t)RSTRING_LEN(sastr), RSTRING_PTR(host), NI_MAXHOST, RSTRING_PTR(serv), NI_MAXSERV, (int)flags);
  if (error) {
    _raisef(mrb, E_SOCKET_ERROR, "getnameinfo: %S", _str_new_cstr(mrb, gai_strerror(error)));
  }
  ary = _ary_new_capa(mrb, 2);
  _str_resize(mrb, host, strlen(RSTRING_PTR(host)));
  _ary_push(mrb, ary, host);
  _str_resize(mrb, serv, strlen(RSTRING_PTR(serv)));
  _ary_push(mrb, ary, serv);
  return ary;
}

#ifndef _WIN32
static value
_addrinfo_unix_path(state *mrb, value self)
{
  value sastr;

  sastr = _iv_get(mrb, self, _intern_lit(mrb, "@sockaddr"));
  if (((struct sockaddr *)RSTRING_PTR(sastr))->sa_family != AF_UNIX)
    _raise(mrb, E_SOCKET_ERROR, "need AF_UNIX address");
  if (RSTRING_LEN(sastr) < (_int)offsetof(struct sockaddr_un, sun_path) + 1) {
    return _str_new(mrb, "", 0);
  } else {
    return _str_new_cstr(mrb, ((struct sockaddr_un *)RSTRING_PTR(sastr))->sun_path);
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
    _raise(mrb, E_ARGUMENT_ERROR, "bad af");
    return _nil_value();
  }
  port = ntohs(port);
  host = _str_buf_new(mrb, NI_MAXHOST);
  if (getnameinfo(sa, salen, RSTRING_PTR(host), NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == -1)
    _sys_fail(mrb, "getnameinfo");
  _str_resize(mrb, host, strlen(RSTRING_PTR(host)));
  ary = _ary_new_capa(mrb, 4);
  _ary_push(mrb, ary, _str_new_cstr(mrb, afstr));
  _ary_push(mrb, ary, _fixnum_value(port));
  _ary_push(mrb, ary, host);
  _ary_push(mrb, ary, host);
  return ary;
}

static int
socket_fd(state *mrb, value sock)
{
  return (int)_fixnum(_funcall(mrb, sock, "fileno", 0));
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
_basicsocket_getpeereid(state *mrb, value self)
{
#ifdef HAVE_GETPEEREID
  value ary;
  gid_t egid;
  uid_t euid;
  int s;

  s = socket_fd(mrb, self);
  if (getpeereid(s, &euid, &egid) != 0)
    _sys_fail(mrb, "getpeereid");

  ary = _ary_new_capa(mrb, 2);
  _ary_push(mrb, ary, _fixnum_value((_int)euid));
  _ary_push(mrb, ary, _fixnum_value((_int)egid));
  return ary;
#else
  _raise(mrb, E_RUNTIME_ERROR, "getpeereid is not available on this system");
  return _nil_value();
#endif
}

static value
_basicsocket_getpeername(state *mrb, value self)
{
  struct sockaddr_storage ss;
  socklen_t salen;

  salen = sizeof(ss);
  if (getpeername(socket_fd(mrb, self), (struct sockaddr *)&ss, &salen) != 0)
    _sys_fail(mrb, "getpeername");

  return _str_new(mrb, (char*)&ss, salen);
}

static value
_basicsocket_getsockname(state *mrb, value self)
{
  struct sockaddr_storage ss;
  socklen_t salen;

  salen = sizeof(ss);
  if (getsockname(socket_fd(mrb, self), (struct sockaddr *)&ss, &salen) != 0)
    _sys_fail(mrb, "getsockname");

  return _str_new(mrb, (char*)&ss, salen);
}

static value
_basicsocket_getsockopt(state *mrb, value self)
{
  char opt[8];
  int s;
  _int family, level, optname;
  value c, data;
  socklen_t optlen;

  _get_args(mrb, "ii", &level, &optname);
  s = socket_fd(mrb, self);
  optlen = sizeof(opt);
  if (getsockopt(s, (int)level, (int)optname, opt, &optlen) == -1)
    _sys_fail(mrb, "getsockopt");
  c = _const_get(mrb, _obj_value(_class_get(mrb, "Socket")), _intern_lit(mrb, "Option"));
  family = socket_family(s);
  data = _str_new(mrb, opt, optlen);
  return _funcall(mrb, c, "new", 4, _fixnum_value(family), _fixnum_value(level), _fixnum_value(optname), data);
}

static value
_basicsocket_recv(state *mrb, value self)
{
  ssize_t n;
  _int maxlen, flags = 0;
  value buf;

  _get_args(mrb, "i|i", &maxlen, &flags);
  buf = _str_buf_new(mrb, maxlen);
  n = recv(socket_fd(mrb, self), RSTRING_PTR(buf), (fsize_t)maxlen, (int)flags);
  if (n == -1)
    _sys_fail(mrb, "recv");
  _str_resize(mrb, buf, (_int)n);
  return buf;
}

static value
_basicsocket_recvfrom(state *mrb, value self)
{
  ssize_t n;
  _int maxlen, flags = 0;
  value ary, buf, sa;
  socklen_t socklen;

  _get_args(mrb, "i|i", &maxlen, &flags);
  buf = _str_buf_new(mrb, maxlen);
  socklen = sizeof(struct sockaddr_storage);
  sa = _str_buf_new(mrb, socklen);
  n = recvfrom(socket_fd(mrb, self), RSTRING_PTR(buf), (fsize_t)maxlen, (int)flags, (struct sockaddr *)RSTRING_PTR(sa), &socklen);
  if (n == -1)
    _sys_fail(mrb, "recvfrom");
  _str_resize(mrb, buf, (_int)n);
  _str_resize(mrb, sa, (_int)socklen);
  ary = _ary_new_capa(mrb, 2);
  _ary_push(mrb, ary, buf);
  _ary_push(mrb, ary, sa);
  return ary;
}

static value
_basicsocket_send(state *mrb, value self)
{
  ssize_t n;
  _int flags;
  value dest, mesg;

  dest = _nil_value();
  _get_args(mrb, "Si|S", &mesg, &flags, &dest);
  if (_nil_p(dest)) {
    n = send(socket_fd(mrb, self), RSTRING_PTR(mesg), (fsize_t)RSTRING_LEN(mesg), (int)flags);
  } else {
    n = sendto(socket_fd(mrb, self), RSTRING_PTR(mesg), (fsize_t)RSTRING_LEN(mesg), (int)flags, (const struct sockaddr*)RSTRING_PTR(dest), (fsize_t)RSTRING_LEN(dest));
  }
  if (n == -1)
    _sys_fail(mrb, "send");
  return _fixnum_value((_int)n);
}

static value
_basicsocket_setnonblock(state *mrb, value self)
{
  int fd, flags;
  _bool nonblocking;
#ifdef _WIN32
  u_long mode = 1;
#endif

  _get_args(mrb, "b", &nonblocking);
  fd = socket_fd(mrb, self);
#ifdef _WIN32
  flags = ioctlsocket(fd, FIONBIO, &mode);
  if (flags != NO_ERROR)
    _sys_fail(mrb, "ioctlsocket");
#else
  flags = fcntl(fd, F_GETFL, 0);
  if (flags == 1)
    _sys_fail(mrb, "fcntl");
  if (nonblocking)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) == -1)
    _sys_fail(mrb, "fcntl");
#endif
  return _nil_value();
}

static value
_basicsocket_setsockopt(state *mrb, value self)
{
  int s;
  _int argc, level = 0, optname;
  value optval, so;

  argc = _get_args(mrb, "o|io", &so, &optname, &optval);
  if (argc == 3) {
    if (!_fixnum_p(so)) {
      _raise(mrb, E_ARGUMENT_ERROR, "level is not an integer");
    }
    level = _fixnum(so);
    if (_string_p(optval)) {
      /* that's good */
    } else if (_type(optval) == MRB_TT_TRUE || _type(optval) == MRB_TT_FALSE) {
      _int i = _test(optval) ? 1 : 0;
      optval = _str_new(mrb, (char*)&i, sizeof(i));
    } else if (_fixnum_p(optval)) {
      if (optname == IP_MULTICAST_TTL || optname == IP_MULTICAST_LOOP) {
        char uc = (char)_fixnum(optval);
        optval = _str_new(mrb, &uc, sizeof(uc));
      } else {
        _int i = _fixnum(optval);
        optval = _str_new(mrb, (char*)&i, sizeof(i));
      }
    } else {
      _raise(mrb, E_ARGUMENT_ERROR, "optval should be true, false, an integer, or a string");
    }
  } else if (argc == 1) {
    if (strcmp(_obj_classname(mrb, so), "Socket::Option") != 0)
      _raise(mrb, E_ARGUMENT_ERROR, "not an instance of Socket::Option");
    level = _fixnum(_funcall(mrb, so, "level", 0));
    optname = _fixnum(_funcall(mrb, so, "optname", 0));
    optval = _funcall(mrb, so, "data", 0);
  } else {
    _raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%S for 3)", _fixnum_value(argc));
  }

  s = socket_fd(mrb, self);
  if (setsockopt(s, (int)level, (int)optname, RSTRING_PTR(optval), (socklen_t)RSTRING_LEN(optval)) == -1)
    _sys_fail(mrb, "setsockopt");
  return _fixnum_value(0);
}

static value
_basicsocket_shutdown(state *mrb, value self)
{
  _int how = SHUT_RDWR;

  _get_args(mrb, "|i", &how);
  if (shutdown(socket_fd(mrb, self), (int)how) != 0)
    _sys_fail(mrb, "shutdown");
  return _fixnum_value(0);
}

static value
_basicsocket_set_is_socket(state *mrb, value self)
{
  _bool b;
  struct _io *io_p;
  _get_args(mrb, "b", &b);

  io_p = (struct _io*)DATA_PTR(self);
  if (io_p) {
    io_p->is_socket = b;
  }

  return _bool_value(b);
}

static value
_ipsocket_ntop(state *mrb, value klass)
{
  _int af, n;
  char *addr, buf[50];

  _get_args(mrb, "is", &af, &addr, &n);
  if ((af == AF_INET && n != 4) || (af == AF_INET6 && n != 16))
    _raise(mrb, E_ARGUMENT_ERROR, "invalid address");
  if (inet_ntop((int)af, addr, buf, sizeof(buf)) == NULL)
    _raise(mrb, E_ARGUMENT_ERROR, "invalid address");
  return _str_new_cstr(mrb, buf);
}

static value
_ipsocket_pton(state *mrb, value klass)
{
  _int af, n;
  char *bp, buf[50];

  _get_args(mrb, "is", &af, &bp, &n);
  if ((size_t)n > sizeof(buf) - 1)
    _raise(mrb, E_ARGUMENT_ERROR, "invalid address");
  memcpy(buf, bp, n);
  buf[n] = '\0';

  if (af == AF_INET) {
    struct in_addr in;
    if (inet_pton(AF_INET, buf, (void *)&in.s_addr) != 1)
      goto invalid;
    return _str_new(mrb, (char*)&in.s_addr, 4);
  } else if (af == AF_INET6) {
    struct in6_addr in6;
    if (inet_pton(AF_INET6, buf, (void *)&in6.s6_addr) != 1)
      goto invalid;
    return _str_new(mrb, (char*)&in6.s6_addr, 16);
  } else
    _raise(mrb, E_ARGUMENT_ERROR, "unsupported address family");

invalid:
  _raise(mrb, E_ARGUMENT_ERROR, "invalid address");
  return _nil_value(); /* dummy */
}

static value
_ipsocket_recvfrom(state *mrb, value self)
{
  struct sockaddr_storage ss;
  socklen_t socklen;
  value a, buf, pair;
  _int flags, maxlen;
  ssize_t n;
  int fd;

  fd = socket_fd(mrb, self);
  flags = 0;
  _get_args(mrb, "i|i", &maxlen, &flags);
  buf = _str_buf_new(mrb, maxlen);
  socklen = sizeof(ss);
  n = recvfrom(fd, RSTRING_PTR(buf), (fsize_t)maxlen, (int)flags,
  	       (struct sockaddr *)&ss, &socklen);
  if (n == -1) {
    _sys_fail(mrb, "recvfrom");
  }
  _str_resize(mrb, buf, (_int)n);
  a = sa2addrlist(mrb, (struct sockaddr *)&ss, socklen);
  pair = _ary_new_capa(mrb, 2);
  _ary_push(mrb, pair, buf);
  _ary_push(mrb, pair, a);
  return pair;
}

static value
_socket_gethostname(state *mrb, value cls)
{
  value buf;
  size_t bufsize;

#ifdef HOST_NAME_MAX
  bufsize = HOST_NAME_MAX + 1;
#else
  bufsize = 256;
#endif
  buf = _str_buf_new(mrb, (_int)bufsize);
  if (gethostname(RSTRING_PTR(buf), (fsize_t)bufsize) != 0)
    _sys_fail(mrb, "gethostname");
  _str_resize(mrb, buf, (_int)strlen(RSTRING_PTR(buf)));
  return buf;
}

static value
_socket_accept(state *mrb, value klass)
{
  int s1;
  _int s0;

  _get_args(mrb, "i", &s0);
  s1 = (int)accept(s0, NULL, NULL);
  if (s1 == -1) {
    _sys_fail(mrb, "accept");
  }
  return _fixnum_value(s1);
}

static value
_socket_accept2(state *mrb, value klass)
{
  value ary, sastr;
  int s1;
  _int s0;
  socklen_t socklen;

  _get_args(mrb, "i", &s0);
  socklen = sizeof(struct sockaddr_storage);
  sastr = _str_buf_new(mrb, socklen);
  s1 = (int)accept(s0, (struct sockaddr *)RSTRING_PTR(sastr), &socklen);
  if (s1 == -1) {
    _sys_fail(mrb, "accept");
  }
  // XXX: possible descriptor leakage here!
  _str_resize(mrb, sastr, socklen);
  ary = _ary_new_capa(mrb, 2);
  _ary_push(mrb, ary, _fixnum_value(s1));
  _ary_push(mrb, ary, sastr);
  return ary;
}

static value
_socket_bind(state *mrb, value klass)
{
  value sastr;
  _int s;

  _get_args(mrb, "iS", &s, &sastr);
  if (bind((int)s, (struct sockaddr *)RSTRING_PTR(sastr), (socklen_t)RSTRING_LEN(sastr)) == -1) {
    _sys_fail(mrb, "bind");
  }
  return _nil_value();
}

static value
_socket_connect(state *mrb, value klass)
{
  value sastr;
  _int s;

  _get_args(mrb, "iS", &s, &sastr);
  if (connect((int)s, (struct sockaddr *)RSTRING_PTR(sastr), (socklen_t)RSTRING_LEN(sastr)) == -1) {
    _sys_fail(mrb, "connect");
  }
  return _nil_value();
}

static value
_socket_listen(state *mrb, value klass)
{
  _int backlog, s;

  _get_args(mrb, "ii", &s, &backlog);
  if (listen((int)s, (int)backlog) == -1) {
    _sys_fail(mrb, "listen");
  }
  return _nil_value();
}

static value
_socket_sockaddr_family(state *mrb, value klass)
{
  const struct sockaddr *sa;
  value str;

  _get_args(mrb, "S", &str);
  if ((size_t)RSTRING_LEN(str) < offsetof(struct sockaddr, sa_family) + sizeof(sa->sa_family)) {
    _raise(mrb, E_SOCKET_ERROR, "invalid sockaddr (too short)");
  }
  sa = (const struct sockaddr *)RSTRING_PTR(str);
  return _fixnum_value(sa->sa_family);
}

static value
_socket_sockaddr_un(state *mrb, value klass)
{
#ifdef _WIN32
  _raise(mrb, E_NOTIMP_ERROR, "sockaddr_un unsupported on Windows");
  return _nil_value();
#else
  struct sockaddr_un *sunp;
  value path, s;

  _get_args(mrb, "S", &path);
  if ((size_t)RSTRING_LEN(path) > sizeof(sunp->sun_path) - 1) {
    _raisef(mrb, E_ARGUMENT_ERROR, "too long unix socket path (max: %S bytes)", _fixnum_value(sizeof(sunp->sun_path) - 1));
  }
  s = _str_buf_new(mrb, sizeof(struct sockaddr_un));
  sunp = (struct sockaddr_un *)RSTRING_PTR(s);
#if HAVE_SA_LEN
  sunp->sun_len = sizeof(struct sockaddr_un);
#endif
  sunp->sun_family = AF_UNIX;
  memcpy(sunp->sun_path, RSTRING_PTR(path), RSTRING_LEN(path));
  sunp->sun_path[RSTRING_LEN(path)] = '\0';
  _str_resize(mrb, s, sizeof(struct sockaddr_un));
  return s;
#endif
}

static value
_socket_socketpair(state *mrb, value klass)
{
#ifdef _WIN32
  _raise(mrb, E_NOTIMP_ERROR, "socketpair unsupported on Windows");
  return _nil_value();
#else
  value ary;
  _int domain, type, protocol;
  int sv[2];

  _get_args(mrb, "iii", &domain, &type, &protocol);
  if (socketpair(domain, type, protocol, sv) == -1) {
    _sys_fail(mrb, "socketpair");
  }
  // XXX: possible descriptor leakage here!
  ary = _ary_new_capa(mrb, 2);
  _ary_push(mrb, ary, _fixnum_value(sv[0]));
  _ary_push(mrb, ary, _fixnum_value(sv[1]));
  return ary;
#endif
}

static value
_socket_socket(state *mrb, value klass)
{
  _int domain, type, protocol;
  int s;

  _get_args(mrb, "iii", &domain, &type, &protocol);
  s = (int)socket((int)domain, (int)type, (int)protocol);
  if (s == -1)
    _sys_fail(mrb, "socket");
  return _fixnum_value(s);
}

static value
_tcpsocket_allocate(state *mrb, value klass)
{
  struct RClass *c = _class_ptr(klass);
  enum _vtype ttype = MRB_INSTANCE_TT(c);

  /* copied from _instance_alloc() */
  if (ttype == 0) ttype = MRB_TT_OBJECT;
  return _obj_value((struct RObject*)_obj_alloc(mrb, ttype, c));
}

/* Windows overrides for IO methods on BasicSocket objects.
 * This is because sockets on Windows are not the same as file
 * descriptors, and thus functions which operate on file descriptors
 * will break on socket descriptors.
 */
#ifdef _WIN32
static value
_win32_basicsocket_close(state *mrb, value self)
{
  if (closesocket(socket_fd(mrb, self)) != NO_ERROR)
    _raise(mrb, E_SOCKET_ERROR, "closesocket unsuccessful");
  return _nil_value();
}

#define E_EOF_ERROR                (_class_get(mrb, "EOFError"))
static value
_win32_basicsocket_sysread(state *mrb, value self)
{
  int sd, ret;
  value buf = _nil_value();
  _int maxlen;

  _get_args(mrb, "i|S", &maxlen, &buf);
  if (maxlen < 0) {
    return _nil_value();
  }

  if (_nil_p(buf)) {
    buf = _str_new(mrb, NULL, maxlen);
  }
  if (RSTRING_LEN(buf) != maxlen) {
    buf = _str_resize(mrb, buf, maxlen);
  }

  sd = socket_fd(mrb, self);
  ret = recv(sd, RSTRING_PTR(buf), (int)maxlen, 0);

  switch (ret) {
    case 0: /* EOF */
      if (maxlen == 0) {
        buf = _str_new_cstr(mrb, "");
      } else {
        _raise(mrb, E_EOF_ERROR, "sysread failed: End of File");
      }
      break;
    case SOCKET_ERROR: /* Error */
      _sys_fail(mrb, "recv");
      break;
    default:
      if (RSTRING_LEN(buf) != ret) {
        buf = _str_resize(mrb, buf, ret);
      }
      break;
  }

  return buf;
}

static value
_win32_basicsocket_sysseek(state *mrb, value self)
{
  _raise(mrb, E_NOTIMP_ERROR, "sysseek not implemented for windows sockets");
  return _nil_value();
}

static value
_win32_basicsocket_syswrite(state *mrb, value self)
{
  int n;
  SOCKET sd;
  value str;

  sd = socket_fd(mrb, self);
  _get_args(mrb, "S", &str);
  n = send(sd, RSTRING_PTR(str), (int)RSTRING_LEN(str), 0);
  if (n == SOCKET_ERROR)
    _sys_fail(mrb, "send");
  return _fixnum_value(n);
}

#endif

void
_mruby_socket_gem_init(state* mrb)
{
  struct RClass *io, *ai, *sock, *bsock, *ipsock, *tcpsock;
  struct RClass *constants;

#ifdef _WIN32
  WSADATA wsaData;
  int result;
  result = WSAStartup(MAKEWORD(2,2), &wsaData);
  if (result != NO_ERROR)
    _raise(mrb, E_RUNTIME_ERROR, "WSAStartup failed");
#endif

  ai = _define_class(mrb, "Addrinfo", mrb->object_class);
  _mod_cv_set(mrb, ai, _intern_lit(mrb, "_lastai"), _nil_value());
  _define_class_method(mrb, ai, "getaddrinfo", _addrinfo_getaddrinfo, MRB_ARGS_REQ(2)|MRB_ARGS_OPT(4));
  _define_method(mrb, ai, "getnameinfo", _addrinfo_getnameinfo, MRB_ARGS_OPT(1));
#ifndef _WIN32
  _define_method(mrb, ai, "unix_path", _addrinfo_unix_path, MRB_ARGS_NONE());
#endif

  io = _class_get(mrb, "IO");

  bsock = _define_class(mrb, "BasicSocket", io);
  _define_method(mrb, bsock, "_recvfrom", _basicsocket_recvfrom, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  _define_method(mrb, bsock, "_setnonblock", _basicsocket_setnonblock, MRB_ARGS_REQ(1));
  _define_method(mrb, bsock, "getpeereid", _basicsocket_getpeereid, MRB_ARGS_NONE());
  _define_method(mrb, bsock, "getpeername", _basicsocket_getpeername, MRB_ARGS_NONE());
  _define_method(mrb, bsock, "getsockname", _basicsocket_getsockname, MRB_ARGS_NONE());
  _define_method(mrb, bsock, "getsockopt", _basicsocket_getsockopt, MRB_ARGS_REQ(2));
  _define_method(mrb, bsock, "recv", _basicsocket_recv, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  // #recvmsg(maxlen, flags=0)
  _define_method(mrb, bsock, "send", _basicsocket_send, MRB_ARGS_REQ(2)|MRB_ARGS_OPT(1));
  // #sendmsg
  // #sendmsg_nonblock
  _define_method(mrb, bsock, "setsockopt", _basicsocket_setsockopt, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(2));
  _define_method(mrb, bsock, "shutdown", _basicsocket_shutdown, MRB_ARGS_OPT(1));
  _define_method(mrb, bsock, "_is_socket=", _basicsocket_set_is_socket, MRB_ARGS_REQ(1));

  ipsock = _define_class(mrb, "IPSocket", bsock);
  _define_class_method(mrb, ipsock, "ntop", _ipsocket_ntop, MRB_ARGS_REQ(1));
  _define_class_method(mrb, ipsock, "pton", _ipsocket_pton, MRB_ARGS_REQ(2));
  _define_method(mrb, ipsock, "recvfrom", _ipsocket_recvfrom, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));

  tcpsock = _define_class(mrb, "TCPSocket", ipsock);
  _define_class_method(mrb, tcpsock, "_allocate", _tcpsocket_allocate, MRB_ARGS_NONE());
  _define_class(mrb, "TCPServer", tcpsock);

  _define_class(mrb, "UDPSocket", ipsock);
  //#recvfrom_nonblock

  sock = _define_class(mrb, "Socket", bsock);
  _define_class_method(mrb, sock, "_accept", _socket_accept, MRB_ARGS_REQ(1));
  _define_class_method(mrb, sock, "_accept2", _socket_accept2, MRB_ARGS_REQ(1));
  _define_class_method(mrb, sock, "_bind", _socket_bind, MRB_ARGS_REQ(3));
  _define_class_method(mrb, sock, "_connect", _socket_connect, MRB_ARGS_REQ(3));
  _define_class_method(mrb, sock, "_listen", _socket_listen, MRB_ARGS_REQ(2));
  _define_class_method(mrb, sock, "_sockaddr_family", _socket_sockaddr_family, MRB_ARGS_REQ(1));
  _define_class_method(mrb, sock, "_socket", _socket_socket, MRB_ARGS_REQ(3));
  //_define_class_method(mrb, sock, "gethostbyaddr", _socket_gethostbyaddr, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  //_define_class_method(mrb, sock, "gethostbyname", _socket_gethostbyname, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  _define_class_method(mrb, sock, "gethostname", _socket_gethostname, MRB_ARGS_NONE());
  //_define_class_method(mrb, sock, "getservbyname", _socket_getservbyname, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  //_define_class_method(mrb, sock, "getservbyport", _socket_getservbyport, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  _define_class_method(mrb, sock, "sockaddr_un", _socket_sockaddr_un, MRB_ARGS_REQ(1));
  _define_class_method(mrb, sock, "socketpair", _socket_socketpair, MRB_ARGS_REQ(3));
  //_define_method(mrb, sock, "sysaccept", _socket_accept, MRB_ARGS_NONE());

#ifndef _WIN32
  _define_class(mrb, "UNIXSocket", bsock);
  //_define_class_method(mrb, usock, "pair", _unixsocket_open, MRB_ARGS_OPT(2));
  //_define_class_method(mrb, usock, "socketpair", _unixsocket_open, MRB_ARGS_OPT(2));

  //_define_method(mrb, usock, "recv_io", _unixsocket_peeraddr, MRB_ARGS_NONE());
  //_define_method(mrb, usock, "recvfrom", _unixsocket_peeraddr, MRB_ARGS_NONE());
  //_define_method(mrb, usock, "send_io", _unixsocket_peeraddr, MRB_ARGS_NONE());
#endif

  /* Windows IO Method Overrides on BasicSocket */
#ifdef _WIN32
  _define_method(mrb, bsock, "close", _win32_basicsocket_close, MRB_ARGS_NONE());
  _define_method(mrb, bsock, "sysread", _win32_basicsocket_sysread, MRB_ARGS_REQ(1)|MRB_ARGS_OPT(1));
  _define_method(mrb, bsock, "sysseek", _win32_basicsocket_sysseek, MRB_ARGS_REQ(1));
  _define_method(mrb, bsock, "syswrite", _win32_basicsocket_syswrite, MRB_ARGS_REQ(1));
#endif

  constants = _define_module_under(mrb, sock, "Constants");

#define define_const(SYM) \
  do {								\
    _define_const(mrb, constants, #SYM, _fixnum_value(SYM));	\
  } while (0)

#include "const.cstub"
}

void
_mruby_socket_gem_final(state* mrb)
{
  value ai;
  ai = _mod_cv_get(mrb, _class_get(mrb, "Addrinfo"), _intern_lit(mrb, "_lastai"));
  if (_cptr_p(ai)) {
    freeaddrinfo((struct addrinfo*)_cptr(ai));
  }
#ifdef _WIN32
  WSACleanup();
#endif
}
