#pragma once
#include "ui_panel.hpp"
#include <string>

// OpenXR + GLX stereo app hosting reversed WebUI panel.
// Returns exit code. Blocks until quit or StartGame spawn.
int RunCubeWebUILauncher(const std::string& gmodRoot, const std::string& xrJson);
