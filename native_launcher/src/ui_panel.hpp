#pragma once
#include "maps_scan.hpp"
#include "addons_mgr.hpp"
#include "bindings_mgr.hpp"
#include <string>
#include <vector>

// Cube native menu: New Game + Addons + Settings + Bindings

enum class WebUIPage {
  NewGame = 0,
  Addons = 1,
  Settings = 2,
  Bindings = 3,
};

// OpenXR backend render (vrmod_* cvars applied via +exec on Start)
struct OpenXrRenderSettings {
  // Supersample index into {0.75,1.0,1.25,1.5,1.75,2.0} — needs VR restart
  int ssIdx = 3;            // 1.5 default
  float viewScale = 1.0f;   // vrmod_viewscale
  float fovScale = 1.0f;    // vrmod_fovscale_x/y (linked)
  float scaleFactor = 1.0f; // vrmod_scalefactor (submit UV crop)
  float eyeScale = 0.5f;    // vrmod_eyescale (IPD-ish submit)
  float zNear = 1.0f;       // vrmod_znear
  int desktopView = 3;      // 1=none 2=left 3=right (vrmod_desktopview)
  bool postProcess = false;
  bool swapEyes = false;
  bool skybox = false;
  bool mq2SinglePass = true;
  bool renderOffset = true;
  bool requireFocus = false;
};

// GMod / Source graphics + window (applied via cfg + launch args)
struct GModGfxSettings {
  // 0=Low 1=Med 2=High 3=Ultra (cycles; tweaking a field sets "Custom" display via preset=-1)
  int preset = 2; // High

  // Desktop mirror window (Source still needs a window for OpenXR path)
  int resIdx = 0; // indexes WinRes table
  int winW = 720;
  int winH = 480;
  bool noborder = true;
  bool windowed = true;

  // Source cvars
  int matPicmip = -1;       // -1 best … 2 worst
  int rRootLod = 0;         // 0 high … 2 low
  int matAntialias = 4;    // 0,2,4,8
  int matForceAniso = 8;    // 0,2,4,8,16
  int matHdrLevel = 2;      // 0 off, 1 bloom, 2 full
  bool shadows = true;
  bool flashlightShadows = true;
  bool specular = true;
  bool bumpmap = true;
  bool waterExpensive = true;
  bool multicore = true;
  int fpsMax = 0;           // 0 unlimited, else 60/90/120/144/240

  OpenXrRenderSettings xr;
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
  int focusCol = 0; // NewGame: 0=cat 1=map 2=server 3=start
  int settingsRow = 0;
  int settingsScroll = 0;

  // GMod native graphics / window (not VRMod-only)
  GModGfxSettings gfx;

  // --- Addons ---
  AddonManager addons;

  // --- Controller bindings (OpenXR remaps → vrmod_openxr_bindings.json) ---
  BindingsManager bindings;

  std::string status;
  bool wantStart = false;
  bool wantQuit = false;

  // Seamless StartGame handoff
  bool handoff = false;
  std::string handoffMap;
  std::string handoffPhase;
  std::string handoffDetail;
  float handoffElapsed = 0.f;

  // Laser cursor
  bool cursorVisible = false;
  int cursorX = 0, cursorY = 0;
};

void WebUI_Init(WebUIState& s, const std::string& gmodRoot);
const std::string& WebUI_SelectedMap(const WebUIState& s);
int WebUI_MaxPlayers(const WebUIState& s);

// Cycle a settings row (trigger / laser click). dir = +1 or -1.
void WebUI_CycleSetting(WebUIState& s, int row, int dir = 1);
// Apply quality preset 0..3 into individual gfx fields.
void WebUI_ApplyGfxPreset(WebUIState& s, int preset);
int WebUI_SettingsRowCount();

void WebUI_Input(WebUIState& s, int stickX, int stickY, bool triggerEdge, bool backEdge);
void WebUI_SetCursor(WebUIState& s, int px, int py, bool visible);
bool WebUI_PointerClick(WebUIState& s, int px, int py);
// Persist dirty bindings to garrysmod/data/vrmod/vrmod_openxr_bindings.json
bool WebUI_SaveBindingsIfDirty(WebUIState& s);

constexpr int UI_W = 960;
constexpr int UI_H = 540;

struct WebUICursor {
  bool visible = false;
  int x = 0, y = 0;
};
void WebUI_Rasterize(const WebUIState& s, unsigned char* rgba, const WebUICursor* cursor = nullptr);
