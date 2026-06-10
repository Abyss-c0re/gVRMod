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
        XrSpaceLocation tempLoc = {XR_TYPE_SPACE_LOCATION};
        tempLoc.locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        tempLoc.pose = tmpViews[0].pose;
        g_xrHMDPose = ConvertXrPose(tempLoc);
    }
}

// Capture / stolen texture names from the share mechanism (gl_hooks.cpp).
// We focus exclusively on these for the pixels sent to the HMD (no FB read fallback).
extern GLuint g_captureTexture;
extern GLuint g_sharedTexture;  // the original eye RT one, for fallback within RT-tex path only

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
        XrSpaceLocation tempLoc = {XR_TYPE_SPACE_LOCATION};
        tempLoc.locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        tempLoc.pose = g_views[0].pose;
        g_xrHMDPose = ConvertXrPose(tempLoc);
    }

    // Submit the *stolenTexture* passed from Lua. This is the GL texture backing the RT
    // that the addon explicitly targets with its two RenderView() calls inside
    // PerformRenderViews (left half + right half side-by-side). This is the "RT view
    // generated by the addon" — exactly the content we want in the HMD (clean 3D scene
    // per the addon's view setup, origins, fovs and viewports; no engine desktop/HUD
    // pollution because we rendered into our private RT).
    //
    // We used to prefer g_captureTexture (a 2D full-UV copy of the rtMaterial into a
    // second RT) as a "clean snapshot", but that extra copy can end up blank or stale.
    // Per requirement: submit directly from the addon's generated RT via the stolen path.
    GLuint srcTex = stolenTexture;

    // If we observed the FBO that the engine set up for the VR RT (via glFramebufferTexture2D
    // during the share window), query its current COLOR_ATTACHMENT0. This gives the live,
    // authoritative texture backing the RT regardless of whether the glGenTextures vtable
    // patch at firstFunc+50 inside togl ever saw the allocation.
    if (g_vrRtFBO != 0 && glBindFramebufferPtr && glGetFramebufferAttachmentParameterivPtr) {
        GLint prevFB = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFB);

        glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, g_vrRtFBO);
        GLint attached = 0;
        glGetFramebufferAttachmentParameterivPtr(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE, &attached);
        glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, prevFB);

        if (attached != 0 && glIsTexture((GLuint)attached)) {
            if ((GLuint)attached != srcTex) {
                static int s_fboResolveLog = 0;
                if ((s_fboResolveLog++ % 120) == 0) {
                    VRMOD_LOG_INFO("Submit: resolved srcTex=%u from remembered VR RT FBO %u (stolen was %u)",
                        (GLuint)attached, g_vrRtFBO, srcTex);
                }
            }
            srcTex = (GLuint)attached;
        }
    }

    if (g_captureTexture && glIsTexture(g_captureTexture)) {
        // Capture is still updated for other uses / future, but we deliberately use the
        // direct stolen one that the addon rendered the views into.
        // (If you want to force capture for debug, swap the line above.)
    }
    if ((s_submitCallCount % 30) == 0) {
        VRMOD_LOG_INFO("Submit using srcTex=%u (DIRECT from addon's generated RT via stolenTexture; capture var=%u, rtFBO=%u)", srcTex, g_captureTexture, g_vrRtFBO);
    }

    // Query dimensions from the texture we will actually attach/blit (capture preferred).
    GLint srcWidth = 0, srcHeight = 0;
    glBindTexture(GL_TEXTURE_2D, srcTex);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &srcWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &srcHeight);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (srcWidth == 0 || srcHeight == 0) {
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

        // Direct attach of the source we chose (captureRt for clean content) + half blit.
        {
            GLint prevR = 0, prevD = 0;
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevR);
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevD);

            glBindFramebufferPtr(GL_READ_FRAMEBUFFER, g_blitSrcFBO);
            glFramebufferTexture2DPtr(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srcTex, 0);

            glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, g_blitFBO);
            glFramebufferTexture2DPtr(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTexture, 0);

            if (glCheckFramebufferStatusPtr(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE &&
                glCheckFramebufferStatusPtr(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {

                // Explicitly select the color attachment for read/draw to guarantee the blit
                // pulls from the attached srcTex (the addon's RT) and writes to the swap image.
                glReadBuffer(GL_COLOR_ATTACHMENT0);
                glDrawBuffer(GL_COLOR_ATTACHMENT0);

                // Use the client textureBounds (computed from the projection matrices in
                // CalculateProjectionParams / ComputeSubmitBounds on the Lua side) to select
                // the exact sub-rect for each eye. This respects the asymmetry from the OpenXR
                // projections instead of forcing 50/50 geometric halves.
                float u0 = (eye == 0) ? textureBounds[0] : textureBounds[4];
                float u1 = (eye == 0) ? textureBounds[2] : textureBounds[6];
                float v0 = (eye == 0) ? textureBounds[1] : textureBounds[5];
                float v1 = (eye == 0) ? textureBounds[3] : textureBounds[7];

                {
                    static bool s_loggedBounds = false;
                    if (!s_loggedBounds) {
                        VRMOD_LOG_INFO("Using client textureBounds for per-eye rect from RT (projection-aware)");
                        s_loggedBounds = true;
                    }
                }

                glFinish();

                // === Automated black texture detection ===
                // Sample at centers of the bounds rects.
                {
                    glReadBuffer(GL_COLOR_ATTACHMENT0);
                    unsigned char pL[4] = {0}, pR[4] = {0};
                    float uL = (textureBounds[0] + textureBounds[2]) * 0.5f;
                    float uR = (textureBounds[4] + textureBounds[6]) * 0.5f;
                    float vC = (textureBounds[1] + textureBounds[3]) * 0.5f;
                    GLint lx = (GLint)(uL * srcWidth);
                    GLint rx = (GLint)(uR * srcWidth);
                    GLint my = (GLint)(vC * srcHeight);
                    glReadPixels(lx, my, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pL);
                    glReadPixels(rx, my, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pR);
                    int sumL = (int)pL[0] + pL[1] + pL[2];
                    int sumR = (int)pR[0] + pR[1] + pR[2];
                    if (sumL < 25 && sumR < 25) {
                        VRMOD_LOG_INFO("### BLACK TEXTURE DETECTED ### srcTex=%u L=(%u,%u,%u) R=(%u,%u,%u) sums=%d/%d size=%dx%d",
                            srcTex, pL[0],pL[1],pL[2], pR[0],pR[1],pR[2], sumL, sumR, srcWidth, srcHeight);
                    } else {
                        VRMOD_LOG_INFO("### GOOD TEXTURE CONTENT ### srcTex=%u leftSum=%d rightSum=%d size=%dx%d",
                            srcTex, sumL, sumR, srcWidth, srcHeight);
                    }
                }

                // Unbind READ before texturing the src (avoids the attached-as-texture problem).
                glBindFramebufferPtr(GL_READ_FRAMEBUFFER, 0);

                // Textured quad using the bounds rect + V flip for Linux GL RT orientation.
                // On Linux the bounds have higher V in the first V slot (v0), lower in v1.
                // We map bottom of dst (low Y) to v0 (high), top of dst to v1 (low) -- this
                // is the assignment that previously made the image right-side-up.
                {
                    // Save GL state
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
                    glBindTexture(GL_TEXTURE_2D, srcTex);

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
                        glTexCoord2f(u0, v0); glVertex2f(-1.0f, -1.0f);  // dst bottom <- bounds v0 (high on Linux)
                        glTexCoord2f(u1, v0); glVertex2f( 1.0f, -1.0f);
                        glTexCoord2f(u1, v1); glVertex2f( 1.0f,  1.0f);  // dst top   <- bounds v1 (low on Linux)
                        glTexCoord2f(u0, v1); glVertex2f(-1.0f,  1.0f);
                    glEnd();

                    glFinish();

                    // Post-copy dst sample
                    {
                        unsigned char dstSample[4] = {0};
                        glReadBuffer(GL_COLOR_ATTACHMENT0);
                        glReadPixels(g_xrSwapchainWidth / 2, g_xrSwapchainHeight / 2, 1, 1,
                                     GL_RGBA, GL_UNSIGNED_BYTE, dstSample);
                        int dstSum = (int)dstSample[0] + dstSample[1] + dstSample[2];
                        if (eye == 0) {
                            VRMOD_LOG_INFO("POST-COPY dst eye0 center sample: (%u,%u,%u) sum=%d",
                                dstSample[0], dstSample[1], dstSample[2], dstSum);
                        }
                    }

                    // Restore
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
                        VRMOD_LOG_INFO("QUAD submission eye0 using bounds u[%.3f-%.3f] v[%.3f-%.3f] -> dst %ux%u",
                            u0, u1, v0, v1, g_xrSwapchainWidth, g_xrSwapchainHeight);
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