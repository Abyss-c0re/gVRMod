#pragma once
// Port of addon/vrmod-x64/lua/vrmod/utils/sh_collisions.lua
// Last-free hull sweep. Proxies never solid to world. Floor/ceiling do not lock.
#include "vec3.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <functional>
#include <string>

namespace cssvr {

constexpr float kDefaultRadius = 2.2f;
constexpr float kDefaultReach = 5.0f;
constexpr float kMinBoxSize = kDefaultRadius * 0.5f; // 1.1
constexpr float kWallReleaseFromHmdSqr = 100.f * 100.f;
constexpr float kMaxLastFreeDistSqr = 48.f * 48.f;
constexpr float kMaxHandCorrection = 40.f;
constexpr float kMinHandCorrectionSqr = 0.35f * 0.35f;

struct Aabb {
  Vec3 mins;
  Vec3 maxs;
};

struct HullBoxes {
  Aabb horizontal;
  Aabb vertical;
  float ex = 0, ey = 0, ez = 0;
};

inline void EnforceMinBoxSize(Aabb& b) {
  b.mins.x = std::min(b.mins.x, -kMinBoxSize);
  b.mins.y = std::min(b.mins.y, -kMinBoxSize);
  b.mins.z = std::min(b.mins.z, -kMinBoxSize);
  b.maxs.x = std::max(b.maxs.x, kMinBoxSize);
  b.maxs.y = std::max(b.maxs.y, kMinBoxSize);
  b.maxs.z = std::max(b.maxs.z, kMinBoxSize);
}

inline HullBoxes BoxesFromAABB(Vec3 amin, Vec3 amax) {
  HullBoxes o;
  o.ex = (amax.x - amin.x) * 0.5f;
  o.ey = (amax.y - amin.y) * 0.5f;
  o.ez = (amax.z - amin.z) * 0.5f;
  o.horizontal.mins = {-o.ex * 0.8f, -o.ey * 0.35f, -o.ez * 0.35f};
  o.horizontal.maxs = {o.ex * 0.8f, o.ey * 0.35f, o.ez * 0.35f};
  o.vertical.mins = {-o.ez * 0.35f, -o.ey * 0.35f, -o.ex};
  o.vertical.maxs = {o.ez * 0.35f, o.ey * 0.35f, o.ex};
  EnforceMinBoxSize(o.horizontal);
  EnforceMinBoxSize(o.vertical);
  return o;
}

inline bool ModelNameLooksMelee(const char* modelPath) {
  if (!modelPath) return false;
  std::string lower(modelPath);
  for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  static const char* kHints[] = {"crowbar", "knife", "melee", "bat", "katana",
                                 "sword", "axe", "machete", "club", nullptr};
  for (int i = 0; kHints[i]; ++i) {
    if (lower.find(kHints[i]) != std::string::npos) return true;
  }
  return false;
}

inline bool DetectMeleeFromAABB(const char* modelPath, Vec3 amin, Vec3 amax) {
  if (ModelNameLooksMelee(modelPath)) return true;
  const float sizeX = amax.x - amin.x;
  const float sizeY = amax.y - amin.y;
  const float sizeZ = amax.z - amin.z;
  const float longest = std::max(sizeX, std::max(sizeY, sizeZ));
  const float shortest = std::min(sizeX, std::min(sizeY, sizeZ));
  if (shortest < 0.01f) return false;
  return sizeZ == longest && (longest / shortest) >= 4.5f;
}

inline Vec3 AdjustCollisionsBox(Vec3 pos, const Ang3& ang, bool isMelee) {
  Vec3 fwd, right, up;
  AngleVectors(ang, &fwd, &right, &up);
  const float forwardOffset = isMelee ? 3.f : 10.f;
  const float leftOffset = isMelee ? 1.f : 1.5f;
  const float upOffset = 4.f;
  return pos + fwd * forwardOffset - right * leftOffset + up * upOffset;
}

inline bool IsFloorOrCeilingNormal(const Vec3& n, float thresh = 0.55f) {
  return std::fabs(n.z) > thresh;
}

inline Vec3 WallRestPos(const Vec3& hitPos, Vec3 n, float pad, const Vec3& fallback) {
  if (n.LengthSqr() < 0.01f) n = {0, 0, 1};
  return hitPos + n * pad;
}

struct TraceHit {
  bool hit = false;
  bool start_solid = false;
  bool all_solid = false;
  bool hit_world = false;
  float fraction = 1.f;
  Vec3 start_pos;
  Vec3 end_pos;
  Vec3 hit_pos;
  Vec3 hit_normal{0, 0, 1};
};

using TraceFn = std::function<TraceHit(Vec3 start, Vec3 end, Vec3 mins, Vec3 maxs)>;

inline Aabb SymmetricHull(float radius) {
  return {{-radius, -radius, -radius}, {radius, radius, radius}};
}

struct WallState {
  Vec3 last_free;
  bool has_free = false;
};

struct WallResolve {
  Vec3 pos;
  bool clipped = false;
  Vec3 normal{0, 0, 1};
  const char* reason = "free";
};

inline bool TraceIsFree(const TraceHit& t) { return !(t.start_solid || t.all_solid); }

/// Last-free hull sweep (Lua ResolveHandWallSweep). Never feed corrected poses back as desired.
inline WallResolve ResolveHandWallSweep(Vec3 desired, WallState& st, float radius, float pad,
                                        const TraceFn& trace) {
  WallResolve out;
  out.pos = desired;
  const Aabb hull = SymmetricHull(radius);
  auto isFree = [&](const Vec3& pos) {
    TraceHit t = trace(pos, pos, hull.mins, hull.maxs);
    return TraceIsFree(t);
  };

  const bool desiredFree = isFree(desired);
  bool lastFreeOk = false;
  if (st.has_free) {
    lastFreeOk = isFree(st.last_free);
    if (!lastFreeOk) {
      st.has_free = false;
    } else if (desiredFree && st.last_free.DistToSqr(desired) > kMaxLastFreeDistSqr) {
      st.has_free = false;
      lastFreeOk = false;
    }
  }

  if (desiredFree && !st.has_free) {
    out.reason = "desired_free";
    return out;
  }

  Vec3 startPos = desired;
  if (st.has_free && lastFreeOk) startPos = st.last_free;
  else st.has_free = false;

  if (desiredFree && st.has_free) {
    TraceHit clear = trace(startPos, desired, hull.mins, hull.maxs);
    if (!clear.hit && TraceIsFree(clear)) {
      out.reason = "path_clear";
      return out;
    }
  }

  TraceHit tr = trace(startPos, desired, hull.mins, hull.maxs);
  if (tr.start_solid || tr.all_solid) {
    out.clipped = true;
    out.normal = tr.hit_normal.LengthSqr() > 0.01f ? tr.hit_normal : Vec3{0, 0, 1};
    if (st.has_free && isFree(st.last_free)) {
      out.pos = st.last_free;
      out.reason = "start_solid_last_free";
      return out;
    }
    out.pos = desired + out.normal * (radius + pad);
    out.reason = "start_solid_depen";
    return out;
  }

  if (tr.hit) {
    Vec3 n = tr.hit_normal.LengthSqr() > 0.01f ? tr.hit_normal : Vec3{0, 0, 1};
    if (IsFloorOrCeilingNormal(n)) {
      if (desiredFree) {
        out.reason = "floor_free";
        return out;
      }
      out.clipped = true;
      out.normal = n;
      out.pos = desired + n * (radius + pad);
      out.reason = "floor_solid_depen";
      return out;
    }
    const float restPad = std::max(0.15f, pad);
    Vec3 rest = WallRestPos(tr.hit_pos, n, restPad, startPos);
    if (!isFree(rest)) {
      out.clipped = true;
      out.normal = n;
      if (st.has_free && isFree(st.last_free)) {
        out.pos = st.last_free;
        out.reason = "rest_solid_last_free";
        return out;
      }
      out.pos = startPos;
      out.reason = "rest_solid";
      return out;
    }
    out.clipped = true;
    out.normal = n;
    out.pos = rest;
    out.reason = "wall_rest";
    return out;
  }

  if (!desiredFree) {
    out.clipped = true;
    if (st.has_free && isFree(st.last_free)) {
      out.pos = st.last_free;
      out.reason = "desired_solid_last_free";
      return out;
    }
    out.pos = desired + Vec3{0, 0, 1} * (radius + pad);
    out.reason = "desired_solid";
    return out;
  }

  out.reason = "no_hit";
  return out;
}

struct WeaponTipResolve {
  Vec3 hand_pos;
  bool clipped = false;
};

/// Barrel tip must not pass through a wall (Lua ApplyWeaponWallToHand tip ray).
inline WeaponTipResolve ApplyWeaponTip(Vec3 handPos, const Ang3& handAng, float tipLen, float pad,
                                       const TraceFn& trace) {
  WeaponTipResolve o;
  o.hand_pos = handPos;
  Vec3 fwd, right, up;
  AngleVectors(handAng, &fwd, &right, &up);
  const float len = std::max(tipLen, 18.f);
  Vec3 tipEnd = handPos + fwd * len + up * 1.5f;
  Aabb line = {{0, 0, 0}, {0, 0, 0}};
  TraceHit t = trace(handPos, tipEnd, line.mins, line.maxs);
  if (t.hit && !t.start_solid && t.fraction < 0.995f) {
    Vec3 n = t.hit_normal.LengthSqr() > 0.01f ? t.hit_normal : Vec3{0, 0, 1};
    Vec3 tipDelta = (t.hit_pos - fwd * 0.75f + n * pad) - tipEnd;
    const float plen = tipDelta.LengthSqr();
    if (plen > 0.0001f && plen < (48.f * 48.f)) {
      o.hand_pos = handPos + tipDelta;
      o.clipped = true;
    }
  }
  return o;
}

inline bool ShouldReleaseWallLock(const Vec3& safePos, const Vec3& hmdPos) {
  return safePos.DistToSqr(hmdPos) > kWallReleaseFromHmdSqr;
}

} // namespace cssvr
