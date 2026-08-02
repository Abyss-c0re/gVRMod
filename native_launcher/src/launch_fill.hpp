#pragma once
#include "gmod_spawn.hpp"
#include "ui_panel.hpp"
#include <cstdlib>

// Map WebUI state → LaunchRequest (graphics + map + server).
inline LaunchRequest LaunchRequestFromUI(const WebUIState& ui, const std::string& gmodRoot) {
  LaunchRequest lr;
  lr.gmodRoot = gmodRoot;
  lr.map = WebUI_SelectedMap(ui);
  lr.maxPlayers = WebUI_MaxPlayers(ui);
  lr.hostname = ui.hostname;
  lr.svLan = ui.svLan;
  lr.p2p = ui.p2p;
  lr.p2pFriends = ui.p2pFriends;
  lr.gamemode = ui.gamemode;
  lr.winW = ui.gfx.winW;
  lr.winH = ui.gfx.winH;
  lr.windowed = ui.gfx.windowed;
  lr.noborder = ui.gfx.noborder;
  lr.gfx.matPicmip = ui.gfx.matPicmip;
  lr.gfx.rRootLod = ui.gfx.rRootLod;
  lr.gfx.matAntialias = ui.gfx.matAntialias;
  lr.gfx.matForceAniso = ui.gfx.matForceAniso;
  lr.gfx.matHdrLevel = ui.gfx.matHdrLevel;
  lr.gfx.shadows = ui.gfx.shadows;
  lr.gfx.flashlightShadows = ui.gfx.flashlightShadows;
  lr.gfx.specular = ui.gfx.specular;
  lr.gfx.bumpmap = ui.gfx.bumpmap;
  lr.gfx.waterExpensive = ui.gfx.waterExpensive;
  lr.gfx.multicore = ui.gfx.multicore;
  lr.gfx.fpsMax = ui.gfx.fpsMax;
  if (const char* xr = getenv("XR_RUNTIME_JSON")) lr.xrRuntimeJson = xr;
  return lr;
}
