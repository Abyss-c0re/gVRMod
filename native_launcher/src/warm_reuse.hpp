#pragma once
// G04: warm GMod process reuse — pure decision + request marker (offline-tested).
// Product still cold-spawns until CubeWarmReuseEnabled() is true AND map-change/attach
// is proven. warm_request records intent when a process is already up.
#include <cstdlib>
#include <sstream>
#include <string>

struct WarmReuseDecision {
  // cold_spawn | warm_request | warm_reuse
  std::string action = "cold_spawn";
  std::string reason = "no_process";
  std::string map;
  bool process_up = false;
  bool skip_spawn = false; // only true for warm_reuse when feature on
  bool valid = false;
};

struct WarmRequestSnapshot {
  int version = 1;
  std::string action = "warm_request";
  std::string reason;
  std::string map;
  std::string source = "cube_webui";
  long ts = 0;
  bool valid = false;
};

// Env opt-in pure (unit-tested). Product default off when env unset.
inline bool CubeWarmReuseWantEnv(const char* envVal) {
  if (!envVal || !envVal[0]) return false;
  char c = envVal[0];
  return c == '1' || c == 'y' || c == 'Y' || c == 't' || c == 'T';
}

// Default off. Careful HMD smoke: GVRMOD_WARM_REUSE=1 (skip Steam when process up).
// Map attach still GMod-side; changelevel remains allow_changelevel=false there.
inline bool CubeWarmReuseEnabled() {
  return CubeWarmReuseWantEnv(std::getenv("GVRMOD_WARM_REUSE"));
}

inline void WarmReuse_Trim(std::string& s) {
  while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
  if (i) s = s.substr(i);
}

// Pure eligibility. featureEnabled defaults to CubeWarmReuseEnabled().
inline WarmReuseDecision CubeWarmReuseDecide(bool processUp, bool forceCold, const std::string& map,
                                            bool featureEnabled = CubeWarmReuseEnabled()) {
  WarmReuseDecision d;
  d.process_up = processUp;
  d.map = map;
  d.valid = true;
  if (forceCold) {
    d.action = "cold_spawn";
    d.reason = "force_cold";
    d.skip_spawn = false;
    return d;
  }
  if (!processUp) {
    d.action = "cold_spawn";
    d.reason = "no_process";
    d.skip_spawn = false;
    return d;
  }
  std::string m = map;
  WarmReuse_Trim(m);
  if (m.empty()) {
    d.action = "cold_spawn";
    d.reason = "no_map";
    d.skip_spawn = false;
    return d;
  }
  d.map = m;
  // Process up + map known → warm-eligible
  if (featureEnabled) {
    d.action = "warm_reuse";
    d.reason = "eligible";
    d.skip_spawn = true;
  } else {
    d.action = "warm_request";
    d.reason = "eligible_deferred";
    d.skip_spawn = false;
  }
  return d;
}

// Map decision → boot kind token used by panel / logs.
inline std::string CubeWarmReuseBootKind(const WarmReuseDecision& d) {
  if (d.action == "warm_reuse") return "WARM_REUSE";
  if (d.action == "warm_request") return "WARM_DETECTED";
  return "COLD_SPAWN";
}

inline std::string CubeWarmReuse_Format(const WarmRequestSnapshot& s) {
  std::ostringstream o;
  o << "v=" << (s.version > 0 ? s.version : 1) << "\n"
    << "action=" << (s.action.empty() ? "warm_request" : s.action) << "\n"
    << "reason=" << s.reason << "\n"
    << "map=" << s.map << "\n"
    << "source=" << (s.source.empty() ? "cube_webui" : s.source) << "\n"
    << "ts=" << s.ts << "\n";
  return o.str();
}

inline bool CubeWarmReuse_Parse(const std::string& body, WarmRequestSnapshot& out) {
  out = WarmRequestSnapshot{};
  bool got = false;
  std::istringstream in(body);
  std::string line;
  while (std::getline(in, line)) {
    WarmReuse_Trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = line.substr(0, eq);
    std::string v = line.substr(eq + 1);
    WarmReuse_Trim(k);
    WarmReuse_Trim(v);
    if (k == "v" || k == "version")
      out.version = std::atoi(v.c_str());
    else if (k == "action") {
      out.action = v.empty() ? "warm_request" : v;
      got = true;
    } else if (k == "reason")
      out.reason = v;
    else if (k == "map") {
      out.map = v;
      got = true;
    } else if (k == "source")
      out.source = v.empty() ? "cube_webui" : v;
    else if (k == "ts")
      out.ts = std::atol(v.c_str());
  }
  if (out.version <= 0) out.version = 1;
  out.valid = got;
  return out.valid;
}

// Human note for handoff detail when warm is detected but deferred.
inline std::string CubeWarmReuseDetail(const WarmReuseDecision& d) {
  if (d.action == "warm_reuse")
    return "warm reuse · skip steam · map attach (experimental)";
  if (d.action == "warm_request")
    return "process was up · warm_request recorded · still cold-spawn";
  if (d.reason == "force_cold")
    return "force cold · Steam/hl2 spawn";
  return "cold Steam/hl2 · holding OpenXR · GMod booting";
}

// ── G04 skip-spawn plan (pure) ─────────────────────────────────────────────
// When process is up and feature on: file markers + enter handoff without Steam.
// Does not know live GMod map (attach is GMod-side). Never changelevels here.

struct WarmSkipSpawnPlan {
  bool skip_spawn = false;
  bool write_markers = false; // openxr_launch + handoff phase + warm already written
  bool write_stage_pack = false;
  std::string initial_phase = "warm_attach"; // cube_handoff.txt phase
  std::string detail;
  std::string reason = "none";
  bool valid = false;
};

// Pure plan from reuse decision. featureEnabled defaults to product gate.
inline WarmSkipSpawnPlan CubeWarmSkipSpawnPlanDecide(const WarmReuseDecision& reuse,
                                                     bool featureEnabled = CubeWarmReuseEnabled()) {
  WarmSkipSpawnPlan p;
  p.valid = true;
  if (!reuse.valid) {
    p.reason = "invalid_reuse";
    p.detail = "cold Steam/hl2 · holding OpenXR · GMod booting";
    return p;
  }
  if (!featureEnabled || !reuse.skip_spawn || reuse.action != "warm_reuse") {
    p.skip_spawn = false;
    p.write_markers = false;
    p.write_stage_pack = false;
    p.reason = reuse.skip_spawn ? "feature_off" : reuse.reason;
    p.detail = CubeWarmReuseDetail(reuse);
    p.initial_phase = "spawned";
    return p;
  }
  // Feature on + skip_spawn: no Steam; file attach markers; wait take_xr
  p.skip_spawn = true;
  p.write_markers = true;
  p.write_stage_pack = true;
  p.initial_phase = "warm_attach";
  p.reason = "skip_spawn_attach";
  p.detail = "warm process · skip Steam · attach markers · waiting take_xr…";
  return p;
}

inline std::string CubeWarmSkipSpawnPhaseLabel(const std::string& phase) {
  if (phase == "warm_attach") return "WARM ATTACH";
  if (phase == "warm_wait_map") return "WARM WAIT MAP";
  if (phase == "warm_ready") return "WARM READY";
  return phase;
}

// ── G04 map attach (pure) ──────────────────────────────────────────────────
// When a warm process is up, Cube files cube_warm.txt with target map.
// GMod may later changelevel / attach without a second Steam spawn.
// Law: allow_changelevel defaults false — no auto changelevel until HMD-proven.

struct WarmAttachDecision {
  // idle | same_map | changelevel | deferred | reject
  std::string action = "idle";
  std::string reason = "none";
  std::string request_map;
  std::string current_map;
  bool would_changelevel = false;
  bool valid = false;
};

// Normalize map token for compare: trim, lower, strip maps/, strip .bsp
inline std::string CubeWarmAttach_NormalizeMap(const std::string& raw) {
  std::string s = raw;
  WarmReuse_Trim(s);
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (c == '\\') c = '/';
  }
  // strip leading maps/
  if (s.rfind("maps/", 0) == 0) s = s.substr(5);
  // strip .bsp
  if (s.size() > 4 && s.compare(s.size() - 4, 4, ".bsp") == 0)
    s = s.substr(0, s.size() - 4);
  WarmReuse_Trim(s);
  return s;
}

// Pure attach decision from warm request + live map.
// allow_changelevel=false → maps differ becomes deferred (product default).
inline WarmAttachDecision CubeWarmAttachDecide(bool requestValid, const std::string& requestMap,
                                              const std::string& requestAction,
                                              const std::string& currentMap,
                                              bool allowChangelevel = false) {
  WarmAttachDecision d;
  d.valid = true;
  d.request_map = CubeWarmAttach_NormalizeMap(requestMap);
  d.current_map = CubeWarmAttach_NormalizeMap(currentMap);
  if (!requestValid) {
    d.action = "idle";
    d.reason = "no_request";
    return d;
  }
  if (d.request_map.empty()) {
    d.action = "reject";
    d.reason = "no_map";
    return d;
  }
  // Optional: only warm_request / warm_reuse actions are attach-relevant
  std::string act = requestAction;
  WarmReuse_Trim(act);
  for (char& c : act) {
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
  }
  if (!act.empty() && act != "warm_request" && act != "warm_reuse") {
    d.action = "reject";
    d.reason = "bad_action";
    return d;
  }
  if (d.current_map.empty()) {
    // No live map yet (menu) — defer change until in-game map known
    d.action = allowChangelevel ? "changelevel" : "deferred";
    d.reason = allowChangelevel ? "menu_or_unknown" : "eligible_deferred";
    d.would_changelevel = allowChangelevel;
    return d;
  }
  if (d.request_map == d.current_map) {
    d.action = "same_map";
    d.reason = "already_on_map";
    d.would_changelevel = false;
    return d;
  }
  if (allowChangelevel) {
    d.action = "changelevel";
    d.reason = "eligible";
    d.would_changelevel = true;
  } else {
    d.action = "deferred";
    d.reason = "eligible_deferred";
    d.would_changelevel = false;
  }
  return d;
}

inline std::string CubeWarmAttachToast(const WarmAttachDecision& d) {
  if (!d.valid || d.action == "idle") return {};
  if (d.action == "same_map")
    return "Warm attach · same map · changelevel not needed";
  if (d.action == "deferred") {
    if (!d.current_map.empty())
      return "Warm attach · want " + d.request_map + " · on " + d.current_map + " · deferred";
    return "Warm attach · want " + d.request_map + " · changelevel deferred";
  }
  if (d.action == "changelevel")
    return "Warm attach · changelevel → " + d.request_map;
  if (d.action == "reject")
    return "Warm attach · rejected (" + d.reason + ")";
  return {};
}
