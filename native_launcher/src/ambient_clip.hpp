#pragma once
// G12: Cube handoff ambient clip contract — pure format/parse + player decide.
// Gain curve lives in CubeHandoffAudioGain; this names the clip + play/stop policy.
// Status file cube_ambient.txt is the SoT. OpenAL/paplay playback is feature-gated.
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

struct AmbientClipSnapshot {
  int version = 1;
  float gain = 1.f;           // 0..1 from handoff gain law
  bool playing = false;       // should a clip be audible
  bool handoff = false;
  bool clip_present = false;  // asset resolved on disk (I/O fills this)
  std::string clip_rel = "ambient/cube_hold.ogg"; // under launcher assets/
  std::string source = "cube_webui";
  long ts = 0;
  bool valid = false;
};

// Pure player decision (no process spawn here).
struct AmbientPlayerDecision {
  // idle | deferred | start | set_gain | stop
  std::string action = "idle";
  std::string reason = "none";
  float volume = 0.f; // effective 0..1 for a future backend
  bool want_audible = false;
  bool valid = false;
};

// Env opt-in pure (unit-tested). Product default remains off when env unset.
inline bool CubeAmbientPlayerWantEnv(const char* envVal) {
  if (!envVal || !envVal[0]) return false;
  char c = envVal[0];
  return c == '1' || c == 'y' || c == 'Y' || c == 't' || c == 'T';
}

// Default off. Careful HMD smoke: GVRMOD_AMBIENT_PLAY=1 (no rebuild).
// Presence + decide always run; spawn only when this is true.
inline bool CubeAmbientPlayerEnabled() {
  return CubeAmbientPlayerWantEnv(std::getenv("GVRMOD_AMBIENT_PLAY"));
}

// 0..100 for ffplay -volume / UI.
inline int CubeAmbient_VolumePercent(float volume01) {
  if (volume01 < 0.f) volume01 = 0.f;
  if (volume01 > 1.f) volume01 = 1.f;
  return (int)(volume01 * 100.f + 0.5f);
}

// Prefer ffplay (loop + volume). paplay is once-shot fallback.
inline const char* CubeAmbient_DefaultBackend() { return "ffplay"; }

// Restart external player when gain steps enough (ffplay cannot duck live).
inline bool CubeAmbient_ShouldRestartForGain(float oldVol, float newVol, float threshold = 0.15f) {
  float d = newVol - oldVol;
  if (d < 0.f) d = -d;
  return d >= threshold;
}

inline void AmbientClip_Trim(std::string& s) {
  while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
  if (i) s = s.substr(i);
}

// Pure argv for external player (no shell). backend: ffplay | paplay
inline std::vector<std::string> CubeAmbient_PlayArgv(const std::string& backend,
                                                     const std::string& absPath,
                                                     float volume01) {
  std::vector<std::string> argv;
  if (absPath.empty()) return argv;
  int vol = CubeAmbient_VolumePercent(volume01);
  if (vol < 1) vol = 1; // avoid total silence while "playing"
  std::string be = backend.empty() ? CubeAmbient_DefaultBackend() : backend;
  AmbientClip_Trim(be);
  for (char& c : be) {
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
  }
  if (be == "paplay") {
    // Pulse once-shot — no native volume/loop; gain law only gates start/stop
    argv.push_back("paplay");
    argv.push_back(absPath);
    return argv;
  }
  // ffplay: loop hold tone, volume 0..100, no video window
  argv.push_back("ffplay");
  argv.push_back("-nodisp");
  argv.push_back("-loglevel");
  argv.push_back("quiet");
  argv.push_back("-loop");
  argv.push_back("0");
  argv.push_back("-volume");
  argv.push_back(std::to_string(vol));
  argv.push_back(absPath);
  return argv;
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

// Ordered search roots for assets/ (pure list; first existing wins at I/O site).
// envAssets: GVRMOD_ASSETS; exeDir: directory containing cube_webui_launcher;
// sourceAssets: monorepo native_launcher/assets absolute if known.
inline std::vector<std::string> CubeAmbient_AssetsDirCandidates(const std::string& envAssets,
                                                                const std::string& exeDir,
                                                                const std::string& sourceAssets) {
  std::vector<std::string> out;
  auto push = [&](std::string p) {
    AmbientClip_Trim(p);
    if (p.empty()) return;
    while (!p.empty() && p.back() == '/') p.pop_back();
    for (const auto& e : out)
      if (e == p) return;
    out.push_back(p);
  };
  push(envAssets);
  if (!exeDir.empty()) {
    std::string d = exeDir;
    while (!d.empty() && d.back() == '/') d.pop_back();
    push(d + "/assets");
    // install/native → ../../native_launcher/assets (dev layout)
    push(d + "/../native_launcher/assets");
    push(d + "/../../native_launcher/assets");
  }
  push(sourceAssets);
  return out;
}

// Pure player FSM. featureEnabled defaults to CubeAmbientPlayerEnabled hard-off.
// currentlyPlaying = backend already has a clip active (process/OpenAL).
inline AmbientPlayerDecision CubeAmbient_PlayerDecide(bool handoff, float gain, bool clipPresent,
                                                      bool currentlyPlaying,
                                                      bool featureEnabled = CubeAmbientPlayerEnabled()) {
  AmbientPlayerDecision d;
  d.valid = true;
  d.volume = CubeAmbient_EffectiveVolume(gain, 1.f);
  const bool want = CubeAmbient_ShouldPlay(gain, handoff);
  d.want_audible = want && clipPresent;
  if (!clipPresent) {
    d.action = currentlyPlaying ? "stop" : "idle";
    d.reason = "clip_missing";
    d.want_audible = false;
    d.volume = 0.f;
    return d;
  }
  if (!want) {
    d.action = currentlyPlaying ? "stop" : "idle";
    d.reason = handoff ? "gain_floor" : "not_handoff";
    d.volume = 0.f;
    return d;
  }
  // Want audible + asset present
  if (!featureEnabled) {
    d.action = "deferred";
    d.reason = "eligible_deferred";
    // keep volume for status; no start
    return d;
  }
  if (!currentlyPlaying) {
    d.action = "start";
    d.reason = "eligible";
    return d;
  }
  d.action = "set_gain";
  d.reason = "audible";
  return d;
}

inline std::string CubeAmbient_Format(const AmbientClipSnapshot& s) {
  std::ostringstream o;
  o << "v=" << (s.version > 0 ? s.version : 1) << "\n"
    << "gain=" << s.gain << "\n"
    << "playing=" << (s.playing ? 1 : 0) << "\n"
    << "handoff=" << (s.handoff ? 1 : 0) << "\n"
    << "clip_present=" << (s.clip_present ? 1 : 0) << "\n"
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
    else if (k == "clip_present" || k == "clip_ok")
      out.clip_present = (v == "1" || v == "true");
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
  out.valid = got || out.playing || out.handoff || out.clip_present;
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

// Extra honesty when player is deferred but asset is ready.
inline std::string CubeAmbient_StatusLabelEx(float gain, bool playing, bool clipPresent,
                                            const AmbientPlayerDecision& dec) {
  if (dec.action == "deferred" && clipPresent && playing) {
    char buf[96];
    snprintf(buf, sizeof(buf), "CLIP READY · DEFERRED · GAIN %.0f%%", gain * 100.f);
    return std::string(buf);
  }
  if (dec.action == "start" || dec.action == "set_gain") {
    char buf[96];
    snprintf(buf, sizeof(buf), "CLIP PLAY · GAIN %.0f%%", gain * 100.f);
    return std::string(buf);
  }
  return CubeAmbient_StatusLabel(gain, playing, clipPresent);
}
