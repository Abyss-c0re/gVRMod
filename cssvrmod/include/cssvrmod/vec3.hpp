#pragma once
// Minimal Source-style vector / angle math. No engine, no GL.
#include <cmath>
#include <cstdint>

namespace cssvr {

struct Vec3 {
  float x = 0.f, y = 0.f, z = 0.f;

  Vec3() = default;
  Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}

  Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3 operator-() const { return {-x, -y, -z}; }
  Vec3& operator+=(const Vec3& o) {
    x += o.x;
    y += o.y;
    z += o.z;
    return *this;
  }

  float Dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
  float LengthSqr() const { return x * x + y * y + z * z; }
  float Length() const { return std::sqrt(LengthSqr()); }
  float DistToSqr(const Vec3& o) const { return (*this - o).LengthSqr(); }

  Vec3 Normalized() const {
    float l = Length();
    if (l < 1e-8f) return {0, 0, 1};
    return *this * (1.f / l);
  }
  bool IsZero(float eps = 1e-4f) const { return LengthSqr() < eps * eps; }
};

inline Vec3 Cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// Source QAngle: pitch (x), yaw (y), roll (z) in degrees.
struct Ang3 {
  float p = 0.f, y = 0.f, r = 0.f;
  Ang3() = default;
  Ang3(float P, float Y, float R) : p(P), y(Y), r(R) {}
};

inline float Deg2Rad(float d) { return d * 0.01745329252f; }
inline float Rad2Deg(float r) { return r * 57.29577951f; }
inline float Clamp(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}
inline float Min(float a, float b) { return a < b ? a : b; }
inline float Max(float a, float b) { return a > b ? a : b; }

inline void AngleVectors(const Ang3& a, Vec3* forward, Vec3* right = nullptr, Vec3* up = nullptr) {
  const float sy = std::sin(Deg2Rad(a.y)), cy = std::cos(Deg2Rad(a.y));
  const float sp = std::sin(Deg2Rad(a.p)), cp = std::cos(Deg2Rad(a.p));
  const float sr = std::sin(Deg2Rad(a.r)), cr = std::cos(Deg2Rad(a.r));
  if (forward) *forward = {cp * cy, cp * sy, -sp};
  if (right) *right = {-1.f * sr * sp * cy + -1.f * cr * -sy, -1.f * sr * sp * sy + -1.f * cr * cy, -1.f * sr * cp};
  if (up) *up = {cr * sp * cy + -sr * -sy, cr * sp * sy + -sr * cy, cr * cp};
}

inline Vec3 Forward(const Ang3& a) {
  Vec3 f;
  AngleVectors(a, &f);
  return f;
}

/// OpenXR meters (y-up, -z forward) → Source inches (z-up, y forward).
inline Vec3 XrPosToSource(float x, float y, float z, float inches_per_meter = 39.3700787f) {
  return {x * inches_per_meter, -z * inches_per_meter, y * inches_per_meter};
}

inline Ang3 QuatToAng(float x, float y, float z, float w) {
  // OpenXR quat → yaw/pitch (then remap to Source z-up).
  const float sinp = 2.f * (w * y - z * x);
  float pitch = (std::fabs(sinp) >= 1.f) ? std::copysign(90.f, sinp) : Rad2Deg(std::asin(sinp));
  float yaw = Rad2Deg(std::atan2(2.f * (w * z + x * y), 1.f - 2.f * (y * y + z * z)));
  // OpenXR yaw around Y → Source yaw around Z (same heading, sign).
  return {-pitch, yaw, 0.f};
}

inline Ang3 VectorAngles(const Vec3& f) {
  Ang3 a;
  if (f.x == 0.f && f.y == 0.f) {
    a.y = 0.f;
    a.p = (f.z > 0.f) ? 270.f : 90.f;
  } else {
    a.y = Rad2Deg(std::atan2(f.y, f.x));
    if (a.y < 0.f) a.y += 360.f;
    a.p = Rad2Deg(std::atan2(-f.z, std::sqrt(f.x * f.x + f.y * f.y)));
    if (a.p < 0.f) a.p += 360.f;
  }
  a.r = 0.f;
  return a;
}

inline float AngleDeltaDeg(float from, float to) {
  float d = to - from;
  while (d > 180.f) d -= 360.f;
  while (d < -180.f) d += 360.f;
  return d;
}

struct Pose {
  Vec3 pos;
  Ang3 ang;
  Vec3 vel;
  bool valid = false;
};

enum class Hand : int { Left = 0, Right = 1, Head = 2 };

inline const char* HandName(Hand h) {
  if (h == Hand::Left) return "left";
  if (h == Hand::Right) return "right";
  if (h == Hand::Head) return "head";
  return "unknown";
}

inline Hand HandFromName(const char* s) {
  if (!s) return Hand::Right;
  if (s[0] == 'l' || s[0] == 'L') return Hand::Left;
  if (s[0] == 'h' || s[0] == 'H') return Hand::Head;
  return Hand::Right;
}

// Source DMG bits (shared with GMod / CSS).
constexpr int kDmgCrush = 1;
constexpr int kDmgBullet = 2;
constexpr int kDmgSlash = 4;
constexpr int kDmgBlast = 64;
constexpr int kDmgClub = 128;
constexpr int kDmgShock = 256;
constexpr int kDmgEnergyBeam = 1024;
constexpr int kDmgBuckshot = 536870912;

// IN_ buttons (usercmd).
constexpr int kInAttack = 1 << 0;
constexpr int kInJump = 1 << 1;
constexpr int kInDuck = 1 << 2;
constexpr int kInForward = 1 << 3;
constexpr int kInBack = 1 << 4;
constexpr int kInUse = 1 << 5;
constexpr int kInMoveLeft = 1 << 9;
constexpr int kInMoveRight = 1 << 10;
constexpr int kInAttack2 = 1 << 11;
constexpr int kInReload = 1 << 13;
constexpr int kInScore = 1 << 16;

} // namespace cssvr
