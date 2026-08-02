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

// Rotation matrix columns = +X +Y +Z of pose; OpenGL/OpenXR column-major quat.
static XrQuaternionf QuatFromAxes(Vec3 xAxis, Vec3 yAxis, Vec3 zAxis) {
  // R = [x y z] as columns
  float m00 = xAxis.x, m01 = yAxis.x, m02 = zAxis.x;
  float m10 = xAxis.y, m11 = yAxis.y, m12 = zAxis.y;
  float m20 = xAxis.z, m21 = yAxis.z, m22 = zAxis.z;
  XrQuaternionf q{};
  float tr = m00 + m11 + m22;
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
  Vec3 n = normalTowardUser;
  n.y = 0.f; // keep panel upright in room
  if (Dot(n, n) < 1e-8f) n = V3(0, 0, 1);
  n = Normalize(n);
  Vec3 worldUp = V3(0, 1, 0);
  Vec3 right = Cross(worldUp, n); // +X
  if (Dot(right, right) < 1e-8f) right = V3(1, 0, 0);
  right = Normalize(right);
  Vec3 up = Normalize(Cross(n, right)); // +Y
  // OpenXR quad: +X right, +Y up, -Z faces user → +Z = -normal
  Vec3 zAxis = n * -1.f;
  XrPosef p{};
  p.position = {center.x, center.y, center.z};
  p.orientation = QuatFromAxes(right, up, zAxis);
  return p;
}

void WorldPanelSyncAxes() {
  // Extract axes from pose orientation
  Vec3 x = QuatRotate(g_wp.pose.orientation, V3(1, 0, 0));
  Vec3 y = QuatRotate(g_wp.pose.orientation, V3(0, 1, 0));
  Vec3 z = QuatRotate(g_wp.pose.orientation, V3(0, 0, 1));
  g_wp.right = Normalize(x);
  g_wp.up = Normalize(y);
  g_wp.normal = Normalize(z * -1.f); // -Z faces user
  g_wp.c = {g_wp.pose.position.x, g_wp.pose.position.y, g_wp.pose.position.z};
}

bool WorldPanelSeed(const XrPosef& headInWorld, bool force) {
  if (g_wp.frozen && !force) {
    static int w = 0;
    if (w++ < 2)
      fprintf(stderr, "[cube_webui] seed blocked (static frozen) — grab/MENU only\n");
    return false;
  }
  const auto& cfg = PanelCfgConst();
  g_wp.widthM = cfg.halfW * 2.f;
  g_wp.heightM = cfg.halfH * 2.f;

  Vec3 headP = {headInWorld.position.x, headInWorld.position.y, headInWorld.position.z};
  // Horizontal look direction only (room place, not head pitch)
  Vec3 fwd = QuatRotate(headInWorld.orientation, V3(0, 0, -1));
  fwd.y = 0.f;
  if (Dot(fwd, fwd) < 1e-8f) fwd = V3(0, 0, -1);
  fwd = Normalize(fwd);

  Vec3 center = headP + fwd * (cfg.dist + cfg.offsetZ) + V3(0, 1, 0) * cfg.offsetY;
  // lateral offset along yaw-right
  Vec3 right = Normalize(Cross(V3(0, 1, 0), fwd * -1.f));
  if (Dot(right, right) > 1e-8f) center = center + right * cfg.offsetX;

  // Panel faces user: normal from panel toward head ≈ -fwd
  Vec3 normalTowardUser = fwd * -1.f; // if fwd is where head looks, panel is along fwd, faces back at head with normal -fwd? 
  // head looks along fwd; panel sits at head+fwd*dist; surface faces head so normal = -fwd (from panel to head is -fwd? 
  // from panel center to head = headP - center ≈ -fwd*dist, so normal toward user = Normalize(headP-center) = -fwd
  normalTowardUser = Normalize(headP - center);

  g_wp.pose = WorldPanelMakePose(center, normalTowardUser);
  WorldPanelSyncAxes();
  g_wp.ready = true;
  g_wp.frozen = true;
  g_wp.seedCount++;
  fprintf(stderr,
          "[cube_webui] STATIC panel #%d pos=(%.2f,%.2f,%.2f) size=%.2fx%.2f FROZEN (force=%d)\n",
          g_wp.seedCount, g_wp.c.x, g_wp.c.y, g_wp.c.z, g_wp.widthM, g_wp.heightM, force ? 1 : 0);
  return true;
}

void WorldPanelSetCenter(Vec3 worldCenter) {
  if (!g_wp.ready || !g_wp.frozen) return;
  g_wp.pose.position.x = worldCenter.x;
  g_wp.pose.position.y = worldCenter.y;
  g_wp.pose.position.z = worldCenter.z;
  WorldPanelSyncAxes();
}

bool WorldPanelRayHit(Vec3 origin, Vec3 dir, int* outPx, int* outPy, Vec3* outHit) {
  if (!g_wp.ready) return false;
  Vec3 d = Normalize(dir);
  float denom = Dot(d, g_wp.normal);
  if (std::fabs(denom) < 1e-5f) return false;
  float t = Dot(g_wp.c - origin, g_wp.normal) / denom;
  if (t < 0.02f || t > 12.f) return false;
  Vec3 hit = origin + d * t;
  float u = Dot(hit - g_wp.c, g_wp.right);
  float v = Dot(hit - g_wp.c, g_wp.up);
  const float hw = g_wp.widthM * 0.5f;
  const float hh = g_wp.heightM * 0.5f;
  const float margin = 1.05f;
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
