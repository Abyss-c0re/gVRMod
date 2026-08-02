#include "ui_panel.hpp"
#include <algorithm>
#include <cmath>
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
      // cycle 1..3
      g.xr.desktopView = 1 + ((g.xr.desktopView - 1 + dir + 3) % 3);
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
      const char* dv = "RIGHT";
      if (g.xr.desktopView == 1) dv = "NONE";
      else if (g.xr.desktopView == 2) dv = "LEFT";
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
  s.cursorVisible = false;
  s.cursorX = 0;
  s.cursorY = 0;
  s.paintDirty = true;
  s.paintFrame = 0;
  s.lastCursorQx = -9999;
  s.lastCursorQy = -9999;
  s.lastCursorVis = false;
  s.paintSoftCursor = true; // laser + soft crosshair (Quest feedback)
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
  s.status = "NEW GAME · ADDONS · SETTINGS · BINDINGS";
  WebUI_MarkDirty(s);
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

bool WebUI_PointerClick(WebUIState& s, int px, int py) {
  WebUI_MarkDirty(s);
  // Always: CLOSE / QUIT top-right
  if (py >= 4 && py <= 40 && px >= UI_W - 110 && px <= UI_W - 8) {
    WebUI_SaveBindingsIfDirty(s);
    s.wantQuit = true;
    s.status = "CLOSING";
    return true;
  }

  // Nav tabs (compact)
  if (py >= 6 && py <= 38) {
    auto leave = [&]() { WebUI_SaveBindingsIfDirty(s); };
    if (px >= 8 && px <= 120) {
      leave();
      s.page = WebUIPage::NewGame;
      s.status = "NEW GAME";
      return true;
    }
    if (px >= 128 && px <= 230) {
      leave();
      s.page = WebUIPage::Addons;
      s.status = "ADDONS";
      return true;
    }
    if (px >= 238 && px <= 360) {
      leave();
      s.page = WebUIPage::Settings;
      s.status = "SETTINGS";
      return true;
    }
    if (px >= 368 && px <= 500) {
      s.page = WebUIPage::Bindings;
      s.status = "CONTROLLER BINDINGS (OpenXR)";
      return true;
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
    // RESET ALL
    if (px >= UI_W - 200 && px <= UI_W - 16 && py >= 50 && py <= 74) {
      Bindings_ResetDefaults(s.bindings);
      s.status = s.bindings.status;
      return true;
    }
    // SAVE → same file Lua uses
    if (px >= UI_W - 320 && px <= UI_W - 210 && py >= 50 && py <= 74) {
      std::string err;
      if (!Bindings_Save(s.bindings, err)) s.status = err;
      else s.status = "SAVED → vrmod_openxr_bindings.json (Lua sync)";
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

static void DrawNav(unsigned char* rgba, WebUIPage page) {
  FillRect(rgba, 0, 0, UI_W, 44, 36, 44, 62, 255);
  bool ng = page == WebUIPage::NewGame;
  bool ad = page == WebUIPage::Addons;
  bool st = page == WebUIPage::Settings;
  bool bd = page == WebUIPage::Bindings;
  FillRect(rgba, 8, 6, 110, 32, ng ? 28 : 0, ng ? 34 : 140, ng ? 46 : 190, 255);
  DrawText(rgba, 16, 14, "NEW GAME", 255, 240, 244, 1);
  FillRect(rgba, 128, 6, 100, 32, ad ? 28 : 0, ad ? 34 : 140, ad ? 46 : 190, 255);
  DrawText(rgba, 144, 14, "ADDONS", 255, 240, 244, 1);
  FillRect(rgba, 238, 6, 120, 32, st ? 28 : 0, st ? 34 : 140, st ? 46 : 190, 255);
  DrawText(rgba, 250, 14, "SETTINGS", 255, 240, 244, 1);
  FillRect(rgba, 368, 6, 128, 32, bd ? 28 : 0, bd ? 34 : 140, bd ? 46 : 190, 255);
  DrawText(rgba, 384, 14, "BINDINGS", 255, 240, 244, 1);
  FillRect(rgba, UI_W - 110, 6, 100, 32, 180, 55, 55, 255);
  DrawText(rgba, UI_W - 92, 14, "CLOSE", 255, 255, 255, 2);
}

void WebUI_Rasterize(const WebUIState& s, unsigned char* rgba, const WebUICursor* cursor) {
  // Full-panel seamless handoff (no black gap while GMod boots)
  if (s.handoff) {
    FillRect(rgba, 0, 0, UI_W, UI_H, 14, 16, 22, 255);
    FillRect(rgba, 0, 0, UI_W, 52, 36, 44, 62, 255);
    DrawText(rgba, 24, 16, "CUBE  ·  STARTING GMOD", 255, 240, 244, 2);
    DrawText(rgba, 24, 80, "NO GAPS  ·  HOLDING OPENXR", 150, 165, 185, 1);

    char line[160];
    snprintf(line, sizeof(line), "MAP  %s", s.handoffMap.empty() ? "..." : s.handoffMap.c_str());
    DrawText(rgba, 24, 130, line, 255, 240, 244, 2);

    snprintf(line, sizeof(line), "PHASE  %s",
             s.handoffPhase.empty() ? "SPAWNING" : s.handoffPhase.c_str());
    DrawText(rgba, 24, 180, line, 0, 220, 255, 2);

    if (!s.handoffDetail.empty())
      DrawText(rgba, 24, 220, s.handoffDetail.c_str(), 200, 180, 190, 1);

    // Progress bar from elapsed (soft, not a hard %)
    float t = s.handoffElapsed;
    float pulse = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(t * 2.2f));
    int barW = UI_W - 48;
    int fill = (int)(barW * std::min(0.92f, 0.12f + t / 45.f));
    FillRect(rgba, 24, 300, barW, 28, 42, 48, 62, 255);
    FillRect(rgba, 24, 300, std::max(8, fill), 28, (int)(0 * pulse), (int)(180 * pulse), (int)(220 * pulse), 255);

    snprintf(line, sizeof(line), "%.0fs  ·  STAY IN VR UNTIL GMOD TAKES SESSION", t);
    DrawText(rgba, 24, 350, line, 150, 165, 185, 1);
    DrawText(rgba, 24, 400, "PASSTHROUGH STAYS  ·  WORLD PANEL STAYS", 120, 135, 155, 1);
    DrawText(rgba, 24, UI_H - 36, s.status.c_str(), 200, 230, 245, 1);
    return;
  }

  FillRect(rgba, 0, 0, UI_W, UI_H, 18, 20, 26, 255);
  DrawNav(rgba, s.page);

  if (s.page == WebUIPage::Bindings) {
    FillRect(rgba, 8, 48, UI_W - 16, UI_H - 56, 30, 34, 46, 255);
    const char* filters[] = {"ALL", "FOOT", "VEHICLE", "CUSTOM"};
    for (int f = 0; f < 4; ++f) {
      int x0 = 16 + f * 88;
      bool on = (s.bindings.filter == f);
      FillRect(rgba, x0, 52, 82, 22, on ? 0 : 50, on ? 130 : 58, on ? 180 : 74, 255);
      DrawText(rgba, x0 + 10, 56, filters[f], 255, 240, 244, 1);
    }
    FillRect(rgba, UI_W - 320, 52, 100, 22, 48, 56, 72, 255);
    DrawText(rgba, UI_W - 300, 56, "SAVE", 255, 240, 244, 1);
    FillRect(rgba, UI_W - 200, 52, 180, 22, 50, 70, 90, 255);
    DrawText(rgba, UI_W - 188, 56, "RESET DEFAULTS", 255, 240, 244, 1);

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
      FillRect(rgba, 12, y, UI_W - 24, rowH - 4, sel ? 20 : 42, sel ? 95 : 48, sel ? 130 : 62, 255);
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
      FillRect(rgba, 20, y + 26, 140, 18, slot0 ? 0 : 55, slot0 ? 140 : 62, slot0 ? 190 : 80, 255);
      DrawText(rgba, 26, y + 28, s0.c_str(), 255, 240, 244, 1);
      const char* joiner = (rule.mode == "all") ? "+" : "|";
      DrawText(rgba, 164, y + 28, joiner, 150, 165, 185, 1);
      FillRect(rgba, 180, y + 26, 140, 18, slot1 ? 0 : 55, slot1 ? 140 : 62, slot1 ? 190 : 80, 255);
      DrawText(rgba, 186, y + 28, s1.c_str(), 255, 240, 244, 1);
      const char* setL = rule.set.empty() ? "BOTH" : (rule.set == "driving" ? "VEH" : "FOOT");
      DrawText(rgba, UI_W - 400, y + 16, setL, 160, 180, 200, 1);
      FillRect(rgba, UI_W - 340, y + 12, 64, 24, 60, 25, 45, 255);
      DrawText(rgba, UI_W - 332, y + 18, "+CHD", 255, 240, 244, 1);
      FillRect(rgba, UI_W - 260, y + 12, 64, 24, 70, 20, 40, 255);
      DrawText(rgba, UI_W - 252, y + 18, rule.mode == "all" ? "ALL" : "ANY", 255, 240, 244, 1);
      FillRect(rgba, UI_W - 180, y + 12, 64, 24, 50, 30, 50, 255);
      DrawText(rgba, UI_W - 168, y + 18, "DEF", 255, 240, 244, 1);
      FillRect(rgba, UI_W - 100, y + 12, 80, 24, 160, 50, 55, 255);
      DrawText(rgba, UI_W - 84, y + 18, "CLR", 255, 240, 244, 1);
    }
    int pc = Bindings_PageCount(s.bindings);
    char page[96];
    snprintf(page, sizeof(page), "P%d/%d slot%d %s%s", s.bindings.page + 1, std::max(1, pc),
             s.bindings.editSlot + 1, s.bindings.dirty ? "* " : "", s.bindings.status.c_str());
    FillRect(rgba, 16, UI_H - 48, 100, 32, 40, 85, 115, 255);
    DrawText(rgba, 36, UI_H - 38, "PREV", 255, 240, 244, 2);
    FillRect(rgba, UI_W - 130, UI_H - 48, 110, 32, 40, 85, 115, 255);
    DrawText(rgba, UI_W - 110, UI_H - 38, "NEXT", 255, 240, 244, 2);
    DrawText(rgba, 130, UI_H - 36, page, 150, 165, 185, 1);
    DrawText(rgba, 130, UI_H - 20,
             "S1/S2=CHORD  +CHD  ANY=OR ALL=AND  DEF  CLR  |  SYNC data/vrmod/vrmod_openxr_bindings.json",
             120, 135, 155, 1);
    if ((cursor && cursor->visible) || s.cursorVisible) {
      int cx = cursor ? cursor->x : s.cursorX;
      int cy = cursor ? cursor->y : s.cursorY;
      FillRect(rgba, cx - 8, cy - 2, 16, 4, 0, 220, 255, 255);
      FillRect(rgba, cx - 2, cy - 8, 4, 16, 0, 220, 255, 255);
    }
    return;
  }

  if (s.page == WebUIPage::Addons) {
    FillRect(rgba, 8, 48, UI_W - 16, UI_H - 56, 30, 34, 46, 255);
    // Filters
    const char* filters[] = {"ALL", "ON", "OFF", "WS", "LOCAL"};
    for (int f = 0; f < 5; ++f) {
      int x0 = 16 + f * 90;
      bool on = (s.addons.filter == f);
      FillRect(rgba, x0, 52, 84, 22, on ? 0 : 50, on ? 130 : 58, on ? 180 : 74, 255);
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
    DrawText(rgba, 480, 56, cnt, 150, 165, 185, 1);

    int start = s.addons.page * s.addons.pageSize;
    const int rowH = 48;
    for (int n = 0; n < s.addons.pageSize; ++n) {
      int k = start + n;
      if (k >= (int)idx.size()) break;
      int ai = idx[k];
      const auto& a = s.addons.addons[ai];
      int y = 84 + n * rowH;
      bool sel = (ai == s.addons.selected);
      FillRect(rgba, 12, y, UI_W - 24, rowH - 4, sel ? 20 : 42, sel ? 95 : 48, sel ? 130 : 62, 255);
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
        FillRect(rgba, 66, y + 12, 44, 20, 30, 120, 60, 255);
      else
        FillRect(rgba, 66, y + 12, 44, 20, 120, 30, 40, 255);
      DrawText(rgba, 72, y + 16, a.enabled ? "ON" : "OFF", 255, 255, 255, 1);
      char line[96];
      snprintf(line, sizeof(line), "%.52s", a.title.c_str());
      DrawText(rgba, 120, y + 10, line, 255, 240, 244, 1);
      snprintf(line, sizeof(line), "%s  %s", a.kind.c_str(), a.id.c_str());
      DrawText(rgba, 120, y + 28, line, 120, 135, 155, 1);
    }

    // Page buttons
    FillRect(rgba, 16, UI_H - 48, 100, 32, 40, 85, 115, 255);
    DrawText(rgba, 36, UI_H - 38, "PREV", 255, 240, 244, 2);
    FillRect(rgba, UI_W - 130, UI_H - 48, 110, 32, 40, 85, 115, 255);
    DrawText(rgba, UI_W - 110, UI_H - 38, "NEXT", 255, 240, 244, 2);
    DrawText(rgba, 140, UI_H - 36, s.addons.status.c_str(), 150, 165, 185, 1);
    DrawText(rgba, 140, UI_H - 20, "TRIGGER = TOGGLE MOUNT  ·  CLOSE = EXIT", 120, 135, 155, 1);

    if ((cursor && cursor->visible) || s.cursorVisible) {
      int cx = cursor ? cursor->x : s.cursorX;
      int cy = cursor ? cursor->y : s.cursorY;
      FillRect(rgba, cx - 8, cy - 2, 16, 4, 0, 220, 255, 255);
      FillRect(rgba, cx - 2, cy - 8, 4, 16, 0, 220, 255, 255);
    }
    return;
  }

  // --- Settings page (GMod native graphics + engine) ---
  if (s.page == WebUIPage::Settings) {
    FillRect(rgba, 8, 52, UI_W - 16, UI_H - 60, 30, 34, 46, 255);
    DrawText(rgba, 20, 58, "SOURCE + OPENXR  ·  TRIGGER CYCLES  ·  XR SS NEEDS VR RESTART", 150, 165, 185, 1);
    char line[96];
    const int visible = 12;
    const int rowH = 32;
    int start = std::max(0, std::min(s.settingsScroll, SR_COUNT - 1));
    for (int n = 0; n < visible; ++n) {
      int i = start + n;
      if (i >= SR_COUNT) break;
      int y = 72 + n * rowH;
      bool foc = (i == s.settingsRow);
      if (foc) FillRect(rgba, 16, y, UI_W - 32, rowH - 4, 20, 95, 130, 255);
      else if (n % 2) FillRect(rgba, 16, y, UI_W - 32, rowH - 4, 38, 42, 56, 255);
      FormatSettingRow(s, i, line, sizeof(line));
      DrawText(rgba, 28, y + 8, line, 255, 240, 244, 1);
    }
    snprintf(line, sizeof(line), "APPLIED ON START VIA gvrmod_cube.cfg + -w/-h");
    DrawText(rgba, 20, UI_H - 48, line, 120, 135, 155, 1);
    DrawText(rgba, 20, UI_H - 28, s.status.c_str(), 150, 165, 185, 1);
    if ((cursor && cursor->visible) || s.cursorVisible) {
      int cx = cursor ? cursor->x : s.cursorX;
      int cy = cursor ? cursor->y : s.cursorY;
      FillRect(rgba, cx - 10, cy - 2, 20, 4, 0, 220, 255, 255);
      FillRect(rgba, cx - 2, cy - 10, 4, 20, 0, 220, 255, 255);
    }
    return;
  }

  // --- New Game page (3-col) ---
  const int catW = 180, mapX = 188, mapW = 500, setX = 700, setW = 250;
  FillRect(rgba, 8, 52, catW, UI_H - 60, 30, 34, 46, 255);
  FillRect(rgba, mapX, 52, mapW, UI_H - 60, 38, 42, 56, 255);
  FillRect(rgba, setX, 52, setW, UI_H - 60, 30, 34, 46, 255);

  DrawText(rgba, 16, 60, "CATEGORY", 150, 165, 185, 1);
  for (int i = 0; i < (int)s.categories.size() && i < 12; ++i) {
    bool sel = (i == s.catIndex);
    bool foc = (s.focusCol == 0 && sel);
    int y = 80 + i * 28;
    if (sel) FillRect(rgba, 12, y - 2, catW - 8, 24, foc ? 30 : 50, foc ? 120 : 70, foc ? 160 : 95, 255);
    DrawText(rgba, 18, y + 2, s.categories[i].name.c_str(), 255, 240, 244, 1);
  }

  DrawText(rgba, mapX + 8, 60, "MAPS", 150, 165, 185, 1);
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
      if (sel) FillRect(rgba, mapX + 4, y - 2, mapW - 8, 24, foc ? 30 : 50, foc ? 120 : 70, foc ? 160 : 95, 255);
      DrawText(rgba, mapX + 12, y + 2, maps[i].c_str(), 255, 240, 244, 1);
    }
  }

  DrawText(rgba, setX + 8, 60, "SERVER", 150, 165, 185, 1);
  char line[96];
  auto row = [&](int idx, const char* label) {
    int y = 84 + idx * 36;
    bool foc = (s.focusCol == 2 && s.settingsRow == idx);
    if (foc) FillRect(rgba, setX + 4, y - 4, setW - 8, 30, 24, 100, 140, 255);
    DrawText(rgba, setX + 12, y, label, 255, 240, 244, 1);
  };
  snprintf(line, sizeof(line), "PLAYERS %d", WebUI_MaxPlayers(s));
  row(0, line);
  row(1, s.svLan ? "LAN SERVER ON" : "LAN SERVER OFF");
  row(2, s.p2p ? "P2P ON" : "P2P OFF");
  row(3, s.p2pFriends ? "P2P FRIENDS ON" : "P2P FRIENDS OFF");

  // Graphics + OpenXR summary → Settings tab
  FillRect(rgba, setX + 8, 236, setW - 16, 56, 48, 56, 72, 255);
  snprintf(line, sizeof(line), "GFX %s", PresetLabel(s.gfx.preset));
  DrawText(rgba, setX + 16, 244, line, 200, 230, 245, 1);
  float ss = kSs[std::clamp(s.gfx.xr.ssIdx, 0, kSsN - 1)];
  snprintf(line, sizeof(line), "XR SS %.2f  AA%d", ss, s.gfx.matAntialias);
  DrawText(rgba, setX + 16, 266, line, 150, 165, 185, 1);

  int by = UI_H - 70;
  bool startFoc = (s.focusCol == 3);
  FillRect(rgba, setX + 12, by, setW - 24, 44, startFoc ? 0 : 0, startFoc ? 200 : 160, startFoc ? 140 : 100, 255);
  DrawText(rgba, setX + 36, by + 14, "START GAME", 255, 255, 255, 2);

  DrawText(rgba, 12, UI_H - 22, s.status.c_str(), 150, 165, 185, 1);
  char sel[128];
  snprintf(sel, sizeof(sel), "SEL %s  | SETTINGS TAB = GRAPHICS", WebUI_SelectedMap(s).c_str());
  DrawText(rgba, mapX + 8, UI_H - 22, sel, 0, 220, 255, 1);

  if ((cursor && cursor->visible) || s.cursorVisible) {
    int cx = cursor ? cursor->x : s.cursorX;
    int cy = cursor ? cursor->y : s.cursorY;
    FillRect(rgba, cx - 10, cy - 2, 20, 4, 0, 220, 255, 255);
    FillRect(rgba, cx - 2, cy - 10, 4, 20, 0, 220, 255, 255);
    FillRect(rgba, cx - 3, cy - 3, 6, 6, 255, 255, 255, 255);
  }
}
