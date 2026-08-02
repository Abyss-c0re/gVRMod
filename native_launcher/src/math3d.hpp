#pragma once
// Lightweight 3D math for Cube WebUI (OpenXR pose helpers).
#include <cmath>
#include <openxr/openxr.h>

struct Vec3 {
  float x = 0.f, y = 0.f, z = 0.f;
};

inline Vec3 V3(float x, float y, float z) { return {x, y, z}; }
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Vec3 Normalize(Vec3 v) {
  float l = std::sqrt(Dot(v, v));
  if (l < 1e-8f) return {0, 0, -1};
  return v * (1.f / l);
}

inline Vec3 QuatRotate(XrQuaternionf q, Vec3 v) {
  Vec3 u = {q.x, q.y, q.z};
  float s = q.w;
  return u * (2.f * Dot(u, v)) + v * (s * s - Dot(u, u)) + Cross(u, v) * (2.f * s);
}

inline XrPosef IdentityPose() {
  XrPosef p{};
  p.orientation.w = 1.f;
  return p;
}

// Column-major parent_from_local
inline void PoseToMat(const XrPosef& pose, float M[16]) {
  float x = pose.orientation.x, y = pose.orientation.y, z = pose.orientation.z, w = pose.orientation.w;
  float r00 = 1 - 2 * y * y - 2 * z * z, r01 = 2 * x * y - 2 * z * w, r02 = 2 * x * z + 2 * y * w;
  float r10 = 2 * x * y + 2 * z * w, r11 = 1 - 2 * x * x - 2 * z * z, r12 = 2 * y * z - 2 * x * w;
  float r20 = 2 * x * z - 2 * y * w, r21 = 2 * y * z + 2 * x * w, r22 = 1 - 2 * x * x - 2 * y * y;
  M[0] = r00; M[1] = r10; M[2] = r20; M[3] = 0;
  M[4] = r01; M[5] = r11; M[6] = r21; M[7] = 0;
  M[8] = r02; M[9] = r12; M[10] = r22; M[11] = 0;
  M[12] = pose.position.x; M[13] = pose.position.y; M[14] = pose.position.z; M[15] = 1;
}
