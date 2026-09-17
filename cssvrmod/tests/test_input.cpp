#include "cssvrmod/input.hpp"
#include "test_framework.h"

using namespace cssvr;

TEST(input_map_fire_and_move) {
  XrSample xr;
  xr.hmd.valid = true;
  xr.hmd.ang = {0, 45, 0};
  xr.right.valid = true;
  xr.right.ang = {0, 90, 0};
  xr.trigger_r = 0.9f;
  xr.stick_ly = 1.f;
  xr.a_click = true;
  GunPose gun;
  gun.valid = true;
  gun.forward = {0, 1, 0};
  InputConfig cfg;
  auto cmd = InputMap(xr, gun, cfg, 0.01f);
  ASSERT_TRUE(cmd.firing);
  ASSERT_TRUE((cmd.buttons & kInAttack) != 0);
  ASSERT_TRUE((cmd.buttons & kInJump) != 0);
  ASSERT_TRUE((cmd.buttons & kInForward) != 0);
  ASSERT_TRUE(cmd.look_from_gun);
  ASSERT_NEAR(cmd.view_yaw, 90.f, 2.f);
  ASSERT_STREQ(cmd.reason, "aim_gun");

  xr.trigger_r = 0.f;
  auto idle = InputMap(xr, gun, cfg, 0.01f);
  ASSERT_FALSE(idle.firing);
  ASSERT_FALSE(idle.look_from_gun);
  ASSERT_NEAR(idle.view_yaw, 45.f, 2.f);
}

TEST(input_deadzone_and_sdl_plan) {
  ASSERT_NEAR(ApplyDead(0.05f, 0.18f), 0.f, 1e-6);
  ASSERT_TRUE(ApplyDead(1.f, 0.18f) > 0.9f);
  UserCmdOverlay cmd;
  cmd.view_yaw = 30.f;
  cmd.buttons = kInAttack | kInForward;
  Ang3 cur{0, 0, 0};
  auto p = PlanSdlInject(cmd, cur, 0.022f);
  ASSERT_TRUE(p.mouse_left);
  ASSERT_TRUE(p.key_w);
  ASSERT_TRUE(p.mouse_dx > 0.f);
}
