// WS2_32 (Winsock 1.x subset used by the game) on top of POSIX UDP sockets. The game's lockstep layer runs even in
// single player and needs a working socket stack; it also makes LAN play possible.
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>

#include "aztec_rt.h"
#include "dbg.h"

#include <pthread.h>

#define WS_MODULE 0x70000000u
#define FAKE_IPX 0x7777u   // IPX does not exist on Linux: a placeholder socket that accepts everything and never delivers
static __thread int wsa_err;

// Opt-in oracle hook for sandboxes without sockets. Single-player UDP only;
// packets addressed to this instance or broadcast are delivered in memory.
#define ORACLE_UDP 0x7778u
static struct sockaddr_in oracle_address;
static struct { uint8_t bytes[1600]; uint32_t size; } oracle_packets[64];
static unsigned oracle_read, oracle_write;
static pthread_mutex_t oracle_lock = PTHREAD_MUTEX_INITIALIZER;

// Loopback queue for the IPX placeholder: the game broadcasts a probe and waits to receive it back to learn its own
// address. Packets sent on the placeholder are delivered to its own receive queue, stamped with a fixed IPX address.
#define LB_MAX 64
static struct { uint8_t data[1600]; uint32_t len; } lb_q[LB_MAX];
static int lb_head, lb_tail;
static uint8_t ipx_bound[16];
static int ipx_fd = -1;          // real UDP socket carrying IPX packets (IPX over UDP)
static struct in_addr local_ip;  // our IPv4 address as the game sees it (node = 00 00 + IPv4)
#define IPX_UDP_BASE 34000
static pthread_mutex_t lb_lock = PTHREAD_MUTEX_INITIALIZER;

static int map_errno(int e) {
  switch (e) {
    case EAGAIN: return 10035;        // WSAEWOULDBLOCK
    case EINTR: return 10004;
    case EBADF: return 10038;         // WSAENOTSOCK
    case EADDRINUSE: return 10048;
    case EADDRNOTAVAIL: return 10049;
    case ENETUNREACH: return 10051;
    case ECONNRESET: return 10054;
    case EMSGSIZE: return 10040;
    case EACCES: return 10013;
    case 0: return 0;
    default: return 10000 + (e % 1000);
  }
}
#define SOCKERR(rc) ((rc) < 0 ? (wsa_err = map_errno(errno), (uint32_t)-1) : 0)

#define SH(n) static uint32_t ws_##n(CPU* c, uint32_t* a)
#define UNUSED (void)c; (void)a

SH(WSAStartup) {   // version, WSADATA*
  UNUSED;
  uint32_t p = a[1];
  memset(GPTR(p), 0, 400);
  GU16(p) = 1;                // wVersion: the game insists on 1.0
  GU16(p + 2) = 0x0202;       // wHighVersion
  strcpy((char*)GPTR(p + 4), "Aztec Winsock layer");
  strcpy((char*)GPTR(p + 261), "Running");
  GU16(p + 390) = 32767; GU16(p + 392) = 65467;
  DBG("net", "WSAStartup(version=%04x)", a[0]);
  return 0;
}
SH(WSACleanup) { UNUSED; return 0; }
SH(WSAGetLastError) { UNUSED; return (uint32_t)wsa_err; }

SH(getprotobyname) {   // name -> struct protoent in guest memory
  UNUSED;
  static uint32_t ent;
  const char* name = (const char*)GPTR(a[0]);
  if (!ent) ent = gmalloc(32);
  uint32_t nm = guest_cstring(name);
  GU32(ent) = nm; GU32(ent + 4) = ent + 16; GU32(ent + 16) = 0;
  GU16(ent + 8) = !strcasecmp(name, "udp") ? 17 : !strcasecmp(name, "tcp") ? 6 : 0;
  DBG("net", "getprotobyname('%s') -> proto %u", name, GU16(ent + 8));
  return ent;
}

SH(socket) {   // af, type, protocol
  UNUSED;
  if (a[0] == 6) { DBG("net", "socket(AF_IPX) -> placeholder %#x", FAKE_IPX); return FAKE_IPX; }
  if (getenv("RT_ORACLE_UDP") && a[0] == AF_INET && a[1] == SOCK_DGRAM) {
    DBG("net", "socket: RT_ORACLE_UDP in-memory transport enabled");
    return ORACLE_UDP;
  }
  int fd = socket((int)a[0], (int)a[1], (int)a[2]);
  DBG("net", "socket(af=%u type=%u proto=%u) -> %d", a[0], a[1], a[2], fd);
  if (fd < 0) { wsa_err = map_errno(errno); return (uint32_t)-1; }
  return (uint32_t)fd;
}

SH(setsockopt) {   // s, level, optname, optval*, optlen
  UNUSED;
  if (a[0] == ORACLE_UDP || a[0] == FAKE_IPX) return 0;
  int level = (int)a[1], name = (int)a[2];
  if (level == 0xffff) {
    level = SOL_SOCKET;
    switch (name) {   // Windows -> Linux option numbers
      case 0x0020: name = SO_BROADCAST; break;
      case 0x0004: name = SO_REUSEADDR; break;
      case 0x1001: name = SO_SNDBUF; break;
      case 0x1002: name = SO_RCVBUF; break;
      case 0x0080: name = SO_LINGER; break;
      case 0x0008: name = SO_KEEPALIVE; break;
      default: DBG("net", "setsockopt: unknown SOL_SOCKET option %#x ignored", name); return 0;
    }
  }
  int rc = setsockopt((int)a[0], level, name, GPTR(a[3]), a[4]);
  DBG("net", "setsockopt(s=%u level=%u opt=%#x) -> %d", a[0], a[1], a[2], rc);
  return rc < 0 ? (wsa_err = map_errno(errno), (uint32_t)-1) : 0;
}

SH(gethostname) {   // buf, len
  UNUSED;
  char buf[256];
  if (gethostname(buf, sizeof buf) != 0) strcpy(buf, "aztec-host");
  buf[sizeof buf - 1] = 0;
  strncpy((char*)GPTR(a[0]), buf, a[1]);
  DBG("net", "gethostname -> '%s'", buf);
  return 0;
}

SH(gethostbyname) {   // name -> struct hostent in guest memory
  UNUSED;
  static uint32_t he, addrs, addr;
  if (!he) { he = gmalloc(32); addrs = gmalloc(8); addr = gmalloc(4); }
  const char* name = (const char*)GPTR(a[0]);
  struct in_addr ia = {0};
  struct addrinfo hints, *res = NULL;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  if (getenv("RT_ORACLE_UDP")) inet_aton("192.168.1.125", &ia);
  else if (getaddrinfo(name, NULL, &hints, &res) == 0 && res) { ia = ((struct sockaddr_in*)res->ai_addr)->sin_addr; freeaddrinfo(res); }
  else inet_aton("127.0.0.1", &ia);
  if (getenv("RT_HOST_IP")) inet_aton(getenv("RT_HOST_IP"), &ia);   // tests: two instances on one machine
  memcpy(GPTR(addr), &ia, 4);
  GU32(addrs) = addr; GU32(addrs + 4) = 0;
  GU32(he) = guest_cstring(name); GU32(he + 4) = addrs + 4; GU16(he + 8) = 2; GU16(he + 10) = 4; GU32(he + 12) = addrs;
  DBG("net", "gethostbyname('%s') -> %s", name, inet_ntoa(ia));
  return he;
}

SH(bind) {   // s, sockaddr*, len
  UNUSED;
  if (a[0] == ORACLE_UDP) {
    if (a[2] != sizeof oracle_address) { wsa_err = 10022; return (uint32_t)-1; }
    memcpy(&oracle_address, GPTR(a[1]), sizeof oracle_address);
    local_ip = oracle_address.sin_addr;
    return 0;
  }
  if (a[0] == FAKE_IPX) {
    memset(ipx_bound, 0, sizeof ipx_bound);
    memcpy(ipx_bound, GPTR(a[1]), a[2] < 14 ? a[2] : 14);
    unsigned sock = (unsigned)(ipx_bound[12] << 8 | ipx_bound[13]);
    ipx_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ipx_fd >= 0) {
      int one = 1;
      setsockopt(ipx_fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
      setsockopt(ipx_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
      struct sockaddr_in ba = {0};
      ba.sin_family = AF_INET;
      ba.sin_port = htons((uint16_t)(IPX_UDP_BASE + (sock & 0x3ff)));
      ba.sin_addr.s_addr = getenv("RT_HOST_IP") ? local_ip.s_addr : htonl(INADDR_ANY);   // tests: several instances on one host
      if (bind(ipx_fd, (struct sockaddr*)&ba, sizeof ba) != 0) { DBG("net", "IPX-over-UDP bind failed: %s (LAN play unavailable, loopback only)", strerror(errno)); close(ipx_fd); ipx_fd = -1; }
      else fcntl(ipx_fd, F_SETFL, O_NONBLOCK);
    }
    DBG("net", "bind(IPX) len=%u socket=%02x%02x -> UDP port %u fd=%d local ip %s", a[2], ipx_bound[12], ipx_bound[13], IPX_UDP_BASE + (sock & 0x3ff), ipx_fd, inet_ntoa(local_ip));
    return 0;
  }
  int rc = bind((int)a[0], (struct sockaddr*)GPTR(a[1]), a[2]);
  struct sockaddr_in* sa = (struct sockaddr_in*)GPTR(a[1]);
  if (rc == 0 && sa->sin_family == AF_INET && sa->sin_addr.s_addr) local_ip = sa->sin_addr;
  DBG("net", "bind(s=%u port=%u addr=%s) -> %d (%s)", a[0], ntohs(sa->sin_port), inet_ntoa(sa->sin_addr), rc, rc ? strerror(errno) : "ok");
  return SOCKERR(rc);
}

SH(getsockname) {   // s, sockaddr*, int* len
  UNUSED;
  if (a[0] == ORACLE_UDP) {
    if (!a[2] || GU32(a[2]) < sizeof oracle_address) { wsa_err = 10022; return (uint32_t)-1; }
    memcpy(GPTR(a[1]), &oracle_address, sizeof oracle_address); GU32(a[2]) = sizeof oracle_address;
    return 0;
  }
  if (a[0] == FAKE_IPX) {
    uint8_t me[14] = {0};
    me[0] = 6; memcpy(me + 8, &local_ip, 4); if (!local_ip.s_addr) me[11] = 1;
    me[12] = ipx_bound[12]; me[13] = ipx_bound[13];
    if (a[2]) { uint32_t l = GU32(a[2]) < 14 ? GU32(a[2]) : 14; memcpy(GPTR(a[1]), me, l); GU32(a[2]) = 14; }
    return 0;
  }
  socklen_t len = GU32(a[2]);
  int rc = getsockname((int)a[0], (struct sockaddr*)GPTR(a[1]), &len);
  GU32(a[2]) = len;
  return SOCKERR(rc);
}

SH(sendto) {   // s, buf, len, flags, to*, tolen
  UNUSED;
  if (a[0] == ORACLE_UDP) {
    if (a[2] > sizeof oracle_packets[0].bytes) { wsa_err = 10040; return (uint32_t)-1; }
    if (!a[4] || a[5] < sizeof oracle_address) { wsa_err = 10022; return (uint32_t)-1; }
    const struct sockaddr_in* to = GPTR(a[4]);
    uint32_t ip = ntohl(to->sin_addr.s_addr);
    if (to->sin_addr.s_addr == oracle_address.sin_addr.s_addr || (ip & 255) == 255) {
      pthread_mutex_lock(&oracle_lock);
      unsigned next = (oracle_write + 1) % 64;
      if (next != oracle_read) {
        memcpy(oracle_packets[oracle_write].bytes, GPTR(a[1]), a[2]);
        oracle_packets[oracle_write].size = a[2]; oracle_write = next;
      }
      pthread_mutex_unlock(&oracle_lock);
    }
    return a[2];
  }
  if (a[0] == FAKE_IPX) {   // IPX over UDP; packets for ourselves (and broadcasts) are also looped back locally
    const uint8_t* to = a[4] ? (const uint8_t*)GPTR(a[4]) : NULL;
    int bcast = !to, self = 0;
    uint16_t port = (uint16_t)(IPX_UDP_BASE + (((ipx_bound[12] << 8) | ipx_bound[13]) & 0x3ff));
    struct in_addr dst = {0};
    if (to) {
      port = (uint16_t)(IPX_UDP_BASE + (((to[12] << 8) | to[13]) & 0x3ff));
      bcast = 1;
      for (int i = 0; i < 6; i++) if (to[6 + i] != 0xff) bcast = 0;
      memcpy(&dst, to + 8, 4);
      self = !bcast && dst.s_addr == local_ip.s_addr;
    }
    if (bcast || self) {
      pthread_mutex_lock(&lb_lock);
      int next = (lb_tail + 1) % LB_MAX;
      if (next != lb_head) {
        uint32_t n = a[2] > sizeof lb_q[0].data ? (uint32_t)sizeof lb_q[0].data : a[2];
        memcpy(lb_q[lb_tail].data, GPTR(a[1]), n);
        lb_q[lb_tail].len = n;
        lb_tail = next;
      }
      pthread_mutex_unlock(&lb_lock);
    }
    if (ipx_fd >= 0 && !self) {
      struct sockaddr_in da = {0};
      da.sin_family = AF_INET; da.sin_port = htons(port);
      if (!bcast) { da.sin_addr = dst; sendto(ipx_fd, GPTR(a[1]), a[2], 0, (struct sockaddr*)&da, sizeof da); }
      else if (getenv("RT_IPX_PEERS")) {   // tests: explicit peer list instead of broadcast
        char list[256]; snprintf(list, sizeof list, "%s", getenv("RT_IPX_PEERS"));
        for (char* tok = strtok(list, ","); tok; tok = strtok(NULL, ","))
          if (inet_aton(tok, &da.sin_addr) && da.sin_addr.s_addr != local_ip.s_addr) sendto(ipx_fd, GPTR(a[1]), a[2], 0, (struct sockaddr*)&da, sizeof da);
      } else {
        da.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        sendto(ipx_fd, GPTR(a[1]), a[2], 0, (struct sockaddr*)&da, sizeof da);
        struct ifaddrs *ifa, *p;
        if (getifaddrs(&ifa) == 0) {
          for (p = ifa; p; p = p->ifa_next)
            if (p->ifa_broadaddr && p->ifa_broadaddr->sa_family == AF_INET && (p->ifa_flags & IFF_BROADCAST) && (p->ifa_flags & IFF_UP)) {
              da.sin_addr = ((struct sockaddr_in*)p->ifa_broadaddr)->sin_addr;
              sendto(ipx_fd, GPTR(a[1]), a[2], 0, (struct sockaddr*)&da, sizeof da);
            }
          freeifaddrs(ifa);
        }
      }
    }
    { static unsigned cnt; if (cnt++ < 60) DBG("net", "IPX send %u bytes %s (udp port %u) first4=%08x", a[2], bcast ? "broadcast" : self ? "to self" : inet_ntoa(dst), port, a[2] >= 4 ? GU32(a[1]) : 0); }
    return a[2];
  }
  ssize_t n = sendto((int)a[0], GPTR(a[1]), a[2], (int)a[3], a[4] ? (struct sockaddr*)GPTR(a[4]) : NULL, a[5]);
  if (n < 0) { wsa_err = map_errno(errno); DBG_EVERY(20, "net", "sendto(%u bytes) failed: %s", a[2], strerror(errno)); return (uint32_t)-1; }
  { static unsigned cnt; if (cnt++ < 60 || cnt % 200 == 0) { struct sockaddr_in* to = a[4] ? (struct sockaddr_in*)GPTR(a[4]) : NULL; DBG("net", "sendto s=%u %u bytes -> %zd (to %s:%u) first4=%08x", a[0], a[2], n, to ? inet_ntoa(to->sin_addr) : "?", to ? ntohs(to->sin_port) : 0, a[2] >= 4 ? GU32(a[1]) : 0); } }
  return (uint32_t)n;
}

SH(recvfrom) {   // s, buf, len, flags, from*, fromlen*
  UNUSED;
  if (a[0] == ORACLE_UDP) {
    pthread_mutex_lock(&oracle_lock);
    if (oracle_read == oracle_write) {
      pthread_mutex_unlock(&oracle_lock); wsa_err = 10035; return (uint32_t)-1;
    }
    uint32_t n = oracle_packets[oracle_read].size;
    if (n > a[2]) n = a[2];
    memcpy(GPTR(a[1]), oracle_packets[oracle_read].bytes, n);
    oracle_read = (oracle_read + 1) % 64;
    pthread_mutex_unlock(&oracle_lock);
    if (a[4] && a[5]) {
      uint32_t len = GU32(a[5]); if (len > sizeof oracle_address) len = sizeof oracle_address;
      memcpy(GPTR(a[4]), &oracle_address, len); GU32(a[5]) = sizeof oracle_address;
    }
    return n;
  }
  if (a[0] == FAKE_IPX) {
    for (int tries = 0; tries < 50; tries++) {   // blocking receive with a short timeout
      uint8_t from[14] = {0};
      from[0] = 6; from[12] = ipx_bound[12]; from[13] = ipx_bound[13];
      int32_t got = -1;
      pthread_mutex_lock(&lb_lock);
      if (lb_head != lb_tail) {
        uint32_t n = lb_q[lb_head].len > a[2] ? a[2] : lb_q[lb_head].len;
        memcpy(GPTR(a[1]), lb_q[lb_head].data, n);
        lb_head = (lb_head + 1) % LB_MAX;
        memcpy(from + 8, &local_ip, 4);
        if (!local_ip.s_addr) from[11] = 1;
        got = (int32_t)n;
      }
      pthread_mutex_unlock(&lb_lock);
      while (got < 0 && ipx_fd >= 0) {
        struct sockaddr_in sa; socklen_t sl = sizeof sa;
        ssize_t n = recvfrom(ipx_fd, GPTR(a[1]), a[2], 0, (struct sockaddr*)&sa, &sl);
        if (n < 0) break;
        if (sa.sin_addr.s_addr == local_ip.s_addr) continue;   // our own broadcast coming back: already delivered locally
        memcpy(from + 8, &sa.sin_addr, 4);
        got = (int32_t)n;
      }
      if (got >= 0) {
        if (a[4] && a[5]) { uint32_t fl = GU32(a[5]) < 14 ? GU32(a[5]) : 14; memcpy(GPTR(a[4]), from, fl); GU32(a[5]) = 14; }
        { static unsigned cnt; if (cnt++ < 60) DBG("net", "IPX recv %d bytes from node %d.%d.%d.%d", got, from[8], from[9], from[10], from[11]); }
        return (uint32_t)got;
      }
      usleep(4000);
    }
    wsa_err = 10035;   // WSAEWOULDBLOCK / timeout
    { static unsigned ec; if (ec++ < 20) DBG("net", "IPX recvfrom timeout (no packet)"); }
    return (uint32_t)-1;
  }
  socklen_t fl = a[5] ? GU32(a[5]) : 0;
  ssize_t n = recvfrom((int)a[0], GPTR(a[1]), a[2], (int)a[3], a[4] ? (struct sockaddr*)GPTR(a[4]) : NULL, a[5] ? &fl : NULL);
  if (a[5]) GU32(a[5]) = fl;
  if (n < 0) { wsa_err = map_errno(errno); { static unsigned ec; if (ec++ < 40 || ec % 100 == 0) DBG("net", "recvfrom s=%u failed: %s (WSA %d)", a[0], strerror(errno), wsa_err); } return (uint32_t)-1; }
  { static unsigned cnt; if (cnt++ < 60 || cnt % 200 == 0) DBG("net", "recvfrom s=%u -> %zd bytes first4=%08x", a[0], n, n >= 4 ? GU32(a[1]) : 0); }
  return (uint32_t)n;
}

SH(WSAIoctl) {   // s, code, in*, inlen, out*, outlen, *ret, overlapped, completion
  UNUSED;
  if (a[0] == ORACLE_UDP || a[0] == FAKE_IPX) { if (a[6]) GU32(a[6]) = 0; return 0; }
  uint32_t code = a[1];
  if (a[6]) GU32(a[6]) = 0;
  if (code == 0x8004667E) {   // FIONBIO
    int on = a[2] ? (int)GU32(a[2]) : 0;
    int fl = fcntl((int)a[0], F_GETFL, 0);
    fcntl((int)a[0], F_SETFL, on ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK));
    DBG("net", "WSAIoctl FIONBIO(s=%u, %d)", a[0], on);
    return 0;
  }
  DBG("net", "WSAIoctl code=%08x (not supported, reporting failure)", code);
  wsa_err = 10045;   // WSAEOPNOTSUPP
  return (uint32_t)-1;
}

#define W "WS2_32.dll"
void rt_register_winsock_shims(void) {
  static const Shim s[] = {
    {W, "WSAStartup", 2, ws_WSAStartup}, {W, "WSACleanup", 0, ws_WSACleanup}, {W, "WSAGetLastError", 0, ws_WSAGetLastError},
    {W, "getprotobyname", 1, ws_getprotobyname}, {W, "socket", 3, ws_socket}, {W, "setsockopt", 5, ws_setsockopt},
    {W, "gethostname", 2, ws_gethostname}, {W, "gethostbyname", 1, ws_gethostbyname}, {W, "bind", 3, ws_bind},
    {W, "getsockname", 3, ws_getsockname}, {W, "sendto", 6, ws_sendto}, {W, "recvfrom", 6, ws_recvfrom}, {W, "WSAIoctl", 9, ws_WSAIoctl},
  };
  rt_register_shims(s, (int)(sizeof s / sizeof *s));
}
uint32_t rt_winsock_module(void) { return WS_MODULE; }
