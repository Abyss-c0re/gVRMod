#pragma once
// CSS weapon catalog — class names, damage, melee, muzzle. Offline SoT.
#include "aim.hpp"
#include "melee.hpp"
#include <cstddef>

namespace cssvr {

enum class WeaponSlot { Knife, Pistol, Rifle, Sniper, Shotgun, Smg, Heavy, Nade, C4, Unknown };

struct WeaponInfo {
  const char* classname = "";
  const char* print = "";
  WeaponSlot slot = WeaponSlot::Unknown;
  float damage = 0.f;       // CSS wiki / SDK Primary.Damage
  float muzzle_len = 18.f;
  bool is_melee = false;
  ImpactType melee_impact = ImpactType::Fist;
  WeaponOffset offset; // hand → gun
  int slot_key = 0;    // 1-5 CSS weapon slots
};

const WeaponInfo* FindWeapon(const char* classname);
const WeaponInfo* WeaponByIndex(int i);
int WeaponCount();

inline bool WeaponIsValid(const WeaponInfo* w) {
  return w && w->classname && w->classname[0];
}

inline bool WeaponIsGun(const WeaponInfo* w) {
  return w && !w->is_melee && w->slot != WeaponSlot::Nade && w->slot != WeaponSlot::C4 &&
         w->slot != WeaponSlot::Unknown;
}

/// CSS accuracy: still hand + not walking → treat as crouched (tighter spread).
inline float WeaponSpreadScale(bool walking, float hand_speed, float still_thresh = 40.f) {
  if (walking) return 1.35f;
  if (hand_speed > still_thresh) return 1.15f;
  return 0.72f; // VR "planted" bonus (not wallbang)
}

} // namespace cssvr
