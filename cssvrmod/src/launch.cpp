#include "cssvrmod/launch.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace cssvr {

static bool IsFile(const std::string& p) {
  struct stat st {};
  return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}
static bool IsDir(const std::string& p) {
  struct stat st {};
  return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static std::string Home() {
  const char* h = std::getenv("HOME");
  return h ? std::string(h) : std::string();
}

CssInstall InspectCssRoot(const std::string& root) {
  CssInstall i;
  i.root = root;
  if (root.empty() || !IsDir(root)) {
    i.reason = "root_missing";
    return i;
  }
  const std::string sh = root + "/cstrike.sh";
  const std::string bin64 = root + "/cstrike_linux64";
  const std::string eng64 = root + "/bin/linux64/engine.so";
  const std::string eng32 = root + "/bin/engine.so";
  const std::string cli64 = root + "/cstrike/bin/linux64/client.so";
  const std::string cli32 = root + "/cstrike/bin/client.so";
  const std::string togl64 = root + "/bin/linux64/libtogl.so";
  const std::string togl32 = root + "/bin/libtogl.so";

  if (IsFile(eng64) && IsFile(cli64)) {
    i.linux64 = true;
    i.engine_so = eng64;
    i.client_so = cli64;
    i.togl_so = IsFile(togl64) ? togl64 : "";
    i.launcher = IsFile(sh) ? sh : bin64;
    i.found = IsFile(i.launcher);
    i.reason = i.found ? "linux64" : "no_launcher";
    return i;
  }
  if (IsFile(eng32) && IsFile(cli32)) {
    i.linux64 = false;
    i.engine_so = eng32;
    i.client_so = cli32;
    i.togl_so = IsFile(togl32) ? togl32 : "";
    i.launcher = IsFile(sh) ? sh : (root + "/hl2_linux");
    i.found = IsFile(i.launcher);
    i.reason = i.found ? "linux32" : "no_launcher";
    return i;
  }
  i.reason = "no_engine";
  return i;
}

CssInstall FindCssInstall() {
  std::vector<std::string> candidates;
  if (const char* e = std::getenv("CSSVR_CSS_ROOT")) candidates.push_back(e);
  if (const char* e = std::getenv("CSS_ROOT")) candidates.push_back(e);
  const std::string home = Home();
  candidates.push_back(home + "/.steam/steam/steamapps/common/Counter-Strike Source");
  candidates.push_back(home + "/.local/share/Steam/steamapps/common/Counter-Strike Source");
  candidates.push_back(home + "/.steam/debian-installation/steamapps/common/Counter-Strike Source");
  // Extra Steam libraries (libraryfolders.vdf) — best-effort first path only.
  const std::string vdf = home + "/.steam/steam/steamapps/libraryfolders.vdf";
  if (IsFile(vdf)) {
    std::ifstream in(vdf);
    std::string line;
    while (std::getline(in, line)) {
      auto q1 = line.find('"');
      if (q1 == std::string::npos) continue;
      // path lines look like: "path"		"/mnt/games"
      if (line.find("path") == std::string::npos) continue;
      auto last = line.rfind('"');
      auto prev = line.rfind('"', last - 1);
      if (prev == std::string::npos || last == std::string::npos || last <= prev) continue;
      std::string p = line.substr(prev + 1, last - prev - 1);
      if (!p.empty() && p[0] == '/')
        candidates.push_back(p + "/steamapps/common/Counter-Strike Source");
    }
  }

  CssInstall best;
  best.reason = "not_found";
  for (const auto& c : candidates) {
    if (c.empty()) continue;
    CssInstall got = InspectCssRoot(c);
    if (got.found) return got;
    if (!best.found && got.reason && std::string(got.reason) != "root_missing") best = got;
  }
  return best;
}

std::string DetectXrRuntimeJson() {
  if (const char* e = std::getenv("XR_RUNTIME_JSON")) return e;
  static const char* k[] = {
      "/usr/share/openxr/1/openxr_wivrn.json",
      "/usr/local/share/openxr/1/openxr_wivrn.json",
      "/usr/share/openxr/1/openxr_monado.json",
      "/usr/share/openxr/1/openxr_steamvr.json",
      nullptr,
  };
  for (int i = 0; k[i]; ++i)
    if (IsFile(k[i])) return k[i];
  return {};
}

std::string DefaultHookSearchPath() {
  if (const char* e = std::getenv("CSSVR_HOOK")) return e;
  const char* self = std::getenv("CSSVR_ROOT");
  if (self) {
    std::string p = std::string(self) + "/install/cssvrmod/libcssvrmod_hook.so";
    if (IsFile(p)) return p;
    p = std::string(self) + "/cssvrmod/build/libcssvrmod_hook.so";
    if (IsFile(p)) return p;
  }
  return {};
}

SpawnPlan PlanSpawn(const CssInstall& inst, const LaunchOpts& opts) {
  SpawnPlan p;
  if (!inst.found) {
    p.reason = "css_not_found";
    return p;
  }
  p.cwd = inst.root;
  p.exe = inst.launcher;
  p.argv.push_back(inst.launcher);
  const BackendLaunch be = BackendPlan(opts.backend);
  p.backend = BackendName(opts.backend);
  if (be.engine_flag && be.engine_flag[0]) p.argv.push_back(be.engine_flag);
  // X11 so the Motif/SDL decoration hook can actually show a title bar.
  p.sdl_videodriver = be.sdl_video ? be.sdl_video : "x11";
  if (opts.novid) p.argv.push_back("-novid");
  if (opts.windowed) p.argv.push_back("-windowed");
  if (opts.noborder) p.argv.push_back("-noborder");
  p.argv.push_back("-w");
  p.argv.push_back(std::to_string(opts.win_w));
  p.argv.push_back("-h");
  p.argv.push_back(std::to_string(opts.win_h));
  // Command-line videomode wins over last-run fullscreen/noborder saved in cfg.
  p.argv.push_back("+mat_setvideomode");
  p.argv.push_back(std::to_string(opts.win_w));
  p.argv.push_back(std::to_string(opts.win_h));
  p.argv.push_back(opts.windowed ? "1" : "0");
  if (opts.sv_lan) {
    p.argv.push_back("+sv_lan");
    p.argv.push_back("1");
  }
  if (!opts.map.empty()) {
    p.argv.push_back("+map");
    p.argv.push_back(opts.map);
  }
  if (!opts.extra_args.empty()) {
    std::istringstream ss(opts.extra_args);
    std::string tok;
    while (ss >> tok) p.argv.push_back(tok);
  }

  const std::string plat = inst.linux64 ? "linux64" : "";
  p.ld_library_path = inst.root + "/bin";
  if (inst.linux64) p.ld_library_path = inst.root + "/bin/linux64:" + p.ld_library_path;
  if (const char* extra = std::getenv("CSSVR_LIBDIR")) {
    if (extra[0]) p.ld_library_path = std::string(extra) + ":" + p.ld_library_path;
  }

  p.ld_preload = opts.hook_so;
  if (const char* old = std::getenv("LD_PRELOAD")) {
    if (old[0]) {
      if (!p.ld_preload.empty()) p.ld_preload += ":";
      p.ld_preload += old;
    }
  }
  p.xr_runtime_json = DetectXrRuntimeJson();
  if (SpawnNeedsHook(opts) && (opts.hook_so.empty() || !IsFile(opts.hook_so))) {
    p.reason = "hook_missing";
    p.ok = false;
    return p;
  }
  p.ok = true;
  p.reason = "ready";
  return p;
}

} // namespace cssvr
