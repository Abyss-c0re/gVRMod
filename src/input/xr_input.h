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
// isActive (optional): true if OpenXR reports the action active, or if analog/chord
// synthesis forced a pressed state. Used by GetActions to avoid inactive driving-set
// actions clobbering main-set short names in the Lua table.
bool XR_GetBooleanAction(VRActionHandle handle, bool* changed, bool* isActive = nullptr);
float XR_GetFloatAction(VRActionHandle handle, bool* isActive = nullptr);
void XR_GetVector2Action(VRActionHandle handle, float* x, float* y, bool* isActive = nullptr);
PoseResult XR_GetPoseAction(VRActionHandle handle);

// ── Haptics ──
void XR_TriggerHaptic(VRActionHandle handle, float startSec, float durationSec,
                       float frequency, float amplitude);

// ── Haptic lookup ──
VRActionHandle XR_FindActionHandleByName(const char* name, const action* actions, int count);

// ── Attach action sets to session (must be done before first sync) ──
// Suggests interaction profile bindings (Oculus Touch / Index / KHR simple)
// then attaches. Call XR_SetActionCache first so bindings can resolve actions.
bool XR_AttachActionSets();

// Reset input SoT so a full VR restart can re-parse the manifest and re-attach.
void XR_ResetInputState();

// Ordered teardown helpers (see XR_Shutdown):
//   spaces while session live → destroy session → sets → clear handle cache.
void XR_DestroyActionSpacesOnly();
void XR_DestroyActionSetsOnly();
// Full cache clear; does not destroy attached sets while session still exists.
void XR_CleanupActions();

// Cache the parsed action table for binding suggestion + analog→boolean synthesis.
void XR_SetActionCache(const action* actions, int count);

// Create any missing pose action spaces (retry after session is available).
// Returns number of spaces newly created.
int XR_EnsureActionSpaces();

// ── Physical controller sources (for Lua binding UI / chords) ──
// Fixed hardware buttons/axes bound independently of logical VRMod actions so
// users can rebind without SteamVR. Values: bools as 0/1, analogs 0..1.
int XR_GetControllerSourceCount();
const char* XR_GetControllerSourceId(int index);     // e.g. "right_trigger"
const char* XR_GetControllerSourceLabel(int index);  // e.g. "Right Trigger"
bool XR_GetControllerSourceIsFloat(int index);
float XR_GetControllerSourceValue(int index);
// true if OpenXR reports the source action as active (bound + path live)
bool XR_GetControllerSourceIsActive(int index);

// OpenVR-compatible: IVRInput::GetSkeletalSummaryData
// eSummaryType: pass VRSummaryType_FromDevice (1) like module-master.
// action: VRMOD_SKELETON_HANDLE_LEFT / RIGHT (synthetic; no OpenXR skeleton actions).
// Fills flFingerCurl[5] + flFingerSplay[4] from controller trigger/grip/buttons.
bool XR_GetSkeletalSummaryData(VRActionHandle action, int eSummaryType,
                               VRSkeletalSummaryData_t* pSkeletalSummaryData);

// ── Update poses (HMD + action spaces) ──
void XR_UpdatePoses();

// Refresh HMD pose cache by locating views at current predicted time (call from Update path).
// Implementation is in xr_render to keep all OpenXR/GLX includes localized.
void XR_RefreshHMDPose();

// HMD pose result (updated by XR_UpdatePoses)
extern PoseResult g_xrHMDPose;