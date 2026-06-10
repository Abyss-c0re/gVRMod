#pragma once

#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <climits>
#include <algorithm>

#define MAX_STR_LEN     256
#define MAX_ACTIONS     64
#define MAX_ACTIONSETS  16
#define PI_F            3.141592654f

constexpr size_t HOOK_SIZE = 14;

enum EActionType {
    ActionType_Pose         = 439,
    ActionType_Vector1      = 708,
    ActionType_Vector2      = 709,
    ActionType_Boolean      = 736,
    ActionType_Skeleton     = 869,
    ActionType_Vibration    = 974,
};

enum ELuaRefIndex {
    LuaRefIndex_EmptyTable,
    LuaRefIndex_PoseTable,
    LuaRefIndex_HmdPose,
    LuaRefIndex_ActionTable,
    LuaRefIndex_Max,
};

// Runtime-agnostic action handle
typedef uint64_t VRActionHandle;
typedef uint64_t VRActionSetHandle;

#define VRMOD_INVALID_ACTION_HANDLE 0
#define VRMOD_INVALID_ACTIONSET_HANDLE 0

struct action {
    VRActionHandle handle;
    char fullname[MAX_STR_LEN];
    int luaRefs[2];
    char* name;
    int type;
};

struct actionSet {
    VRActionSetHandle handle;
    char name[MAX_STR_LEN];
};

struct PoseResult {
    float pos[3];
    float vel[3];
    float ang[3];
    float angvel[3];
    bool valid;
};
