#pragma once
#include "math3d.hpp"
#include <openxr/openxr.h>

// Room-static panel: pose freezes in STAGE/LOCAL after seed.
// Only grab (translate) or MENU (re-place) may change pose.
// Never update from HMD each frame.

struct WorldPanel {
  // Frozen OpenXR pose in world space (+X right, +Y up, -Z faces user)
  XrPosef pose{};
  float widthM = 0.84f;
  float heightM = 0.48f;
  bool ready = false;
  bool frozen = false;
  int seedCount = 0;

  // Cached axes from pose (for hit / grab)
  Vec3 c{0, 0, -1.05f};
  Vec3 right{1, 0, 0};
  Vec3 up{0, 1, 0};
  Vec3 normal{0, 0, 1}; // toward user (-Z of pose)
};

WorldPanel& WorldPanelState();
void WorldPanelReset();

// Rebuild c/right/up/normal from pose (call after any pose write).
void WorldPanelSyncAxes();

// Place once in front of head (yaw-only). force=true only for MENU re-place.
// Returns false if frozen and !force.
bool WorldPanelSeed(const XrPosef& headInWorld, bool force = false);

// Grab: set world-space center, keep orientation. No re-face.
void WorldPanelSetCenter(Vec3 worldCenter);

// slopScale: 1 = exact panel, >1 expands hit box (soft click when tip looks near rim).
bool WorldPanelRayHit(Vec3 origin, Vec3 dir, int* outPx, int* outPy, Vec3* outHit,
                      float slopScale = 1.08f);

// Build XrPosef from center + facing normal (normal points toward user).
XrPosef WorldPanelMakePose(Vec3 center, Vec3 normalTowardUser);
