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

// ── G04 careful changelevel plan (pure, default OFF) ───────────────────────
// Product never auto-changelevels unless GVRMOD_WARM_CHANGELEVEL / GMod opt-in.
// Map tokens must be [a-z0-9_]+ after normalize (console-safe).

inline bool CubeWarmChangelevelWantEnv(const char* envVal) {
  return CubeWarmReuseWantEnv(envVal);
}

// Default off. Careful HMD smoke: GVRMOD_WARM_CHANGELEVEL=1 (+ warm reuse path).
inline bool CubeWarmChangelevelEnabled() {
  return CubeWarmChangelevelWantEnv(std::getenv("GVRMOD_WARM_CHANGELEVEL"));
}

// Safe map token for changelevel cmd: lowercase alnum + underscore, 1..64.
inline bool CubeWarmAttach_MapTokenOk(const std::string& raw) {
  std::string s = CubeWarmAttach_NormalizeMap(raw);
  if (s.empty() || s.size() > 64) return false;
  for (char c : s) {
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') continue;
    return false;
  }
  return true;
}

// Pure allow gate (unit-tested). force/convar/file/env any true → allow.
struct WarmChangelevelAllowFlags {
  bool convar_on = false;
  bool file_enable = false;
  bool env_on = false;
  bool force = false;
};

inline bool CubeWarmAttach_AllowChangelevelFromFlags(const WarmChangelevelAllowFlags& f) {
  return f.force || f.convar_on || f.file_enable || f.env_on;
}

struct WarmChangelevelPlan {
  bool valid = false;
  bool do_changelevel = false;
  std::string map;
  std::string from_map;
  std::string method = "none"; // none | changelevel
  std::string reason = "none";
  std::string cmd;
};

// Pure plan from attach decision. do_changelevel only when action=changelevel + token ok.
inline WarmChangelevelPlan CubeWarmChangelevelPlanDecide(const WarmAttachDecision& d) {
  WarmChangelevelPlan p;
  p.valid = true;
  if (!d.valid) {
    p.valid = false;
    p.reason = "invalid_decision";
    return p;
  }
  p.map = CubeWarmAttach_NormalizeMap(d.request_map);
  p.from_map = CubeWarmAttach_NormalizeMap(d.current_map);
  if (d.action == "same_map") {
    p.reason = "already_on_map";
    return p;
  }
  if (d.action == "idle" || d.action == "reject") {
    p.reason = d.reason.empty() ? d.action : d.reason;
    return p;
  }
  if (d.action == "deferred") {
    p.reason = "eligible_deferred";
    return p;
  }
  if (d.action != "changelevel" || !d.would_changelevel) {
    p.reason = "not_armed";
    return p;
  }
  if (!CubeWarmAttach_MapTokenOk(p.map)) {
    p.reason = "bad_map_token";
    return p;
  }
  p.do_changelevel = true;
  p.method = "changelevel";
  p.reason = d.reason.empty() ? "eligible" : d.reason;
  p.cmd = "changelevel " + p.map;
  return p;
}

inline std::string CubeWarmChangelevelCmd(const WarmChangelevelPlan& p) {
  if (!p.valid || !p.do_changelevel) return {};
  if (!p.cmd.empty()) return p.cmd;
  if (CubeWarmAttach_MapTokenOk(p.map)) return "changelevel " + p.map;
  return {};
}

inline bool CubeWarmShouldExecuteChangelevel(const WarmChangelevelPlan& p, bool allow) {
  if (!allow) return false;
  if (!p.valid || !p.do_changelevel) return false;
  if (p.method != "changelevel") return false;
  return CubeWarmAttach_MapTokenOk(p.map);
}

inline std::string CubeWarmChangelevelExecuteToast(bool applied, bool ok, const std::string& map,
                                                   const std::string& err) {
  if (applied && ok) return "Warm attach · changelevel → " + map;
  if (!err.empty()) return "Warm attach · changelevel failed · " + err;
  return {};
}

// G04 HMD warm attach observer contract (offline tokens — does not prove headset).
struct WarmHmdExpect {
  std::string verdict = "idle"; // idle|expect_same_map|expect_deferred|expect_changelevel|expect_reject
  bool expect_no_changelevel = true;
  std::string checklist;
  std::string pass_line;
  std::string fail_line;
};

inline WarmHmdExpect CubeWarm_HmdExpect(const WarmAttachDecision& d, const WarmChangelevelPlan* plan,
                                        bool execApplied = false, bool reuseSkipSpawn = false) {
  WarmHmdExpect e;
  e.expect_no_changelevel = true;
  if (!d.valid) {
    e.checklist = "G04 · IDLE · no warm request";
    e.pass_line = "N/A";
    e.fail_line = "N/A";
    return e;
  }
  if (execApplied) {
    e.verdict = "expect_changelevel";
    e.expect_no_changelevel = false;
    e.checklist = "G04 · CHANGELEVEL · opt-in applied · " + d.request_map;
    e.pass_line = "Map switches once; stereo returns; XR kept";
    e.fail_line = "Map thrash / XR death / unsolicited changelevel";
    return e;
  }
  if (d.action == "idle") {
    e.verdict = "idle";
    e.checklist = reuseSkipSpawn ? "G04 · WARM SKIP · no attach marker"
                                 : "G04 · COLD · no warm request";
    e.pass_line = reuseSkipSpawn ? "Process reused; no accidental map flip"
                                 : "Cold Steam spawn still valid default";
    e.fail_line = "Silent map flip without cube_warm";
    return e;
  }
  if (d.action == "same_map") {
    e.verdict = "expect_same_map";
    e.checklist = "G04 · SAME MAP · " + d.request_map + " · no changelevel";
    e.pass_line = "Already on target; handoff without map load";
    e.fail_line = "Forced changelevel on same map";
    return e;
  }
  if (d.action == "reject") {
    e.verdict = "expect_reject";
    e.checklist = "G04 · REJECT · " + d.reason;
    e.pass_line = "Bad token ignored";
    e.fail_line = "Injected changelevel from bad map";
    return e;
  }
  if (d.action == "changelevel" || (plan && plan->do_changelevel)) {
    e.verdict = "expect_changelevel";
    e.expect_no_changelevel = false;
    e.checklist = "G04 · ARMED · changelevel → " + d.request_map;
    e.pass_line = "Opt-in changelevel once; dual-hold through load";
    e.fail_line = "Map thrash / mono load flash";
    return e;
  }
  if (d.action == "deferred") {
    e.verdict = "expect_deferred";
    e.checklist = "G04 · DEFERRED · want " + d.request_map + " · default no RCC";
    e.pass_line = "Toast deferred only; no auto changelevel";
    e.fail_line = "Silent changelevel without opt-in";
    return e;
  }
  e.checklist = "G04 · HOLD · action=" + d.action;
  e.pass_line = "Observe only";
  e.fail_line = "Unexpected map thrash";
  return e;
}
