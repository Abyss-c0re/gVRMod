#include <vector>
#include <string>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

#include <GL/gl.h>

#include <gmod/Interface.h>

#include "core/vrmod_common.h"
#include "core/vrmod_log.h"
#include "input/xr_input.h"
#include "rendering/opengl/gl_hooks.h"
#include "rendering/openxr/xr_session.h"
#include "rendering/openxr/xr_render.h"

// ── Lua-side state ──
static char g_errorString[MAX_STR_LEN];
static int  g_luaRefs[LuaRefIndex_Max];
static int  g_luaRefCount = 0;
static bool g_IsPaused = false;
static bool g_xrInitialized = false;
static bool g_xrSwapchainsCreated = false;

// ── Texture bounds (stored for Lua compatibility; legacy side-by-side path only) ──
static float g_texBounds[8] = {0}; // left uMin,vMin,uMax,vMax, right uMin,vMin,uMax,vMax

// Forward decls from gl_hooks for per-eye exposure in submit
extern GLuint g_leftEyeTexture;
extern GLuint g_rightEyeTexture;
extern GLuint g_leftEyeFBO;
extern GLuint g_leftEyeColorTex;
extern GLuint g_rightEyeFBO;
extern GLuint g_rightEyeColorTex;

extern bool g_rtTextureNeedsVFlip;

// Eye poses from OpenXR (to let headset values drive the per-eye cameras in Lua with minimal manual math)
extern PoseResult g_xrHMDPose;
extern PoseResult ConvertXrPose(const XrSpaceLocation& loc);
PoseResult g_xrEyePoses[2];
bool g_xrEyePosesValid = false;
XrFovf g_xrEyeFovs[2];

// ── Action state ──
static action    g_actions[MAX_ACTIONS];
static int       g_actionCount = 0;
static actionSet g_actionSets[MAX_ACTIONSETS];
static int       g_actionSetCount = 0;
static actionSet g_activeActionSets[MAX_ACTIONSETS];
static int       g_activeActionSetCount = 0;

// ── Helpers ──

// LUA pointer stashed for log forwarding
static GarrysMod::Lua::ILuaBase* g_luaForPrint = nullptr;

static void LuaPrint(GarrysMod::Lua::ILuaBase* LUA, const char* msg) {
    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField(-1, "print");
    LUA->PushString(msg);
    LUA->Call(1, 0);
    LUA->Pop(1);
}

static void LogPrintBridge(const char* msg) {
    // This is called from vrmod_log_write. We can't safely call Lua from
    // arbitrary threads, so just let the file logging handle it.
    // The Lua print happens explicitly in error paths.
}

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

// ── LUA_FUNCTIONs ──
// All function signatures and return values are preserved for Lua API compatibility.

LUA_FUNCTION(GetVersion) {
    LUA->PushNumber(23);
    return 1;
}

LUA_FUNCTION(IsHMDPresent) {
    LUA->PushBool(XR_IsHMDPresent());
    return 1;
}

LUA_FUNCTION(Init) {
    if (g_xrInitialized) {
        if (g_IsPaused) {
            LUA->CreateTable();
            g_luaRefs[LuaRefIndex_PoseTable] = LUA->ReferenceCreate();

            LUA->CreateTable();
            g_luaRefs[LuaRefIndex_EmptyTable] = LUA->ReferenceCreate();

            LUA->CreateTable();
            g_luaRefs[LuaRefIndex_ActionTable] = LUA->ReferenceCreate();

            LUA->CreateTable();
            g_luaRefs[LuaRefIndex_HmdPose] = LUA->ReferenceCreate();
            g_IsPaused = false;
            VRMOD_LOG_INFO("Resumed from paused state.");
            return 0;
        }
        return 0;
    }

    char errMsg[MAX_STR_LEN];
    if (!XR_Init(errMsg, MAX_STR_LEN)) {
        // Make sure the real reason is visible even if the Lua caller doesn't pcall.
        VRMOD_LOG_ERROR("Init failed: %s", errMsg);
        // Prepend a hint so users know where to look when this bubbles up as a red error.
        char full[ MAX_STR_LEN + 256 ];
        snprintf(full, sizeof(full),
            "%s\n\nSee garrysmod/vrmod_debug.log (and data/vrmod_logs/) for details.\n"
            "On Linux this is often caused by libopenxr_loader.so not being loadable\n"
            "(missing package, Steam runtime library path, or no active_runtime.json).",
            errMsg[0] ? errMsg : "unknown OpenXR error");
        LUA->ThrowError(full);
        return 0;
    }

    // Poll events to get session state to READY
    // The session needs a few event pump cycles
    for (int attempt = 0; attempt < 100 && !g_xrSessionRunning; attempt++) {
        XR_PollEvents();
        if (g_xrSessionRunning) break;
        usleep(10000); // 10ms
    }
    if (!g_xrSessionRunning) {
        VRMOD_LOG_WARN("Session not running after init, will retry on first frame");
    }

    g_xrInitialized = true;

    memset(g_luaRefs, 0, sizeof(g_luaRefs));
    g_luaRefCount = 0;
    for (int i = 0; i < LuaRefIndex_Max; i++) {
        LUA->CreateTable();
        g_luaRefs[i] = LUA->ReferenceCreate();
        g_luaRefCount++;
    }

    // Get GL entry points for texture hook
    void* lib = dlopen("libtogl_client.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib) LUA->ThrowError("VRMOD: dlopen failed");

    auto GetOpenGLEntryPoints = reinterpret_cast<GetOpenGLEntryPoints_t>(dlsym(lib, "GetOpenGLEntryPoints"));
    if (!GetOpenGLEntryPoints) LUA->ThrowError("VRMOD: dlsym failed");

    g_GL = GetOpenGLEntryPoints(nullptr);
    dlclose(lib);

    g_createTexture = *((void**)&g_GL->firstFunc + 50);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        char buf[128];
        snprintf(buf, sizeof(buf), "VRMOD: OpenGL error: %u", err);
        LUA->ThrowError(buf);
        return 0;
    }

    VRMOD_LOG_INFO("VR initialized successfully (OpenXR).");
    return 0;
}

LUA_FUNCTION(SetActionManifest) {
    const char* fileName = LUA->CheckString(1);
    char path[PATH_MAX];
    char currentDir[PATH_MAX];
    if (getcwd(currentDir, PATH_MAX) == NULL)
        LUA->ThrowError("VRMOD: getcwd failed");
    if (snprintf(path, PATH_MAX, "%s/garrysmod/data/%s", currentDir, fileName) >= PATH_MAX)
        LUA->ThrowError("VRMOD: SetActionManifest path too long");

    int result = XR_ParseActionManifest(path, g_actions, MAX_ACTIONS);
    if (result == -1)
        LUA->ThrowError("VRMOD: SetActionManifestPath failed");
    if (result == -2)
        LUA->ThrowError("VRMOD: failed to open action manifest");

    g_actionCount = result;

    // Attach action sets to session
    if (!XR_AttachActionSets()) {
        LuaPrint(LUA, "[VRMOD] Warning: Failed to attach action sets (input may not work)");
    }

    // Create lua refs for actions that need them
    for (int i = 0; i < g_actionCount; i++) {
        for (int j = 0; j < 2; j++) {
            LUA->CreateTable();
            g_actions[i].luaRefs[j] = LUA->ReferenceCreate();
        }
    }
    return 0;
}

LUA_FUNCTION(SetActiveActionSets) {
    g_activeActionSetCount = 0;
    for (int i = 0; i < MAX_ACTIONSETS; i++) {
        if (LUA->GetType(i + 1) == GarrysMod::Lua::Type::STRING) {
            const char* actionSetName = LUA->CheckString(i + 1);
            int actionSetIndex = XR_FindOrCreateActionSet(
                actionSetName, g_actionSets, &g_actionSetCount);
            g_activeActionSets[g_activeActionSetCount] = g_actionSets[actionSetIndex];
            g_activeActionSetCount++;
        } else {
            break;
        }
    }
    return 0;
}

LUA_FUNCTION(GetDisplayInfo) {
    float fNearZ = (float)LUA->CheckNumber(1);
    float fFarZ = (float)LUA->CheckNumber(2);

    // Ensure session is running
    if (!g_xrSessionRunning) {
        XR_PollEvents();
    }

    // Do a wait/begin frame cycle to get valid display time
    if (g_xrSessionRunning) {
        XR_WaitAndBeginFrame();
    }

    XrDisplayInfo di;
    if (!XR_GetDisplayInfo(fNearZ, fFarZ, &di)) {
        // Return defaults if display info not available yet
        LUA->CreateTable();
        float identity4x4[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        float identity3x4[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
        PushMatrixAsTable(LUA, identity4x4, 4, 4);
        LUA->SetField(-2, "ProjectionLeft");
        PushMatrixAsTable(LUA, identity4x4, 4, 4);
        LUA->SetField(-2, "ProjectionRight");
        PushMatrixAsTable(LUA, identity3x4, 3, 4);
        LUA->SetField(-2, "TransformLeft");
        PushMatrixAsTable(LUA, identity3x4, 3, 4);
        LUA->SetField(-2, "TransformRight");
        LUA->PushNumber(g_xrRecommendedWidth > 0 ? g_xrRecommendedWidth : 1024);
        LUA->SetField(-2, "RecommendedWidth");
        LUA->PushNumber(g_xrRecommendedHeight > 0 ? g_xrRecommendedHeight : 1024);
        LUA->SetField(-2, "RecommendedHeight");
        // End the frame we began
        if (g_xrSessionRunning) XR_EndFrame();
        return 1;
    }

    // End the frame we began for display info query
    if (g_xrSessionRunning) XR_EndFrame();

    LUA->CreateTable();
    PushMatrixAsTable(LUA, di.projLeft, 4, 4);
    LUA->SetField(-2, "ProjectionLeft");
    PushMatrixAsTable(LUA, di.projRight, 4, 4);
    LUA->SetField(-2, "ProjectionRight");
    PushMatrixAsTable(LUA, di.transformLeft, 3, 4);
    LUA->SetField(-2, "TransformLeft");
    PushMatrixAsTable(LUA, di.transformRight, 3, 4);
    LUA->SetField(-2, "TransformRight");
    LUA->PushNumber(di.recommendedWidth);
    LUA->SetField(-2, "RecommendedWidth");
    LUA->PushNumber(di.recommendedHeight);
    LUA->SetField(-2, "RecommendedHeight");
    return 1;
}

LUA_FUNCTION(UpdatePosesAndActions) {
    if (!g_xrInitialized) return 0;

    // Poll events
    XR_PollEvents();

    if (!g_xrSessionRunning) return 0;

    // Wait and begin frame (this blocks until next VR frame)
    XR_WaitAndBeginFrame();

    // Sync actions
    XR_SyncActions(g_activeActionSets, g_activeActionSetCount);

    // Update HMD pose
    XR_UpdatePoses();

    // Locate eye views early using predicted display time (in stage space, matching hmd pose space).
    // This lets Lua use the exact headset-provided eye poses (position + orientation) for the
    // per-eye RenderViews, so OpenXR drives the stereo camera placement directly with no
    // additional manual offset/yaw/scale math in the render path.
    if (g_xrSessionRunning && g_xrLocateViews) {
      XrViewLocateInfo vli = {XR_TYPE_VIEW_LOCATE_INFO};
      vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
      vli.displayTime = g_xrFrameState.predictedDisplayTime;
      vli.space = g_xrStageSpace;

      XrViewState vs = {XR_TYPE_VIEW_STATE};
      uint32_t vc = 0;
      XrView ev[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
      if (g_xrLocateViews(g_xrSession, &vli, &vs, 2, &vc, ev) == XR_SUCCESS && vc >= 2) {
        XrSpaceLocation tloc = {XR_TYPE_SPACE_LOCATION};
        tloc.locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        tloc.pose = ev[0].pose;
        g_xrEyePoses[0] = ConvertXrPose(tloc);
        tloc.pose = ev[1].pose;
        g_xrEyePoses[1] = ConvertXrPose(tloc);
        g_xrEyeFovs[0] = ev[0].fov;
        g_xrEyeFovs[1] = ev[1].fov;
        g_xrEyePosesValid = true;
      }
    }

    return 0;
}

LUA_FUNCTION(GetPoses) {
    LUA->ReferencePush(g_luaRefs[LuaRefIndex_PoseTable]);

    // HMD pose
    PoseResult pr = g_xrHMDPose;
    if (pr.valid) {
        Vector pos; pos.x = pr.pos[0]; pos.y = pr.pos[1]; pos.z = pr.pos[2];
        Vector vel; vel.x = pr.vel[0]; vel.y = pr.vel[1]; vel.z = pr.vel[2];
        QAngle ang; ang.x = pr.ang[0]; ang.y = pr.ang[1]; ang.z = pr.ang[2];
        QAngle angvel; angvel.x = pr.angvel[0]; angvel.y = pr.angvel[1]; angvel.z = pr.angvel[2];
        LUA->ReferencePush(g_luaRefs[LuaRefIndex_HmdPose]);
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

    // Eye poses directly from OpenXR (headset values for per-eye cameras)
    if (g_xrEyePosesValid) {
      for (int ei = 0; ei < 2; ei++) {
        PoseResult pr = g_xrEyePoses[ei];
        if (pr.valid) {
          Vector pos; pos.x = pr.pos[0]; pos.y = pr.pos[1]; pos.z = pr.pos[2];
          Vector vel; vel.x = 0; vel.y = 0; vel.z = 0;
          QAngle ang; ang.x = pr.ang[0]; ang.y = pr.ang[1]; ang.z = pr.ang[2];
          QAngle angvel; angvel.x = 0; angvel.y = 0; angvel.z = 0;
          const char* ename = (ei == 0) ? "eye_left" : "eye_right";
          LUA->CreateTable();
          LUA->PushVector(pos); LUA->SetField(-2, "pos");
          LUA->PushVector(vel); LUA->SetField(-2, "vel");
          LUA->PushAngle(ang); LUA->SetField(-2, "ang");
          LUA->PushAngle(angvel); LUA->SetField(-2, "angvel");

          // Symmetric overrender FOV that fully encloses the asymmetric OpenXR frustum.
          // Source engine's render.RenderView creates a symmetric frustum from fov + aspectratio.
          // To handle the asymmetric OpenXR FOV correctly:
          //  1. Overrender with symmetric FOV = 2*atan(max(|tanL|,|tanR|)) per axis
          //  2. Submit UV bounds that select the correct asymmetric sub-rect
          //  3. Both eyes use the same head orientation (no per-eye rotation)
          // This matches the legacy OpenVR approach that produced correct results.
          float tanL = tanf(g_xrEyeFovs[ei].angleLeft);
          float tanR = tanf(g_xrEyeFovs[ei].angleRight);
          float tanU = tanf(g_xrEyeFovs[ei].angleUp);
          float tanD = tanf(g_xrEyeFovs[ei].angleDown);

          float halfTanX = fmaxf(fabsf(tanL), fabsf(tanR));
          float halfTanY = fmaxf(fabsf(tanU), fabsf(tanD));
          float asp = halfTanX / halfTanY;
          float h_fov = 2.0f * atanf(halfTanX) * (180.0f / 3.14159265358979323846f);

          LUA->PushNumber(h_fov); LUA->SetField(-2, "fov");
          LUA->PushNumber(asp); LUA->SetField(-2, "aspectratio");

          // UV crop bounds for selecting the asymmetric frustum from the symmetric
          // overrender. Convention: u0 < u1, v0 < v1 (D3D style: v0 = top of crop).
          // The V-flip for OpenGL render targets is handled in xr_render.cpp.
          float su0 = (tanL + halfTanX) / (2.0f * halfTanX);
          float su1 = (tanR + halfTanX) / (2.0f * halfTanX);
          float sv0 = (halfTanY - tanU) / (2.0f * halfTanY);
          float sv1 = (halfTanY - tanD) / (2.0f * halfTanY);

          LUA->PushNumber(su0); LUA->SetField(-2, "submit_u0");
          LUA->PushNumber(su1); LUA->SetField(-2, "submit_u1");
          LUA->PushNumber(sv0); LUA->SetField(-2, "submit_v0");
          LUA->PushNumber(sv1); LUA->SetField(-2, "submit_v1");

          LUA->SetField(-2, ename);
        }
      }
    }

    // Action poses
    for (int i = 0; i < g_actionCount; i++) {
        if (g_actions[i].type == ActionType_Pose) {
            pr = XR_GetPoseAction(g_actions[i].handle);
            if (pr.valid) {
                Vector pos; pos.x = pr.pos[0]; pos.y = pr.pos[1]; pos.z = pr.pos[2];
                Vector vel; vel.x = pr.vel[0]; vel.y = pr.vel[1]; vel.z = pr.vel[2];
                QAngle ang; ang.x = pr.ang[0]; ang.y = pr.ang[1]; ang.z = pr.ang[2];
                QAngle angvel; angvel.x = pr.angvel[0]; angvel.y = pr.angvel[1]; angvel.z = pr.angvel[2];
                LUA->ReferencePush(g_actions[i].luaRefs[0]);
                LUA->PushVector(pos);
                LUA->SetField(-2, "pos");
                LUA->PushVector(vel);
                LUA->SetField(-2, "vel");
                LUA->PushAngle(ang);
                LUA->SetField(-2, "ang");
                LUA->PushAngle(angvel);
                LUA->SetField(-2, "angvel");
                LUA->SetField(-2, g_actions[i].name);
            }
        }
    }
    return 1;
}

LUA_FUNCTION(GetActions) {
    char* changedActionNames[MAX_ACTIONS];
    bool changedActionStates[MAX_ACTIONS];
    int changedActionCount = 0;

    LUA->ReferencePush(g_luaRefs[LuaRefIndex_ActionTable]);

    for (int i = 0; i < g_actionCount; i++) {
        if (g_actions[i].type == ActionType_Boolean) {
            bool changed = false;
            bool state = XR_GetBooleanAction(g_actions[i].handle, &changed);
            LUA->PushBool(state);
            LUA->SetField(-2, g_actions[i].name);
            if (changed) {
                changedActionNames[changedActionCount] = g_actions[i].name;
                changedActionStates[changedActionCount] = state;
                changedActionCount++;
            }
        }
        else if (g_actions[i].type == ActionType_Vector1) {
            float val = XR_GetFloatAction(g_actions[i].handle);
            LUA->PushNumber(val);
            LUA->SetField(-2, g_actions[i].name);
        }
        else if (g_actions[i].type == ActionType_Vector2) {
            float x, y;
            XR_GetVector2Action(g_actions[i].handle, &x, &y);
            LUA->ReferencePush(g_actions[i].luaRefs[0]);
            LUA->PushNumber(x);
            LUA->SetField(-2, "x");
            LUA->PushNumber(y);
            LUA->SetField(-2, "y");
            LUA->SetField(-2, g_actions[i].name);
        }
        else if (g_actions[i].type == ActionType_Skeleton) {
            // OpenXR doesn't have skeletal summary in the same way.
            // For PoC, push zeroed finger curls.
            LUA->ReferencePush(g_actions[i].luaRefs[0]);
            LUA->ReferencePush(g_actions[i].luaRefs[1]);
            for (int j = 0; j < 5; j++) {
                LUA->PushNumber(j + 1);
                LUA->PushNumber(0.0);
                LUA->SetTable(-3);
            }
            LUA->SetField(-2, "fingerCurls");
            LUA->SetField(-2, g_actions[i].name);
        }
    }

    if (changedActionCount == 0) {
        LUA->ReferencePush(g_luaRefs[LuaRefIndex_EmptyTable]);
    } else {
        LUA->CreateTable();
        for (int i = 0; i < changedActionCount; i++) {
            LUA->PushBool(changedActionStates[i]);
            LUA->SetField(-2, changedActionNames[i]);
        }
    }
    return 2;
}

LUA_FUNCTION(ShareTextureBegin) {
    // Error bridge
    auto errBridge = [](const char* msg) {
        VRMOD_LOG_ERROR("%s", msg);
    };

    // Per-eye sizes (new path). Lua now creates one RT per eye at recommended resolution.
    uint32_t texW = g_xrRecommendedWidth > 0 ? g_xrRecommendedWidth : 1024;
    uint32_t texH = g_xrRecommendedHeight > 0 ? g_xrRecommendedHeight : 1024;

    int rc = ::ShareTextureBegin(texW, texH, errBridge);
    if (rc != 0) {
        LUA->ThrowError("VRMOD: mprotect RWX failed");
    }

    // Ensure we have an XrSession bound to the *current* render GLX context (this is the
    // moment the main game context for the VR RTs is active). Doing the create here (deferred
    // from XR_Init) makes the textures we later blit from valid names in the session's context.
    if (!g_xrSession) {
        char serr[MAX_STR_LEN];
        if (!XR_CreateSessionWithCurrentGL(serr, MAX_STR_LEN)) {
            LuaPrint(LUA, serr);
            VRMOD_LOG_ERROR("Deferred session create failed: %s", serr);
        } else {
            // Drive events so we reach READY / running quickly.
            for (int i = 0; i < 60 && !g_xrSessionRunning; ++i) {
                XR_PollEvents();
                usleep(3000);
            }
        }
    }

    // Create OpenXR swapchains if not already done
    if (!g_xrSwapchainsCreated && g_xrSessionRunning) {
        char errMsg[MAX_STR_LEN];
        if (XR_CreateSwapchains(errMsg, MAX_STR_LEN)) {
            g_xrSwapchainsCreated = true;
        } else {
            LuaPrint(LUA, errMsg);
            VRMOD_LOG_ERROR("Failed to create swapchains: %s", errMsg);
        }
    }

    return 0;
}

LUA_FUNCTION(ShareTextureFinish) {
    auto errBridge = [](const char* msg) {
        VRMOD_LOG_ERROR("%s", msg);
    };

    if (!RemoveTexturePatch(errBridge)) {
        LUA->ThrowError("VRMOD: Failed to remove the texture patch.");
    }

    // Promote per-eye textures discovered via FBO COLOR_ATTACHMENT0 (most authoritative for GetRenderTargetEx backing stores).
    if (g_leftEyeColorTex != 0 && glIsTexture(g_leftEyeColorTex)) {
        if (g_leftEyeTexture != g_leftEyeColorTex) {
            VRMOD_LOG_INFO("Promoting left eye color tex from FBO: %u (was %u)", g_leftEyeColorTex, g_leftEyeTexture);
            g_leftEyeTexture = g_leftEyeColorTex;
        }
    }
    if (g_rightEyeColorTex != 0 && glIsTexture(g_rightEyeColorTex)) {
        if (g_rightEyeTexture != g_rightEyeColorTex) {
            VRMOD_LOG_INFO("Promoting right eye color tex from FBO: %u (was %u)", g_rightEyeColorTex, g_rightEyeTexture);
            g_rightEyeTexture = g_rightEyeColorTex;
        }
    }

    // Also keep legacy shared promotion for fallback paths.
    if (g_vrRtColorTex != 0 && glIsTexture(g_vrRtColorTex)) {
        if (g_sharedTexture != g_vrRtColorTex) {
            VRMOD_LOG_INFO("Promoting VR RT color texture from FBO attach: %u (was %u)", g_vrRtColorTex, g_sharedTexture);
            g_sharedTexture = g_vrRtColorTex;
        }
    }

    // Log what we ended up with for the per-eye path.
    VRMOD_LOG_INFO("Per-eye textures ready: L=%u (fbo=%u) R=%u (fbo=%u) | legacy shared=%u",
        g_leftEyeTexture, g_leftEyeFBO, g_rightEyeTexture, g_rightEyeFBO, g_sharedTexture);
    return 0;
}

LUA_FUNCTION(ShareCaptureTextureBegin) {
    auto errBridge = [](const char* msg) {
        VRMOD_LOG_ERROR("%s", msg);
    };

    // g_xrRecommended* is the per-eye value (keeps main Share *2 logic working).
    // Capture RT in Lua is allocated at the *packed* size (2x wide) so we pass 2x here.
    // Our ShareCapture impl treats the passed size as the final tex size (no extra *2).
    uint32_t eyeW = g_xrRecommendedWidth > 0 ? g_xrRecommendedWidth : 1024;
    uint32_t eyeH = g_xrRecommendedHeight > 0 ? g_xrRecommendedHeight : 1024;
    uint32_t capW = eyeW * 2;
    uint32_t capH = eyeH;

    int rc = ::ShareCaptureTextureBegin(capW, capH, errBridge);
    if (rc != 0) {
        LUA->ThrowError("VRMOD: ShareCaptureTextureBegin failed");
    }
    return 0;
}

LUA_FUNCTION(ShareCaptureTextureFinish) {
    if (g_captureTexture == 0 || !glIsTexture(g_captureTexture)) {
        LUA->ThrowError("VRMOD: Failed to generate capture texture.");
        return 0;
    }

    auto errBridge = [](const char* msg) {
        VRMOD_LOG_ERROR("%s", msg);
    };
    // Unpatch + disarm capture steal so we don't hook random future glGen calls.
    ::ShareCaptureTextureFinish(errBridge);

    VRMOD_LOG_INFO("Capture texture ready: GL id=%u", g_captureTexture);
    return 0;
}

LUA_FUNCTION(SetSubmitTextureBounds) {
    g_texBounds[0] = (float)LUA->CheckNumber(1);  // left uMin
    g_texBounds[1] = (float)LUA->CheckNumber(2);  // left vMin
    g_texBounds[2] = (float)LUA->CheckNumber(3);  // left uMax
    g_texBounds[3] = (float)LUA->CheckNumber(4);  // left vMax

    g_texBounds[4] = (float)LUA->CheckNumber(5);  // right uMin
    g_texBounds[5] = (float)LUA->CheckNumber(6);  // right vMin
    g_texBounds[6] = (float)LUA->CheckNumber(7);  // right uMax
    g_texBounds[7] = (float)LUA->CheckNumber(8);  // right vMax

    return 0;
}

LUA_FUNCTION(SetRTTextureFlip) {
    // true = the engine RT textures need V flip when submitting (Linux/OpenGL case)
    // Driven by Lua: VRMOD_SetRTTextureFlip( not system.IsWindows() )
    if (LUA->GetType(1) == GarrysMod::Lua::Type::BOOL) {
        g_rtTextureNeedsVFlip = LUA->GetBool(1);
    } else {
        // Fallback: on unknown, assume we need the flip (typical Linux/OpenGL case)
        g_rtTextureNeedsVFlip = true;
    }
    return 0;
}

LUA_FUNCTION(SubmitSharedTexture) {
    // Usable source: prefer per-eye textures (new path), fall back to legacy single shared/FBO.
    bool haveLeft = (g_leftEyeTexture != 0 && glIsTexture(g_leftEyeTexture)) || (g_leftEyeColorTex != 0 && glIsTexture(g_leftEyeColorTex)) || (g_leftEyeFBO != 0);
    bool haveRight = (g_rightEyeTexture != 0 && glIsTexture(g_rightEyeTexture)) || (g_rightEyeColorTex != 0 && glIsTexture(g_rightEyeColorTex)) || (g_rightEyeFBO != 0);
    bool haveLegacy = (g_sharedTexture != 0) ||
                      (g_vrRtColorTex != 0 && glIsTexture(g_vrRtColorTex)) ||
                      (g_vrRtFBO != 0);
    bool haveUsableSrc = haveLeft || haveRight || haveLegacy;
    if (!g_xrSwapchainsCreated || !haveUsableSrc) {
        if (g_xrSessionRunning && !g_xrSwapchainsCreated) {
            XR_EndFrame();
        }
        return 0;
    }

    // We pass a best-effort single id (Submit will ignore it when per-eye globals are populated).
    GLuint submitId = 0;
    if (haveLeft || haveRight) {
        submitId = g_leftEyeTexture ? g_leftEyeTexture : (g_leftEyeColorTex ? g_leftEyeColorTex : g_rightEyeTexture);
    }
    if (submitId == 0) {
        submitId = g_sharedTexture ? g_sharedTexture :
                   (g_vrRtColorTex && glIsTexture(g_vrRtColorTex) ? g_vrRtColorTex : g_sharedTexture);
    }
    XrSubmitResult res = XR_SubmitStolenTexture(submitId, g_texBounds);
    if (!res.ok && res.errMsg[0]) {
        LuaPrint(LUA, res.errMsg);
    }
    return 0;
}

LUA_FUNCTION(Shutdown) {
    if (!g_xrInitialized)
        return 0;
    if (g_IsPaused)
        return 0;

    glFlush();
    glFinish();

    // Free Lua references
    for (int i = 0; i < g_luaRefCount; i++) {
        if (g_luaRefs[i] != 0) {
            LUA->ReferenceFree(g_luaRefs[i]);
            g_luaRefs[i] = 0;
        }
    }
    for (int i = 0; i < g_actionCount; i++) {
        for (int j = 0; j < 2; j++) {
            if (g_actions[i].luaRefs[j] != 0) {
                LUA->ReferenceFree(g_actions[i].luaRefs[j]);
                g_actions[i].luaRefs[j] = 0;
            }
        }
    }
    g_luaRefCount = LuaRefIndex_Max;
    g_actionCount = 0;
    memset(g_actions, 0, sizeof(g_actions));
    g_actionSetCount = 0;
    g_activeActionSetCount = 0;
    g_IsPaused = true;

    // Destroy swapchains
    if (g_xrSwapchainsCreated) {
        XR_DestroySwapchains();
        g_xrSwapchainsCreated = false;
    }

    VRMOD_LOG_INFO("VR shutdown (paused).");
    return 0;
}

LUA_FUNCTION(TriggerHaptic) {
    const char* actionName = LUA->CheckString(1);
    VRActionHandle handle = XR_FindActionHandleByName(actionName, g_actions, g_actionCount);
    if (handle != VRMOD_INVALID_ACTION_HANDLE) {
        XR_TriggerHaptic(handle,
            (float)LUA->CheckNumber(2),
            (float)LUA->CheckNumber(3),
            (float)LUA->CheckNumber(4),
            (float)LUA->CheckNumber(5));
    }
    return 0;
}

LUA_FUNCTION(GetTrackedDeviceNames) {
    // OpenXR doesn't have the same tracked device enumeration.
    // For PoC, return a table with the controller interaction profiles.
    LUA->CreateTable();

    // Return a basic list indicating Quest 3 controllers
    LUA->PushNumber(1);
    LUA->PushString("meta_quest_touch_pro");
    LUA->SetTable(-3);

    return 1;
}

// ── Module entry points ──

GMOD_MODULE_OPEN() {
    VRMOD_LOG_INIT("vrmod_debug.log");
    VRMOD_LOG_INFO("Module loading (OpenXR backend)...");

    // Set up log forwarding to client console
    vrmod_log_set_print(LogPrintBridge);

    LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField(-1, "vrmod");
    if (!LUA->IsType(-1, GarrysMod::Lua::Type::TABLE)) {
        LUA->Pop(1);
        LUA->CreateTable();
    }
    LUA->PushCFunction(GetVersion);
    LUA->SetField(-2, "GetVersion");
    LUA->PushCFunction(IsHMDPresent);
    LUA->SetField(-2, "IsHMDPresent");
    LUA->PushCFunction(Init);
    LUA->SetField(-2, "Init");
    LUA->PushCFunction(SetActionManifest);
    LUA->SetField(-2, "SetActionManifest");
    LUA->PushCFunction(SetActiveActionSets);
    LUA->SetField(-2, "SetActiveActionSets");
    LUA->PushCFunction(GetDisplayInfo);
    LUA->SetField(-2, "GetDisplayInfo");
    LUA->PushCFunction(UpdatePosesAndActions);
    LUA->SetField(-2, "UpdatePosesAndActions");
    LUA->PushCFunction(GetPoses);
    LUA->SetField(-2, "GetPoses");
    LUA->PushCFunction(GetActions);
    LUA->SetField(-2, "GetActions");
    LUA->PushCFunction(ShareTextureBegin);
    LUA->SetField(-2, "ShareTextureBegin");
    LUA->PushCFunction(ShareTextureFinish);
    LUA->SetField(-2, "ShareTextureFinish");
    LUA->PushCFunction(ShareCaptureTextureBegin);
    LUA->SetField(-2, "ShareCaptureTextureBegin");
    LUA->PushCFunction(ShareCaptureTextureFinish);
    LUA->SetField(-2, "ShareCaptureTextureFinish");
    LUA->PushCFunction(SetSubmitTextureBounds);
    LUA->SetField(-2, "SetSubmitTextureBounds");
    LUA->PushCFunction(SetRTTextureFlip);
    LUA->SetField(-2, "SetRTTextureFlip");
    LUA->PushCFunction(SubmitSharedTexture);
    LUA->SetField(-2, "SubmitSharedTexture");
    LUA->PushCFunction(Shutdown);
    LUA->SetField(-2, "Shutdown");
    LUA->PushCFunction(TriggerHaptic);
    LUA->SetField(-2, "TriggerHaptic");
    LUA->PushCFunction(GetTrackedDeviceNames);
    LUA->SetField(-2, "GetTrackedDeviceNames");
    LUA->SetField(-2, "vrmod");

    VRMOD_LOG_INFO("Module loaded (OpenXR).");
    return 0;
}

GMOD_MODULE_CLOSE() {
    VRMOD_LOG_INFO("Module closing.");

    // Full shutdown if not paused
    if (g_xrInitialized && !g_IsPaused) {
        if (g_xrSwapchainsCreated) {
            XR_DestroySwapchains();
            g_xrSwapchainsCreated = false;
        }
        XR_Shutdown();
        g_xrInitialized = false;
    } else if (g_xrInitialized) {
        // Was paused, do full shutdown now
        if (g_xrSwapchainsCreated) {
            XR_DestroySwapchains();
            g_xrSwapchainsCreated = false;
        }
        XR_Shutdown();
        g_xrInitialized = false;
    }

    VRMOD_LOG_CLOSE();
    return 0;
}
