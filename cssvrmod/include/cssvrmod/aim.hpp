#pragma once
// Port of sh_weps.lua gun-slave + cl_laser_pointer.lua muzzle + sh_laser_law.lua.
// Gun is a slave of the primary hand. Laser only from that hand. Bullet dir = gun forward.
#include "vec3.hpp"
#include <cstring>
#include <string>

namespace cssvr {

struct WeaponOffset {
  Vec3 pos;
  Ang3 ang;
  float muzzle_len = 12.f;
  bool wrong_muzzle = false;
};

struct GunPose {
  Vec3 pos;
  Ang3 ang;
  Vec3 forward;
  Vec3 muzzle;
  bool valid = false;
};

inline GunPose GunFromHand(const Pose& hand, const WeaponOffset& off) {
  GunPose g;
  if (!hand.valid) return g;
  Vec3 fwd, right, up;
  AngleVectors(hand.ang, &fwd, &right, &up);
  g.pos = hand.pos + fwd * off.pos.x + right * off.pos.y + up * off.pos.z;
  g.ang = hand.ang;
  g.ang.p += off.ang.p;
  g.ang.y += off.ang.y;
  g.ang.r += off.ang.r;
  g.forward = Forward(g.ang);
  if (off.wrong_muzzle) g.forward = Forward(hand.ang);
  g.muzzle = g.pos + g.forward * off.muzzle_len;
  g.valid = true;
  return g;
}

struct AimRay {
  Vec3 start;
  Vec3 dir;
  bool valid = false;
  bool used_attachment = false;
};

/// Muzzle + aim. Start must stay near hand/gun (no free-floating beam).
inline AimRay ResolveMuzzle(const GunPose& gun, const Pose& hand, const Vec3* att_pos = nullptr,
                            float att_max_dist = 80.f) {
  AimRay r;
  if (!gun.valid || !hand.valid) return r;
  if (att_pos && att_pos->DistToSqr(gun.pos) < att_max_dist * att_max_dist) {
    r.start = *att_pos;
    r.used_attachment = true;
  } else {
    r.start = gun.muzzle;
  }
  r.dir = gun.forward.Normalized();
  if (r.start.DistToSqr(hand.pos) > (120.f * 120.f)) {
    r.start = gun.pos + gun.forward * 10.f;
    r.dir = gun.forward.Normalized();
    r.used_attachment = false;
  }
  r.valid = true;
  return r;
}

inline const char* Laser_PrimaryHandFromInt(int v) { return (v == 1) ? "left" : "right"; }
inline const char* Laser_SecondaryHand(const char* primary) {
  return (primary && std::strcmp(primary, "left") == 0) ? "right" : "left";
}
inline bool Laser_AllowFromHand(const char* handName, const char* primaryHand) {
  if (!handName || !primaryHand) return false;
  if (std::strcmp(handName, "left") != 0 && std::strcmp(handName, "right") != 0) return false;
  return std::strcmp(handName, primaryHand) == 0;
}
inline bool Laser_IsMenuPrimaryClick(const char* action, const char* primaryHand) {
  if (!action) return false;
  if (std::strcmp(action, "boolean_car_mouse_left") == 0) return true;
  if (primaryHand && std::strcmp(primaryHand, "left") == 0)
    return std::strcmp(action, "boolean_left_primaryfire") == 0;
  return std::strcmp(action, "boolean_primaryfire") == 0;
}
inline bool Laser_IsWrongHandPrimaryClick(const char* action, const char* primaryHand) {
  if (!action) return false;
  if (std::strcmp(action, "boolean_car_mouse_left") == 0) return false;
  if (primaryHand && std::strcmp(primaryHand, "left") == 0)
    return std::strcmp(action, "boolean_primaryfire") == 0;
  return std::strcmp(action, "boolean_left_primaryfire") == 0;
}

struct LaserOpts {
  bool vr_active = false;
  bool laser_on = false;
  bool has_primary_pose = false;
  bool menu_focus = false;
  const char* primary_hand = "right";
  int primary_hand_int = -1;
  const char* laser_hand = nullptr;
  const char* click_action = nullptr;
  bool dual_laser = false;
};

struct LaserDecision {
  bool valid = true;
  const char* primary_hand = "right";
  const char* secondary_hand = "left";
  const char* laser_hand = "right";
  bool allow_laser = true;
  bool click_steal = false;
  bool menu_primary_click = false;
  bool dual_laser = false;
  bool vr_active = false;
  bool laser_on = false;
  bool has_primary_pose = false;
  bool menu_focus = false;
  const char* risk = "none";
  const char* reason = "ok";
  bool path_ok = true;
};

inline LaserDecision Laser_Decide(LaserOpts opts) {
  LaserDecision d;
  const char* primary = opts.primary_hand;
  if (opts.primary_hand_int >= 0) primary = Laser_PrimaryHandFromInt(opts.primary_hand_int);
  if (!primary || (std::strcmp(primary, "left") != 0 && std::strcmp(primary, "right") != 0))
    primary = "right";
  d.primary_hand = primary;
  d.secondary_hand = Laser_SecondaryHand(primary);
  d.laser_hand = opts.laser_hand ? opts.laser_hand : primary;
  d.allow_laser = Laser_AllowFromHand(d.laser_hand, primary);
  if (opts.click_action) d.click_steal = Laser_IsWrongHandPrimaryClick(opts.click_action, primary);
  d.menu_primary_click =
      opts.click_action && Laser_IsMenuPrimaryClick(opts.click_action, primary);
  d.dual_laser = opts.dual_laser;
  d.vr_active = opts.vr_active;
  d.laser_on = opts.laser_on;
  d.has_primary_pose = opts.has_primary_pose;
  d.menu_focus = opts.menu_focus;
  if (!d.vr_active) {
    d.risk = "idle";
    d.reason = "vr_inactive";
  } else if (d.dual_laser || !d.allow_laser) {
    d.risk = "dual";
    d.reason = !d.allow_laser ? "laser_wrong_hand" : "dual_laser_forbidden";
    d.path_ok = false;
  } else if (d.click_steal) {
    d.risk = "steal";
    d.reason = "wrong_hand_primary_click";
    d.path_ok = false;
  } else if (!d.laser_on) {
    d.risk = "off";
    d.reason = "laser_off";
  } else if (!d.has_primary_pose) {
    d.risk = "no_pose";
    d.reason = "primary_pose_missing";
    d.path_ok = false;
  } else if (d.menu_focus) {
    d.reason = "focus_primary";
  } else {
    d.reason = "aim_primary";
  }
  return d;
}

inline const char* Laser_StatusLabel(const LaserDecision& d) {
  if (std::strcmp(d.risk, "dual") == 0) return "LASER · DUAL FAIL";
  if (std::strcmp(d.risk, "steal") == 0) return "LASER · STEAL";
  if (std::strcmp(d.risk, "off") == 0) return "LASER · OFF";
  if (std::strcmp(d.risk, "no_pose") == 0) return "LASER · NO POSE";
  if (std::strcmp(d.risk, "idle") == 0) return "LASER · IDLE";
  if (d.menu_focus) return "LASER · FOCUS";
  if (d.path_ok) return "LASER · AIM";
  return "LASER · HOLD";
}

/// Snap-on-fire: while shooting, viewangles follow the gun; otherwise HMD.
inline Ang3 AimViewAngles(const Pose& hmd, const GunPose& gun, bool firing) {
  if (firing && gun.valid) return VectorAngles(gun.forward);
  if (hmd.valid) return hmd.ang;
  if (gun.valid) return gun.ang;
  return {};
}

/// Mouse delta (pixels) to close viewangles toward target. CSS look via SDL mouse.
inline void MouseDeltaToReach(const Ang3& current, const Ang3& target, float sens,
                              float* dx, float* dy) {
  const float yaw = AngleDeltaDeg(current.y, target.y);
  const float pitch = AngleDeltaDeg(current.p, target.p);
  const float s = (sens < 1e-4f) ? 0.022f : sens;
  if (dx) *dx = yaw / s;
  if (dy) *dy = -pitch / s;
}

} // namespace cssvr
