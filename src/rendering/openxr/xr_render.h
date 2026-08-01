#pragma once

#include "xr_session.h"

// ── Swapchain management ──
// Create OpenXR swapchains for both eyes.
// Must be called after XR_Init() and after session is running.
bool XR_CreateSwapchains(char* errMsg, int errMsgLen);

// Destroy swapchains.
void XR_DestroySwapchains();

// ── Texture submission ──
// Linux: blit stolen GL texture(s) into OpenGL swapchains.
// Windows: CopySubresourceRegion from D3D11 shared RT into D3D11 swapchains (flip=0).
// textureBounds: left uMin,vMin,uMax,vMax, right uMin,vMin,uMax,vMax
struct XrSubmitResult {
    bool ok;
    int errCode;      // 0 = success
    char errMsg[256];
};

// stolenTexture is GL id on Linux; ignored on Windows (uses g_d3d11SharedTexture).
XrSubmitResult XR_SubmitStolenTexture(unsigned int stolenTexture, const float textureBounds[8]);

// ── Recommended size ──
extern uint32_t g_xrSwapchainWidth;
extern uint32_t g_xrSwapchainHeight;

// Chosen swapchain format (GL enum on Linux, DXGI_FORMAT on Windows)
extern int64_t g_xrSwapchainFormat;
