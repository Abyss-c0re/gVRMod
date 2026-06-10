#include "xr_render.h"
#include "core/vrmod_log.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <algorithm>

// The hmd pose (defined in xr_input.cpp) and conversion - used to feed live layer view pose
// back to the game's tracking so RenderViews use current head pose.
extern PoseResult g_xrHMDPose;
extern PoseResult ConvertXrPose(const XrSpaceLocation& loc);

extern PoseResult g_xrEyePoses[2];
extern bool g_xrEyePosesValid;
extern XrFovf g_xrEyeFovs[2];

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
// We prefer linear formats for the current side-by-side blit path (GMod renders in linear).
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

    // Best: exact linear RGBA8 (matches our Source RT content)
    for (int64_t f : formats) {
        if (f == GL_RGBA8) return f;
    }

    // Acceptable: sRGB8_ALPHA8 (we will enable GL_FRAMEBUFFER_SRGB during blit)
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

    VRMOD_LOG_INFO("OpenXR swapchains created successfully (format 0x%llx)",
        (unsigned long long)g_xrSwapchainFormat);
    return true;
}

void XR_DestroySwapchains() {
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

XrSubmitResult XR_SubmitStolenTexture(GLuint stolenTexture, const float textureBounds[8]) {
    XrSubmitResult result;
    result.ok = false;
    result.errCode = 0;
    result.errMsg[0] = '\0';

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

    // Per-eye path (preferred): each eye has its own RT of recommended size.
    // The content of each RT was rendered with that eye's correct (asymmetric) projection
    // and full viewport, so we submit the full texture rect per eye (small inset for safety).
    // Fallback to the classic single stolen + bounds only if per-eye textures are not populated.
    GLuint perEyeSrc[2] = {0, 0};

    // Resolve left
    GLuint leftSrc = g_leftEyeTexture;
    if (g_leftEyeFBO != 0 && glBindFramebufferPtr && glGetFramebufferAttachmentParameterivPtr) {
        GLint prevFB = 0; glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFB);
        glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, g_leftEyeFBO);
        GLint attached = 0;
        glGetFramebufferAttachmentParameterivPtr(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE, &attached);
        glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, prevFB);
        if (attached != 0 && glIsTexture((GLuint)attached)) leftSrc = (GLuint)attached;
    }
    if (g_leftEyeColorTex && glIsTexture(g_leftEyeColorTex)) leftSrc = g_leftEyeColorTex;
    perEyeSrc[0] = leftSrc ? leftSrc : stolenTexture;

    // Resolve right
    GLuint rightSrc = g_rightEyeTexture;
    if (g_rightEyeFBO != 0 && glBindFramebufferPtr && glGetFramebufferAttachmentParameterivPtr) {
        GLint prevFB = 0; glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFB);
        glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, g_rightEyeFBO);
        GLint attached = 0;
        glGetFramebufferAttachmentParameterivPtr(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE, &attached);
        glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, prevFB);
        if (attached != 0 && glIsTexture((GLuint)attached)) rightSrc = (GLuint)attached;
    }
    if (g_rightEyeColorTex && glIsTexture(g_rightEyeColorTex)) rightSrc = g_rightEyeColorTex;
    perEyeSrc[1] = rightSrc ? rightSrc : stolenTexture;

    // Legacy single-src resolve (only used if per-eye not available).
    GLuint srcTex = stolenTexture;
    if (g_vrRtFBO != 0 && glBindFramebufferPtr && glGetFramebufferAttachmentParameterivPtr) {
        GLint prevFB = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFB);
        glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, g_vrRtFBO);
        GLint attached = 0;
        glGetFramebufferAttachmentParameterivPtr(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE, &attached);
        glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, prevFB);
        if (attached != 0 && glIsTexture((GLuint)attached)) srcTex = (GLuint)attached;
    }
    if (g_captureTexture && glIsTexture(g_captureTexture)) {
        // capture kept for debug/compat only
    }

    bool havePerEye = (perEyeSrc[0] != 0 && glIsTexture(perEyeSrc[0])) || (perEyeSrc[1] != 0 && glIsTexture(perEyeSrc[1]));
    if ((s_submitCallCount % 30) == 0) {
        if (havePerEye) {
            VRMOD_LOG_INFO("Submit using PER-EYE textures L=%u R=%u (leftFBO=%u rightFBO=%u)", perEyeSrc[0], perEyeSrc[1], g_leftEyeFBO, g_rightEyeFBO);
        } else {
            VRMOD_LOG_INFO("Submit using legacy single srcTex=%u (rtFBO=%u)", srcTex, g_vrRtFBO);
        }
    }

    // Query dimensions from a usable texture (prefer any per-eye for safety, else legacy).
    GLint srcWidth = 0, srcHeight = 0;
    GLuint dimProbe = (perEyeSrc[0] && glIsTexture(perEyeSrc[0])) ? perEyeSrc[0] : ((perEyeSrc[1] && glIsTexture(perEyeSrc[1])) ? perEyeSrc[1] : srcTex);
    if (dimProbe && glIsTexture(dimProbe)) {
        glBindTexture(GL_TEXTURE_2D, dimProbe);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &srcWidth);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &srcHeight);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    // Only hard-fail if we have literally nothing to submit and legacy path would be used.
    if (!havePerEye && (srcWidth == 0 || srcHeight == 0)) {
        result.errCode = -2;
        snprintf(result.errMsg, sizeof(result.errMsg), "Source texture has zero dimensions (capture or stolen)");
        if (result.errCode != s_lastSubmitErrCode || s_lastSubmitOk) {
            VRMOD_LOG_WARN("%s", result.errMsg);
            s_lastSubmitErrCode = result.errCode;
            s_lastSubmitOk = false;
        }
        XR_EndFrame();
        return result;
    }

    // Make sure the draw that touched the source (rtMaterial into captureRt or engine into main RT)
    // has completed before we attach it for the per-eye blits.
    glFinish();

    // Direct path (modeled on backup xr_render.cpp): attach srcTex (g_captureTexture preferred) and blit.
    // No s_capture/Get/TexSub in the submitted data path. Locate was already done; we will submit layer with g_views.

    GLint prevReadFBO = 0, prevDrawFBO = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFBO);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFBO);
    GLboolean prevSrgbEnabled = glIsEnabled(GL_FRAMEBUFFER_SRGB);

    const bool useSrgbFramebuffer = (g_xrSwapchainFormat == GL_SRGB8_ALPHA8);
    if (useSrgbFramebuffer) {
        glEnable(GL_FRAMEBUFFER_SRGB);
    } else {
        glDisable(GL_FRAMEBUFFER_SRGB);
    }

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
        waitInfo.timeout = XR_INFINITE_DURATION;
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
        GLint eyeSrcW = 0, eyeSrcH = 0;
        if (eyeSrcTex && glIsTexture(eyeSrcTex)) {
            glBindTexture(GL_TEXTURE_2D, eyeSrcTex);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &eyeSrcW);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &eyeSrcH);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        if (eyeSrcW == 0 || eyeSrcH == 0) {
            eyeSrcW = srcWidth; eyeSrcH = srcHeight;
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

                // Rect selection from Lua-provided textureBounds (allows tuning "bounds" / inset / asymmetry
                // from Lua debug convars without recompiling the mod). Falls back to near-full rect if bounds
                // look invalid (e.g. not initialized).
                float u0 = (eye == 0) ? textureBounds[0] : textureBounds[4];
                float u1 = (eye == 0) ? textureBounds[2] : textureBounds[6];
                float v0 = (eye == 0) ? textureBounds[1] : textureBounds[5];
                float v1 = (eye == 0) ? textureBounds[3] : textureBounds[7];

                // safety fallback to near-full if bounds are zero or inverted (e.g. before first SetSubmitTextureBounds)
                if (u1 <= u0 + 0.001f || v1 <= v0 + 0.001f) {
                    const float ins = 0.003f;
                    u0 = ins; u1 = 1.0f - ins;
                    v0 = ins; v1 = 1.0f - ins;
                }

                // V ordering matches the previous successful mapping for GL RT orientation on Linux.

                if (g_rtTextureNeedsVFlip) {
                    // Engine RTs (from GetRenderTargetEx) on this platform (Linux/OpenGL)
                    // have inverted V compared to what the OpenXR compositor expects.
                    // Swap so that the quad samples the rendered content right-side-up.
                    // This reproduces the effect the old ComputeSubmitBounds + isWindows
                    // logic had for the packed-RT case.
                    std::swap(v0, v1);
                }

                glFinish();

                // Quick content check at center of the source rect we will copy.
                {
                    glReadBuffer(GL_COLOR_ATTACHMENT0);
                    unsigned char p[4] = {0};
                    float uc = (u0 + u1) * 0.5f;
                    float vc = (v0 + v1) * 0.5f;
                    GLint sx = (GLint)(uc * eyeSrcW);
                    GLint sy = (GLint)(vc * eyeSrcH);
                    glReadPixels(sx, sy, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
                    int sm = (int)p[0] + (int)p[1] + (int)p[2];
                    if (sm < 25) {
                        VRMOD_LOG_INFO("### EYE%d BLACK? src=%u center=(%u,%u,%u) sz=%dx%d", eye, eyeSrcTex, p[0],p[1],p[2], eyeSrcW, eyeSrcH);
                    } else if ((s_submitCallCount % 45) == 0) {
                        VRMOD_LOG_INFO("EYE%d content OK src=%u sum=%d sz=%dx%d", eye, eyeSrcTex, sm, eyeSrcW, eyeSrcH);
                    }
                }

                glBindFramebufferPtr(GL_READ_FRAMEBUFFER, 0);

                // Textured quad submission. Same V mapping as before for orientation.
                {
                    GLint prevViewport[4];
                    glGetIntegerv(GL_VIEWPORT, prevViewport);
                    GLboolean depthWasOn = glIsEnabled(GL_DEPTH_TEST);
                    GLboolean cullWasOn = glIsEnabled(GL_CULL_FACE);
                    GLboolean blendWasOn = glIsEnabled(GL_BLEND);
                    GLint prevTex;
                    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
                    GLint prevMatrixMode;
                    glGetIntegerv(GL_MATRIX_MODE, &prevMatrixMode);

                    glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, g_blitFBO);
                    glViewport(0, 0, g_xrSwapchainWidth, g_xrSwapchainHeight);
                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_CULL_FACE);
                    glDisable(GL_BLEND);

                    if (glActiveTexturePtr) glActiveTexturePtr(GL_TEXTURE0);
                    if (glUseProgramPtr) glUseProgramPtr(0);
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, eyeSrcTex);

                    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

                    glMatrixMode(GL_PROJECTION);
                    glPushMatrix();
                    glLoadIdentity();
                    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

                    glMatrixMode(GL_MODELVIEW);
                    glPushMatrix();
                    glLoadIdentity();

                    glBegin(GL_QUADS);
                        glTexCoord2f(u0, v0); glVertex2f(-1.0f, -1.0f);
                        glTexCoord2f(u1, v0); glVertex2f( 1.0f, -1.0f);
                        glTexCoord2f(u1, v1); glVertex2f( 1.0f,  1.0f);
                        glTexCoord2f(u0, v1); glVertex2f(-1.0f,  1.0f);
                    glEnd();

                    glFinish();

                    // Post sample
                    {
                        unsigned char dstSample[4] = {0};
                        glReadBuffer(GL_COLOR_ATTACHMENT0);
                        glReadPixels(g_xrSwapchainWidth / 2, g_xrSwapchainHeight / 2, 1, 1,
                                     GL_RGBA, GL_UNSIGNED_BYTE, dstSample);
                        int dstSum = (int)dstSample[0] + dstSample[1] + dstSample[2];
                        if ((s_submitCallCount % 45) == 0 && eye == 0) {
                            VRMOD_LOG_INFO("POST dst eye0 center sum=%d", dstSum);
                        }
                    }

                    glBindTexture(GL_TEXTURE_2D, prevTex);
                    glMatrixMode(GL_PROJECTION);
                    glPopMatrix();
                    glMatrixMode(GL_MODELVIEW);
                    glPopMatrix();
                    glMatrixMode(prevMatrixMode);

                    if (blendWasOn) glEnable(GL_BLEND);
                    if (cullWasOn) glEnable(GL_CULL_FACE);
                    if (depthWasOn) glEnable(GL_DEPTH_TEST);
                    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

                    glFlush();

                    if ((s_submitCallCount % 30) == 0 && eye == 0) {
                        VRMOD_LOG_INFO("QUAD eye%d u[%.3f-%.3f] v[%.3f-%.3f] src=%ux%u -> dst %ux%u perEye=%d",
                            eye, u0, u1, v0, v1, eyeSrcW, eyeSrcH, g_xrSwapchainWidth, g_xrSwapchainHeight, (int)havePerEye);
                    }
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

    // Final flush so the compositor sees completed images when we call xrEndFrame
    glFlush();

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

    VRMOD_LOG_INFO("xrEndFrame: submitting layer space=stage views=2 srcTex=%u swap=%ux%u fmt=0x%llx", srcTex, g_xrSwapchainWidth, g_xrSwapchainHeight, (unsigned long long)g_xrSwapchainFormat);
    res = g_xrEndFrame(g_xrSession, &fei);
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

    // Success - log periodically to see if layer is submitted every frame (to catch if "logs are lying" about presentation)
    VRMOD_LOG_INFO("EndFrame with layer succeeded (count=%d, state=%d)", s_submitCallCount, (int)g_xrSessionState);

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