#include "test_framework.h"
#include "mocks/mock_lua.h"
#include "mocks/mock_pose.h"
#include "core/vrmod_common.h"
#include <cstring>

// ─── PoseResult → Lua table push verification ───
// Simulates what GetPoses does with the converted pose data

static void PushPoseToLua(GarrysMod::Lua::ILuaBase* LUA, const PoseResult& pr, const char* name, int poseRef) {
    if (!pr.valid) return;
    Vector pos; pos.x = pr.pos[0]; pos.y = pr.pos[1]; pos.z = pr.pos[2];
    Vector vel; vel.x = pr.vel[0]; vel.y = pr.vel[1]; vel.z = pr.vel[2];
    QAngle ang; ang.x = pr.ang[0]; ang.y = pr.ang[1]; ang.z = pr.ang[2];
    QAngle angvel; angvel.x = pr.angvel[0]; angvel.y = pr.angvel[1]; angvel.z = pr.angvel[2];
    LUA->ReferencePush(poseRef);
    LUA->PushVector(pos);
    LUA->SetField(-2, "pos");
    LUA->PushVector(vel);
    LUA->SetField(-2, "vel");
    LUA->PushAngle(ang);
    LUA->SetField(-2, "ang");
    LUA->PushAngle(angvel);
    LUA->SetField(-2, "angvel");
    LUA->SetField(-2, name);
}

TEST(LuaPose_PushesCorrectFields) {
    mock::MockLuaBase lua;
    // Use the runtime-agnostic PoseResult directly
    PoseResult pr = mock::MakePoseResult(-3.0f, -1.0f, 2.0f,  0,0,0,  0,0,0,  0,0,0);
    PushPoseToLua(&lua, pr, "hmd", 1);

    ASSERT_TRUE(mock::HasSetField(lua, "pos"));
    ASSERT_TRUE(mock::HasSetField(lua, "vel"));
    ASSERT_TRUE(mock::HasSetField(lua, "ang"));
    ASSERT_TRUE(mock::HasSetField(lua, "angvel"));
    ASSERT_TRUE(mock::HasSetField(lua, "hmd"));

    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::PUSH_VECTOR), 2);
    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::PUSH_ANGLE), 2);
    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::REF_PUSH), 1);
}

TEST(LuaPose_InvalidSkipped) {
    mock::MockLuaBase lua;
    PoseResult pr = mock::MakePoseResult(0,0,0, 0,0,0, 0,0,0, 0,0,0, false);
    PushPoseToLua(&lua, pr, "hmd", 1);

    // Nothing should have been pushed
    ASSERT_EQ((int)lua.calls.size(), 0);
}

// ─── Matrix push verification ───

static void PushMatrixAsTable(GarrysMod::Lua::ILuaBase* LUA, float* mtx, unsigned int rows, unsigned int cols) {
    LUA->CreateTable();
    for (unsigned int row = 0; row < rows; row++) {
        LUA->PushNumber(row + 1);
        LUA->CreateTable();
        for (unsigned int col = 0; col < cols; col++) {
            LUA->PushNumber(col + 1);
            LUA->PushNumber(mtx[row * cols + col]);
            LUA->SetTable(-3);
        }
        LUA->SetTable(-3);
    }
}

TEST(LuaMatrix_3x4) {
    mock::MockLuaBase lua;
    float mat[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    PushMatrixAsTable(&lua, mat, 3, 4);

    // 1 outer table + 3 row tables = 4 CreateTable calls
    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::CREATE_TABLE), 4);
    // 3 row indices + (3 * 4) col indices + (3 * 4) values = 3 + 12 + 12 = 27 PushNumber
    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::PUSH_NUMBER), 27);
    // 3 * 4 cell SetTable + 3 row SetTable = 15 SetTable
    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::SET_TABLE), 15);
}

TEST(LuaMatrix_4x4) {
    mock::MockLuaBase lua;
    float mat[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    PushMatrixAsTable(&lua, mat, 4, 4);

    // 1 outer + 4 row = 5 CreateTable
    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::CREATE_TABLE), 5);
    // 4 row indices + (4*4) col indices + (4*4) values = 4 + 16 + 16 = 36
    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::PUSH_NUMBER), 36);
}

// ─── Lua ref management ───

TEST(LuaRef_CreateAndFree) {
    mock::MockLuaBase lua;

    // Simulate creating refs like Init does
    int refs[4];
    for (int i = 0; i < 4; i++) {
        lua.CreateTable();
        refs[i] = lua.ReferenceCreate();
    }
    ASSERT_EQ(refs[0], 1);
    ASSERT_EQ(refs[1], 2);
    ASSERT_EQ(refs[2], 3);
    ASSERT_EQ(refs[3], 4);

    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::CREATE_TABLE), 4);
    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::REF_CREATE), 4);

    // Free them
    for (int i = 0; i < 4; i++) {
        lua.ReferenceFree(refs[i]);
    }
    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::REF_FREE), 4);
}

// ─── Error reporting ───

TEST(LuaError_ThrowRecorded) {
    mock::MockLuaBase lua;
    lua.ThrowError("VRMOD: test error");
    ASSERT_TRUE(lua.throwCalled);
    ASSERT_STREQ(lua.lastError.c_str(), "VRMOD: test error");
}

// ─── Module registration verification ───
// Verifies that the expected function names would be registered

TEST(ModuleRegistration_AllFunctionsPresent) {
    const char* expectedFunctions[] = {
        "GetVersion", "IsHMDPresent", "Init",
        "SetActionManifest", "SetActiveActionSets", "GetDisplayInfo",
        "UpdatePosesAndActions", "GetPoses", "GetActions",
        "ShareTextureBegin", "ShareTextureFinish",
        "SetSubmitTextureBounds", "SubmitSharedTexture",
        "Shutdown", "TriggerHaptic", "GetTrackedDeviceNames"
    };

    // Simulate what GMOD_MODULE_OPEN does: PushCFunction + SetField for each
    mock::MockLuaBase lua;
    for (auto name : expectedFunctions) {
        lua.PushCFunction(nullptr);
        lua.SetField(-2, name);
    }

    // Verify all names were set
    for (auto name : expectedFunctions) {
        ASSERT_TRUE(mock::HasSetField(lua, name));
    }
    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::PUSH_CFUNC), 16);
    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::SET_FIELD), 16);
}

// ─── CheckString/CheckNumber mock returns ───

TEST(MockLua_CheckStringReturns) {
    mock::MockLuaBase lua;
    lua.checkStringReturns = {"hello", "world"};
    ASSERT_STREQ(lua.CheckString(1), "hello");
    ASSERT_STREQ(lua.CheckString(2), "world");
}

TEST(MockLua_CheckNumberReturns) {
    mock::MockLuaBase lua;
    lua.checkNumberReturns = {1.5, 2.5, 3.5};
    ASSERT_NEAR(lua.CheckNumber(1), 1.5, 0.001);
    ASSERT_NEAR(lua.CheckNumber(2), 2.5, 0.001);
    ASSERT_NEAR(lua.CheckNumber(3), 3.5, 0.001);
}

// ─── End-to-end: pose conversion + lua push for multiple poses ───

TEST(EndToEnd_MultiplePoses) {
    mock::MockLuaBase lua;

    // Simulate HMD + 2 controller poses using runtime-agnostic PoseResult
    PoseResult poses[3];
    poses[0] = mock::MakePoseResult(0, -0, 1.7f,  0,0,0,  0,0,0,  0,0,0);  // HMD at 1.7m height
    poses[1] = mock::MakePoseResult(-0.5f, 0.3f, 1.0f,  0,0,0,  0,0,0,  0,0,0);  // Left
    poses[2] = mock::MakePoseResult(-0.5f, -0.3f, 1.0f,  0,0,0,  0,0,0,  0,0,0);  // Right

    const char* names[] = {"hmd", "hand_left", "hand_right"};
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(poses[i].valid);
        PushPoseToLua(&lua, poses[i], names[i], i + 1);
    }

    // 3 poses * (1 ReferencePush + 2 PushVector + 2 PushAngle + 5 SetField)
    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::REF_PUSH), 3);
    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::PUSH_VECTOR), 6);
    ASSERT_EQ(mock::CountCalls(lua, mock::LuaCall::PUSH_ANGLE), 6);
    ASSERT_TRUE(mock::HasSetField(lua, "hmd"));
    ASSERT_TRUE(mock::HasSetField(lua, "hand_left"));
    ASSERT_TRUE(mock::HasSetField(lua, "hand_right"));
}
