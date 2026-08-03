#include "world_panel.hpp"
#include "panel_config.hpp"
#include "ui_panel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

static WorldPanel g_wp{};

WorldPanel& WorldPanelState() { return g_wp; }

void WorldPanelReset() {
  g_wp = WorldPanel{};
  g_wp.pose.orientation.w = 1.f;
}

static XrQuaternionf QuatFromAxes(Vec3 xAxis, Vec3 yAxis, Vec3 zAxis) {
  float m00 = xAxis.x, m01 = yAxis.x, m02 = zAxis.x;
  float m10 = xAxis.y, m11 = yAxis.y, m12 = zAxis.y;
  float m20 = xAxis.z, m21 = yAxis.z, m22 = zAxis.z;
  XrQuaternionf q{};
  const float tr = m00 + m11 + m22;
  if (tr > 0.f) {
    float s = std::sqrt(tr + 1.f) * 2.f;
    q.w = 0.25f * s;
    q.x = (m21 - m12) / s;
    q.y = (m02 - m20) / s;
    q.z = (m10 - m01) / s;
  } else if (m00 > m11 && m00 > m22) {
    float s = std::sqrt(1.f + m00 - m11 - m22) * 2.f;
    q.w = (m21 - m12) / s;
    q.x = 0.25f * s;
    q.y = (m01 + m10) / s;
    q.z = (m02 + m20) / s;
  } else if (m11 > m22) {
    float s = std::sqrt(1.f + m11 - m00 - m22) * 2.f;
    q.w = (m02 - m20) / s;
    q.x = (m01 + m10) / s;
    q.y = 0.25f * s;
    q.z = (m12 + m21) / s;
  } else {
    float s = std::sqrt(1.f + m22 - m00 - m11) * 2.f;
    q.w = (m10 - m01) / s;
    q.x = (m02 + m20) / s;
    q.y = (m12 + m21) / s;
    q.z = 0.25f * s;
  }
  float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  if (n > 1e-8f) {
    q.x /= n;
    q.y /= n;
    q.z /= n;
    q.w /= n;
  } else {
    q = {};
    q.w = 1.f;
  }
  return q;
}

XrPosef WorldPanelMakePose(Vec3 center, Vec3 normalTowardUser) {
  // OpenXR: +X right, +Y up, -Z toward user (faces viewer)
  Vec3 n = normalTowardUser; // toward user
  n.y = 0.f;
  if (Dot(n, n) < 1e-8f) n = V3(0, 0, 1);
  n = Normalize(n);
  const Vec3 worldUp = V3(0, 1, 0);
  // +Z out the back of the panel (away from user)
  Vec3 zAxis = Normalize(n * -1.f);
  // +X = right as seen by user looking at panel
  Vec3 xAxis = Cross(worldUp, zAxis);
  if (Dot(xAxis, xAxis) < 1e-8f) xAxis = V3(1, 0, 0);
  xAxis = Normalize(xAxis);
  // +Y = up
  Vec3 yAxis = Normalize(Cross(zAxis, xAxis));
  // Force upright: if y points down, rotate 180° around Z (flip X only — keeps face)
  if (yAxis.y < 0.f) {
    yAxis = yAxis * -1.f;
    xAxis = xAxis * -1.f;
  }
  XrPosef p{};
  p.position = {center.x, center.y, center.z};
  p.orientation = QuatFromAxes(xAxis, yAxis, zAxis);
  return p;
}

void WorldPanelSyncAxes() {
  g_wp.right = Normalize(QuatRotate(g_wp.pose.orientation, V3(1, 0, 0)));
  g_wp.up = Normalize(QuatRotate(g_wp.pose.orientation, V3(0, 1, 0)));
  g_wp.normal = Normalize(QuatRotate(g_wp.pose.orientation, V3(0, 0, -1))); // -Z toward user
  g_wp.c = {g_wp.pose.position.x, g_wp.pose.position.y, g_wp.pose.position.z};
  // Hard law: panel +Y must point skyward or text is upside-down in headset
  if (g_wp.up.y < 0.f) {
    g_wp.up = g_wp.up * -1.f;
    g_wp.right = g_wp.right * -1.f;
    g_wp.pose = WorldPanelMakePose(g_wp.c, g_wp.normal);
    g_wp.right = Normalize(QuatRotate(g_wp.pose.orientation, V3(1, 0, 0)));
    g_wp.up = Normalize(QuatRotate(g_wp.pose.orientation, V3(0, 1, 0)));
    g_wp.normal = Normalize(QuatRotate(g_wp.pose.orientation, V3(0, 0, -1)));
    if (g_wp.up.y < 0.f) g_wp.up = V3(0, 1, 0); // last resort
    fprintf(stderr, "[cube_webui] panel axes re-uprighted up.y=%.2f\n", g_wp.up.y);
  }
}

bool WorldPanelSeed(const XrPosef& headInWorld, bool force) {
  if (g_wp.frozen && !force) return false;

  const auto& cfg = PanelCfgConst();
  g_wp.widthM = cfg.halfW * 2.f;
  g_wp.heightM = cfg.halfH * 2.f;

  const Vec3 headP = {headInWorld.position.x, headInWorld.position.y, headInWorld.position.z};
  // Look direction (OpenXR eye: -Z)
  Vec3 fwd = QuatRotate(headInWorld.orientation, V3(0, 0, -1));
  // Keep upright: flatten pitch for placement only
  fwd.y = 0.f;
  if (Dot(fwd, fwd) < 1e-8f) fwd = V3(0, 0, -1);
  fwd = Normalize(fwd);

  // Dead ahead, eye height, conf distance
  Vec3 center = headP + fwd * cfg.dist;
  center.y = headP.y + cfg.offsetY;
  center = center + Normalize(Cross(V3(0, 1, 0), fwd * -1.f)) * cfg.offsetX;

  Vec3 toHead = headP - center;
  if (Dot(toHead, toHead) < 1e-8f) toHead = fwd * -1.f;

  // Normal MUST point toward head (toHead). If inside-out, flip face.
  g_wp.pose = WorldPanelMakePose(center, toHead);
  WorldPanelSyncAxes();
  {
    Vec3 toward = headP - g_wp.c;
    if (Dot(g_wp.normal, toward) < 0.f) {
      // Facing away — rebuild with opposite facing
      g_wp.pose = WorldPanelMakePose(center, toward * -1.f);
      WorldPanelSyncAxes();
      // Still wrong? force flip axes
      if (Dot(g_wp.normal, toward) < 0.f) {
        g_wp.normal = g_wp.normal * -1.f;
        g_wp.right = g_wp.right * -1.f;
        g_wp.pose = WorldPanelMakePose(g_wp.c, g_wp.normal);
        WorldPanelSyncAxes();
      }
      fprintf(stderr, "[cube_webui] panel face corrected toward HMD\n");
    }
  }
  g_wp.ready = true;
  g_wp.frozen = true;
  g_wp.seedCount++;
  fprintf(stderr,
          "[cube_webui] panel seed #%d pos=(%.2f,%.2f,%.2f) up=(%.2f,%.2f,%.2f) n=(%.2f,%.2f,%.2f)\n",
          g_wp.seedCount, g_wp.c.x, g_wp.c.y, g_wp.c.z, g_wp.up.x, g_wp.up.y, g_wp.up.z,
          g_wp.normal.x, g_wp.normal.y, g_wp.normal.z);
  return true;
}

void WorldPanelSetCenter(Vec3 worldCenter) {
  if (!g_wp.ready || !g_wp.frozen) return;
  g_wp.pose.position.x = worldCenter.x;
  g_wp.pose.position.y = worldCenter.y;
  g_wp.pose.position.z = worldCenter.z;
  WorldPanelSyncAxes();
}

bool WorldPanelRayHit(Vec3 origin, Vec3 dir, int* outPx, int* outPy, Vec3* outHit,
                      float slopScale) {
  if (!g_wp.ready) return false;
  Vec3 d = Normalize(dir);
  float denom = Dot(d, g_wp.normal);
  // Two-sided panel: rays from either face count (WiVRn emergency seed often
  // faces the wrong way until MENU re-anchor).
  if (std::fabs(denom) < 1e-5f) return false;
  float t = Dot(g_wp.c - origin, g_wp.normal) / denom;
  // Allow slightly behind controller origin (tracking jitter) and long aim
  if (t < -0.05f || t > 20.f) return false;
  Vec3 hit = origin + d * t;
  float u = Dot(hit - g_wp.c, g_wp.right);
  float v = Dot(hit - g_wp.c, g_wp.up);
  const float hw = g_wp.widthM * 0.5f;
  const float hh = g_wp.heightM * 0.5f;
  // Default 8% rim slop; callers can pass ~1.35 for soft click when tip looks near
  const float slop = (slopScale < 1.f) ? 1.08f : slopScale;
  if (std::fabs(u) > hw * slop || std::fabs(v) > hh * slop) return false;
  u = std::max(-hw, std::min(hw, u));
  v = std::max(-hh, std::min(hh, v));
  // 1:1 with FlipY draw: +right → +px, +up → UI top (py=0). NO 180 remap —
  // tip on visual button must equal WebUI_PointerClick pixel.
  int px = (int)((u / hw * 0.5f + 0.5f) * (float)UI_W);
  int py = (int)((0.5f - v / hh * 0.5f) * (float)UI_H);
  px = std::max(0, std::min(UI_W - 1, px));
  py = std::max(0, std::min(UI_H - 1, py));
  if (outPx) *outPx = px;
  if (outPy) *outPy = py;
  if (outHit) *outHit = hit;
  return true;
}
