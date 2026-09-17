#include "cssvrmod/weapons.hpp"
#include <cstring>

namespace cssvr {

static const WeaponInfo kWeapons[] = {
    {"weapon_knife", "Knife", WeaponSlot::Knife, 20.f, 8.f, true, ImpactType::Sharp, {{0, 0, 0}, {0, 0, 0}, 8.f, false}, 3},
    {"weapon_glock", "Glock-18", WeaponSlot::Pistol, 25.f, 14.f, false, ImpactType::Blunt, {{2, 0, -2}, {0, 0, 0}, 14.f, false}, 2},
    {"weapon_usp", "USP-S", WeaponSlot::Pistol, 34.f, 16.f, false, ImpactType::Blunt, {{2, 0, -2}, {0, 0, 0}, 16.f, false}, 2},
    {"weapon_p228", "P228", WeaponSlot::Pistol, 32.f, 14.f, false, ImpactType::Blunt, {{2, 0, -2}, {0, 0, 0}, 14.f, false}, 2},
    {"weapon_deagle", "Desert Eagle", WeaponSlot::Pistol, 54.f, 18.f, false, ImpactType::Blunt, {{3, 0, -2}, {0, 0, 0}, 18.f, false}, 2},
    {"weapon_elite", "Dual Berettas", WeaponSlot::Pistol, 45.f, 14.f, false, ImpactType::Blunt, {{2, 0, -2}, {0, 0, 0}, 14.f, false}, 2},
    {"weapon_fiveseven", "Five-SeveN", WeaponSlot::Pistol, 25.f, 14.f, false, ImpactType::Blunt, {{2, 0, -2}, {0, 0, 0}, 14.f, false}, 2},
    {"weapon_m3", "M3 Super 90", WeaponSlot::Shotgun, 26.f, 28.f, false, ImpactType::Blunt, {{6, 0, -3}, {0, 0, 0}, 28.f, false}, 1},
    {"weapon_xm1014", "XM1014", WeaponSlot::Shotgun, 22.f, 28.f, false, ImpactType::Blunt, {{6, 0, -3}, {0, 0, 0}, 28.f, false}, 1},
    {"weapon_mac10", "MAC-10", WeaponSlot::Smg, 29.f, 16.f, false, ImpactType::Blunt, {{3, 0, -2}, {0, 0, 0}, 16.f, false}, 1},
    {"weapon_tmp", "TMP", WeaponSlot::Smg, 26.f, 16.f, false, ImpactType::Blunt, {{3, 0, -2}, {0, 0, 0}, 16.f, false}, 1},
    {"weapon_mp5navy", "MP5", WeaponSlot::Smg, 26.f, 20.f, false, ImpactType::Blunt, {{4, 0, -2}, {0, 0, 0}, 20.f, false}, 1},
    {"weapon_ump45", "UMP-45", WeaponSlot::Smg, 30.f, 20.f, false, ImpactType::Blunt, {{4, 0, -2}, {0, 0, 0}, 20.f, false}, 1},
    {"weapon_p90", "P90", WeaponSlot::Smg, 26.f, 18.f, false, ImpactType::Blunt, {{4, 0, -2}, {0, 0, 0}, 18.f, false}, 1},
    {"weapon_galil", "Galil", WeaponSlot::Rifle, 30.f, 26.f, false, ImpactType::Blunt, {{8, 0, -3}, {0, 0, 0}, 26.f, false}, 1},
    {"weapon_famas", "FAMAS", WeaponSlot::Rifle, 30.f, 26.f, false, ImpactType::Blunt, {{8, 0, -3}, {0, 0, 0}, 26.f, false}, 1},
    {"weapon_ak47", "AK-47", WeaponSlot::Rifle, 36.f, 28.f, false, ImpactType::Blunt, {{8, 0, -3}, {0, 0, 0}, 28.f, false}, 1},
    {"weapon_m4a1", "M4A1", WeaponSlot::Rifle, 33.f, 28.f, false, ImpactType::Blunt, {{8, 0, -3}, {0, 0, 0}, 28.f, false}, 1},
    {"weapon_sg552", "SG 552", WeaponSlot::Rifle, 33.f, 28.f, false, ImpactType::Blunt, {{8, 0, -3}, {0, 0, 0}, 28.f, false}, 1},
    {"weapon_aug", "AUG", WeaponSlot::Rifle, 32.f, 30.f, false, ImpactType::Blunt, {{8, 0, -3}, {0, 0, 0}, 30.f, false}, 1},
    {"weapon_scout", "Scout", WeaponSlot::Sniper, 75.f, 36.f, false, ImpactType::Blunt, {{10, 0, -3}, {0, 0, 0}, 36.f, false}, 1},
    {"weapon_awp", "AWP", WeaponSlot::Sniper, 115.f, 40.f, false, ImpactType::Blunt, {{12, 0, -3}, {0, 0, 0}, 40.f, false}, 1},
    {"weapon_g3sg1", "G3SG1", WeaponSlot::Sniper, 80.f, 38.f, false, ImpactType::Blunt, {{12, 0, -3}, {0, 0, 0}, 38.f, false}, 1},
    {"weapon_sg550", "SG 550", WeaponSlot::Sniper, 70.f, 38.f, false, ImpactType::Blunt, {{12, 0, -3}, {0, 0, 0}, 38.f, false}, 1},
    {"weapon_m249", "M249", WeaponSlot::Heavy, 32.f, 32.f, false, ImpactType::Heavy, {{10, 0, -4}, {0, 0, 0}, 32.f, false}, 1},
    {"weapon_hegrenade", "HE Grenade", WeaponSlot::Nade, 98.f, 6.f, false, ImpactType::Explosive, {{0, 0, 0}, {0, 0, 0}, 6.f, false}, 4},
    {"weapon_flashbang", "Flashbang", WeaponSlot::Nade, 0.f, 6.f, false, ImpactType::Blunt, {{0, 0, 0}, {0, 0, 0}, 6.f, false}, 4},
    {"weapon_smokegrenade", "Smoke", WeaponSlot::Nade, 0.f, 6.f, false, ImpactType::Blunt, {{0, 0, 0}, {0, 0, 0}, 6.f, false}, 4},
    {"weapon_c4", "C4", WeaponSlot::C4, 0.f, 8.f, false, ImpactType::Blunt, {{0, 0, 0}, {0, 0, 0}, 8.f, false}, 5},
};

int WeaponCount() { return static_cast<int>(sizeof(kWeapons) / sizeof(kWeapons[0])); }

const WeaponInfo* WeaponByIndex(int i) {
  if (i < 0 || i >= WeaponCount()) return nullptr;
  return &kWeapons[i];
}

const WeaponInfo* FindWeapon(const char* classname) {
  if (!classname || !classname[0]) return nullptr;
  for (int i = 0; i < WeaponCount(); ++i) {
    if (std::strcmp(kWeapons[i].classname, classname) == 0) return &kWeapons[i];
  }
  return nullptr;
}

} // namespace cssvr
