#include "xr_input.h"
#include "core/vrmod_log.h"

#include <cstring>
#include <cstdio>
#include <cmath>

// ── Global state ──
XrActionSet       g_xrActionSets[MAX_ACTIONSETS];
int               g_xrActionSetCount = 0;
// Short names parallel to g_xrActionSets (e.g. "base", "main", "driving")
static char       g_xrActionSetNames[MAX_ACTIONSETS][MAX_STR_LEN];
XrActionSpaceEntry g_xrActionSpaces[MAX_ACTION_SPACES];
int               g_xrActionSpaceCount = 0;
bool              g_xrActionsAttached = false;
PoseResult        g_xrHMDPose;

// Defined after internal action statics — must zero them or restart SuggestBindings
// feeds dead XrAction handles into the runtime (WiVRn SEGV on vrmod_exit→start).
static void XR_ClearInternalActionHandles();

void XR_DestroyActionSpacesOnly() {
    for (int i = 0; i < g_xrActionSpaceCount; i++) {
        if (g_xrActionSpaces[i].space != XR_NULL_HANDLE && g_xrDestroySpace) {
            g_xrDestroySpace(g_xrActionSpaces[i].space);
            g_xrActionSpaces[i].space = XR_NULL_HANDLE;
        }
    }
    g_xrActionSpaceCount = 0;
    memset(g_xrActionSpaces, 0, sizeof(g_xrActionSpaces));
}

// Call ONLY after session destroy (or when never attached). Spec: attached action
// sets must not be destroyed until the session is destroyed.
void XR_DestroyActionSetsOnly() {
    for (int i = 0; i < g_xrActionSetCount; i++) {
        if (g_xrActionSets[i] != XR_NULL_HANDLE && g_xrDestroyActionSet) {
            g_xrDestroyActionSet(g_xrActionSets[i]);
            g_xrActionSets[i] = XR_NULL_HANDLE;
        }
    }
    g_xrActionSetCount = 0;
    memset(g_xrActionSets, 0, sizeof(g_xrActionSets));
    memset(g_xrActionSetNames, 0, sizeof(g_xrActionSetNames));
    g_xrActionsAttached = false;
}

void XR_CleanupActions() {
    // Full clear of handle caches. Prefer ordered teardown via XR_Shutdown:
    // spaces → session → sets → instance. If session still exists, skip set
    // destroy (instance teardown owns them) but always null local caches.
    XR_DestroyActionSpacesOnly();
    if (g_xrSession == XR_NULL_HANDLE) {
        XR_DestroyActionSetsOnly();
    } else {
        // Session still live — do not xrDestroyActionSet (attached). Clear counts
        // only after caller destroys session; still wipe local set array so we
        // never pass stale handles into a new instance.
        g_xrActionSetCount = 0;
        memset(g_xrActionSets, 0, sizeof(g_xrActionSets));
        memset(g_xrActionSetNames, 0, sizeof(g_xrActionSetNames));
        g_xrActionsAttached = false;
    }
    XR_ClearInternalActionHandles();
    XR_SetActionCache(nullptr, 0);
    memset(&g_xrHMDPose, 0, sizeof(g_xrHMDPose));
    VRMOD_LOG_INFO("XR action state cleaned for restart");
}

void XR_ResetInputState() {
    XR_CleanupActions();
    VRMOD_LOG_INFO("OpenXR input state reset (ready for restart)");
}

// Map from our internal action index to XrAction handle
// We store XrAction as VRActionHandle (uint64_t)

// ── Quaternion to 3x3 rotation matrix (column vectors in the quat's basis) ──
static void QuatToRotMat(const XrQuaternionf& q, float m[3][3]) {
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    m[0][0] = 1.0f - 2.0f * (yy + zz);
    m[0][1] = 2.0f * (xy - wz);
    m[0][2] = 2.0f * (xz + wy);
    m[1][0] = 2.0f * (xy + wz);
    m[1][1] = 1.0f - 2.0f * (xx + zz);
    m[1][2] = 2.0f * (yz - wx);
    m[2][0] = 2.0f * (xz - wy);
    m[2][1] = 2.0f * (yz + wx);
    m[2][2] = 1.0f - 2.0f * (xx + yy);
}

// Source pitch/yaw/roll extract (same formulas as OpenVR ConvertPose / HmdMatrix34).
// HMD + eye poses use this on the *raw* OpenXR rotation matrix (fef9de7). Do NOT
// apply M*R*M^T here — that tilts the whole view ~90° while the compositor still
// uses native XR orientations.
static void ConvertRotToSourceAng(const float m[3][3], float ang[3]) {
    float s = m[1][2];
    if (s > 1.0f) s = 1.0f;
    if (s < -1.0f) s = -1.0f;
    ang[0] =  asinf(s) * (180.0f / PI_F);
    ang[1] =  atan2f(m[0][2], m[2][2]) * (180.0f / PI_F);
    ang[2] =  atan2f(-m[1][0], m[1][1]) * (180.0f / PI_F);
}

PoseResult ConvertXrPose(const XrSpaceLocation& loc) {
    PoseResult r;
    memset(&r, 0, sizeof(r));

    bool posValid = (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
    bool oriValid = (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0;
    r.valid = posValid && oriValid;

    if (!r.valid) return r;

    // OpenXR: x=right, y=up, z=back → Source: x=forward, y=left, z=up
    r.pos[0] = -loc.pose.position.z;
    r.pos[1] = -loc.pose.position.x;
    r.pos[2] =  loc.pose.position.y;

    // Same convert for HMD and controllers. Hand *meshes* add ValveBiped offsets
    // (e.g. right hand +180 roll) in Lua — do not invent a second grip rotation here
    // (v33 -90°X stacked with bone roll and twisted fingers/wrists).
    float Rxr[3][3];
    QuatToRotMat(loc.pose.orientation, Rxr);
    ConvertRotToSourceAng(Rxr, r.ang);

    // Velocity on XrSpaceVelocity chain only (not locationFlags).
    const XrSpaceVelocity* vel = (const XrSpaceVelocity*)loc.next;
    if (vel && vel->type == XR_TYPE_SPACE_VELOCITY) {
        if (vel->velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
            r.vel[0] = -vel->linearVelocity.z;
            r.vel[1] = -vel->linearVelocity.x;
            r.vel[2] =  vel->linearVelocity.y;
        }
        if (vel->velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) {
            r.angvel[0] = -vel->angularVelocity.z * (180.0f / PI_F);
            r.angvel[1] = -vel->angularVelocity.x * (180.0f / PI_F);
            r.angvel[2] =  vel->angularVelocity.y * (180.0f / PI_F);
        }
    }

    return r;
}

PoseResult GetHMDPose() {
    // Return the last set HMD pose (populated by submit layer locate or Update path).
    // Do not zero/clobber on transient !running checks -- that was causing the game
    // to never see valid poses even after successful submits/layer locates.
    PoseResult r = g_xrHMDPose;

    // Diagnostic logging for head tracking (throttled).
    static int s_hmdPoseLog = 0;
    if ((++s_hmdPoseLog % 90) == 0) {
        VRMOD_LOG_INFO("HMD pose: valid=%d pos(%.3f,%.3f,%.3f) ang(%.1f,%.1f,%.1f)",
            (int)r.valid, r.pos[0], r.pos[1], r.pos[2], r.ang[0], r.ang[1], r.ang[2]);
    }

    return r;
}

// Also log the layer view poses occasionally from submit path for comparison
void LogViewPosesForDebug(const XrView views[2]) {
    static int s_viewLog = 0;
    if ((++s_viewLog % 90) == 0) {
        VRMOD_LOG_INFO("Layer view0: pos(%.3f,%.3f,%.3f)  view1: pos(%.3f,%.3f,%.3f)",
            views[0].pose.position.x, views[0].pose.position.y, views[0].pose.position.z,
            views[1].pose.position.x, views[1].pose.position.y, views[1].pose.position.z);
    }
}

// ── Action manifest parsing ──
// The existing manifest is SteamVR JSON format:
// { "actions": [ { "name": "/actions/set/in/name", "type": "boolean" }, ... ] }
// We parse this and create OpenXR action sets + actions.

int XR_ParseActionManifest(const char* path, action* actions, int maxActions) {
    VRMOD_LOG_INFO("XR_ParseActionManifest: %s", path);

    FILE* file = fopen(path, "r");
    if (!file) {
        VRMOD_LOG_ERROR("Failed to open action manifest: %s", path);
        return -2;
    }

    memset(actions, 0, sizeof(action) * maxActions);
    int count = 0;

    char word[MAX_STR_LEN];
    char fmt1[MAX_STR_LEN], fmt2[MAX_STR_LEN];
    snprintf(fmt1, MAX_STR_LEN, "%%*[^\"]\"%%%i[^\"]\"", MAX_STR_LEN - 1);
    snprintf(fmt2, MAX_STR_LEN, "%%%i[^\"]\"", MAX_STR_LEN - 1);

    // Find "actions" array
    while (fscanf(file, fmt1, word) == 1 && strcmp(word, "actions") != 0)
        ;

    // Parse actions
    while (fscanf(file, fmt2, word) == 1) {
        if (strchr(word, ']') != nullptr)
            break;
        if (strcmp(word, "name") == 0) {
            if (fscanf(file, fmt1, actions[count].fullname) != 1)
                break;
            actions[count].name = actions[count].fullname;
            for (unsigned int i = 0; i < strlen(actions[count].fullname); i++) {
                if (actions[count].fullname[i] == '/')
                    actions[count].name = actions[count].fullname + i + 1;
            }
        }
        if (strcmp(word, "type") == 0) {
            char typeStr[MAX_STR_LEN] = {0};
            if (fscanf(file, fmt1, typeStr) != 1)
                break;
            for (int i = 0; typeStr[i]; i++)
                actions[count].type += typeStr[i];
        }
        if (actions[count].fullname[0] && actions[count].type) {
            count++;
            if (count == maxActions)
                break;
        }
    }
    fclose(file);

    VRMOD_LOG_INFO("Parsed %d actions from manifest", count);

    // Now create OpenXR action sets and actions
    // First pass: identify unique action sets from the action paths
    // Action path format: /actions/<set>/in/<name>
    g_xrActionSetCount = 0;

    for (int i = 0; i < count; i++) {
        // Extract action set name from fullname (e.g., "/actions/main" from "/actions/main/in/foo")
        char setPath[MAX_STR_LEN] = {0};
        // Find the third '/' to get the set path
        int slashCount = 0;
        int setEnd = 0;
        for (int j = 0; actions[i].fullname[j] && j < MAX_STR_LEN - 1; j++) {
            if (actions[i].fullname[j] == '/') slashCount++;
            if (slashCount == 3) { setEnd = j; break; }
        }
        if (setEnd == 0) continue;
        strncpy(setPath, actions[i].fullname, setEnd);

        // Find or create this action set
        int setIdx = -1;
        for (int j = 0; j < g_xrActionSetCount; j++) {
            char existingPath[MAX_STR_LEN];
            snprintf(existingPath, MAX_STR_LEN, "/actions/%s",
                ((char*)&g_xrActionSets[j]) + sizeof(XrActionSet)); // name is stored after handle
            // We'll use a different approach - store action set names separately
        }

        // Use the existing actionSet array for name storage
        // We need to find or create the XrActionSet
        // For now, extract just the set name part (e.g., "main" from "/actions/main")
        char setName[MAX_STR_LEN] = {0};
        // setPath is like "/actions/main"
        const char* lastSlash = strrchr(setPath, '/');
        if (lastSlash) {
            strncpy(setName, lastSlash + 1, MAX_STR_LEN - 1);
        }

        // Check if we already have this set
        setIdx = -1;
        for (int j = 0; j < g_xrActionSetCount; j++) {
            // Compare the stored set name
            // We reuse the XrActionSetCreateInfo localized name field concept
            // The XrActionSet handle was stored, but we need name matching
            // Use a separate lookup
        }

        // Actually, let's use the actionSet struct from vrmod_common for name storage
        // and create XrActionSets in a parallel array
    }

    // Simpler approach: create one XrActionSet per unique set path,
    // then create XrActions within them.

    // Reset
    g_xrActionSetCount = 0;
    memset(g_xrActionSets, 0, sizeof(g_xrActionSets));

    // Track set names alongside XrActionSets
    struct SetInfo {
        char path[MAX_STR_LEN];     // e.g., "/actions/main"
        char xrName[64];            // e.g., "main" (OpenXR name, max 64 chars)
        XrActionSet handle;
    };
    SetInfo setInfos[MAX_ACTIONSETS];
    int setInfoCount = 0;

    auto findOrCreateSet = [&](const char* setPath, const char* setName) -> int {
        for (int j = 0; j < setInfoCount; j++) {
            if (strcmp(setInfos[j].path, setPath) == 0) return j;
        }
        if (setInfoCount >= MAX_ACTIONSETS) return -1;

        XrActionSetCreateInfo asci = {XR_TYPE_ACTION_SET_CREATE_INFO};
        strncpy(asci.actionSetName, setName, XR_MAX_ACTION_SET_NAME_SIZE - 1);
        strncpy(asci.localizedActionSetName, setName, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
        // OpenXR requires lowercase alphanumeric + dashes/dots/underscores
        for (int k = 0; asci.actionSetName[k]; k++) {
            if (asci.actionSetName[k] >= 'A' && asci.actionSetName[k] <= 'Z')
                asci.actionSetName[k] += ('a' - 'A');
        }
        asci.priority = 0;

        XrResult res = g_xrCreateActionSet(g_xrInstance, &asci, &setInfos[setInfoCount].handle);
        if (res != XR_SUCCESS) {
            VRMOD_LOG_ERROR("xrCreateActionSet '%s' failed: %s", setName, XR_ResultToString(res));
            return -1;
        }
        strncpy(setInfos[setInfoCount].path, setPath, MAX_STR_LEN - 1);
        strncpy(setInfos[setInfoCount].xrName, setName, 63);
        g_xrActionSets[setInfoCount] = setInfos[setInfoCount].handle;
        strncpy(g_xrActionSetNames[setInfoCount], setName, MAX_STR_LEN - 1);
        VRMOD_LOG_INFO("Created action set '%s' (path=%s)", setName, setPath);

        int idx = setInfoCount;
        setInfoCount++;
        g_xrActionSetCount = setInfoCount;
        return idx;
    };

    // Create all action sets and actions
    for (int i = 0; i < count; i++) {
        // Parse set path and name
        char setPath[MAX_STR_LEN] = {0};
        char setName[MAX_STR_LEN] = {0};
        int slashCount = 0;
        int setEnd = 0;
        for (int j = 0; actions[i].fullname[j] && j < MAX_STR_LEN - 1; j++) {
            if (actions[i].fullname[j] == '/') slashCount++;
            if (slashCount == 3) { setEnd = j; break; }
        }
        if (setEnd == 0) {
            VRMOD_LOG_WARN("Skipping action with malformed path: %s", actions[i].fullname);
            continue;
        }
        strncpy(setPath, actions[i].fullname, setEnd);

        const char* lastSlash = strrchr(setPath, '/');
        if (lastSlash) strncpy(setName, lastSlash + 1, MAX_STR_LEN - 1);

        int setIdx = findOrCreateSet(setPath, setName);
        if (setIdx < 0) continue;

        // Determine XrActionType
        XrActionType xrType;
        bool isPose = false;
        bool isVibration = false;
        switch (actions[i].type) {
            case ActionType_Boolean:    xrType = XR_ACTION_TYPE_BOOLEAN_INPUT; break;
            case ActionType_Vector1:    xrType = XR_ACTION_TYPE_FLOAT_INPUT; break;
            case ActionType_Vector2:    xrType = XR_ACTION_TYPE_VECTOR2F_INPUT; break;
            case ActionType_Pose:       xrType = XR_ACTION_TYPE_POSE_INPUT; isPose = true; break;
            case ActionType_Skeleton: {
                // OpenVR: GetActionHandle + GetSkeletalSummaryData.
                // OpenXR: no skeleton actions — keep Lua SoT with synthetic handles
                // so GetActions can call XR_GetSkeletalSummaryData like OpenVR.
                const char* sn = actions[i].name ? actions[i].name : "";
                const char* full = actions[i].fullname;
                bool left = (strstr(sn, "left") != nullptr) || (strstr(full, "left") != nullptr)
                         || (strstr(sn, "Left") != nullptr) || (strstr(full, "Left") != nullptr);
                if ((strstr(sn, "right") != nullptr) || (strstr(full, "right") != nullptr)
                    || (strstr(sn, "Right") != nullptr) || (strstr(full, "Right") != nullptr))
                    left = false;
                actions[i].handle = left ? VRMOD_SKELETON_HANDLE_LEFT : VRMOD_SKELETON_HANDLE_RIGHT;
                VRMOD_LOG_INFO("Skeleton action '%s' → synthetic handle %llu (OpenXR summary synthesis)",
                    actions[i].name, (unsigned long long)actions[i].handle);
                continue; // no xrCreateAction
            }
            case ActionType_Vibration:  xrType = XR_ACTION_TYPE_VIBRATION_OUTPUT; isVibration = true; break;
            default:                    continue;
        }

        // Create action name from the short name
        XrActionCreateInfo aci = {XR_TYPE_ACTION_CREATE_INFO};
        // Make a valid OpenXR name (lowercase, alphanumeric, underscore, dash, dot)
        char xrActionName[XR_MAX_ACTION_NAME_SIZE] = {0};
        strncpy(xrActionName, actions[i].name, XR_MAX_ACTION_NAME_SIZE - 1);
        for (int k = 0; xrActionName[k]; k++) {
            char c = xrActionName[k];
            if (c >= 'A' && c <= 'Z') xrActionName[k] = c + ('a' - 'A');
            else if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                       c == '_' || c == '-' || c == '.'))
                xrActionName[k] = '_';
        }
        strncpy(aci.actionName, xrActionName, XR_MAX_ACTION_NAME_SIZE - 1);
        strncpy(aci.localizedActionName, actions[i].name, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
        aci.actionType = xrType;

        XrAction xrAction = XR_NULL_HANDLE;
        XrResult res = g_xrCreateAction(setInfos[setIdx].handle, &aci, &xrAction);
        if (res != XR_SUCCESS) {
            VRMOD_LOG_WARN("xrCreateAction '%s' failed: %s (skipping)", actions[i].name,
                XR_ResultToString(res));
            continue;
        }

        // Store the XrAction handle
        actions[i].handle = (VRActionHandle)(uintptr_t)xrAction;

        // Create action space for pose actions (requires a live session).
        // If session is not ready yet we still keep the action handle; XR_EnsureActionSpaces
        // will retry after ShareTextureBegin / Attach.
        if (isPose && g_xrActionSpaceCount < MAX_ACTION_SPACES) {
            if (g_xrSession == XR_NULL_HANDLE) {
                VRMOD_LOG_WARN("Deferring action space for '%s' (no session yet)", actions[i].name);
            } else {
                XrActionSpaceCreateInfo asci = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
                asci.action = xrAction;
                asci.poseInActionSpace = {{0, 0, 0, 1}, {0, 0, 0}};

                XrSpace space = XR_NULL_HANDLE;
                res = g_xrCreateActionSpace(g_xrSession, &asci, &space);
                if (res == XR_SUCCESS) {
                    g_xrActionSpaces[g_xrActionSpaceCount].vrActionHandle = actions[i].handle;
                    g_xrActionSpaces[g_xrActionSpaceCount].space = space;
                    strncpy(g_xrActionSpaces[g_xrActionSpaceCount].name, actions[i].name, MAX_STR_LEN - 1);
                    g_xrActionSpaceCount++;
                    VRMOD_LOG_INFO("Created action space for '%s'", actions[i].name);
                } else {
                    VRMOD_LOG_WARN("xrCreateActionSpace '%s' failed: %s",
                        actions[i].name, XR_ResultToString(res));
                }
            }
        }

        VRMOD_LOG_INFO("Created action '%s' type=%d in set '%s'", actions[i].name,
            actions[i].type, setName);
    }

    VRMOD_LOG_INFO("XR_ParseActionManifest complete: %d actions, %d sets, %d spaces",
        count, g_xrActionSetCount, g_xrActionSpaceCount);
    return count;
}

int XR_FindOrCreateActionSet(const char* name, actionSet* sets, int* count) {
    // Check if already exists
    for (int j = 0; j < *count; j++) {
        if (strcmp(name, sets[j].name) == 0)
            return j;
    }

    // Extract short set name (last component of path like "/actions/main" → "main")
    char setName[MAX_STR_LEN] = {0};
    const char* lastSlash = strrchr(name, '/');
    if (lastSlash) strncpy(setName, lastSlash + 1, MAX_STR_LEN - 1);
    else strncpy(setName, name, MAX_STR_LEN - 1);

    int idx = *count;
    if (idx >= MAX_ACTIONSETS) {
        VRMOD_LOG_WARN("Too many action sets registered");
        return 0;
    }
    strncpy(sets[idx].name, name, MAX_STR_LEN - 1);
    sets[idx].handle = 0;

    // Match by short name recorded at parse time
    for (int j = 0; j < g_xrActionSetCount; j++) {
        if (strcmp(g_xrActionSetNames[j], setName) == 0) {
            sets[idx].handle = (VRActionSetHandle)(uintptr_t)g_xrActionSets[j];
            break;
        }
    }
    // Fallback: index-aligned (parse order base, main, driving)
    if (sets[idx].handle == 0 && idx < g_xrActionSetCount) {
        sets[idx].handle = (VRActionSetHandle)(uintptr_t)g_xrActionSets[idx];
    }

    (*count)++;
    VRMOD_LOG_INFO("Registered active action set '%s' at index %d handle=%llu",
        name, idx, (unsigned long long)sets[idx].handle);
    return idx;
}

// ── Interaction profile bindings ──
// OpenXR requires explicit xrSuggestInteractionProfileBindings before attach.
// Without this, every action stays inactive and controllers appear completely dead.
//
// SteamVR binding JSON is not consumed by OpenXR; we mirror the oculus_touch
// layout from vrmod_bindings_oculus_touch.txt onto standard OpenXR paths.
//
// Note: Oculus Touch has trigger/value (float) but no trigger/click (bool).
// Boolean fire actions are synthesized from float threshold in GetBooleanAction
// via companion float actions (vector1_primaryfire + internal left trigger).

// Companion floats for analog→boolean threshold synthesis (Oculus has no trigger/click).
static XrAction g_xrLeftTriggerFloat  = XR_NULL_HANDLE;
static XrAction g_xrRightTriggerFloat = XR_NULL_HANDLE; // mirrors vector1_primaryfire when present
static XrAction g_xrLeftSqueezeFloat  = XR_NULL_HANDLE;
static XrAction g_xrRightSqueezeFloat = XR_NULL_HANDLE;
// Companion booleans for SteamVR-style chords (OpenXR has no chord API).
// Oculus Touch: dual thumbrest touch → boolean_reload (see vrmod oculus bindings).
static XrAction g_xrLeftThumbrestTouch  = XR_NULL_HANDLE;
static XrAction g_xrRightThumbrestTouch = XR_NULL_HANDLE;
// Base-set stick vector2s for synthetic dpad sources. Must live in /actions/base so
// they stay isActive while driving (main's walkdirection / smoothturn go inactive).
static XrAction g_xrLeftStickVec2  = XR_NULL_HANDLE;
static XrAction g_xrRightStickVec2 = XR_NULL_HANDLE;
static const float kTriggerClickThreshold = 0.55f;

// ── Physical controller sources for Lua rebinding UI ──
// These mirror hardware paths so Lua can map any button/chord → logical action
// without SteamVR. Logical OpenXR action bindings remain as defaults/fallback.
enum { kMaxCtrlSources = 32 };
// stickAxis: 0 = real OpenXR action; 1..4 = synthetic stick dpad from *stickVec
//   1=north(+y), 2=south(-y), 3=east(+x), 4=west(-x)
// stickVec: parent vector2 action (base-set internal sticks; always synced with base)
struct CtrlSource {
    const char* id;
    const char* label;
    bool isFloat;
    const char* pathTouch;  // Oculus / Meta Quest (null = not suggested; synthetic)
    const char* pathIndex;  // Valve Index (null = skip on Index suggest)
    XrAction* shared;       // if non-null, reuse this action handle instead of creating
    XrAction action;
    int stickAxis;          // 0 = hardware; 1..4 = dpad from *stickVec
    XrAction* stickVec;     // parent vector2 for synthetic dpad (null if hardware)
};
// shared pointers filled after internal actions exist
static XrAction s_srcLeftX = XR_NULL_HANDLE, s_srcLeftY = XR_NULL_HANDLE;
static XrAction s_srcLeftMenu = XR_NULL_HANDLE;
static XrAction s_srcRightA = XR_NULL_HANDLE, s_srcRightB = XR_NULL_HANDLE;
static XrAction s_srcLeftStickClick = XR_NULL_HANDLE, s_srcRightStickClick = XR_NULL_HANDLE;

static CtrlSource g_ctrlSources[kMaxCtrlSources] = {
    // id, label, isFloat, pathTouch, pathIndex, shared*, action, stickAxis, stickVec
    { "left_x",           "Left X",            false, "/user/hand/left/input/x/click",           "/user/hand/left/input/x/click",           &s_srcLeftX,           XR_NULL_HANDLE, 0, nullptr },
    { "left_y",           "Left Y",            false, "/user/hand/left/input/y/click",           "/user/hand/left/input/y/click",           &s_srcLeftY,           XR_NULL_HANDLE, 0, nullptr },
    { "left_menu",        "Left Menu",         false, "/user/hand/left/input/menu/click",        "/user/hand/left/input/a/touch",           &s_srcLeftMenu,        XR_NULL_HANDLE, 0, nullptr },
    { "left_stick_click", "Left Stick Click",  false, "/user/hand/left/input/thumbstick/click",  "/user/hand/left/input/thumbstick/click",  &s_srcLeftStickClick,  XR_NULL_HANDLE, 0, nullptr },
    { "left_thumbrest",   "Left Thumbrest",    false, "/user/hand/left/input/thumbrest/touch",   nullptr,                                   &g_xrLeftThumbrestTouch, XR_NULL_HANDLE, 0, nullptr },
    { "left_trigger",     "Left Trigger",      true,  "/user/hand/left/input/trigger/value",     "/user/hand/left/input/trigger/value",     &g_xrLeftTriggerFloat, XR_NULL_HANDLE, 0, nullptr },
    { "left_squeeze",     "Left Grip",         true,  "/user/hand/left/input/squeeze/value",     "/user/hand/left/input/squeeze/value",     &g_xrLeftSqueezeFloat, XR_NULL_HANDLE, 0, nullptr },
    { "right_a",          "Right A",           false, "/user/hand/right/input/a/click",          "/user/hand/right/input/a/click",          &s_srcRightA,          XR_NULL_HANDLE, 0, nullptr },
    { "right_b",          "Right B",           false, "/user/hand/right/input/b/click",          "/user/hand/right/input/b/click",          &s_srcRightB,          XR_NULL_HANDLE, 0, nullptr },
    { "right_stick_click","Right Stick Click", false, "/user/hand/right/input/thumbstick/click", "/user/hand/right/input/thumbstick/click", &s_srcRightStickClick, XR_NULL_HANDLE, 0, nullptr },
    { "right_thumbrest",  "Right Thumbrest",   false, "/user/hand/right/input/thumbrest/touch",  nullptr,                                   &g_xrRightThumbrestTouch, XR_NULL_HANDLE, 0, nullptr },
    { "right_trigger",    "Right Trigger",     true,  "/user/hand/right/input/trigger/value",    "/user/hand/right/input/trigger/value",    &g_xrRightTriggerFloat, XR_NULL_HANDLE, 0, nullptr },
    { "right_squeeze",    "Right Grip",        true,  "/user/hand/right/input/squeeze/value",    "/user/hand/right/input/squeeze/value",    &g_xrRightSqueezeFloat, XR_NULL_HANDLE, 0, nullptr },
    // Synthetic stick dpad from base-set sticks (works on foot AND while driving)
    { "left_stick_north",  "Left Stick Up",    false, nullptr, nullptr, nullptr, XR_NULL_HANDLE, 1, &g_xrLeftStickVec2 },
    { "left_stick_south",  "Left Stick Down",  false, nullptr, nullptr, nullptr, XR_NULL_HANDLE, 2, &g_xrLeftStickVec2 },
    { "left_stick_east",   "Left Stick Right", false, nullptr, nullptr, nullptr, XR_NULL_HANDLE, 3, &g_xrLeftStickVec2 },
    { "left_stick_west",   "Left Stick Left",  false, nullptr, nullptr, nullptr, XR_NULL_HANDLE, 4, &g_xrLeftStickVec2 },
    { "right_stick_north", "Right Stick Up",   false, nullptr, nullptr, nullptr, XR_NULL_HANDLE, 1, &g_xrRightStickVec2 },
    { "right_stick_south", "Right Stick Down", false, nullptr, nullptr, nullptr, XR_NULL_HANDLE, 2, &g_xrRightStickVec2 },
    { "right_stick_east",  "Right Stick Right",false, nullptr, nullptr, nullptr, XR_NULL_HANDLE, 3, &g_xrRightStickVec2 },
    { "right_stick_west",  "Right Stick Left", false, nullptr, nullptr, nullptr, XR_NULL_HANDLE, 4, &g_xrRightStickVec2 },
};
static const int g_ctrlSourceCount = 21;
static const float kStickDpadThreshold = 0.55f;

// Null every cached XrAction after instance/session teardown. Leaving non-null
// values makes XR_AttachActionSets skip CreateInternal* and pass dead handles
// into xrSuggestInteractionProfileBindings → SEGV in WiVRn/Monado on restart.
static void XR_ClearInternalActionHandles() {
    g_xrLeftTriggerFloat = XR_NULL_HANDLE;
    g_xrRightTriggerFloat = XR_NULL_HANDLE;
    g_xrLeftSqueezeFloat = XR_NULL_HANDLE;
    g_xrRightSqueezeFloat = XR_NULL_HANDLE;
    g_xrLeftThumbrestTouch = XR_NULL_HANDLE;
    g_xrRightThumbrestTouch = XR_NULL_HANDLE;
    g_xrLeftStickVec2 = XR_NULL_HANDLE;
    g_xrRightStickVec2 = XR_NULL_HANDLE;
    s_srcLeftX = XR_NULL_HANDLE;
    s_srcLeftY = XR_NULL_HANDLE;
    s_srcLeftMenu = XR_NULL_HANDLE;
    s_srcRightA = XR_NULL_HANDLE;
    s_srcRightB = XR_NULL_HANDLE;
    s_srcLeftStickClick = XR_NULL_HANDLE;
    s_srcRightStickClick = XR_NULL_HANDLE;
    for (int i = 0; i < g_ctrlSourceCount; i++) {
        g_ctrlSources[i].action = XR_NULL_HANDLE;
    }
}

// Cached action list pointer set by Attach (for name→handle lookups in GetBoolean).
static const action* g_xrCachedActions = nullptr;
static int           g_xrCachedActionCount = 0;

static XrAction FindXrActionByName(const char* name) {
    if (!g_xrCachedActions) return XR_NULL_HANDLE;
    for (int i = 0; i < g_xrCachedActionCount; i++) {
        if (g_xrCachedActions[i].name && strcmp(g_xrCachedActions[i].name, name) == 0)
            return (XrAction)(uintptr_t)g_xrCachedActions[i].handle;
    }
    return XR_NULL_HANDLE;
}

// Forward decl — defined below with PushBinding.
static bool PushBinding(XrActionSuggestedBinding* out, int* n, int max,
                        XrAction action, const char* pathStr);
// Bind every action with this short name (main + driving duplicates share hardware paths).
static void PushBindingAllByName(XrActionSuggestedBinding* out, int* n, int max,
                                 const char* shortName, const char* pathStr) {
    if (!g_xrCachedActions || !shortName || !pathStr) return;
    for (int i = 0; i < g_xrCachedActionCount; i++) {
        if (g_xrCachedActions[i].name && strcmp(g_xrCachedActions[i].name, shortName) == 0) {
            PushBinding(out, n, max,
                (XrAction)(uintptr_t)g_xrCachedActions[i].handle, pathStr);
        }
    }
}

static bool SuggestBindingsForProfile(const char* profilePathStr,
                                      XrActionSuggestedBinding* bindings, uint32_t count) {
    if (!g_xrSuggestInteractionProfileBindings || !g_xrStringToPath) return false;
    if (count == 0) return true;

    XrPath profilePath = XR_NULL_PATH;
    XrResult res = g_xrStringToPath(g_xrInstance, profilePathStr, &profilePath);
    if (res != XR_SUCCESS) {
        VRMOD_LOG_WARN("xrStringToPath(%s) failed: %s", profilePathStr, XR_ResultToString(res));
        return false;
    }

    XrInteractionProfileSuggestedBinding spb = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    spb.interactionProfile = profilePath;
    spb.countSuggestedBindings = count;
    spb.suggestedBindings = bindings;

    res = g_xrSuggestInteractionProfileBindings(g_xrInstance, &spb);
    if (res != XR_SUCCESS) {
        VRMOD_LOG_WARN("xrSuggestInteractionProfileBindings(%s) failed: %s (%u bindings)",
            profilePathStr, XR_ResultToString(res), count);
        return false;
    }
    VRMOD_LOG_INFO("Suggested %u bindings for %s", count, profilePathStr);
    return true;
}

// Helper: resolve path + push binding if action is valid.
static bool PushBinding(XrActionSuggestedBinding* out, int* n, int max,
                        XrAction action, const char* pathStr) {
    if (action == XR_NULL_HANDLE || !pathStr || *n >= max) return false;
    XrPath path = XR_NULL_PATH;
    if (g_xrStringToPath(g_xrInstance, pathStr, &path) != XR_SUCCESS) return false;
    out[*n].action = action;
    out[*n].binding = path;
    (*n)++;
    return true;
}

// Bind all physical controller sources that have a path for the given profile kind.
// profileKind: 0 = touch/oculus, 1 = index
static void PushControllerSourceBindings(XrActionSuggestedBinding* binds, int* n, int max, int profileKind) {
    for (int i = 0; i < g_ctrlSourceCount; i++) {
        CtrlSource& s = g_ctrlSources[i];
        XrAction a = s.action;
        if (a == XR_NULL_HANDLE && s.shared) a = *s.shared;
        if (a == XR_NULL_HANDLE) continue;
        const char* path = (profileKind == 1) ? s.pathIndex : s.pathTouch;
        if (!path) continue;
        PushBinding(binds, n, max, a, path);
    }
}

static bool XR_SuggestAllInteractionBindings() {
    // Build a large suggested-binding table for Oculus Touch / Meta Quest
    // (primary target for WiVRn) and a smaller KHR simple_controller fallback.
    const int kMax = 96;
    XrActionSuggestedBinding binds[kMax];
    int n = 0;

    auto A = [](const char* name) -> XrAction { return FindXrActionByName(name); };

    // ── poses + haptics (base set) ──
    PushBinding(binds, &n, kMax, A("pose_lefthand"),  "/user/hand/left/input/grip/pose");
    PushBinding(binds, &n, kMax, A("pose_righthand"), "/user/hand/right/input/grip/pose");
    PushBinding(binds, &n, kMax, A("vibration_left"),  "/user/hand/left/output/haptic");
    PushBinding(binds, &n, kMax, A("vibration_right"), "/user/hand/right/output/haptic");

    // ── main + driving locomotion / combat (mirrors vrmod_bindings_oculus_touch.txt) ──
    // Manifest short names collide across main/driving (spawnmenu, reload, pickups, …).
    // PushBindingAllByName binds *every* XrAction with that short name so the active
    // action set always has a live binding (SteamVR scoped by full path; we cannot).
    // Logical bindings remain defaults; Lua binding UI remaps on top via sources.
    PushBinding(binds, &n, kMax, A("vector2_walkdirection"), "/user/hand/left/input/thumbstick");
    PushBinding(binds, &n, kMax, A("boolean_sprint"),        "/user/hand/left/input/thumbstick/click");
    PushBinding(binds, &n, kMax, A("vector2_smoothturn"),    "/user/hand/right/input/thumbstick");
    PushBindingAllByName(binds, &n, kMax, "boolean_changeweapon", "/user/hand/right/input/thumbstick/click");
    // Base-set sticks for dpad sources (always active with /actions/base)
    PushBinding(binds, &n, kMax, g_xrLeftStickVec2,  "/user/hand/left/input/thumbstick");
    PushBinding(binds, &n, kMax, g_xrRightStickVec2, "/user/hand/right/input/thumbstick");

    // Triggers (float). Boolean fire is synthesized via threshold in GetBooleanAction.
    // Prefer the manifest vector1_primaryfire for the right trigger when present.
    XrAction rightTrig = A("vector1_primaryfire");
    if (rightTrig == XR_NULL_HANDLE) rightTrig = g_xrRightTriggerFloat;
    else g_xrRightTriggerFloat = rightTrig; // so threshold synthesis reads the same action
    PushBinding(binds, &n, kMax, rightTrig, "/user/hand/right/input/trigger/value");
    PushBinding(binds, &n, kMax, g_xrLeftTriggerFloat, "/user/hand/left/input/trigger/value");

    // Squeeze (float) → pickup threshold synthesis
    PushBinding(binds, &n, kMax, g_xrLeftSqueezeFloat,  "/user/hand/left/input/squeeze/value");
    PushBinding(binds, &n, kMax, g_xrRightSqueezeFloat, "/user/hand/right/input/squeeze/value");

    // Face buttons — bind all short-name duplicates (main + driving)
    PushBinding(binds, &n, kMax, A("boolean_jump"),      "/user/hand/right/input/b/click");
    PushBinding(binds, &n, kMax, A("boolean_crouch"),    "/user/hand/right/input/a/click");
    PushBindingAllByName(binds, &n, kMax, "boolean_spawnmenu", "/user/hand/left/input/y/click");
    PushBinding(binds, &n, kMax, A("boolean_use"),       "/user/hand/left/input/x/click");
    PushBinding(binds, &n, kMax, A("boolean_flashlight"), "/user/hand/left/input/menu/click");
    // Pickups / reload: no direct path (float squeeze + dual-thumbrest chord synthesis)

    // Driving-unique actions from the action manifest / Quest 3 scheme
    PushBinding(binds, &n, kMax, A("vector1_forward"),   "/user/hand/right/input/trigger/value");
    PushBinding(binds, &n, kMax, A("vector1_reverse"),   "/user/hand/left/input/trigger/value");
    PushBinding(binds, &n, kMax, A("vector2_steer"),     "/user/hand/left/input/thumbstick");
    PushBinding(binds, &n, kMax, A("boolean_handbrake"), "/user/hand/right/input/a/click");
    PushBinding(binds, &n, kMax, A("boolean_turbo"),     "/user/hand/right/input/b/click");
    PushBinding(binds, &n, kMax, A("boolean_exit"),      "/user/hand/left/input/x/click");
    PushBinding(binds, &n, kMax, A("boolean_turret"),    "/user/hand/right/input/thumbstick/click");
    PushBinding(binds, &n, kMax, A("boolean_horn"),      "/user/hand/left/input/thumbstick/click");

    // Physical sources for Lua rebinding (Index paths; no thumbrest).
    PushControllerSourceBindings(binds, &n, kMax, /*index*/1);
    const int nIndex = n;
    bool okIndex = SuggestBindingsForProfile(
        "/interaction_profiles/valve/index_controller", binds, (uint32_t)nIndex);

    // Oculus / Quest: rebuild full Touch table (includes thumbrest sources).
    n = 0;
    PushBinding(binds, &n, kMax, A("pose_lefthand"),  "/user/hand/left/input/grip/pose");
    PushBinding(binds, &n, kMax, A("pose_righthand"), "/user/hand/right/input/grip/pose");
    PushBinding(binds, &n, kMax, A("vibration_left"),  "/user/hand/left/output/haptic");
    PushBinding(binds, &n, kMax, A("vibration_right"), "/user/hand/right/output/haptic");
    PushBinding(binds, &n, kMax, A("vector2_walkdirection"), "/user/hand/left/input/thumbstick");
    PushBinding(binds, &n, kMax, A("boolean_sprint"),        "/user/hand/left/input/thumbstick/click");
    PushBinding(binds, &n, kMax, A("vector2_smoothturn"),    "/user/hand/right/input/thumbstick");
    PushBindingAllByName(binds, &n, kMax, "boolean_changeweapon", "/user/hand/right/input/thumbstick/click");
    PushBinding(binds, &n, kMax, g_xrLeftStickVec2,  "/user/hand/left/input/thumbstick");
    PushBinding(binds, &n, kMax, g_xrRightStickVec2, "/user/hand/right/input/thumbstick");
    rightTrig = A("vector1_primaryfire");
    if (rightTrig == XR_NULL_HANDLE) rightTrig = g_xrRightTriggerFloat;
    else g_xrRightTriggerFloat = rightTrig;
    PushBinding(binds, &n, kMax, rightTrig, "/user/hand/right/input/trigger/value");
    PushBinding(binds, &n, kMax, g_xrLeftTriggerFloat, "/user/hand/left/input/trigger/value");
    PushBinding(binds, &n, kMax, g_xrLeftSqueezeFloat,  "/user/hand/left/input/squeeze/value");
    PushBinding(binds, &n, kMax, g_xrRightSqueezeFloat, "/user/hand/right/input/squeeze/value");
    PushBinding(binds, &n, kMax, A("boolean_jump"),      "/user/hand/right/input/b/click");
    PushBinding(binds, &n, kMax, A("boolean_crouch"),    "/user/hand/right/input/a/click");
    PushBindingAllByName(binds, &n, kMax, "boolean_spawnmenu", "/user/hand/left/input/y/click");
    PushBinding(binds, &n, kMax, A("boolean_use"),       "/user/hand/left/input/x/click");
    PushBinding(binds, &n, kMax, A("boolean_flashlight"), "/user/hand/left/input/menu/click");
    PushBinding(binds, &n, kMax, A("vector1_forward"),   "/user/hand/right/input/trigger/value");
    PushBinding(binds, &n, kMax, A("vector1_reverse"),   "/user/hand/left/input/trigger/value");
    PushBinding(binds, &n, kMax, A("vector2_steer"),     "/user/hand/left/input/thumbstick");
    PushBinding(binds, &n, kMax, A("boolean_handbrake"), "/user/hand/right/input/a/click");
    PushBinding(binds, &n, kMax, A("boolean_turbo"),     "/user/hand/right/input/b/click");
    PushBinding(binds, &n, kMax, A("boolean_exit"),      "/user/hand/left/input/x/click");
    PushBinding(binds, &n, kMax, A("boolean_turret"),    "/user/hand/right/input/thumbstick/click");
    PushBinding(binds, &n, kMax, A("boolean_horn"),      "/user/hand/left/input/thumbstick/click");
    PushControllerSourceBindings(binds, &n, kMax, /*touch*/0);

    bool okTouch = SuggestBindingsForProfile(
        "/interaction_profiles/oculus/touch_controller", binds, (uint32_t)n);

    // Quest 3 / Touch Pro / WiVRn often report facebook/touch_controller_pro (or
    // meta/touch_controller_plus) rather than classic oculus/touch_controller.
    // Same path set as Oculus Touch for the common buttons we use — suggest the
    // identical table so controllers are not dead when the active profile differs.
    bool okTouchPro = SuggestBindingsForProfile(
        "/interaction_profiles/facebook/touch_controller_pro", binds, (uint32_t)n);
    // Optional Meta Touch Plus (Quest 3); ignore failure if runtime lacks extension.
    bool okTouchPlus = SuggestBindingsForProfile(
        "/interaction_profiles/meta/touch_controller_plus", binds, (uint32_t)n);
    if (okTouchPro) VRMOD_LOG_INFO("Suggested Touch-compatible bindings for facebook/touch_controller_pro");
    if (okTouchPlus) VRMOD_LOG_INFO("Suggested Touch-compatible bindings for meta/touch_controller_plus");
    okTouch = okTouch || okTouchPro || okTouchPlus;

    // If the full table was rejected (one bad path fails the whole suggest on some
    // runtimes), retry a reduced core set that every Touch/Index profile must support.
    if (!okTouch) {
        XrActionSuggestedBinding core[24];
        int cn = 0;
        PushBinding(core, &cn, 24, A("pose_lefthand"),  "/user/hand/left/input/grip/pose");
        PushBinding(core, &cn, 24, A("pose_righthand"), "/user/hand/right/input/grip/pose");
        PushBinding(core, &cn, 24, A("vibration_left"),  "/user/hand/left/output/haptic");
        PushBinding(core, &cn, 24, A("vibration_right"), "/user/hand/right/output/haptic");
        PushBinding(core, &cn, 24, A("vector2_walkdirection"), "/user/hand/left/input/thumbstick");
        PushBinding(core, &cn, 24, A("vector2_smoothturn"),    "/user/hand/right/input/thumbstick");
        PushBinding(core, &cn, 24, g_xrLeftStickVec2,  "/user/hand/left/input/thumbstick");
        PushBinding(core, &cn, 24, g_xrRightStickVec2, "/user/hand/right/input/thumbstick");
        XrAction rt = A("vector1_primaryfire");
        if (rt == XR_NULL_HANDLE) rt = g_xrRightTriggerFloat;
        PushBinding(core, &cn, 24, rt, "/user/hand/right/input/trigger/value");
        PushBinding(core, &cn, 24, g_xrLeftTriggerFloat, "/user/hand/left/input/trigger/value");
        PushBinding(core, &cn, 24, g_xrLeftSqueezeFloat,  "/user/hand/left/input/squeeze/value");
        PushBinding(core, &cn, 24, g_xrRightSqueezeFloat, "/user/hand/right/input/squeeze/value");
        PushBinding(core, &cn, 24, A("boolean_jump"),      "/user/hand/right/input/b/click");
        PushBinding(core, &cn, 24, A("boolean_crouch"),    "/user/hand/right/input/a/click");
        PushBinding(core, &cn, 24, A("boolean_spawnmenu"), "/user/hand/left/input/y/click");
        PushBinding(core, &cn, 24, A("boolean_use"),       "/user/hand/left/input/x/click");
        okTouch = SuggestBindingsForProfile(
            "/interaction_profiles/oculus/touch_controller", core, (uint32_t)cn);
        if (!okTouch) {
            okTouch = SuggestBindingsForProfile(
                "/interaction_profiles/facebook/touch_controller_pro", core, (uint32_t)cn);
        }
        if (okTouch) VRMOD_LOG_INFO("Touch-class profile accepted reduced core binding set");
    }

    // KHR simple controller: minimal set so something works on any runtime.
    {
        XrActionSuggestedBinding simple[16];
        int sn = 0;
        PushBinding(simple, &sn, 16, A("pose_lefthand"),  "/user/hand/left/input/grip/pose");
        PushBinding(simple, &sn, 16, A("pose_righthand"), "/user/hand/right/input/grip/pose");
        PushBinding(simple, &sn, 16, A("vibration_left"),  "/user/hand/left/output/haptic");
        PushBinding(simple, &sn, 16, A("vibration_right"), "/user/hand/right/output/haptic");
        PushBinding(simple, &sn, 16, A("boolean_primaryfire"), "/user/hand/right/input/select/click");
        PushBinding(simple, &sn, 16, A("boolean_use"),         "/user/hand/left/input/select/click");
        PushBinding(simple, &sn, 16, A("boolean_spawnmenu"),   "/user/hand/left/input/menu/click");
        SuggestBindingsForProfile(
            "/interaction_profiles/khr/simple_controller", simple, (uint32_t)sn);
    }

    if (!okTouch && !okIndex) {
        VRMOD_LOG_WARN("No full controller profile accepted bindings; input may be limited to simple_controller");
    }
    return okTouch || okIndex;
}

// Create internal float actions used only for analog→boolean threshold synthesis
// (left/right trigger + left/right squeeze for pickup).
static XrAction CreateInternalFloatAction(XrActionSet set, const char* name) {
    if (set == XR_NULL_HANDLE || !g_xrCreateAction) return XR_NULL_HANDLE;
    XrActionCreateInfo aci = {XR_TYPE_ACTION_CREATE_INFO};
    strncpy(aci.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
    strncpy(aci.localizedActionName, name, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
    aci.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
    XrAction a = XR_NULL_HANDLE;
    XrResult res = g_xrCreateAction(set, &aci, &a);
    if (res != XR_SUCCESS) {
        VRMOD_LOG_WARN("CreateInternalFloatAction '%s' failed: %s", name, XR_ResultToString(res));
        return XR_NULL_HANDLE;
    }
    return a;
}

static XrAction CreateInternalBooleanAction(XrActionSet set, const char* name) {
    if (set == XR_NULL_HANDLE || !g_xrCreateAction) return XR_NULL_HANDLE;
    XrActionCreateInfo aci = {XR_TYPE_ACTION_CREATE_INFO};
    strncpy(aci.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
    strncpy(aci.localizedActionName, name, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
    aci.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    XrAction a = XR_NULL_HANDLE;
    XrResult res = g_xrCreateAction(set, &aci, &a);
    if (res != XR_SUCCESS) {
        VRMOD_LOG_WARN("CreateInternalBooleanAction '%s' failed: %s", name, XR_ResultToString(res));
        return XR_NULL_HANDLE;
    }
    return a;
}

static XrAction CreateInternalVector2Action(XrActionSet set, const char* name) {
    if (set == XR_NULL_HANDLE || !g_xrCreateAction) return XR_NULL_HANDLE;
    XrActionCreateInfo aci = {XR_TYPE_ACTION_CREATE_INFO};
    strncpy(aci.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
    strncpy(aci.localizedActionName, name, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
    aci.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
    XrAction a = XR_NULL_HANDLE;
    XrResult res = g_xrCreateAction(set, &aci, &a);
    if (res != XR_SUCCESS) {
        VRMOD_LOG_WARN("CreateInternalVector2Action '%s' failed: %s", name, XR_ResultToString(res));
        return XR_NULL_HANDLE;
    }
    return a;
}

static float ReadFloatActionRaw(XrAction action) {
    if (action == XR_NULL_HANDLE || !g_xrSession) return 0.0f;
    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;
    XrActionStateFloat state = {XR_TYPE_ACTION_STATE_FLOAT};
    if (g_xrGetActionStateFloat(g_xrSession, &getInfo, &state) != XR_SUCCESS) return 0.0f;
    return state.isActive ? state.currentState : 0.0f;
}

static bool ReadBooleanActionRaw(XrAction action) {
    if (action == XR_NULL_HANDLE || !g_xrSession) return false;
    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;
    XrActionStateBoolean state = {XR_TYPE_ACTION_STATE_BOOLEAN};
    if (g_xrGetActionStateBoolean(g_xrSession, &getInfo, &state) != XR_SUCCESS) return false;
    return state.isActive && state.currentState;
}

bool XR_AttachActionSets() {
    if (g_xrActionsAttached) return true;
    if (g_xrActionSetCount == 0) return false;
    if (!g_xrSession) {
        VRMOD_LOG_ERROR("XR_AttachActionSets: no session yet");
        return false;
    }

    // Cache action list for FindXrActionByName (set from lua_interface before attach).
    // g_xrCachedActions is populated via XR_SetActionCache from the caller if needed;
    // ParseActionManifest already filled g_actions on the Lua side — we receive them
    // through XR_SetActionCache below. If not set, try nothing.
    // Ensure internal analog helpers + physical controller sources live in base set.
    if (g_xrActionSets[0] != XR_NULL_HANDLE) {
        XrActionSet base = g_xrActionSets[0];
        if (g_xrLeftTriggerFloat == XR_NULL_HANDLE)
            g_xrLeftTriggerFloat = CreateInternalFloatAction(base, "internal_left_trigger");
        if (g_xrRightTriggerFloat == XR_NULL_HANDLE)
            g_xrRightTriggerFloat = CreateInternalFloatAction(base, "internal_right_trigger");
        if (g_xrLeftSqueezeFloat == XR_NULL_HANDLE)
            g_xrLeftSqueezeFloat = CreateInternalFloatAction(base, "internal_left_squeeze");
        if (g_xrRightSqueezeFloat == XR_NULL_HANDLE)
            g_xrRightSqueezeFloat = CreateInternalFloatAction(base, "internal_right_squeeze");
        if (g_xrLeftThumbrestTouch == XR_NULL_HANDLE)
            g_xrLeftThumbrestTouch = CreateInternalBooleanAction(base, "internal_left_thumbrest");
        if (g_xrRightThumbrestTouch == XR_NULL_HANDLE)
            g_xrRightThumbrestTouch = CreateInternalBooleanAction(base, "internal_right_thumbrest");
        if (s_srcLeftX == XR_NULL_HANDLE)
            s_srcLeftX = CreateInternalBooleanAction(base, "src_left_x");
        if (s_srcLeftY == XR_NULL_HANDLE)
            s_srcLeftY = CreateInternalBooleanAction(base, "src_left_y");
        if (s_srcLeftMenu == XR_NULL_HANDLE)
            s_srcLeftMenu = CreateInternalBooleanAction(base, "src_left_menu");
        if (s_srcRightA == XR_NULL_HANDLE)
            s_srcRightA = CreateInternalBooleanAction(base, "src_right_a");
        if (s_srcRightB == XR_NULL_HANDLE)
            s_srcRightB = CreateInternalBooleanAction(base, "src_right_b");
        if (s_srcLeftStickClick == XR_NULL_HANDLE)
            s_srcLeftStickClick = CreateInternalBooleanAction(base, "src_left_stick_click");
        if (s_srcRightStickClick == XR_NULL_HANDLE)
            s_srcRightStickClick = CreateInternalBooleanAction(base, "src_right_stick_click");
        // Stick vectors for dpad sources (must be base so they work while driving).
        if (g_xrLeftStickVec2 == XR_NULL_HANDLE)
            g_xrLeftStickVec2 = CreateInternalVector2Action(base, "internal_left_stick");
        if (g_xrRightStickVec2 == XR_NULL_HANDLE)
            g_xrRightStickVec2 = CreateInternalVector2Action(base, "internal_right_stick");

        // Wire source table to the shared action handles.
        for (int i = 0; i < g_ctrlSourceCount; i++) {
            if (g_ctrlSources[i].shared && *g_ctrlSources[i].shared != XR_NULL_HANDLE)
                g_ctrlSources[i].action = *g_ctrlSources[i].shared;
        }
    }

    // Suggest bindings BEFORE attach (required by OpenXR).
    // Includes pose/haptic/buttons/sticks + internal trigger/squeeze floats + sources.
    XR_SuggestAllInteractionBindings();

    XrSessionActionSetsAttachInfo attachInfo = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = g_xrActionSetCount;
    attachInfo.actionSets = g_xrActionSets;

    XrResult res = g_xrAttachSessionActionSets(g_xrSession, &attachInfo);
    if (res != XR_SUCCESS) {
        VRMOD_LOG_ERROR("xrAttachSessionActionSets failed: %s", XR_ResultToString(res));
        return false;
    }

    g_xrActionsAttached = true;
    int spaces = XR_EnsureActionSpaces();
    VRMOD_LOG_INFO("Action sets attached to session (%d sets, %d action spaces total, %d newly ensured)",
        g_xrActionSetCount, g_xrActionSpaceCount, spaces);
    return true;
}

// Provide the parsed action table so binding suggestion can resolve short names.
void XR_SetActionCache(const action* actions, int count) {
    g_xrCachedActions = actions;
    g_xrCachedActionCount = count;
}

int XR_GetControllerSourceCount() {
    return g_ctrlSourceCount;
}

const char* XR_GetControllerSourceId(int index) {
    if (index < 0 || index >= g_ctrlSourceCount) return "";
    return g_ctrlSources[index].id ? g_ctrlSources[index].id : "";
}

const char* XR_GetControllerSourceLabel(int index) {
    if (index < 0 || index >= g_ctrlSourceCount) return "";
    return g_ctrlSources[index].label ? g_ctrlSources[index].label : "";
}

bool XR_GetControllerSourceIsFloat(int index) {
    if (index < 0 || index >= g_ctrlSourceCount) return false;
    return g_ctrlSources[index].isFloat;
}

static XrAction CtrlSourceAction(int index) {
    if (index < 0 || index >= g_ctrlSourceCount) return XR_NULL_HANDLE;
    XrAction a = g_ctrlSources[index].action;
    if (a == XR_NULL_HANDLE && g_ctrlSources[index].shared)
        a = *g_ctrlSources[index].shared;
    return a;
}

// Pure axis→dpad threshold (also covered by unit tests offline).
// axis: 1=north(+y), 2=south(-y), 3=east(+x), 4=west(-x)
static bool StickDpadFromAxes(float x, float y, int axis, float threshold) {
    switch (axis) {
        case 1: return y >=  threshold;
        case 2: return y <= -threshold;
        case 3: return x >=  threshold;
        case 4: return x <= -threshold;
        default: return false;
    }
}

// Synthetic stick dpad: read base-set parent vector2 and threshold one axis direction.
// Parent sticks live in /actions/base so they stay active while main is swapped for driving.
static bool ReadStickDpad(int index, float* valueOut, bool* activeOut) {
    if (valueOut) *valueOut = 0.0f;
    if (activeOut) *activeOut = false;
    if (index < 0 || index >= g_ctrlSourceCount) return false;
    const CtrlSource& s = g_ctrlSources[index];
    if (s.stickAxis < 1 || s.stickAxis > 4 || !s.stickVec) return false;
    XrAction a = *s.stickVec;
    if (a == XR_NULL_HANDLE || !g_xrSession) return false;
    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = a;
    XrActionStateVector2f state = {XR_TYPE_ACTION_STATE_VECTOR2F};
    if (g_xrGetActionStateVector2f(g_xrSession, &getInfo, &state) != XR_SUCCESS) return false;
    if (activeOut) *activeOut = state.isActive;
    if (!state.isActive) return true;
    bool pressed = StickDpadFromAxes(state.currentState.x, state.currentState.y,
                                     s.stickAxis, kStickDpadThreshold);
    if (valueOut) *valueOut = pressed ? 1.0f : 0.0f;
    return true;
}

float XR_GetControllerSourceValue(int index) {
    if (index < 0 || index >= g_ctrlSourceCount) return 0.0f;
    if (g_ctrlSources[index].stickAxis > 0) {
        float v = 0.0f;
        ReadStickDpad(index, &v, nullptr);
        return v;
    }
    XrAction a = CtrlSourceAction(index);
    if (a == XR_NULL_HANDLE) return 0.0f;
    if (g_ctrlSources[index].isFloat)
        return ReadFloatActionRaw(a);
    return ReadBooleanActionRaw(a) ? 1.0f : 0.0f;
}

bool XR_GetControllerSourceIsActive(int index) {
    if (index < 0 || index >= g_ctrlSourceCount) return false;
    if (g_ctrlSources[index].stickAxis > 0) {
        bool active = false;
        ReadStickDpad(index, nullptr, &active);
        return active;
    }
    XrAction a = CtrlSourceAction(index);
    if (a == XR_NULL_HANDLE || !g_xrSession) return false;
    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = a;
    if (g_ctrlSources[index].isFloat) {
        XrActionStateFloat state = {XR_TYPE_ACTION_STATE_FLOAT};
        if (g_xrGetActionStateFloat(g_xrSession, &getInfo, &state) != XR_SUCCESS) return false;
        return state.isActive;
    }
    XrActionStateBoolean state = {XR_TYPE_ACTION_STATE_BOOLEAN};
    if (g_xrGetActionStateBoolean(g_xrSession, &getInfo, &state) != XR_SUCCESS) return false;
    return state.isActive;
}

static float clampf01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

// OpenVR IVRInput::GetSkeletalSummaryData — same signature / fill as module-master.
// OpenXR has no skeleton input; FromDevice summary is built from controller analogs
// (same finger order: thumb, index, middle, ring, pinky).
bool XR_GetSkeletalSummaryData(VRActionHandle action, int eSummaryType,
                               VRSkeletalSummaryData_t* pSkeletalSummaryData) {
    (void)eSummaryType; // OpenVR passes FromDevice (1); we only have device-derived data
    if (!pSkeletalSummaryData) return false;
    memset(pSkeletalSummaryData, 0, sizeof(*pSkeletalSummaryData));

    const bool leftHand = (action == VRMOD_SKELETON_HANDLE_LEFT);
    const bool rightHand = (action == VRMOD_SKELETON_HANDLE_RIGHT);
    if (!leftHand && !rightHand) return false;
    if (!g_xrSession) return true; // valid empty summary (OpenVR also zeros on failure)

    float trigger = 0.0f;
    float squeeze = 0.0f;
    float thumb = 0.0f;

    if (leftHand) {
        trigger = ReadFloatActionRaw(g_xrLeftTriggerFloat);
        squeeze = ReadFloatActionRaw(g_xrLeftSqueezeFloat);
        if (trigger < 0.02f) {
            XrAction fire = FindXrActionByName("boolean_left_primaryfire");
            if (fire != XR_NULL_HANDLE && ReadBooleanActionRaw(fire)) trigger = 1.0f;
        }
        if (ReadBooleanActionRaw(s_srcLeftX) || ReadBooleanActionRaw(s_srcLeftY)
            || ReadBooleanActionRaw(s_srcLeftStickClick)
            || ReadBooleanActionRaw(g_xrLeftThumbrestTouch)
            || ReadBooleanActionRaw(s_srcLeftMenu)) {
            thumb = 1.0f;
        }
        if (g_xrLeftStickVec2 != XR_NULL_HANDLE) {
            XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
            gi.action = g_xrLeftStickVec2;
            XrActionStateVector2f st = {XR_TYPE_ACTION_STATE_VECTOR2F};
            if (g_xrGetActionStateVector2f(g_xrSession, &gi, &st) == XR_SUCCESS && st.isActive) {
                float m = sqrtf(st.currentState.x * st.currentState.x
                              + st.currentState.y * st.currentState.y);
                if (m > 0.12f) thumb = fmaxf(thumb, clampf01(0.25f + m * 0.75f));
            }
        }
    } else {
        trigger = ReadFloatActionRaw(g_xrRightTriggerFloat);
        {
            XrAction pf = FindXrActionByName("vector1_primaryfire");
            if (pf != XR_NULL_HANDLE) {
                float v = ReadFloatActionRaw(pf);
                if (v > trigger) trigger = v;
            }
        }
        squeeze = ReadFloatActionRaw(g_xrRightSqueezeFloat);
        if (trigger < 0.02f) {
            XrAction fire = FindXrActionByName("boolean_primaryfire");
            if (fire != XR_NULL_HANDLE && ReadBooleanActionRaw(fire)) trigger = 1.0f;
        }
        if (ReadBooleanActionRaw(s_srcRightA) || ReadBooleanActionRaw(s_srcRightB)
            || ReadBooleanActionRaw(s_srcRightStickClick)
            || ReadBooleanActionRaw(g_xrRightThumbrestTouch)) {
            thumb = 1.0f;
        }
        if (g_xrRightStickVec2 != XR_NULL_HANDLE) {
            XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
            gi.action = g_xrRightStickVec2;
            XrActionStateVector2f st = {XR_TYPE_ACTION_STATE_VECTOR2F};
            if (g_xrGetActionStateVector2f(g_xrSession, &gi, &st) == XR_SUCCESS && st.isActive) {
                float m = sqrtf(st.currentState.x * st.currentState.x
                              + st.currentState.y * st.currentState.y);
                if (m > 0.12f) thumb = fmaxf(thumb, clampf01(0.25f + m * 0.75f));
            }
        }
    }

    if (squeeze < 0.02f) {
        XrAction sq = FindXrActionByName(leftHand ? "boolean_left_secondaryfire" : "boolean_secondaryfire");
        if (sq != XR_NULL_HANDLE && ReadBooleanActionRaw(sq)) squeeze = 1.0f;
        XrAction sqf = FindXrActionByName(leftHand ? "vector1_left_secondaryfire" : "vector1_secondaryfire");
        if (sqf != XR_NULL_HANDLE) {
            float v = ReadFloatActionRaw(sqf);
            if (v > squeeze) squeeze = v;
        }
    }

    trigger = clampf01(trigger);
    squeeze = clampf01(squeeze);
    thumb = clampf01(thumb);

    // flFingerCurl — same order as OpenVR VRFinger_t / module-master
    pSkeletalSummaryData->flFingerCurl[0] = thumb;   // Thumb
    pSkeletalSummaryData->flFingerCurl[1] = trigger; // Index
    pSkeletalSummaryData->flFingerCurl[2] = squeeze; // Middle
    pSkeletalSummaryData->flFingerCurl[3] = clampf01(squeeze * 0.95f); // Ring
    pSkeletalSummaryData->flFingerCurl[4] = clampf01(squeeze * 0.88f); // Pinky

    // flFingerSplay — 0=touching, 1=separated (OpenVR). Controllers: open hand → high splay.
    const float open = 1.0f - squeeze;
    pSkeletalSummaryData->flFingerSplay[0] = open; // thumb-index
    pSkeletalSummaryData->flFingerSplay[1] = open; // index-middle
    pSkeletalSummaryData->flFingerSplay[2] = open; // middle-ring
    pSkeletalSummaryData->flFingerSplay[3] = open; // ring-pinky
    return true;
}

// Create any missing action spaces for pose actions (safe to call repeatedly).
// Needed when Parse ran before the GL-bound session existed, or a prior create failed.
int XR_EnsureActionSpaces() {
    if (!g_xrSession || !g_xrCachedActions || g_xrCachedActionCount <= 0) return 0;
    int created = 0;
    for (int i = 0; i < g_xrCachedActionCount; i++) {
        if (g_xrCachedActions[i].type != ActionType_Pose) continue;
        if (g_xrCachedActions[i].handle == VRMOD_INVALID_ACTION_HANDLE) continue;

        // Already have a space for this handle?
        bool have = false;
        for (int j = 0; j < g_xrActionSpaceCount; j++) {
            if (g_xrActionSpaces[j].vrActionHandle == g_xrCachedActions[i].handle) {
                have = true;
                break;
            }
        }
        if (have) continue;
        if (g_xrActionSpaceCount >= MAX_ACTION_SPACES) break;

        XrAction xrAction = (XrAction)(uintptr_t)g_xrCachedActions[i].handle;
        XrActionSpaceCreateInfo asci = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
        asci.action = xrAction;
        asci.poseInActionSpace = {{0, 0, 0, 1}, {0, 0, 0}};

        XrSpace space = XR_NULL_HANDLE;
        XrResult res = g_xrCreateActionSpace(g_xrSession, &asci, &space);
        if (res == XR_SUCCESS) {
            g_xrActionSpaces[g_xrActionSpaceCount].vrActionHandle = g_xrCachedActions[i].handle;
            g_xrActionSpaces[g_xrActionSpaceCount].space = space;
            strncpy(g_xrActionSpaces[g_xrActionSpaceCount].name,
                g_xrCachedActions[i].name ? g_xrCachedActions[i].name : "?", MAX_STR_LEN - 1);
            g_xrActionSpaceCount++;
            created++;
            VRMOD_LOG_INFO("Ensured action space for '%s'", g_xrActionSpaces[g_xrActionSpaceCount - 1].name);
        } else {
            VRMOD_LOG_WARN("XR_EnsureActionSpaces '%s' failed: %s",
                g_xrCachedActions[i].name ? g_xrCachedActions[i].name : "?",
                XR_ResultToString(res));
        }
    }
    return created;
}

void XR_SyncActions(const actionSet* activeSets, int activeCount) {
    if (!g_xrSessionRunning) return;

    // Late attach: if SetActionManifest ran before the GL session existed, retry now.
    if (!g_xrActionsAttached && g_xrSession && g_xrActionSetCount > 0) {
        VRMOD_LOG_INFO("XR_SyncActions: late-attaching action sets");
        XR_AttachActionSets();
    }
    if (!g_xrActionsAttached) return;

    // Ensure pose spaces exist (no-op if already created).
    static int s_spaceRetry = 0;
    if (g_xrActionSpaceCount < 2 && (s_spaceRetry++ % 120) == 0) {
        XR_EnsureActionSpaces();
    }

    XrActiveActionSet activeXrSets[MAX_ACTIONSETS];
    int validCount = 0;
    for (int i = 0; i < activeCount && i < MAX_ACTIONSETS; i++) {
        // Find the matching XrActionSet
        XrActionSet xrSet = (XrActionSet)(uintptr_t)activeSets[i].handle;
        if (xrSet == XR_NULL_HANDLE) {
            // Fallback: activate all known sets (index matching is unreliable)
            break;
        }
        if (xrSet != XR_NULL_HANDLE) {
            activeXrSets[validCount].actionSet = xrSet;
            activeXrSets[validCount].subactionPath = XR_NULL_PATH;
            validCount++;
        }
    }

    if (validCount == 0) {
        // Sync all sets as fallback
        for (int i = 0; i < g_xrActionSetCount; i++) {
            activeXrSets[i].actionSet = g_xrActionSets[i];
            activeXrSets[i].subactionPath = XR_NULL_PATH;
        }
        validCount = g_xrActionSetCount;
    }

    XrActionsSyncInfo syncInfo = {XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = validCount;
    syncInfo.activeActionSets = activeXrSets;

    XrResult res = g_xrSyncActions(g_xrSession, &syncInfo);
    if (res != XR_SUCCESS && res != XR_SESSION_NOT_FOCUSED) {
        VRMOD_LOG_WARN("xrSyncActions failed: %s", XR_ResultToString(res));
    }

    // Periodic input health: are pose actions active? (proves bindings took effect)
    static int s_inputHealth = 0;
    if ((++s_inputHealth % 180) == 0 && g_xrGetActionStatePose) {
        XrAction left = FindXrActionByName("pose_lefthand");
        XrAction right = FindXrActionByName("pose_righthand");
        auto poseActive = [](XrAction a) -> int {
            if (a == XR_NULL_HANDLE || !g_xrSession) return -1;
            XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
            gi.action = a;
            XrActionStatePose st = {XR_TYPE_ACTION_STATE_POSE};
            if (g_xrGetActionStatePose(g_xrSession, &gi, &st) != XR_SUCCESS) return -2;
            return st.isActive ? 1 : 0;
        };
        float walkX = 0, walkY = 0;
        XrAction walk = FindXrActionByName("vector2_walkdirection");
        if (walk != XR_NULL_HANDLE) {
            XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
            gi.action = walk;
            XrActionStateVector2f st = {XR_TYPE_ACTION_STATE_VECTOR2F};
            if (g_xrGetActionStateVector2f(g_xrSession, &gi, &st) == XR_SUCCESS && st.isActive) {
                walkX = st.currentState.x;
                walkY = st.currentState.y;
            }
        }
        VRMOD_LOG_INFO("INPUT health: leftPose=%d rightPose=%d walk=(%.2f,%.2f) spaces=%d attached=%d",
            poseActive(left), poseActive(right), walkX, walkY,
            g_xrActionSpaceCount, (int)g_xrActionsAttached);
    }
}

bool XR_GetBooleanAction(VRActionHandle handle, bool* changed, bool* isActiveOut) {
    XrAction action = (XrAction)(uintptr_t)handle;
    if (isActiveOut) *isActiveOut = false;
    if (action == XR_NULL_HANDLE) return false;

    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;

    XrActionStateBoolean state = {XR_TYPE_ACTION_STATE_BOOLEAN};
    XrResult res = g_xrGetActionStateBoolean(g_xrSession, &getInfo, &state);

    bool boolState = false;
    bool boolChanged = false;
    bool active = false;
    if (res == XR_SUCCESS) {
        active = state.isActive;
        boolState = state.currentState && state.isActive;
        boolChanged = state.changedSinceLastSync;
    }

    // Analog→boolean synthesis for Oculus Touch paths that only expose float
    // components (trigger/value, squeeze/value). Without this, primary fire /
    // secondary fire / grip pickup stay dead on Quest-class controllers.
    // Synthesis also marks the action "active" so GetActions won't let an
    // inactive driving-set duplicate clobber the synthesized main-set value.
    if (g_xrCachedActions) {
        const char* name = nullptr;
        for (int i = 0; i < g_xrCachedActionCount; i++) {
            if (g_xrCachedActions[i].handle == handle) {
                name = g_xrCachedActions[i].name;
                break;
            }
        }
        if (name) {
            float analog = 0.0f;
            if (strcmp(name, "boolean_primaryfire") == 0 ||
                strcmp(name, "boolean_left_primaryfire") == 0) {
                XrAction f = FindXrActionByName("vector1_primaryfire");
                if (f == XR_NULL_HANDLE) f = g_xrRightTriggerFloat;
                // left primary uses left trigger
                if (strcmp(name, "boolean_left_primaryfire") == 0)
                    f = g_xrLeftTriggerFloat;
                analog = ReadFloatActionRaw(f);
            } else if (strcmp(name, "boolean_secondaryfire") == 0 ||
                       strcmp(name, "boolean_left_secondaryfire") == 0) {
                analog = ReadFloatActionRaw(g_xrLeftTriggerFloat);
            } else if (strcmp(name, "boolean_right_pickup") == 0) {
                analog = ReadFloatActionRaw(g_xrRightSqueezeFloat);
            } else if (strcmp(name, "boolean_left_pickup") == 0) {
                analog = ReadFloatActionRaw(g_xrLeftSqueezeFloat);
            }
            if (analog >= kTriggerClickThreshold) {
                if (!boolState) boolChanged = true;
                boolState = true;
                active = true;
            }

            // SteamVR chords (OpenXR has no chord API) — Quest 3 / oculus_touch scheme:
            //   reload  = both thumbrests touch
            //   teleport = left stick click (held) + right thumbrest touch
            if (strcmp(name, "boolean_reload") == 0) {
                bool both = ReadBooleanActionRaw(g_xrLeftThumbrestTouch) &&
                            ReadBooleanActionRaw(g_xrRightThumbrestTouch);
                if (both) {
                    if (!boolState) boolChanged = true;
                    boolState = true;
                    active = true;
                }
            } else if (strcmp(name, "boolean_teleport") == 0) {
                bool chord = ReadBooleanActionRaw(s_srcLeftStickClick) &&
                             ReadBooleanActionRaw(g_xrRightThumbrestTouch);
                if (chord) {
                    if (!boolState) boolChanged = true;
                    boolState = true;
                    active = true;
                }
            }
        }
    }

    if (changed) *changed = boolChanged;
    if (isActiveOut) *isActiveOut = active;
    return boolState;
}

float XR_GetFloatAction(VRActionHandle handle, bool* isActiveOut) {
    XrAction action = (XrAction)(uintptr_t)handle;
    if (isActiveOut) *isActiveOut = false;
    if (action == XR_NULL_HANDLE) return 0.0f;

    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;

    XrActionStateFloat state = {XR_TYPE_ACTION_STATE_FLOAT};
    XrResult res = g_xrGetActionStateFloat(g_xrSession, &getInfo, &state);
    if (res != XR_SUCCESS) return 0.0f;

    if (isActiveOut) *isActiveOut = state.isActive;
    return state.isActive ? state.currentState : 0.0f;
}

void XR_GetVector2Action(VRActionHandle handle, float* x, float* y, bool* isActiveOut) {
    XrAction action = (XrAction)(uintptr_t)handle;
    *x = 0.0f; *y = 0.0f;
    if (isActiveOut) *isActiveOut = false;
    if (action == XR_NULL_HANDLE) return;

    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;

    XrActionStateVector2f state = {XR_TYPE_ACTION_STATE_VECTOR2F};
    XrResult res = g_xrGetActionStateVector2f(g_xrSession, &getInfo, &state);
    if (res != XR_SUCCESS) return;

    if (state.isActive) {
        *x = state.currentState.x;
        *y = state.currentState.y;
        if (isActiveOut) *isActiveOut = true;
    }
}

PoseResult XR_GetPoseAction(VRActionHandle handle) {
    PoseResult r;
    memset(&r, 0, sizeof(r));

    // Find the action space for this handle
    XrSpace space = XR_NULL_HANDLE;
    for (int i = 0; i < g_xrActionSpaceCount; i++) {
        if (g_xrActionSpaces[i].vrActionHandle == handle) {
            space = g_xrActionSpaces[i].space;
            break;
        }
    }
    if (space == XR_NULL_HANDLE) return r;

    XrSpaceVelocity velocity = {XR_TYPE_SPACE_VELOCITY};
    XrSpaceLocation location = {XR_TYPE_SPACE_LOCATION};
    location.next = &velocity;

    XrResult res = g_xrLocateSpace(space, g_xrStageSpace,
        g_xrFrameState.predictedDisplayTime, &location);
    if (res != XR_SUCCESS) return r;

    r = ConvertXrPose(location);
    return r;
}

void XR_TriggerHaptic(VRActionHandle handle, float startSec, float durationSec,
                       float frequency, float amplitude) {
    XrAction action = (XrAction)(uintptr_t)handle;
    if (action == XR_NULL_HANDLE) return;

    XrHapticVibration vibration = {XR_TYPE_HAPTIC_VIBRATION};
    vibration.amplitude = amplitude;
    vibration.frequency = frequency > 0 ? frequency : XR_FREQUENCY_UNSPECIFIED;
    vibration.duration = (XrDuration)(durationSec * 1000000000.0);  // seconds to nanoseconds

    XrHapticActionInfo hai = {XR_TYPE_HAPTIC_ACTION_INFO};
    hai.action = action;

    g_xrApplyHapticFeedback(g_xrSession, &hai, (XrHapticBaseHeader*)&vibration);
}

VRActionHandle XR_FindActionHandleByName(const char* name, const action* actions, int count) {
    for (int i = 0; i < count; i++) {
        if (strcmp(actions[i].name, name) == 0)
            return actions[i].handle;
    }
    return VRMOD_INVALID_ACTION_HANDLE;
}

void XR_UpdatePoses() {
    // Delegate to render unit (has the OpenXR headers and locate impl in scope).
    // This populates g_xrHMDPose with fresh data from xrLocateViews using the
    // frame predicted time, so the very first UpdateTracking after start gets
    // a valid hmd entry instead of relying on a prior submit.
    XR_RefreshHMDPose();
}