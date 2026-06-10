#include "vr_input.h"
#include "core/vrmod_log.h"
#include <cstring>
#include <cstdio>
#include <cmath>

// ── Global input state definitions ──
vr::IVRSystem*          g_pSystem = nullptr;
vr::IVRInput*           g_pInput  = nullptr;
vr::TrackedDevicePose_t g_poses[vr::k_unMaxTrackedDeviceCount];
actionSet               g_actionSets[MAX_ACTIONSETS];
int                     g_actionSetCount = 0;
vr::VRActiveActionSet_t g_activeActionSets[MAX_ACTIONSETS];
int                     g_activeActionSetCount = 0;
action                  g_actions[MAX_ACTIONS];
int                     g_actionCount = 0;

PoseResult ConvertPose(const vr::TrackedDevicePose_t& pose) {
    PoseResult r;
    r.valid = pose.bPoseIsValid;
    if (!r.valid) {
        memset(&r, 0, sizeof(r));
        return r;
    }
    const vr::HmdMatrix34_t& mat = pose.mDeviceToAbsoluteTracking;
    r.pos[0] = -mat.m[2][3];
    r.pos[1] = -mat.m[0][3];
    r.pos[2] =  mat.m[1][3];

    r.ang[0] =  asinf(mat.m[1][2]) * (180.0f / PI_F);
    r.ang[1] =  atan2f(mat.m[0][2], mat.m[2][2]) * (180.0f / PI_F);
    r.ang[2] =  atan2f(-mat.m[1][0], mat.m[1][1]) * (180.0f / PI_F);

    r.vel[0] = -pose.vVelocity.v[2];
    r.vel[1] = -pose.vVelocity.v[0];
    r.vel[2] =  pose.vVelocity.v[1];

    r.angvel[0] = -pose.vAngularVelocity.v[2] * (180.0f / PI_F);
    r.angvel[1] = -pose.vAngularVelocity.v[0] * (180.0f / PI_F);
    r.angvel[2] =  pose.vAngularVelocity.v[1] * (180.0f / PI_F);

    VRMOD_LOG_DEBUG("ConvertPose: pos(%.2f,%.2f,%.2f) ang(%.2f,%.2f,%.2f)",
                    r.pos[0], r.pos[1], r.pos[2], r.ang[0], r.ang[1], r.ang[2]);
    return r;
}

int ParseActionManifest(const char* path, action* actions, int maxActions, vr::IVRInput* pInput) {
    VRMOD_LOG_INFO("ParseActionManifest: %s", path);

    if (pInput->SetActionManifestPath(path) != vr::VRInputError_None) {
        VRMOD_LOG_ERROR("SetActionManifestPath failed for %s", path);
        return -1;
    }

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

    while (fscanf(file, fmt1, word) == 1 && strcmp(word, "actions") != 0)
        ;
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
            pInput->GetActionHandle(actions[count].fullname, &(actions[count].handle));
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
    VRMOD_LOG_INFO("ParseActionManifest: parsed %d actions", count);
    return count;
}

int FindOrCreateActionSet(const char* name, actionSet* sets, int* count, vr::IVRInput* pInput) {
    for (int j = 0; j < *count; j++) {
        if (strcmp(name, sets[j].name) == 0)
            return j;
    }
    pInput->GetActionSetHandle(name, &sets[*count].handle);
    memcpy(sets[*count].name, name, strlen(name));
    int idx = *count;
    (*count)++;
    VRMOD_LOG_DEBUG("Created action set '%s' at index %d", name, idx);
    return idx;
}

vr::VRActionHandle_t FindActionHandleByName(const char* name, const action* actions, int count) {
    for (int i = 0; i < count; i++) {
        if (strcmp(actions[i].name, name) == 0)
            return actions[i].handle;
    }
    return vr::k_ulInvalidActionHandle;
}
