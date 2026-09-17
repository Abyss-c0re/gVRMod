#pragma once
// Port of addon/vrmod-x64/lua/vrmod/utils/sh_hand_bullet_law.lua (G37).
// Hands Real for grabs; bullets redirect; proxies never solid to world.
#include "vec3.hpp"
#include <cstring>
#include <string>

namespace cssvr {

inline float HandBullet_HandDamageScale() { return 0.45f; }
inline float HandBullet_HeadDamageScale() { return 10.0f; }
inline bool HandBullet_ProxySolidToWorld() { return false; }
inline bool HandBullet_AllowGrabContact() { return true; }
inline bool HandBullet_PreventSelfMeleeOnHand() { return true; }
inline bool HandBullet_AbsorbNonBulletOnProxy() { return true; }

inline bool HandBullet_IsBulletDamageType(int dmgType) {
  if (dmgType == 0) return false;
  if ((dmgType & kDmgBullet) != 0) return true;
  if ((dmgType & kDmgBuckshot) != 0) return true;
  return false;
}

inline Hand HandBullet_NormalizePart(const char* part) {
  return HandFromName(part);
}

inline bool HandBullet_IsHandPart(Hand part) {
  return part == Hand::Left || part == Hand::Right;
}

inline bool HandBullet_ShouldAbsorbOnProxy(bool is_bullet, bool is_self, Hand part) {
  if (!is_bullet) {
    if (is_self && HandBullet_IsHandPart(part) && HandBullet_PreventSelfMeleeOnHand())
      return true;
    return HandBullet_AbsorbNonBulletOnProxy();
  }
  return false;
}

inline float HandBullet_RedirectScale(Hand part) {
  if (part == Hand::Head) return HandBullet_HeadDamageScale();
  if (HandBullet_IsHandPart(part)) return HandBullet_HandDamageScale();
  return 0.f;
}

inline bool HandBullet_ShouldDropOnHandBullet(Hand part, bool is_bullet) {
  return is_bullet && HandBullet_IsHandPart(part);
}

struct HandBulletOpts {
  Hand part = Hand::Right;
  bool is_bullet = false;
  bool dmg_type_set = false;
  int dmg_type = 0;
  bool is_self = false;
  float damage = 0.f;
  bool solid_to_world = false;
  bool blocks_bullets_as_world = false;
};

struct HandBulletDecision {
  bool valid = true;
  Hand part = Hand::Right;
  bool is_bullet = false;
  bool is_self = false;
  bool absorb = false;
  float redirect_scale = 0.f;
  float player_damage = 0.f;
  bool drop_weapon = false;
  bool proxy_solid_to_world = false;
  bool grab_contact = true;
  const char* risk = "none";
  const char* reason = "ok";
  bool path_ok = true;
};

inline HandBulletDecision HandBullet_Decide(HandBulletOpts opts) {
  HandBulletDecision d;
  d.part = opts.part;
  bool isBullet = opts.is_bullet;
  if (opts.dmg_type_set) isBullet = HandBullet_IsBulletDamageType(opts.dmg_type);
  d.is_bullet = isBullet;
  d.is_self = opts.is_self;
  d.absorb = HandBullet_ShouldAbsorbOnProxy(isBullet, opts.is_self, opts.part);
  d.redirect_scale = isBullet ? HandBullet_RedirectScale(opts.part) : 0.f;
  d.player_damage = (!d.absorb && isBullet) ? (opts.damage * d.redirect_scale) : 0.f;
  d.drop_weapon = HandBullet_ShouldDropOnHandBullet(opts.part, isBullet);
  d.proxy_solid_to_world = HandBullet_ProxySolidToWorld();
  d.grab_contact = HandBullet_AllowGrabContact();
  if (opts.solid_to_world) {
    d.risk = "solid_world";
    d.reason = "proxy_must_not_solid_world";
    d.path_ok = false;
  } else if (opts.blocks_bullets_as_world) {
    d.risk = "blocks_bullets";
    d.reason = "hands_block_bullet_traces";
    d.path_ok = false;
  } else if (opts.is_self && !isBullet && HandBullet_IsHandPart(opts.part) && !d.absorb) {
    d.risk = "self_melee";
    d.reason = "self_punch_not_absorbed";
    d.path_ok = false;
  } else if (isBullet && d.player_damage > 0.f) {
    d.risk = "none";
    d.reason = "bullet_redirect";
    d.path_ok = true;
  } else if (d.absorb) {
    d.reason = "absorbed_non_bullet";
    d.path_ok = true;
  } else {
    d.reason = "idle";
  }
  return d;
}

inline const char* HandBullet_StatusLabel(const HandBulletDecision& d) {
  if (!d.valid) return "HAND · IDLE";
  if (std::strcmp(d.risk, "solid_world") == 0) return "HAND · SOLID WORLD";
  if (std::strcmp(d.risk, "blocks_bullets") == 0) return "HAND · BLOCKS BULLETS";
  if (std::strcmp(d.risk, "self_melee") == 0) return "HAND · SELF MELEE";
  if (d.is_bullet && d.player_damage > 0.f) return "HAND · BULLET REDIRECT";
  if (d.absorb) return "HAND · ABSORB";
  if (d.path_ok) return "HAND · OK";
  return "HAND · HOLD";
}

inline bool HandBullet_IsBlockRisk(const HandBulletDecision& d) {
  return std::strcmp(d.risk, "blocks_bullets") == 0 || std::strcmp(d.risk, "solid_world") == 0;
}

struct HandBulletHmdExpect {
  const char* verdict = "idle";
  bool expect_grabs = true;
  bool expect_bullets_filtered = true;
  const char* checklist = "G37 · IDLE · no hand-bullet decision";
  const char* pass_line = "N/A";
  const char* fail_line = "N/A";
};

inline HandBulletHmdExpect HandBullet_HmdExpect(const HandBulletDecision& d) {
  HandBulletHmdExpect e;
  if (!d.valid) return e;
  if (!d.path_ok) {
    e.verdict = "expect_fail";
    e.expect_bullets_filtered = false;
    e.checklist = "G37 · FAIL";
    e.pass_line = "Hands grab Real; bullets filtered; no world solid";
    e.fail_line = "Hands block all bullets / solid walls thrash";
    return e;
  }
  if (d.is_bullet) {
    e.verdict = "expect_bullet_redirect";
    e.checklist = "G37 · BULLET";
    e.pass_line = "Damage redirect; hand drop if hand hit; no silent stop";
    e.fail_line = "Bullets vanish in hands with no feedback";
    return e;
  }
  e.verdict = "expect_ok";
  e.checklist = "G37 · OK · grabs Real · non-bullet absorbed · world not solid";
  e.pass_line = "Grab props; shoot past hands; no wall spark thrash";
  e.fail_line = "Hands block bullets or feel non-physical for grabs";
  return e;
}

} // namespace cssvr
