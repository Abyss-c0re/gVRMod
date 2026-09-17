#include "cssvrmod/hand_bullet.hpp"
#include "test_framework.h"

using namespace cssvr;

TEST(hand_bullet_g37_constants) {
  ASSERT_NEAR(HandBullet_HandDamageScale(), 0.45f, 1e-6);
  ASSERT_NEAR(HandBullet_HeadDamageScale(), 10.f, 1e-6);
  ASSERT_FALSE(HandBullet_ProxySolidToWorld());
  ASSERT_TRUE(HandBullet_AllowGrabContact());
  ASSERT_TRUE(HandBullet_PreventSelfMeleeOnHand());
  ASSERT_TRUE(HandBullet_AbsorbNonBulletOnProxy());
  ASSERT_TRUE(HandBullet_IsBulletDamageType(kDmgBullet));
  ASSERT_FALSE(HandBullet_IsBulletDamageType(0));
  ASSERT_TRUE(HandBullet_IsHandPart(Hand::Left));
  ASSERT_FALSE(HandBullet_IsHandPart(Hand::Head));
  ASSERT_NEAR(HandBullet_RedirectScale(Hand::Head), 10.f, 1e-6);
  ASSERT_NEAR(HandBullet_RedirectScale(Hand::Right), 0.45f, 1e-6);
  ASSERT_TRUE(HandBullet_ShouldDropOnHandBullet(Hand::Left, true));
  ASSERT_FALSE(HandBullet_ShouldDropOnHandBullet(Hand::Head, true));
}

TEST(hand_bullet_g37_decide) {
  ASSERT_TRUE(HandBullet_ShouldAbsorbOnProxy(false, true, Hand::Left));
  ASSERT_FALSE(HandBullet_ShouldAbsorbOnProxy(true, false, Hand::Left));
  HandBulletOpts hit;
  hit.part = Hand::Right;
  hit.is_bullet = true;
  hit.damage = 100.f;
  auto handHit = HandBullet_Decide(hit);
  ASSERT_TRUE(handHit.path_ok);
  ASSERT_NEAR(handHit.player_damage, 45.f, 1e-4);
  ASSERT_TRUE(handHit.drop_weapon);
  ASSERT_STREQ(HandBullet_StatusLabel(handHit), "HAND · BULLET REDIRECT");
  auto he = HandBullet_HmdExpect(handHit);
  ASSERT_STREQ(he.verdict, "expect_bullet_redirect");

  HandBulletOpts block;
  block.part = Hand::Left;
  block.is_bullet = true;
  block.blocks_bullets_as_world = true;
  auto b = HandBullet_Decide(block);
  ASSERT_FALSE(b.path_ok);
  ASSERT_STREQ(b.risk, "blocks_bullets");
  ASSERT_TRUE(HandBullet_IsBlockRisk(b));

  HandBulletOpts solid;
  solid.solid_to_world = true;
  solid.part = Hand::Left;
  ASSERT_STREQ(HandBullet_Decide(solid).risk, "solid_world");

  HandBulletOpts absorb;
  absorb.part = Hand::Left;
  absorb.is_bullet = false;
  absorb.is_self = true;
  absorb.damage = 10.f;
  auto a = HandBullet_Decide(absorb);
  ASSERT_TRUE(a.absorb);
  ASSERT_STREQ(HandBullet_StatusLabel(a), "HAND · ABSORB");
}
