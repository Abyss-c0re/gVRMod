// smx_proto — standalone State Matrix Exchange lab tool.
// Does NOT link into CubeUI / gmcl_vrmod_*. Build only this target.
//
// SERVE (listen):
//   GVRMOD_SMX_BIND=0.0.0.0:17740 ./smx_proto
// DIAL (client):
//   GVRMOD_SMX_PEER=127.0.0.1:17740 ./smx_proto
// Both sides can set GVRMOD_SMX_HZ (default 20) and GVRMOD_SMX_KEY.
//
// Synthetic matrix: head orbit + sticks so you can verify wire without OpenXR.

#include "smx_player.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

static void FillSynthetic(SmxPlayerMatrix& m, uint32_t tick, const char* id) {
  std::memset(&m, 0, sizeof m);
  std::strncpy(m.player_id, id, sizeof m.player_id - 1);
  m.valid_mask = 7; // head + both aims
  const float t = tick * 0.05f;
  const float cx = std::cos(t), sx = std::sin(t);
  SmxFillPose(m.head, cx * 0.3f, 1.6f, sx * 0.3f, 0.f, 0.f, 0.f, 1.f);
  SmxFillPose(m.aim_l, -0.2f, 1.2f, -0.4f, 0.f, 0.f, 0.f, 1.f);
  SmxFillPose(m.aim_r, 0.2f, 1.2f, -0.4f, 0.f, 0.f, 0.f, 1.f);
  m.trigger_l = 0.5f + 0.5f * sx;
  m.trigger_r = 0.5f + 0.5f * cx;
  m.stick_lx = sx;
  m.stick_ly = cx;
  m.ui_tex_w = 1280;
  m.ui_tex_h = 720;
  m.ui_tex_hash = tick * 2654435761u;
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  fprintf(stderr,
          "smx_proto — State Matrix Exchange LAB (not product)\n"
          "  SERVE: GVRMOD_SMX_BIND=0.0.0.0:17740 %s\n"
          "  DIAL:  GVRMOD_SMX_PEER=127.0.0.1:17740 %s\n"
          "  idle:  no env → exits after note (won't touch product)\n",
          argv[0], argv[0]);

  SmxPeerState smx{};
  SmxInit(smx, "smx-proto");
  if (!SmxEnabled(smx)) {
    fprintf(stderr, "smx_proto: no GVRMOD_SMX_BIND/PEER — nothing to do. Exit 0.\n");
    SmxShutdown(smx);
    return 0;
  }

  uint32_t tick = 0;
  uint32_t last_log = 0;
  while (true) {
    SmxPlayerMatrix local{};
    FillSynthetic(local, tick++, smx.self_id);
    SmxPlayerMatrix peer{};
    SmxPump(smx, local, &peer);

    if (tick - last_log >= 20) {
      last_log = tick;
      fprintf(stderr,
              "[smx_proto] tx_seq=%u connected=%d peer_seq=%u peer_id=%.16s head=(%.2f,%.2f,%.2f) err=%s\n",
              smx.tx_seq, smx.connected ? 1 : 0, peer.seq, peer.player_id,
              peer.head.px, peer.head.py, peer.head.pz,
              smx.last_err.empty() ? "-" : smx.last_err.c_str());
    }
    // ~50 Hz loop; SmxPump rate-limits TX to GVRMOD_SMX_HZ
    usleep(20000);
  }
  SmxShutdown(smx);
  return 0;
}
