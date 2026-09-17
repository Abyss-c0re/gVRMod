#include "cssvrmod/tick.hpp"
#include "test_framework.h"

using namespace cssvr;

TEST(tick_fire_ak_and_knife_melee) {
  TickIn in;
  in.xr.hmd.valid = true;
  in.xr.hmd.ang = {0, 0, 0};
  in.xr.right.valid = true;
  in.xr.right.pos = {0, 0, 40};
  in.xr.right.ang = {0, 90, 0};
  in.xr.right.vel = {120, 0, 0};
  in.xr.trigger_r = 0.9f;
  in.wep = FindWeapon("weapon_ak47");
  in.now = 1.f;
  WallState L, R;
  float next = 0.f;
  auto fire = Tick(in, L, R, &next);
  ASSERT_TRUE(fire.gun.valid);
  ASSERT_TRUE(fire.aim.valid);
  ASSERT_TRUE(fire.cmd.firing);
  ASSERT_STREQ(fire.status, "firing");
  ASSERT_TRUE(fire.laser.path_ok);

  in.xr.trigger_r = 0.f;
  in.xr.trigger_l = 0.9f; // melee intent (off-hand)
  in.wep = FindWeapon("weapon_knife");
  in.world_melee_hit = true;
  auto knife = Tick(in, L, R, &next);
  ASSERT_TRUE(knife.melee.hit);
  ASSERT_TRUE(knife.melee.damage > 20.f);
  ASSERT_STREQ(knife.status, "melee_hit");
}

TEST(tick_wall_blocks_hand) {
  auto world = [](Vec3 start, Vec3 end, Vec3, Vec3) {
    TraceHit t;
    t.start_pos = start;
    t.end_pos = end;
    auto solid = [](const Vec3& p) { return p.x >= 50.f; };
    t.start_solid = solid(start);
    t.all_solid = solid(start) && solid(end);
    if (t.start_solid) {
      t.hit = true;
      t.hit_world = true;
      t.hit_pos = start;
      t.hit_normal = {-1, 0, 0};
      t.fraction = 0.f;
      return t;
    }
    if (start.x < 50.f && end.x >= 50.f) {
      t.hit = true;
      t.hit_world = true;
      t.hit_pos = {50, start.y, start.z};
      t.hit_normal = {-1, 0, 0};
      t.fraction = (50.f - start.x) / (end.x - start.x);
    }
    return t;
  };
  TickIn in;
  in.xr.right.valid = true;
  in.xr.right.pos = {80, 0, 40};
  in.xr.right.ang = {0, 0, 0};
  in.trace = world;
  WallState L, R;
  R.last_free = {10, 0, 40};
  R.has_free = true;
  float next = 0.f;
  auto o = Tick(in, L, R, &next);
  ASSERT_TRUE(o.right_wall.clipped);
  ASSERT_TRUE(o.right_resolved.pos.x < 80.f);
}
