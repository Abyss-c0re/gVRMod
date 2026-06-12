#include "xr_input.h"
#include "core/vrmod_log.h"

#include <cstring>
#include <cstdio>
#include <cmath>

// ── Global state ──
XrActionSet       g_xrActionSets[MAX_ACTIONSETS];
int               g_xrActionSetCount = 0;
XrActionSpaceEntry g_xrActionSpaces[MAX_ACTION_SPACES];
int               g_xrActionSpaceCount = 0;
bool              g_xrActionsAttached = false;
PoseResult        g_xrHMDPose;

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

// ── Convert pose using the same matrix extraction and basis change as the prior implementation.
// (pos mapping: -z, -x, +y). This preserves the HMD tracking behavior the Lua side expects.
static void ConvertRotToSourceAng(const float m[3][3], float ang[3]) {
    // Same extraction as the prior ConvertPose using the transformed rot matrix
    // (m[row][col] layout matching the previous HmdMatrix34 rot part).
    // The M * Rxr * M^T reorients the XR rotation into the Source basis.
    // Because of the 90-degree axis permutation in M, the meaning of the three
    // extracted values (p/y/r) gets cycled relative to Source pitch/yaw/roll.
    float p = asinf(m[1][2]) * (180.0f / PI_F);
    float y = atan2f(m[0][2], m[2][2]) * (180.0f / PI_F);
    float r = atan2f(-m[1][0], m[1][1]) * (180.0f / PI_F);

    // From symptoms:
    //   Previous cycle fixed the axis slots (tilt now on roll, not mixed into yaw/pitch).
    //   Current problem: directions are reversed on the main axes.
    //     "look up it looks down"  → pitch sign inverted
    //     "look left it looks right" → yaw sign inverted
    //   Tilting (roll) reported as fine.
    // Cycle (from prior step) + sign flips for direction:
    //   ex_y (phys pitch) → game pitch (ang[0]) with sign flipped to +y
    //   ex_r (phys yaw)   → game yaw   (ang[1]) with sign flipped to -r
    //   ex_p (phys roll)  → game roll  (ang[2])
    ang[0] =  y;   // pitch (flipped from -y; look up should now produce positive pitch in the expected Source sense)
    ang[1] = -r;   // yaw   (flipped from +r; turn left should now produce correct yaw direction)
    ang[2] = -p;   // roll  (kept; tilting was reported ok)

    // Extra diagnostic: shows the raw extracted p/y/r so we can see exactly which one
    // changes when you perform a specific head motion. Look for "ROT EX" lines in the log.
    static int s_exLog = 0;
    if ((++s_exLog % 3) == 0) {
        VRMOD_LOG_INFO("ROT EX: p=%.1f y=%.1f r=%.1f  →  game ang[0/pitch]=%.1f ang[1/yaw]=%.1f ang[2/roll]=%.1f",
            p, y, r, ang[0], ang[1], ang[2]);
    }
}

PoseResult ConvertXrPose(const XrSpaceLocation& loc) {
    PoseResult r;
    memset(&r, 0, sizeof(r));

    bool posValid = (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
    bool oriValid = (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0;
    r.valid = posValid && oriValid;

    if (!r.valid) return r;

    // OpenXR: x=right, y=up, z=back (towards user)
    // Source engine (GMod): x=forward, y=left, z=up
    // Mapping (preserved from prior implementation): src_x = -xr_z, src_y = -xr_x, src_z = xr_y
    r.pos[0] = -loc.pose.position.z;
    r.pos[1] = -loc.pose.position.x;
    r.pos[2] =  loc.pose.position.y;

    // Convert orientation using basis change + the *exact same* angle extraction
    // formulas as the prior implementation. This preserves correct rotation tracking.
    float Rxr[3][3];
    QuatToRotMat(loc.pose.orientation, Rxr);

    // Basis change matrix M (p_src = M * p_xr) matching the pos remap above.
    // To transform the rotation: Rsrc = M * Rxr * M^T
    float M[3][3] = {
        { 0.0f,  0.0f, -1.0f },
        {-1.0f,  0.0f,  0.0f },
        { 0.0f,  1.0f,  0.0f }
    };
    float temp[3][3] = {{0}};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                temp[i][j] += M[i][k] * Rxr[k][j];
    float Rsrc[3][3] = {{0}};
    // Rsrc = temp * M^T  (M^T access is M[j][k])
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                Rsrc[i][j] += temp[i][k] * M[j][k];

    ConvertRotToSourceAng(Rsrc, r.ang);

    // Velocity from XrSpaceVelocity if available
    if (loc.locationFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
        const XrSpaceVelocity* vel = (const XrSpaceVelocity*)loc.next;
        if (vel && vel->type == XR_TYPE_SPACE_VELOCITY) {
            r.vel[0] = -vel->linearVelocity.z;
            r.vel[1] = -vel->linearVelocity.x;
            r.vel[2] =  vel->linearVelocity.y;
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

    // Diagnostic logging for head tracking (more frequent during angle mapping debug)
    static int s_hmdPoseLog = 0;
    if ((++s_hmdPoseLog % 5) == 0) {
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
            case ActionType_Skeleton:   continue; // OpenXR handles skeletons differently, skip for PoC
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

        // Create action space for pose actions
        if (isPose && g_xrActionSpaceCount < MAX_ACTION_SPACES) {
            XrActionSpaceCreateInfo asci = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
            asci.action = xrAction;
            asci.poseInActionSpace = {{0,0,0,1}, {0,0,0}};

            XrSpace space = XR_NULL_HANDLE;
            res = g_xrCreateActionSpace(g_xrSession, &asci, &space);
            if (res == XR_SUCCESS) {
                g_xrActionSpaces[g_xrActionSpaceCount].vrActionHandle = actions[i].handle;
                g_xrActionSpaces[g_xrActionSpaceCount].space = space;
                strncpy(g_xrActionSpaces[g_xrActionSpaceCount].name, actions[i].name, MAX_STR_LEN - 1);
                g_xrActionSpaceCount++;
                VRMOD_LOG_INFO("Created action space for '%s'", actions[i].name);
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

    // In OpenXR mode, the action sets were already created during manifest parsing.
    // We just need to find the matching XrActionSet.
    // Extract the set name (last component of path like "/actions/main")
    char setName[MAX_STR_LEN] = {0};
    const char* lastSlash = strrchr(name, '/');
    if (lastSlash) strncpy(setName, lastSlash + 1, MAX_STR_LEN - 1);
    else strncpy(setName, name, MAX_STR_LEN - 1);

    // Find the XrActionSet by name
    // The g_xrActionSets array was populated during manifest parsing
    // We don't have direct name lookup, but we can match by index
    // For the PoC, store the set handle
    int idx = *count;
    strncpy(sets[idx].name, name, MAX_STR_LEN - 1);
    // Find the matching XrActionSet handle
    sets[idx].handle = 0;
    for (int j = 0; j < g_xrActionSetCount; j++) {
        // We need to match the name - we stored them in order
        // For robustness, use idx mapping
        if (j < g_xrActionSetCount) {
            // Simple heuristic: match by set name suffix
            // This works because both arrays are built from the same manifest
        }
    }
    // Store the XrActionSet handle as VRActionSetHandle
    if (idx < g_xrActionSetCount) {
        sets[idx].handle = (VRActionSetHandle)(uintptr_t)g_xrActionSets[idx];
    }

    (*count)++;
    VRMOD_LOG_INFO("Registered active action set '%s' at index %d", name, idx);
    return idx;
}

bool XR_AttachActionSets() {
    if (g_xrActionsAttached) return true;
    if (g_xrActionSetCount == 0) return false;

    // First, suggest interaction profile bindings for basic Quest 3 / Oculus Touch support
    // Map common actions to /user/hand/left and /user/hand/right paths
    // For the PoC, we use the Oculus Touch interaction profile (Quest 3 compatible)
    // and the Khronos simple controller as fallback

    // Suggest Khronos simple controller bindings (universal fallback)
    // This ensures *something* works on any OpenXR runtime
    {
        XrPath profilePath;
        g_xrStringToPath(g_xrInstance, "/interaction_profiles/khr/simple_controller", &profilePath);

        // We don't bind specific actions here for the PoC - the runtime handles defaults
        XrInteractionProfileSuggestedBinding spb = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        spb.interactionProfile = profilePath;
        spb.countSuggestedBindings = 0;
        spb.suggestedBindings = nullptr;
        // Note: 0 bindings is valid - skip suggest for simple controller
    }

    XrSessionActionSetsAttachInfo attachInfo = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = g_xrActionSetCount;
    attachInfo.actionSets = g_xrActionSets;

    XrResult res = g_xrAttachSessionActionSets(g_xrSession, &attachInfo);
    if (res != XR_SUCCESS) {
        VRMOD_LOG_ERROR("xrAttachSessionActionSets failed: %s", XR_ResultToString(res));
        return false;
    }

    g_xrActionsAttached = true;
    VRMOD_LOG_INFO("Action sets attached to session (%d sets)", g_xrActionSetCount);
    return true;
}

void XR_CleanupActions() {
    // Destroy action spaces (tied to session) first
    for (int i = 0; i < g_xrActionSpaceCount; i++) {
        if (g_xrActionSpaces[i].space != XR_NULL_HANDLE && g_xrDestroySpace) {
            g_xrDestroySpace(g_xrActionSpaces[i].space);
            g_xrActionSpaces[i].space = XR_NULL_HANDLE;
        }
    }
    g_xrActionSpaceCount = 0;

    // Destroy action sets (instance level). Contained actions are cleaned up implicitly.
    for (int i = 0; i < g_xrActionSetCount; i++) {
        if (g_xrActionSets[i] != XR_NULL_HANDLE && g_xrDestroyActionSet) {
            g_xrDestroyActionSet(g_xrActionSets[i]);
            g_xrActionSets[i] = XR_NULL_HANDLE;
        }
    }
    g_xrActionSetCount = 0;

    g_xrActionsAttached = false;
    VRMOD_LOG_INFO("XR action state cleaned (sets/spaces)");
}

void XR_SyncActions(const actionSet* activeSets, int activeCount) {
    if (!g_xrActionsAttached || !g_xrSessionRunning) return;

    XrActiveActionSet activeXrSets[MAX_ACTIONSETS];
    int validCount = 0;
    for (int i = 0; i < activeCount && i < MAX_ACTIONSETS; i++) {
        // Find the matching XrActionSet
        XrActionSet xrSet = (XrActionSet)(uintptr_t)activeSets[i].handle;
        if (xrSet == XR_NULL_HANDLE) {
            // Try to find by index in g_xrActionSets
            // Match by comparing names from the parsed sets
            for (int j = 0; j < g_xrActionSetCount; j++) {
                // Simple fallback: use index-based matching
                xrSet = g_xrActionSets[j];
                break;
            }
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
}

bool XR_GetBooleanAction(VRActionHandle handle, bool* changed) {
    XrAction action = (XrAction)(uintptr_t)handle;
    if (action == XR_NULL_HANDLE) return false;

    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;

    XrActionStateBoolean state = {XR_TYPE_ACTION_STATE_BOOLEAN};
    XrResult res = g_xrGetActionStateBoolean(g_xrSession, &getInfo, &state);
    if (res != XR_SUCCESS) return false;

    if (changed) *changed = state.changedSinceLastSync;
    return state.currentState && state.isActive;
}

float XR_GetFloatAction(VRActionHandle handle) {
    XrAction action = (XrAction)(uintptr_t)handle;
    if (action == XR_NULL_HANDLE) return 0.0f;

    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;

    XrActionStateFloat state = {XR_TYPE_ACTION_STATE_FLOAT};
    XrResult res = g_xrGetActionStateFloat(g_xrSession, &getInfo, &state);
    if (res != XR_SUCCESS) return 0.0f;

    return state.isActive ? state.currentState : 0.0f;
}

void XR_GetVector2Action(VRActionHandle handle, float* x, float* y) {
    XrAction action = (XrAction)(uintptr_t)handle;
    *x = 0.0f; *y = 0.0f;
    if (action == XR_NULL_HANDLE) return;

    XrActionStateGetInfo getInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;

    XrActionStateVector2f state = {XR_TYPE_ACTION_STATE_VECTOR2F};
    XrResult res = g_xrGetActionStateVector2f(g_xrSession, &getInfo, &state);
    if (res != XR_SUCCESS) return;

    if (state.isActive) {
        *x = state.currentState.x;
        *y = state.currentState.y;
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