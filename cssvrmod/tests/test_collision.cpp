#include "cssvrmod/collision.hpp"
#include "test_framework.h"

using namespace cssvr;

TEST(collision_boxes_and_melee_detect) {
  auto b = BoxesFromAABB({-10, -2, -2}, {10, 2, 2});
  ASSERT_TRUE(b.ex > 9.f);
  ASSERT_TRUE(b.horizontal.maxs.x > 0.f);
  ASSERT_TRUE(DetectMeleeFromAABB("models/weapons/w_knife_t.mdl", {-1, -1, -20}, {1, 1, 20}));
  ASSERT_FALSE(DetectMeleeFromAABB("models/weapons/w_ak47.mdl", {-20, -4, -4}, {20, 4, 4}));
  ASSERT_TRUE(ModelNameLooksMelee("weapon_knife"));
}

TEST(collision_last_free_wall) {
  // Wall at x=100, solid x>=100.
  auto world = [](Vec3 start, Vec3 end, Vec3, Vec3) {
    TraceHit t;
    t.start_pos = start;
    t.end_pos = end;
    auto solid = [](const Vec3& p) { return p.x >= 100.f; };
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
    if (start.x < 100.f && end.x >= 100.f) {
      t.hit = true;
      t.hit_world = true;
      const float f = (100.f - start.x) / (end.x - start.x);
      t.fraction = f;
      t.hit_pos = {100.f, start.y, start.z};
      t.hit_normal = {-1, 0, 0};
    }
    return t;
  };

  WallState st;
  auto free = ResolveHandWallSweep({50, 0, 40}, st, 2.2f, 0.75f, world);
  ASSERT_FALSE(free.clipped);
  st.last_free = {50, 0, 40};
  st.has_free = true;

  auto blocked = ResolveHandWallSweep({140, 0, 40}, st, 2.2f, 0.75f, world);
  ASSERT_TRUE(blocked.clipped);
  ASSERT_TRUE(blocked.pos.x < 100.f + 1.f);
  ASSERT_STREQ(blocked.reason, "wall_rest");

  // Desired still solid, keep last free if rest fails — start_solid from last free path.
  auto buried = ResolveHandWallSweep({200, 0, 40}, st, 2.2f, 0.75f, world);
  ASSERT_TRUE(buried.clipped);
}

TEST(collision_floor_does_not_lock) {
  auto floor = [](Vec3 start, Vec3 end, Vec3, Vec3) {
    TraceHit t;
    t.start_pos = start;
    t.end_pos = end;
    if (end.z < 0.f && start.z >= 0.f) {
      t.hit = true;
      t.hit_world = true;
      t.hit_pos = {end.x, end.y, 0.f};
      t.hit_normal = {0, 0, 1};
      t.fraction = 0.5f;
    }
    return t;
  };
  WallState st;
  auto r = ResolveHandWallSweep({10, 10, 20}, st, 2.2f, 0.75f, floor);
  ASSERT_FALSE(r.clipped);
}
