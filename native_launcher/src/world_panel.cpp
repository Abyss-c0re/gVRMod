#include "world_panel.hpp"
#include "panel_config.hpp"
#include "ui_panel.hpp" // UI_W, UI_H

#include <algorithm>
#include <cstdio>

static WorldPanel g_wp{};

WorldPanel& WorldPanelState() { return g_wp; }

void WorldPanelReset() { g_wp = WorldPanel{}; }

void WorldPanelSeed(const XrPosef& headLocal) {
  const auto& cfg = PanelCfgConst();
  Vec3 headP = {headLocal.position.x, headLocal.position.y, headLocal.position.z};
  Vec3 fwd = Normalize(QuatRotate(headLocal.orientation, V3(0, 0, -1)));
  Vec3 headR = Normalize(QuatRotate(headLocal.orientation, V3(1, 0, 0)));
  Vec3 headU = Normalize(QuatRotate(headLocal.orientation, V3(0, 1, 0)));
  g_wp.c = headP + fwd * cfg.dist + headR * cfg.offsetX + headU * cfg.offsetY + fwd * cfg.offsetZ;
  g_wp.normal = Normalize(headP - g_wp.c);
  Vec3 worldUp = V3(0, 1, 0);
  g_wp.right = Cross(worldUp, g_wp.normal);
  if (Dot(g_wp.right, g_wp.right) < 1e-8f)
    g_wp.right = Normalize(Cross(headU, g_wp.normal));
  else
    g_wp.right = Normalize(g_wp.right);
  g_wp.up = Normalize(Cross(g_wp.normal, g_wp.right));
  g_wp.ready = true;
  fprintf(stderr, "[cube_webui] world panel seeded LOCAL c=(%.2f,%.2f,%.2f)\n",
          g_wp.c.x, g_wp.c.y, g_wp.c.z);
}

void WorldPanelReface(const XrPosef& headLocal) {
  if (!g_wp.ready) return;
  Vec3 headP = {headLocal.position.x, headLocal.position.y, headLocal.position.z};
  g_wp.normal = Normalize(headP - g_wp.c);
  Vec3 worldUp = V3(0, 1, 0);
  g_wp.right = Cross(worldUp, g_wp.normal);
  if (Dot(g_wp.right, g_wp.right) < 1e-8f)
    g_wp.right = V3(1, 0, 0);
  else
    g_wp.right = Normalize(g_wp.right);
  g_wp.up = Normalize(Cross(g_wp.normal, g_wp.right));
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
