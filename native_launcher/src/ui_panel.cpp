#include "ui_panel.hpp"
#include "gmod_spawn.hpp"
#include "last_play.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

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

// Row-wise fill — research-2: nested PutPx full-panel clear was the structural bottleneck.
static void FillRect(unsigned char* rgba, int x, int y, int w, int h, int r, int g, int b, int a = 255) {
  if (w <= 0 || h <= 0) return;
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > UI_W) w = UI_W - x;
  if (y + h > UI_H) h = UI_H - y;
  if (w <= 0 || h <= 0) return;
  unsigned char px[4] = {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};
  // First row
  unsigned char* row0 = rgba + (y * UI_W + x) * 4;
  for (int i = 0; i < w; ++i) {
    row0[i * 4 + 0] = px[0];
    row0[i * 4 + 1] = px[1];
    row0[i * 4 + 2] = px[2];
    row0[i * 4 + 3] = px[3];
  }
  const int rowBytes = w * 4;
  for (int j = 1; j < h; ++j)
    std::memcpy(rgba + ((y + j) * UI_W + x) * 4, row0, rowBytes);
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

// --- Settings row IDs: Source gfx → OpenXR → server ---
enum SetRow : int {
  SR_PRESET = 0,
  SR_RES,
  SR_TEX,
  SR_MODELS,
  SR_AA,
  SR_AF,
  SR_HDR,
  SR_SHADOWS,
  SR_FL_SHADOWS,
  SR_SPECULAR,
  SR_BUMP,
  SR_WATER,
  SR_FPS,
  SR_MULTICORE,
  // OpenXR backend (vrmod_*)
  SR_XR_SS,
  SR_XR_VIEWSCALE,
  SR_XR_FOVSCALE,
  SR_XR_SCALEFACTOR,
  SR_XR_EYESCALE,
  SR_XR_ZNEAR,
  SR_XR_DESKTOP,
  SR_XR_POST,
  SR_XR_SWAP,
  SR_XR_SKYBOX,
  SR_XR_MQ2,
  SR_XR_RENDEROFFSET,
  SR_XR_FOCUS,
  // Server
  SR_PLAYERS,
  SR_LAN,
  SR_P2P,
  SR_P2P_FRIENDS,
  SR_COUNT
};

int WebUI_SettingsRowCount() { return SR_COUNT; }

struct WinRes { int w, h; const char* label; };
static const WinRes kRes[] = {
  {720, 480, "720x480"},
  {960, 540, "960x540"},
  {1280, 720, "1280x720"},
  {1600, 900, "1600x900"},
  {1920, 1080, "1920x1080"},
  {2560, 1440, "2560x1440"},
};
static constexpr int kResN = 6;
static const int kAa[] = {0, 2, 4, 8};
static const int kAf[] = {0, 2, 4, 8, 16};
static const int kFps[] = {0, 60, 90, 120, 144, 240};
// OpenXR supersample ladder (vrmod_supersample)
static const float kSs[] = {0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
static constexpr int kSsN = 6;
static const float kViewSc[] = {0.85f, 0.9f, 1.0f, 1.1f, 1.25f};
static constexpr int kViewScN = 5;
static const float kFovSc[] = {0.9f, 0.95f, 1.0f, 1.05f, 1.1f};
static constexpr int kFovScN = 5;
static const float kScaleF[] = {0.9f, 0.95f, 1.0f, 1.05f, 1.1f};
static constexpr int kScaleFN = 5;
static const float kEyeSc[] = {0.35f, 0.45f, 0.5f, 0.55f, 0.65f};
static constexpr int kEyeScN = 5;
static const float kZNear[] = {0.5f, 1.0f, 1.5f, 2.0f, 3.0f};
static constexpr int kZNearN = 5;

static int IndexOf(const int* a, int n, int v) {
  for (int i = 0; i < n; ++i) if (a[i] == v) return i;
  return 0;
}
static int IndexOfF(const float* a, int n, float v) {
  int best = 0;
  float bd = 1e9f;
  for (int i = 0; i < n; ++i) {
    float d = std::fabs(a[i] - v);
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

void WebUI_ApplyGfxPreset(WebUIState& s, int preset) {
  preset = std::clamp(preset, 0, 3);
  s.gfx.preset = preset;
  auto& x = s.gfx.xr;
  // Low / Med / High / Ultra → Source + OpenXR defaults
  if (preset == 0) {
    s.gfx.matPicmip = 2; s.gfx.rRootLod = 2; s.gfx.matAntialias = 0; s.gfx.matForceAniso = 0;
    s.gfx.matHdrLevel = 0; s.gfx.shadows = false; s.gfx.flashlightShadows = false;
    s.gfx.specular = false; s.gfx.bumpmap = true; s.gfx.waterExpensive = false;
    s.gfx.multicore = true; s.gfx.fpsMax = 60;
    x.ssIdx = 1; x.viewScale = 1.0f; x.fovScaleX = 1.0f; x.fovScaleY = 1.0f; x.scaleFactor = 1.0f;
    x.fovTouched = false; x.postProcess = false; x.skybox = false; x.mq2SinglePass = true;
    x.desktopView = 1;
  } else if (preset == 1) {
    s.gfx.matPicmip = 1; s.gfx.rRootLod = 1; s.gfx.matAntialias = 2; s.gfx.matForceAniso = 4;
    s.gfx.matHdrLevel = 1; s.gfx.shadows = true; s.gfx.flashlightShadows = false;
    s.gfx.specular = true; s.gfx.bumpmap = true; s.gfx.waterExpensive = false;
    s.gfx.multicore = true; s.gfx.fpsMax = 90;
    x.ssIdx = 2; x.viewScale = 1.0f; x.fovScaleX = 1.0f; x.fovScaleY = 1.0f; x.scaleFactor = 1.0f;
    x.fovTouched = false; x.postProcess = false; x.skybox = false; x.mq2SinglePass = true;
    x.desktopView = 1;
  } else if (preset == 2) {
    s.gfx.matPicmip = 0; s.gfx.rRootLod = 0; s.gfx.matAntialias = 4; s.gfx.matForceAniso = 8;
    s.gfx.matHdrLevel = 2; s.gfx.shadows = true; s.gfx.flashlightShadows = true;
    s.gfx.specular = true; s.gfx.bumpmap = true; s.gfx.waterExpensive = true;
    s.gfx.multicore = true; s.gfx.fpsMax = 0;
    x.ssIdx = 3; x.viewScale = 1.0f; x.fovScaleX = 1.0f; x.fovScaleY = 1.0f; x.scaleFactor = 1.0f;
    x.fovTouched = false; x.postProcess = false; x.skybox = false; x.mq2SinglePass = true;
    x.desktopView = 1;
  } else {
    s.gfx.matPicmip = -1; s.gfx.rRootLod = 0; s.gfx.matAntialias = 8; s.gfx.matForceAniso = 16;
    s.gfx.matHdrLevel = 2; s.gfx.shadows = true; s.gfx.flashlightShadows = true;
    s.gfx.specular = true; s.gfx.bumpmap = true; s.gfx.waterExpensive = true;
    s.gfx.multicore = true; s.gfx.fpsMax = 0;
    x.ssIdx = 5; x.viewScale = 1.0f; x.fovScaleX = 1.0f; x.fovScaleY = 1.0f; x.scaleFactor = 1.0f;
    x.fovTouched = false; x.postProcess = true; x.skybox = true; x.mq2SinglePass = true;
    x.desktopView = 1;
  }
}

static void SyncResFromIdx(GModGfxSettings& g) {
  int i = std::clamp(g.resIdx, 0, kResN - 1);
  g.resIdx = i;
  g.winW = kRes[i].w;
  g.winH = kRes[i].h;
}

void WebUI_CycleSetting(WebUIState& s, int row, int dir) {
  if (dir == 0) dir = 1;
  auto& g = s.gfx;
  switch (row) {
    case SR_PRESET:
      g.preset = (g.preset + dir + 4) % 4;
      WebUI_ApplyGfxPreset(s, g.preset);
      break;
    case SR_RES:
      g.resIdx = (g.resIdx + dir + kResN) % kResN;
      SyncResFromIdx(g);
      g.preset = -1;
      break;
    case SR_TEX:
      g.matPicmip = std::clamp(g.matPicmip - dir, -1, 2); // higher quality = lower picmip
      g.preset = -1;
      break;
    case SR_MODELS:
      g.rRootLod = std::clamp(g.rRootLod - dir, 0, 2);
      g.preset = -1;
      break;
    case SR_AA: {
      int i = IndexOf(kAa, 4, g.matAntialias);
      i = (i + dir + 4) % 4;
      g.matAntialias = kAa[i];
      g.preset = -1;
      break;
    }
    case SR_AF: {
      int i = IndexOf(kAf, 5, g.matForceAniso);
      i = (i + dir + 5) % 5;
      g.matForceAniso = kAf[i];
      g.preset = -1;
      break;
    }
    case SR_HDR:
      g.matHdrLevel = (g.matHdrLevel + dir + 3) % 3;
      g.preset = -1;
      break;
    case SR_SHADOWS: g.shadows = !g.shadows; g.preset = -1; break;
    case SR_FL_SHADOWS: g.flashlightShadows = !g.flashlightShadows; g.preset = -1; break;
    case SR_SPECULAR: g.specular = !g.specular; g.preset = -1; break;
    case SR_BUMP: g.bumpmap = !g.bumpmap; g.preset = -1; break;
    case SR_WATER: g.waterExpensive = !g.waterExpensive; g.preset = -1; break;
    case SR_FPS: {
      int i = IndexOf(kFps, 6, g.fpsMax);
      i = (i + dir + 6) % 6;
      g.fpsMax = kFps[i];
      g.preset = -1;
      break;
    }
    case SR_MULTICORE: g.multicore = !g.multicore; g.preset = -1; break;
    case SR_XR_SS:
      g.xr.ssIdx = (g.xr.ssIdx + dir + kSsN) % kSsN;
      g.preset = -1;
      break;
    case SR_XR_VIEWSCALE: {
      int i = IndexOfF(kViewSc, kViewScN, g.xr.viewScale);
      i = (i + dir + kViewScN) % kViewScN;
      g.xr.viewScale = kViewSc[i];
      g.preset = -1;
      break;
    }
    case SR_XR_FOVSCALE: {
      // Linked cycle for both axes when user intentionally edits (does not auto-write archive until Start)
      int i = IndexOfF(kFovSc, kFovScN, g.xr.fovScaleX);
      i = (i + dir + kFovScN) % kFovScN;
      g.xr.fovScaleX = kFovSc[i];
      g.xr.fovScaleY = kFovSc[i];
      g.xr.fovTouched = true;
      g.preset = -1;
      break;
    }
    case SR_XR_SCALEFACTOR: {
      int i = IndexOfF(kScaleF, kScaleFN, g.xr.scaleFactor);
      i = (i + dir + kScaleFN) % kScaleFN;
      g.xr.scaleFactor = kScaleF[i];
      g.preset = -1;
      break;
    }
    case SR_XR_EYESCALE: {
      int i = IndexOfF(kEyeSc, kEyeScN, g.xr.eyeScale);
      i = (i + dir + kEyeScN) % kEyeScN;
      g.xr.eyeScale = kEyeSc[i];
      g.preset = -1;
      break;
    }
    case SR_XR_ZNEAR: {
      int i = IndexOfF(kZNear, kZNearN, g.xr.zNear);
      i = (i + dir + kZNearN) % kZNearN;
      g.xr.zNear = kZNear[i];
      g.preset = -1;
      break;
    }
    case SR_XR_DESKTOP:
      // cycle 1..4 (none / left / right / follow-cam)
      g.xr.desktopView = 1 + ((g.xr.desktopView - 1 + dir + 4) % 4);
      g.preset = -1;
      break;
    case SR_XR_POST: g.xr.postProcess = !g.xr.postProcess; g.preset = -1; break;
    case SR_XR_SWAP: g.xr.swapEyes = !g.xr.swapEyes; g.preset = -1; break;
    case SR_XR_SKYBOX: g.xr.skybox = !g.xr.skybox; g.preset = -1; break;
    case SR_XR_MQ2: g.xr.mq2SinglePass = !g.xr.mq2SinglePass; g.preset = -1; break;
    case SR_XR_RENDEROFFSET: g.xr.renderOffset = !g.xr.renderOffset; g.preset = -1; break;
    case SR_XR_FOCUS: g.xr.requireFocus = !g.xr.requireFocus; g.preset = -1; break;
    case SR_PLAYERS:
      s.maxPlayersIdx = (s.maxPlayersIdx + dir + 8) % 8;
      break;
    case SR_LAN: s.svLan = !s.svLan; break;
    case SR_P2P: s.p2p = !s.p2p; break;
    case SR_P2P_FRIENDS: s.p2pFriends = !s.p2pFriends; break;
    default: break;
  }
}

static const char* PresetLabel(int p) {
  if (p < 0) return "CUSTOM";
  static const char* n[] = {"LOW", "MEDIUM", "HIGH", "ULTRA"};
  return n[std::clamp(p, 0, 3)];
}
static const char* PicmipLabel(int p) {
  if (p <= -1) return "VERY HIGH";
  if (p == 0) return "HIGH";
  if (p == 1) return "MEDIUM";
  return "LOW";
}
static const char* LodLabel(int p) {
  if (p <= 0) return "HIGH";
  if (p == 1) return "MEDIUM";
  return "LOW";
}
static const char* HdrLabel(int p) {
  if (p <= 0) return "OFF";
  if (p == 1) return "BLOOM";
  return "FULL";
}

static void FormatSettingRow(const WebUIState& s, int row, char* out, int outN) {
  const auto& g = s.gfx;
  switch (row) {
    case SR_PRESET: snprintf(out, outN, "PRESET          %s", PresetLabel(g.preset)); break;
    case SR_RES: snprintf(out, outN, "WINDOW          %s", kRes[std::clamp(g.resIdx, 0, kResN - 1)].label); break;
    case SR_TEX: snprintf(out, outN, "TEXTURES        %s", PicmipLabel(g.matPicmip)); break;
    case SR_MODELS: snprintf(out, outN, "MODELS          %s", LodLabel(g.rRootLod)); break;
    case SR_AA:
      if (g.matAntialias <= 0) snprintf(out, outN, "ANTIALIAS       OFF");
      else snprintf(out, outN, "ANTIALIAS       %dx", g.matAntialias);
      break;
    case SR_AF:
      if (g.matForceAniso <= 0) snprintf(out, outN, "ANISOTROPIC     OFF");
      else snprintf(out, outN, "ANISOTROPIC     %dx", g.matForceAniso);
      break;
    case SR_HDR: snprintf(out, outN, "HDR             %s", HdrLabel(g.matHdrLevel)); break;
    case SR_SHADOWS: snprintf(out, outN, "SHADOWS         %s", g.shadows ? "ON" : "OFF"); break;
    case SR_FL_SHADOWS: snprintf(out, outN, "FLASHLIGHT SHAD %s", g.flashlightShadows ? "ON" : "OFF"); break;
    case SR_SPECULAR: snprintf(out, outN, "SPECULAR        %s", g.specular ? "ON" : "OFF"); break;
    case SR_BUMP: snprintf(out, outN, "BUMP MAPS       %s", g.bumpmap ? "ON" : "OFF"); break;
    case SR_WATER: snprintf(out, outN, "WATER QUALITY   %s", g.waterExpensive ? "HIGH" : "LOW"); break;
    case SR_FPS:
      if (g.fpsMax <= 0) snprintf(out, outN, "FPS CAP         UNLIMITED");
      else snprintf(out, outN, "FPS CAP         %d", g.fpsMax);
      break;
    case SR_MULTICORE: snprintf(out, outN, "MULTICORE       %s", g.multicore ? "ON" : "OFF"); break;
    case SR_XR_SS: {
      float ss = kSs[std::clamp(g.xr.ssIdx, 0, kSsN - 1)];
      snprintf(out, outN, "XR SUPERSAMPLE  %.2f", ss);
      break;
    }
    case SR_XR_VIEWSCALE:
      snprintf(out, outN, "XR VIEW SCALE   %.2f", g.xr.viewScale);
      break;
    case SR_XR_FOVSCALE:
      snprintf(out, outN, "XR FOV X/Y      %.2f/%.2f%s", g.xr.fovScaleX, g.xr.fovScaleY,
               g.xr.fovTouched ? "" : " (keep archive)");
      break;
    case SR_XR_SCALEFACTOR:
      snprintf(out, outN, "XR UV SCALE     %.2f", g.xr.scaleFactor);
      break;
    case SR_XR_EYESCALE:
      snprintf(out, outN, "XR EYE OFFSET   %.2f", g.xr.eyeScale);
      break;
    case SR_XR_ZNEAR:
      snprintf(out, outN, "XR ZNEAR        %.1f", g.xr.zNear);
      break;
    case SR_XR_DESKTOP: {
      // G23: all four modes labeled (1 none / 2 left / 3 right / 4 follow-cam)
      const char* dv = "RIGHT";
      if (g.xr.desktopView == 1) dv = "NONE";
      else if (g.xr.desktopView == 2) dv = "LEFT";
      else if (g.xr.desktopView == 3) dv = "RIGHT";
      else if (g.xr.desktopView == 4) dv = "FOLLOW CAM";
      else dv = "NONE";
      snprintf(out, outN, "XR DESKTOP VIEW %s", dv);
      break;
    }
    case SR_XR_POST:
      snprintf(out, outN, "XR POSTPROCESS  %s", g.xr.postProcess ? "ON" : "OFF");
      break;
    case SR_XR_SWAP:
      snprintf(out, outN, "XR SWAP EYES    %s", g.xr.swapEyes ? "ON" : "OFF");
      break;
    case SR_XR_SKYBOX:
      snprintf(out, outN, "XR 3D SKYBOX    %s", g.xr.skybox ? "ON" : "OFF");
      break;
    case SR_XR_MQ2:
      snprintf(out, outN, "XR MQ2 1-PASS   %s", g.xr.mq2SinglePass ? "ON" : "OFF");
      break;
    case SR_XR_RENDEROFFSET:
      snprintf(out, outN, "XR RENDER OFFSET%s", g.xr.renderOffset ? " ON" : " OFF");
      break;
    case SR_XR_FOCUS:
      snprintf(out, outN, "XR NEED FOCUS   %s", g.xr.requireFocus ? "ON" : "OFF");
      break;
    case SR_PLAYERS: snprintf(out, outN, "MAX PLAYERS     %d", WebUI_MaxPlayers(s)); break;
    case SR_LAN: snprintf(out, outN, "LAN SERVER      %s", s.svLan ? "ON" : "OFF"); break;
    case SR_P2P: snprintf(out, outN, "P2P             %s", s.p2p ? "ON" : "OFF"); break;
    case SR_P2P_FRIENDS: snprintf(out, outN, "P2P FRIENDS     %s", s.p2pFriends ? "ON" : "OFF"); break;
    default: snprintf(out, outN, "?"); break;
  }
}

void WebUI_Init(WebUIState& s, const std::string& gmodRoot) {
  // In-place reset (AddonManager holds mutex — not assignable)
  s.gmodRoot = gmodRoot;
  s.page = WebUIPage::NewGame;
  s.categories.clear();
  s.catIndex = 0;
  s.mapScroll = 0;
  s.mapIndex = 0;
  s.maxPlayersIdx = 0;
  s.hostname = "gVRMod Cube";
  s.svLan = true;
  s.p2p = false;
  s.p2pFriends = false;
  s.gamemode = "sandbox";
  s.focusCol = 0;
  s.settingsRow = 0;
  s.settingsScroll = 0;
  s.gfx = GModGfxSettings{};
  s.wantStart = false;
  s.wantQuit = false;
  s.handoff = false;
  s.handoffMap.clear();
  s.handoffPhase.clear();
  s.handoffDetail.clear();
  s.handoffElapsed = 0.f;
  s.handoffFade = 0.f;
  s.handoffAudioGain = 1.f;
  s.handoffRefSpace.clear();
  s.handoffHeadY = 0.f;
  s.handoffHeadOk = false;
  s.handoffBootKind.clear();
  s.cursorVisible = false;
  s.cursorX = 0;
  s.cursorY = 0;
  s.paintDirty = true;
  s.paintFrame = 0;
  s.lastCursorQx = -9999;
  s.lastCursorQy = -9999;
  s.lastCursorVis = false;
  // Soft cursor is the "3rd reticle" (WiVRn 041757): L laser + R laser + red cross.
  // Laser tip alone is the hit feedback — no on-texture cursor.
  // Soft reticle at ray hit so tip-on-button == click pixel is visible (1:1 verify).
  s.paintSoftCursor = true;
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
  WebUI_ApplyGfxPreset(s, 2); // High defaults
  SyncResFromIdx(s.gfx);
  Addons_Load(s.addons, gmodRoot);
  Bindings_Load(s.bindings, gmodRoot);
  // G11: restore last map + gfx so Quick Play / START reuse prior session
  s.hasLastPlay = false;
  s.lastPlayMap.clear();
  if (WebUI_LoadLastPlay(s) && WebUI_ApplyLastPlayMap(s)) {
    s.status = "QUICK PLAY · " + s.lastPlayMap;
  } else {
    s.status = "NEW GAME · ADDONS · SETTINGS · BINDINGS";
  }
  WebUI_MarkDirty(s);
}

static std::string LastPlayPath(const std::string& gmodRoot) {
  if (gmodRoot.empty()) return {};
  return gmodRoot + "/garrysmod/data/vrmod/cube_last_play.txt";
}

static LastPlaySnapshot SnapshotFromUI(const WebUIState& s) {
  LastPlaySnapshot lp;
  lp.map = WebUI_SelectedMap(s);
  lp.gamemode = s.gamemode;
  lp.maxPlayers = WebUI_MaxPlayers(s);
  lp.svLan = s.svLan;
  lp.p2p = s.p2p;
  lp.p2pFriends = s.p2pFriends;
  lp.gfxPreset = s.gfx.preset;
  lp.matPicmip = s.gfx.matPicmip;
  lp.rRootLod = s.gfx.rRootLod;
  lp.matAntialias = s.gfx.matAntialias;
  lp.matForceAniso = s.gfx.matForceAniso;
  lp.matHdrLevel = s.gfx.matHdrLevel;
  lp.shadows = s.gfx.shadows;
  lp.multicore = s.gfx.multicore;
  lp.fpsMax = s.gfx.fpsMax;
  lp.winW = s.gfx.winW;
  lp.winH = s.gfx.winH;
  lp.windowed = s.gfx.windowed;
  lp.noborder = s.gfx.noborder; // never force on save of false default
  lp.xrSsIdx = s.gfx.xr.ssIdx;
  lp.xrViewScale = s.gfx.xr.viewScale;
  lp.xrScaleFactor = s.gfx.xr.scaleFactor;
  lp.xrDesktopView = s.gfx.xr.desktopView;
  lp.valid = !lp.map.empty();
  return lp;
}

bool WebUI_SaveLastPlay(const WebUIState& s) {
  const std::string path = LastPlayPath(s.gmodRoot);
  if (path.empty()) return false;
  LastPlaySnapshot lp = SnapshotFromUI(s);
  if (!lp.valid) return false;
  std::ofstream f(path);
  if (!f) return false;
  f << LastPlay_Format(lp);
  fprintf(stderr, "[cube_webui] last play saved map=%s → %s\n", lp.map.c_str(), path.c_str());
  return true;
}

bool WebUI_LoadLastPlay(WebUIState& s) {
  const std::string path = LastPlayPath(s.gmodRoot);
  if (path.empty()) return false;
  std::ifstream f(path);
  if (!f) return false;
  std::ostringstream body;
  body << f.rdbuf();
  LastPlaySnapshot lp;
  if (!LastPlay_Parse(body.str(), lp)) return false;
  // Stash onto state via fields we apply next; temporary use of lastPlayMap
  s.lastPlayMap = lp.map;
  s.hasLastPlay = true;
  // Apply gfx/server immediately; map selection in WebUI_ApplyLastPlayMap
  s.gamemode = lp.gamemode.empty() ? "sandbox" : lp.gamemode;
  s.svLan = lp.svLan;
  s.p2p = lp.p2p;
  s.p2pFriends = lp.p2pFriends;
  for (int i = 0; i < 8; ++i) {
    if (s.maxPlayersOpts[i] == lp.maxPlayers) {
      s.maxPlayersIdx = i;
      break;
    }
  }
  s.gfx.preset = lp.gfxPreset;
  s.gfx.matPicmip = lp.matPicmip;
  s.gfx.rRootLod = lp.rRootLod;
  s.gfx.matAntialias = lp.matAntialias;
  s.gfx.matForceAniso = lp.matForceAniso;
  s.gfx.matHdrLevel = lp.matHdrLevel;
  s.gfx.shadows = lp.shadows;
  s.gfx.multicore = lp.multicore;
  s.gfx.fpsMax = lp.fpsMax;
  s.gfx.winW = lp.winW;
  s.gfx.winH = lp.winH;
  s.gfx.windowed = lp.windowed;
  // Pain point: never force borderless from corrupt snapshot alone
  s.gfx.noborder = lp.noborder;
  s.gfx.xr.ssIdx = std::clamp(lp.xrSsIdx, 0, 5);
  s.gfx.xr.viewScale = lp.xrViewScale;
  s.gfx.xr.scaleFactor = lp.xrScaleFactor;
  s.gfx.xr.desktopView = lp.xrDesktopView;
  // Match resIdx if window size is a known ladder entry
  for (int i = 0; i < kResN; ++i) {
    if (kRes[i].w == s.gfx.winW && kRes[i].h == s.gfx.winH) {
      s.gfx.resIdx = i;
      break;
    }
  }
  return true;
}

bool WebUI_ApplyLastPlayMap(WebUIState& s) {
  if (!s.hasLastPlay || s.lastPlayMap.empty()) return false;
  const std::string& want = s.lastPlayMap;
  for (size_t ci = 0; ci < s.categories.size(); ++ci) {
    const auto& maps = s.categories[ci].maps;
    for (size_t mi = 0; mi < maps.size(); ++mi) {
      if (maps[mi] == want) {
        s.catIndex = (int)ci;
        s.mapIndex = (int)mi;
        return true;
      }
    }
  }
  // Map missing from disk scan — keep lastPlayMap label but no selection change
  return false;
}

bool WebUI_SaveBindingsIfDirty(WebUIState& s) {
  if (!s.bindings.dirty) return true;
  std::string err;
  if (!Bindings_Save(s.bindings, err)) {
    s.status = "BIND SAVE FAIL: " + err;
    return false;
  }
  s.status = s.bindings.status;
  return true;
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

void WebUI_MarkDirty(WebUIState& s) { s.paintDirty = true; }

bool WebUI_ShouldRepaint(WebUIState& s) {
  // Handoff progress bar animates — full rate
  if (s.handoff) return true;
  if (s.paintDirty) return true;
  // Idle heartbeat: pick up async addon meta without every-frame paint
  int interval = 12;
  if (s.page == WebUIPage::Addons) {
    // Faster when titles/thumbs still loading
    bool pending = false;
    for (const auto& a : s.addons.addons) {
      if (a.metaPending) {
        pending = true;
        break;
      }
    }
    interval = pending ? 6 : 24;
  } else if (s.page == WebUIPage::Bindings || s.page == WebUIPage::Settings) {
    interval = 30; // mostly static until input dirties
  }
  if (interval > 0 && s.paintFrame >= interval) return true;
  return false;
}

void WebUI_DidRepaint(WebUIState& s) {
  s.paintDirty = false;
  s.paintFrame = 0;
}

void WebUI_SetCursor(WebUIState& s, int px, int py, bool visible) {
  // Quantize to 4px (VRMod focused dirty law) — sub-cell motion does not repaint
  const int qx = px >> 2;
  const int qy = py >> 2;
  const bool moved = (qx != s.lastCursorQx) || (qy != s.lastCursorQy) || (visible != s.lastCursorVis);
  s.cursorX = px;
  s.cursorY = py;
  s.cursorVisible = visible;
  if (moved) {
    s.lastCursorQx = qx;
    s.lastCursorQy = qy;
    s.lastCursorVis = visible;
    // Soft cursor only when requested; laser is the primary reticle (research-2)
    if (s.paintSoftCursor) WebUI_MarkDirty(s);
  }
}

// Full-width nav geometry (shared paint + hit)
static constexpr int kNavY0 = 0, kNavY1 = 48;
static constexpr int kTabN = 4;
static constexpr int kTabX0[kTabN] = {0, 240, 480, 720};
static constexpr int kTabW = 240;
static constexpr const char* kTabLabel[kTabN] = {"NEW GAME", "ADDONS", "SETTINGS", "BINDINGS"};

bool WebUI_PointerClick(WebUIState& s, int px, int py) {
  WebUI_MarkDirty(s);
  // CLOSE bottom-left (away from rest-aim / START)
  if (py >= UI_H - 40 && py <= UI_H - 8 && px >= 8 && px <= 100) {
    WebUI_SaveBindingsIfDirty(s);
    s.wantQuit = true;
    s.status = "CLOSING";
    return true;
  }

  // Full-width nav (same geometry as DrawNav)
  if (py >= kNavY0 && py <= kNavY1) {
    auto leave = [&]() { WebUI_SaveBindingsIfDirty(s); };
    for (int i = 0; i < kTabN; ++i) {
      if (px >= kTabX0[i] && px < kTabX0[i] + kTabW) {
        leave();
        if (i == 0) {
          s.page = WebUIPage::NewGame;
          s.status = "NEW GAME";
        } else if (i == 1) {
          s.page = WebUIPage::Addons;
          s.status = "ADDONS";
        } else if (i == 2) {
          s.page = WebUIPage::Settings;
          s.status = "SETTINGS";
        } else {
          s.page = WebUIPage::Bindings;
          // Re-read disk so launcher matches what Lua last saved
          if (!s.gmodRoot.empty())
            Bindings_Load(s.bindings, s.gmodRoot);
          s.status = "CONTROLLER BINDINGS · sync " + s.bindings.filePath;
        }
        return true;
      }
    }
  }

  if (s.page == WebUIPage::Bindings) {
    // Filters ALL / FOOT / VEHICLE / CUSTOM
    const char* filters[] = {"ALL", "FOOT", "VEHICLE", "CUSTOM"};
    for (int f = 0; f < 4; ++f) {
      int x0 = 16 + f * 88;
      if (px >= x0 && px <= x0 + 82 && py >= 50 && py <= 74) {
        s.bindings.filter = f;
        s.bindings.page = 0;
        Bindings_ClampPage(s.bindings);
        s.status = filters[f];
        return true;
      }
    }
    // SAVE — same path Lua uses; always timestamp-backup first
    if (px >= UI_W - 320 && px <= UI_W - 210 && py >= 50 && py <= 74) {
      std::string err;
      if (!Bindings_Save(s.bindings, err)) s.status = err;
      else s.status = "SAVED → vrmod_openxr_bindings.json (Lua sync)";
      return true;
    }
    // RESET → Quest 3 GOLD (double-tap). Writes disk so GMod/Lua match launcher.
    if (px >= UI_W - 200 && px <= UI_W - 16 && py >= 50 && py <= 74) {
      static double lastResetTap = 0.;
      double now = (double)s.paintFrame / 72.0;
      if (now - lastResetTap < 1.5) {
        Bindings_ResetDefaults(s.bindings);
        s.status = s.bindings.status;
        lastResetTap = 0.;
      } else {
        lastResetTap = now;
        s.status = "RESET DEFAULTS? tap again to confirm";
      }
      return true;
    }
    // Prev/next
    if (py >= UI_H - 48 && py <= UI_H - 12) {
      if (px >= 16 && px <= 120) {
        s.bindings.page = std::max(0, s.bindings.page - 1);
        return true;
      }
      if (px >= UI_W - 130 && px <= UI_W - 16) {
        s.bindings.page++;
        Bindings_ClampPage(s.bindings);
        return true;
      }
    }
    std::vector<int> idx;
    Bindings_Filtered(s.bindings, idx);
    int start = s.bindings.page * s.bindings.pageSize;
    const int rowH = 52;
    for (int n = 0; n < s.bindings.pageSize; ++n) {
      int k = start + n;
      if (k >= (int)idx.size()) break;
      int y = 84 + n * rowH;
      if (py < y || py > y + rowH - 4) continue;
      s.bindings.selected = k;
      // Buttons: +CHD | ANY/ALL | DEF | CLR
      if (px >= UI_W - 340 && px <= UI_W - 270) {
        Bindings_ToggleChordSlot(s.bindings, k);
        s.status = s.bindings.status;
        return true;
      }
      if (px >= UI_W - 260 && px <= UI_W - 190) {
        Bindings_ToggleMode(s.bindings, k);
        s.status = s.bindings.status;
        return true;
      }
      if (px >= UI_W - 180 && px <= UI_W - 110) {
        const auto& list = s.bindings.logical.empty() ? Bindings_LogicalActions() : s.bindings.logical;
        const auto& info = list[idx[k]];
        Bindings_RestoreAction(s.bindings, info.id);
        s.status = s.bindings.status;
        return true;
      }
      if (px >= UI_W - 100 && px <= UI_W - 16) {
        Bindings_ClearAction(s.bindings, k);
        s.status = s.bindings.status;
        return true;
      }
      // Slot pick: left half of source line = slot0, right of mid = slot1 when chord
      if (py >= y + 24 && py <= y + rowH - 6) {
        if (px >= 120 && px < 320) {
          Bindings_SetEditSlot(s.bindings, 0);
          int dir = (px < 220) ? -1 : 1;
          Bindings_CycleSourceSlot(s.bindings, k, 0, dir);
          s.status = s.bindings.status;
          return true;
        }
        if (px >= 320 && px < UI_W - 350) {
          Bindings_SetEditSlot(s.bindings, 1);
          // ensure chord slot exists
          Bindings_CycleSourceSlot(s.bindings, k, 1, (px < 420) ? -1 : 1);
          s.status = s.bindings.status;
          return true;
        }
      }
      // Label row: cycle active edit slot
      int dir = (px < 400) ? -1 : 1;
      Bindings_CycleSourceSlot(s.bindings, k, s.bindings.editSlot, dir);
      s.status = s.bindings.status;
      return true;
    }
    return false;
  }

  if (s.page == WebUIPage::Settings) {
    const int visible = 12;
    const int rowH = 32;
    int start = s.settingsScroll;
    for (int n = 0; n < visible; ++n) {
      int i = start + n;
      if (i >= SR_COUNT) break;
      int y = 72 + n * rowH;
      if (px >= 24 && px <= UI_W - 24 && py >= y && py <= y + rowH - 4) {
        s.settingsRow = i;
        int dir = (px < UI_W / 2) ? -1 : 1;
        WebUI_CycleSetting(s, i, dir);
        char buf[96];
        FormatSettingRow(s, i, buf, sizeof(buf));
        s.status = buf;
        return true;
      }
    }
    return false;
  }

  if (s.page == WebUIPage::Addons) {
    // Filter chips
    const char* filters[] = {"ALL", "ON", "OFF", "WS", "LOCAL"};
    for (int f = 0; f < 5; ++f) {
      int x0 = 16 + f * 90;
      if (px >= x0 && px <= x0 + 84 && py >= 50 && py <= 74) {
        s.addons.filter = f;
        s.addons.page = 0;
        Addons_ClampPage(s.addons);
        s.status = filters[f];
        return true;
      }
    }
    // Page prev / next
    if (py >= UI_H - 48 && py <= UI_H - 12) {
      if (px >= 16 && px <= 120) {
        s.addons.page = std::max(0, s.addons.page - 1);
        s.status = "PAGE " + std::to_string(s.addons.page + 1);
        return true;
      }
      if (px >= UI_W - 130 && px <= UI_W - 16) {
        s.addons.page++;
        Addons_ClampPage(s.addons);
        s.status = "PAGE " + std::to_string(s.addons.page + 1);
        return true;
      }
    }
    // Rows on current page
    std::vector<int> idx;
    Addons_FilteredIndices(s.addons, idx);
    int start = s.addons.page * s.addons.pageSize;
    const int rowH = 48;
    for (int n = 0; n < s.addons.pageSize; ++n) {
      int k = start + n;
      if (k >= (int)idx.size()) break;
      int y = 84 + n * rowH;
      if (px >= 12 && px <= UI_W - 12 && py >= y && py <= y + rowH - 4) {
        int abs = idx[k];
        s.addons.selected = abs;
        std::string err;
        if (!Addons_ToggleIndex(s.addons, abs, err))
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

  // Quick server rows (full settings on SETTINGS tab)
  for (int idx = 0; idx < 4; ++idx) {
    int y = 84 + idx * 36;
    if (px >= setX + 4 && px <= setX + setW - 4 && py >= y - 4 && py <= y + 26) {
      s.focusCol = 2;
      s.settingsRow = idx;
      if (idx == 0) s.maxPlayersIdx = (s.maxPlayersIdx + 1) % 8;
      else if (idx == 1) s.svLan = !s.svLan;
      else if (idx == 2) s.p2p = !s.p2p;
      else if (idx == 3) s.p2pFriends = !s.p2pFriends;
      s.status = "SERVER SETTING";
      return true;
    }
  }

  // Link to full settings
  if (px >= setX + 8 && px <= setX + setW - 8 && py >= 240 && py <= 270) {
    s.page = WebUIPage::Settings;
    s.status = "GMOD GRAPHICS + ENGINE";
    return true;
  }

  // G11 Quick Play — one click last map + gfx
  int qy = UI_H - 120;
  if (s.hasLastPlay && px >= setX + 12 && px <= setX + setW - 12 && py >= qy && py <= qy + 36) {
    s.focusCol = 3;
    WebUI_ApplyLastPlayMap(s);
    s.wantStart = true;
    s.status = "QUICK PLAY · " + s.lastPlayMap;
    return true;
  }

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
  if (stickX || stickY || triggerEdge || backEdge) WebUI_MarkDirty(s);
  if (backEdge) {
    WebUI_SaveBindingsIfDirty(s);
    s.wantQuit = true;
    return;
  }

  if (s.page == WebUIPage::Bindings) {
    if (stickX < 0) {
      if (s.bindings.page > 0) { s.bindings.page--; return; }
      WebUI_SaveBindingsIfDirty(s);
      s.page = WebUIPage::Settings;
      s.status = "SETTINGS";
      return;
    }
    if (stickX > 0) {
      int pc = Bindings_PageCount(s.bindings);
      if (s.bindings.page + 1 < pc) { s.bindings.page++; return; }
      WebUI_SaveBindingsIfDirty(s);
      s.page = WebUIPage::NewGame;
      return;
    }
    std::vector<int> idx;
    Bindings_Filtered(s.bindings, idx);
    if (idx.empty()) return;
    if (stickY < 0) s.bindings.selected = std::max(0, s.bindings.selected - 1);
    if (stickY > 0) s.bindings.selected = std::min((int)idx.size() - 1, s.bindings.selected + 1);
    s.bindings.page = s.bindings.selected / s.bindings.pageSize;
    Bindings_ClampPage(s.bindings);
    if (triggerEdge) {
      Bindings_CyclePrimarySource(s.bindings, s.bindings.selected, 1);
      s.status = s.bindings.status;
    }
    return;
  }

  if (s.page == WebUIPage::Settings) {
    if (stickX < 0) {
      s.page = WebUIPage::Addons;
      s.status = "ADDONS";
      return;
    }
    if (stickX > 0) {
      s.page = WebUIPage::Bindings;
      s.status = "BINDINGS";
      return;
    }
    if (stickY < 0) s.settingsRow = std::max(0, s.settingsRow - 1);
    if (stickY > 0) s.settingsRow = std::min(SR_COUNT - 1, s.settingsRow + 1);
    // keep row visible
    if (s.settingsRow < s.settingsScroll) s.settingsScroll = s.settingsRow;
    if (s.settingsRow >= s.settingsScroll + 12) s.settingsScroll = s.settingsRow - 11;
    if (triggerEdge) WebUI_CycleSetting(s, s.settingsRow, 1);
    return;
  }

  if (s.page == WebUIPage::NewGame) {
    if (s.categories.empty()) return;

    if (stickX < 0) s.focusCol = std::max(0, s.focusCol - 1);
    if (stickX > 0) {
      if (s.focusCol >= 3) {
        s.page = WebUIPage::Addons;
        s.status = "ADDON MANAGER";
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
    // page prev when on addons list, else New Game
    if (s.addons.page > 0) {
      s.addons.page--;
      return;
    }
    s.page = WebUIPage::NewGame;
    s.status = "NEW GAME";
    return;
  }
  if (stickX > 0) {
    int pc = Addons_PageCount(s.addons);
    if (s.addons.page + 1 < pc) {
      s.addons.page++;
      return;
    }
    s.page = WebUIPage::Settings;
    s.status = "SETTINGS";
    return;
  }
  std::vector<int> idx;
  Addons_FilteredIndices(s.addons, idx);
  if (idx.empty()) return;
  // Find selected in filtered list
  int pos = 0;
  for (int i = 0; i < (int)idx.size(); ++i)
    if (idx[i] == s.addons.selected) { pos = i; break; }
  if (stickY < 0) pos = std::max(0, pos - 1);
  if (stickY > 0) pos = std::min((int)idx.size() - 1, pos + 1);
  s.addons.selected = idx[pos];
  s.addons.page = pos / s.addons.pageSize;
  Addons_ClampPage(s.addons);
  if (triggerEdge) {
    std::string err;
    if (!Addons_ToggleIndex(s.addons, s.addons.selected, err))
      s.status = "TOGGLE FAIL: " + err;
    else
      s.status = s.addons.status;
  }
}


// =============================================================================
// Cube UI palette — MUST match addon vrmod-x64/lua/vrmod/ui/cl_cube_theme.lua
//   vrmod.cube.Theme (and cl_vr_newgame Theme fallback)
// =============================================================================
namespace CubeTheme {
// r,g,b (alpha always 255 in RT)
inline constexpr int BG[]        = {12, 6, 10};       // T.bg
inline constexpr int BG_GLASS[]  = {22, 10, 16};      // T.bgGlass
inline constexpr int PANEL[]     = {36, 12, 18};      // T.panel
inline constexpr int PANEL_DIM[] = {28, 10, 16};      // newgame panel
inline constexpr int ROW[]       = {40, 14, 20};      // newgame row
inline constexpr int ROW_HOT[]   = {90, 22, 36};      // newgame rowHot / sel
inline constexpr int BTN[]       = {55, 14, 24};      // T.btn
inline constexpr int BTN_HOVER[] = {100, 22, 38};     // T.btnHover
inline constexpr int BTN_DIM[]   = {30, 10, 16};      // T.btnDim
inline constexpr int HEADER[]    = {196, 30, 58};     // T.crimson / header
inline constexpr int HEADER_DIM[]= {80, 12, 24};      // headerDim
inline constexpr int CRIMSON[]   = {196, 30, 58};
inline constexpr int CRIMSON_HOT[]={255, 70, 100};    // T.crimsonHot / hot
inline constexpr int CRIMSON_DIM[]={120, 20, 40};     // T.crimsonDim
inline constexpr int TEXT[]      = {255, 240, 244};   // T.text
inline constexpr int MUTED[]     = {200, 150, 165};   // T.muted
inline constexpr int OK[]        = {90, 220, 150};    // T.ok
inline constexpr int WARN[]      = {255, 200, 100};   // T.warn
inline constexpr int CLOSE[]     = {180, 40, 50};     // cubeui close
}
#define CT_RGB(c) (c)[0], (c)[1], (c)[2]

static void DrawNav(unsigned char* rgba, WebUIPage page) {
  FillRect(rgba, 0, kNavY0, UI_W, kNavY1, CT_RGB(CubeTheme::HEADER), 255);
  const WebUIPage pages[kTabN] = {WebUIPage::NewGame, WebUIPage::Addons, WebUIPage::Settings,
                                  WebUIPage::Bindings};
  for (int i = 0; i < kTabN; ++i) {
    bool on = (page == pages[i]);
    int x = kTabX0[i];
    if (on)
      FillRect(rgba, x + 4, 6, kTabW - 8, 36, CT_RGB(CubeTheme::BTN_HOVER), 255);
    else
      FillRect(rgba, x + 4, 6, kTabW - 8, 36, CT_RGB(CubeTheme::BTN_DIM), 255);
    // center-ish label
    int tx = x + 40;
    DrawText(rgba, tx, 16, kTabLabel[i], CT_RGB(CubeTheme::TEXT), 1);
  }
}

// G02: blend panel toward black (opaque RT — no real alpha; darken in place).
static void BlendTowardBlack(unsigned char* rgba, float fade) {
  fade = std::clamp(fade, 0.f, 1.f);
  if (fade <= 0.001f) return;
  const float keep = 1.f - fade;
  const int n = UI_W * UI_H * 4;
  for (int i = 0; i < n; i += 4) {
    rgba[i + 0] = (unsigned char)(rgba[i + 0] * keep);
    rgba[i + 1] = (unsigned char)(rgba[i + 1] * keep);
    rgba[i + 2] = (unsigned char)(rgba[i + 2] * keep);
  }
}

void WebUI_Rasterize(const WebUIState& s, unsigned char* rgba, const WebUICursor* cursor) {
  // Seamless handoff: keep Cube panel painted (same theme) until GMod takes XR.
  if (s.handoff) {
    FillRect(rgba, 0, 0, UI_W, UI_H, CT_RGB(CubeTheme::BG), 255);
    FillRect(rgba, 0, 0, UI_W, 52, CT_RGB(CubeTheme::HEADER), 255);
    FillRect(rgba, 0, 52, UI_W, 4, CT_RGB(CubeTheme::CRIMSON_HOT), 255);
    DrawText(rgba, 24, 16, "CUBE  ·  SEAMLESS HANDOFF", CT_RGB(CubeTheme::TEXT), 2);
    DrawText(rgba, 24, 72, "STAY IN HEADSET  ·  NO DESKTOP  ·  ONE SESSION", CT_RGB(CubeTheme::MUTED), 1);

    char line[160];
    snprintf(line, sizeof(line), "MAP     %s", s.handoffMap.empty() ? "..." : s.handoffMap.c_str());
    DrawText(rgba, 24, 130, line, CT_RGB(CubeTheme::TEXT), 2);

    {
      std::string plabel = CubeHandoffPhaseLabel(s.handoffPhase);
      snprintf(line, sizeof(line), "PHASE   %s", plabel.c_str());
    }
    DrawText(rgba, 24, 180, line, CT_RGB(CubeTheme::CRIMSON_HOT), 2);

    if (!s.handoffDetail.empty())
      DrawText(rgba, 24, 230, s.handoffDetail.c_str(), CT_RGB(CubeTheme::MUTED), 1);

    // G04: cold vs warm-detected boot (warm reuse not shipped)
    if (!s.handoffBootKind.empty()) {
      std::string bl = CubeLaunchBootLabel(s.handoffBootKind);
      snprintf(line, sizeof(line), "BOOT    %s", bl.c_str());
      DrawText(rgba, 24, 250, line, CT_RGB(CubeTheme::MUTED), 1);
    }

    // G03: show Cube shell ref space + head Y (packed for GMod continuity)
    if (!s.handoffRefSpace.empty()) {
      if (s.handoffHeadOk)
        snprintf(line, sizeof(line), "SPACE   %s  ·  HEAD Y %.2fm  ·  PACKED",
                 s.handoffRefSpace.c_str(), s.handoffHeadY);
      else
        snprintf(line, sizeof(line), "SPACE   %s  ·  HEAD  —  ·  PACKED",
                 s.handoffRefSpace.c_str());
      DrawText(rgba, 24, 270, line, CT_RGB(CubeTheme::MUTED), 1);
    }

    float t = s.handoffElapsed;
    float pulse = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(t * 2.2f));
    int barW = UI_W - 48;
    // G01: prefer phase progress when known; else cold-start time fallback (G04 honest gap)
    float phaseP = CubeHandoffProgressForPhase(s.handoffPhase);
    float coldSec = CubeColdStartProgressSeconds();
    if (coldSec < 20.f) coldSec = 55.f;
    float frac = (phaseP >= 0.f) ? phaseP : std::min(0.95f, 0.08f + t / coldSec);
    // G02: progress bar fills through release dim
    if (s.handoffFade > 0.f)
      frac = std::min(0.99f, std::max(frac, 0.78f + 0.2f * s.handoffFade));
    int fill = (int)(barW * frac);
    FillRect(rgba, 24, 300, barW, 32, CT_RGB(CubeTheme::ROW), 255);
    FillRect(rgba, 24, 300, std::max(8, fill), 32,
             (int)(196 * pulse), (int)(30 * pulse), (int)(58 * pulse), 255);
    // Accent tick on bar (Cube DrawPanel style)
    FillRect(rgba, 24, 300, 6, 32, CT_RGB(CubeTheme::CRIMSON_HOT), 255);

    if (s.handoffFade > 0.05f)
      snprintf(line, sizeof(line), "%.0fs  ·  LAYER FADE %.0f%%  ·  RELEASING TO GMOD",
               t, s.handoffFade * 100.f);
    else
      snprintf(line, sizeof(line), "%.0fs  ·  HOLDING OPENXR FOR GMOD", t);
    DrawText(rgba, 24, 360, line, CT_RGB(CubeTheme::MUTED), 1);
    // G12: ambient gain law (panel contract; actual clip optional future)
    {
      float g = s.handoffAudioGain;
      if (g < 0.f) g = 0.f;
      if (g > 1.f) g = 1.f;
      if (g <= 0.02f)
        snprintf(line, sizeof(line), "AUDIO   SILENT  ·  GAIN LAW READY");
      else if (g < 0.99f)
        snprintf(line, sizeof(line), "AUDIO   DUCK %.0f%%  ·  AMBIENT HOLD", g * 100.f);
      else
        snprintf(line, sizeof(line), "AUDIO   HOLD  ·  AMBIENT GAIN 100%%");
      DrawText(rgba, 24, 380, line, CT_RGB(CubeTheme::MUTED), 1);
    }
    DrawText(rgba, 24, 400, "THEME = LUA CUBE  ·  BINDINGS CARRY OVER  ·  NO BLACK GAP",
             CT_RGB(CubeTheme::MUTED), 1);
    DrawText(rgba, 24, 440, "When GMod signals take_xr, both eyes fade black then release XR.",
             160, 120, 130, 1);
    DrawText(rgba, 24, UI_H - 36, s.status.c_str(), 255, 200, 210, 1);
    // Intentional dim before compositor cut (panel-side; full XR layer fade is future)
    BlendTowardBlack(rgba, s.handoffFade);
    return;
  }

  FillRect(rgba, 0, 0, UI_W, UI_H, CT_RGB(CubeTheme::BG), 255);
  DrawNav(rgba, s.page);

  if (s.page == WebUIPage::Bindings) {
    FillRect(rgba, 8, 48, UI_W - 16, UI_H - 56, CT_RGB(CubeTheme::PANEL_DIM), 255);
    const char* filters[] = {"ALL", "FOOT", "VEHICLE", "CUSTOM"};
    for (int f = 0; f < 4; ++f) {
      int x0 = 16 + f * 88;
      bool on = (s.bindings.filter == f);
      FillRect(rgba, x0, 52, 82, 22, on ? CubeTheme::CRIMSON[0] : CubeTheme::BTN[0], on ? CubeTheme::CRIMSON[1] : CubeTheme::BTN[1], on ? CubeTheme::CRIMSON[2] : CubeTheme::BTN[2], 255);
      DrawText(rgba, x0 + 10, 56, filters[f], 255, 240, 244, 1);
    }
    FillRect(rgba, UI_W - 320, 52, 100, 22, CT_RGB(CubeTheme::BTN_DIM), 255);
    DrawText(rgba, UI_W - 300, 56, "SAVE", 255, 240, 244, 1);
    FillRect(rgba, UI_W - 200, 52, 180, 22, CT_RGB(CubeTheme::CRIMSON_DIM), 255);
    DrawText(rgba, UI_W - 180, 56, "RESET DEFAULTS", 255, 240, 244, 1);

    std::vector<int> idx;
    Bindings_Filtered(s.bindings, idx);
    int start = s.bindings.page * s.bindings.pageSize;
    const int rowH = 52;
    for (int n = 0; n < s.bindings.pageSize; ++n) {
      int k = start + n;
      if (k >= (int)idx.size()) break;
      const auto& list = s.bindings.logical.empty() ? Bindings_LogicalActions() : s.bindings.logical;
      const auto& info = list[idx[k]];
      BindRule rule{};
      auto it = s.bindings.actions.find(info.id);
      if (it != s.bindings.actions.end()) rule = it->second;
      int y = 84 + n * rowH;
      bool sel = (k == s.bindings.selected);
      FillRect(rgba, 12, y, UI_W - 24, rowH - 4, sel ? CubeTheme::ROW_HOT[0] : CubeTheme::ROW[0], sel ? CubeTheme::ROW_HOT[1] : CubeTheme::ROW[1], sel ? CubeTheme::ROW_HOT[2] : CubeTheme::ROW[2], 255);
      char line[96];
      snprintf(line, sizeof(line), "%.22s", info.label.c_str());
      DrawText(rgba, 20, y + 6, line, 255, 240, 244, 1);
      // Dual source chips (chord = S1 + S2 with mode ALL)
      std::string s0 = rule.sources.empty() ? "(empty)" : Bindings_SourceLabel(rule.sources[0]);
      std::string s1 = rule.sources.size() >= 2 ? Bindings_SourceLabel(rule.sources[1]) : "—";
      if (s0.size() > 14) s0 = s0.substr(0, 12) + "..";
      if (s1.size() > 14) s1 = s1.substr(0, 12) + "..";
      bool slot0 = (s.bindings.editSlot == 0 && sel);
      bool slot1 = (s.bindings.editSlot == 1 && sel);
      FillRect(rgba, 20, y + 26, 140, 18, slot0 ? CubeTheme::CRIMSON[0] : CubeTheme::BTN[0], slot0 ? CubeTheme::CRIMSON[1] : CubeTheme::BTN[1], slot0 ? CubeTheme::CRIMSON[2] : CubeTheme::BTN[2], 255);
      DrawText(rgba, 26, y + 28, s0.c_str(), 255, 240, 244, 1);
      const char* joiner = (rule.mode == "all") ? "+" : "|";
      DrawText(rgba, 164, y + 28, joiner, CT_RGB(CubeTheme::MUTED), 1);
      FillRect(rgba, 180, y + 26, 140, 18, slot1 ? CubeTheme::CRIMSON[0] : CubeTheme::BTN[0], slot1 ? CubeTheme::CRIMSON[1] : CubeTheme::BTN[1], slot1 ? CubeTheme::CRIMSON[2] : CubeTheme::BTN[2], 255);
      DrawText(rgba, 186, y + 28, s1.c_str(), 255, 240, 244, 1);
      const char* setL = rule.set.empty() ? "BOTH" : (rule.set == "driving" ? "VEH" : "FOOT");
      DrawText(rgba, UI_W - 400, y + 16, setL, 160, 180, 200, 1);
      FillRect(rgba, UI_W - 340, y + 12, 64, 24, CT_RGB(CubeTheme::BTN), 255);
      DrawText(rgba, UI_W - 332, y + 18, "+CHD", 255, 240, 244, 1);
      FillRect(rgba, UI_W - 260, y + 12, 64, 24, CT_RGB(CubeTheme::CRIMSON_DIM), 255);
      DrawText(rgba, UI_W - 252, y + 18, rule.mode == "all" ? "ALL" : "ANY", 255, 240, 244, 1);
      FillRect(rgba, UI_W - 180, y + 12, 64, 24, CT_RGB(CubeTheme::BTN_DIM), 255);
      DrawText(rgba, UI_W - 168, y + 18, "DEF", 255, 240, 244, 1);
      FillRect(rgba, UI_W - 100, y + 12, 80, 24, CT_RGB(CubeTheme::CRIMSON_DIM), 255);
      DrawText(rgba, UI_W - 84, y + 18, "CLR", 255, 240, 244, 1);
    }
    int pc = Bindings_PageCount(s.bindings);
    char page[96];
    snprintf(page, sizeof(page), "P%d/%d slot%d %s%s", s.bindings.page + 1, std::max(1, pc),
             s.bindings.editSlot + 1, s.bindings.dirty ? "* " : "", s.bindings.status.c_str());
    FillRect(rgba, 16, UI_H - 48, 100, 32, CT_RGB(CubeTheme::BTN), 255);
    DrawText(rgba, 36, UI_H - 38, "PREV", 255, 240, 244, 2);
    FillRect(rgba, UI_W - 130, UI_H - 48, 110, 32, CT_RGB(CubeTheme::BTN), 255);
    DrawText(rgba, UI_W - 110, UI_H - 38, "NEXT", 255, 240, 244, 2);
    DrawText(rgba, 130, UI_H - 36, page, CT_RGB(CubeTheme::MUTED), 1);
    DrawText(rgba, 130, UI_H - 20,
             "SAVE=write JSON  RESET x2=Quest3 gold  DEF=one action  |  same file as Lua VRMod",
             160, 120, 130, 1);
    if (s.paintSoftCursor && ((cursor && cursor->visible) || s.cursorVisible)) {
      int cx = cursor ? cursor->x : s.cursorX;
      int cy = cursor ? cursor->y : s.cursorY;
      FillRect(rgba, cx - 8, cy - 2, 16, 4, CT_RGB(CubeTheme::CRIMSON_HOT), 255);
      FillRect(rgba, cx - 2, cy - 8, 4, 16, CT_RGB(CubeTheme::CRIMSON_HOT), 255);
    }
    return;
  }

  if (s.page == WebUIPage::Addons) {
    FillRect(rgba, 8, 48, UI_W - 16, UI_H - 56, CT_RGB(CubeTheme::PANEL_DIM), 255);
    // Filters
    const char* filters[] = {"ALL", "ON", "OFF", "WS", "LOCAL"};
    for (int f = 0; f < 5; ++f) {
      int x0 = 16 + f * 90;
      bool on = (s.addons.filter == f);
      FillRect(rgba, x0, 52, 84, 22, on ? CubeTheme::CRIMSON[0] : CubeTheme::BTN[0], on ? CubeTheme::CRIMSON[1] : CubeTheme::BTN[1], on ? CubeTheme::CRIMSON[2] : CubeTheme::BTN[2], 255);
      DrawText(rgba, x0 + 12, 56, filters[f], 255, 240, 244, 1);
    }
    char cnt[96];
    std::vector<int> idx;
    Addons_FilteredIndices(s.addons, idx);
    int pc = Addons_PageCount(s.addons);
    snprintf(cnt, sizeof(cnt), "%zu SHOWN / %zu TOTAL  ON %d  OFF %d  P%d/%d",
             idx.size(), s.addons.addons.size(),
             Addons_EnabledCount(s.addons), Addons_DisabledCount(s.addons),
             s.addons.page + 1, std::max(1, pc));
    DrawText(rgba, 480, 56, cnt, CT_RGB(CubeTheme::MUTED), 1);

    int start = s.addons.page * s.addons.pageSize;
    const int rowH = 48;
    for (int n = 0; n < s.addons.pageSize; ++n) {
      int k = start + n;
      if (k >= (int)idx.size()) break;
      int ai = idx[k];
      const auto& a = s.addons.addons[ai];
      int y = 84 + n * rowH;
      bool sel = (ai == s.addons.selected);
      FillRect(rgba, 12, y, UI_W - 24, rowH - 4, sel ? CubeTheme::ROW_HOT[0] : CubeTheme::ROW[0], sel ? CubeTheme::ROW_HOT[1] : CubeTheme::ROW[1], sel ? CubeTheme::ROW_HOT[2] : CubeTheme::ROW[2], 255);
      // Thumb 40x40
      int tx = 18, ty = y + 4;
      if (a.thumbW > 0 && (int)a.thumbRgba.size() >= a.thumbW * a.thumbH * 4) {
        for (int j = 0; j < 40; ++j) {
          for (int i = 0; i < 40; ++i) {
            int sx = i * a.thumbW / 40;
            int sy = j * a.thumbH / 40;
            int si = (sy * a.thumbW + sx) * 4;
            PutPx(rgba, tx + i, ty + j,
                  a.thumbRgba[si], a.thumbRgba[si + 1], a.thumbRgba[si + 2], 255);
          }
        }
      } else {
        int pr = a.metaPending ? 90 : (a.kind == "workshop" ? 60 : 40);
        int pg = a.metaPending ? 70 : (a.kind == "workshop" ? 40 : 70);
        int pb = 90;
        FillRect(rgba, tx, ty, 40, 40, pr, pg, pb, 255);
        DrawText(rgba, tx + 4, ty + 14, a.metaPending ? "..." : (a.kind == "workshop" ? "WS" : "L"), 255, 255, 255, 1);
      }
      // ON/OFF badge
      if (a.enabled)
        FillRect(rgba, 66, y + 12, 44, 20, CT_RGB(CubeTheme::OK), 255);
      else
        FillRect(rgba, 66, y + 12, 44, 20, CT_RGB(CubeTheme::CRIMSON_DIM), 255);
      DrawText(rgba, 72, y + 16, a.enabled ? "ON" : "OFF", 255, 255, 255, 1);
      char line[96];
      snprintf(line, sizeof(line), "%.52s", a.title.c_str());
      DrawText(rgba, 120, y + 10, line, 255, 240, 244, 1);
      snprintf(line, sizeof(line), "%s  %s", a.kind.c_str(), a.id.c_str());
      DrawText(rgba, 120, y + 28, line, 160, 120, 130, 1);
    }

    // Page buttons
    FillRect(rgba, 16, UI_H - 48, 100, 32, CT_RGB(CubeTheme::BTN), 255);
    DrawText(rgba, 36, UI_H - 38, "PREV", 255, 240, 244, 2);
    FillRect(rgba, UI_W - 130, UI_H - 48, 110, 32, CT_RGB(CubeTheme::BTN), 255);
    DrawText(rgba, UI_W - 110, UI_H - 38, "NEXT", 255, 240, 244, 2);
    DrawText(rgba, 140, UI_H - 36, s.addons.status.c_str(), CT_RGB(CubeTheme::MUTED), 1);
    DrawText(rgba, 140, UI_H - 20, "TRIGGER = TOGGLE MOUNT  ·  CLOSE = EXIT", 160, 120, 130, 1);

    if (s.paintSoftCursor && ((cursor && cursor->visible) || s.cursorVisible)) {
      int cx = cursor ? cursor->x : s.cursorX;
      int cy = cursor ? cursor->y : s.cursorY;
      FillRect(rgba, cx - 8, cy - 2, 16, 4, CT_RGB(CubeTheme::CRIMSON_HOT), 255);
      FillRect(rgba, cx - 2, cy - 8, 4, 16, CT_RGB(CubeTheme::CRIMSON_HOT), 255);
    }
    return;
  }

  // --- Settings page (GMod native graphics + engine) ---
  if (s.page == WebUIPage::Settings) {
    FillRect(rgba, 8, 52, UI_W - 16, UI_H - 60, CT_RGB(CubeTheme::PANEL_DIM), 255);
    DrawText(rgba, 20, 58, "SOURCE + OPENXR  ·  TRIGGER CYCLES  ·  XR SS NEEDS VR RESTART", CT_RGB(CubeTheme::MUTED), 1);
    char line[96];
    const int visible = 12;
    const int rowH = 32;
    int start = std::max(0, std::min(s.settingsScroll, SR_COUNT - 1));
    for (int n = 0; n < visible; ++n) {
      int i = start + n;
      if (i >= SR_COUNT) break;
      int y = 72 + n * rowH;
      bool foc = (i == s.settingsRow);
      if (foc) FillRect(rgba, 16, y, UI_W - 32, rowH - 4, CT_RGB(CubeTheme::BTN_HOVER), 255);
      else if (n % 2) FillRect(rgba, 16, y, UI_W - 32, rowH - 4, CT_RGB(CubeTheme::PANEL), 255);
      FormatSettingRow(s, i, line, sizeof(line));
      DrawText(rgba, 28, y + 8, line, 255, 240, 244, 1);
    }
    snprintf(line, sizeof(line), "APPLIED ON START VIA gvrmod_cube.cfg + -w/-h");
    DrawText(rgba, 20, UI_H - 48, line, 160, 120, 130, 1);
    DrawText(rgba, 20, UI_H - 28, s.status.c_str(), CT_RGB(CubeTheme::MUTED), 1);
    if (s.paintSoftCursor && ((cursor && cursor->visible) || s.cursorVisible)) {
      int cx = cursor ? cursor->x : s.cursorX;
      int cy = cursor ? cursor->y : s.cursorY;
      FillRect(rgba, cx - 10, cy - 2, 20, 4, CT_RGB(CubeTheme::CRIMSON_HOT), 255);
      FillRect(rgba, cx - 2, cy - 10, 4, 20, CT_RGB(CubeTheme::CRIMSON_HOT), 255);
    }
    return;
  }

  // --- New Game page (3-col) ---
  const int catW = 180, mapX = 188, mapW = 500, setX = 700, setW = 250;
  FillRect(rgba, 8, 52, catW, UI_H - 60, CT_RGB(CubeTheme::PANEL_DIM), 255);
  FillRect(rgba, mapX, 52, mapW, UI_H - 60, CT_RGB(CubeTheme::PANEL), 255);
  FillRect(rgba, setX, 52, setW, UI_H - 60, CT_RGB(CubeTheme::PANEL_DIM), 255);

  DrawText(rgba, 16, 60, "CATEGORY", CT_RGB(CubeTheme::MUTED), 1);
  for (int i = 0; i < (int)s.categories.size() && i < 12; ++i) {
    bool sel = (i == s.catIndex);
    bool foc = (s.focusCol == 0 && sel);
    int y = 80 + i * 28;
    if (sel) FillRect(rgba, 12, y - 2, catW - 8, 24, foc ? CubeTheme::ROW_HOT[0] : CubeTheme::ROW[0], foc ? CubeTheme::ROW_HOT[1] : CubeTheme::ROW[1], foc ? CubeTheme::ROW_HOT[2] : CubeTheme::ROW[2], 255);
    DrawText(rgba, 18, y + 2, s.categories[i].name.c_str(), 255, 240, 244, 1);
  }

  DrawText(rgba, mapX + 8, 60, "MAPS", CT_RGB(CubeTheme::MUTED), 1);
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
      if (sel) FillRect(rgba, mapX + 4, y - 2, mapW - 8, 24, foc ? CubeTheme::ROW_HOT[0] : CubeTheme::ROW[0], foc ? CubeTheme::ROW_HOT[1] : CubeTheme::ROW[1], foc ? CubeTheme::ROW_HOT[2] : CubeTheme::ROW[2], 255);
      DrawText(rgba, mapX + 12, y + 2, maps[i].c_str(), 255, 240, 244, 1);
    }
  }

  DrawText(rgba, setX + 8, 60, "SERVER", CT_RGB(CubeTheme::MUTED), 1);
  char line[96];
  auto row = [&](int idx, const char* label) {
    int y = 84 + idx * 36;
    bool foc = (s.focusCol == 2 && s.settingsRow == idx);
    if (foc) FillRect(rgba, setX + 4, y - 4, setW - 8, 30, CT_RGB(CubeTheme::ROW_HOT), 255);
    DrawText(rgba, setX + 12, y, label, 255, 240, 244, 1);
  };
  snprintf(line, sizeof(line), "PLAYERS %d", WebUI_MaxPlayers(s));
  row(0, line);
  row(1, s.svLan ? "LAN SERVER ON" : "LAN SERVER OFF");
  row(2, s.p2p ? "P2P ON" : "P2P OFF");
  row(3, s.p2pFriends ? "P2P FRIENDS ON" : "P2P FRIENDS OFF");

  // Graphics + OpenXR summary → Settings tab
  FillRect(rgba, setX + 8, 236, setW - 16, 56, CT_RGB(CubeTheme::BTN_DIM), 255);
  snprintf(line, sizeof(line), "GFX %s", PresetLabel(s.gfx.preset));
  DrawText(rgba, setX + 16, 244, line, 255, 200, 210, 1);
  float ss = kSs[std::clamp(s.gfx.xr.ssIdx, 0, kSsN - 1)];
  snprintf(line, sizeof(line), "XR SS %.2f  AA%d", ss, s.gfx.matAntialias);
  DrawText(rgba, setX + 16, 266, line, CT_RGB(CubeTheme::MUTED), 1);

  // G11 Quick Play button (last map + gfx) when snapshot exists
  int qy = UI_H - 120;
  if (s.hasLastPlay) {
    FillRect(rgba, setX + 12, qy, setW - 24, 36, CT_RGB(CubeTheme::BTN_HOVER), 255);
    DrawText(rgba, setX + 28, qy + 10, "QUICK PLAY", 255, 240, 244, 1);
    // short map label under-ish in status; button stays one line
    char qp[48];
    snprintf(qp, sizeof(qp), "%.18s", s.lastPlayMap.c_str());
    DrawText(rgba, setX + 28, qy + 22, qp, CT_RGB(CubeTheme::MUTED), 1);
  }

  int by = UI_H - 70;
  bool startFoc = (s.focusCol == 3);
  FillRect(rgba, setX + 12, by, setW - 24, 44, startFoc ? CubeTheme::CRIMSON_HOT[0] : CubeTheme::CRIMSON[0], startFoc ? CubeTheme::CRIMSON_HOT[1] : CubeTheme::CRIMSON[1], startFoc ? CubeTheme::CRIMSON_HOT[2] : CubeTheme::CRIMSON[2], 255);
  DrawText(rgba, setX + 36, by + 14, "START GAME", 255, 255, 255, 2);

  FillRect(rgba, 8, UI_H - 36, 92, 28, CT_RGB(CubeTheme::CLOSE), 255);
  DrawText(rgba, 22, UI_H - 28, "CLOSE", 255, 255, 255, 1);
  DrawText(rgba, 110, UI_H - 28, s.status.c_str(), CT_RGB(CubeTheme::MUTED), 1);
  char sel[128];
  snprintf(sel, sizeof(sel), "SEL %s", WebUI_SelectedMap(s).c_str());
  DrawText(rgba, mapX + 8, UI_H - 28, sel, CT_RGB(CubeTheme::CRIMSON_HOT), 1);

  if (s.paintSoftCursor && ((cursor && cursor->visible) || s.cursorVisible)) {
    int cx = cursor ? cursor->x : s.cursorX;
    int cy = cursor ? cursor->y : s.cursorY;
    FillRect(rgba, cx - 10, cy - 2, 20, 4, CT_RGB(CubeTheme::CRIMSON_HOT), 255);
    FillRect(rgba, cx - 2, cy - 10, 4, 20, CT_RGB(CubeTheme::CRIMSON_HOT), 255);
    FillRect(rgba, cx - 3, cy - 3, 6, 6, 255, 255, 255, 255);
  }
}
