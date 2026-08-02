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
// Returns 0 on spawn success.
int SpawnGModFromWebUI(const LaunchRequest& req, std::string& errOut);
