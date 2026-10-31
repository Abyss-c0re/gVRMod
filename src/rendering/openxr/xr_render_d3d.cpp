// Windows OpenXR submit path: D3D11 shared engine RT → per-eye swapchains.
// Texture capture: vrmod-module-master main D3D9 CreateTexture hook (d3d_hooks).
// Flip: always 0 — D3D top-left matches OpenXR (do not invert V).

#ifdef _WIN32

#include "xr_render.h"
#include "xr_session.h"
#include "core/vrmod_log.h"
#include "rendering/d3d/d3d_hooks.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#define XR_USE_GRAPHICS_API_D3D11
#define XR_USE_PLATFORM_WIN32
#include <openxr/openxr/openxr.h>
#include <openxr/openxr/openxr_platform.h>

#include <vector>
#include <cstring>
#include <cmath>
#include <algorithm>

extern PoseResult g_xrHMDPose;
extern PoseResult ConvertXrPose(const XrSpaceLocation& loc);
extern PoseResult g_xrEyePoses[2];
extern bool g_xrEyePosesValid;
extern XrFovf g_xrEyeFovs[2];

// ── Swapchain state (D3D11) ──
static XrSwapchain g_swapchains[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
static XrSwapchainImageD3D11KHR* g_swapchainImages[2] = {nullptr, nullptr};
static uint32_t g_swapchainImageCount[2] = {0, 0};
static XrView g_views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};

uint32_t g_xrSwapchainWidth = 0;
uint32_t g_xrSwapchainHeight = 0;
int64_t  g_xrSwapchainFormat = 0;

static int64_t ChooseD3D11SwapchainFormat() {
    uint32_t count = 0;
    if (!g_xrEnumerateSwapchainFormats ||
        g_xrEnumerateSwapchainFormats(g_xrSession, 0, &count, nullptr) != XR_SUCCESS ||
        count == 0) {
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }
    std::vector<int64_t> formats(count);
    if (g_xrEnumerateSwapchainFormats(g_xrSession, count, &count, formats.data()) != XR_SUCCESS) {
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }
    for (int64_t f : formats) {
        if (f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) return f;
    }
    for (int64_t f : formats) {
        if (f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) return f;
    }
    for (int64_t f : formats) {
        if (f == DXGI_FORMAT_R8G8B8A8_UNORM) return f;
    }
    for (int64_t f : formats) {
        if (f == DXGI_FORMAT_B8G8R8A8_UNORM) return f;
    }
    return formats.empty() ? (int64_t)DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : formats[0];
}

bool XR_CreateSwapchains(char* errMsg, int errMsgLen) {
    if (!g_d3d11Device) {
        snprintf(errMsg, errMsgLen, "VRMOD OpenXR: D3D11 device not ready (ShareTextureFinish first)");
        return false;
    }

    g_xrSwapchainWidth = g_xrRecommendedWidth;
    g_xrSwapchainHeight = g_xrRecommendedHeight;
    int64_t chosen = ChooseD3D11SwapchainFormat();
    g_xrSwapchainFormat = chosen;
    VRMOD_LOG_INFO("Windows swapchain format: 0x%llx (SRGB preferred)",
                   (unsigned long long)chosen);

    for (int eye = 0; eye < 2; eye++) {
        XrSwapchainCreateInfo sci = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
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
            snprintf(errMsg, errMsgLen,
                "VRMOD OpenXR: xrCreateSwapchain eye %d failed (%s)",
                eye, XR_ResultToString(res));
            return false;
        }

        g_xrEnumerateSwapchainImages(g_swapchains[eye], 0, &g_swapchainImageCount[eye], nullptr);
        g_swapchainImages[eye] = new XrSwapchainImageD3D11KHR[g_swapchainImageCount[eye]];
        for (uint32_t i = 0; i < g_swapchainImageCount[eye]; i++) {
            g_swapchainImages[eye][i] = {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR};
        }
        g_xrEnumerateSwapchainImages(
            g_swapchains[eye], g_swapchainImageCount[eye], &g_swapchainImageCount[eye],
            (XrSwapchainImageBaseHeader*)g_swapchainImages[eye]);

        VRMOD_LOG_INFO("Eye %d D3D11 swapchain: %u images, %ux%u",
                       eye, g_swapchainImageCount[eye],
                       g_xrSwapchainWidth, g_xrSwapchainHeight);
    }

    VRMOD_LOG_INFO("OpenXR D3D11 swapchains ready (flip=0)");
    return true;
}

void XR_DestroySwapchains() {
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
    VRMOD_LOG_INFO("OpenXR D3D11 swapchains destroyed");
}

static int s_submitCallCount = 0;

XrSubmitResult XR_SubmitStolenTexture(unsigned int /*stolenTexture*/, const float textureBounds[8]) {
    XrSubmitResult result{};
    result.ok = false;
    result.errCode = 0;
    result.errMsg[0] = '\0';

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
    if (!g_d3d11SharedTexture || !g_d3d11Context) {
        result.errCode = -2;
        snprintf(result.errMsg, sizeof(result.errMsg), "No D3D11 shared texture");
        if (g_xrSessionRunning) XR_EndFrame();
        return result;
    }

    s_submitCallCount++;

    if (!g_xrFrameState.shouldRender) {
        XR_EndFrame();
        result.ok = true;
        return result;
    }

    // Locate views
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
        XR_EndFrame();
        return result;
    }

    // Refresh HMD / eye poses for Lua tracking
    {
        XrVector3f headPos = {
            (g_views[0].pose.position.x + g_views[1].pose.position.x) * 0.5f,
            (g_views[0].pose.position.y + g_views[1].pose.position.y) * 0.5f,
            (g_views[0].pose.position.z + g_views[1].pose.position.z) * 0.5f
        };
        XrSpaceLocation tempLoc = {XR_TYPE_SPACE_LOCATION};
        tempLoc.locationFlags =
            XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        tempLoc.pose.position = headPos;
        tempLoc.pose.orientation = g_views[0].pose.orientation;
        g_xrHMDPose = ConvertXrPose(tempLoc);

        XrSpaceLocation tloc = {XR_TYPE_SPACE_LOCATION};
        tloc.locationFlags = tempLoc.locationFlags;
        tloc.pose = g_views[0].pose;
        g_xrEyePoses[0] = ConvertXrPose(tloc);
        tloc.pose = g_views[1].pose;
        g_xrEyePoses[1] = ConvertXrPose(tloc);
        g_xrEyeFovs[0] = g_views[0].fov;
        g_xrEyeFovs[1] = g_views[1].fov;
        g_xrEyePosesValid = true;
    }

    D3D11_TEXTURE2D_DESC srcDesc{};
    g_d3d11SharedTexture->GetDesc(&srcDesc);
    const int srcW = g_knownSubmitSrcW > 0 ? g_knownSubmitSrcW : (int)srcDesc.Width;
    const int srcH = g_knownSubmitSrcH > 0 ? g_knownSubmitSrcH : (int)srcDesc.Height;
    if (srcW < 16 || srcH < 16) {
        result.errCode = -2;
        snprintf(result.errMsg, sizeof(result.errMsg), "Source size invalid %dx%d", srcW, srcH);
        XR_EndFrame();
        return result;
    }

    XrCompositionLayerProjectionView projViews[2];

    for (int eye = 0; eye < 2; eye++) {
        XrSwapchainImageAcquireInfo acqInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t imageIndex = 0;
        res = g_xrAcquireSwapchainImage(g_swapchains[eye], &acqInfo, &imageIndex);
        if (res != XR_SUCCESS) {
            result.errCode = (int)res;
            snprintf(result.errMsg, sizeof(result.errMsg),
                     "xrAcquireSwapchainImage eye %d: %s", eye, XR_ResultToString(res));
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
                     "xrWaitSwapchainImage eye %d: %s", eye, XR_ResultToString(res));
            XR_EndFrame();
            return result;
        }

        ID3D11Texture2D* dstTex = g_swapchainImages[eye][imageIndex].texture;
        if (!dstTex) {
            g_xrReleaseSwapchainImage(g_swapchains[eye], nullptr);
            result.errCode = -4;
            snprintf(result.errMsg, sizeof(result.errMsg), "Null swapchain texture eye %d", eye);
            XR_EndFrame();
            return result;
        }

        // UV bounds from Lua — Windows ComputeSubmitBounds uses ordered V (vMin < vMax).
        // flip=0: do not invert V. D3D CopySubresourceRegion uses top-left origin.
        float u0 = (eye == 0) ? textureBounds[0] : textureBounds[4];
        float u1 = (eye == 0) ? textureBounds[2] : textureBounds[6];
        float v0 = (eye == 0) ? textureBounds[1] : textureBounds[5];
        float v1 = (eye == 0) ? textureBounds[3] : textureBounds[7];

        const float ins = 0.003f;
        if (!(u1 > u0 + 0.001f)) {
            if (eye == 0) { u0 = ins; u1 = 0.5f; }
            else { u0 = 0.5f; u1 = 1.0f - ins; }
        }
        // Empty or inverted V → full rect (Windows should already be ordered).
        if (std::fabs(v1 - v0) < 0.001f || v0 > v1) {
            v0 = ins;
            v1 = 1.0f - ins;
        }
        // Explicit: never apply g_rtTextureNeedsVFlip on Windows path (must stay 0).
        (void)g_rtTextureNeedsVFlip;

        int srcX0 = (int)(u0 * srcW);
        int srcX1 = (int)(u1 * srcW);
        int srcY0 = (int)(v0 * srcH);
        int srcY1 = (int)(v1 * srcH);
        if (srcX0 < 0) srcX0 = 0;
        if (srcY0 < 0) srcY0 = 0;
        if (srcX1 > srcW) srcX1 = srcW;
        if (srcY1 > srcH) srcY1 = srcH;
        if (srcX1 <= srcX0) srcX1 = srcX0 + 1;
        if (srcY1 <= srcY0) srcY1 = srcY0 + 1;

        const UINT boxW = (UINT)(srcX1 - srcX0);
        const UINT boxH = (UINT)(srcY1 - srcY0);

        D3D11_BOX box{};
        box.left = (UINT)srcX0;
        box.top = (UINT)srcY0;
        box.front = 0;
        box.right = (UINT)srcX1;
        box.bottom = (UINT)srcY1;
        box.back = 1;

        // If sizes match, copy; else stretch via intermediate is complex — scale by
        // copying top-left region when dims differ (runtime often matches recommended).
        if (boxW == g_xrSwapchainWidth && boxH == g_xrSwapchainHeight) {
            g_d3d11Context->CopySubresourceRegion(
                dstTex, 0, 0, 0, 0,
                g_d3d11SharedTexture, 0, &box);
        } else {
            // Fallback: copy min region (no stretch) — better than nothing.
            D3D11_BOX box2 = box;
            UINT copyW = boxW < g_xrSwapchainWidth ? boxW : g_xrSwapchainWidth;
            UINT copyH = boxH < g_xrSwapchainHeight ? boxH : g_xrSwapchainHeight;
            box2.right = box2.left + copyW;
            box2.bottom = box2.top + copyH;
            g_d3d11Context->CopySubresourceRegion(
                dstTex, 0, 0, 0, 0,
                g_d3d11SharedTexture, 0, &box2);
            if ((s_submitCallCount % 90) == 1) {
                VRMOD_LOG_WARN("D3D copy size mismatch src=%ux%u dst=%ux%u (no stretch)",
                               boxW, boxH, g_xrSwapchainWidth, g_xrSwapchainHeight);
            }
        }

        XrSwapchainImageReleaseInfo relInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        g_xrReleaseSwapchainImage(g_swapchains[eye], &relInfo);

        projViews[eye] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        projViews[eye].pose = g_views[eye].pose;
        projViews[eye].fov = g_views[eye].fov;
        projViews[eye].subImage.swapchain = g_swapchains[eye];
        projViews[eye].subImage.imageRect.offset = {0, 0};
        projViews[eye].subImage.imageRect.extent = {
            (int32_t)g_xrSwapchainWidth, (int32_t)g_xrSwapchainHeight
        };
        projViews[eye].subImage.imageArrayIndex = 0;
    }

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

    res = g_xrEndFrame(g_xrSession, &fei);
    XR_MarkFrameEnded();
    if (res != XR_SUCCESS) {
        result.errCode = (int)res;
        snprintf(result.errMsg, sizeof(result.errMsg), "xrEndFrame failed: %s",
                 XR_ResultToString(res));
        return result;
    }

    static bool s_first = false;
    if (!s_first) {
        VRMOD_LOG_INFO("First successful OpenXR D3D11 submit: %ux%u flip=0 src=%dx%d",
                       g_xrSwapchainWidth, g_xrSwapchainHeight, srcW, srcH);
        s_first = true;
    }

    result.ok = true;
    return result;
}

// HMD pose refresh (shared with input) — D3D build.
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
        XrVector3f headPos = {
            (tmpViews[0].pose.position.x + tmpViews[1].pose.position.x) * 0.5f,
            (tmpViews[0].pose.position.y + tmpViews[1].pose.position.y) * 0.5f,
            (tmpViews[0].pose.position.z + tmpViews[1].pose.position.z) * 0.5f
        };
        XrSpaceLocation tempLoc = {XR_TYPE_SPACE_LOCATION};
        tempLoc.locationFlags =
            XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        tempLoc.pose.position = headPos;
        tempLoc.pose.orientation = tmpViews[0].pose.orientation;
        g_xrHMDPose = ConvertXrPose(tempLoc);
    }
}

#endif // _WIN32
