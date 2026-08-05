#pragma once
// G13: return-to-Cube reverse handoff — pure format/parse + reclaim poll decide.
// GMod writes garrysmod/data/vrmod/cube_return.txt on VR exit after Cube launch.
// Product: Cube shell polls the marker and may show a RETURN panel line.
// Auto reclaim (re-open / re-bind XR after GMod release) is HARD-OFF until proven.
#include <cstdlib>
#include <sstream>
#include <string>

struct CubeReturnSnapshot {
  int version = 1;
  std::string phase = "vr_exit"; // vr_exit | xr_released | cube_claim | panel_live
  std::string map;
  std::string source = "vrmod";
  // vr_exit | temp_return (GMod kept — RESUME) | resume
  std::string intent = "vr_exit";
  long ts = 0;
  bool valid = false;
};

struct CubeReclaimDecision {
  // idle | notify | reclaim_auto
  std::string action = "idle";
  std::string reason = "no_marker";
  std::string phase;
  std::string map;
  bool show_panel = false;
  bool auto_reclaim = false; // only true when feature on + eligible phase
  bool valid = false;
};

// Env opt-in for experimental auto reclaim path (unit-tested).
inline bool CubeReclaimWantEnv(const char* envVal) {
  if (!envVal || !envVal[0]) return false;
  char c = envVal[0];
  return c == '1' || c == 'y' || c == 'Y' || c == 't' || c == 'T';
}

// Hard XR rebind reclaim: off unless GVRMOD_CUBE_RECLAIM=1.
inline bool CubeReclaimEnabled() {
  return CubeReclaimWantEnv(std::getenv("GVRMOD_CUBE_RECLAIM"));
}

// Soft ack default ON: after brief RETURN banner, write phase=panel_live.
// Safe — Cube shell already owns OpenXR when panel is live; no session thrash.
inline bool CubeReclaimSoftAckEnabled() { return true; }

// Hold seconds before soft-acking return marker (pure constant).
inline float CubeReclaimSoftAckHoldSeconds() { return 2.5f; }

inline void CubeReturn_Trim(std::string& s) {
  while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
  if (i) s = s.substr(i);
}

inline std::string CubeReturn_NormalizePhase(const std::string& raw) {
  std::string s = raw;
  CubeReturn_Trim(s);
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
  }
  if (s == "vr_exit" || s == "xr_released" || s == "cube_claim" || s == "panel_live")
    return s;
  if (s.empty()) return "vr_exit";
  return s;
}

inline bool CubeReturn_IsReturnPhase(const std::string& phase) {
  const std::string p = CubeReturn_NormalizePhase(phase);
  return p == "vr_exit" || p == "xr_released" || p == "cube_claim";
}

// Panel / reverse labels (shared with CubeReverse* aliases in gmod_spawn.hpp).
inline std::string CubeReversePhaseLabel(const std::string& phase) {
  const std::string p = CubeReturn_NormalizePhase(phase);
  if (p == "vr_exit") return "VR EXIT";
  if (p == "xr_released") return "XR RELEASED";
  if (p == "cube_claim") return "CUBE CLAIM";
  if (p == "panel_live") return "PANEL LIVE";
  if (p.empty()) return "RETURN IDLE";
  return phase;
}

inline std::string CubeReverseDetailForPhase(const std::string& phase) {
  const std::string p = CubeReturn_NormalizePhase(phase);
  if (p == "vr_exit") return "GMod left VR · soft ack → panel live";
  if (p == "xr_released") return "OpenXR free · Cube shell already holds session";
  if (p == "cube_claim") return "Cube claiming OpenXR…";
  if (p == "panel_live") return "Cube panel live · reverse handoff complete";
  return "return-to-Cube protocol (future)";
}

inline float CubeReverseProgressForPhase(const std::string& phase) {
  const std::string p = CubeReturn_NormalizePhase(phase);
  if (p == "vr_exit") return 0.25f;
  if (p == "xr_released") return 0.5f;
  if (p == "cube_claim") return 0.75f;
  if (p == "panel_live") return 1.f;
  return -1.f;
}

inline std::string CubeReturn_NormalizeIntent(const std::string& raw) {
  std::string s = raw;
  CubeReturn_Trim(s);
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
  }
  if (s == "temp_return" || s == "temporary" || s == "launcher") return "temp_return";
  if (s == "resume") return "resume";
  if (s.empty() || s == "exit" || s == "vr_exit") return "vr_exit";
  return s;
}

inline std::string CubeReturn_Format(const CubeReturnSnapshot& s) {
  std::ostringstream o;
  o << "v=" << (s.version > 0 ? s.version : 1) << "\n"
    << "phase=" << CubeReturn_NormalizePhase(s.phase.empty() ? "vr_exit" : s.phase) << "\n"
    << "map=" << s.map << "\n"
    << "source=" << (s.source.empty() ? "vrmod" : s.source) << "\n"
    << "intent=" << CubeReturn_NormalizeIntent(s.intent.empty() ? "vr_exit" : s.intent) << "\n"
    << "ts=" << s.ts << "\n";
  return o.str();
}

inline bool CubeReturn_Parse(const std::string& body, CubeReturnSnapshot& out) {
  out = CubeReturnSnapshot{};
  bool gotPhase = false;
  std::istringstream in(body);
  std::string line;
  while (std::getline(in, line)) {
    CubeReturn_Trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = line.substr(0, eq);
    std::string v = line.substr(eq + 1);
    CubeReturn_Trim(k);
    CubeReturn_Trim(v);
    if (k == "v" || k == "version")
      out.version = std::atoi(v.c_str());
    else if (k == "phase") {
      out.phase = CubeReturn_NormalizePhase(v);
      gotPhase = true;
    } else if (k == "map")
      out.map = v;
    else if (k == "source")
      out.source = v.empty() ? "vrmod" : v;
    else if (k == "intent")
      out.intent = CubeReturn_NormalizeIntent(v);
    else if (k == "ts")
      out.ts = std::atol(v.c_str());
  }
  if (out.version <= 0) out.version = 1;
  if (out.phase.empty()) out.phase = "vr_exit";
  if (out.intent.empty()) out.intent = "vr_exit";
  out.valid = gotPhase;
  return out.valid;
}

// Pure reclaim poll decision. featureEnabled defaults to hard-off product gate.
// markerPresent = file readable + parse ok (caller owns I/O).
inline CubeReclaimDecision CubeReclaimDecide(bool markerPresent, const CubeReturnSnapshot& snap,
                                             bool featureEnabled = CubeReclaimEnabled()) {
  CubeReclaimDecision d;
  d.valid = true;
  if (!markerPresent || !snap.valid) {
    d.action = "idle";
    d.reason = "no_marker";
    d.show_panel = false;
    d.auto_reclaim = false;
    return d;
  }
  d.phase = CubeReturn_NormalizePhase(snap.phase);
  d.map = snap.map;
  if (d.phase == "panel_live") {
    d.action = "idle";
    d.reason = "already_live";
    d.show_panel = false;
    d.auto_reclaim = false;
    return d;
  }
  if (!CubeReturn_IsReturnPhase(d.phase)) {
    d.action = "idle";
    d.reason = "unknown_phase";
    d.show_panel = false;
    d.auto_reclaim = false;
    return d;
  }
  // Eligible reverse signal (vr_exit / xr_released / cube_claim)
  if (featureEnabled) {
    d.action = "reclaim_auto";
    d.reason = "eligible";
    d.show_panel = true;
    d.auto_reclaim = true;
  } else {
    d.action = "notify";
    d.reason = "eligible_deferred";
    d.show_panel = true;
    d.auto_reclaim = false;
  }
  return d;
}

// One-line panel RETURN label (pure).
inline std::string CubeReclaimPanelLabel(const CubeReclaimDecision& d) {
  if (!d.show_panel || d.action == "idle") return {};
  std::string pl = CubeReversePhaseLabel(d.phase);
  if (d.action == "reclaim_auto")
    return "RETURN  " + pl + "  ·  AUTO RECLAIM";
  return "RETURN  " + pl + "  ·  SOFT ACK";
}

inline std::string CubeReclaimDetail(const CubeReclaimDecision& d) {
  if (!d.show_panel || d.action == "idle") return {};
  if (d.action == "reclaim_auto")
    return "reverse eligible · auto reclaim path (experimental)";
  // Prefer phase detail; note deferred
  std::string base = CubeReverseDetailForPhase(d.phase);
  if (d.reason == "eligible_deferred")
    return base;
  return base;
}

// Soft reclaim ack plan: after hold, advance marker to panel_live (no XR rebind).
struct CubeReclaimAckPlan {
  bool should_write = false;
  std::string next_phase = "panel_live";
  bool clear_banner = false;
  std::string detail;
  bool valid = false;
};

// visibleSec = seconds RETURN banner has been active this session.
// softAck defaults to CubeReclaimSoftAckEnabled(); auto_reclaim also triggers ack.
inline CubeReclaimAckPlan CubeReclaimAckPlanDecide(const CubeReclaimDecision& d, float visibleSec,
                                                   float holdSec = CubeReclaimSoftAckHoldSeconds(),
                                                   bool softAck = CubeReclaimSoftAckEnabled()) {
  CubeReclaimAckPlan p;
  p.valid = true;
  p.next_phase = "panel_live";
  if (!d.show_panel || d.action == "idle") {
    p.detail = "no return signal";
    return p;
  }
  if (d.phase == "panel_live") {
    p.detail = "already panel_live";
    return p;
  }
  const bool wantAck = softAck || d.auto_reclaim;
  if (!wantAck) {
    p.detail = "ack disabled";
    return p;
  }
  if (holdSec < 0.5f) holdSec = 0.5f;
  if (visibleSec < holdSec) {
    p.should_write = false;
    p.clear_banner = false;
    p.detail = "RETURN hold · soft ack pending";
    return p;
  }
  p.should_write = true;
  p.clear_banner = true;
  p.next_phase = "panel_live";
  p.detail = "Cube panel live · reverse handoff acked";
  return p;
}

// ── G13 careful XR reclaim plan (env GVRMOD_CUBE_RECLAIM, default OFF) ───────
// Law: never destroy/create OpenXR session here. Cube shell already owns XR when
// the panel is live. "Reclaim" offline means panel refresh + marker ack only.
// allowActionRebind remains false until HMD-proven bindings thrash is safe.

struct CubeReclaimXrPlan {
  bool valid = false;
  bool do_anything = false;
  bool refresh_panel = false;       // mark UI dirty / status reclaim refresh
  bool clear_return_banner = false; // drop RETURN chrome after ack
  bool write_panel_live = false;    // ensure marker phase panel_live
  bool rebind_actions = false;      // experimental; product default false
  bool restart_session = false;     // always false — hard law
  // none | panel_refresh | full_rebind_deferred
  std::string method = "none";
  std::string reason = "none";
  std::string detail;
  std::string checklist; // HMD observer row
};

// Pure XR reclaim plan. featureEnabled defaults to CubeReclaimEnabled() (off).
// allowActionRebind defaults false — never thrash action sets offline.
inline CubeReclaimXrPlan CubeReclaimXrPlanDecide(const CubeReclaimDecision& d,
                                                 bool featureEnabled = CubeReclaimEnabled(),
                                                 bool allowActionRebind = false) {
  CubeReclaimXrPlan p;
  p.valid = true;
  p.restart_session = false;
  p.rebind_actions = false;
  if (!d.valid || d.action == "idle" || !d.show_panel) {
    p.reason = d.reason.empty() ? "no_return" : d.reason;
    p.method = "none";
    p.detail = "no XR reclaim plan";
    p.checklist = "G13 · IDLE · no return signal";
    return p;
  }
  if (d.phase == "panel_live") {
    p.reason = "already_live";
    p.method = "none";
    p.detail = "already panel_live";
    p.checklist = "G13 · PANEL LIVE · reverse complete";
    return p;
  }
  if (!featureEnabled || !d.auto_reclaim) {
    // Soft path only (handled by AckPlan) — XR plan stays empty
    p.reason = featureEnabled ? "not_auto" : "feature_off";
    p.method = "none";
    p.detail = "soft ack only · XR rebind deferred";
    p.checklist = "G13 · SOFT ACK · no XR rebind";
    return p;
  }
  // Feature on + reclaim_auto: safe panel refresh (session already Cube-owned)
  p.do_anything = true;
  p.refresh_panel = true;
  p.clear_return_banner = true;
  p.write_panel_live = true;
  p.method = "panel_refresh";
  p.reason = "eligible_panel";
  p.detail = "XR reclaim · panel refresh · session kept (no rebind thrash)";
  p.checklist = "G13 · PANEL REFRESH · session kept · marker panel_live";
  if (allowActionRebind) {
    // Documented future path — still not executed product-side offline
    p.rebind_actions = true;
    p.method = "full_rebind_deferred";
    p.reason = "rebind_deferred";
    p.detail = "XR reclaim · action rebind requested but deferred (not online)";
    p.checklist = "G13 · REBIND DEFERRED · panel refresh only";
  }
  return p;
}

// True when product may apply panel-side XR plan steps (not session restart).
inline bool CubeReclaimShouldExecuteXrPlan(const CubeReclaimXrPlan& p, bool featureEnabled) {
  if (!featureEnabled) return false;
  if (!p.valid || !p.do_anything) return false;
  if (p.restart_session) return false; // hard law
  return p.method == "panel_refresh" || p.method == "full_rebind_deferred";
}

inline std::string CubeReclaimXrPlanLabel(const CubeReclaimXrPlan& p) {
  if (!p.valid || p.method == "none") return {};
  if (p.method == "panel_refresh") return "RECLAIM · PANEL REFRESH";
  if (p.method == "full_rebind_deferred") return "RECLAIM · REBIND DEFERRED";
  return "RECLAIM · " + p.method;
}

// HMD observer contract for reverse handoff (offline tokens).
struct CubeReclaimHmdExpect {
  std::string verdict = "idle"; // expect_soft_ack | expect_panel_refresh | expect_live | idle
  bool expect_return_banner = false;
  bool expect_session_kept = true; // Cube keeps XR; no second session
  std::string checklist;
  std::string pass_line;
  std::string fail_line;
};

inline CubeReclaimHmdExpect CubeReclaim_HmdExpect(const CubeReclaimDecision& d,
                                                  const CubeReclaimXrPlan& xr) {
  CubeReclaimHmdExpect e;
  e.expect_session_kept = true;
  if (!d.show_panel || d.action == "idle") {
    if (d.phase == "panel_live" || d.reason == "already_live") {
      e.verdict = "expect_live";
      e.checklist = "G13 · PANEL LIVE · no RETURN banner";
      e.pass_line = "Cube panel usable after VR exit; tracking still valid";
      e.fail_line = "Black void / lost XR / need hard relaunch";
      return e;
    }
    e.verdict = "idle";
    e.checklist = "G13 · IDLE · no reverse signal";
    e.pass_line = "N/A";
    e.fail_line = "N/A";
    return e;
  }
  e.expect_return_banner = true;
  if (xr.do_anything && xr.method == "panel_refresh") {
    e.verdict = "expect_panel_refresh";
    e.checklist = xr.checklist.empty() ? "G13 · PANEL REFRESH" : xr.checklist;
    e.pass_line = "Brief RETURN then panel live; session never dies";
    e.fail_line = "OpenXR restart thrash or frozen RETURN banner";
    return e;
  }
  e.verdict = "expect_soft_ack";
  e.checklist = "G13 · SOFT ACK · RETURN then panel_live";
  e.pass_line = "RETURN banner ~2.5s then PANEL LIVE; no session recreate";
  e.fail_line = "Stuck RETURN forever or XR drops";
  return e;
}
