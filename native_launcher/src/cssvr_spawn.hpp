#pragma once
// Cube → CSSVRMod. Category name is the repo name (exact case).
#include <cstdlib>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

inline bool CubeTarget_IsCSSVRMod(const std::string& category) {
  return category == "CSSVRMod";
}

inline std::vector<std::string> CSSVRModDefaultMaps() {
  return {"de_dust2", "de_inferno", "de_nuke", "de_train", "de_aztec",
          "cs_office", "cs_italy", "cs_assault"};
}

inline std::string CSSVRLaunchCmd(const std::string& bin, const std::string& map, bool noborder) {
  std::ostringstream o;
  o << "env -u LD_LIBRARY_PATH -u STEAM_RUNTIME SDL_VIDEODRIVER=x11 ";
  o << "'" << bin << "'";
  if (noborder) o << " --noborder";
  if (!map.empty()) o << " --map " << map;
  o << " >/tmp/CSSVRMod_launch.log 2>&1 &";
  return o.str();
}

inline std::string FindCSSVRBin(const std::string& cubeExeDir = {}) {
  if (const char* e = std::getenv("CSSVR_BIN")) {
    if (e[0] && access(e, X_OK) == 0) return e;
  }
  std::vector<std::string> cands;
  if (const char* root = std::getenv("CSSVR_ROOT")) {
    cands.push_back(std::string(root) + "/install/CSSVR");
    cands.push_back(std::string(root) + "/scripts/CSSVR.sh");
  }
  if (!cubeExeDir.empty()) {
    cands.push_back(cubeExeDir + "/../../CSSVRMod/install/CSSVR");
    cands.push_back(cubeExeDir + "/../../../CSSVRMod/install/CSSVR");
    cands.push_back(cubeExeDir + "/../../CSSVRMod/scripts/CSSVR.sh");
  }
  if (const char* home = std::getenv("HOME")) {
    cands.push_back(std::string(home) + "/Dev/GMod/CSSVRMod/install/CSSVR");
    cands.push_back(std::string(home) + "/Dev/GMod/CSSVRMod/scripts/CSSVR.sh");
  }
  for (const auto& p : cands)
    if (!p.empty() && access(p.c_str(), X_OK) == 0) return p;
  return {};
}
