#include "ui_panel.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

static uint8_t Glyph(char c, int row) {
  static const uint8_t A[] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
  static const uint8_t B[] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E};
  static const uint8_t C[] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E};
  static const uint8_t D[] = {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E};
  static const uint8_t E[] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
  static const uint8_t F[] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10};
  static const uint8_t G[] = {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E};
  static const uint8_t H[] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11};
  static const uint8_t I[] = {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E};
  static const uint8_t J[] = {0x01,0x01,0x01,0x01,0x11,0x11,0x0E};
  static const uint8_t K[] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11};
  static const uint8_t L[] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F};
  static const uint8_t M[] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11};
  static const uint8_t N[] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11};
  static const uint8_t O[] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
  static const uint8_t P[] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};
  static const uint8_t Q[] = {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D};
  static const uint8_t R[] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
  static const uint8_t S[] = {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E};
  static const uint8_t T[] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
  static const uint8_t U[] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E};
  static const uint8_t V[] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04};
  static const uint8_t W[] = {0x11,0x11,0x11,0x15,0x15,0x1B,0x11};
  static const uint8_t X[] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11};
  static const uint8_t Y[] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04};
  static const uint8_t Z[] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F};
  static const uint8_t N0[] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E};
  static const uint8_t N1[] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E};
  static const uint8_t N2[] = {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F};
  static const uint8_t N3[] = {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E};
  static const uint8_t N4[] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02};
  static const uint8_t N5[] = {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E};
  static const uint8_t N6[] = {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E};
  static const uint8_t N7[] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08};
  static const uint8_t N8[] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E};
  static const uint8_t N9[] = {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C};
  static const uint8_t US[] = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00};
  static const uint8_t DT[] = {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C};
  static const uint8_t SP[] = {0,0,0,0,0,0,0};
  const uint8_t* g = SP;
  if (c >= 'a' && c <= 'z') c = (char)(c - 32);
  if (c >= 'A' && c <= 'Z') {
    const uint8_t* tab[] = {A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z};
    g = tab[c - 'A'];
  } else if (c >= '0' && c <= '9') {
    const uint8_t* tab[] = {N0,N1,N2,N3,N4,N5,N6,N7,N8,N9};
    g = tab[c - '0'];
  } else if (c == '_' || c == '-') g = US;
  else if (c == '.') g = DT;
  return g[row];
}

static void PutPx(unsigned char* rgba, int x, int y, int r, int g, int b, int a = 255) {
  if (x < 0 || y < 0 || x >= UI_W || y >= UI_H) return;
  int i = (y * UI_W + x) * 4;
  rgba[i] = (unsigned char)r;
  rgba[i + 1] = (unsigned char)g;
  rgba[i + 2] = (unsigned char)b;
  rgba[i + 3] = (unsigned char)a;
}

static void FillRect(unsigned char* rgba, int x, int y, int w, int h, int r, int g, int b, int a = 255) {
  for (int j = 0; j < h; ++j)
    for (int i = 0; i < w; ++i)
      PutPx(rgba, x + i, y + j, r, g, b, a);
}

static void DrawText(unsigned char* rgba, int x, int y, const char* s, int r, int g, int b, int scale = 2) {
  for (const char* p = s; *p; ++p) {
    for (int row = 0; row < 7; ++row) {
      uint8_t bits = Glyph(*p, row);
      for (int col = 0; col < 5; ++col) {
        if (bits & (0x10 >> col)) {
          for (int sy = 0; sy < scale; ++sy)
            for (int sx = 0; sx < scale; ++sx)
              PutPx(rgba, x + col * scale + sx, y + row * scale + sy, r, g, b);
        }
      }
    }
    x += 6 * scale;
  }
}

void WebUI_Init(WebUIState& s, const std::string& gmodRoot) {
  s = WebUIState{};
  s.gmodRoot = gmodRoot;
  s.page = WebUIPage::NewGame;
  s.categories = ScanGModMaps(gmodRoot);
  if (s.categories.empty()) {
    MapCategory c;
    c.name = "Sandbox";
    c.maps = {"gm_construct", "gm_flatgrass"};
    c.order = 0;
    s.categories.push_back(c);
  }
  for (size_t i = 0; i < s.categories.size(); ++i) {
    if (s.categories[i].name == "Sandbox") {
      s.catIndex = (int)i;
      break;
    }
  }
  auto& maps = s.categories[s.catIndex].maps;
  for (size_t i = 0; i < maps.size(); ++i) {
    if (maps[i] == "gm_construct") {
      s.mapIndex = (int)i;
      break;
    }
  }
  Addons_Load(s.addons, gmodRoot);
  s.status = "WEBUI REVERSE: NEW GAME + ADDONS — START SPAWNS GMOD";
}

const std::string& WebUI_SelectedMap(const WebUIState& s) {
  static std::string fallback = "gm_construct";
  if (s.categories.empty()) return fallback;
  const auto& maps = s.categories[s.catIndex].maps;
  if (maps.empty()) return fallback;
  int i = std::clamp(s.mapIndex, 0, (int)maps.size() - 1);
  return maps[i];
}

int WebUI_MaxPlayers(const WebUIState& s) {
  return s.maxPlayersOpts[std::clamp(s.maxPlayersIdx, 0, 7)];
}

void WebUI_SetCursor(WebUIState& s, int px, int py, bool visible) {
  s.cursorX = px;
  s.cursorY = py;
  s.cursorVisible = visible;
  if (visible) {
    char buf[64];
    snprintf(buf, sizeof(buf), "LASER %d,%d", px, py);
    // keep short; status set by click handlers primarily
    (void)s;
  }
}

bool WebUI_PointerClick(WebUIState& s, int px, int py) {
  // Nav tabs
  if (py >= 6 && py <= 38) {
    if (px >= 8 && px <= 168) {
      s.page = WebUIPage::NewGame;
      s.status = "NEW GAME";
      return true;
    }
    if (px >= 180 && px <= 320) {
      s.page = WebUIPage::Addons;
      s.status = "ADDON MANAGER — AIM + TRIGGER";
      return true;
    }
  }

  if (s.page == WebUIPage::Addons) {
    // list rows
    const int visible = 14;
    int start = s.addons.scroll;
    for (int n = 0; n < visible; ++n) {
      int i = start + n;
      if (i >= (int)s.addons.addons.size()) break;
      int y = 92 + n * 28;
      if (px >= 244 && px <= UI_W - 16 && py >= y - 2 && py <= y + 22) {
        s.addons.selected = i;
        std::string err;
        if (!Addons_ToggleSelected(s.addons, err))
          s.status = "TOGGLE FAIL: " + err;
        else
          s.status = s.addons.status;
        return true;
      }
    }
    return false;
  }

  // New Game hit regions
  const int catW = 180, mapX = 188, mapW = 500, setX = 700, setW = 250;

  // Categories
  for (int i = 0; i < (int)s.categories.size() && i < 12; ++i) {
    int y = 80 + i * 28;
    if (px >= 12 && px <= 8 + catW && py >= y - 2 && py <= y + 22) {
      s.catIndex = i;
      s.mapIndex = 0;
      s.focusCol = 0;
      s.status = "CAT " + s.categories[i].name;
      return true;
    }
  }

  // Maps
  if (!s.categories.empty()) {
    const auto& maps = s.categories[s.catIndex].maps;
    int visible = 12;
    int start = std::max(0, s.mapIndex - visible / 2);
    for (int n = 0; n < visible; ++n) {
      int i = start + n;
      if (i >= (int)maps.size()) break;
      int y = 80 + n * 28;
      if (px >= mapX + 4 && px <= mapX + mapW - 4 && py >= y - 2 && py <= y + 22) {
        s.mapIndex = i;
        s.focusCol = 1;
        s.status = "MAP " + maps[i];
        return true;
      }
    }
  }

  // Settings rows
  for (int idx = 0; idx < 4; ++idx) {
    int y = 84 + idx * 36;
    if (px >= setX + 4 && px <= setX + setW - 4 && py >= y - 4 && py <= y + 26) {
      s.focusCol = 2;
      s.settingsRow = idx;
      if (idx == 0) s.maxPlayersIdx = (s.maxPlayersIdx + 1) % 8;
      else if (idx == 1) s.svLan = !s.svLan;
      else if (idx == 2) s.p2p = !s.p2p;
      else if (idx == 3) s.p2pFriends = !s.p2pFriends;
      s.status = "SETTING TOGGLED";
      return true;
    }
  }

  // START GAME button
  int by = UI_H - 70;
  if (px >= setX + 12 && px <= setX + setW - 12 && py >= by && py <= by + 44) {
    s.focusCol = 3;
    s.wantStart = true;
    s.status = "START GAME";
    return true;
  }
  return false;
}

void WebUI_Input(WebUIState& s, int stickX, int stickY, bool triggerEdge, bool backEdge) {
  if (backEdge) {
    s.wantQuit = true;
    return;
  }

  // Top-level page tabs: stick left/right when focusCol special? Use stickX at focus -1
  // Simpler: stickX cycles pages when on NewGame focusCol wraps, or dedicated
  // Page switch: stickX while holding "tab" — use stickX when focusCol is at edge with wrap to page
  // Better: explicit focusCol -1 for tabs via stickY up from col0
  // Commands: page_newgame / page_addons via file; also stickX from tab row

  // Tab row navigation: if stickY up from top of list, enter tab mode (focusCol = -1)
  // For simplicity: stickX with focusCol==0 and stickY==0 double... 
  // Use: when stickX and focusCol==3 and stickX>0 -> Addons page
  //      when on Addons stickX left at edge -> NewGame

  if (s.page == WebUIPage::NewGame) {
    if (s.categories.empty()) return;

    if (stickX < 0) s.focusCol = std::max(0, s.focusCol - 1);
    if (stickX > 0) {
      if (s.focusCol >= 3) {
        s.page = WebUIPage::Addons;
        s.status = "ADDON MANAGER (WEBUI SUBSCRIBED REVERSE)";
        return;
      }
      s.focusCol = std::min(3, s.focusCol + 1);
    }

    auto& maps = s.categories[s.catIndex].maps;
    if (s.focusCol == 0) {
      if (stickY < 0) s.catIndex = std::max(0, s.catIndex - 1);
      if (stickY > 0) s.catIndex = std::min((int)s.categories.size() - 1, s.catIndex + 1);
      s.mapIndex = 0;
    } else if (s.focusCol == 1) {
      if (stickY < 0) s.mapIndex = std::max(0, s.mapIndex - 1);
      if (stickY > 0 && !maps.empty())
        s.mapIndex = std::min((int)maps.size() - 1, s.mapIndex + 1);
    } else if (s.focusCol == 2) {
      if (stickY < 0) s.settingsRow = std::max(0, s.settingsRow - 1);
      if (stickY > 0) s.settingsRow = std::min(3, s.settingsRow + 1);
      if (triggerEdge) {
        if (s.settingsRow == 0) s.maxPlayersIdx = (s.maxPlayersIdx + 1) % 8;
        else if (s.settingsRow == 1) s.svLan = !s.svLan;
        else if (s.settingsRow == 2) s.p2p = !s.p2p;
        else if (s.settingsRow == 3) s.p2pFriends = !s.p2pFriends;
      }
    } else if (s.focusCol == 3) {
      if (triggerEdge) s.wantStart = true;
    }
    return;
  }

  // --- Addons page ---
  if (stickX < 0) {
    s.page = WebUIPage::NewGame;
    s.status = "NEW GAME";
    return;
  }
  if (s.addons.addons.empty()) return;
  if (stickY < 0) {
    s.addons.selected = std::max(0, s.addons.selected - 1);
    if (s.addons.selected < s.addons.scroll) s.addons.scroll = s.addons.selected;
  }
  if (stickY > 0) {
    s.addons.selected = std::min((int)s.addons.addons.size() - 1, s.addons.selected + 1);
    if (s.addons.selected >= s.addons.scroll + 12)
      s.addons.scroll = s.addons.selected - 11;
  }
  if (triggerEdge) {
    std::string err;
    if (!Addons_ToggleSelected(s.addons, err)) {
      s.status = "TOGGLE FAIL: " + err;
    } else {
      s.status = s.addons.status;
    }
  }
}

static void DrawNav(unsigned char* rgba, WebUIPage page) {
  FillRect(rgba, 0, 0, UI_W, 44, 196, 30, 58, 255);
  // Tabs like menu.html NavBar
  bool ng = page == WebUIPage::NewGame;
  bool ad = page == WebUIPage::Addons;
  FillRect(rgba, 8, 6, 160, 32, ng ? 40 : 120, ng ? 12 : 20, ng ? 18 : 40, 255);
  DrawText(rgba, 20, 14, "NEW GAME", 255, 240, 244, 2);
  FillRect(rgba, 180, 6, 140, 32, ad ? 40 : 120, ad ? 12 : 20, ad ? 18 : 40, 255);
  DrawText(rgba, 196, 14, "ADDONS", 255, 240, 244, 2);
  DrawText(rgba, 340, 14, "CUBE WEBUI NATIVE", 255, 200, 210, 1);
}

void WebUI_Rasterize(const WebUIState& s, unsigned char* rgba, const WebUICursor* cursor) {
  FillRect(rgba, 0, 0, UI_W, UI_H, 12, 6, 10, 255);
  DrawNav(rgba, s.page);

  if (s.page == WebUIPage::Addons) {
    // Left: help / counts — Right: subscribed list (enable/disable)
    FillRect(rgba, 8, 52, 220, UI_H - 60, 28, 10, 16, 255);
    FillRect(rgba, 236, 52, UI_W - 244, UI_H - 60, 36, 12, 18, 255);

    DrawText(rgba, 16, 60, "SUBSCRIBED", 200, 150, 165, 1);
    char cnt[80];
    snprintf(cnt, sizeof(cnt), "ON %d", Addons_EnabledCount(s.addons));
    DrawText(rgba, 16, 88, cnt, 90, 220, 150, 1);
    snprintf(cnt, sizeof(cnt), "OFF %d", Addons_DisabledCount(s.addons));
    DrawText(rgba, 16, 108, cnt, 255, 100, 100, 1);
    DrawText(rgba, 16, 140, "TRIGGER TOGGLE", 200, 150, 165, 1);
    DrawText(rgba, 16, 160, "MOUNT VIA", 200, 150, 165, 1);
    DrawText(rgba, 16, 180, "ADDONNOMOUNT", 200, 150, 165, 1);
    DrawText(rgba, 16, 210, "LOCAL .DISABLED", 200, 150, 165, 1);
    DrawText(rgba, 16, 250, "LEFT = NEW GAME", 255, 70, 100, 1);

    DrawText(rgba, 248, 60, "ADDON MANAGER", 255, 70, 100, 2);
    const int visible = 14;
    int start = s.addons.scroll;
    for (int n = 0; n < visible; ++n) {
      int i = start + n;
      if (i >= (int)s.addons.addons.size()) break;
      const auto& a = s.addons.addons[i];
      bool sel = (i == s.addons.selected);
      int y = 92 + n * 28;
      if (sel) FillRect(rgba, 244, y - 2, UI_W - 260, 24, 120, 22, 36, 255);
      // status pill
      if (a.enabled)
        FillRect(rgba, 248, y, 36, 18, 40, 120, 70, 255);
      else
        FillRect(rgba, 248, y, 36, 18, 100, 30, 40, 255);
      DrawText(rgba, 252, y + 3, a.enabled ? "ON" : "OFF", 255, 255, 255, 1);
      // title truncated
      char line[96];
      snprintf(line, sizeof(line), "%.48s", a.title.c_str());
      DrawText(rgba, 292, y + 3, line, 255, 240, 244, 1);
    }
    DrawText(rgba, 16, UI_H - 22, s.status.c_str(), 200, 150, 165, 1);
    if ((cursor && cursor->visible) || s.cursorVisible) {
      int cx = cursor ? cursor->x : s.cursorX;
      int cy = cursor ? cursor->y : s.cursorY;
      FillRect(rgba, cx - 8, cy - 2, 16, 4, 255, 70, 100, 255);
      FillRect(rgba, cx - 2, cy - 8, 4, 16, 255, 70, 100, 255);
    }
    return;
  }

  // --- New Game page (existing 3-col) ---
  const int catW = 180, mapX = 188, mapW = 500, setX = 700, setW = 250;
  FillRect(rgba, 8, 52, catW, UI_H - 60, 28, 10, 16, 255);
  FillRect(rgba, mapX, 52, mapW, UI_H - 60, 36, 12, 18, 255);
  FillRect(rgba, setX, 52, setW, UI_H - 60, 28, 10, 16, 255);

  DrawText(rgba, 16, 60, "CATEGORY", 200, 150, 165, 1);
  for (int i = 0; i < (int)s.categories.size() && i < 12; ++i) {
    bool sel = (i == s.catIndex);
    bool foc = (s.focusCol == 0 && sel);
    int y = 80 + i * 28;
    if (sel) FillRect(rgba, 12, y - 2, catW - 8, 24, foc ? 120 : 80, 20, 36, 255);
    DrawText(rgba, 18, y + 2, s.categories[i].name.c_str(), 255, 240, 244, 1);
  }

  DrawText(rgba, mapX + 8, 60, "MAPS", 200, 150, 165, 1);
  if (!s.categories.empty()) {
    const auto& maps = s.categories[s.catIndex].maps;
    int visible = 12;
    int start = std::max(0, s.mapIndex - visible / 2);
    for (int n = 0; n < visible; ++n) {
      int i = start + n;
      if (i >= (int)maps.size()) break;
      bool sel = (i == s.mapIndex);
      bool foc = (s.focusCol == 1 && sel);
      int y = 80 + n * 28;
      if (sel) FillRect(rgba, mapX + 4, y - 2, mapW - 8, 24, foc ? 120 : 80, 22, 36, 255);
      DrawText(rgba, mapX + 12, y + 2, maps[i].c_str(), 255, 240, 244, 1);
    }
  }

  DrawText(rgba, setX + 8, 60, "SETTINGS", 200, 150, 165, 1);
  char line[96];
  auto row = [&](int idx, const char* label) {
    int y = 84 + idx * 36;
    bool foc = (s.focusCol == 2 && s.settingsRow == idx);
    if (foc) FillRect(rgba, setX + 4, y - 4, setW - 8, 30, 90, 22, 36, 255);
    DrawText(rgba, setX + 12, y, label, 255, 240, 244, 1);
  };
  snprintf(line, sizeof(line), "PLAYERS %d", WebUI_MaxPlayers(s));
  row(0, line);
  row(1, s.svLan ? "LAN SERVER ON" : "LAN SERVER OFF");
  row(2, s.p2p ? "P2P ON" : "P2P OFF");
  row(3, s.p2pFriends ? "P2P FRIENDS ON" : "P2P FRIENDS OFF");

  int by = UI_H - 70;
  bool startFoc = (s.focusCol == 3);
  FillRect(rgba, setX + 12, by, setW - 24, 44, startFoc ? 255 : 196, startFoc ? 60 : 30, 58, 255);
  DrawText(rgba, setX + 36, by + 14, "START GAME", 255, 255, 255, 2);

  DrawText(rgba, 12, UI_H - 22, s.status.c_str(), 200, 150, 165, 1);
  char sel[128];
  snprintf(sel, sizeof(sel), "SEL %s  | AIM LASER + TRIGGER", WebUI_SelectedMap(s).c_str());
  DrawText(rgba, mapX + 8, UI_H - 22, sel, 255, 70, 100, 1);

  // Laser crosshair on panel
  if ((cursor && cursor->visible) || s.cursorVisible) {
    int cx = cursor ? cursor->x : s.cursorX;
    int cy = cursor ? cursor->y : s.cursorY;
    FillRect(rgba, cx - 10, cy - 2, 20, 4, 255, 70, 100, 255);
    FillRect(rgba, cx - 2, cy - 10, 4, 20, 255, 70, 100, 255);
    FillRect(rgba, cx - 3, cy - 3, 6, 6, 255, 255, 255, 255);
  }
}
