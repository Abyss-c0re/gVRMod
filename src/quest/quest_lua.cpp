// Thin GMod module: gVRLink / Quest host backend.
// Does NOT open local OpenXR. WiVRn/OpenXR stays in gmcl_vrmod_xr_*.
// require("vrmod_quest") → gmcl_vrmod_quest_linux64.dll

#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <gmod/Interface.h>

#include "core/vrmod_common.h"
#include "core/vrmod_log.h"
#include "quest/quest_link.h"

static char g_errorString[MAX_STR_LEN];
static int g_luaRefs[LuaRefIndex_Max];
static int g_luaRefCount = 0;
static bool g_initialized = false;
static bool g_submitEnabled = true;
static float g_texBounds[8] = {0};
static uint32_t g_knownW = 0, g_knownH = 0;
static int g_cropMode = 0;
static bool g_rtFlip = true;

static questlink::Link g_link;
static PoseResult g_hmd{};
static PoseResult g_eyeL{};
static PoseResult g_eyeR{};
static PoseResult g_handL{};
static PoseResult g_handR{};

static action g_actions[MAX_ACTIONS];
static int g_actionCount = 0;
static actionSet g_actionSets[MAX_ACTIONSETS];
static int g_actionSetCount = 0;

static void LogPrintBridge(const char*) {}

static void RefreshPosesFromLink() {
  questlink::PoseSample s;
  if (!g_link.GetPose(s) || !s.valid) return;
  const auto& p = s.hdr;

  float src[3], ang[3];
  questlink::PosOpenXrToSource(p.hmd_pos, src);
  questlink::QuatToSourceAng(p.hmd_quat, ang);
  g_hmd.pos[0] = src[0];
  g_hmd.pos[1] = src[1];
  g_hmd.pos[2] = src[2];
  g_hmd.ang[0] = ang[0];
  g_hmd.ang[1] = ang[1];
  g_hmd.ang[2] = ang[2];
  g_hmd.valid = true;

  questlink::PosOpenXrToSource(p.eye_l_pos, src);
  g_eyeL.pos[0] = src[0];
  g_eyeL.pos[1] = src[1];
  g_eyeL.pos[2] = src[2];
  g_eyeL.ang[0] = ang[0];
  g_eyeL.ang[1] = ang[1];
  g_eyeL.ang[2] = ang[2];
  g_eyeL.valid = true;

  questlink::PosOpenXrToSource(p.eye_r_pos, src);
  g_eyeR.pos[0] = src[0];
  g_eyeR.pos[1] = src[1];
  g_eyeR.pos[2] = src[2];
  g_eyeR.ang[0] = ang[0];
  g_eyeR.ang[1] = ang[1];
  g_eyeR.ang[2] = ang[2];
  g_eyeR.valid = true;

  questlink::PosOpenXrToSource(p.hand_l_pos, src);
  questlink::QuatToSourceAng(p.hand_l_quat, ang);
  g_handL.pos[0] = src[0];
  g_handL.pos[1] = src[1];
  g_handL.pos[2] = src[2];
  g_handL.ang[0] = ang[0];
  g_handL.ang[1] = ang[1];
  g_handL.ang[2] = ang[2];
  g_handL.valid = true;

  questlink::PosOpenXrToSource(p.hand_r_pos, src);
  questlink::QuatToSourceAng(p.hand_r_quat, ang);
  g_handR.pos[0] = src[0];
  g_handR.pos[1] = src[1];
  g_handR.pos[2] = src[2];
  g_handR.ang[0] = ang[0];
  g_handR.ang[1] = ang[1];
  g_handR.ang[2] = ang[2];
  g_handR.valid = true;
}

static void PushPoseField(GarrysMod::Lua::ILuaBase* LUA, const char* name, const PoseResult& pr) {
  if (!pr.valid) return;
  Vector pos;
  pos.x = pr.pos[0];
  pos.y = pr.pos[1];
  pos.z = pr.pos[2];
  Vector vel;
  vel.x = pr.vel[0];
  vel.y = pr.vel[1];
  vel.z = pr.vel[2];
  QAngle ang;
  ang.x = pr.ang[0];
  ang.y = pr.ang[1];
  ang.z = pr.ang[2];
  QAngle angvel;
  angvel.x = pr.angvel[0];
  angvel.y = pr.angvel[1];
  angvel.z = pr.angvel[2];
  LUA->CreateTable();
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

LUA_FUNCTION(GetVersion) {
  LUA->PushNumber(100); // quest module family
  return 1;
}

LUA_FUNCTION(GetBackend) {
  LUA->PushString("quest");
  return 1;
}

LUA_FUNCTION(IsHMDPresent) {
  LUA->PushBool(g_link.HasRecentPose(2.0));
  return 1;
}

LUA_FUNCTION(Init) {
  if (g_initialized) {
    g_submitEnabled = true;
    return 0;
  }
  uint16_t port = questlink::kDefaultPosePort;
  const char* env = getenv("GVRMOD_QUEST_POSE_PORT");
  if (env && env[0]) {
    int p = atoi(env);
    if (p > 0 && p < 65536) port = (uint16_t)p;
  }
  if (!g_link.Start(port)) {
    LUA->ThrowError(
        "vrmod_quest: failed to bind pose UDP (GVRMOD_QUEST_POSE_PORT). "
        "OpenXR WiVRn path is unaffected — use vrmod_prefer_backend openxr.");
    return 0;
  }
  const char* host = getenv("GVRMOD_QUEST_HOST");
  if (host && host[0]) g_link.SetQuestHost(host);
  g_initialized = true;
  g_submitEnabled = true;
  VRMOD_LOG_INFO("vrmod_quest Init OK (pose listen %u)", port);
  return 0;
}

LUA_FUNCTION(SetActionManifest) {
  (void)LUA;
  return 0;
}

LUA_FUNCTION(SetActiveActionSets) {
  (void)LUA;
  return 0;
}

LUA_FUNCTION(GetDisplayInfo) {
  // Identity-ish until Quest CAP advertises FOV; Lua SoftRefresh tolerates this.
  LUA->CreateTable();
  auto pushId = [&](const char* name) {
    LUA->CreateTable();
    for (int r = 1; r <= 4; r++) {
      LUA->PushNumber(r);
      LUA->CreateTable();
      for (int c = 1; c <= 4; c++) {
        LUA->PushNumber(c);
        LUA->PushNumber(r == c ? 1.0 : 0.0);
        LUA->SetTable(-3);
      }
      LUA->SetTable(-3);
    }
    LUA->SetField(-2, name);
  };
  pushId("ProjectionLeft");
  pushId("ProjectionRight");
  pushId("TransformLeft");
  pushId("TransformRight");
  // Default Quest-ish eye size
  LUA->PushNumber(g_knownW > 0 ? g_knownW / 2 : 1440);
  LUA->SetField(-2, "RecommendedWidth");
  LUA->PushNumber(g_knownH > 0 ? g_knownH : 1584);
  LUA->SetField(-2, "RecommendedHeight");
  return 1;
}

LUA_FUNCTION(UpdatePosesAndActions) {
  RefreshPosesFromLink();
  return 0;
}

LUA_FUNCTION(GetPoses) {
  RefreshPosesFromLink();
  LUA->ReferencePush(g_luaRefs[LuaRefIndex_PoseTable]);

  if (g_hmd.valid) {
    LUA->ReferencePush(g_luaRefs[LuaRefIndex_HmdPose]);
    Vector pos;
    pos.x = g_hmd.pos[0];
    pos.y = g_hmd.pos[1];
    pos.z = g_hmd.pos[2];
    Vector vel;
    vel.x = vel.y = vel.z = 0;
    QAngle ang;
    ang.x = g_hmd.ang[0];
    ang.y = g_hmd.ang[1];
    ang.z = g_hmd.ang[2];
    QAngle angvel;
    angvel.x = angvel.y = angvel.z = 0;
    LUA->PushVector(pos);
    LUA->SetField(-2, "pos");
    LUA->PushVector(vel);
    LUA->SetField(-2, "vel");
    LUA->PushAngle(ang);
    LUA->SetField(-2, "ang");
    LUA->PushAngle(angvel);
    LUA->SetField(-2, "angvel");
    LUA->SetField(-2, "hmd");
  }

  PushPoseField(LUA, "eye_left", g_eyeL);
  PushPoseField(LUA, "eye_right", g_eyeR);
  PushPoseField(LUA, "pose_lefthand", g_handL);
  PushPoseField(LUA, "pose_righthand", g_handR);
  return 1;
}

LUA_FUNCTION(GetActions) {
  LUA->ReferencePush(g_luaRefs[LuaRefIndex_ActionTable]);
  return 1;
}

LUA_FUNCTION(GetControllerSources) {
  LUA->CreateTable();
  return 1;
}

LUA_FUNCTION(ShareTextureBegin) {
  (void)LUA;
  return 0;
}

LUA_FUNCTION(ShareTextureFinish) {
  LUA->PushBool(true);
  return 1;
}

LUA_FUNCTION(ShareCaptureTextureBegin) {
  (void)LUA;
  return 0;
}

LUA_FUNCTION(ShareCaptureTextureFinish) {
  LUA->PushBool(true);
  return 1;
}

LUA_FUNCTION(SetSubmitTextureBounds) {
  for (int i = 0; i < 8; i++) {
    if (LUA->IsType(i + 1, GarrysMod::Lua::Type::NUMBER))
      g_texBounds[i] = (float)LUA->GetNumber(i + 1);
  }
  return 0;
}

LUA_FUNCTION(SetRTTextureFlip) {
  if (LUA->IsType(1, GarrysMod::Lua::Type::BOOL)) g_rtFlip = LUA->GetBool(1);
  return 0;
}

LUA_FUNCTION(GLFinish) {
  return 0;
}

LUA_FUNCTION(SetKnownSubmitSize) {
  if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER)) g_knownW = (uint32_t)LUA->GetNumber(1);
  if (LUA->IsType(2, GarrysMod::Lua::Type::NUMBER)) g_knownH = (uint32_t)LUA->GetNumber(2);
  return 0;
}

LUA_FUNCTION(SetSubmitEnabled) {
  if (LUA->IsType(1, GarrysMod::Lua::Type::BOOL)) g_submitEnabled = LUA->GetBool(1);
  return 0;
}

LUA_FUNCTION(SetSubmitCropMode) {
  if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER)) g_cropMode = (int)LUA->GetNumber(1);
  return 0;
}

LUA_FUNCTION(GetSubmitCropMode) {
  LUA->PushNumber(g_cropMode);
  return 1;
}

LUA_FUNCTION(ShouldRender) {
  LUA->PushBool(g_submitEnabled && g_link.HasRecentPose(2.0));
  return 1;
}

LUA_FUNCTION(CollectEyes) {
  LUA->PushBool(false);
  return 1;
}

LUA_FUNCTION(HasCollectedEyes) {
  LUA->PushBool(false);
  return 1;
}

LUA_FUNCTION(SubmitSharedTexture) {
  // Phase 1: poses only. Texture TX lands when readback path is ready.
  // Must not throw — Lua frame path expects soft no-op.
  (void)LUA;
  return 0;
}

LUA_FUNCTION(Shutdown) {
  g_submitEnabled = false;
  g_link.Stop();
  g_initialized = false;
  g_hmd.valid = g_eyeL.valid = g_eyeR.valid = false;
  VRMOD_LOG_INFO("vrmod_quest Shutdown");
  return 0;
}

LUA_FUNCTION(TriggerHaptic) {
  (void)LUA;
  return 0;
}

LUA_FUNCTION(GetTrackedDeviceNames) {
  LUA->CreateTable();
  return 1;
}

// VR Keyboard stubs (same export names as XR module)
LUA_FUNCTION(VRKeyboardLayoutCount) {
  LUA->PushNumber(0);
  return 1;
}
LUA_FUNCTION(VRKeyboardGetLayout) {
  LUA->CreateTable();
  return 1;
}

GMOD_MODULE_OPEN() {
  VRMOD_LOG_INIT("vrmod_quest_debug.log");
  VRMOD_LOG_INFO("Module loading (Quest / gVRLink thin backend)...");
  vrmod_log_set_print(LogPrintBridge);

  g_luaRefCount = 0;
  LUA->CreateTable();
  g_luaRefs[LuaRefIndex_EmptyTable] = LUA->ReferenceCreate();
  LUA->CreateTable();
  g_luaRefs[LuaRefIndex_PoseTable] = LUA->ReferenceCreate();
  LUA->CreateTable();
  g_luaRefs[LuaRefIndex_HmdPose] = LUA->ReferenceCreate();
  LUA->CreateTable();
  g_luaRefs[LuaRefIndex_ActionTable] = LUA->ReferenceCreate();

  LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
  LUA->GetField(-1, "vrmod");
  if (!LUA->IsType(-1, GarrysMod::Lua::Type::TABLE)) {
    LUA->Pop(1);
    LUA->CreateTable();
  }

  auto set = [&](const char* name, GarrysMod::Lua::CFunc f) {
    LUA->PushCFunction(f);
    LUA->SetField(-2, name);
  };
  set("GetVersion", GetVersion);
  set("GetBackend", GetBackend);
  set("IsHMDPresent", IsHMDPresent);
  set("Init", Init);
  set("SetActionManifest", SetActionManifest);
  set("SetActiveActionSets", SetActiveActionSets);
  set("GetDisplayInfo", GetDisplayInfo);
  set("UpdatePosesAndActions", UpdatePosesAndActions);
  set("GetPoses", GetPoses);
  set("GetActions", GetActions);
  set("GetControllerSources", GetControllerSources);
  set("ShareTextureBegin", ShareTextureBegin);
  set("ShareTextureFinish", ShareTextureFinish);
  set("ShareCaptureTextureBegin", ShareCaptureTextureBegin);
  set("ShareCaptureTextureFinish", ShareCaptureTextureFinish);
  set("SetSubmitTextureBounds", SetSubmitTextureBounds);
  set("SetRTTextureFlip", SetRTTextureFlip);
  set("GLFinish", GLFinish);
  set("SetKnownSubmitSize", SetKnownSubmitSize);
  set("SetSubmitEnabled", SetSubmitEnabled);
  set("SetSubmitCropMode", SetSubmitCropMode);
  set("GetSubmitCropMode", GetSubmitCropMode);
  set("ShouldRender", ShouldRender);
  set("CollectEyes", CollectEyes);
  set("HasCollectedEyes", HasCollectedEyes);
  set("SubmitSharedTexture", SubmitSharedTexture);
  set("Shutdown", Shutdown);
  set("TriggerHaptic", TriggerHaptic);
  set("GetTrackedDeviceNames", GetTrackedDeviceNames);

  LUA->SetField(-2, "vrmod");
  LUA->Pop(1);

  g_errorString[0] = '\0';
  VRMOD_LOG_INFO("vrmod_quest exports ready (opt-in: vrmod_prefer_backend quest)");
  return 0;
}

GMOD_MODULE_CLOSE() {
  g_link.Stop();
  for (int i = 0; i < LuaRefIndex_Max; i++) {
    if (g_luaRefs[i]) {
      // refs freed with Lua state
      g_luaRefs[i] = 0;
    }
  }
  VRMOD_LOG_INFO("vrmod_quest closed");
  return 0;
}
