#pragma once
#include "math3d.hpp"
#include <openxr/openxr.h>

// Anchored panel in a fixed reference space (STAGE preferred, else LOCAL).
// Orientation freezes at seed/re-seed — never tracks HMD after that.
struct WorldPanel {
  Vec3 c{0, 0, -1.05f};
  Vec3 right{1, 0, 0};
  Vec3 up{0, 1, 0};
  Vec3 normal{0, 0, 1};
  bool ready = false;
  // Debug: true if last seed used STAGE
  bool usedStage = false;
};

WorldPanel& WorldPanelState();
void WorldPanelReset();

// Place once in front of head; pose freezes in world space after this call.
void WorldPanelSeed(const XrPosef& headInWorld);

// Optional: re-orient to face head without moving center (MENU only — never per-frame).
void WorldPanelReface(const XrPosef& headInWorld);

// Translate center only (grab). Orientation stays frozen.
void WorldPanelTranslate(Vec3 delta);

bool WorldPanelRayHit(Vec3 origin, Vec3 dir, int* outPx, int* outPy, Vec3* outHit);
