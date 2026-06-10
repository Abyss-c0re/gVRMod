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
