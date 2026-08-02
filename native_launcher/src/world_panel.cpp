#include "world_panel.hpp"
#include "panel_config.hpp"
#include "ui_panel.hpp"

#include <algorithm>
#include <cstdio>

static WorldPanel g_wp{};

WorldPanel& WorldPanelState() { return g_wp; }

void WorldPanelReset() { g_wp = WorldPanel{}; }

// Room-vertical axes: yaw-only facing (ignore head pitch/roll) so panel is not HMD-tilted.
static void BuildAxesYawOnly(Vec3 headP, Vec3 center) {
  Vec3 toHead = headP - center;
  toHead.y = 0.f;
  if (Dot(toHead, toHead) < 1e-8f) toHead = V3(0, 0, 1);
  g_wp.normal = Normalize(toHead); // points toward head on XZ
  Vec3 worldUp = V3(0, 1, 0);
  g_wp.right = Normalize(Cross(worldUp, g_wp.normal));
  g_wp.up = Normalize(Cross(g_wp.normal, g_wp.right));
}

bool WorldPanelSeed(const XrPosef& headInWorld, bool force) {
  if (g_wp.frozen && !force) {
    // Hard law: never re-seed from HMD after freeze (would look like head-follow)
    static int warn = 0;
    if (warn++ < 3)
      fprintf(stderr, "[cube_webui] PANEL seed REJECTED (frozen) — not head-following\n");
    return false;
  }

  const auto& cfg = PanelCfgConst();
  Vec3 headP = {headInWorld.position.x, headInWorld.position.y, headInWorld.position.z};
  // Yaw-only forward in world XZ (OpenXR -Z is forward in view, map to world via quat)
  Vec3 fwd = QuatRotate(headInWorld.orientation, V3(0, 0, -1));
  fwd.y = 0.f;
  if (Dot(fwd, fwd) < 1e-8f) fwd = V3(0, 0, -1);
  fwd = Normalize(fwd);
  Vec3 headR = Normalize(Cross(V3(0, 1, 0), fwd * -1.f)); // right from yaw forward
  if (Dot(headR, headR) < 1e-8f) headR = V3(1, 0, 0);
  // Place once: world position from head at THIS moment, then freeze forever
  g_wp.c = headP + fwd * (cfg.dist + cfg.offsetZ) + headR * cfg.offsetX + V3(0, 1, 0) * cfg.offsetY;
  BuildAxesYawOnly(headP, g_wp.c);
  g_wp.ready = true;
  g_wp.frozen = true;
  g_wp.seedCount++;
  fprintf(stderr,
          "[cube_webui] PANEL ANCHOR #%d c=(%.3f,%.3f,%.3f) FROZEN world (force=%d)\n",
          g_wp.seedCount, g_wp.c.x, g_wp.c.y, g_wp.c.z, force ? 1 : 0);
  return true;
}

void WorldPanelTranslate(Vec3 delta) {
  if (!g_wp.ready || !g_wp.frozen) return;
  g_wp.c = g_wp.c + delta;
}

bool WorldPanelRayHit(Vec3 origin, Vec3 dir, int* outPx, int* outPy, Vec3* outHit) {
  if (!g_wp.ready) return false;
  const auto& cfg = PanelCfgConst();
  Vec3 d = Normalize(dir);
  float denom = Dot(d, g_wp.normal);
  if (std::fabs(denom) < 1e-5f) return false;
  float t = Dot(g_wp.c - origin, g_wp.normal) / denom;
  if (t < 0.02f || t > 10.f) return false;
  Vec3 hit = origin + d * t;
  float u = Dot(hit - g_wp.c, g_wp.right);
  float v = Dot(hit - g_wp.c, g_wp.up);
  const float hw = cfg.halfW, hh = cfg.halfH;
  const float margin = 1.08f;
  if (std::fabs(u) > hw * margin || std::fabs(v) > hh * margin) return false;
  float cu = std::max(-hw, std::min(hw, u));
  float cv = std::max(-hh, std::min(hh, v));
  int px = (int)((cu / hw * 0.5f + 0.5f) * (float)UI_W);
  int py = (int)((0.5f - cv / hh * 0.5f) * (float)UI_H);
  px = std::max(0, std::min(UI_W - 1, px));
  py = std::max(0, std::min(UI_H - 1, py));
  if (outPx) *outPx = px;
  if (outPy) *outPy = py;
  if (outHit) *outHit = hit;
  return true;
}
