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
  // OpenXR backend (ladder indices → float)
  static const float kSs[] = {0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
  int ssi = ui.gfx.xr.ssIdx;
  if (ssi < 0) ssi = 0;
  if (ssi > 5) ssi = 5;
  lr.gfx.xrSupersample = kSs[ssi];
  lr.gfx.xrViewScale = ui.gfx.xr.viewScale;
  lr.gfx.xrFovScale = ui.gfx.xr.fovScale;
  lr.gfx.xrScaleFactor = ui.gfx.xr.scaleFactor;
  lr.gfx.xrEyeScale = ui.gfx.xr.eyeScale;
  lr.gfx.xrZNear = ui.gfx.xr.zNear;
  lr.gfx.xrDesktopView = ui.gfx.xr.desktopView;
  lr.gfx.xrPostProcess = ui.gfx.xr.postProcess;
  lr.gfx.xrSwapEyes = ui.gfx.xr.swapEyes;
  lr.gfx.xrSkybox = ui.gfx.xr.skybox;
  lr.gfx.xrMq2SinglePass = ui.gfx.xr.mq2SinglePass;
  lr.gfx.xrRenderOffset = ui.gfx.xr.renderOffset;
  lr.gfx.xrRequireFocus = ui.gfx.xr.requireFocus;
  if (const char* xr = getenv("XR_RUNTIME_JSON")) lr.xrRuntimeJson = xr;
  return lr;
}
