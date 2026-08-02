#pragma once
#include "maps_scan.hpp"
#include "addons_mgr.hpp"
#include <string>
#include <vector>

// Reversed WebUI shell: New Game (control.NewGame) + Addons (control.Addons)

enum class WebUIPage {
  NewGame = 0,
  Addons = 1,
};

struct WebUIState {
  WebUIPage page = WebUIPage::NewGame;
  std::string gmodRoot;

  // --- New Game ---
  std::vector<MapCategory> categories;
  int catIndex = 0;
  int mapScroll = 0;
  int mapIndex = 0;
  int maxPlayersIdx = 0;
  int maxPlayersOpts[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  std::string hostname = "gVRMod Cube";
  bool svLan = true;
  bool p2p = false;
  bool p2pFriends = false;
  std::string gamemode = "sandbox";
  int focusCol = 0; // 0=cat 1=map 2=settings 3=start
  int settingsRow = 0;

  // --- Addons (WebUI reverse) ---
  AddonManager addons;

  std::string status;
  bool wantStart = false;
  bool wantQuit = false;
};

void WebUI_Init(WebUIState& s, const std::string& gmodRoot);
const std::string& WebUI_SelectedMap(const WebUIState& s);
int WebUI_MaxPlayers(const WebUIState& s);

// stickX/Y -1/0/1, triggerEdge = select/toggle, backEdge = quit / back
// pageSwitch: when on top tab row
void WebUI_Input(WebUIState& s, int stickX, int stickY, bool triggerEdge, bool backEdge);

// Panel size
constexpr int UI_W = 960;
constexpr int UI_H = 540;

void WebUI_Rasterize(const WebUIState& s, unsigned char* rgba);
