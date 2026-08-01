#pragma once

// Windows GMod texture share — from vrmod-module-master main (D3D9 CreateTexture hook
// + D3D11 OpenSharedResource). Used by OpenXR XR_KHR_D3D11_enable path.
//
// Flip: Windows D3D + OpenXR share top-left origin → g_rtTextureNeedsVFlip = false (0).

#include "core/vrmod_common.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <d3d9.h>
#include <d3d11.h>

typedef void (*ErrorFunc)(const char* msg);

// NT shared handle from hooked D3D9 CreateTexture (engine RT).
extern HANDLE            g_sharedTextureHandle;
// D3D11 device/context used for OpenXR graphics binding + copy into swapchains.
extern ID3D11Device*     g_d3d11Device;
extern ID3D11DeviceContext* g_d3d11Context;
// Shared engine RT opened as D3D11 texture (SBS stereo from Lua).
extern ID3D11Texture2D*  g_d3d11SharedTexture;
extern IDirect3DDevice9* g_pD3D9Device;

// Same as Linux: last-known SBS size from ShareTextureBegin / Lua.
extern int               g_knownSubmitSrcW;
extern int               g_knownSubmitSrcH;
// Windows default: no V flip (origin matches OpenXR).
extern bool              g_rtTextureNeedsVFlip;

void VRMOD_SetKnownSubmitSize(uint32_t w, uint32_t h);
void VRMOD_MarkSharedTextureEngineOwned(); // no-op on D3D (API parity)

// Resolve shaderapidx9 device + CreateTexture vtable slot (call from Init).
bool D3D_InitDeviceHooks(char* errMsg, int errMsgLen);

// Master-style share window around GetRenderTarget.
int  ShareTextureBegin(uint32_t eyeWidth, uint32_t eyeHeight, ErrorFunc errFunc);
bool ShareTextureFinish(ErrorFunc errFunc); // opens shared handle as D3D11 tex
bool RemoveTexturePatch(ErrorFunc errFunc);

// Optional capture APIs (no-ops / stubs on Windows — SBS RT is the shared texture).
extern unsigned int g_captureTexture; // always 0 on Windows
int  ShareCaptureTextureBegin(uint32_t texWidth, uint32_t texHeight, ErrorFunc errFunc);
bool ShareCaptureTextureFinish(ErrorFunc errFunc);

// GL-compat stubs so lua_interface / xr_render can compile shared names.
extern unsigned int g_sharedTexture;     // 0 on Windows (use D3D handles)
extern unsigned int g_leftEyeTexture;
extern unsigned int g_rightEyeTexture;
extern unsigned int g_leftEyeFBO;
extern unsigned int g_leftEyeColorTex;
extern unsigned int g_rightEyeFBO;
extern unsigned int g_rightEyeColorTex;
extern unsigned int g_vrRtFBO;
extern unsigned int g_vrRtColorTex;

void D3D_ShutdownShare();

#endif // _WIN32
