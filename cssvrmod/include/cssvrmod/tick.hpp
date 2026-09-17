#pragma once
// One CSSVR frame: poses → wall resolve → gun/aim → input → melee.
#include "aim.hpp"
#include "collision.hpp"
#include "hand_bullet.hpp"
#include "input.hpp"
#include "melee.hpp"
#include "weapons.hpp"

namespace cssvr {

struct TickIn {
  XrSample xr;
  const WeaponInfo* wep = nullptr;
  float now = 0.f;
  float dt = 0.011f;
  InputConfig input;
  MeleeConfig melee;
  float wall_pad = 0.75f;
  float hand_radius = kDefaultRadius;
  bool world_melee_hit = false; // engine hull result (or mock)
  Ang3 current_view;
  float mouse_sens = 0.022f;
  TraceFn trace; // empty → skip wall
};

struct TickOut {
  Pose left_resolved;
  Pose right_resolved;
  GunPose gun;
  AimRay aim;
  LaserDecision laser;
  UserCmdOverlay cmd;
  SdlInjectPlan sdl;
  MeleeDecision melee;
  WallResolve left_wall;
  WallResolve right_wall;
  const char* status = "ok";
};

inline TickOut Tick(TickIn in, WallState& leftWall, WallState& rightWall, float* next_melee) {
  TickOut o;
  o.left_resolved = in.xr.left;
  o.right_resolved = in.xr.right;

  if (in.trace) {
    if (in.xr.left.valid) {
      o.left_wall =
          ResolveHandWallSweep(in.xr.left.pos, leftWall, in.hand_radius, in.wall_pad, in.trace);
      o.left_resolved.pos = o.left_wall.pos;
      if (!o.left_wall.clipped) {
        leftWall.last_free = o.left_resolved.pos;
        leftWall.has_free = true;
      }
    }
    if (in.xr.right.valid) {
      o.right_wall =
          ResolveHandWallSweep(in.xr.right.pos, rightWall, in.hand_radius, in.wall_pad, in.trace);
      o.right_resolved.pos = o.right_wall.pos;
      if (!o.right_wall.clipped) {
        rightWall.last_free = o.right_resolved.pos;
        rightWall.has_free = true;
      }
    }
  }

  WeaponOffset off;
  if (in.wep) off = in.wep->offset;
  if (in.wep && in.wep->muzzle_len > 0.f) off.muzzle_len = in.wep->muzzle_len;
  o.gun = GunFromHand(o.right_resolved, off);
  o.aim = ResolveMuzzle(o.gun, o.right_resolved);

  LaserOpts lo;
  lo.vr_active = in.xr.hmd.valid;
  lo.laser_on = true;
  lo.has_primary_pose = in.input.left_handed ? in.xr.left.valid : in.xr.right.valid;
  lo.primary_hand = in.input.left_handed ? "left" : "right";
  lo.laser_hand = lo.primary_hand;
  o.laser = Laser_Decide(lo);

  o.cmd = InputMap(in.xr, o.gun, in.input, in.dt);
  o.sdl = PlanSdlInject(o.cmd, in.current_view, in.mouse_sens);

  MeleeSample ms;
  const bool knife = in.wep && in.wep->is_melee;
  if (knife || o.cmd.melee_intent) {
    const Pose& hand = o.right_resolved.valid ? o.right_resolved : in.xr.right;
    ms.pos = hand.pos;
    ms.dir = Forward(hand.ang);
    ms.vel = hand.vel;
    ms.hand = Hand::Right;
    ms.impact = knife ? in.wep->melee_impact : ImpactType::Fist;
    ms.use_weapon = knife;
    ms.is_melee_weapon = knife;
    if (knife) ms.weapon_base_damage = in.wep->damage;
    ms.reach = knife ? 24.f : kDefaultReach;
    o.melee = MeleeDecide(ms, in.world_melee_hit, in.melee, in.now, next_melee);
  }

  if (o.melee.hit) o.status = "melee_hit";
  else if (o.cmd.firing) o.status = "firing";
  else if (o.right_wall.clipped || o.left_wall.clipped) o.status = "wall";
  else o.status = "ok";
  return o;
}

} // namespace cssvr
