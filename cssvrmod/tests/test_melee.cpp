#include "cssvrmod/melee.hpp"
#include "test_framework.h"

using namespace cssvr;

TEST(melee_threshold_and_damage) {
  MeleeConfig c;
  ASSERT_NEAR(MeleeThresholdUnits(c), 75.f, 1e-4);
  ASSERT_NEAR(MeleeHeadThresholdUnits(c), 37.5f, 1e-4);
  ASSERT_NEAR(MeleeImpactMultiplier(ImpactType::Sharp), 1.5f, 1e-6);
  ASSERT_EQ(MeleeDamageType(ImpactType::Sharp), kDmgSlash);
  ASSERT_EQ(MeleeDamageType(ImpactType::Fist), kDmgClub);
  const float dmg = MeleeDamageAmount(20.f, 100.f, 0.05f, 1.5f);
  // speedFactor = min(5, 1+5) = 5; dmg = 20*5*1.5 = 150
  ASSERT_NEAR(dmg, 150.f, 1e-3);
  ASSERT_NEAR(MeleeSpeedFactor(0.f, 0.05f), 1.f, 1e-6);
}

TEST(melee_decide_gates) {
  MeleeConfig c;
  MeleeSample s;
  s.pos = {0, 0, 40};
  s.dir = {1, 0, 0};
  s.vel = {10, 0, 0}; // below 75
  s.hand = Hand::Right;
  s.impact = ImpactType::Sharp;
  s.weapon_base_damage = 20.f;
  float next = 0.f;
  auto hold = MeleeDecide(s, true, c, 1.f, &next);
  ASSERT_FALSE(hold.hit);
  ASSERT_STREQ(hold.reason, "below_threshold");
  ASSERT_STREQ(MeleeStatusLabel(hold), "MELEE · HOLD");

  s.vel = {80, 0, 0};
  auto miss = MeleeDecide(s, false, c, 1.f, &next);
  ASSERT_TRUE(miss.swing);
  ASSERT_FALSE(miss.hit);
  ASSERT_STREQ(MeleeStatusLabel(miss), "MELEE · SWING");

  auto hit = MeleeDecide(s, true, c, 1.f, &next);
  ASSERT_TRUE(hit.hit);
  ASSERT_TRUE(hit.damage > 20.f);
  ASSERT_TRUE(next > 1.f);
  ASSERT_STREQ(MeleeStatusLabel(hit), "MELEE · HIT");

  auto cd = MeleeDecide(s, true, c, 1.1f, &next);
  ASSERT_FALSE(cd.hit);
  ASSERT_STREQ(cd.reason, "cooldown");
}
