#pragma once
#include <string>

// Tunable panel / XR shell settings (cube_webui.conf + env).
struct PanelConfig {
  float dist = 1.05f;
  float halfW = 0.42f;
  float halfH = 0.24f;
  float offsetX = 0.f;
  float offsetY = 0.f;
  float offsetZ = 0.f;
  bool viewLock = false; // false = world-locked (product default); conf view_lock=1 honors HUD
  bool passthrough = true;
  float grabThresh = 0.90f;
  float panelAlpha = 1.0f;
  // Grip-to-move OFF by default — Meta Cam/WiVRn: resting squeeze thrash stole all clicks.
  // MENU re-anchors; set grab_enable=1 to restore grip-move.
  bool grabEnable = false;
  float triggerThresh = 0.40f; // slightly soft so Quest float trigger registers
};

// Global shell config (loaded once at startup).
PanelConfig& PanelCfg();
const PanelConfig& PanelCfgConst();

// Load project → user → gmod → env. Resets world panel ready state caller-side.
void LoadPanelConfig(const std::string& gmodRoot);

std::string PathDirname(const std::string& p);
std::string PathExeDir();
