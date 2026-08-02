#pragma once
#include "math3d.hpp"
#include <openxr/openxr.h>

// Anchored panel in STAGE/LOCAL world space.
// After Seed(force=false) succeeds once, pose is FROZEN — Seed without force is a no-op.
// Never update from HMD each frame (that is head-follow heresy).

struct WorldPanel {
  Vec3 c{0, 0, -1.05f};
  Vec3 right{1, 0, 0};
  Vec3 up{0, 1, 0};
  Vec3 normal{0, 0, 1};
  bool ready = false;
  bool frozen = false; // true after first successful seed
  bool usedStage = false;
  int seedCount = 0; // debug: must stay 1 unless MENU re-place
};

WorldPanel& WorldPanelState();
void WorldPanelReset();

// Place in front of head in WORLD space, then freeze.
// force=false: only if not yet frozen (first anchor).
// force=true: MENU intentional re-place only.
// Returns true if pose was written.
bool WorldPanelSeed(const XrPosef& headInWorld, bool force = false);

// Translate center only (grab). Orientation stays frozen. No-op if not frozen.
void WorldPanelTranslate(Vec3 delta);

bool WorldPanelRayHit(Vec3 origin, Vec3 dir, int* outPx, int* outPy, Vec3* outHit);
