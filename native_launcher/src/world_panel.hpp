#pragma once
#include "math3d.hpp"
#include <openxr/openxr.h>

// World-locked floating panel in LOCAL space (frozen orientation after seed/place).
struct WorldPanel {
  Vec3 c{0, 0, -1.05f};
  Vec3 right{1, 0, 0};
  Vec3 up{0, 1, 0};
  Vec3 normal{0, 0, 1};
  bool ready = false;
};

WorldPanel& WorldPanelState();
void WorldPanelReset();

// Place panel in front of head; orientation freezes until re-seed or grab-release reface.
void WorldPanelSeed(const XrPosef& headLocal);

// After grab: re-face head once (still world-fixed position).
void WorldPanelReface(const XrPosef& headLocal);

// Ray vs panel plane in LOCAL → UI pixels (UI_W x UI_H).
bool WorldPanelRayHit(Vec3 origin, Vec3 dir, int* outPx, int* outPy, Vec3* outHit);
