// cube_webui_launcher — entry: resolve GMod/XR, then OpenXR WebUI host.
#include "xr_app.hpp"
#include "paths.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0) {
      std::printf(
          "cube_webui_launcher — Cube WebUI in OpenXR (seamless product)\n"
          "  TRIGGER click · MENU re-place panel · CLOSE exit · START → GMod handoff\n"
          "  Same Cube crimson theme as Lua vrmod-x64 (cl_cube_theme).\n"
          "  grab_enable=0 by default (no grip thrash). Env: GMOD_DIR XR_RUNTIME_JSON\n"
          "  Cmds: echo start|close|addons|settings|reset|click >/tmp/cube_webui_cmd\n");
      return 0;
    }
  }

  std::string gmod = FindGModRoot();
  if (gmod.empty()) {
    std::fprintf(stderr, "[cube_webui] GMod not found — set GMOD_DIR\n");
    return 1;
  }
  std::string xr = FindXrRuntimeJson();
  if (xr.empty()) {
    std::fprintf(stderr, "[cube_webui] No OpenXR runtime JSON (WiVRn/Monado)\n");
    return 1;
  }

  if (system("pgrep -x wivrn-server >/dev/null 2>&1") != 0) {
    if (system("command -v wivrn-server >/dev/null 2>&1") == 0) {
      std::fprintf(stderr, "[cube_webui] starting wivrn-server…\n");
      system("wivrn-server >/tmp/cube_webui_wivrn.log 2>&1 &");
      sleep(1);
    }
  }

  {
    std::string cmd = "mkdir -p \"$HOME/.config/openxr/1\" && ln -sfn '" + xr +
                      "' \"$HOME/.config/openxr/1/active_runtime.json\"";
    system(cmd.c_str());
  }

  return RunCubeWebUILauncher(gmod, xr);
}
