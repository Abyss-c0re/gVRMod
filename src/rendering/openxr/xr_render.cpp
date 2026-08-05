#ifndef _WIN32

#include "xr_render.h"
#include "core/vrmod_log.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

// The hmd pose (defined in xr_input.cpp) and conversion - used to feed live layer view pose
// back to the game's tracking so RenderViews use current head pose.
extern PoseResult g_xrHMDPose;
extern PoseResult ConvertXrPose(const XrSpaceLocation& loc);

extern PoseResult g_xrEyePoses[2];
extern bool g_xrEyePosesValid;
extern XrFovf g_xrEyeFovs[2];

// 0=SAFE 1=FULL 2=FOV_CROP — see XR_SetSubmitCropMode. Default SAFE after FOV_CROP disaster.
static int g_submitCropMode = 0;

void XR_SetSubmitCropMode(int mode) {
    if (mode < 0) mode = 0;
    if (mode > 2) mode = 2;
    g_submitCropMode = mode;
}
int XR_GetSubmitCropMode() { return g_submitCropMode; }

// Map asymmetric OpenXR FOV → UV inside symmetric overrender (single-eye RT).
// Opt-in only (mode FOV_CROP). Wrong use looks like swapped/crossed eyes.
static void AsymmetricFovToUvCrop(const XrFovf& fov, float* u0, float* u1, float* v0, float* v1) {
    const float tanL = tanf(fov.angleLeft);
    const float tanR = tanf(fov.angleRight);
    const float tanU = tanf(fov.angleUp);
    const float tanD = tanf(fov.angleDown);
    float halfTanX = fmaxf(fabsf(tanL), fabsf(tanR));
    float halfTanY = fmaxf(fabsf(tanU), fabsf(tanD));
    if (halfTanX < 1e-6f) halfTanX = 1e-6f;
    if (halfTanY < 1e-6f) halfTanY = 1e-6f;
    *u0 = (tanL + halfTanX) / (2.0f * halfTanX);
    *u1 = (tanR + halfTanX) / (2.0f * halfTanX);
    *v0 = (halfTanY - tanU) / (2.0f * halfTanY);
    *v1 = (halfTanY - tanD) / (2.0f * halfTanY);
    if (*u1 < *u0) { float t = *u0; *u0 = *u1; *u1 = t; }
    if (*v1 < *v0) { float t = *v0; *v0 = *v1; *v1 = t; }
    const float ins = 0.003f;
    if (*u0 < ins) *u0 = ins;
    if (*u1 > 1.0f - ins) *u1 = 1.0f - ins;
    if (*v0 < ins) *v0 = ins;
    if (*v1 > 1.0f - ins) *v1 = 1.0f - ins;
    if (!(*u1 > *u0 + 0.01f)) { *u0 = ins; *u1 = 1.0f - ins; }
    if (!(*v1 > *v0 + 0.01f)) { *v0 = ins; *v1 = 1.0f - ins; }
}

// Exposed to input unit so UpdatePoses can ensure a fresh HMD pose is available
// for Lua GetPoses on the first frame (fixes "no head tracking" and helps avoid
// initial large height delta that caused player to fly upwards).
void XR_RefreshHMDPose() {
    if (!g_xrSession || !g_xrSessionRunning) return;

    XrViewLocateInfo vli = {XR_TYPE_VIEW_LOCATE_INFO};
    vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    vli.displayTime = g_xrFrameState.predictedDisplayTime;
    vli.space = g_xrStageSpace;

    XrViewState viewState = {XR_TYPE_VIEW_STATE};
    XrView tmpViews[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    uint32_t viewCount = 0;
    XrResult res = g_xrLocateViews(g_xrSession, &vli, &viewState, 2, &viewCount, tmpViews);
    if (res == XR_SUCCESS && viewCount >= 2) {
        // Use average of the two eye positions as the head pose (center between eyes).
        // This matches typical OpenVR HMD pose behavior and fixes roll artifacts
        // where using left eye as "head" caused left-eye-specific vertical drift on roll.
        XrVector3f headPos = {
            (tmpViews[0].pose.position.x + tmpViews[1].pose.position.x) * 0.5f,
            (tmpViews[0].pose.position.y + tmpViews[1].pose.position.y) * 0.5f,
            (tmpViews[0].pose.position.z + tmpViews[1].pose.position.z) * 0.5f
        };
        XrSpaceLocation tempLoc = {XR_TYPE_SPACE_LOCATION};
        tempLoc.locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        tempLoc.pose.position = headPos;
        tempLoc.pose.orientation = tmpViews[0].pose.orientation;  // orientations are nearly identical; use left as representative
        g_xrHMDPose = ConvertXrPose(tempLoc);
    }
}

// Capture / stolen texture names from the share mechanism (gl_hooks.cpp).
// We focus exclusively on these for the pixels sent to the HMD (no FB read fallback).
extern GLuint g_captureTexture;
extern GLuint g_sharedTexture;  // fallback

// Per-eye textures (new proper path: one RT per eye, no side-by-side packing).
extern GLuint g_leftEyeTexture;
extern GLuint g_rightEyeTexture;
extern GLuint g_leftEyeFBO;
extern GLuint g_leftEyeColorTex;
extern GLuint g_rightEyeFBO;
extern GLuint g_rightEyeColorTex;

extern bool g_rtTextureNeedsVFlip;
extern GLint g_knownSubmitSrcW;
extern GLint g_knownSubmitSrcH;
void VRMOD_SetKnownSubmitSize(uint32_t w, uint32_t h);

// FBO + authoritative color texture discovered by observing glFramebufferTexture2D during
// the ShareTextureBegin/Finish window. Querying the attachment on this FBO at submit time
// gives us the texture the engine is actually rendering the two RenderViews into.
extern GLuint g_vrRtFBO;
extern GLuint g_vrRtColorTex;

// OpenXR OpenGL swapchain image type
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr/openxr.h>
#include <openxr/openxr/openxr_platform.h>

// ── Swapchain state ──
static XrSwapchain g_swapchains[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};  // left, right
static XrSwapchainImageOpenGLKHR* g_swapchainImages[2] = {nullptr, nullptr};
static uint32_t g_swapchainImageCount[2] = {0, 0};
static XrView g_views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};

uint32_t g_xrSwapchainWidth = 0;
uint32_t g_xrSwapchainHeight = 0;
int64_t  g_xrSwapchainFormat = 0;

// FBO for blitting
static GLuint g_blitFBO = 0;
static GLuint g_blitSrcFBO = 0;

// Per-eye module-owned staging (double-buffered). Backend times XR; Lua only fills engine RT.
// [eye][slot] — after Collect, read slot holds last complete pair for submit.
static GLuint g_eyeStage[2][2] = {{0, 0}, {0, 0}};
static uint32_t g_eyeStageW = 0;
static uint32_t g_eyeStageH = 0;
static int g_eyeStageWrite = 0; // next Collect writes here
static int g_eyeStageRead = 1;  // Submit reads this (previous Collect)
static bool g_eyeStageReady = false;
// Prefer staging only after a successful Collect; default false so cold start
// uses engine RT (v38 path). Collect under mode 0/1 can set ready.
static bool g_preferCollectedEyes = false;

// GL extension function pointers (resolved lazily)
static PFNGLGENFRAMEBUFFERSPROC    glGenFramebuffersPtr = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffersPtr = nullptr;
static PFNGLBINDFRAMEBUFFERPROC    glBindFramebufferPtr = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2DPtr = nullptr;
static PFNGLBLITFRAMEBUFFERPROC    glBlitFramebufferPtr = nullptr;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatusPtr = nullptr;
static PFNGLCOPYIMAGESUBDATAPROC   glCopyImageSubDataPtr = nullptr;
static PFNGLACTIVETEXTUREPROC      glActiveTexturePtr = nullptr;
static PFNGLUSEPROGRAMPROC         glUseProgramPtr = nullptr;
static PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glGetFramebufferAttachmentParameterivPtr = nullptr;

static bool LoadGLExtensions() {
    if (glGenFramebuffersPtr) return true;  // Already loaded

    glGenFramebuffersPtr = (PFNGLGENFRAMEBUFFERSPROC)glXGetProcAddress((const GLubyte*)"glGenFramebuffers");
    glDeleteFramebuffersPtr = (PFNGLDELETEFRAMEBUFFERSPROC)glXGetProcAddress((const GLubyte*)"glDeleteFramebuffers");
    glBindFramebufferPtr = (PFNGLBINDFRAMEBUFFERPROC)glXGetProcAddress((const GLubyte*)"glBindFramebuffer");
    glFramebufferTexture2DPtr = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glXGetProcAddress((const GLubyte*)"glFramebufferTexture2D");
    glBlitFramebufferPtr = (PFNGLBLITFRAMEBUFFERPROC)glXGetProcAddress((const GLubyte*)"glBlitFramebuffer");
    glCheckFramebufferStatusPtr = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)glXGetProcAddress((const GLubyte*)"glCheckFramebufferStatus");
    glCopyImageSubDataPtr = (PFNGLCOPYIMAGESUBDATAPROC)glXGetProcAddress((const GLubyte*)"glCopyImageSubData");
    glActiveTexturePtr = (PFNGLACTIVETEXTUREPROC)glXGetProcAddress((const GLubyte*)"glActiveTexture");
    glUseProgramPtr = (PFNGLUSEPROGRAMPROC)glXGetProcAddress((const GLubyte*)"glUseProgram");
    glGetFramebufferAttachmentParameterivPtr = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)
        glXGetProcAddress((const GLubyte*)"glGetFramebufferAttachmentParameteriv");

    if (!glGenFramebuffersPtr || !glBindFramebufferPtr || !glFramebufferTexture2DPtr ||
        !glBlitFramebufferPtr || !glCheckFramebufferStatusPtr) {
        VRMOD_LOG_ERROR("Failed to load GL framebuffer extensions");
        return false;
    }
    if (glCopyImageSubDataPtr) {
        VRMOD_LOG_INFO("glCopyImageSubData available — will use direct texture-to-texture copy for swapchain (more robust with engine RT hooks)");
    } else {
        VRMOD_LOG_INFO("glCopyImageSubData not available, falling back to FBO blit path");
    }
    return true;
}

// Pick a swapchain format from the runtime's supported list.
//
// Source Engine RTs are already tonemapped / display-referred (gamma-ish 8-bit).
// Blitting those bytes into an sRGB swapchain (0x8C43) with FRAMEBUFFER_SRGB off
// stores them as-if-sRGB; the compositor then *decodes* → image looks too dark.
// Prefer linear GL_RGBA8 and passthrough blit (no encode) so midtones stay correct
// on WiVRn/Quest/Monado. Fall back to sRGB if the runtime has no RGBA8.
static int64_t ChooseXrSwapchainFormat() {
    uint32_t count = 0;
    if (!g_xrEnumerateSwapchainFormats ||
        g_xrEnumerateSwapchainFormats(g_xrSession, 0, &count, nullptr) != XR_SUCCESS ||
        count == 0) {
        VRMOD_LOG_WARN("xrEnumerateSwapchainFormats unavailable or empty, defaulting to GL_RGBA8");
        return GL_RGBA8;
    }

    std::vector<int64_t> formats(count);
    if (g_xrEnumerateSwapchainFormats(g_xrSession, count, &count, formats.data()) != XR_SUCCESS) {
        return GL_RGBA8;
    }

    // Best: linear RGBA8 (Source tonemap → no second gamma decode)
    for (int64_t f : formats) {
        if (f == GL_RGBA8) return f;
    }

    // Fallback: sRGB (may look dark with passthrough blit of display-referred data)
    for (int64_t f : formats) {
        if (f == GL_SRGB8_ALPHA8) return f;
    }

    // Next best: plain RGB8 if somehow present
    for (int64_t f : formats) {
        if (f == GL_RGB8) return f;
    }

    // Last resort: first format the runtime offered (it is guaranteed to work)
    if (!formats.empty()) {
        VRMOD_LOG_WARN("No preferred swapchain format found, using runtime first choice 0x%llx",
            (unsigned long long)formats[0]);
        return formats[0];
    }

    return GL_RGBA8;
}

bool XR_CreateSwapchains(char* errMsg, int errMsgLen) {
    if (!LoadGLExtensions()) {
        snprintf(errMsg, errMsgLen, "VRMOD OpenXR: Failed to load GL framebuffer extensions");
        return false;
    }

    g_xrSwapchainWidth = g_xrRecommendedWidth;
    g_xrSwapchainHeight = g_xrRecommendedHeight;

    // Choose a format the runtime actually supports (fixes black screen on many Linux OpenXR setups)
    int64_t chosen = ChooseXrSwapchainFormat();
    g_xrSwapchainFormat = chosen;
    VRMOD_LOG_INFO("Selected swapchain format: 0x%llx (GL_RGBA8=0x8058, GL_SRGB8_ALPHA8=0x8C43)",
        (unsigned long long)chosen);

    for (int eye = 0; eye < 2; eye++) {
        XrSwapchainCreateInfo sci = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        // SAMPLED_BIT is required so the compositor can sample the images.
        // Without it many runtimes produce images that read as black.
        sci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                         XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                         XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        sci.format = chosen;
        sci.sampleCount = 1;
        sci.width = g_xrSwapchainWidth;
        sci.height = g_xrSwapchainHeight;
        sci.faceCount = 1;
        sci.arraySize = 1;
        sci.mipCount = 1;

        XrResult res = g_xrCreateSwapchain(g_xrSession, &sci, &g_swapchains[eye]);
        if (res != XR_SUCCESS) {
            snprintf(errMsg, errMsgLen, "VRMOD OpenXR: xrCreateSwapchain failed for eye %d format 0x%llx (%s)",
                eye, (unsigned long long)chosen, XR_ResultToString(res));
            return false;
        }

        // Enumerate images
        g_xrEnumerateSwapchainImages(g_swapchains[eye], 0, &g_swapchainImageCount[eye], nullptr);
        g_swapchainImages[eye] = new XrSwapchainImageOpenGLKHR[g_swapchainImageCount[eye]];
        for (uint32_t i = 0; i < g_swapchainImageCount[eye]; i++) {
            g_swapchainImages[eye][i] = {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR};
        }
        g_xrEnumerateSwapchainImages(g_swapchains[eye], g_swapchainImageCount[eye],
            &g_swapchainImageCount[eye], (XrSwapchainImageBaseHeader*)g_swapchainImages[eye]);

        VRMOD_LOG_INFO("Eye %d swapchain: %u images, %ux%u", eye,
            g_swapchainImageCount[eye], g_xrSwapchainWidth, g_xrSwapchainHeight);
    }

    // Create FBOs for blitting
    glGenFramebuffersPtr(1, &g_blitFBO);
    glGenFramebuffersPtr(1, &g_blitSrcFBO);

    // Pre-allocate eye collectors at swapchain size (once) — never glGen mid-frame under mode 2.
    if (g_xrSwapchainWidth > 0 && g_xrSwapchainHeight > 0) {
        XR_EnsureEyeCollectors(g_xrSwapchainWidth, g_xrSwapchainHeight);
    }

    VRMOD_LOG_INFO("OpenXR swapchains created successfully (format 0x%llx)",
        (unsigned long long)g_xrSwapchainFormat);
    return true;
}

void XR_DestroyEyeCollectors() {
    for (int e = 0; e < 2; e++) {
        for (int s = 0; s < 2; s++) {
            if (g_eyeStage[e][s]) {
                // Safe at full XR teardown only (not mid mat_queue session thrash).
                glDeleteTextures(1, &g_eyeStage[e][s]);
                g_eyeStage[e][s] = 0;
            }
        }
    }
    g_eyeStageW = g_eyeStageH = 0;
    g_eyeStageReady = false;
    g_eyeStageWrite = 0;
    g_eyeStageRead = 1;
}

void XR_EnsureEyeCollectors(uint32_t eyeW, uint32_t eyeH) {
    if (eyeW < 16) eyeW = 16;
    if (eyeH < 16) eyeH = 16;
    if (g_eyeStage[0][0] && g_eyeStageW == eyeW && g_eyeStageH == eyeH)
        return;
    // Size change: abandon old IDs (no delete mid-session — leak until shutdown).
    if (g_eyeStageW != eyeW || g_eyeStageH != eyeH) {
        for (int e = 0; e < 2; e++)
            for (int s = 0; s < 2; s++)
                g_eyeStage[e][s] = 0;
        g_eyeStageReady = false;
    }
    g_eyeStageW = eyeW;
    g_eyeStageH = eyeH;
    for (int e = 0; e < 2; e++) {
        for (int s = 0; s < 2; s++) {
            if (g_eyeStage[e][s]) continue;
            glGenTextures(1, &g_eyeStage[e][s]);
            glBindTexture(GL_TEXTURE_2D, g_eyeStage[e][s]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)eyeW, (GLsizei)eyeH,
                0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    VRMOD_LOG_INFO("Eye collectors ready %ux%u (double-buffered)", eyeW, eyeH);
}

bool XR_HasCollectedEyes() { return g_eyeStageReady; }
void XR_SetPreferCollectedEyes(bool prefer) { g_preferCollectedEyes = prefer; }
bool XR_PreferCollectedEyes() { return g_preferCollectedEyes; }

bool XR_CollectEyesFromEngine(const float textureBounds[8]) {
    // Soft-fail only — never assert. Mid-session GL alloc/blit under mat_queue 2
    // is the crash path; callers gate Collect to mode < 2.
    if (!LoadGLExtensions() || !glBlitFramebufferPtr) return false;
    if (!g_blitFBO || !g_blitSrcFBO) return false;

    GLuint leftSrc = g_leftEyeColorTex ? g_leftEyeColorTex
        : (g_leftEyeTexture ? g_leftEyeTexture : 0);
    GLuint rightSrc = g_rightEyeColorTex ? g_rightEyeColorTex
        : (g_rightEyeTexture ? g_rightEyeTexture : 0);
    GLuint sbs = g_vrRtColorTex ? g_vrRtColorTex
        : (g_sharedTexture ? g_sharedTexture : g_captureTexture);
    bool havePerEye = leftSrc && rightSrc && leftSrc != rightSrc
        && g_leftEyeFBO != 0 && g_rightEyeFBO != 0;
    if (!havePerEye && !sbs) return false;

    GLint srcW = g_knownSubmitSrcW;
    GLint srcH = g_knownSubmitSrcH;
    if (srcW <= 0 || srcH <= 0) {
        srcW = (GLint)(g_xrSwapchainWidth > 0 ? g_xrSwapchainWidth * 2 : 0);
        srcH = (GLint)(g_xrSwapchainHeight > 0 ? g_xrSwapchainHeight : 0);
    }
    if (srcW < 32 || srcH < 32) return false;

    uint32_t eyeW = g_xrSwapchainWidth > 0 ? g_xrSwapchainWidth : (uint32_t)(srcW / 2);
    uint32_t eyeH = g_xrSwapchainHeight > 0 ? g_xrSwapchainHeight : (uint32_t)srcH;
    // Only ensure collectors if missing — never resize/recreate mid-session (worker race).
    if (!g_eyeStage[0][0] || !g_eyeStage[1][0]) {
        XR_EnsureEyeCollectors(eyeW, eyeH);
    }
    if (!g_eyeStage[0][g_eyeStageWrite] || !g_eyeStage[1][g_eyeStageWrite])
        return false;

    GLint prevR = 0, prevD = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevR);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevD);
    const int wslot = g_eyeStageWrite;
    int okEyes = 0;

    for (int eye = 0; eye < 2; eye++) {
        GLuint srcTex = havePerEye ? (eye == 0 ? leftSrc : rightSrc) : sbs;
        if (!srcTex || !g_eyeStage[eye][wslot]) continue;

        // Lua textureBounds for U crop + V crop/pan — but NEVER flip orientation here.
        // Linux Lua often passes inverted V (v0>v1) as a *submit* flip signal. Using that
        // as glBlit srcY order flips the collector; submit then flips again → HMD upside-down
        // while desktop (different path) stays fine. Crop with ordered V only.
        float u0, u1, v0, v1;
        if (textureBounds) {
            u0 = textureBounds[eye * 4 + 0];
            v0 = textureBounds[eye * 4 + 1];
            u1 = textureBounds[eye * 4 + 2];
            v1 = textureBounds[eye * 4 + 3];
        } else if (havePerEye) {
            u0 = 0.f; u1 = 1.f; v0 = 0.f; v1 = 1.f;
        } else {
            u0 = (eye == 0) ? 0.f : 0.5f;
            u1 = (eye == 0) ? 0.5f : 1.f;
            v0 = 0.f; v1 = 1.f;
        }
        if (havePerEye) {
            // Map SBS-space bounds (0–0.5 / 0.5–1) → per-eye 0–1 UV
            if (eye == 0) {
                u0 = u0 * 2.f;
                u1 = u1 * 2.f;
            } else {
                u0 = (u0 - 0.5f) * 2.f;
                u1 = (u1 - 0.5f) * 2.f;
            }
            if (u0 < 0.f) u0 = 0.f;
            if (u1 > 1.f) u1 = 1.f;
            if (u0 > 1.f) u0 = 1.f;
            if (u1 < 0.f) u1 = 0.f;
        } else {
            if (!(std::fabs(u1 - u0) > 0.001f)) {
                u0 = (eye == 0) ? 0.f : 0.5f;
                u1 = (eye == 0) ? 0.5f : 1.f;
            }
        }
        // Ordered U (left→right)
        if (u0 > u1) {
            float t = u0; u0 = u1; u1 = t;
        }
        // Ordered V for crop only — strip submit-style inversion so we do not flip
        {
            float va = (v0 < v1) ? v0 : v1;
            float vb = (v0 < v1) ? v1 : v0;
            if (vb - va < 0.001f) {
                va = 0.f;
                vb = 1.f;
            }
            v0 = va;
            v1 = vb;
        }

        // SBS: full stereo RT. Per-eye: each RT is already one eye (0–1 UV after map).
        GLint texW = srcW;
        GLint texH = srcH;
        if (havePerEye) {
            texW = (GLint)(g_xrSwapchainWidth > 0 ? g_xrSwapchainWidth : (srcW > 1 ? srcW / 2 : srcW));
            texH = (GLint)(g_xrSwapchainHeight > 0 ? g_xrSwapchainHeight : srcH);
            if (texW < 32) texW = (srcW > 1) ? srcW / 2 : srcW;
            if (texH < 32) texH = srcH;
        }

        GLint sx0 = (GLint)(u0 * (float)texW);
        GLint sx1 = (GLint)(u1 * (float)texW);
        GLint sy0 = (GLint)(v0 * (float)texH);
        GLint sy1 = (GLint)(v1 * (float)texH);
        auto clampX = [&](GLint& x) {
            if (x < 0) x = 0;
            if (x > texW) x = texW;
        };
        auto clampY = [&](GLint& y) {
            if (y < 0) y = 0;
            if (y > texH) y = texH;
        };
        clampX(sx0); clampX(sx1);
        clampY(sy0); clampY(sy1);
        if (sx0 == sx1) continue;
        if (sy0 == sy1) {
            if (sy1 < texH) sy1 = sy0 + 1;
            else sy0 = sy1 - 1;
        }
        // Enforce sy0 < sy1 (ordered) — never flip into staging
        if (sy0 > sy1) {
            GLint t = sy0; sy0 = sy1; sy1 = t;
        }

        glBindFramebufferPtr(GL_READ_FRAMEBUFFER, g_blitSrcFBO);
        glFramebufferTexture2DPtr(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srcTex, 0);
        glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, g_blitFBO);
        glFramebufferTexture2DPtr(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_eyeStage[eye][wslot], 0);
        if (glCheckFramebufferStatusPtr(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE ||
            glCheckFramebufferStatusPtr(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            continue;
        }
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        // Ordered src → ordered dest. Orientation flip is submit-only (one flip on Linux).
        glBlitFramebufferPtr(sx0, sy0, sx1, sy1, 0, 0, (GLint)g_eyeStageW, (GLint)g_eyeStageH,
            GL_COLOR_BUFFER_BIT, GL_LINEAR);
        okEyes++;
    }

    glBindFramebufferPtr(GL_READ_FRAMEBUFFER, prevR);
    glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, prevD);

    if (okEyes < 2) return false;

    g_eyeStageRead = wslot;
    g_eyeStageWrite = 1 - wslot;
    g_eyeStageReady = true;
    g_preferCollectedEyes = true;
    return true;
}

void XR_DestroySwapchains() {
    XR_DestroyEyeCollectors();
    if (g_blitFBO) {
        glDeleteFramebuffersPtr(1, &g_blitFBO);
        g_blitFBO = 0;
    }
    if (g_blitSrcFBO) {
        glDeleteFramebuffersPtr(1, &g_blitSrcFBO);
        g_blitSrcFBO = 0;
    }
    for (int eye = 0; eye < 2; eye++) {
        if (g_swapchains[eye] != XR_NULL_HANDLE) {
            g_xrDestroySwapchain(g_swapchains[eye]);
            g_swapchains[eye] = XR_NULL_HANDLE;
        }
        delete[] g_swapchainImages[eye];
        g_swapchainImages[eye] = nullptr;
        g_swapchainImageCount[eye] = 0;
    }
    g_xrSwapchainFormat = 0;
    VRMOD_LOG_INFO("OpenXR swapchains destroyed");
}

// Deduplicated error tracking to avoid spamming identical messages
static int  s_lastSubmitErrCode = 0;
static int  s_sameErrCount = 0;
static bool s_lastSubmitOk = true;

XrSubmitResult XR_SubmitStolenTexture(unsigned int stolenTexture, const float textureBounds[8]) {
    XrSubmitResult result;
    result.ok = false;
    result.errCode = 0;
    result.errMsg[0] = '\0';

    // Async exit gate (also checked in Lua SubmitSharedTexture).
    if (!XR_IsSubmitEnabled()) {
        result.errCode = -3;
        snprintf(result.errMsg, sizeof(result.errMsg), "Submit disabled");
        return result;
    }

    if (!g_xrSessionRunning) {
        result.errCode = -1;
        snprintf(result.errMsg, sizeof(result.errMsg), "Session not running");
        return result;
    }

    static int s_submitCallCount = 0;
    if ((s_submitCallCount++ % 90) == 0) {
        VRMOD_LOG_INFO("SubmitStolenTexture entered (every ~90 frames), stolenTex=%u bounds[0]=%.2f", stolenTexture, textureBounds[0]);
    }

    // If the WaitAndBegin we did in UpdatePosesAndActions said we should not render this frame,
    // just end it with no layers (prevents submitting stale/should-not-render frames which can appear black).
    if (!g_xrFrameState.shouldRender) {
        VRMOD_LOG_INFO("Submit: shouldRender=false, ending frame with 0 layers");
        XR_EndFrame();
        result.ok = true; // not a hard error
        return result;
    }

    // Locate views for this frame
    XrViewLocateInfo vli = {XR_TYPE_VIEW_LOCATE_INFO};
    vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    vli.displayTime = g_xrFrameState.predictedDisplayTime;
    vli.space = g_xrStageSpace;

    XrViewState viewState = {XR_TYPE_VIEW_STATE};
    uint32_t viewCount = 0;
    g_views[0] = {XR_TYPE_VIEW};
    g_views[1] = {XR_TYPE_VIEW};
    XrResult res = g_xrLocateViews(g_xrSession, &vli, &viewState, 2, &viewCount, g_views);
    if (res != XR_SUCCESS || viewCount < 2) {
        result.errCode = (int)res;
        snprintf(result.errMsg, sizeof(result.errMsg), "xrLocateViews failed: %s",
            XR_ResultToString(res));
        // Log only on state change
        if (result.errCode != s_lastSubmitErrCode || s_lastSubmitOk) {
            VRMOD_LOG_WARN("%s", result.errMsg);
            s_lastSubmitErrCode = result.errCode;
            s_lastSubmitOk = false;
            s_sameErrCount = 0;
        }
        // End frame with no layers
        XR_EndFrame();
        return result;
    }

    // Log the layer view poses (the ones used for the composition layer) so we can see if the runtime
    // is providing live, varying head motion in the data the compositor uses for head tracking.
    // Compare to the "HMD pose" log from the game tracking.
    static int s_layerViewLog = 0;
    if ((++s_layerViewLog % 30) == 0) {
        VRMOD_LOG_INFO("Layer view0: pos(%.3f,%.3f,%.3f)  view1: pos(%.3f,%.3f,%.3f)",
            g_views[0].pose.position.x, g_views[0].pose.position.y, g_views[0].pose.position.z,
            g_views[1].pose.position.x, g_views[1].pose.position.y, g_views[1].pose.position.z);
    }

    // Refresh the hmd pose for the game (used by GetPoses / Lua tracking and RenderViews camera)
    // from the live layer view data. This makes the game's camera follow the head motion
    // that the runtime provides for the composition layer (with 1 frame lag, which is typical).
    {
        // Average eye positions for head pose (center between eyes). This matches the
        // behavior expected from the old OpenVR HMD pose and fixes left-eye-specific
        // roll artifacts (roll left causing view to rise) that appeared after the
        // OpenVR->OpenXR migration (where using eye 0 as "hmd" made the left eye the
        // roll center).
        XrVector3f headPos = {
            (g_views[0].pose.position.x + g_views[1].pose.position.x) * 0.5f,
            (g_views[0].pose.position.y + g_views[1].pose.position.y) * 0.5f,
            (g_views[0].pose.position.z + g_views[1].pose.position.z) * 0.5f
        };
        XrSpaceLocation tempLoc = {XR_TYPE_SPACE_LOCATION};
        tempLoc.locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        tempLoc.pose.position = headPos;
        tempLoc.pose.orientation = g_views[0].pose.orientation;
        g_xrHMDPose = ConvertXrPose(tempLoc);
    }

    // Also refresh eye poses so Lua tracking has the latest headset eye values (for render or other use)
    if (g_xrEyePosesValid || true) {
      XrSpaceLocation tloc = {XR_TYPE_SPACE_LOCATION};
      tloc.locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
      tloc.pose = g_views[0].pose;
      g_xrEyePoses[0] = ConvertXrPose(tloc);
      tloc.pose = g_views[1].pose;
      g_xrEyePoses[1] = ConvertXrPose(tloc);
      g_xrEyeFovs[0] = g_views[0].fov;
      g_xrEyeFovs[1] = g_views[1].fov;
      g_xrEyePosesValid = true;
    }

    // Per-eye path. Prefer module staging (collected after previous stereo + MatQueue drain).
    // Backend times this submit (WaitFrame already done); staging isolates live engine RTs.
    GLuint perEyeSrc[2] = {0, 0};
    bool havePerEye = false;
    GLuint srcTex = stolenTexture;

    if (g_preferCollectedEyes && g_eyeStageReady
        && g_eyeStage[0][g_eyeStageRead] && g_eyeStage[1][g_eyeStageRead]) {
        perEyeSrc[0] = g_eyeStage[0][g_eyeStageRead];
        perEyeSrc[1] = g_eyeStage[1][g_eyeStageRead];
        havePerEye = true;
        srcTex = perEyeSrc[0];
        // Staging is already one eye each at swapchain size — full rect.
        // Override known size for this submit path.
        if (g_eyeStageW > 0 && g_eyeStageH > 0) {
            VRMOD_SetKnownSubmitSize(g_eyeStageW, g_eyeStageH);
        }
    } else {
        // mat_queue_mode 2: do NOT rebind engine FBOs every frame to re-query
        // attachments — that races material workers. Trust IDs captured at share time.
        GLuint leftSrc = g_leftEyeColorTex ? g_leftEyeColorTex
            : (g_leftEyeTexture ? g_leftEyeTexture : 0);
        GLuint rightSrc = g_rightEyeColorTex ? g_rightEyeColorTex
            : (g_rightEyeTexture ? g_rightEyeTexture : 0);
        perEyeSrc[0] = leftSrc ? leftSrc : stolenTexture;
        perEyeSrc[1] = rightSrc ? rightSrc : stolenTexture;

        if (!srcTex && g_vrRtColorTex) srcTex = g_vrRtColorTex;
        if (!srcTex && g_sharedTexture) srcTex = g_sharedTexture;
        if (!srcTex && g_captureTexture) srcTex = g_captureTexture;

        bool leftOk = (perEyeSrc[0] != 0);
        bool rightOk = (perEyeSrc[1] != 0);
        // Require both FBOs — gen-steal color+depth as L/R was false dual (black eye).
        havePerEye = leftOk && rightOk && (perEyeSrc[0] != perEyeSrc[1])
            && g_leftEyeFBO != 0 && g_rightEyeFBO != 0;
        if (!havePerEye) {
            if (g_vrRtColorTex) srcTex = g_vrRtColorTex;
            if (!srcTex && stolenTexture) srcTex = stolenTexture;
            if (srcTex) {
                perEyeSrc[0] = perEyeSrc[1] = srcTex;
            }
        }
    }
    if ((s_submitCallCount % 30) == 0) {
        if (havePerEye) {
            VRMOD_LOG_INFO("Submit using PER-EYE textures L=%u R=%u (leftFBO=%u rightFBO=%u)", perEyeSrc[0], perEyeSrc[1], g_leftEyeFBO, g_rightEyeFBO);
        } else {
            VRMOD_LOG_INFO("Submit using legacy SBS srcTex=%u (rtFBO=%u) bounds-crop L/R halves", srcTex, g_vrRtFBO);
        }
    }

    // Dimensions: Lua/ShareTexture known size FIRST — never glGetTexLevel on live
    // engine RTs (mat_queue_mode 2 races workers; glBindTexture mid-frame can SEGV).
    // Never glIsTexture (false negatives under mode 2).
    GLuint dimProbe = perEyeSrc[0] ? perEyeSrc[0] : (perEyeSrc[1] ? perEyeSrc[1] : srcTex);
    GLint srcWidth = g_knownSubmitSrcW;
    GLint srcHeight = g_knownSubmitSrcH;
    if (srcWidth <= 0 || srcHeight <= 0) {
        if (g_xrRecommendedWidth > 0 && g_xrRecommendedHeight > 0) {
            srcWidth = (GLint)(g_xrRecommendedWidth * 2);
            srcHeight = (GLint)g_xrRecommendedHeight;
            VRMOD_SetKnownSubmitSize((uint32_t)srcWidth, (uint32_t)srcHeight);
        }
    }
    static int s_zeroDimStreak = 0;
    // Non-zero ID + known size is enough to blit (crash log: tex=31 size=ok but glIsTexture failed).
    const bool haveSrcTex = (dimProbe != 0);
    if (!havePerEye && (!haveSrcTex || srcWidth <= 0 || srcHeight <= 0)) {
        s_zeroDimStreak++;
        result.errCode = -2;
        if (s_zeroDimStreak == 1 || (s_zeroDimStreak % 120) == 0) {
            snprintf(result.errMsg, sizeof(result.errMsg),
                "Source texture unavailable (tex=%u size=%dx%d known=%dx%d streak=%d)",
                (unsigned)dimProbe, (int)srcWidth, (int)srcHeight,
                g_knownSubmitSrcW, g_knownSubmitSrcH, s_zeroDimStreak);
            VRMOD_LOG_WARN("%s", result.errMsg);
            s_lastSubmitErrCode = result.errCode;
            s_lastSubmitOk = false;
        }
        XR_EndFrame();
        return result;
    }
    s_zeroDimStreak = 0;

    // No glFinish / no pre-blit glFlush — both stall mat_queue 2 workers.
    // Compositor sees completed images after the end-of-submit glFlush.

    // Direct path: attach srcTex and blit. Locate already done; submit g_views.

    GLint prevReadFBO = 0, prevDrawFBO = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFBO);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFBO);
    GLboolean prevSrgbEnabled = glIsEnabled(GL_FRAMEBUFFER_SRGB);

    // Always disable FRAMEBUFFER_SRGB during blit.
    // Source RT = display-referred. We prefer linear RGBA8 swapchains so these
    // bytes pass through. If runtime only has sRGB, still disable encode so we
    // don't linear→sRGB encode already-gamma content (would go darker still).
    glDisable(GL_FRAMEBUFFER_SRGB);

    XrCompositionLayerProjectionView projViews[2];

    for (int eye = 0; eye < 2; eye++) {
        XrSwapchainImageAcquireInfo acqInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t imageIndex = 0;
        res = g_xrAcquireSwapchainImage(g_swapchains[eye], &acqInfo, &imageIndex);
        if (res != XR_SUCCESS) {
            result.errCode = (int)res;
            snprintf(result.errMsg, sizeof(result.errMsg),
                "xrAcquireSwapchainImage failed eye %d: %s", eye, XR_ResultToString(res));
            if (result.errCode != s_lastSubmitErrCode || s_lastSubmitOk) {
                VRMOD_LOG_WARN("%s", result.errMsg);
                s_lastSubmitErrCode = result.errCode;
                s_lastSubmitOk = false;
            }
            XR_EndFrame();
            return result;
        }

        XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        // Bound wait — XR_INFINITE_DURATION freezes the game (and mat workers) if
        // the compositor stalls. 100ms is enough for a healthy runtime frame.
        waitInfo.timeout = 100000000; // 100 ms in nanoseconds
        res = g_xrWaitSwapchainImage(g_swapchains[eye], &waitInfo);
        if (res != XR_SUCCESS) {
            g_xrReleaseSwapchainImage(g_swapchains[eye], nullptr);
            result.errCode = (int)res;
            snprintf(result.errMsg, sizeof(result.errMsg),
                "xrWaitSwapchainImage failed eye %d: %s", eye, XR_ResultToString(res));
            if (result.errCode != s_lastSubmitErrCode || s_lastSubmitOk) {
                VRMOD_LOG_WARN("%s", result.errMsg);
                s_lastSubmitErrCode = result.errCode;
                s_lastSubmitOk = false;
            }
            XR_EndFrame();
            return result;
        }

        GLuint dstTexture = g_swapchainImages[eye][imageIndex].image;

        // Choose source texture for this eye.
        GLuint eyeSrcTex = havePerEye ? perEyeSrc[eye] : srcTex;
        // Query actual source dimensions (may differ per eye in theory, but we use per-eye RTs at recommended size).
        // Always use frame-level known size under mat_queue 2 (per-eye GL query lies).
        GLint eyeSrcW = srcWidth;
        GLint eyeSrcH = srcHeight;
        if (eyeSrcW <= 0 || eyeSrcH <= 0) {
            eyeSrcW = g_knownSubmitSrcW > 0 ? g_knownSubmitSrcW : (GLint)(g_xrSwapchainWidth > 0 ? g_xrSwapchainWidth * 2 : 2048);
            eyeSrcH = g_knownSubmitSrcH > 0 ? g_knownSubmitSrcH : (GLint)(g_xrSwapchainHeight > 0 ? g_xrSwapchainHeight : 1024);
        }

        // Direct attach + full (or inset) blit per eye. With per-eye RTs the rendered content
        // already incorporates the correct asymmetric frustum via the per-eye RenderView setup
        // (origin offset + projection from OpenXR). No more UV shifting for side-by-side packing.
        {
            GLint prevR = 0, prevD = 0;
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevR);
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevD);

            glBindFramebufferPtr(GL_READ_FRAMEBUFFER, g_blitSrcFBO);
            glFramebufferTexture2DPtr(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eyeSrcTex, 0);

            glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, g_blitFBO);
            glFramebufferTexture2DPtr(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTexture, 0);

            if (glCheckFramebufferStatusPtr(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE &&
                glCheckFramebufferStatusPtr(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {

                glReadBuffer(GL_COLOR_ATTACHMENT0);
                glDrawBuffer(GL_COLOR_ATTACHMENT0);

                // Rect selection — policy g_submitCropMode (Lua vrmod_submit_crop):
                //   SAFE(0): collector/per-eye = full eye; SBS = Lua bounds halves
                //   FULL(1): force full-eye UV (debug borders)
                //   FOV_CROP(2): experimental asymmetric FOV crop on per-eye only
                // IMPORTANT: Linux SBS bounds often invert V (v0>v1). Do not treat that
                // as invalid — would reset U and double the image.
                float u0 = (eye == 0) ? textureBounds[0] : textureBounds[4];
                float u1 = (eye == 0) ? textureBounds[2] : textureBounds[6];
                float v0 = (eye == 0) ? textureBounds[1] : textureBounds[5];
                float v1 = (eye == 0) ? textureBounds[3] : textureBounds[7];

                const float ins = 0.003f;
                const bool fromCollector = g_preferCollectedEyes && g_eyeStageReady
                    && perEyeSrc[eye] == g_eyeStage[eye][g_eyeStageRead];
                const bool singleEyeTex = fromCollector || havePerEye;
                const int cropMode = g_submitCropMode;

                if (singleEyeTex && cropMode == 2) {
                    // Experimental: crop symmetric overrender to OpenXR FOV
                    AsymmetricFovToUvCrop(g_views[eye].fov, &u0, &u1, &v0, &v1);
                } else if (fromCollector) {
                    // Collector already baked Lua scale/offset/lens into full eye stage → sample full
                    u0 = ins;
                    u1 = 1.0f - ins;
                    v0 = ins;
                    v1 = 1.0f - ins;
                } else if (singleEyeTex && cropMode == 1) {
                    // FULL debug: force full eye
                    u0 = ins;
                    u1 = 1.0f - ins;
                    v0 = ins;
                    v1 = 1.0f - ins;
                } else if (singleEyeTex && cropMode == 0) {
                    // Per-eye RT without collector: map SBS Lua bounds → eye 0–1 (U+V)
                    if (eye == 0) {
                        u0 = u0 * 2.f;
                        u1 = u1 * 2.f;
                    } else {
                        u0 = (u0 - 0.5f) * 2.f;
                        u1 = (u1 - 0.5f) * 2.f;
                    }
                    if (u0 < ins) u0 = ins;
                    if (u1 > 1.f - ins) u1 = 1.f - ins;
                    if (u1 <= u0 + 0.001f) {
                        u0 = ins;
                        u1 = 1.f - ins;
                    }
                    // V kept from Lua (may be inverted on Linux)
                    if (std::fabs(v1 - v0) < 0.001f) {
                        v0 = ins;
                        v1 = 1.0f - ins;
                    }
                } else if (!(u1 > u0 + 0.001f) && !(u0 > u1 + 0.001f)) {
                    // SBS path: repair broken U only
                    if (eye == 0) {
                        u0 = ins;
                        u1 = 0.5f;
                    } else {
                        u0 = 0.5f;
                        u1 = 1.0f - ins;
                    }
                }
                // SBS direct: empty V only → full. Inverted V intentional (flip via blit).
                if (!singleEyeTex && cropMode != 1 && std::fabs(v1 - v0) < 0.001f) {
                    v0 = ins;
                    v1 = 1.0f - ins;
                }

                // g_rtTextureNeedsVFlip: mirror V into GL bottom-left space when bounds
                // were authored in D3D-style (low V = top). If bounds already inverted
                // (Linux Lua convention), they already encode the flip — do not mirror again.
                // Staging uses ordered V → always apply one flip on Linux via dest when needed.
                const bool boundsVInverted = (v0 > v1);
                if (g_rtTextureNeedsVFlip && !boundsVInverted) {
                    float tmp0 = 1.0f - v1;
                    float tmp1 = 1.0f - v0;
                    v0 = tmp0;
                    v1 = tmp1;
                }

                // Source rect in texels (OpenGL origin = bottom-left).
                // srcY0 > srcY1 is allowed: glBlitFramebuffer flips when src Y is inverted.
                GLint srcX0 = (GLint)(u0 * eyeSrcW);
                GLint srcX1 = (GLint)(u1 * eyeSrcW);
                GLint srcY0 = (GLint)(v0 * eyeSrcH);
                GLint srcY1 = (GLint)(v1 * eyeSrcH);
                // Clamp X into texture
                if (srcX0 < 0) srcX0 = 0;
                if (srcX1 < 0) srcX1 = 0;
                if (srcX0 > eyeSrcW) srcX0 = eyeSrcW;
                if (srcX1 > eyeSrcW) srcX1 = eyeSrcW;
                if (srcX0 == srcX1) {
                    if (srcX1 < eyeSrcW) srcX1 = srcX0 + 1;
                    else srcX0 = srcX1 - 1;
                }
                // Clamp Y independently (order may be flipped)
                if (srcY0 < 0) srcY0 = 0;
                if (srcY1 < 0) srcY1 = 0;
                if (srcY0 > eyeSrcH) srcY0 = eyeSrcH;
                if (srcY1 > eyeSrcH) srcY1 = eyeSrcH;
                if (srcY0 == srcY1) {
                    if (srcY1 < eyeSrcH) srcY1 = srcY0 + 1;
                    else srcY0 = srcY1 - 1;
                }

                // Primary path: glBlitFramebuffer.
                // No glReadPixels here — that forces a full GPU sync and races
                // mat_queue_mode 2 workers ("Illegal termination of worker thread").
                // OpenXR OpenGL swapchain: first row = top of view. Source Engine GL RTs
                // store top-of-scene at high Y. When bounds already invert src Y (Linux),
                // the blit itself flips. When bounds are ordered low→high, invert dest Y
                // on Linux so the image is not upside-down in the HMD.
                GLint dstY0 = 0;
                GLint dstY1 = (GLint)g_xrSwapchainHeight;
                const bool srcYFlips = (srcY0 > srcY1);
                // Need one vertical flip total for Linux GL→OpenXR.
                // - If src Y already inverted (blit flips): dest identity.
                // - Else if g_rtTextureNeedsVFlip (or default Linux): invert dest Y.
                if (!srcYFlips && g_rtTextureNeedsVFlip) {
                    dstY0 = (GLint)g_xrSwapchainHeight;
                    dstY1 = 0;
                }

                glBindFramebufferPtr(GL_READ_FRAMEBUFFER, g_blitSrcFBO);
                glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, g_blitFBO);
                glReadBuffer(GL_COLOR_ATTACHMENT0);
                glDrawBuffer(GL_COLOR_ATTACHMENT0);

                glBlitFramebufferPtr(
                    srcX0, srcY0, srcX1, srcY1,
                    0, dstY0, (GLint)g_xrSwapchainWidth, dstY1,
                    GL_COLOR_BUFFER_BIT, GL_LINEAR);

                if ((s_submitCallCount % 90) == 0 && eye == 0) {
                    VRMOD_LOG_INFO("BLIT eye%d u[%.3f-%.3f] v[%.3f-%.3f] srcRect(%d,%d)-(%d,%d) -> dst %ux%u perEye=%d",
                        eye, u0, u1, v0, v1, srcX0, srcY0, srcX1, srcY1,
                        g_xrSwapchainWidth, g_xrSwapchainHeight, (int)havePerEye);
                }
            }

            glBindFramebufferPtr(GL_READ_FRAMEBUFFER, prevR);
            glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, prevD);
        }

        XrSwapchainImageReleaseInfo relInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        g_xrReleaseSwapchainImage(g_swapchains[eye], &relInfo);

        // Use the views located at the top of this Submit (with the frame's predictedDisplayTime).
        projViews[eye] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        projViews[eye].pose = g_views[eye].pose;
        projViews[eye].fov = g_views[eye].fov;
        projViews[eye].subImage.swapchain = g_swapchains[eye];
        projViews[eye].subImage.imageRect.offset = {0, 0};
        projViews[eye].subImage.imageRect.extent = { (int32_t)g_xrSwapchainWidth, (int32_t)g_xrSwapchainHeight };
        projViews[eye].subImage.imageArrayIndex = 0;
    }
    // Restore GL state (FBO bindings + sRGB enable)
    glBindFramebufferPtr(GL_READ_FRAMEBUFFER, prevReadFBO);
    glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
    if (prevSrgbEnabled) glEnable(GL_FRAMEBUFFER_SRGB); else glDisable(GL_FRAMEBUFFER_SRGB);

    // glFlush can stall MatQueue workers under mat_queue_mode 2.
    // Compositor usually sees images after EndFrame without an explicit flush.
    // (glFinish is always forbidden — kills CThread.)
    // glFlush();

    // Submit frame
    XrCompositionLayerProjection projLayer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projLayer.space = g_xrStageSpace;
    projLayer.viewCount = 2;
    projLayer.views = projViews;

    const XrCompositionLayerBaseHeader* layers[] = {
        (XrCompositionLayerBaseHeader*)&projLayer
    };

    XrFrameEndInfo fei = {XR_TYPE_FRAME_END_INFO};
    fei.displayTime = g_xrFrameState.predictedDisplayTime;
    fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    fei.layerCount = 1;
    fei.layers = layers;

    if ((s_submitCallCount % 90) == 0) {
        VRMOD_LOG_INFO("xrEndFrame: submitting layer space=stage views=2 srcTex=%u swap=%ux%u fmt=0x%llx",
            srcTex, g_xrSwapchainWidth, g_xrSwapchainHeight, (unsigned long long)g_xrSwapchainFormat);
    }
    res = g_xrEndFrame(g_xrSession, &fei);
    // Frame is closed whether EndFrame succeeded or not (do not double-end).
    XR_MarkFrameEnded();
    if (res != XR_SUCCESS) {
        result.errCode = (int)res;
        snprintf(result.errMsg, sizeof(result.errMsg), "xrEndFrame failed: %s",
            XR_ResultToString(res));
        if (result.errCode != s_lastSubmitErrCode || s_lastSubmitOk) {
            VRMOD_LOG_WARN("%s", result.errMsg);
            s_lastSubmitErrCode = result.errCode;
            s_lastSubmitOk = false;
        }
        return result;
    }

    // Success — log periodically (was every frame → 60MB logs).
    if ((s_submitCallCount % 90) == 0) {
        VRMOD_LOG_INFO("EndFrame with layer succeeded (count=%d, state=%d)", s_submitCallCount, (int)g_xrSessionState);
    }

    result.ok = true;
    if (!s_lastSubmitOk) {
        VRMOD_LOG_INFO("Submit recovered after %d errors", s_sameErrCount);
    }
    // One-time confirmation that we are actually feeding images to OpenXR (helps distinguish
    // "tracking only" from "images submitted but black for other reasons").
    static bool s_firstSubmitLogged = false;
    if (!s_firstSubmitLogged) {
        VRMOD_LOG_INFO("First successful OpenXR submit: swapchain %ux%u format=0x%llx srcTex=%u",
            g_xrSwapchainWidth, g_xrSwapchainHeight,
            (unsigned long long)g_xrSwapchainFormat, stolenTexture);
        s_firstSubmitLogged = true;
    }
    s_lastSubmitOk = true;
    s_lastSubmitErrCode = 0;
    s_sameErrCount = 0;

    return result;
}

#endif // !_WIN32