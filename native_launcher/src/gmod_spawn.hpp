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

// G35: viewscale fisheye law (pure). W8: extreme viewscale warps projection → fisheye.
// Product: default 1.0; clamp 0.1..2.0; comfort 0.75..1.25 flagged risk only.
inline float CubeViewScale_Default() { return 1.0f; }
inline float CubeViewScale_Min() { return 0.1f; }
inline float CubeViewScale_Max() { return 2.0f; }
inline float CubeViewScale_ComfortMin() { return 0.75f; }
inline float CubeViewScale_ComfortMax() { return 1.25f; }

inline float CubeViewScale_Clamp(float v) {
  if (v < CubeViewScale_Min()) return CubeViewScale_Min();
  if (v > CubeViewScale_Max()) return CubeViewScale_Max();
  return v;
}

inline bool CubeViewScale_IsFisheyeRisk(float v) {
  return v < CubeViewScale_ComfortMin() || v > CubeViewScale_ComfortMax();
}

struct ViewScaleDecision {
  bool valid = true;
  float requested = 1.0f;
  float applied = 1.0f;
  bool clamped = false;
  bool fisheye_risk = false;
  /// none | clamp | fisheye
  std::string risk = "none";
  std::string reason = "ok";
};

struct ViewScaleHmdExpect {
  std::string verdict = "idle";
  bool expect_natural = true;
  std::string checklist = "G35 · IDLE · no viewscale decision";
  std::string pass_line = "N/A";
  std::string fail_line = "N/A";
};

inline ViewScaleDecision CubeViewScale_Decide(float requested) {
  ViewScaleDecision d;
  d.requested = requested;
  d.applied = CubeViewScale_Clamp(requested);
  d.clamped = d.applied < requested - 1e-4f || d.applied > requested + 1e-4f;
  d.fisheye_risk = CubeViewScale_IsFisheyeRisk(d.applied);
  if (d.clamped) {
    d.risk = "clamp";
    d.reason = "viewscale_clamped";
  } else if (d.fisheye_risk) {
    d.risk = "fisheye";
    d.reason = "viewscale_outside_comfort";
  } else {
    d.risk = "none";
    d.reason = "viewscale_ok";
  }
  return d;
}

inline std::string CubeViewScale_StatusLabel(const ViewScaleDecision& d) {
  if (!d.valid) return "VS · IDLE";
  if (d.risk == "clamp") return "VS · CLAMP";
  if (d.risk == "fisheye") return "VS · FISHEYE RISK";
  return "VS · OK";
}

inline ViewScaleHmdExpect CubeViewScale_HmdExpect(const ViewScaleDecision& d) {
  ViewScaleHmdExpect e;
  if (!d.valid) return e;
  if (d.risk == "fisheye") {
    e.verdict = "expect_fisheye_risk";
    e.expect_natural = false;
    e.checklist = "G35 · FISHEYE RISK · applied viewscale outside 0.75..1.25";
    e.pass_line = "Reset viewscale to 1.0 / Vision defaults";
    e.fail_line = "Everything fisheye or tunnel vision";
    return e;
  }
  e.verdict = "expect_ok";
  e.checklist = "G35 · OK · viewscale natural";
  e.pass_line = "Natural edges; HMD projection live";
  e.fail_line = "Fisheye at default 1.0";
  return e;
}

// G29: supersample cold-start cap law (pure). Soft care: don't crank SS at Start.
// Product: gvrmod_cube.cfg writes min(requested, cold_cap). Live ladder may go higher
// after bring-up (user SETTINGS) — cold cfg must never inject 1.75/2.0 thrash.
// Ladder (UI): 0.75, 1.0, 1.25, 1.5, 1.75, 2.0. Cube high default: 1.5 (idx 3).
// Cold bring-up cap: 1.4 (historical product pin — smoother first frames).
inline float CubeSs_ColdStartCap() { return 1.4f; }
inline float CubeSs_CubeDefault() { return 1.5f; }
inline float CubeSs_LiveMax() { return 2.0f; }
inline float CubeSs_LiveMin() { return 0.5f; }

inline float CubeSs_LadderFromIdx(int idx) {
  static const float kSs[] = {0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
  if (idx < 0) idx = 0;
  if (idx > 5) idx = 5;
  return kSs[idx];
}

inline int CubeSs_ClampIdx(int idx) {
  if (idx < 0) return 0;
  if (idx > 5) return 5;
  return idx;
}

/// Clamp for cold Start cfg (+exec) — never crank above bring-up cap.
inline float CubeSs_ClampColdStart(float requested) {
  if (requested < CubeSs_LiveMin()) requested = CubeSs_LiveMin();
  if (requested > CubeSs_ColdStartCap()) return CubeSs_ColdStartCap();
  return requested;
}

/// Live SETTINGS / runtime ladder clamp (full range after bring-up).
inline float CubeSs_ClampLive(float requested) {
  if (requested < CubeSs_LiveMin()) return CubeSs_LiveMin();
  if (requested > CubeSs_LiveMax()) return CubeSs_LiveMax();
  return requested;
}

struct SupersampleDecision {
  bool valid = true;
  float requested = 1.5f;
  float applied = 1.4f;
  bool cold_start = true;
  bool capped = false;
  /// none | cold_capped | live | overcrank_risk
  std::string risk = "none";
  std::string reason = "ok";
};

struct SupersampleHmdExpect {
  std::string verdict = "idle";
  bool expect_smooth_bringup = true;
  std::string checklist = "G29 · IDLE · no supersample decision";
  std::string pass_line = "N/A";
  std::string fail_line = "N/A";
};

inline SupersampleDecision CubeSs_Decide(float requested, bool coldStart = true) {
  SupersampleDecision d;
  d.requested = requested;
  d.cold_start = coldStart;
  if (coldStart) {
    d.applied = CubeSs_ClampColdStart(requested);
    d.capped = d.applied < requested - 1e-4f || (requested > CubeSs_ColdStartCap());
    if (requested > CubeSs_ColdStartCap()) {
      d.risk = "cold_capped";
      d.reason = "cold_start_cap_1_4";
    } else {
      d.risk = "none";
      d.reason = "cold_within_cap";
    }
  } else {
    d.applied = CubeSs_ClampLive(requested);
    d.capped = d.applied < requested - 1e-4f;
    if (requested > CubeSs_LiveMax()) {
      d.risk = "overcrank_risk";
      d.reason = "live_above_ladder_max";
    } else {
      d.risk = "live";
      d.reason = "live_ladder";
    }
  }
  return d;
}

inline std::string CubeSs_StatusLabel(const SupersampleDecision& d) {
  if (!d.valid) return "SS · IDLE";
  if (d.cold_start && d.risk == "cold_capped") return "SS · COLD CAP 1.4";
  if (d.cold_start) return "SS · COLD OK";
  if (d.risk == "overcrank_risk") return "SS · LIVE CLAMP";
  return "SS · LIVE";
}

inline SupersampleHmdExpect CubeSs_HmdExpect(const SupersampleDecision& d) {
  SupersampleHmdExpect e;
  if (!d.valid) return e;
  if (d.cold_start) {
    e.verdict = "expect_cold_cap";
    e.expect_smooth_bringup = true;
    e.checklist = "G29 · COLD · applied=" + std::to_string(d.applied).substr(0, 4) +
                  " · cap=" + std::to_string(CubeSs_ColdStartCap()).substr(0, 4);
    e.pass_line = "First frames smooth; cfg SS ≤ 1.4";
    e.fail_line = "Cold Start injects 1.75/2.0 thrash / long hitch";
    return e;
  }
  e.verdict = "expect_live";
  e.checklist = "G29 · LIVE · SS=" + std::to_string(d.applied).substr(0, 4);
  e.pass_line = "User ladder after bring-up";
  e.fail_line = "Silent overcrank above 2.0";
  return e;
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

// G30: FOV archive write-only-when-touched law (pure). Soft care: Vision/border cal archives.
// Product: gvrmod_cube.cfg omits vrmod_fovscale_x/y unless user edited XR FOV in SETTINGS.
// Linked Start defaults (1.0/1.0) must never clobber asymmetric Vision cal left on disk.
// UI: fovTouched flips only when user cycles FOV scale; presets reset touched=false.
inline float CubeFov_DefaultScale() { return 1.0f; }
inline float CubeFov_MinScale() { return 0.1f; }
inline float CubeFov_MaxScale() { return 2.0f; }

/// Clamp one axis for cfg write (only used when writing).
inline float CubeFov_ClampScale(float s) {
  if (s < CubeFov_MinScale()) return CubeFov_MinScale();
  if (s > CubeFov_MaxScale()) return CubeFov_MaxScale();
  return s;
}

/// Law: write fovscale only when user intentionally touched SETTINGS FOV.
inline bool CubeFov_ShouldWrite(bool userTouched) { return userTouched; }

struct FovArchiveDecision {
  bool valid = true;
  bool user_touched = false;
  bool write = false;
  float scale_x = 1.0f;
  float scale_y = 1.0f;
  /// none | write_user | keep_archive | clamp_write
  std::string risk = "none";
  std::string reason = "ok";
};

struct FovArchiveHmdExpect {
  std::string verdict = "idle"; // idle | expect_keep_archive | expect_write_user
  bool expect_vision_preserved = true;
  std::string checklist = "G30 · IDLE · no FOV archive decision";
  std::string pass_line = "N/A";
  std::string fail_line = "N/A";
};

inline FovArchiveDecision CubeFov_Decide(bool userTouched, float scaleX, float scaleY) {
  FovArchiveDecision d;
  d.user_touched = userTouched;
  d.write = CubeFov_ShouldWrite(userTouched);
  const float cx = CubeFov_ClampScale(scaleX);
  const float cy = CubeFov_ClampScale(scaleY);
  d.scale_x = d.write ? cx : scaleX;
  d.scale_y = d.write ? cy : scaleY;
  if (!d.write) {
    d.risk = "keep_archive";
    d.reason = "omit_fovscale_preserve_vision";
  } else if (cx != scaleX || cy != scaleY) {
    d.risk = "clamp_write";
    d.reason = "user_touched_clamped";
  } else {
    d.risk = "write_user";
    d.reason = "user_touched_settings";
  }
  return d;
}

inline std::string CubeFov_StatusLabel(const FovArchiveDecision& d) {
  if (!d.valid) return "FOV · IDLE";
  if (!d.write) return "FOV · KEEP ARCHIVE";
  if (d.risk == "clamp_write") return "FOV · WRITE CLAMP";
  return "FOV · WRITE USER";
}

/// Comment/line for cfg when omitting (product uses this string path).
inline std::string CubeFov_OmitComment() {
  return "// fovscale x/y omitted — preserve archived / Vision calibration";
}

inline FovArchiveHmdExpect CubeFov_HmdExpect(const FovArchiveDecision& d) {
  FovArchiveHmdExpect e;
  if (!d.valid) return e;
  if (!d.write) {
    e.verdict = "expect_keep_archive";
    e.expect_vision_preserved = true;
    e.checklist = "G30 · KEEP · no fovscale lines in cold cfg";
    e.pass_line = "Vision/border FOV archive intact after Start without FOV edit";
    e.fail_line = "Cold Start rewrites fovscale 1.0/1.0 over asymmetric cal";
    return e;
  }
  e.verdict = "expect_write_user";
  e.expect_vision_preserved = false;
  e.checklist = "G30 · WRITE · x=" + std::to_string(d.scale_x).substr(0, 4) +
                " y=" + std::to_string(d.scale_y).substr(0, 4);
  e.pass_line = "User SETTINGS FOV applied once touched";
  e.fail_line = "Touched FOV ignored or silent archive clobber without touch";
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
