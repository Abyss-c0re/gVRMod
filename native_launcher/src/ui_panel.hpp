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
  float fovScaleX = 1.0f;   // vrmod_fovscale_x (Vision cal may differ from Y)
  float fovScaleY = 1.0f;   // vrmod_fovscale_y
  bool fovTouched = false;  // only rewrite FOV on Start when user edited SETTINGS
  float scaleFactor = 1.0f; // vrmod_scalefactor (submit UV crop)
  float eyeScale = 0.5f;    // vrmod_eyescale (IPD-ish submit)
  float zNear = 1.0f;       // vrmod_znear
  int desktopView = 1;      // 1=none (Cube seamless) 2=left 3=right
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
  bool noborder = false; // default: framed GMod window (not borderless)
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

  // G11 Quick Play — last map + gfx snapshot loaded from cube_last_play.txt
  bool hasLastPlay = false;
  std::string lastPlayMap; // display label when hasLastPlay

  // Seamless StartGame handoff
  bool handoff = false;
  std::string handoffMap;
  std::string handoffPhase;
  std::string handoffDetail;
  float handoffElapsed = 0.f;

  // Laser cursor (software reticle — optional; laser usually enough)
  bool cursorVisible = false;
  int cursorX = 0, cursorY = 0;

  // Paint law (research-2 / VRMod MenuShouldRepaint):
  // full CPU raster + GL upload only when dirty or idle heartbeat — never every stereo eye.
  bool paintDirty = true;
  int paintFrame = 0;          // frames since last full paint
  int lastCursorQx = -9999;    // quantized cursor (÷4) for dirty gating
  int lastCursorQy = -9999;
  bool lastCursorVis = false;
  // When true, draw software cursor into buffer (else laser-only, skip soft cursor)
  bool paintSoftCursor = false;
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

// G11: persist / restore last map + gfx for Quick Play
bool WebUI_SaveLastPlay(const WebUIState& s);
bool WebUI_LoadLastPlay(WebUIState& s);
// Apply snapshot into UI (map selection + server + gfx). Returns false if map missing.
bool WebUI_ApplyLastPlayMap(WebUIState& s);

// Mark content dirty (input, meta apply, page change, handoff anim).
void WebUI_MarkDirty(WebUIState& s);
// True if full CPU raster + tex upload should run this frame.
bool WebUI_ShouldRepaint(WebUIState& s);
// After a successful repaint/upload, clear dirty + reset frame counter.
void WebUI_DidRepaint(WebUIState& s);

constexpr int UI_W = 960;
constexpr int UI_H = 540;

struct WebUICursor {
  bool visible = false;
  int x = 0, y = 0;
};
// Full panel paint (content). Soft cursor only if s.paintSoftCursor && cursor.
void WebUI_Rasterize(const WebUIState& s, unsigned char* rgba, const WebUICursor* cursor = nullptr);
