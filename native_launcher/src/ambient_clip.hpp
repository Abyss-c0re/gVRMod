#pragma once
// G12: Cube handoff ambient clip contract — pure format/parse (offline-tested).
// Gain curve lives in CubeHandoffAudioGain; this names the clip + play/stop policy.
// No OpenAL required: status file cube_ambient.txt is the SoT for a future player.
#include <cstdlib>
#include <sstream>
#include <string>

struct AmbientClipSnapshot {
  int version = 1;
  float gain = 1.f;           // 0..1 from handoff gain law
  bool playing = false;       // should a clip be audible
  bool handoff = false;
  std::string clip_rel = "ambient/cube_hold.ogg"; // under launcher assets/
  std::string source = "cube_webui";
  long ts = 0;
  bool valid = false;
};

inline void AmbientClip_Trim(std::string& s) {
  while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
  if (i) s = s.substr(i);
}

inline const char* CubeAmbient_DefaultClipRel() { return "ambient/cube_hold.ogg"; }

// True when handoff ambient should be considered active (gain above silence floor).
inline bool CubeAmbient_ShouldPlay(float gain, bool handoff) {
  if (!handoff) return false;
  if (gain < 0.02f) return false;
  return true;
}

// Master * gain, clamped 0..1.
inline float CubeAmbient_EffectiveVolume(float gain, float master = 1.f) {
  if (gain < 0.f) gain = 0.f;
  if (gain > 1.f) gain = 1.f;
  if (master < 0.f) master = 0.f;
  if (master > 1.f) master = 1.f;
  return gain * master;
}

// Join assets dir + relative clip (or keep absolute path). Pure string only.
inline std::string CubeAmbient_ResolveClipPath(const std::string& assetsDir, const std::string& clipRel) {
  std::string rel = clipRel.empty() ? CubeAmbient_DefaultClipRel() : clipRel;
  AmbientClip_Trim(rel);
  if (!rel.empty() && rel[0] == '/') return rel;
  if (assetsDir.empty()) return rel;
  if (assetsDir.back() == '/') return assetsDir + rel;
  return assetsDir + "/" + rel;
}

inline std::string CubeAmbient_Format(const AmbientClipSnapshot& s) {
  std::ostringstream o;
  o << "v=" << (s.version > 0 ? s.version : 1) << "\n"
    << "gain=" << s.gain << "\n"
    << "playing=" << (s.playing ? 1 : 0) << "\n"
    << "handoff=" << (s.handoff ? 1 : 0) << "\n"
    << "clip_rel=" << (s.clip_rel.empty() ? CubeAmbient_DefaultClipRel() : s.clip_rel) << "\n"
    << "source=" << (s.source.empty() ? "cube_webui" : s.source) << "\n"
    << "ts=" << s.ts << "\n";
  return o.str();
}

inline bool CubeAmbient_Parse(const std::string& body, AmbientClipSnapshot& out) {
  out = AmbientClipSnapshot{};
  bool got = false;
  std::istringstream in(body);
  std::string line;
  while (std::getline(in, line)) {
    AmbientClip_Trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = line.substr(0, eq);
    std::string v = line.substr(eq + 1);
    AmbientClip_Trim(k);
    AmbientClip_Trim(v);
    if (k == "v" || k == "version")
      out.version = std::atoi(v.c_str());
    else if (k == "gain") {
      out.gain = std::strtof(v.c_str(), nullptr);
      got = true;
    } else if (k == "playing")
      out.playing = (v == "1" || v == "true");
    else if (k == "handoff")
      out.handoff = (v == "1" || v == "true");
    else if (k == "clip_rel" || k == "clip")
      out.clip_rel = v.empty() ? CubeAmbient_DefaultClipRel() : v;
    else if (k == "source")
      out.source = v.empty() ? "cube_webui" : v;
    else if (k == "ts")
      out.ts = std::atol(v.c_str());
  }
  if (out.version <= 0) out.version = 1;
  if (out.gain < 0.f) out.gain = 0.f;
  if (out.gain > 1.f) out.gain = 1.f;
  if (out.clip_rel.empty()) out.clip_rel = CubeAmbient_DefaultClipRel();
  // Consistency: playing implies handoff policy wants audio
  if (out.playing && !out.handoff) out.handoff = true;
  out.valid = got || out.playing || out.handoff;
  return out.valid;
}

// Panel/status one-liner (pure).
inline std::string CubeAmbient_StatusLabel(float gain, bool playing, bool clipPresent) {
  if (!playing || gain < 0.02f) {
    if (clipPresent) return "CLIP READY · SILENT";
    return "CLIP CONTRACT · NO ASSET";
  }
  char buf[96];
  if (clipPresent)
    snprintf(buf, sizeof(buf), "CLIP HOLD · GAIN %.0f%%", gain * 100.f);
  else
    snprintf(buf, sizeof(buf), "CLIP MISSING · GAIN %.0f%%", gain * 100.f);
  return std::string(buf);
}
