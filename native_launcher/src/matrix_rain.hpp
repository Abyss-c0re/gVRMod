// Pure C++ matrix rain — handoff reality blend (no GL, offline-testable).
// Used by Cube on take_xr: passthrough + panel dim → rain density → black/GMod.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

#ifndef MATRIX_RAIN_MAX_COLS
#define MATRIX_RAIN_MAX_COLS 64
#endif
#ifndef MATRIX_RAIN_MAX_TRAIL
#define MATRIX_RAIN_MAX_TRAIL 14
#endif

struct MatrixRainCol {
  float x;       // NDC-ish 0..1 across view
  float head;    // 0..1+ head vertical (grows downward in screen space)
  float speed;   // units per second
  float bright;  // 0..1 head brightness
  int trail;     // length in cells
  uint32_t seed;
};

struct MatrixRainState {
  MatrixRainCol cols[MATRIX_RAIN_MAX_COLS];
  int n = 0;
  float time = 0.f;
  bool inited = false;
};

// Deterministic hash for pure tests
inline uint32_t MatrixRain_Hash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

inline float MatrixRain_U01(uint32_t& s) {
  s = MatrixRain_Hash(s + 0x9e3779b9u);
  return (s & 0x00ffffffu) / float(0x01000000u);
}

// Init columns. seed any; nCols clamped.
inline void MatrixRain_Init(MatrixRainState* st, int nCols, uint32_t seed) {
  if (!st) return;
  std::memset(st, 0, sizeof(*st));
  if (nCols < 4) nCols = 4;
  if (nCols > MATRIX_RAIN_MAX_COLS) nCols = MATRIX_RAIN_MAX_COLS;
  st->n = nCols;
  st->time = 0.f;
  st->inited = true;
  uint32_t s = seed ? seed : 0xC0BEu;
  for (int i = 0; i < nCols; ++i) {
    MatrixRainCol& c = st->cols[i];
    c.x = (i + 0.5f) / float(nCols);
    c.head = MatrixRain_U01(s) * 1.2f - 0.2f;
    c.speed = 0.35f + MatrixRain_U01(s) * 0.85f;
    c.bright = 0.55f + MatrixRain_U01(s) * 0.45f;
    c.trail = 6 + int(MatrixRain_U01(s) * (MATRIX_RAIN_MAX_TRAIL - 6));
    c.seed = s;
  }
}

// Advance simulation. dt seconds.
inline void MatrixRain_Tick(MatrixRainState* st, float dt) {
  if (!st || !st->inited) return;
  if (dt < 0.f) dt = 0.f;
  if (dt > 0.1f) dt = 0.1f;
  st->time += dt;
  for (int i = 0; i < st->n; ++i) {
    MatrixRainCol& c = st->cols[i];
    c.head += c.speed * dt;
    if (c.head > 1.15f + c.trail * 0.04f) {
      uint32_t s = c.seed;
      c.head = -MatrixRain_U01(s) * 0.4f;
      c.speed = 0.35f + MatrixRain_U01(s) * 0.85f;
      c.bright = 0.55f + MatrixRain_U01(s) * 0.45f;
      c.trail = 6 + int(MatrixRain_U01(s) * (MATRIX_RAIN_MAX_TRAIL - 6));
      c.seed = s;
    }
  }
}

// How much rain should show during handoff fade (0..1).
// Early fade: rain rises; late fade: rain + black; full black still ok.
inline float MatrixRain_DensityFromFade(float fade) {
  if (fade < 0.f) fade = 0.f;
  if (fade > 1.f) fade = 1.f;
  // Peak rain mid-handoff, soft in / soft out under black
  float rise = fade / 0.35f;
  if (rise > 1.f) rise = 1.f;
  float fall = (1.f - fade) / 0.25f;
  if (fall > 1.f) fall = 1.f;
  float d = rise < fall ? rise : fall;
  // Keep some rain until almost black
  if (fade > 0.15f && d < 0.35f) d = 0.35f + 0.65f * fade;
  if (d > 1.f) d = 1.f;
  return d;
}

// Sample column glyph alpha at vertical cell index (0=head, trail-1=tail).
inline float MatrixRain_CellAlpha(const MatrixRainCol& c, int cell) {
  if (cell < 0 || cell >= c.trail) return 0.f;
  float t = 1.f - float(cell) / float(c.trail > 1 ? c.trail : 1);
  float a = c.bright * t * t;
  if (cell == 0) a = c.bright; // bright head
  if (a < 0.f) a = 0.f;
  if (a > 1.f) a = 1.f;
  return a;
}

// Glyph “character” code 0..31 for pure tests / optional atlas
inline int MatrixRain_Glyph(const MatrixRainCol& c, int cell, float time) {
  uint32_t h = MatrixRain_Hash(c.seed ^ uint32_t(cell * 17) ^ uint32_t(time * 20.f));
  return int(h % 32u);
}

// Pure draw list: NDC points for GL layer (x,y in -1..1, alpha 0..1).
struct MatrixRainPoint {
  float x, y, a;
  int glyph;
};

inline int MatrixRain_BuildPoints(const MatrixRainState& st, float density,
                                  MatrixRainPoint* out, int maxOut) {
  if (!st.inited || !out || maxOut <= 0 || density < 0.001f) return 0;
  if (density > 1.f) density = 1.f;
  int n = 0;
  const float cellH = 2.f / 36.f; // ~36 rows of NDC
  for (int i = 0; i < st.n; ++i) {
    const MatrixRainCol& c = st.cols[i];
    float nx = c.x * 2.f - 1.f;
    for (int k = 0; k < c.trail; ++k) {
      float a = MatrixRain_CellAlpha(c, k) * density;
      if (a < 0.02f) continue;
      float y01 = c.head - k * 0.035f;
      float ny = 1.f - y01 * 2.f; // head near top when head small
      if (ny < -1.2f || ny > 1.2f) continue;
      if (n >= maxOut) return n;
      out[n].x = nx;
      out[n].y = ny;
      out[n].a = a;
      out[n].glyph = MatrixRain_Glyph(c, k, st.time);
      // slight vertical stack for trail body
      (void)cellH;
      ++n;
    }
  }
  return n;
}
