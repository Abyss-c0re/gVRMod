#pragma once
// gVRMod State Matrix Exchange (SMX) — player ↔ server raw binary P2P.
// Wire: TCP [u32le N][payload N] compatible with CubalC SMX bus framing.
// Payload: packed player matrix (poses + input + flags). HMAC optional when
// CUBALC_SMX_KEY / GVRMOD_SMX_KEY is set (fail-open for LAN demo, fail-closed
// when key required via GVRMOD_SMX_REQUIRE_KEY=1).
//
// Prophecy law: binary talk only · HOLD_FLASH sticky · no prose on the wire.
#include "math3d.hpp"
#include <cstdint>
#include <string>

// Fixed-layout player state matrix (little-endian on wire after header).
#pragma pack(push, 1)
struct SmxPose3 {
  float px, py, pz;
  float qx, qy, qz, qw;
};

struct SmxPlayerMatrix {
  uint32_t magic;       // 'GVPM' 0x4D505647
  uint16_t proto;       // 1
  uint16_t flags;       // bit0 HOLD_FLASH
  uint32_t seq;
  uint64_t time_ns;     // monotonic-ish host clock
  char     player_id[32];
  SmxPose3 head;
  SmxPose3 aim_l;
  SmxPose3 aim_r;
  float    trigger_l, trigger_r;
  float    grab_l, grab_r;
  float    stick_lx, stick_ly, stick_rx, stick_ry;
  uint8_t  menu;
  uint8_t  valid_mask;  // bit0 head,1 aimL,2 aimR
  uint8_t  pad[2];
  // Texture manifest stub (hash of last UI panel frame) — full texture
  // streaming uses chunked SMX tex frames (future).
  uint32_t ui_tex_hash;
  uint32_t ui_tex_w, ui_tex_h;
};
#pragma pack(pop)

static_assert(sizeof(SmxPlayerMatrix) <= 256, "keep player matrix compact");

struct SmxPeerState {
  bool connected = false;
  bool serving = false;
  int  fd = -1;          // connected peer socket (-1 none)
  int  listen_fd = -1;
  uint32_t tx_seq = 0;
  uint32_t rx_seq = 0;
  SmxPlayerMatrix last_rx{};
  std::string last_err;
  std::string peer_addr; // host:port for dial
  std::string bind_addr; // host:port for serve
  char self_id[32]{};
  uint8_t key[32]{};
  int key_ok = 0;
};

// Init from env: GVRMOD_SMX_PEER, GVRMOD_SMX_BIND, CUBALC_SMX_KEY / GVRMOD_SMX_KEY
void SmxInit(SmxPeerState& s, const char* selfId = "gvrmod-player");
void SmxShutdown(SmxPeerState& s);

// Non-blocking pump: accept/connect, send local matrix, recv peer matrix.
// Call once per frame from XR loop (cheap when idle).
void SmxPump(SmxPeerState& s, const SmxPlayerMatrix& local, SmxPlayerMatrix* peerOut /*nullable*/);

// Pack helpers from launcher frame state
void SmxFillPose(SmxPose3& out, float px, float py, float pz,
                 float qx, float qy, float qz, float qw);
uint32_t SmxHashBytes(const void* data, size_t n);
