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

// Hard off until attach/map-change path is HMD-proven. Do not flip lightly.
inline bool CubeWarmReuseEnabled() { return false; }

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
