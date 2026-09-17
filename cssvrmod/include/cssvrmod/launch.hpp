#pragma once
// Find Counter-Strike: Source and build an honest spawn plan (no Steam theater).
#include <string>
#include <vector>

namespace cssvr {

struct CssInstall {
  bool found = false;
  std::string root;
  std::string launcher; // cstrike.sh or cstrike_linux64
  std::string engine_so;
  std::string client_so;
  std::string togl_so;
  bool linux64 = false;
  const char* reason = "not_searched";
};

struct SpawnPlan {
  bool ok = false;
  std::string cwd;
  std::string exe;
  std::vector<std::string> argv;
  std::string ld_library_path;
  std::string ld_preload;
  std::string xr_runtime_json;
  const char* reason = "idle";
};

struct LaunchOpts {
  std::string hook_so; // libcssvrmod_hook.so
  std::string map;     // empty → menu
  int win_w = 1280;
  int win_h = 720;
  bool windowed = true;
  bool noborder = true;
  bool novid = true;
  bool sv_lan = true;
  std::string extra_args;
};

CssInstall FindCssInstall();
CssInstall InspectCssRoot(const std::string& root);
SpawnPlan PlanSpawn(const CssInstall& inst, const LaunchOpts& opts);
std::string DefaultHookSearchPath();
std::string DetectXrRuntimeJson();

/// Pure: never skip spawn unless hook path is empty (tests).
inline bool SpawnNeedsHook(const LaunchOpts& o) { return !o.hook_so.empty(); }

} // namespace cssvr
