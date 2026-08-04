#pragma once
#include "ambient_clip.hpp"
#include "cube_return.hpp"
#include "stage_pack.hpp"
#include "warm_reuse.hpp"
#include <string>

// GMod/Source graphics + OpenXR (vrmod_*) applied via +exec cfg
struct GfxLaunch {
  int matPicmip = 0;
  int rRootLod = 0;
  int matAntialias = 4;
  int matForceAniso = 8;
  int matHdrLevel = 2;
  bool shadows = true;
  bool flashlightShadows = true;
  bool specular = true;
  bool bumpmap = true;
  bool waterExpensive = true;
  bool multicore = true;
  int fpsMax = 0; // 0 = unlimited

  // OpenXR backend (vrmod module convars)
  float xrSupersample = 1.5f;
  float xrViewScale = 1.0f;
  // Independent X/Y — never force-link over archived asymmetric Vision cal
  float xrFovScaleX = 1.0f;
  float xrFovScaleY = 1.0f;
  bool xrWriteFov = false; // only write fovscale when user touched SETTINGS
  float xrScaleFactor = 1.0f;
  float xrEyeScale = 0.5f;
  float xrZNear = 1.0f;
  // 1=none (Cube seamless / shell policy), 2=left, 3=right
  int xrDesktopView = 1;
  bool xrPostProcess = false;
  bool xrSwapEyes = false;
  bool xrSkybox = false;
  bool xrMq2SinglePass = true;
  bool xrRenderOffset = true;
  bool xrRequireFocus = false;
};

struct LaunchRequest {
  std::string gmodRoot;
  std::string map = "gm_construct";
  int maxPlayers = 1;
  std::string hostname = "gVRMod Cube";
  bool svLan = true;
  bool p2p = false;
  bool p2pFriends = false;
  std::string gamemode = "sandbox";
  int winW = 720;
  int winH = 480;
  bool windowed = true;
  bool noborder = false; // keep normal window chrome (user can move/resize)
  bool useSteam = true;
  std::string xrRuntimeJson;
  std::string cubeCfg = "gvrmod_cube";
  GfxLaunch gfx;
};

// Write openxr_launch marker + gvrmod_cube.cfg (VR + GMod graphics), spawn GMod.
// Returns 0 on spawn success. Does not take OpenXR — caller keeps session until handoff.
int SpawnGModFromWebUI(const LaunchRequest& req, std::string& errOut);

bool GModProcessRunning();
std::string ReadCubeHandoffPhase(const std::string& gmodRoot);
void ClearCubeHandoffMarkers(const std::string& gmodRoot);

// G03: write cube_stage_pack.txt (STAGE/LOCAL + head sample). Does not clear on handoff markers.
// Applying pack in GMod is intentionally separate — this only persists continuity data.
bool WriteCubeStagePack(const std::string& gmodRoot, const StagePackSnapshot& pack);

// G12: write cube_ambient.txt (gain/playing/clip_present/clip_rel). Status SoT only.
bool WriteCubeAmbientStatus(const std::string& gmodRoot, const AmbientClipSnapshot& snap);

// G12: resolve assets/ root + absolute clip path; clipPresent when file readable.
// Player spawn stays behind CubeAmbientPlayerEnabled hard-off.
std::string ResolveCubeAmbientAssetsDir();
bool CubeAmbientClipPresent(const std::string& absClipPath);
// Fill snap.clip_rel default, resolve path, set clip_present. Returns abs path (may be empty).
std::string FillCubeAmbientClipPaths(AmbientClipSnapshot& snap);

// G04: write cube_warm.txt when process already up (intent only; still cold-spawns).
bool WriteCubeWarmRequest(const std::string& gmodRoot, const WarmRequestSnapshot& snap);

// G04: warm attach markers without Steam spawn (openxr_launch + handoff phase).
// Used only when skip-spawn plan is active (CubeWarmReuseEnabled / env).
bool WriteWarmAttachMarkers(const std::string& gmodRoot, const std::string& map,
                            const std::string& phase = "warm_attach");

// G13: read cube_return.txt (reverse handoff marker). Pure parse lives in cube_return.hpp.
// Returns true when file present and body parses. Does not auto-reclaim XR.
bool ReadCubeReturnMarker(const std::string& gmodRoot, CubeReturnSnapshot& out);

// G13: write/overwrite cube_return.txt (soft ack → panel_live).
bool WriteCubeReturnMarker(const std::string& gmodRoot, const CubeReturnSnapshot& snap);

// G04: cold Steam/hl2 Start inventory — pure strategy labels.
// Warm reuse: see warm_reuse.hpp (CubeWarmReuseEnabled hard-off until attach proven).
inline std::string CubeLaunchBootKind(bool gmodAlreadyRunning, bool forceCold = false) {
  if (forceCold) return "COLD_SPAWN";
  if (gmodAlreadyRunning) return "WARM_DETECTED";
  return "COLD_SPAWN";
}

inline std::string CubeLaunchBootLabel(const std::string& kind) {
  if (kind == "WARM_DETECTED") return "WARM DETECTED · REQUEST FILED";
  if (kind == "WARM_REUSE") return "WARM REUSE"; // only if feature enabled
  return "COLD SPAWN · STEAM/HL2";
}

// Honest cold Facepunch gap for time-based progress fallback (seconds).
inline float CubeColdStartProgressSeconds() { return 55.f; }

// Skip steam only when decision.skip_spawn (requires CubeWarmReuseEnabled).
inline bool CubeLaunchShouldSkipSpawn(const WarmReuseDecision& d) {
  return d.skip_spawn && d.action == "warm_reuse";
}
// Legacy kind string — never skips without feature (kept for tests/call sites).
inline bool CubeLaunchShouldSkipSpawn(const std::string& kind) {
  return kind == "WARM_REUSE" && CubeWarmReuseEnabled();
}

// G13 reverse labels: see cube_return.hpp (CubeReversePhaseLabel / Detail / Progress).

// Pure helpers for Cube handoff panel (G01) — no I/O; unit-tested offline.
// Map status-file phase tokens → human detail / progress / display label.
inline std::string CubeHandoffDetailForPhase(const std::string& phase, bool gmodUp) {
  if (phase == "take_xr" || phase == "ready")
    return "GMod claims OpenXR · coordinated fade · releasing…";
  if (phase == "starting_xr")
    return "starting OpenXR in GMod · stay in headset…";
  if (phase == "vr_active")
    return "VR live in GMod · handoff complete";
  if (phase == "wait_module")
    return "GMod up · loading vrmod module…";
  if (phase == "map_ready")
    return "map loaded · preparing VR start…";
  if (phase == "boot")
    return "Lua boot · openxr launch path…";
  if (phase == "spawned" || phase == "SPAWNED")
    return gmodUp ? "GMod process up · waiting Lua…" : "spawned · booting hl2…";
  if (phase == "gmod_process")
    return "GMod live · waiting addon signal…";
  if (phase == "warm_attach")
    return "warm process · attach markers · waiting take_xr…";
  if (phase == "warm_wait_map")
    return "warm attach · map continuity · waiting GMod…";
  if (phase == "warm_ready")
    return "warm attach ready · preparing take_xr…";
  if (phase == "waiting_process")
    return "cold Steam/hl2 boot · panel holds OpenXR…";
  return gmodUp ? "GMod live · waiting take_xr (seamless)"
                : "cold Steam/hl2 boot · panel holds OpenXR…";
}

// Progress 0..1 for known phases; negative → caller uses time-based fallback.
inline float CubeHandoffProgressForPhase(const std::string& phase) {
  if (phase == "waiting_process") return 0.08f;
  if (phase == "spawned" || phase == "SPAWNED") return 0.12f;
  if (phase == "warm_attach") return 0.28f;
  if (phase == "warm_wait_map") return 0.35f;
  if (phase == "warm_ready") return 0.48f;
  if (phase == "gmod_process") return 0.22f;
  if (phase == "boot") return 0.38f;
  if (phase == "map_ready") return 0.52f;
  if (phase == "wait_module") return 0.62f;
  if (phase == "take_xr" || phase == "ready") return 0.78f;
  if (phase == "starting_xr") return 0.88f;
  if (phase == "vr_active") return 0.98f;
  return -1.f;
}

// Display label for PHASE line (uppercase words; empty phase → SPAWNING).
inline std::string CubeHandoffPhaseLabel(const std::string& phase) {
  if (phase.empty()) return "SPAWNING";
  if (phase == "spawned" || phase == "SPAWNED") return "SPAWNED";
  if (phase == "waiting_process") return "WAITING PROCESS";
  if (phase == "gmod_process") return "GMOD PROCESS";
  if (phase == "warm_attach") return "WARM ATTACH";
  if (phase == "warm_wait_map") return "WARM WAIT MAP";
  if (phase == "warm_ready") return "WARM READY";
  if (phase == "boot") return "LUA BOOT";
  if (phase == "map_ready") return "MAP READY";
  if (phase == "wait_module") return "WAIT MODULE";
  if (phase == "take_xr") return "TAKE XR · FADE";
  if (phase == "starting_xr") return "STARTING XR";
  if (phase == "vr_active") return "VR ACTIVE";
  if (phase == "ready") return "READY · FADE";
  return phase;
}

// G02: intentional fade toward black during take_xr / session release.
// Returns 0..1. Used for (1) panel buffer dim and (2) full eye-swapchain overlay alpha.
// Runtime XR composition-layer flag fade is not required — content fade is enough for cut.
inline float CubeHandoffFadeAmount(const std::string& phase, bool exitRequested, float exitWaitSec) {
  if (exitRequested) {
    // Ramp over ~2.5s of orderly release (matches Lua wait budget)
    float f = exitWaitSec / 2.5f;
    if (f < 0.f) f = 0.f;
    if (f > 1.f) f = 1.f;
    return f;
  }
  if (phase == "take_xr" || phase == "ready") return 0.28f;
  if (phase == "starting_xr") return 0.45f;
  if (phase == "vr_active") return 0.65f;
  return 0.f;
}

// G02: clamp black-overlay alpha for eye buffers (0 clear · 1 solid black).
inline float CubeHandoffLayerFadeAlpha(float fadeAmount) {
  if (fadeAmount < 0.f) return 0.f;
  if (fadeAmount > 1.f) return 1.f;
  return fadeAmount;
}

// G28: soft handoff timeout law (pure). Pain soft-care: 90s/180s; never racey early release.
// Soft: GMod process up but never signaled take_xr — wait long enough for cold boot.
// Hard: absolute panel-hold ceiling, then orderly xrRequestExitSession (not mid-frame destroy).
// Racey: releasing before soft when process is down and phase is not take_xr (void/gap).
inline float CubeHandoffSoftReleaseSeconds() { return 90.f; }
inline float CubeHandoffHardTimeoutSeconds() { return 180.f; }

struct HandoffTimeoutDecision {
  bool valid = true;
  bool take_xr = false;
  bool gmod_up = false;
  float elapsed = 0.f;
  float soft_sec = 90.f;
  float hard_sec = 180.f;
  bool soft_due = false;
  bool hard_due = false;
  bool should_release = false;
  bool racey = false; // true if product would release too early (must not ship)
  /// none | take_xr | soft | hard | hold
  std::string risk = "none";
  std::string reason = "ok";
};

struct HandoffTimeoutHmdExpect {
  std::string verdict = "idle"; // idle | expect_hold | expect_take_xr | expect_soft | expect_hard | expect_race_fail
  bool expect_orderly = true;
  std::string checklist = "G28 · IDLE · no handoff timeout decision";
  std::string pass_line = "N/A";
  std::string fail_line = "N/A";
};

/// Pure release gate. elapsed_sec is panel hold time during ui.handoff.
inline HandoffTimeoutDecision CubeHandoffTimeout_Decide(bool takeXrSignal, bool gmodUp, float elapsedSec) {
  HandoffTimeoutDecision d;
  d.take_xr = takeXrSignal;
  d.gmod_up = gmodUp;
  d.elapsed = elapsedSec < 0.f ? 0.f : elapsedSec;
  d.soft_sec = CubeHandoffSoftReleaseSeconds();
  d.hard_sec = CubeHandoffHardTimeoutSeconds();
  d.soft_due = gmodUp && d.elapsed > d.soft_sec;
  d.hard_due = d.elapsed > d.hard_sec;
  d.should_release = d.take_xr || d.soft_due || d.hard_due;
  // Race: release without take_xr when process not up and before hard ceiling.
  d.racey = d.should_release && !d.take_xr && !gmodUp && !d.hard_due;
  if (d.racey) {
    d.risk = "racey";
    d.reason = "early_release_no_gmod";
    d.should_release = false; // law: refuse racey path
  } else if (d.take_xr) {
    d.risk = "take_xr";
    d.reason = "phase_take_xr";
  } else if (d.soft_due) {
    d.risk = "soft";
    d.reason = "soft_90s_gmod_up_no_signal";
  } else if (d.hard_due) {
    d.risk = "hard";
    d.reason = "hard_180s_ceiling";
  } else {
    d.risk = "hold";
    d.reason = "panel_holds_openxr";
  }
  return d;
}

inline std::string CubeHandoffTimeout_StatusLabel(const HandoffTimeoutDecision& d) {
  if (!d.valid) return "HAND · IDLE";
  if (d.racey) return "HAND · RACEY FORBID";
  if (d.risk == "take_xr") return "HAND · TAKE XR";
  if (d.risk == "soft") return "HAND · SOFT 90S";
  if (d.risk == "hard") return "HAND · HARD 180S";
  if (d.should_release) return "HAND · RELEASE";
  return "HAND · HOLD";
}

inline HandoffTimeoutHmdExpect CubeHandoffTimeout_HmdExpect(const HandoffTimeoutDecision& d) {
  HandoffTimeoutHmdExpect e;
  if (!d.valid) return e;
  if (d.racey) {
    e.verdict = "expect_race_fail";
    e.expect_orderly = false;
    e.checklist = "G28 · RACEY · must not early-release without GMod";
    e.pass_line = "Hold XR until take_xr / soft 90s with process / hard 180s";
    e.fail_line = "Session dropped while GMod still booting (void)";
    return e;
  }
  if (d.risk == "take_xr") {
    e.verdict = "expect_take_xr";
    e.checklist = "G28 · TAKE XR · orderly release after claim";
    e.pass_line = "Coordinated fade then xrRequestExitSession";
    e.fail_line = "Hard destroy mid-frame or no fade";
    return e;
  }
  if (d.risk == "soft") {
    e.verdict = "expect_soft";
    e.checklist = "G28 · SOFT 90S · GMod up · no take_xr signal";
    e.pass_line = "Orderly release after long wait with process live";
    e.fail_line = "Race release before 90s without take_xr";
    return e;
  }
  if (d.risk == "hard") {
    e.verdict = "expect_hard";
    e.checklist = "G28 · HARD 180S · absolute ceiling";
    e.pass_line = "Release after 180s even if stuck";
    e.fail_line = "Infinite hold with dead handoff";
    return e;
  }
  e.verdict = "expect_hold";
  e.checklist = "G28 · HOLD · panel owns OpenXR · t=" + std::to_string((int)d.elapsed) + "s";
  e.pass_line = "Seamless hold through cold Steam/hl2 boot";
  e.fail_line = "Early void / racey release";
  return e;
}

// G12: handoff ambient gain law (0..1). Pure contract for optional Cube ambient clip.
// No audio engine required — panel status + future OpenAL/Sound source share this curve.
// Intent: hold presence while GMod boots, duck at take_xr, silence when session releases.
inline float CubeHandoffAudioGain(const std::string& phase, bool exitRequested, float exitWaitSec) {
  if (exitRequested) {
    // Mirror fade window: gain → 0 over ~2.5s orderly release
    float f = exitWaitSec / 2.5f;
    if (f < 0.f) f = 0.f;
    if (f > 1.f) f = 1.f;
    return 1.f - f;
  }
  if (phase == "vr_active") return 0.f;
  if (phase == "starting_xr") return 0.18f;
  if (phase == "take_xr" || phase == "ready") return 0.35f;
  if (phase == "wait_module") return 0.55f;
  if (phase == "map_ready") return 0.72f;
  if (phase == "boot") return 0.88f;
  // spawned / waiting_process / gmod_process / unknown hold → full ambient
  return 1.f;
}
