#pragma once
// OpenXR shell sample → CSS usercmd overlay + SDL inject plan.
#include "aim.hpp"
#include "vec3.hpp"
#include <cmath>
#include <cstring>

namespace cssvr {

struct XrSample {
  Pose hmd;
  Pose left;
  Pose right;
  float trigger_l = 0.f, trigger_r = 0.f;
  float grab_l = 0.f, grab_r = 0.f;
  float stick_lx = 0.f, stick_ly = 0.f;
  float stick_rx = 0.f, stick_ry = 0.f;
  bool menu = false;
  bool a_click = false; // jump
  bool b_click = false; // reload
  bool x_click = false; // use / inspect
  bool y_click = false; // last weapon / knife
};

struct UserCmdOverlay {
  float view_pitch = 0.f;
  float view_yaw = 0.f;
  float forwardmove = 0.f;
  float sidemove = 0.f;
  float upmove = 0.f;
  int buttons = 0;
  bool firing = false;
  bool melee_intent = false;
  bool look_from_gun = false;
  const char* reason = "ok";
};

struct InputConfig {
  float trigger_thresh = 0.55f;
  float grab_thresh = 0.6f;
  float stick_dead = 0.18f;
  float move_speed = 450.f; // CSS cl_forwardspeed default-ish
  float snap_yaw = 30.f;
  bool snap_turn = false;
  bool left_handed = false;
  bool smooth_turn = true;
  float turn_speed = 120.f; // deg/s
};

inline float ApplyDead(float v, float dead) {
  if (std::fabs(v) < dead) return 0.f;
  const float s = (v > 0.f) ? 1.f : -1.f;
  return s * (std::fabs(v) - dead) / (1.f - dead);
}

inline UserCmdOverlay InputMap(const XrSample& xr, const GunPose& gun, const InputConfig& cfg,
                               float dt) {
  UserCmdOverlay o;
  const bool primaryLeft = cfg.left_handed;
  const float fireAxis = primaryLeft ? xr.trigger_l : xr.trigger_r;
  const float meleeAxis = primaryLeft ? xr.trigger_r : xr.trigger_l;
  o.firing = fireAxis >= cfg.trigger_thresh;
  o.melee_intent = meleeAxis >= cfg.trigger_thresh ||
                   (primaryLeft ? xr.grab_r : xr.grab_l) >= cfg.grab_thresh;
  o.look_from_gun = o.firing && gun.valid;
  const Ang3 look = AimViewAngles(xr.hmd, gun, o.look_from_gun);
  o.view_pitch = look.p;
  o.view_yaw = look.y;

  const float mx = ApplyDead(xr.stick_lx, cfg.stick_dead);
  const float my = ApplyDead(xr.stick_ly, cfg.stick_dead);
  o.forwardmove = my * cfg.move_speed;
  o.sidemove = mx * cfg.move_speed;
  if (o.forwardmove > 1.f) o.buttons |= kInForward;
  if (o.forwardmove < -1.f) o.buttons |= kInBack;
  if (o.sidemove > 1.f) o.buttons |= kInMoveRight;
  if (o.sidemove < -1.f) o.buttons |= kInMoveLeft;

  if (o.firing) o.buttons |= kInAttack;
  if (o.melee_intent && !o.firing) o.buttons |= kInAttack2;
  if (xr.a_click) o.buttons |= kInJump;
  if (xr.b_click) o.buttons |= kInReload;
  if (xr.x_click) o.buttons |= kInUse;
  if (xr.menu) o.buttons |= kInScore;

  // Right stick: snap / smooth turn applied to yaw (locomotion, not aim).
  const float rx = ApplyDead(xr.stick_rx, cfg.stick_dead);
  if (cfg.snap_turn) {
    if (rx > 0.7f) o.view_yaw += cfg.snap_yaw;
    if (rx < -0.7f) o.view_yaw -= cfg.snap_yaw;
  } else if (cfg.smooth_turn) {
    o.view_yaw += rx * cfg.turn_speed * (dt > 0.f ? dt : 0.011f);
  }
  o.reason = o.look_from_gun ? "aim_gun" : "aim_hmd";
  return o;
}

struct SdlInjectPlan {
  float mouse_dx = 0.f;
  float mouse_dy = 0.f;
  bool key_w = false, key_a = false, key_s = false, key_d = false;
  bool key_space = false, key_ctrl = false, key_r = false, key_e = false;
  bool key_tab = false, key_1 = false;
  bool mouse_left = false, mouse_right = false;
};

inline SdlInjectPlan PlanSdlInject(const UserCmdOverlay& cmd, const Ang3& current_view,
                                   float mouse_sens) {
  SdlInjectPlan p;
  Ang3 target{cmd.view_pitch, cmd.view_yaw, 0.f};
  MouseDeltaToReach(current_view, target, mouse_sens, &p.mouse_dx, &p.mouse_dy);
  p.key_w = (cmd.buttons & kInForward) != 0;
  p.key_s = (cmd.buttons & kInBack) != 0;
  p.key_a = (cmd.buttons & kInMoveLeft) != 0;
  p.key_d = (cmd.buttons & kInMoveRight) != 0;
  p.key_space = (cmd.buttons & kInJump) != 0;
  p.key_ctrl = (cmd.buttons & kInDuck) != 0;
  p.key_r = (cmd.buttons & kInReload) != 0;
  p.key_e = (cmd.buttons & kInUse) != 0;
  p.key_tab = (cmd.buttons & kInScore) != 0;
  p.mouse_left = (cmd.buttons & kInAttack) != 0;
  p.mouse_right = (cmd.buttons & kInAttack2) != 0;
  return p;
}

} // namespace cssvr
