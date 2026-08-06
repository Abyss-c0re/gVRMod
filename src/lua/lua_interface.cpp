#include <vector>
#include <string>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <direct.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#define getcwd _getcwd
#else
#include <dlfcn.h>
#include <unistd.h>
#include <GL/gl.h>
#include <GL/glx.h>
#endif

#include <gmod/Interface.h>

#include "core/vrmod_common.h"
#include "core/vrmod_log.h"
#include "input/xr_input.h"
#include "rendering/texture_hooks.h"
#include "rendering/openxr/xr_session.h"
#include "rendering/openxr/xr_render.h"
#include "rendering/vdisplay/vdisplay.h"
#include "input/vkeyboard.h"

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
    // v55: single magenta #FF00FF chroma void (stable).
    LUA->PushNumber(55);
    return 1;
}

// 0=OPAQUE 1=ALPHA_BLEND 2=ADDITIVE 3=AUTO — for WiVRn/Quest room-scale AR.
LUA_FUNCTION(SetEnvironmentBlendMode) {
    int mode = 0;
    if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER))
        mode = (int)LUA->GetNumber(1);
    LUA->PushNumber((double)XR_SetEnvironmentBlendMode(mode));
    return 1;
}

LUA_FUNCTION(GetEnvironmentBlendMode) {
    LUA->PushNumber((double)XR_GetEnvironmentBlendMode());
    return 1;
}

LUA_FUNCTION(SupportsAlphaBlend) {
    LUA->PushBool(XR_SupportsAlphaBlend());
    return 1;
}

// enable[, pinkTol] — dual Source error mosaic chroma (pink + neighbor-gated black).
LUA_FUNCTION(SetPassthroughChroma) {
    bool en = false;
    float thr = 0.18f;
    if (LUA->IsType(1, GarrysMod::Lua::Type::BOOL))
        en = LUA->GetBool(1);
    else if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER))
        en = LUA->GetNumber(1) != 0.0;
    if (LUA->IsType(2, GarrysMod::Lua::Type::NUMBER))
        thr = (float)LUA->GetNumber(2);
    XR_SetPassthroughChroma(en, thr);
    return 0;
}

// r,g,b in 0..1 — pink key (default #FF00DC).
LUA_FUNCTION(SetPassthroughChromaKey) {
    float r = 1.f, g = 0.f, b = 220.f / 255.f;
    if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER)) r = (float)LUA->GetNumber(1);
    if (LUA->IsType(2, GarrysMod::Lua::Type::NUMBER)) g = (float)LUA->GetNumber(2);
    if (LUA->IsType(3, GarrysMod::Lua::Type::NUMBER)) b = (float)LUA->GetNumber(3);
    XR_SetPassthroughChromaKey(r, g, b);
    return 0;
}

// r,g,b in 0..1 — black mosaic cell (default #010001).
LUA_FUNCTION(SetPassthroughChromaKey2) {
    float r = 1.f / 255.f, g = 0.f, b = 1.f / 255.f;
    if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER)) r = (float)LUA->GetNumber(1);
    if (LUA->IsType(2, GarrysMod::Lua::Type::NUMBER)) g = (float)LUA->GetNumber(2);
    if (LUA->IsType(3, GarrysMod::Lua::Type::NUMBER)) b = (float)LUA->GetNumber(3);
    XR_SetPassthroughChromaKey2(r, g, b);
    return 0;
}

LUA_FUNCTION(SetPassthroughChromaTol2) {
    float t = 0.08f;
    if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER)) t = (float)LUA->GetNumber(1);
    XR_SetPassthroughChromaTol2(t);
    return 0;
}

// mask: 1=pink 2=black 4=blackNeedsPinkNear. 7=full error checker (default).
LUA_FUNCTION(SetPassthroughChromaMask) {
    int m = 7;
    if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER)) m = (int)LUA->GetNumber(1);
    XR_SetPassthroughChromaMask(m);
    return 0;
}

LUA_FUNCTION(GetPassthroughChroma) {
    LUA->PushBool(XR_GetPassthroughChroma());
    return 1;
}

// "openxr" | future backends — missing export means OpenVR (module-master).
LUA_FUNCTION(GetBackend) {
    LUA->PushString("openxr");
    return 1;
}

LUA_FUNCTION(IsHMDPresent) {
    LUA->PushBool(XR_IsHMDPresent());
    return 1;
}

LUA_FUNCTION(Init) {
    // After a proper Shutdown(), g_xrInitialized is false → always cold-start here.
    // Double-start while already live is a no-op (Lua re-ShareTexture after).
    if (g_xrInitialized) {
        g_IsPaused = false;
        XR_SetSubmitEnabled(true);
        VRMOD_LOG_INFO("Init: already initialized — re-enabled submit");
        return 0;
    }

    char errMsg[MAX_STR_LEN];
    if (!XR_Init(errMsg, MAX_STR_LEN)) {
        VRMOD_LOG_ERROR("Init failed: %s", errMsg);
        char full[ MAX_STR_LEN + 256 ];
        snprintf(full, sizeof(full),
            "%s\n\nSee garrysmod/vrmod_debug.log (and data/vrmod_logs/) for details.\n"
            "On Linux this is often a missing OpenXR loader under Steam runtime.",
            errMsg[0] ? errMsg : "unknown OpenXR error");
        LUA->ThrowError(full);
        return 0;
    }

    XR_PollEvents();
    if (!g_xrSessionRunning) {
        VRMOD_LOG_INFO("Session deferred after init (binds on first Share/Render)");
    }
    XR_SetSubmitEnabled(true);
    g_IsPaused = false;
    g_xrInitialized = true;

    memset(g_luaRefs, 0, sizeof(g_luaRefs));
    g_luaRefCount = 0;
    for (int i = 0; i < LuaRefIndex_Max; i++) {
        LUA->CreateTable();
        g_luaRefs[i] = LUA->ReferenceCreate();
        g_luaRefCount++;
    }

#ifdef _WIN32
    {
        char derr[MAX_STR_LEN];
        if (!D3D_InitDeviceHooks(derr, MAX_STR_LEN)) {
            VRMOD_LOG_ERROR("%s", derr);
            LUA->ThrowError(derr[0] ? derr : "VRMOD: D3D device init failed");
            return 0;
        }
    }
    g_rtTextureNeedsVFlip = false;
    VRMOD_LOG_INFO("VR initialized successfully (OpenXR D3D11 Windows, flip=0).");
#else
    void* lib = dlopen("libtogl_client.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib) LUA->ThrowError("VRMOD: dlopen failed");

    auto GetOpenGLEntryPoints = reinterpret_cast<GetOpenGLEntryPoints_t>(dlsym(lib, "GetOpenGLEntryPoints"));
    if (!GetOpenGLEntryPoints) LUA->ThrowError("VRMOD: dlsym failed");

    g_GL = GetOpenGLEntryPoints(nullptr);
    dlclose(lib);

    // cb59aeb: drain stale GL errors after prior VR teardown / togl (else restart fails once).
    while (glGetError() != GL_NO_ERROR) {}

    g_createTexture = *((void**)&g_GL->firstFunc + 50);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        char buf[128];
        snprintf(buf, sizeof(buf), "VRMOD: OpenGL error: %u", err);
        LUA->ThrowError(buf);
        return 0;
    }

    VRMOD_LOG_INFO("VR initialized successfully (OpenXR).");
#endif
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

    // Provide action table for binding suggestion (name→XrAction) and for
    // analog→boolean threshold synthesis on Oculus Touch triggers/grips.
    XR_SetActionCache(g_actions, g_actionCount);

    // Attach if session already exists; otherwise XR_EnsureSessionAndInput will attach
    // on the first render frame when GLX is current (SetActionManifest often runs at
    // startup before ShareTexture / RenderScene — "no session yet" is expected).
    if (!XR_AttachActionSets()) {
        VRMOD_LOG_INFO("SetActionManifest: action sets will attach after GL session is created");
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
    // Exit gate: never WaitFrame after submit disabled (avoids orphan begin + worker races).
    if (!g_xrInitialized || g_IsPaused || !XR_IsSubmitEnabled()) return 0;

    // RenderScene / PreRender: GLX context is current — finish deferred session + input.
    {
        char serr[MAX_STR_LEN];
        if (XR_EnsureSessionAndInput(serr, MAX_STR_LEN)) {
            if (!g_xrSwapchainsCreated && g_xrSessionRunning) {
                char scerr[MAX_STR_LEN];
                if (XR_CreateSwapchains(scerr, MAX_STR_LEN)) {
                    g_xrSwapchainsCreated = true;
                    VRMOD_LOG_INFO("Swapchains created on first render frame (GLX deferred path)");
                } else {
                    static int s_scWarn = 0;
                    if ((s_scWarn++ % 120) == 0)
                        VRMOD_LOG_ERROR("Swapchain create: %s", scerr);
                }
            }
        } else {
            static int s_ensWarn = 0;
            if ((s_ensWarn++ % 120) == 0 && serr[0])
                VRMOD_LOG_WARN("%s", serr);
        }
    }

    // Poll events
    XR_PollEvents();

    if (!g_xrSessionRunning || !XR_IsSubmitEnabled()) return 0;

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

// Physical controller sources for Lua rebinding UI (replaces SteamVR binding UI).
LUA_FUNCTION(GetControllerSources) {
    LUA->CreateTable();
    const int n = XR_GetControllerSourceCount();
    for (int i = 0; i < n; i++) {
        const char* id = XR_GetControllerSourceId(i);
        if (!id || !id[0]) continue;
        LUA->CreateTable();
        LUA->PushString(id);
        LUA->SetField(-2, "id");
        LUA->PushString(XR_GetControllerSourceLabel(i));
        LUA->SetField(-2, "label");
        LUA->PushBool(XR_GetControllerSourceIsFloat(i));
        LUA->SetField(-2, "analog");
        float value = XR_GetControllerSourceValue(i);
        LUA->PushNumber(value);
        LUA->SetField(-2, "value");
        bool active = XR_GetControllerSourceIsActive(i);
        LUA->PushBool(active);
        LUA->SetField(-2, "active");
        // pressed from raw value — do NOT gate on isActive.
        // OpenXR often reports isActive=false for unbound/secondary paths while
        // currentState still updates; gating made Lua chords (mode=all) flaky
        // because any single dead active bit killed the whole chord.
        // Thresholds match fire/pickup synthesis in xr_input.cpp.
        bool pressed = XR_GetControllerSourceIsFloat(i)
            ? (value >= 0.55f)
            : (value >= 0.5f);
        LUA->PushBool(pressed);
        LUA->SetField(-2, "pressed");
        LUA->SetField(-2, id);
    }
    return 1;
}

LUA_FUNCTION(GetActions) {
    // OpenVR module-master: InputDigital/Analog/SkeletalSummary locals at top of GetActions.
    VRSkeletalSummaryData_t skeletalSummaryData;
    char* changedActionNames[MAX_ACTIONS];
    bool changedActionStates[MAX_ACTIONS];
    int changedActionCount = 0;

    LUA->ReferencePush(g_luaRefs[LuaRefIndex_ActionTable]);

    // Manifest short names collide across main/driving (boolean_spawnmenu, etc.).
    // Inactive set actions must not overwrite active ones in the Lua table.
    // Track short names already written by an *active* action this frame.
    char activeWritten[MAX_ACTIONS][MAX_STR_LEN];
    int activeWrittenCount = 0;
    auto markActiveWritten = [&](const char* name) {
        if (!name || activeWrittenCount >= MAX_ACTIONS) return;
        for (int k = 0; k < activeWrittenCount; k++)
            if (strcmp(activeWritten[k], name) == 0) return;
        strncpy(activeWritten[activeWrittenCount], name, MAX_STR_LEN - 1);
        activeWritten[activeWrittenCount][MAX_STR_LEN - 1] = '\0';
        activeWrittenCount++;
    };
    auto wasActiveWritten = [&](const char* name) -> bool {
        if (!name) return false;
        for (int k = 0; k < activeWrittenCount; k++)
            if (strcmp(activeWritten[k], name) == 0) return true;
        return false;
    };

    for (int i = 0; i < g_actionCount; i++) {
        if (g_actions[i].type == ActionType_Boolean) {
            bool changed = false;
            bool active = false;
            bool state = XR_GetBooleanAction(g_actions[i].handle, &changed, &active);
            const char* name = g_actions[i].name;
            if (active) {
                // OR with any earlier active duplicate (both sets rarely active together)
                if (wasActiveWritten(name)) {
                    LUA->GetField(-1, name);
                    if (LUA->IsType(-1, GarrysMod::Lua::Type::BOOL) && LUA->GetBool(-1))
                        state = true;
                    LUA->Pop();
                }
                LUA->PushBool(state);
                LUA->SetField(-2, name);
                markActiveWritten(name);
                if (changed) {
                    changedActionNames[changedActionCount] = g_actions[i].name;
                    changedActionStates[changedActionCount] = state;
                    changedActionCount++;
                }
            } else if (!wasActiveWritten(name)) {
                // Inactive and no active sibling yet — seed false (clears stale vehicle keys)
                LUA->PushBool(false);
                LUA->SetField(-2, name);
            }
            // else: inactive duplicate after active write — leave active value alone
        }
        else if (g_actions[i].type == ActionType_Vector1) {
            bool active = false;
            float val = XR_GetFloatAction(g_actions[i].handle, &active);
            const char* name = g_actions[i].name;
            if (active) {
                LUA->PushNumber(val);
                LUA->SetField(-2, name);
                markActiveWritten(name);
            } else if (!wasActiveWritten(name)) {
                LUA->PushNumber(0.0);
                LUA->SetField(-2, name);
            }
        }
        else if (g_actions[i].type == ActionType_Vector2) {
            float x, y;
            bool active = false;
            XR_GetVector2Action(g_actions[i].handle, &x, &y, &active);
            const char* name = g_actions[i].name;
            if (active || !wasActiveWritten(name)) {
                LUA->ReferencePush(g_actions[i].luaRefs[0]);
                LUA->PushNumber(active ? x : 0.0f);
                LUA->SetField(-2, "x");
                LUA->PushNumber(active ? y : 0.0f);
                LUA->SetField(-2, "y");
                LUA->SetField(-2, name);
                if (active) markActiveWritten(name);
            }
        }
        else if (g_actions[i].type == ActionType_Skeleton) {
            // Duplicate OpenVR module-master GetActions skeleton branch:
            //   g_pInput->GetSkeletalSummaryData(handle, EVRSummaryType(1), &skeletalSummaryData);
            //   push flFingerCurl[0..4] into fingerCurls table on action name.
            XR_GetSkeletalSummaryData(g_actions[i].handle,
                static_cast<int>(VRSummaryType_FromDevice), &skeletalSummaryData);
            LUA->ReferencePush(g_actions[i].luaRefs[0]);
            LUA->ReferencePush(g_actions[i].luaRefs[1]);
            for (int j = 0; j < 5; j++) {
                LUA->PushNumber(j + 1);
                LUA->PushNumber(skeletalSummaryData.flFingerCurl[j]);
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

#ifndef _WIN32
    // cb59aeb: drain stale GL errors after prior exit so hook/share is clean.
    while (glGetError() != GL_NO_ERROR) {}
#endif

    // Optional Lua args: eyeW, eyeH (from SafeShareTextureBegin). Else HMD recommended.
    uint32_t texW = g_xrRecommendedWidth > 0 ? g_xrRecommendedWidth : 1024;
    uint32_t texH = g_xrRecommendedHeight > 0 ? g_xrRecommendedHeight : 1024;
    if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER)) {
        double w = LUA->GetNumber(1);
        if (w >= 16.0) texW = (uint32_t)w;
    }
    if (LUA->IsType(2, GarrysMod::Lua::Type::NUMBER)) {
        double h = LUA->GetNumber(2);
        if (h >= 16.0) texH = (uint32_t)h;
    }

    int rc = ::ShareTextureBegin(texW, texH, errBridge);
    if (rc != 0) {
#ifdef _WIN32
        LUA->ThrowError("VRMOD: ShareTextureBegin failed (D3D CreateTexture hook)");
#else
        LUA->ThrowError("VRMOD: mprotect RWX failed");
#endif
    }

    // Session/GL often not current at Lua startup ShareTexture. Soft-try; first
    // UpdatePosesAndActions (RenderScene, GLX current) will ensure session + attach.
    {
        char serr[MAX_STR_LEN];
        if (XR_EnsureSessionAndInput(serr, MAX_STR_LEN)) {
            if (!g_xrSwapchainsCreated && g_xrSessionRunning) {
                char errMsg[MAX_STR_LEN];
                if (XR_CreateSwapchains(errMsg, MAX_STR_LEN)) {
                    g_xrSwapchainsCreated = true;
                } else {
                    VRMOD_LOG_ERROR("Failed to create swapchains: %s", errMsg);
                }
            }
        } else if (serr[0]) {
            VRMOD_LOG_INFO("ShareTextureBegin: session deferred (%s)", serr);
        }
    }

    return 0;
}

LUA_FUNCTION(ShareTextureFinish) {
    auto errBridge = [](const char* msg) {
        VRMOD_LOG_ERROR("%s", msg);
    };

#ifdef _WIN32
    // Master path: unpatch + OpenSharedResource as D3D11 texture.
    if (!::ShareTextureFinish(errBridge)) {
        LUA->ThrowError("VRMOD: ShareTextureFinish failed (D3D shared texture)");
    }
    // D3D11 device is now ready — create OpenXR session + swapchains.
    {
        char serr[MAX_STR_LEN];
        if (XR_EnsureSessionAndInput(serr, MAX_STR_LEN)) {
            if (!g_xrSwapchainsCreated && g_xrSessionRunning) {
                char scerr[MAX_STR_LEN];
                if (XR_CreateSwapchains(scerr, MAX_STR_LEN)) {
                    g_xrSwapchainsCreated = true;
                    VRMOD_LOG_INFO("D3D11 swapchains created after ShareTextureFinish");
                } else {
                    VRMOD_LOG_ERROR("Swapchain create: %s", scerr);
                }
            }
        } else if (serr[0]) {
            VRMOD_LOG_INFO("ShareTextureFinish: session pending (%s)", serr);
        }
    }
#else
    if (!RemoveTexturePatch(errBridge)) {
        LUA->ThrowError("VRMOD: Failed to remove the texture patch.");
    }

    // Promote per-eye textures discovered via FBO COLOR_ATTACHMENT0.
    // Do not require glIsTexture — false negatives under mat_queue_mode 2.
    if (g_leftEyeColorTex != 0) {
        if (g_leftEyeTexture != g_leftEyeColorTex) {
            VRMOD_LOG_INFO("Promoting left eye color tex from FBO: %u (was %u)", g_leftEyeColorTex, g_leftEyeTexture);
            g_leftEyeTexture = g_leftEyeColorTex;
        }
    }
    if (g_rightEyeColorTex != 0) {
        if (g_rightEyeTexture != g_rightEyeColorTex) {
            VRMOD_LOG_INFO("Promoting right eye color tex from FBO: %u (was %u)", g_rightEyeColorTex, g_rightEyeTexture);
            g_rightEyeTexture = g_rightEyeColorTex;
        }
    }

    // Legacy shared promotion for fallback paths (non-zero ID is enough).
    if (g_vrRtColorTex != 0) {
        if (g_sharedTexture != g_vrRtColorTex) {
            VRMOD_LOG_INFO("Promoting VR RT color texture from FBO attach: %u (was %u)", g_vrRtColorTex, g_sharedTexture);
            g_sharedTexture = g_vrRtColorTex;
        }
        VRMOD_MarkSharedTextureEngineOwned();
    }

    VRMOD_LOG_INFO("Per-eye textures ready: L=%u (fbo=%u) R=%u (fbo=%u) | legacy shared=%u",
        g_leftEyeTexture, g_leftEyeFBO, g_rightEyeTexture, g_rightEyeFBO, g_sharedTexture);
#endif
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
    // Non-zero ID only — glIsTexture is unreliable under mat_queue 2.
    if (g_captureTexture == 0) {
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
    // true  = Linux/OpenGL RTs need V flip into OpenXR
    // false = Windows/D3D (flip=0) — top-left matches OpenXR; do not invert
    // Driven by Lua: VRMOD_SetRTTextureFlip( not system.IsWindows() )
    if (LUA->GetType(1) == GarrysMod::Lua::Type::BOOL) {
        g_rtTextureNeedsVFlip = LUA->GetBool(1);
    } else {
#ifdef _WIN32
        g_rtTextureNeedsVFlip = false;
#else
        g_rtTextureNeedsVFlip = true;
#endif
    }
#ifdef _WIN32
    // Hard enforce flip=0 on Windows even if a bad Lua path passes true.
    g_rtTextureNeedsVFlip = false;
#endif
    return 0;
}

// Optional GPU drain. NEVER glFinish — mat_queue_mode 2 workers die
// ("Illegal termination of worker thread"). glFlush is the safe upper bound.
LUA_FUNCTION(GLFinish) {
#ifndef _WIN32
    if (glXGetCurrentContext()) {
        glFlush();
    }
#endif
    return 0;
}

// Authoritative SBS/RT size from Lua (g_VR.rtWidth/Height). Required for mat_queue 2
// because glGetTexLevelParameteriv often returns 0 on live engine RTs.
LUA_FUNCTION(SetKnownSubmitSize) {
    uint32_t w = 0, h = 0;
    if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER)) {
        double n = LUA->GetNumber(1);
        if (n >= 16.0) w = (uint32_t)n;
    }
    if (LUA->IsType(2, GarrysMod::Lua::Type::NUMBER)) {
        double n = LUA->GetNumber(2);
        if (n >= 16.0) h = (uint32_t)n;
    }
    if (w > 0 && h > 0) {
        VRMOD_SetKnownSubmitSize(w, h);
    }
    return 0;
}

LUA_FUNCTION(SetSubmitEnabled) {
    // Async exit: Lua disables submit before deferred Shutdown / mat_queue restore.
    bool en = true;
    if (LUA->IsType(1, GarrysMod::Lua::Type::BOOL)) {
        en = LUA->GetBool(1);
    } else if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER)) {
        en = LUA->GetNumber(1) != 0.0;
    }
    XR_SetSubmitEnabled(en);
    return 0;
}

// Per-eye UV crop policy (vrmod_submit_crop). 0=SAFE 1=FULL 2=FOV_CROP. Default 0.
LUA_FUNCTION(SetSubmitCropMode) {
#ifndef _WIN32
    int mode = 0;
    if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER)) {
        mode = (int)LUA->GetNumber(1);
    }
    XR_SetSubmitCropMode(mode);
#endif
    return 0;
}

LUA_FUNCTION(GetSubmitCropMode) {
#ifndef _WIN32
    LUA->PushNumber((double)XR_GetSubmitCropMode());
#else
    LUA->PushNumber(0.0);
#endif
    return 1;
}

// After UpdatePosesAndActions (WaitFrame+Begin): true if compositor wants a layer this frame.
LUA_FUNCTION(ShouldRender) {
    LUA->PushBool(XR_ShouldRenderThisFrame());
    return 1;
}

// Copy last complete engine stereo (SBS or per-eye) into module staging.
// Call at frame start *before* overwriting the engine RT (after MatQueue had a frame boundary).
LUA_FUNCTION(CollectEyes) {
#ifndef _WIN32
    if (!XR_IsSubmitEnabled() || g_IsPaused || !g_xrInitialized) {
        LUA->PushBool(false);
        return 1;
    }
    bool ok = XR_CollectEyesFromEngine(g_texBounds);
    LUA->PushBool(ok);
#else
    LUA->PushBool(false);
#endif
    return 1;
}

LUA_FUNCTION(HasCollectedEyes) {
#ifndef _WIN32
    LUA->PushBool(XR_HasCollectedEyes());
#else
    LUA->PushBool(false);
#endif
    return 1;
}

LUA_FUNCTION(SubmitSharedTexture) {
    // Exit / teardown gate — never touch GL or OpenXR once disabled.
    if (!XR_IsSubmitEnabled() || g_IsPaused || !g_xrInitialized) {
        return 0;
    }

    // Last-chance session create if UpdatePoses skipped (GL must be current here).
    if (!g_xrSessionRunning) {
        char serr[MAX_STR_LEN];
        XR_EnsureSessionAndInput(serr, MAX_STR_LEN);
        if (!g_xrSwapchainsCreated && g_xrSessionRunning) {
            char scerr[MAX_STR_LEN];
            if (XR_CreateSwapchains(scerr, MAX_STR_LEN))
                g_xrSwapchainsCreated = true;
        }
    }

    // Do not require glIsTexture — false under mat_queue 2 for live engine RTs.
    bool haveLeft = (g_leftEyeTexture != 0) || (g_leftEyeColorTex != 0) || (g_leftEyeFBO != 0);
    bool haveRight = (g_rightEyeTexture != 0) || (g_rightEyeColorTex != 0) || (g_rightEyeFBO != 0);
    bool haveLegacy = (g_sharedTexture != 0) || (g_vrRtColorTex != 0) || (g_vrRtFBO != 0);
    bool haveUsableSrc = haveLeft || haveRight || haveLegacy;
    if (!g_xrSwapchainsCreated || !haveUsableSrc || !g_xrSessionRunning) {
        if (g_xrSessionRunning && !g_xrSwapchainsCreated && XR_IsSubmitEnabled()) {
            XR_EndFrame();
        }
        return 0;
    }

    GLuint submitId = 0;
    if (haveLeft || haveRight) {
        submitId = g_leftEyeTexture ? g_leftEyeTexture
            : (g_leftEyeColorTex ? g_leftEyeColorTex
            : (g_rightEyeTexture ? g_rightEyeTexture : g_rightEyeColorTex));
    }
    if (submitId == 0) {
        submitId = g_vrRtColorTex ? g_vrRtColorTex
            : (g_sharedTexture ? g_sharedTexture : g_captureTexture);
    }
    XrSubmitResult res = XR_SubmitStolenTexture(submitId, g_texBounds);
    // Rate-limit console spam (was hundreds of lines under mat_queue 2).
    if (!res.ok && res.errMsg[0]) {
        static int s_subErr = 0;
        if ((++s_subErr % 120) == 1) {
            LuaPrint(LUA, res.errMsg);
        }
    }
    return 0;
}

LUA_FUNCTION(Shutdown) {
    // Full teardown (cb59aeb restart model): next Init always gets a fresh
    // instance/session/swapchains. Soft-pause left submit gated / HMD probe races.
    if (!g_xrInitialized) {
        XR_SetSubmitEnabled(false);
        g_IsPaused = false;
        return 0;
    }

    XR_SetSubmitEnabled(false);
    g_IsPaused = false;

#ifdef _WIN32
#else
    // No glFlush/glFinish on teardown — stalls/races mat_queue workers mid-exit.
    // Force-remove texture hooks left mid-Share (partial start/fail).
    {
        auto errBridge = [](const char* msg) { VRMOD_LOG_ERROR("%s", msg); };
        RemoveTexturePatch(errBridge);
        g_glIsPatched = false;
        g_fbIsPatched = false;
        g_captureStealActive = false;
    }
    // Drop IDs only — do not glDelete engine RTs.
    g_sharedTexture = 0;
    g_leftEyeTexture = 0;
    g_rightEyeTexture = 0;
    g_leftEyeColorTex = 0;
    g_rightEyeColorTex = 0;
    g_leftEyeFBO = 0;
    g_rightEyeFBO = 0;
    g_vrRtFBO = 0;
    g_vrRtColorTex = 0;
    g_captureTexture = 0;
    VRMOD_MarkSharedTextureEngineOwned();
    while (glGetError() != GL_NO_ERROR) {}
#endif

    // Free Lua references + action table (rebuilt on next SetActionManifest).
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
    g_luaRefCount = 0;
    g_actionCount = 0;
    memset(g_actions, 0, sizeof(g_actions));
    g_actionSetCount = 0;
    g_activeActionSetCount = 0;
    memset(g_actionSets, 0, sizeof(g_actionSets));
    memset(g_activeActionSets, 0, sizeof(g_activeActionSets));

    if (g_xrSwapchainsCreated) {
        XR_DestroySwapchains();
        g_xrSwapchainsCreated = false;
    }

    // Ordered teardown lives in XR_Shutdown (spaces → session → sets → instance).
    // Do NOT XR_CleanupActions() before session destroy — destroys attached sets.
    XR_Shutdown();
    g_xrInitialized = false;
    g_xrEyePosesValid = false;

    VRMOD_LOG_INFO("VR shutdown (full teardown — next Init is cold start)");
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

// ── Virtual Display (reusable launcher + pause panel surface) ──

LUA_FUNCTION(VirtualDisplayIsSupported) {
    LUA->PushBool(VDisplay_IsSupported());
    return 1;
}

LUA_FUNCTION(VirtualDisplayCreate) {
    uint32_t w = (uint32_t)LUA->CheckNumber(1);
    uint32_t h = (uint32_t)LUA->CheckNumber(2);
    int hint = 0;
    if (LUA->IsType(3, GarrysMod::Lua::Type::NUMBER))
        hint = (int)LUA->GetNumber(3);
    char err[MAX_STR_LEN] = {};
    int id = VDisplay_Create(w, h, hint, err, sizeof(err));
    if (id <= 0) {
        LUA->PushBool(false);
        LUA->PushString(err[0] ? err : "VDisplay create failed");
        return 2;
    }
    LUA->PushNumber(id);
    return 1;
}

LUA_FUNCTION(VirtualDisplayResize) {
    int id = (int)LUA->CheckNumber(1);
    uint32_t w = (uint32_t)LUA->CheckNumber(2);
    uint32_t h = (uint32_t)LUA->CheckNumber(3);
    char err[MAX_STR_LEN] = {};
    if (!VDisplay_Resize(id, w, h, err, sizeof(err))) {
        LUA->PushBool(false);
        LUA->PushString(err[0] ? err : "VDisplay resize failed");
        return 2;
    }
    LUA->PushBool(true);
    return 1;
}

LUA_FUNCTION(VirtualDisplayDestroy) {
    int id = 0;
    if (LUA->IsType(1, GarrysMod::Lua::Type::NUMBER))
        id = (int)LUA->GetNumber(1);
    VDisplay_Destroy(id);
    return 0;
}

LUA_FUNCTION(VirtualDisplayGetInfo) {
    int id = (int)LUA->CheckNumber(1);
    VDisplayInfo info = {};
    if (!VDisplay_GetInfo(id, &info)) {
        LUA->PushNil();
        return 1;
    }
    LUA->CreateTable();
    LUA->PushNumber(info.id);
    LUA->SetField(-2, "id");
    LUA->PushNumber(info.width);
    LUA->SetField(-2, "width");
    LUA->PushNumber(info.height);
    LUA->SetField(-2, "height");
    LUA->PushNumber(info.glTexture);
    LUA->SetField(-2, "glTexture");
    LUA->PushNumber(info.glFBO);
    LUA->SetField(-2, "glFBO");
    LUA->PushBool(info.valid);
    LUA->SetField(-2, "valid");
    LUA->PushBool(info.hasCapture);
    LUA->SetField(-2, "hasCapture");
    return 1;
}

LUA_FUNCTION(VirtualDisplayCaptureWindow) {
    int id = (int)LUA->CheckNumber(1);
    char err[MAX_STR_LEN] = {};
    if (!VDisplay_CaptureWindow(id, err, sizeof(err))) {
        LUA->PushBool(false);
        LUA->PushString(err[0] ? err : "capture failed");
        return 2;
    }
    LUA->PushBool(true);
    return 1;
}

LUA_FUNCTION(VirtualDisplayClear) {
    int id = (int)LUA->CheckNumber(1);
    float r = LUA->IsType(2, GarrysMod::Lua::Type::NUMBER) ? (float)LUA->GetNumber(2) : 0.f;
    float g = LUA->IsType(3, GarrysMod::Lua::Type::NUMBER) ? (float)LUA->GetNumber(3) : 0.f;
    float b = LUA->IsType(4, GarrysMod::Lua::Type::NUMBER) ? (float)LUA->GetNumber(4) : 0.f;
    float a = LUA->IsType(5, GarrysMod::Lua::Type::NUMBER) ? (float)LUA->GetNumber(5) : 1.f;
    LUA->PushBool(VDisplay_Clear(id, r, g, b, a));
    return 1;
}

// ── VR Keyboard driver (shared launcher + GMod) ──

LUA_FUNCTION(KeyboardIsSupported) {
    LUA->PushBool(VKB_IsSupported());
    return 1;
}

LUA_FUNCTION(KeyboardSystemAvailable) {
    LUA->PushBool(VKB_SystemKeyboardAvailable());
    return 1;
}

LUA_FUNCTION(KeyboardOpen) {
    const char* title = LUA->IsType(1, GarrysMod::Lua::Type::STRING) ? LUA->GetString(1) : "KEYBOARD";
    const char* initial = LUA->IsType(2, GarrysMod::Lua::Type::STRING) ? LUA->GetString(2) : "";
    int w = LUA->IsType(3, GarrysMod::Lua::Type::NUMBER) ? (int)LUA->GetNumber(3) : 555;
    int h = LUA->IsType(4, GarrysMod::Lua::Type::NUMBER) ? (int)LUA->GetNumber(4) : 300;
    int hint = LUA->IsType(5, GarrysMod::Lua::Type::NUMBER) ? (int)LUA->GetNumber(5) : 0;
    char err[MAX_STR_LEN] = {};
    int id = VKB_Open(title, initial, w, h, hint, err, sizeof(err));
    if (id <= 0) {
        LUA->PushBool(false);
        LUA->PushString(err[0] ? err : "KeyboardOpen failed");
        return 2;
    }
    LUA->PushNumber(id);
    return 1;
}

LUA_FUNCTION(KeyboardClose) {
    int id = LUA->IsType(1, GarrysMod::Lua::Type::NUMBER) ? (int)LUA->GetNumber(1) : 0;
    if (id > 0)
        VKB_Close(id);
    else
        VKB_Shutdown();
    return 0;
}

LUA_FUNCTION(KeyboardIsOpen) {
    int id = LUA->IsType(1, GarrysMod::Lua::Type::NUMBER) ? (int)LUA->GetNumber(1) : 0;
    if (id > 0) {
        LUA->PushBool(VKB_IsOpen(id));
    } else {
        LUA->PushBool(VKB_FirstOpen() > 0);
    }
    return 1;
}

LUA_FUNCTION(KeyboardGetInfo) {
    int id = (int)LUA->CheckNumber(1);
    VKeyboardInfo info = {};
    if (!VKB_GetInfo(id, &info)) {
        LUA->PushNil();
        return 1;
    }
    LUA->CreateTable();
    LUA->PushNumber(info.id); LUA->SetField(-2, "id");
    LUA->PushBool(info.open); LUA->SetField(-2, "open");
    LUA->PushBool(info.upper); LUA->SetField(-2, "upper");
    LUA->PushNumber(info.width); LUA->SetField(-2, "width");
    LUA->PushNumber(info.height); LUA->SetField(-2, "height");
    LUA->PushNumber(info.headerH); LUA->SetField(-2, "headerH");
    LUA->PushNumber(info.keyCount); LUA->SetField(-2, "keyCount");
    LUA->PushString(info.title); LUA->SetField(-2, "title");
    LUA->PushString(info.text); LUA->SetField(-2, "text");
    LUA->PushNumber(info.lastAction); LUA->SetField(-2, "lastAction");
    LUA->PushString(info.lastChar); LUA->SetField(-2, "lastChar");
    return 1;
}

LUA_FUNCTION(KeyboardGetText) {
    int id = (int)LUA->CheckNumber(1);
    LUA->PushString(VKB_GetText(id));
    return 1;
}

LUA_FUNCTION(KeyboardSetText) {
    int id = (int)LUA->CheckNumber(1);
    const char* t = LUA->IsType(2, GarrysMod::Lua::Type::STRING) ? LUA->GetString(2) : "";
    LUA->PushBool(VKB_SetText(id, t));
    return 1;
}

LUA_FUNCTION(KeyboardAppend) {
    int id = (int)LUA->CheckNumber(1);
    const char* t = LUA->IsType(2, GarrysMod::Lua::Type::STRING) ? LUA->GetString(2) : "";
    LUA->PushBool(VKB_Append(id, t));
    return 1;
}

LUA_FUNCTION(KeyboardBackspace) {
    int id = (int)LUA->CheckNumber(1);
    LUA->PushBool(VKB_Backspace(id));
    return 1;
}

LUA_FUNCTION(KeyboardGetShift) {
    int id = (int)LUA->CheckNumber(1);
    LUA->PushBool(VKB_GetShift(id));
    return 1;
}

LUA_FUNCTION(KeyboardSetShift) {
    int id = (int)LUA->CheckNumber(1);
    bool upper = LUA->IsType(2, GarrysMod::Lua::Type::BOOL) && LUA->GetBool(2);
    LUA->PushBool(VKB_SetShift(id, upper));
    return 1;
}

LUA_FUNCTION(KeyboardSetTitle) {
    int id = (int)LUA->CheckNumber(1);
    const char* t = LUA->IsType(2, GarrysMod::Lua::Type::STRING) ? LUA->GetString(2) : "KEYBOARD";
    LUA->PushBool(VKB_SetTitle(id, t));
    return 1;
}

LUA_FUNCTION(KeyboardHitTest) {
    int id = (int)LUA->CheckNumber(1);
    float px = (float)LUA->CheckNumber(2);
    float py = (float)LUA->CheckNumber(3);
    LUA->PushNumber(VKB_HitTest(id, px, py));
    return 1;
}

LUA_FUNCTION(KeyboardPointerClick) {
    int id = (int)LUA->CheckNumber(1);
    float px = (float)LUA->CheckNumber(2);
    float py = (float)LUA->CheckNumber(3);
    int act = VKB_PointerClick(id, px, py);
    LUA->PushNumber(act);
    // second return: text after mutation
    LUA->PushString(VKB_GetText(id));
    // third: lastChar
    VKeyboardInfo info = {};
    if (VKB_GetInfo(id, &info))
        LUA->PushString(info.lastChar);
    else
        LUA->PushString("");
    return 3;
}

LUA_FUNCTION(KeyboardGetKeys) {
    int id = (int)LUA->CheckNumber(1);
    int n = VKB_GetKeyCount(id);
    LUA->CreateTable();
    for (int i = 0; i < n; ++i) {
        VKeyInfo k = {};
        if (!VKB_GetKey(id, i, &k)) continue;
        LUA->PushNumber(i + 1);
        LUA->CreateTable();
        LUA->PushNumber(k.x); LUA->SetField(-2, "x");
        LUA->PushNumber(k.y); LUA->SetField(-2, "y");
        LUA->PushNumber(k.w); LUA->SetField(-2, "w");
        LUA->PushNumber(k.h); LUA->SetField(-2, "h");
        LUA->PushNumber(k.action); LUA->SetField(-2, "action");
        LUA->PushString(k.label); LUA->SetField(-2, "label");
        LUA->PushBool(k.special); LUA->SetField(-2, "special");
        LUA->SetTable(-3);
    }
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
    LUA->PushCFunction(GetBackend);
    LUA->SetField(-2, "GetBackend");
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
    LUA->PushCFunction(GetControllerSources);
    LUA->SetField(-2, "GetControllerSources");
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
    LUA->PushCFunction(GLFinish);
    LUA->SetField(-2, "GLFinish");
    LUA->PushCFunction(SetKnownSubmitSize);
    LUA->SetField(-2, "SetKnownSubmitSize");
    LUA->PushCFunction(SetSubmitEnabled);
    LUA->SetField(-2, "SetSubmitEnabled");
    LUA->PushCFunction(SetSubmitCropMode);
    LUA->SetField(-2, "SetSubmitCropMode");
    LUA->PushCFunction(GetSubmitCropMode);
    LUA->SetField(-2, "GetSubmitCropMode");
    LUA->PushCFunction(SetEnvironmentBlendMode);
    LUA->SetField(-2, "SetEnvironmentBlendMode");
    LUA->PushCFunction(GetEnvironmentBlendMode);
    LUA->SetField(-2, "GetEnvironmentBlendMode");
    LUA->PushCFunction(SupportsAlphaBlend);
    LUA->SetField(-2, "SupportsAlphaBlend");
    LUA->PushCFunction(SetPassthroughChroma);
    LUA->SetField(-2, "SetPassthroughChroma");
    LUA->PushCFunction(SetPassthroughChromaKey);
    LUA->SetField(-2, "SetPassthroughChromaKey");
    LUA->PushCFunction(SetPassthroughChromaKey2);
    LUA->SetField(-2, "SetPassthroughChromaKey2");
    LUA->PushCFunction(SetPassthroughChromaTol2);
    LUA->SetField(-2, "SetPassthroughChromaTol2");
    LUA->PushCFunction(SetPassthroughChromaMask);
    LUA->SetField(-2, "SetPassthroughChromaMask");
    LUA->PushCFunction(GetPassthroughChroma);
    LUA->SetField(-2, "GetPassthroughChroma");
    LUA->PushCFunction(ShouldRender);
    LUA->SetField(-2, "ShouldRender");
    LUA->PushCFunction(CollectEyes);
    LUA->SetField(-2, "CollectEyes");
    LUA->PushCFunction(HasCollectedEyes);
    LUA->SetField(-2, "HasCollectedEyes");
    LUA->PushCFunction(SubmitSharedTexture);
    LUA->SetField(-2, "SubmitSharedTexture");
    LUA->PushCFunction(Shutdown);
    LUA->SetField(-2, "Shutdown");
    LUA->PushCFunction(TriggerHaptic);
    LUA->SetField(-2, "TriggerHaptic");
    LUA->PushCFunction(GetTrackedDeviceNames);
    LUA->SetField(-2, "GetTrackedDeviceNames");
    LUA->PushCFunction(VirtualDisplayIsSupported);
    LUA->SetField(-2, "VirtualDisplayIsSupported");
    LUA->PushCFunction(VirtualDisplayCreate);
    LUA->SetField(-2, "VirtualDisplayCreate");
    LUA->PushCFunction(VirtualDisplayResize);
    LUA->SetField(-2, "VirtualDisplayResize");
    LUA->PushCFunction(VirtualDisplayDestroy);
    LUA->SetField(-2, "VirtualDisplayDestroy");
    LUA->PushCFunction(VirtualDisplayGetInfo);
    LUA->SetField(-2, "VirtualDisplayGetInfo");
    LUA->PushCFunction(VirtualDisplayCaptureWindow);
    LUA->SetField(-2, "VirtualDisplayCaptureWindow");
    LUA->PushCFunction(VirtualDisplayClear);
    LUA->SetField(-2, "VirtualDisplayClear");
    LUA->PushCFunction(KeyboardIsSupported);
    LUA->SetField(-2, "KeyboardIsSupported");
    LUA->PushCFunction(KeyboardSystemAvailable);
    LUA->SetField(-2, "KeyboardSystemAvailable");
    LUA->PushCFunction(KeyboardOpen);
    LUA->SetField(-2, "KeyboardOpen");
    LUA->PushCFunction(KeyboardClose);
    LUA->SetField(-2, "KeyboardClose");
    LUA->PushCFunction(KeyboardIsOpen);
    LUA->SetField(-2, "KeyboardIsOpen");
    LUA->PushCFunction(KeyboardGetInfo);
    LUA->SetField(-2, "KeyboardGetInfo");
    LUA->PushCFunction(KeyboardGetText);
    LUA->SetField(-2, "KeyboardGetText");
    LUA->PushCFunction(KeyboardSetText);
    LUA->SetField(-2, "KeyboardSetText");
    LUA->PushCFunction(KeyboardAppend);
    LUA->SetField(-2, "KeyboardAppend");
    LUA->PushCFunction(KeyboardBackspace);
    LUA->SetField(-2, "KeyboardBackspace");
    LUA->PushCFunction(KeyboardGetShift);
    LUA->SetField(-2, "KeyboardGetShift");
    LUA->PushCFunction(KeyboardSetShift);
    LUA->SetField(-2, "KeyboardSetShift");
    LUA->PushCFunction(KeyboardSetTitle);
    LUA->SetField(-2, "KeyboardSetTitle");
    LUA->PushCFunction(KeyboardHitTest);
    LUA->SetField(-2, "KeyboardHitTest");
    LUA->PushCFunction(KeyboardPointerClick);
    LUA->SetField(-2, "KeyboardPointerClick");
    LUA->PushCFunction(KeyboardGetKeys);
    LUA->SetField(-2, "KeyboardGetKeys");
    LUA->SetField(-2, "vrmod");

    VRMOD_LOG_INFO("Module loaded (OpenXR + VirtualDisplay + Keyboard).");
    return 0;
}

GMOD_MODULE_CLOSE() {
    VRMOD_LOG_INFO("Module closing.");

    VKB_Shutdown();
    VDisplay_Shutdown();

    // Real unload only — destroy OpenXR instance/loader (not a soft VR pause).
    XR_SetSubmitEnabled(false);
    if (g_xrSwapchainsCreated) {
        XR_DestroySwapchains();
        g_xrSwapchainsCreated = false;
    }
    if (g_xrInitialized) {
        XR_ResetInputState();
        XR_Shutdown();
        g_xrInitialized = false;
    }
    g_IsPaused = false;

    VRMOD_LOG_CLOSE();
    return 0;
}
