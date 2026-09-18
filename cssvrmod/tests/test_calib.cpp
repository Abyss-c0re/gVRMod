#include "cssvrmod/calib.hpp"
#include "test_framework.h"

using namespace cssvr;

TEST(calib_clamp_and_defaults) {
  Calib c;
  c.eyescale = 9.f;
  c.hoffset = -4.f;
  c.scalefactor = 0.f;
  c.ipd_m = 1.f;
  c = ClampCalib(c);
  ASSERT_NEAR(c.eyescale, 1.f, 0.001);
  ASSERT_NEAR(c.hoffset, -1.f, 0.001);
  ASSERT_NEAR(c.scalefactor, 0.05f, 0.001);
  ASSERT_NEAR(c.ipd_m, 0.12f, 0.001);
}

TEST(calib_eye_apart_follows_eyescale) {
  Calib c;
  c.eyescale = 0.f;
  auto l0 = CalibEye(c, 0);
  auto r0 = CalibEye(c, 1);
  ASSERT_NEAR(l0.pose_x, 0.f, 0.0001);
  ASSERT_NEAR(r0.pose_x, 0.f, 0.0001);
  ASSERT_NEAR(l0.u0, r0.u0, 0.001);

  c.eyescale = 1.f;
  auto l1 = CalibEye(c, 0);
  auto r1 = CalibEye(c, 1);
  ASSERT_TRUE(l1.pose_x < 0.f);
  ASSERT_TRUE(r1.pose_x > 0.f);
  ASSERT_TRUE(r1.pose_x - l1.pose_x > 0.05f); // ~full 64 mm
  ASSERT_TRUE(l1.u0 > r1.u0);                 // left crop shifted right
}

TEST(calib_parse_vrmod_names) {
  Calib c;
  ASSERT_TRUE(ParseCalibText(
      "# comment\neyescale 0.2\nhorizontaloffset 0.1\nverticaloffset -0.25\n"
      "scalefactor 1.1\nswap_eyes 1\nipd_m 0.07\n",
      &c));
  ASSERT_NEAR(c.eyescale, 0.2f, 0.001);
  ASSERT_NEAR(c.hoffset, 0.1f, 0.001);
  ASSERT_NEAR(c.voffset, -0.25f, 0.001);
  ASSERT_NEAR(c.scalefactor, 1.1f, 0.001);
  ASSERT_NEAR(c.ipd_m, 0.07f, 0.001);
  ASSERT_TRUE(c.swap_eyes);
  auto swapped = CalibEye(c, 0);
  c.swap_eyes = false;
  auto right = CalibEye(c, 1);
  ASSERT_NEAR(swapped.pose_x, right.pose_x, 0.0001);
}
