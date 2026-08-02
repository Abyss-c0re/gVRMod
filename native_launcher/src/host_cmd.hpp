#pragma once
#include "ui_panel.hpp"

// Poll /tmp/cube_webui_cmd for desktop debug control.
// Returns true if panel should re-seed (reset).
bool HostCmdPoll(WebUIState& ui, bool* outResetPanel);
