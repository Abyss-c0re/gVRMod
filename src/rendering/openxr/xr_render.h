#pragma once

#include "xr_session.h"
#include <GL/gl.h>

// ── Swapchain management ──
// Create OpenXR swapchains for both eyes.
// Must be called after XR_Init() and after session is running.
bool XR_CreateSwapchains(char* errMsg, int errMsgLen);

// Destroy swapchains.
void XR_DestroySwapchains();

// ── Texture submission with buffering ──
// Blit the stolen GL texture into the OpenXR swapchain and submit.
// textureBounds: [leftUMin, leftVMin, leftUMax, leftVMax, rightUMin, rightVMin, rightUMax, rightVMax]
struct XrSubmitResult {
    bool ok;
    int errCode;      // 0 = success
    char errMsg[256];
};

XrSubmitResult XR_SubmitStolenTexture(GLuint stolenTexture, const float textureBounds[8]);

// ── Recommended size ──
extern uint32_t g_xrSwapchainWidth;
extern uint32_t g_xrSwapchainHeight;

// Chosen swapchain format (int64_t GL enum value from xrEnumerateSwapchainFormats)
extern int64_t g_xrSwapchainFormat;