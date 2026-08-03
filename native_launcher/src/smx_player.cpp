// SMX player matrix P2P — raw TCP bidirectional binary exchange.
// Compatible framing with CubalC: [u32le len][bytes].
#define _POSIX_C_SOURCE 200809L
#include "smx_player.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

namespace {

constexpr uint32_t kMagic = 0x4D505647u; // 'GVPM' LE
constexpr uint16_t kProto = 1;
constexpr uint16_t kHoldFlash = 0x0001;

static uint64_t NowNs() {
  struct timespec ts {};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int SetNonblock(int fd) {
  int fl = fcntl(fd, F_GETFL, 0);
  if (fl < 0) return -1;
  return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static int HexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static int LoadKeyHex(SmxPeerState& s, const char* hex64) {
  if (!hex64 || std::strlen(hex64) < 64) return -1;
  for (int i = 0; i < 32; i++) {
    int hi = HexNibble(hex64[i * 2]), lo = HexNibble(hex64[i * 2 + 1]);
    if (hi < 0 || lo < 0) return -1;
    s.key[i] = (uint8_t)((hi << 4) | lo);
  }
  s.key_ok = 1;
  return 0;
}

// Lightweight FNV-1a for UI texture fingerprint (not crypto).
static uint32_t Fnv1a(const void* data, size_t n) {
  uint32_t h = 2166136261u;
  const uint8_t* p = (const uint8_t*)data;
  for (size_t i = 0; i < n; i++) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}

// Optional XOR stream from key for demo-grade privacy (not SMX2 HMAC full yet;
// full cubalc_smx_seal can wrap when linked). HOLD_FLASH still asserted.
static void XorKeystream(const uint8_t key[32], uint32_t seq, uint8_t* buf, size_t n) {
  // Expand key+seq into repeating stream
  uint8_t stream[32];
  for (int i = 0; i < 32; i++)
    stream[i] = key[i] ^ (uint8_t)(seq + i * 17);
  for (size_t i = 0; i < n; i++)
    buf[i] ^= stream[i % 32];
}

static bool ParseHostPort(const std::string& s, std::string& host, int& port) {
  auto colon = s.rfind(':');
  if (colon == std::string::npos) return false;
  host = s.substr(0, colon);
  port = std::atoi(s.c_str() + colon + 1);
  return !host.empty() && port > 0 && port < 65536;
}

static int TcpListen(const std::string& bindSpec) {
  std::string host;
  int port = 0;
  if (!ParseHostPort(bindSpec, host, port)) return -1;
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  int yes = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  if (host == "0.0.0.0" || host.empty())
    addr.sin_addr.s_addr = INADDR_ANY;
  else if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    close(fd);
    return -1;
  }
  if (::bind(fd, (sockaddr*)&addr, sizeof addr) < 0 || ::listen(fd, 1) < 0) {
    close(fd);
    return -1;
  }
  SetNonblock(fd);
  return fd;
}

static int TcpDial(const std::string& peer) {
  std::string host;
  int port = 0;
  if (!ParseHostPort(peer, host, port)) return -1;
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  SetNonblock(fd);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    // try getaddrinfo
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[16];
    std::snprintf(portStr, sizeof portStr, "%d", port);
    if (getaddrinfo(host.c_str(), portStr, &hints, &res) != 0 || !res) {
      close(fd);
      return -1;
    }
    addr = *(sockaddr_in*)res->ai_addr;
    freeaddrinfo(res);
  }
  int r = connect(fd, (sockaddr*)&addr, sizeof addr);
  if (r < 0 && errno != EINPROGRESS) {
    close(fd);
    return -1;
  }
  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
  return fd;
}

// Returns 1 ok, 0 would-block (keep fd), -1 hard fail
static int SendAll(int fd, const void* data, size_t n) {
  const uint8_t* p = (const uint8_t*)data;
  size_t off = 0;
  while (off < n) {
    ssize_t w = ::send(fd, p + off, n - off, MSG_NOSIGNAL);
    if (w < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
      return -1;
    }
    if (w == 0) return -1;
    off += (size_t)w;
  }
  return 1;
}

static bool RecvExact(int fd, void* data, size_t n) {
  uint8_t* p = (uint8_t*)data;
  size_t off = 0;
  while (off < n) {
    ssize_t r = ::recv(fd, p + off, n - off, 0);
    if (r < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) return false;
      return false;
    }
    if (r == 0) return false;
    off += (size_t)r;
  }
  return true;
}

// Non-blocking: try receive one full frame; return 1 ok, 0 would-block, -1 error
static int TryRecvFrame(int fd, uint8_t* out, size_t cap, size_t* nOut) {
  // Peek length first — if incomplete, leave for next pump
  uint32_t len = 0;
  ssize_t r = ::recv(fd, &len, sizeof len, MSG_PEEK);
  if (r < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    return -1;
  }
  if (r == 0) return -1;
  if ((size_t)r < sizeof len) return 0;
  if (len == 0 || len > cap || len > 65536) return -1;
  // Check enough data available
  uint8_t tmp[4 + 256];
  size_t need = 4 + len;
  if (need > sizeof tmp) return -1;
  r = ::recv(fd, tmp, need, MSG_PEEK);
  if (r < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    return -1;
  }
  if ((size_t)r < need) return 0;
  // Consume
  if (!RecvExact(fd, tmp, need)) return -1;
  std::memcpy(out, tmp + 4, len);
  if (nOut) *nOut = len;
  return 1;
}

// 1 ok, 0 skip (EAGAIN), -1 fail
static int SendFrame(int fd, const void* payload, uint32_t n) {
  uint8_t hdr[4];
  hdr[0] = (uint8_t)(n);
  hdr[1] = (uint8_t)(n >> 8);
  hdr[2] = (uint8_t)(n >> 16);
  hdr[3] = (uint8_t)(n >> 24);
  int r = SendAll(fd, hdr, 4);
  if (r != 1) return r;
  return SendAll(fd, payload, n);
}

} // namespace

void SmxFillPose(SmxPose3& out, float px, float py, float pz,
                 float qx, float qy, float qz, float qw) {
  out.px = px; out.py = py; out.pz = pz;
  out.qx = qx; out.qy = qy; out.qz = qz; out.qw = qw;
}

uint32_t SmxHashBytes(const void* data, size_t n) {
  return Fnv1a(data, n);
}

bool SmxEnabled(const SmxPeerState& s) {
  return s.listen_fd >= 0 || s.fd >= 0 || !s.peer_addr.empty() || !s.bind_addr.empty();
}

void SmxInit(SmxPeerState& s, const char* selfId) {
  s = {};
  s.tx_hz = 20;
  if (const char* hz = getenv("GVRMOD_SMX_HZ")) {
    int v = std::atoi(hz);
    if (v >= 1 && v <= 90) s.tx_hz = (uint32_t)v;
  }
  std::strncpy(s.self_id, selfId ? selfId : "gvrmod-player", sizeof s.self_id - 1);
  const char* key = getenv("GVRMOD_SMX_KEY");
  if (!key || !key[0]) key = getenv("CUBALC_SMX_KEY");
  if (key && key[0]) LoadKeyHex(s, key);
  const char* bind = getenv("GVRMOD_SMX_BIND");
  if (!bind || !bind[0]) bind = getenv("CUBALC_P2P_BIND");
  if (bind && bind[0]) s.bind_addr = bind;
  const char* peer = getenv("GVRMOD_SMX_PEER");
  if (!peer || !peer[0]) peer = getenv("CUBALC_P2P_PEER");
  if (peer && peer[0]) s.peer_addr = peer;

  if (!s.bind_addr.empty()) {
    s.listen_fd = TcpListen(s.bind_addr);
    if (s.listen_fd >= 0) {
      s.serving = true;
      fprintf(stderr, "[smx] SERVE %s (player matrix P2P)\n", s.bind_addr.c_str());
    } else {
      s.last_err = "listen_failed";
      fprintf(stderr, "[smx] SERVE failed %s errno=%d\n", s.bind_addr.c_str(), errno);
    }
  }
  if (!s.peer_addr.empty() && s.fd < 0) {
    s.fd = TcpDial(s.peer_addr);
    if (s.fd >= 0) {
      fprintf(stderr, "[smx] DIAL %s (pending connect)\n", s.peer_addr.c_str());
    } else {
      s.last_err = "dial_failed";
      fprintf(stderr, "[smx] DIAL failed %s\n", s.peer_addr.c_str());
    }
  }
  if (s.bind_addr.empty() && s.peer_addr.empty()) {
    fprintf(stderr, "[smx] idle (no bind/peer) — zero cost until GVRMOD_SMX_BIND/PEER set\n");
  } else {
    fprintf(stderr, "[smx] key=%s self=%s tx_hz=%u\n", s.key_ok ? "ok" : "none(open)", s.self_id,
            s.tx_hz);
  }
}

void SmxShutdown(SmxPeerState& s) {
  if (s.fd >= 0) close(s.fd);
  if (s.listen_fd >= 0) close(s.listen_fd);
  s.fd = s.listen_fd = -1;
  s.connected = false;
  s.serving = false;
}

void SmxPump(SmxPeerState& s, const SmxPlayerMatrix& local, SmxPlayerMatrix* peerOut) {
  // Fast path: no sockets configured and no live peer
  if (!SmxEnabled(s)) return;

  // Accept
  if (s.listen_fd >= 0 && s.fd < 0) {
    sockaddr_in ca{};
    socklen_t cl = sizeof ca;
    int c = accept(s.listen_fd, (sockaddr*)&ca, &cl);
    if (c >= 0) {
      SetNonblock(c);
      int one = 1;
      setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
      s.fd = c;
      s.connected = true;
      char ip[64];
      inet_ntop(AF_INET, &ca.sin_addr, ip, sizeof ip);
      fprintf(stderr, "[smx] peer accepted %s:%d\n", ip, ntohs(ca.sin_port));
    }
  }

  // Check dial progress
  if (s.fd >= 0 && !s.connected && !s.serving) {
    int err = 0;
    socklen_t el = sizeof err;
    if (getsockopt(s.fd, SOL_SOCKET, SO_ERROR, &err, &el) == 0 && err == 0) {
      s.connected = true;
      fprintf(stderr, "[smx] dial connected %s\n", s.peer_addr.c_str());
    } else if (err != 0 && err != EINPROGRESS) {
      close(s.fd);
      s.fd = -1;
      s.last_err = "connect_err";
      // retry later
      static int retryCd = 0;
      if (++retryCd > 120) {
        retryCd = 0;
        s.fd = TcpDial(s.peer_addr);
      }
    }
  }

  if (s.fd < 0 || !s.connected) {
    // retry dial periodically
    if (!s.peer_addr.empty() && s.fd < 0) {
      static int retry = 0;
      if (++retry > 180) {
        retry = 0;
        s.fd = TcpDial(s.peer_addr);
      }
    }
    return;
  }

  // Rate-limit TX (poses are ~20Hz enough for matrix bus; saves CPU vs 72/90fps)
  const uint64_t now = NowNs();
  const uint64_t minGap = 1000000000ull / (uint64_t)(s.tx_hz ? s.tx_hz : 20);
  const bool doTx = (s.last_tx_ns == 0 || now - s.last_tx_ns >= minGap);

  if (doTx) {
    SmxPlayerMatrix tx = local;
    tx.magic = kMagic;
    tx.proto = kProto;
    tx.flags = kHoldFlash;
    s.tx_seq++;
    if (s.tx_seq == 0) s.tx_seq = 1;
    tx.seq = s.tx_seq;
    tx.time_ns = now;
    if (tx.player_id[0] == 0)
      std::strncpy(tx.player_id, s.self_id, sizeof tx.player_id - 1);

    uint8_t wire[sizeof(SmxPlayerMatrix)];
    std::memcpy(wire, &tx, sizeof tx);
    if (s.key_ok)
      XorKeystream(s.key, tx.seq, wire, sizeof wire);

    int sr = SendFrame(s.fd, wire, (uint32_t)sizeof wire);
    if (sr < 0) {
      close(s.fd);
      s.fd = -1;
      s.connected = false;
      s.last_err = "send_fail";
      fprintf(stderr, "[smx] send fail — peer dropped\n");
      return;
    }
    if (sr == 1) s.last_tx_ns = now;
    // sr==0: EAGAIN — keep connection, skip this tick
  }

  // Recv at most one frame (non-blocking)
  uint8_t rxbuf[sizeof(SmxPlayerMatrix) + 64];
  size_t n = 0;
  int rr = TryRecvFrame(s.fd, rxbuf, sizeof rxbuf, &n);
  if (rr < 0) {
    close(s.fd);
    s.fd = -1;
    s.connected = false;
    s.last_err = "recv_fail";
    fprintf(stderr, "[smx] recv fail — peer dropped\n");
    return;
  }
  if (rr == 1 && n >= sizeof(SmxPlayerMatrix)) {
    if (s.key_ok) {
      uint32_t seqGuess = 0;
      std::memcpy(&seqGuess, rxbuf + 8, 4);
      XorKeystream(s.key, seqGuess, rxbuf, sizeof(SmxPlayerMatrix));
    }
    SmxPlayerMatrix rx{};
    std::memcpy(&rx, rxbuf, sizeof rx);
    if (rx.magic == kMagic && rx.proto == kProto && (rx.flags & kHoldFlash)) {
      if (rx.seq > s.rx_seq) {
        s.rx_seq = rx.seq;
        s.last_rx = rx;
        if (peerOut) *peerOut = rx;
      }
    }
  }
}
