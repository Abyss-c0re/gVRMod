#pragma once

#include "core/vrmod_common.h"
#include "rendering/openxr/xr_session.h"

// ── OpenXR action state ──
// Action space handles for pose actions
#define MAX_ACTION_SPACES 16

struct XrActionSpaceEntry {
    VRActionHandle vrActionHandle;  // XrAction cast to uint64_t
    XrSpace space;
    char name[MAX_STR_LEN];
};

extern XrActionSet       g_xrActionSets[MAX_ACTIONSETS];
extern int               g_xrActionSetCount;
extern XrActionSpaceEntry g_xrActionSpaces[MAX_ACTION_SPACES];
extern int               g_xrActionSpaceCount;
extern bool              g_xrActionsAttached;

// ── Pose conversion from OpenXR quaternion ──
PoseResult ConvertXrPose(const XrSpaceLocation& loc);

// HMD pose (from view space locate)
PoseResult GetHMDPose();

// ── Action manifest parsing ──
// Reads the existing SteamVR-format action manifest, creates OpenXR action sets + actions.
// Returns number of actions parsed, or negative on error.
int XR_ParseActionManifest(const char* path, action* actions, int maxActions);

// ── Action set management ──
int XR_FindOrCreateActionSet(const char* name, actionSet* sets, int* count);

// ── Sync actions (called per frame) ──
void XR_SyncActions(const actionSet* activeSets, int activeCount);

// ── Query action states ──
bool XR_GetBooleanAction(VRActionHandle handle, bool* changed);
float XR_GetFloatAction(VRActionHandle handle);
void XR_GetVector2Action(VRActionHandle handle, float* x, float* y);
PoseResult XR_GetPoseAction(VRActionHandle handle);

// ── Haptics ──
void XR_TriggerHaptic(VRActionHandle handle, float startSec, float durationSec,
                       float frequency, float amplitude);

// ── Haptic lookup ──
VRActionHandle XR_FindActionHandleByName(const char* name, const action* actions, int count);

// ── Attach action sets to session (must be done before first sync) ──
bool XR_AttachActionSets();

// ── Cleanup created action sets, actions and action spaces (for shutdown/restart) ──
void XR_CleanupActions();

// ── Update poses (HMD + action spaces) ──
void XR_UpdatePoses();

// Refresh HMD pose cache by locating views at current predicted time (call from Update path).
// Implementation is in xr_render to keep all OpenXR/GLX includes localized.
void XR_RefreshHMDPose();

// HMD pose result (updated by XR_UpdatePoses)
extern PoseResult g_xrHMDPose;