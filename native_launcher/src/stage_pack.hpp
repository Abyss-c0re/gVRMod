#pragma once
// G03: Cube STAGE / cal continuity pack — pure parse/format (offline-tested).
// Written by CubeUI at Start + refreshed at take_xr release.
// GMod may read later to avoid height/playspace jumps; applying is a separate careful step.
// No OpenXR / filesystem deps in this header.
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>

struct StagePackSnapshot {
  int version = 1;
  std::string refSpace = "LOCAL"; // STAGE | LOCAL (Cube shell content space)
  float headX = 0.f;              // meters in ref space (OpenXR Y-up)
  float headY = 0.f;
  float headZ = 0.f;
  bool headOk = false;
  float viewScale = 1.f;
  float scaleFactor = 1.f;
  float supersample = 1.f;
  std::string map;
  std::string source = "CubeUI";
  long ts = 0;
  bool valid = false;
};

inline void StagePack_Trim(std::string& s) {
  while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
  if (i) s = s.substr(i);
}

inline std::string StagePack_NormalizeSpace(const std::string& raw) {
  std::string s = raw;
  StagePack_Trim(s);
  for (char& c : s) c = (char)std::toupper((unsigned char)c);
  if (s == "STAGE") return "STAGE";
  if (s == "LOCAL") return "LOCAL";
  if (s == "VIEW") return "VIEW"; // should not be used for content; still parse
  return s.empty() ? "LOCAL" : s;
}

inline std::string StagePack_Format(const StagePackSnapshot& s) {
  std::ostringstream o;
  o << "v=" << (s.version > 0 ? s.version : 1) << "\n"
    << "ref_space=" << StagePack_NormalizeSpace(s.refSpace) << "\n"
    << "head_x_m=" << s.headX << "\n"
    << "head_y_m=" << s.headY << "\n"
    << "head_z_m=" << s.headZ << "\n"
    << "head_ok=" << (s.headOk ? 1 : 0) << "\n"
    << "viewscale=" << s.viewScale << "\n"
    << "scalefactor=" << s.scaleFactor << "\n"
    << "supersample=" << s.supersample << "\n"
    << "map=" << s.map << "\n"
    << "source=" << (s.source.empty() ? "CubeUI" : s.source) << "\n"
    << "ts=" << s.ts << "\n";
  return o.str();
}

// True when pack has a usable content reference space (STAGE preferred, LOCAL ok).
// head_ok is optional continuity data — missing head still valid for space preference.
inline bool StagePack_IsUsable(const StagePackSnapshot& s) {
  if (!s.valid) return false;
  const std::string sp = StagePack_NormalizeSpace(s.refSpace);
  return sp == "STAGE" || sp == "LOCAL";
}

inline bool StagePack_Parse(const std::string& body, StagePackSnapshot& out) {
  out = StagePackSnapshot{};
  bool gotSpace = false;
  std::istringstream in(body);
  std::string line;
  while (std::getline(in, line)) {
    StagePack_Trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = line.substr(0, eq);
    std::string v = line.substr(eq + 1);
    StagePack_Trim(k);
    StagePack_Trim(v);
    if (k == "v" || k == "version")
      out.version = std::atoi(v.c_str());
    else if (k == "ref_space" || k == "space") {
      out.refSpace = StagePack_NormalizeSpace(v);
      gotSpace = !out.refSpace.empty();
    } else if (k == "head_x_m" || k == "head_x")
      out.headX = std::strtof(v.c_str(), nullptr);
    else if (k == "head_y_m" || k == "head_y")
      out.headY = std::strtof(v.c_str(), nullptr);
    else if (k == "head_z_m" || k == "head_z")
      out.headZ = std::strtof(v.c_str(), nullptr);
    else if (k == "head_ok")
      out.headOk = (v == "1" || v == "true");
    else if (k == "viewscale")
      out.viewScale = std::strtof(v.c_str(), nullptr);
    else if (k == "scalefactor")
      out.scaleFactor = std::strtof(v.c_str(), nullptr);
    else if (k == "supersample")
      out.supersample = std::strtof(v.c_str(), nullptr);
    else if (k == "map")
      out.map = v;
    else if (k == "source")
      out.source = v.empty() ? "CubeUI" : v;
    else if (k == "ts")
      out.ts = std::atol(v.c_str());
  }
  if (out.version <= 0) out.version = 1;
  if (out.viewScale < 0.05f) out.viewScale = 1.f;
  if (out.viewScale > 4.f) out.viewScale = 4.f;
  if (out.scaleFactor < 0.05f) out.scaleFactor = 1.f;
  if (out.scaleFactor > 4.f) out.scaleFactor = 4.f;
  if (out.supersample < 0.5f) out.supersample = 0.5f;
  if (out.supersample > 3.f) out.supersample = 3.f;
  // Sanity: head Y in meters (OpenXR). Extreme values → mark not ok.
  if (out.headOk && (out.headY < 0.2f || out.headY > 2.8f))
    out.headOk = false;
  out.valid = gotSpace;
  return out.valid;
}
