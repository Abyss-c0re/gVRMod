#pragma once
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
