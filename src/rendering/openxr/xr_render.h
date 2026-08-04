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

// ── Per-eye collect + backend-timed submit (Linux GL) ──
// Pipeline for mat_queue_mode 2 / true dual-eye:
//   1) XR_WaitAndBeginFrame()          — backend owns frame timing
//   2) XR_CollectEyesFromEngine()      — copy last complete engine stereo into module staging
//   3) XR_SubmitStolenTexture / collected path — EndFrame with staging (not live engine RT)
//   4) Lua dual RenderView             — record next eyes into engine RT
// Collect is deferred to the *next* frame start so MatQueue can drain before the blit.
#ifndef _WIN32
void XR_EnsureEyeCollectors(uint32_t eyeW, uint32_t eyeH);
// Blit engine SBS or per-eye RTs → write slot, swap. Uses textureBounds for SBS U halves.
bool XR_CollectEyesFromEngine(const float textureBounds[8]);
// True when a complete collected pair is ready for submit.
bool XR_HasCollectedEyes();
// Prefer collected staging as submit source (set after successful Collect).
void XR_SetPreferCollectedEyes(bool prefer);
bool XR_PreferCollectedEyes();
void XR_DestroyEyeCollectors();

// Submit UV crop policy (Lua: vrmod_submit_crop). Live-safe.
//   0 SAFE     — collector/per-eye full eye rect; SBS uses Lua textureBounds (default)
//   1 FULL     — force full-eye UV even if bounds look like SBS halves (debug)
//   2 FOV_CROP — experimental asymmetric FOV crop on per-eye textures (can break stereo)
void XR_SetSubmitCropMode(int mode);
int  XR_GetSubmitCropMode();
#endif

// ── Recommended size ──
extern uint32_t g_xrSwapchainWidth;
extern uint32_t g_xrSwapchainHeight;

// Chosen swapchain format (GL enum on Linux, DXGI_FORMAT on Windows)
extern int64_t g_xrSwapchainFormat;
