#include "cssvrmod/aim.hpp"
#include "test_framework.h"

using namespace cssvr;

TEST(aim_gun_slave_and_muzzle) {
  Pose hand;
  hand.pos = {0, 0, 40};
  hand.ang = {0, 90, 0}; // +Y
  hand.valid = true;
  WeaponOffset off;
  off.pos = {8, 0, -2};
  off.muzzle_len = 20.f;
  auto gun = GunFromHand(hand, off);
  ASSERT_TRUE(gun.valid);
  auto ray = ResolveMuzzle(gun, hand);
  ASSERT_TRUE(ray.valid);
  ASSERT_NEAR(ray.dir.y, 1.f, 0.05);
  // Detached attachment is rejected.
  Vec3 farAtt{500, 500, 500};
  auto bad = ResolveMuzzle(gun, hand, &farAtt, 80.f);
  ASSERT_TRUE(bad.valid);
  ASSERT_FALSE(bad.used_attachment);
}

TEST(aim_laser_primary_only) {
  ASSERT_STREQ(Laser_PrimaryHandFromInt(1), "left");
  ASSERT_STREQ(Laser_SecondaryHand("left"), "right");
  ASSERT_TRUE(Laser_AllowFromHand("right", "right"));
  ASSERT_FALSE(Laser_AllowFromHand("left", "right"));
  LaserOpts o;
  o.vr_active = true;
  o.laser_on = true;
  o.has_primary_pose = true;
  o.primary_hand = "right";
  o.laser_hand = "left";
  auto dual = Laser_Decide(o);
  ASSERT_FALSE(dual.path_ok);
  ASSERT_STREQ(dual.risk, "dual");
  ASSERT_STREQ(Laser_StatusLabel(dual), "LASER · DUAL FAIL");

  o.laser_hand = "right";
  o.click_action = "boolean_left_primaryfire";
  auto steal = Laser_Decide(o);
  ASSERT_STREQ(steal.risk, "steal");

  o.click_action = nullptr;
  auto ok = Laser_Decide(o);
  ASSERT_TRUE(ok.path_ok);
  ASSERT_STREQ(Laser_StatusLabel(ok), "LASER · AIM");
}

TEST(aim_view_snap_on_fire) {
  Pose hmd;
  hmd.ang = {0, 0, 0};
  hmd.valid = true;
  GunPose gun;
  gun.forward = {0, 1, 0};
  gun.valid = true;
  auto look = AimViewAngles(hmd, gun, true);
  ASSERT_NEAR(look.y, 90.f, 1.f);
  auto idle = AimViewAngles(hmd, gun, false);
  ASSERT_NEAR(idle.y, 0.f, 1.f);
}
