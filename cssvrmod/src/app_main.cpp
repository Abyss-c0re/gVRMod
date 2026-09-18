#include "cssvrmod/launch.hpp"
#include "cssvrmod/weapons.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

static void Usage() {
  std::fprintf(stdout,
               "CSSVRMod — Counter-Strike: Source VR (OpenXR)\n"
               "usage: CSSVR [--map de_dust2] [--hook PATH] [--find] [--print]\n"
               "  --find     locate CSS and print install (no spawn)\n"
               "  --print    print spawn plan and exit\n"
               "  --map MAP  +map after launch\n"
               "  --hook SO  LD_PRELOAD hook (default: sibling libcssvrmod_hook.so)\n"
               "  --vk       shaderapivk present + OpenXR submit (default)\n"
               "  --gl       OpenGL/togl (CreateDevice still broken on this GPU)\n"
               "  --dx9      original vrmod CreateTexture path (shaderapidx9)\n"
               "  --noborder borderless window (default is decorated)\n"
               "  --no-hook  spawn CSS without VR hook (debug)\n");
}

static std::string SiblingHook() {
  char buf[4096];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) return {};
  buf[n] = 0;
  std::string exe(buf);
  auto slash = exe.find_last_of('/');
  std::string dir = (slash == std::string::npos) ? "." : exe.substr(0, slash);
  return dir + "/libcssvrmod_hook.so";
}

static int Spawn(const cssvr::SpawnPlan& p) {
  std::vector<char*> argv;
  for (const auto& a : p.argv) argv.push_back(const_cast<char*>(a.c_str()));
  argv.push_back(nullptr);
  if (!p.ld_library_path.empty()) {
    const char* old = std::getenv("LD_LIBRARY_PATH");
    std::string lp = p.ld_library_path;
    if (old && old[0]) {
      lp += ":";
      lp += old;
    }
    setenv("LD_LIBRARY_PATH", lp.c_str(), 1);
  }
  if (!p.ld_preload.empty()) setenv("LD_PRELOAD", p.ld_preload.c_str(), 1);
  if (!p.xr_runtime_json.empty()) setenv("XR_RUNTIME_JSON", p.xr_runtime_json.c_str(), 1);
  if (!p.sdl_videodriver.empty()) setenv("SDL_VIDEODRIVER", p.sdl_videodriver.c_str(), 1);
  setenv("CSSVR_BACKEND", p.backend ? p.backend : "vk", 1);
  if (!std::getenv("CSSVR_XR")) setenv("CSSVR_XR", "1", 0);
  if (chdir(p.cwd.c_str()) != 0) {
    std::fprintf(stderr, "cssvr: chdir %s failed\n", p.cwd.c_str());
    return 2;
  }
  std::fprintf(stdout, "cssvr: exec %s (hook=%s)\n", p.exe.c_str(),
               p.ld_preload.empty() ? "none" : p.ld_preload.c_str());
  execv(p.exe.c_str(), argv.data());
  std::perror("cssvr: execv");
  return 3;
}

int main(int argc, char** argv) {
  bool find_only = false, print_only = false, no_hook = false;
  cssvr::LaunchOpts opts;
  opts.hook_so = SiblingHook();
  if (opts.hook_so.empty() || access(opts.hook_so.c_str(), R_OK) != 0)
    opts.hook_so = cssvr::DefaultHookSearchPath();

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      Usage();
      return 0;
    }
    if (std::strcmp(argv[i], "--find") == 0) find_only = true;
    else if (std::strcmp(argv[i], "--print") == 0) print_only = true;
    else if (std::strcmp(argv[i], "--no-hook") == 0) no_hook = true;
    else if (std::strcmp(argv[i], "--map") == 0 && i + 1 < argc) opts.map = argv[++i];
    else if (std::strcmp(argv[i], "--hook") == 0 && i + 1 < argc) opts.hook_so = argv[++i];
    else if (std::strcmp(argv[i], "--gl") == 0) opts.backend = cssvr::Backend::Gl;
    else if (std::strcmp(argv[i], "--dx9") == 0) opts.backend = cssvr::Backend::Dx9;
    else if (std::strcmp(argv[i], "--vk") == 0) opts.backend = cssvr::Backend::Vk;
    else if (std::strcmp(argv[i], "--noborder") == 0) opts.noborder = true;
    else {
      std::fprintf(stderr, "cssvr: unknown arg %s\n", argv[i]);
      Usage();
      return 1;
    }
  }
  if (no_hook) opts.hook_so.clear();

  cssvr::CssInstall inst = cssvr::FindCssInstall();
  std::fprintf(stdout, "cssvr: css found=%d root=%s reason=%s linux64=%d weapons=%d\n",
               inst.found ? 1 : 0, inst.root.c_str(), inst.reason, inst.linux64 ? 1 : 0,
               cssvr::WeaponCount());
  if (find_only) return inst.found ? 0 : 1;

  cssvr::SpawnPlan plan = cssvr::PlanSpawn(inst, opts);
  std::fprintf(stdout, "cssvr: spawn ok=%d backend=%s reason=%s exe=%s preload=%s sdl=%s xr=%s\n",
               plan.ok ? 1 : 0, plan.backend, plan.reason, plan.exe.c_str(),
               plan.ld_preload.c_str(), plan.sdl_videodriver.c_str(),
               plan.xr_runtime_json.c_str());
  if (print_only || !plan.ok) return plan.ok ? 0 : 1;
  setenv("CSSVR_NOBORDER", opts.noborder ? "1" : "0", 1);
  return Spawn(plan);
}
