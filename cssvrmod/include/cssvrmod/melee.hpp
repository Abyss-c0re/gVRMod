#pragma once
// Port of addon/vrmod-x64/lua/vrmod/combat/sh_combat.lua
// Velocity-gated hull melee. CSS knife uses sharp / CSS knife base.
#include "vec3.hpp"
#include <cstring>

namespace cssvr {

enum class ImpactType {
  Fist,
  Head,
  Blunt,
  Sharp,
  Piercing,
  Heavy,
  Energy,
  Explosive,
  Stunstick
};

inline const char* ImpactTypeName(ImpactType t) {
  switch (t) {
  case ImpactType::Head: return "head";
  case ImpactType::Blunt: return "blunt";
  case ImpactType::Sharp: return "sharp";
  case ImpactType::Piercing: return "piercing";
  case ImpactType::Heavy: return "heavy";
  case ImpactType::Energy: return "energy";
  case ImpactType::Explosive: return "explosive";
  case ImpactType::Stunstick: return "stunstick";
  case ImpactType::Fist:
  default: return "fist";
  }
}

inline ImpactType ImpactTypeFromName(const char* s) {
  if (!s) return ImpactType::Fist;
  if (std::strcmp(s, "head") == 0) return ImpactType::Head;
  if (std::strcmp(s, "blunt") == 0) return ImpactType::Blunt;
  if (std::strcmp(s, "sharp") == 0) return ImpactType::Sharp;
  if (std::strcmp(s, "piercing") == 0) return ImpactType::Piercing;
  if (std::strcmp(s, "heavy") == 0) return ImpactType::Heavy;
  if (std::strcmp(s, "energy") == 0) return ImpactType::Energy;
  if (std::strcmp(s, "explosive") == 0) return ImpactType::Explosive;
  if (std::strcmp(s, "stunstick") == 0) return ImpactType::Stunstick;
  return ImpactType::Fist;
}

struct MeleeConfig {
  float vel_threshold = 1.5f; // Lua: * 50 → units/s
  float base_damage = 3.f;
  float delay = 0.45f;
  float speed_scale = 0.05f;
};

inline float MeleeThresholdUnits(const MeleeConfig& c) { return c.vel_threshold * 50.f; }
inline float MeleeHeadThresholdUnits(const MeleeConfig& c) { return MeleeThresholdUnits(c) * 0.5f; }

inline float MeleeImpactMultiplier(ImpactType t) {
  switch (t) {
  case ImpactType::Blunt: return 1.25f;
  case ImpactType::Stunstick: return 1.1f;
  case ImpactType::Sharp: return 1.5f;
  case ImpactType::Piercing: return 1.3f;
  case ImpactType::Heavy: return 2.0f;
  case ImpactType::Energy: return 1.4f;
  case ImpactType::Explosive: return 2.5f;
  case ImpactType::Head: return 1.15f;
  case ImpactType::Fist:
  default: return 1.0f;
  }
}

inline int MeleeDamageType(ImpactType t) {
  switch (t) {
  case ImpactType::Stunstick: return kDmgClub | kDmgShock;
  case ImpactType::Sharp: return kDmgSlash;
  case ImpactType::Piercing: return kDmgBullet;
  case ImpactType::Heavy: return kDmgClub | kDmgCrush;
  case ImpactType::Energy: return kDmgEnergyBeam | kDmgShock;
  case ImpactType::Explosive: return kDmgBlast | kDmgClub;
  case ImpactType::Blunt:
  case ImpactType::Head:
  case ImpactType::Fist:
  default: return kDmgClub;
  }
}

inline float MeleeSpeedFactor(float relative_speed, float speed_scale) {
  return Min(5.0f, 1.0f + Max(0.f, relative_speed) * speed_scale);
}

inline float MeleeDamageAmount(float base, float relative_speed, float speed_scale, float multiplier) {
  return base * MeleeSpeedFactor(relative_speed, speed_scale) * multiplier;
}

struct MeleeSample {
  Vec3 pos;
  Vec3 dir;
  Vec3 vel;
  float reach = 5.f;
  float radius = 2.2f;
  Hand hand = Hand::Right;
  ImpactType impact = ImpactType::Fist;
  float weapon_base_damage = 0.f; // 0 → config.base_damage
  bool use_weapon = false;
  bool is_melee_weapon = false;
};

struct MeleeDecision {
  bool swing = false;
  bool hit = false;
  float damage = 0.f;
  int damage_type = kDmgClub;
  float speed_factor = 1.f;
  float multiplier = 1.f;
  float relative_speed = 0.f;
  ImpactType impact = ImpactType::Fist;
  Hand hand = Hand::Right;
  Vec3 src;
  Vec3 dir;
  const char* risk = "none";
  const char* reason = "idle";
  bool path_ok = true;
};

inline bool MeleeVelocityGates(const MeleeSample& s, const MeleeConfig& c) {
  const float speed = s.vel.Length();
  if (s.hand == Hand::Head) return speed >= MeleeHeadThresholdUnits(c);
  return speed >= MeleeThresholdUnits(c);
}

/// Pure melee resolve. world_hit = server/client hull already reported a hit.
inline MeleeDecision MeleeDecide(const MeleeSample& s, bool world_hit, const MeleeConfig& c,
                                float now, float* next_melee_time) {
  MeleeDecision d;
  d.hand = s.hand;
  d.impact = s.impact;
  d.dir = s.dir.LengthSqr() > 1e-8f ? s.dir.Normalized() : Vec3{1, 0, 0};
  d.src = s.pos;
  d.relative_speed = s.vel.Length();
  d.multiplier = MeleeImpactMultiplier(s.impact);
  d.damage_type = MeleeDamageType(s.impact);
  d.speed_factor = MeleeSpeedFactor(d.relative_speed, c.speed_scale);
  const float base = (s.weapon_base_damage > 0.f) ? s.weapon_base_damage : c.base_damage;
  d.damage = MeleeDamageAmount(base, d.relative_speed, c.speed_scale, d.multiplier);

  if (next_melee_time && now < *next_melee_time) {
    d.reason = "cooldown";
    d.risk = "hold";
    return d;
  }
  if (!MeleeVelocityGates(s, c)) {
    d.reason = "below_threshold";
    return d;
  }
  if (!world_hit) {
    d.swing = true;
    d.reason = "swing_miss";
    return d;
  }
  d.swing = true;
  d.hit = true;
  d.reason = "hit";
  if (next_melee_time) *next_melee_time = now + c.delay;
  return d;
}

inline const char* MeleeStatusLabel(const MeleeDecision& d) {
  if (d.hit) return "MELEE · HIT";
  if (d.swing) return "MELEE · SWING";
  if (std::strcmp(d.reason, "cooldown") == 0) return "MELEE · COOLDOWN";
  if (std::strcmp(d.reason, "below_threshold") == 0) return "MELEE · HOLD";
  return "MELEE · IDLE";
}

} // namespace cssvr
