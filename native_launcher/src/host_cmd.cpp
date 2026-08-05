#include "host_cmd.hpp"
#include <cstdio>
#include <cstring>
#include <unistd.h>

bool HostCmdPoll(CubeUIState& ui, bool* outResetPanel) {
  if (outResetPanel) *outResetPanel = false;
  FILE* f = fopen("/tmp/CubeUI_cmd", "r");
  if (!f) f = fopen("/tmp/cube_webui_cmd", "r"); // legacy
  if (!f) return false;
  char buf[64] = {};
  if (fgets(buf, sizeof(buf), f)) {
    if (std::strncmp(buf, "start", 5) == 0) ui.wantStart = true;
    if (std::strncmp(buf, "quit", 4) == 0) ui.wantQuit = true;
    if (std::strncmp(buf, "addons", 6) == 0) ui.page = CubeUIPage::Addons;
    if (std::strncmp(buf, "newgame", 7) == 0) ui.page = CubeUIPage::NewGame;
    if (std::strncmp(buf, "settings", 8) == 0) ui.page = CubeUIPage::Settings;
    if (std::strncmp(buf, "bindings", 8) == 0) ui.page = CubeUIPage::Bindings;
    if (std::strncmp(buf, "reset", 5) == 0) {
      if (outResetPanel) *outResetPanel = true;
      ui.status = "PANEL RESET";
    }
    if (std::strncmp(buf, "close", 5) == 0 || std::strncmp(buf, "exit", 4) == 0)
      ui.wantQuit = true;
    if (std::strncmp(buf, "up", 2) == 0) CubeUI_Input(ui, 0, -1, false, false);
    if (std::strncmp(buf, "down", 4) == 0) CubeUI_Input(ui, 0, 1, false, false);
    if (std::strncmp(buf, "left", 4) == 0) CubeUI_Input(ui, -1, 0, false, false);
    if (std::strncmp(buf, "right", 5) == 0) CubeUI_Input(ui, 1, 0, false, false);
    if (std::strncmp(buf, "click", 5) == 0 || std::strncmp(buf, "toggle", 6) == 0) {
      if (ui.cursorVisible)
        CubeUI_PointerClick(ui, ui.cursorX, ui.cursorY);
      else
        CubeUI_Input(ui, 0, 0, true, false);
    }
  }
  fclose(f);
  unlink("/tmp/CubeUI_cmd");
  return true;
}
