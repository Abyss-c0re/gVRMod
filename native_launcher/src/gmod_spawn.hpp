#pragma once
#include "ambient_clip.hpp"
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

// G12: write cube_ambient.txt (gain/playing/clip_rel). No audio engine — status SoT only.
bool WriteCubeAmbientStatus(const std::string& gmodRoot, const AmbientClipSnapshot& snap);

// G04: write cube_warm.txt when process already up (intent only; still cold-spawns).
bool WriteCubeWarmRequest(const std::string& gmodRoot, const WarmRequestSnapshot& snap);

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

// G13: reverse handoff (GMod exit VR → Cube reclaim) — pure labels only.
// Product today: GMod writes cube_return.txt; Cube does not auto-reclaim yet.
inline std::string CubeReversePhaseLabel(const std::string& phase) {
  if (phase == "vr_exit") return "VR EXIT";
  if (phase == "xr_released") return "XR RELEASED";
  if (phase == "cube_claim") return "CUBE CLAIM";
  if (phase == "panel_live") return "PANEL LIVE";
  if (phase.empty()) return "RETURN IDLE";
  return phase;
}

inline std::string CubeReverseDetailForPhase(const std::string& phase) {
  if (phase == "vr_exit") return "GMod left VR · Cube reclaim not auto yet";
  if (phase == "xr_released") return "OpenXR free · relaunch Cube shell to reclaim";
  if (phase == "cube_claim") return "Cube claiming OpenXR…";
  if (phase == "panel_live") return "Cube panel live · reverse handoff complete";
  return "return-to-Cube protocol (future)";
}

inline float CubeReverseProgressForPhase(const std::string& phase) {
  if (phase == "vr_exit") return 0.25f;
  if (phase == "xr_released") return 0.5f;
  if (phase == "cube_claim") return 0.75f;
  if (phase == "panel_live") return 1.f;
  return -1.f;
}

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
  if (phase == "waiting_process")
    return "cold Steam/hl2 boot · panel holds OpenXR…";
  return gmodUp ? "GMod live · waiting take_xr (seamless)"
                : "cold Steam/hl2 boot · panel holds OpenXR…";
}

// Progress 0..1 for known phases; negative → caller uses time-based fallback.
inline float CubeHandoffProgressForPhase(const std::string& phase) {
  if (phase == "waiting_process") return 0.08f;
  if (phase == "spawned" || phase == "SPAWNED") return 0.12f;
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
