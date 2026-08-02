// cube_webui_launcher — Native OpenXR host for reversed GMod WebUI New Game.
// Does NOT launch GMod until the user selects Start Game.
#include "xr_app.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

static std::string FindGMod() {
  if (const char* e = getenv("GMOD_DIR")) return e;
  const char* home = getenv("HOME");
  if (!home) return {};
  const char* cands[] = {
    "/.steam/steam/steamapps/common/GarrysMod",
    "/.local/share/Steam/steamapps/common/GarrysMod",
    "/.steam/root/steamapps/common/GarrysMod",
  };
  for (auto rel : cands) {
    std::string p = std::string(home) + rel;
    if (access((p + "/hl2.sh").c_str(), X_OK) == 0) return p;
  }
  return {};
}

static std::string FindXrJson() {
  if (const char* e = getenv("XR_RUNTIME_JSON")) return e;
  const char* cands[] = {
    "/usr/share/openxr/1/openxr_wivrn.json",
    "/usr/local/share/openxr/1/openxr_wivrn.json",
    "/usr/share/openxr/1/openxr_monado.json",
  };
  for (auto c : cands)
    if (access(c, R_OK) == 0) return c;
  return {};
}

int main(int argc, char** argv) {
  bool desktopOnly = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0) {
      std::printf(
          "cube_webui_launcher — reversed GMod WebUI New Game in OpenXR\n"
          "  Put on headset. Browse maps. START GAME spawns GMod.\n"
          "  Env: GMOD_DIR  XR_RUNTIME_JSON\n"
          "  Cmds: echo start|addons|newgame|up|down|left|right|click|toggle|quit\n"
          "        > /tmp/cube_webui_cmd\n");
      return 0;
    }
    if (std::strcmp(argv[i], "--desktop-preview") == 0) desktopOnly = true;
  }

  std::string gmod = FindGMod();
  if (gmod.empty()) {
    std::fprintf(stderr, "[cube_webui] GMod not found — set GMOD_DIR\n");
    return 1;
  }
  std::string xr = FindXrJson();
  if (xr.empty()) {
    std::fprintf(stderr, "[cube_webui] No OpenXR runtime JSON (WiVRn/Monado)\n");
    return 1;
  }

  // Start WiVRn if needed
  if (system("pgrep -x wivrn-server >/dev/null 2>&1") != 0) {
    if (system("command -v wivrn-server >/dev/null 2>&1") == 0) {
      std::fprintf(stderr, "[cube_webui] starting wivrn-server…\n");
      system("wivrn-server >/tmp/cube_webui_wivrn.log 2>&1 &");
      sleep(1);
    }
  }

  // Pin active runtime
  {
    std::string cmd = "mkdir -p \"$HOME/.config/openxr/1\" && ln -sfn '" + xr +
                      "' \"$HOME/.config/openxr/1/active_runtime.json\"";
    system(cmd.c_str());
  }

  if (desktopOnly) {
    std::fprintf(stderr, "[cube_webui] --desktop-preview not implemented; use OpenXR path\n");
  }

  return RunCubeWebUILauncher(gmod, xr);
}
