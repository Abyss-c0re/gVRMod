#pragma once
// Video calibration — same knobs as vrmod Vision (scale → V → H → eye).
// Pure: no GL / no XR. Right eye too far apart → lower eyescale.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace cssvr {

struct Calib {
  float eyescale = 0.25f;     // 0=one image  1=full ipd_m  (vrmod_eyescale)
  float hoffset = 0.f;        // -1..1 UV pan (vrmod_horizontaloffset)
  float voffset = 0.f;        // -1..1 (vrmod_verticaloffset)
  float scalefactor = 1.f;    // 0.05..4 zoom crop (vrmod_scalefactor)
  float lens_bend = 0.f;      // -0.5..0.5 (vrmod_lens_bend)
  float ipd_m = 0.064f;       // base IPD metres before eyescale
  bool swap_eyes = false;
};

inline float CalibClamp(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

inline Calib ClampCalib(Calib c) {
  c.eyescale = CalibClamp(c.eyescale, 0.f, 1.f);
  c.hoffset = CalibClamp(c.hoffset, -1.f, 1.f);
  c.voffset = CalibClamp(c.voffset, -1.f, 1.f);
  c.scalefactor = CalibClamp(c.scalefactor, 0.05f, 4.f);
  c.lens_bend = CalibClamp(c.lens_bend, -0.5f, 0.5f);
  c.ipd_m = CalibClamp(c.ipd_m, 0.02f, 0.12f);
  return c;
}

struct EyeBlit {
  float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
  float pose_x = 0.f; // VIEW-space metres
};

// Same-frame synthetic stereo. Gains match vrmod ComputeSubmitBounds.
inline EyeBlit CalibEye(const Calib& raw, int eye) {
  const Calib c = ClampCalib(raw);
  if (c.swap_eyes) eye = 1 - eye;
  const float halfU = std::max(0.02f, 0.5f / c.scalefactor);
  const float halfV = std::max(0.02f, 0.5f / c.scalefactor);
  const float panU = c.hoffset * 0.22f;
  const float panV = c.voffset * 0.45f;
  // Opposite U shift per eye — this is the "eye" / IPD dial.
  const float stereo = 0.04f * c.eyescale;
  float cx = 0.5f + (eye == 0 ? stereo : -stereo);
  float cy = 0.5f;
  cx = cx + (0.5f - cx) * c.lens_bend;
  cy = cy + (0.5f - cy) * c.lens_bend;
  EyeBlit b;
  b.u0 = CalibClamp(cx - halfU + panU, 0.f, 1.f);
  b.u1 = CalibClamp(cx + halfU + panU, 0.f, 1.f);
  b.v0 = CalibClamp(cy - halfV - panV, 0.f, 1.f);
  b.v1 = CalibClamp(cy + halfV - panV, 0.f, 1.f);
  if (b.u1 < b.u0 + 0.04f) b.u1 = CalibClamp(b.u0 + 0.04f, 0.f, 1.f);
  if (b.v1 < b.v0 + 0.04f) b.v1 = CalibClamp(b.v0 + 0.04f, 0.f, 1.f);
  b.pose_x = (eye == 0 ? -1.f : 1.f) * (c.ipd_m * 0.5f * c.eyescale);
  return b;
}

/// Parse `key value` text (cfg / unit tests). Unknown keys ignored.
inline bool ParseCalibText(const char* text, Calib* out) {
  if (!text || !out) return false;
  Calib c = *out;
  const char* p = text;
  while (*p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p == '#' || *p == '/' || *p == ';') {
      while (*p && *p != '\n') ++p;
      continue;
    }
    if (!*p) break;
    char key[64] = {};
    int ki = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '=' && ki < 63) key[ki++] = *p++;
    while (*p == ' ' || *p == '\t' || *p == '=') ++p;
    char val[64] = {};
    int vi = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && vi < 63) val[vi++] = *p++;
    while (*p && *p != '\n') ++p;
    if (!key[0] || !val[0]) continue;
    const float f = std::strtof(val, nullptr);
    if (std::strcmp(key, "eyescale") == 0) c.eyescale = f;
    else if (std::strcmp(key, "horizontaloffset") == 0 || std::strcmp(key, "hoffset") == 0)
      c.hoffset = f;
    else if (std::strcmp(key, "verticaloffset") == 0 || std::strcmp(key, "voffset") == 0)
      c.voffset = f;
    else if (std::strcmp(key, "scalefactor") == 0) c.scalefactor = f;
    else if (std::strcmp(key, "lens_bend") == 0 || std::strcmp(key, "lensbend") == 0)
      c.lens_bend = f;
    else if (std::strcmp(key, "ipd") == 0 || std::strcmp(key, "ipd_m") == 0) c.ipd_m = f;
    else if (std::strcmp(key, "swap_eyes") == 0 || std::strcmp(key, "swapeyes") == 0)
      c.swap_eyes = !(val[0] == '0' && val[1] == 0);
  }
  *out = ClampCalib(c);
  return true;
}

/// Live cfg: ~/.config/gvrmod/cssvr_calib.cfg (reloads when the file changes).
const Calib& CalibLive();
const char* CalibPath();
bool CalibWriteDefault(const char* path);

} // namespace cssvr
