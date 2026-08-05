#pragma once
#include "ui_panel.hpp"

// Poll /tmp/CubeUI_cmd for desktop debug control.
// Returns true if panel should re-seed (reset).
bool HostCmdPoll(CubeUIState& ui, bool* outResetPanel);
