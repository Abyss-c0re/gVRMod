#include "test_framework.h"
#include "mocks/mock_pose.h"
#include "core/vrmod_common.h"
#include <cstdio>
#include <cstring>
#include <cmath>

// ─── PoseResult tests (runtime-agnostic) ───

TEST(PoseResult_Valid) {
    PoseResult r = mock::MakePoseResult(-5.0f, -2.0f, 3.0f,  0,0,0,  0,0,0,  0,0,0);
    ASSERT_TRUE(r.valid);
    ASSERT_NEAR(r.pos[0], -5.0f, 0.001f);
    ASSERT_NEAR(r.pos[1], -2.0f, 0.001f);
    ASSERT_NEAR(r.pos[2],  3.0f, 0.001f);
}

TEST(PoseResult_Velocity) {
    PoseResult r = mock::MakePoseResult(0,0,0,  -3.0f, -1.0f, 2.0f,  0,0,0,  0,0,0);
    ASSERT_TRUE(r.valid);
    ASSERT_NEAR(r.vel[0], -3.0f, 0.001f);
    ASSERT_NEAR(r.vel[1], -1.0f, 0.001f);
    ASSERT_NEAR(r.vel[2],  2.0f, 0.001f);
}

TEST(PoseResult_Angles) {
    PoseResult r = mock::MakePoseResult(0,0,0, 0,0,0,  45.0f, 90.0f, 0.0f,  0,0,0);
    ASSERT_TRUE(r.valid);
    ASSERT_NEAR(r.ang[0], 45.0f, 0.001f);
    ASSERT_NEAR(r.ang[1], 90.0f, 0.001f);
    ASSERT_NEAR(r.ang[2],  0.0f, 0.001f);
}

TEST(PoseResult_AngularVelocity) {
    float toDeg = 180.0f / PI_F;
    PoseResult r = mock::MakePoseResult(0,0,0, 0,0,0, 0,0,0,
        -1.5f * toDeg, -0.5f * toDeg, 1.0f * toDeg);
    ASSERT_TRUE(r.valid);
    ASSERT_NEAR(r.angvel[0], -1.5f * toDeg, 0.01f);
    ASSERT_NEAR(r.angvel[1], -0.5f * toDeg, 0.01f);
    ASSERT_NEAR(r.angvel[2],  1.0f * toDeg, 0.01f);
}

TEST(PoseResult_Invalid) {
    PoseResult r = mock::MakePoseResult(1,2,3, 4,5,6, 7,8,9, 10,11,12, false);
    ASSERT_FALSE(r.valid);
}

// ─── Action manifest parsing tests (file format only, no OpenXR) ───
// We test the manifest file parsing logic by directly scanning the file.

static int TestParseManifestFileOnly(const char* path, action* actions, int maxActions) {
    FILE* file = fopen(path, "r");
    if (!file) return -2;

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
            actions[count].handle = count + 1; // Assign sequential handles for testing
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
    return count;
}

TEST(ParseManifest_ValidFile) {
    const char* tmpPath = "/tmp/vrmod_test_actions.json";
    FILE* f = fopen(tmpPath, "w");
    ASSERT_TRUE(f != nullptr);
    fprintf(f, "{\n");
    fprintf(f, "  \"actions\": [\n");
    fprintf(f, "    { \"name\": \"/actions/main/in/trigger\", \"type\": \"boolean\" },\n");
    fprintf(f, "    { \"name\": \"/actions/main/in/trackpad\", \"type\": \"vector2\" },\n");
    fprintf(f, "    { \"name\": \"/actions/main/in/hand_left\", \"type\": \"pose\" }\n");
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);

    action actions[MAX_ACTIONS];
    int count = TestParseManifestFileOnly(tmpPath, actions, MAX_ACTIONS);

    ASSERT_EQ(count, 3);
    ASSERT_STREQ(actions[0].name, "trigger");
    ASSERT_STREQ(actions[1].name, "trackpad");
    ASSERT_STREQ(actions[2].name, "hand_left");
    ASSERT_STREQ(actions[0].fullname, "/actions/main/in/trigger");
    ASSERT_TRUE(actions[0].type > 0);
    ASSERT_TRUE(actions[1].type > 0);
    ASSERT_TRUE(actions[2].type > 0);

    remove(tmpPath);
}

TEST(ParseManifest_EmptyFile) {
    const char* tmpPath = "/tmp/vrmod_test_empty.json";
    FILE* f = fopen(tmpPath, "w");
    fprintf(f, "{}\n");
    fclose(f);

    action actions[MAX_ACTIONS];
    int count = TestParseManifestFileOnly(tmpPath, actions, MAX_ACTIONS);
    ASSERT_EQ(count, 0);

    remove(tmpPath);
}

TEST(ParseManifest_FileNotFound) {
    action actions[MAX_ACTIONS];
    int count = TestParseManifestFileOnly("/tmp/this_file_does_not_exist_12345.json", actions, MAX_ACTIONS);
    ASSERT_EQ(count, -2);
}

// ─── ConvertXrPose math (ported from src/input/xr_input.cpp — no OpenXR runtime) ───
// Locks the OpenXR→Source position remap and legacy OpenVR angle extractors so
// orientation “sideways” regressions are caught offline.

static void TestQuatToRotMat(float qx, float qy, float qz, float qw, float m[3][3]) {
    float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    float xy = qx * qy, xz = qx * qz, yz = qy * qz;
    float wx = qw * qx, wy = qw * qy, wz = qw * qz;
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

static void TestConvertRotToSourceAng(const float m[3][3], float ang[3]) {
    ang[0] =  asinf(m[1][2]) * (180.0f / PI_F);
    ang[1] =  atan2f(m[0][2], m[2][2]) * (180.0f / PI_F);
    ang[2] =  atan2f(-m[1][0], m[1][1]) * (180.0f / PI_F);
}

static void TestConvertXrPosePos(float xrX, float xrY, float xrZ, float out[3]) {
    // OpenXR: x=right, y=up, z=back → Source: x=forward, y=left, z=up
    out[0] = -xrZ;
    out[1] = -xrX;
    out[2] =  xrY;
}

TEST(ConvertXrPose_PositionRemap) {
    float p[3];
    // 1m in front of HMD in OpenXR (-Z) → Source +X forward
    TestConvertXrPosePos(0, 0, -1, p);
    ASSERT_NEAR(p[0], 1.0f, 0.001f);
    ASSERT_NEAR(p[1], 0.0f, 0.001f);
    ASSERT_NEAR(p[2], 0.0f, 0.001f);
    // 1m up (OpenXR +Y) → Source +Z
    TestConvertXrPosePos(0, 1, 0, p);
    ASSERT_NEAR(p[0], 0.0f, 0.001f);
    ASSERT_NEAR(p[1], 0.0f, 0.001f);
    ASSERT_NEAR(p[2], 1.0f, 0.001f);
    // 1m right (OpenXR +X) → Source -Y (right)
    TestConvertXrPosePos(1, 0, 0, p);
    ASSERT_NEAR(p[0], 0.0f, 0.001f);
    ASSERT_NEAR(p[1], -1.0f, 0.001f);
    ASSERT_NEAR(p[2], 0.0f, 0.001f);
}

TEST(ConvertXrPose_IdentityAngles) {
    float m[3][3], ang[3];
    TestQuatToRotMat(0, 0, 0, 1, m);
    TestConvertRotToSourceAng(m, ang);
    ASSERT_NEAR(ang[0], 0.0f, 0.1f);
    ASSERT_NEAR(ang[1], 0.0f, 0.1f);
    ASSERT_NEAR(ang[2], 0.0f, 0.1f);
}

TEST(ConvertXrPose_YawLeft90) {
    // +90° around OpenXR Y (turn left) → Source yaw ≈ +90
    float s = sinf(PI_F / 4.0f), c = cosf(PI_F / 4.0f);
    float m[3][3], ang[3];
    TestQuatToRotMat(0, s, 0, c, m);
    TestConvertRotToSourceAng(m, ang);
    ASSERT_NEAR(ang[0], 0.0f, 1.0f);
    ASSERT_NEAR(ang[1], 90.0f, 1.0f);
    ASSERT_NEAR(ang[2], 0.0f, 1.0f);
}

TEST(ConvertXrPose_PitchNotScrambledToYaw) {
    // Pure +30° around OpenXR X must affect pitch only (not become yaw — "sideways" bug)
    float a = 30.0f * PI_F / 180.0f;
    float s = sinf(a * 0.5f), c = cosf(a * 0.5f);
    float m[3][3], ang[3];
    TestQuatToRotMat(s, 0, 0, c, m);
    TestConvertRotToSourceAng(m, ang);
    ASSERT_TRUE(fabsf(ang[0]) > 20.0f);       // pitch moved
    ASSERT_NEAR(ang[1], 0.0f, 5.0f);           // yaw must stay near 0
    ASSERT_NEAR(ang[2], 0.0f, 5.0f);           // roll near 0
}

// ─── Stick dpad threshold (mirrors StickDpadFromAxes in xr_input.cpp) ───
// axis: 1=north(+y), 2=south(-y), 3=east(+x), 4=west(-x)
static bool TestStickDpadFromAxes(float x, float y, int axis, float threshold) {
    switch (axis) {
        case 1: return y >=  threshold;
        case 2: return y <= -threshold;
        case 3: return x >=  threshold;
        case 4: return x <= -threshold;
        default: return false;
    }
}

TEST(StickDpad_ThresholdCardinals) {
    const float t = 0.55f;
    // At rest: no direction
    ASSERT_FALSE(TestStickDpadFromAxes(0, 0, 1, t));
    ASSERT_FALSE(TestStickDpadFromAxes(0, 0, 2, t));
    ASSERT_FALSE(TestStickDpadFromAxes(0, 0, 3, t));
    ASSERT_FALSE(TestStickDpadFromAxes(0, 0, 4, t));
    // Pure north / south / east / west past threshold
    ASSERT_TRUE(TestStickDpadFromAxes(0, 0.9f, 1, t));
    ASSERT_TRUE(TestStickDpadFromAxes(0, -0.9f, 2, t));
    ASSERT_TRUE(TestStickDpadFromAxes(0.9f, 0, 3, t));
    ASSERT_TRUE(TestStickDpadFromAxes(-0.9f, 0, 4, t));
    // Opposite directions false
    ASSERT_FALSE(TestStickDpadFromAxes(0, 0.9f, 2, t));
    ASSERT_FALSE(TestStickDpadFromAxes(0.9f, 0, 4, t));
}

TEST(StickDpad_BelowThreshold) {
    const float t = 0.55f;
    // Soft tilt must not fire (avoids accidental shift/lights while turning)
    ASSERT_FALSE(TestStickDpadFromAxes(0, 0.5f, 1, t));
    ASSERT_FALSE(TestStickDpadFromAxes(0.5f, 0, 3, t));
    // Exactly at threshold counts as pressed
    ASSERT_TRUE(TestStickDpadFromAxes(0, 0.55f, 1, t));
    ASSERT_TRUE(TestStickDpadFromAxes(-0.55f, 0, 4, t));
}

TEST(StickDpad_DiagonalPrefersBothAxes) {
    const float t = 0.55f;
    // Diagonal past both thresholds → north and east both true (chord-free OR mapping)
    ASSERT_TRUE(TestStickDpadFromAxes(0.8f, 0.8f, 1, t));
    ASSERT_TRUE(TestStickDpadFromAxes(0.8f, 0.8f, 3, t));
    ASSERT_FALSE(TestStickDpadFromAxes(0.8f, 0.8f, 2, t));
    ASSERT_FALSE(TestStickDpadFromAxes(0.8f, 0.8f, 4, t));
}

// ─── Space velocity flag semantics (mirrors ConvertXrPose velocity branch) ───
// OpenXR: velocity validity is on XrSpaceVelocity.velocityFlags, not locationFlags.
// LINEAR_VALID_BIT (0x1) == ORIENTATION_VALID_BIT (0x1) numerically — must not
// treat "orientation valid" as "linear velocity valid".
static const unsigned kLocOrientValid = 0x00000001u;
static const unsigned kLocPosValid    = 0x00000002u;
static const unsigned kVelLinearValid = 0x00000001u;
static const unsigned kVelAngularValid = 0x00000002u;

static void TestRemapVel(float xrX, float xrY, float xrZ, float out[3]) {
    out[0] = -xrZ; out[1] = -xrX; out[2] = xrY;
}

static bool TestShouldCopyLinear(unsigned locationFlags, bool hasVelChain, unsigned velocityFlags) {
    // Correct: only velocityFlags, and only if chain present
    (void)locationFlags;
    return hasVelChain && (velocityFlags & kVelLinearValid) != 0;
}

static bool TestWrongOldCheck(unsigned locationFlags) {
    // Bug: checked locationFlags & LINEAR_VALID (same bit as ORIENTATION_VALID)
    return (locationFlags & kVelLinearValid) != 0;
}

TEST(SpaceVelocity_FlagsNotLocationFlags) {
    // Pose with valid orientation+position, no velocity → must NOT copy linear
    unsigned loc = kLocOrientValid | kLocPosValid;
    ASSERT_TRUE(TestWrongOldCheck(loc)); // documents the old bug would fire
    ASSERT_FALSE(TestShouldCopyLinear(loc, /*hasVel*/true, /*velocityFlags*/0));
    ASSERT_FALSE(TestShouldCopyLinear(loc, false, kVelLinearValid));
    // Real linear valid
    ASSERT_TRUE(TestShouldCopyLinear(loc, true, kVelLinearValid));
    // Angular independent of linear
    ASSERT_FALSE(TestShouldCopyLinear(loc, true, kVelAngularValid));
}

TEST(SpaceVelocity_PositionRemapMatchesPose) {
    float v[3];
    // OpenXR +Z back → Source -X; +X right → Source -Y; +Y up → Source +Z
    TestRemapVel(1, 2, 3, v);
    ASSERT_NEAR(v[0], -3.0f, 0.001f);
    ASSERT_NEAR(v[1], -1.0f, 0.001f);
    ASSERT_NEAR(v[2],  2.0f, 0.001f);
}

// ─── Action set management tests (runtime-agnostic) ───

static int TestFindOrCreateActionSet(const char* name, actionSet* sets, int* count) {
    for (int j = 0; j < *count; j++) {
        if (strcmp(name, sets[j].name) == 0)
            return j;
    }
    strncpy(sets[*count].name, name, MAX_STR_LEN - 1);
    sets[*count].handle = *count + 100;
    int idx = *count;
    (*count)++;
    return idx;
}

TEST(FindOrCreateActionSet_New) {
    actionSet sets[MAX_ACTIONSETS];
    memset(sets, 0, sizeof(sets));
    int count = 0;

    int idx = TestFindOrCreateActionSet("/actions/main", sets, &count);
    ASSERT_EQ(idx, 0);
    ASSERT_EQ(count, 1);
    ASSERT_STREQ(sets[0].name, "/actions/main");
}

TEST(FindOrCreateActionSet_Existing) {
    actionSet sets[MAX_ACTIONSETS];
    memset(sets, 0, sizeof(sets));
    int count = 0;

    int idx1 = TestFindOrCreateActionSet("/actions/main", sets, &count);
    int idx2 = TestFindOrCreateActionSet("/actions/main", sets, &count);
    ASSERT_EQ(idx1, idx2);
    ASSERT_EQ(count, 1);
}

TEST(FindOrCreateActionSet_Multiple) {
    actionSet sets[MAX_ACTIONSETS];
    memset(sets, 0, sizeof(sets));
    int count = 0;

    int idx1 = TestFindOrCreateActionSet("/actions/main", sets, &count);
    int idx2 = TestFindOrCreateActionSet("/actions/driving", sets, &count);
    ASSERT_EQ(idx1, 0);
    ASSERT_EQ(idx2, 1);
    ASSERT_EQ(count, 2);
}

// ─── Haptic lookup tests ───

static VRActionHandle TestFindActionHandleByName(const char* name, const action* actions, int count) {
    for (int i = 0; i < count; i++) {
        if (strcmp(actions[i].name, name) == 0)
            return actions[i].handle;
    }
    return VRMOD_INVALID_ACTION_HANDLE;
}

TEST(FindActionHandleByName_Found) {
    action actions[3];
    memset(actions, 0, sizeof(actions));
    strcpy(actions[0].fullname, "/actions/main/in/haptic_left");
    actions[0].name = actions[0].fullname + 17; // "haptic_left"
    actions[0].handle = 42;
    strcpy(actions[1].fullname, "/actions/main/in/haptic_right");
    actions[1].name = actions[1].fullname + 17; // "haptic_right"
    actions[1].handle = 43;

    VRActionHandle h = TestFindActionHandleByName("haptic_left", actions, 2);
    ASSERT_EQ(h, (VRActionHandle)42);
}

TEST(FindActionHandleByName_NotFound) {
    action actions[1];
    memset(actions, 0, sizeof(actions));
    strcpy(actions[0].fullname, "/actions/main/in/trigger");
    actions[0].name = actions[0].fullname + 17;
    actions[0].handle = 10;

    VRActionHandle h = TestFindActionHandleByName("nonexistent", actions, 1);
    ASSERT_EQ(h, VRMOD_INVALID_ACTION_HANDLE);
}

// ─── Boolean action type hash test ───

TEST(ActionType_BooleanHash) {
    const char* typeStr = "boolean";
    int hash = 0;
    for (int i = 0; typeStr[i]; i++) hash += typeStr[i];
    ASSERT_EQ(hash, ActionType_Boolean);
}

TEST(ActionType_PoseHash) {
    const char* typeStr = "pose";
    int hash = 0;
    for (int i = 0; typeStr[i]; i++) hash += typeStr[i];
    ASSERT_EQ(hash, ActionType_Pose);
}

TEST(ActionType_Vector1Hash) {
    const char* typeStr = "vector1";
    int hash = 0;
    for (int i = 0; typeStr[i]; i++) hash += typeStr[i];
    ASSERT_EQ(hash, ActionType_Vector1);
}

TEST(ActionType_Vector2Hash) {
    const char* typeStr = "vector2";
    int hash = 0;
    for (int i = 0; typeStr[i]; i++) hash += typeStr[i];
    ASSERT_EQ(hash, ActionType_Vector2);
}
