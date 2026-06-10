#pragma once

#include "core/vrmod_common.h"

// ── Global input state ──
extern vr::IVRSystem*          g_pSystem;
extern vr::IVRInput*           g_pInput;
extern vr::TrackedDevicePose_t g_poses[vr::k_unMaxTrackedDeviceCount];
extern actionSet               g_actionSets[MAX_ACTIONSETS];
extern int                     g_actionSetCount;
extern vr::VRActiveActionSet_t g_activeActionSets[MAX_ACTIONSETS];
extern int                     g_activeActionSetCount;
extern action                  g_actions[MAX_ACTIONS];
extern int                     g_actionCount;

// ── Pure conversion ──
PoseResult ConvertPose(const vr::TrackedDevicePose_t& pose);

// ── Action manifest parsing (file I/O only, no Lua) ──
// Returns number of actions parsed, fills actions[] and sets their handles via pInput.
int ParseActionManifest(const char* path, action* actions, int maxActions, vr::IVRInput* pInput);

// ── Action set management ──
// Finds or creates an action set entry, returns the index. Sets handle via pInput.
int FindOrCreateActionSet(const char* name, actionSet* sets, int* count, vr::IVRInput* pInput);

// ── Haptic lookup ──
vr::VRActionHandle_t FindActionHandleByName(const char* name, const action* actions, int count);
