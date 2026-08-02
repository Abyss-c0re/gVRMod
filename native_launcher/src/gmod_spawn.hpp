#pragma once
#include <string>

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
  bool useSteam = true;
  std::string xrRuntimeJson; // XR_RUNTIME_JSON
  std::string cubeCfg = "gvrmod_cube";
};

// Write openxr_launch marker + gvrmod_cube.cfg, then spawn GMod (WebUI StartGame reverse).
// Returns 0 on spawn success. Does not take OpenXR — caller keeps session until handoff.
int SpawnGModFromWebUI(const LaunchRequest& req, std::string& errOut);

// True if a GMod/hl2 client process is running (for seamless handoff wait).
bool GModProcessRunning();

// Read Lua handoff phase from garrysmod/data/vrmod/cube_handoff.txt (phase=...).
// Returns empty if missing.
std::string ReadCubeHandoffPhase(const std::string& gmodRoot);

// Clear prior handoff markers before StartGame.
void ClearCubeHandoffMarkers(const std::string& gmodRoot);
