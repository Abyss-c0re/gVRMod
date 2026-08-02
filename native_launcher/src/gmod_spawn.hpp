#pragma once
#include <string>

// GMod/Source graphics applied via +exec cfg (not VRMod-only)
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
  bool noborder = true;
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
