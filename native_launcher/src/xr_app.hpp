#pragma once
#include <string>

// OpenXR stereo host for Cube WebUI panel.
// Blocks until quit, close, or seamless handoff after Start Game.
// Modules: panel_config, world_panel, glx_context, gl_render, xr_input, ui_panel, gmod_spawn.
int RunCubeWebUILauncher(const std::string& gmodRoot, const std::string& xrJson);
