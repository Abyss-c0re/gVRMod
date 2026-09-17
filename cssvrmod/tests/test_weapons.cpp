#include "cssvrmod/weapons.hpp"
#include "test_framework.h"

using namespace cssvr;

TEST(weapons_catalog_css) {
  ASSERT_TRUE(WeaponCount() >= 20);
  auto* knife = FindWeapon("weapon_knife");
  ASSERT_TRUE(knife);
  ASSERT_TRUE(knife->is_melee);
  ASSERT_TRUE(knife->damage >= 15.f);
  ASSERT_EQ(static_cast<int>(knife->melee_impact), static_cast<int>(ImpactType::Sharp));
  auto* ak = FindWeapon("weapon_ak47");
  ASSERT_TRUE(ak);
  ASSERT_FALSE(ak->is_melee);
  ASSERT_NEAR(ak->damage, 36.f, 0.01);
  ASSERT_TRUE(WeaponIsGun(ak));
  ASSERT_FALSE(WeaponIsGun(knife));
  ASSERT_TRUE(FindWeapon("weapon_awp"));
  ASSERT_TRUE(FindWeapon("weapon_m4a1"));
  ASSERT_FALSE(FindWeapon("weapon_physgun"));
  ASSERT_NEAR(WeaponSpreadScale(true, 0.f), 1.35f, 1e-4);
  ASSERT_TRUE(WeaponSpreadScale(false, 0.f) < 1.f);
}

TEST(weapons_index_roundtrip) {
  for (int i = 0; i < WeaponCount(); ++i) {
    auto* w = WeaponByIndex(i);
    ASSERT_TRUE(WeaponIsValid(w));
    ASSERT_TRUE(FindWeapon(w->classname) == w);
  }
  ASSERT_FALSE(WeaponByIndex(-1));
  ASSERT_FALSE(WeaponByIndex(999));
}
