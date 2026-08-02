#pragma once
// =============================================================================
// Virtual Display driver — module-owned offscreen "monitor" for VR panels
//
// Prophecy: GMod UI (launcher cinema + in-game pause) projects through one
// reusable surface that behaves like a fixed-resolution display. On Linux this
// is a real GL FBO/texture; CaptureWindow blits the current drawable into it
// so the desktop mirror can become the VR panel source later (or already for
// native consumers that read the GL texture id).
//
// Call only when a GLX context is current (same rule as OpenXR session bind).
// =============================================================================

#include "core/vrmod_common.h"
#include <cstdint>

// Max simultaneous virtual displays (launcher + pause + spare)
#define VRMOD_VDISPLAY_MAX 4

struct VDisplayInfo {
    int id;                 // 1-based handle; 0 = invalid
    uint32_t width;
    uint32_t height;
    unsigned int glTexture; // GLuint color attachment (0 if unavailable)
    unsigned int glFBO;     // GLuint FBO (0 if unavailable)
    bool valid;
    bool hasCapture;        // last CaptureWindow succeeded
};

// Create or replace display slot. idHint: 0 = allocate, or reuse 1..MAX.
// Returns id (>0) or 0 on failure. errMsg optional (MAX_STR_LEN).
int VDisplay_Create(uint32_t width, uint32_t height, int idHint, char* errMsg, size_t errMsgLen);

// Resize existing display (recreates GL objects). Returns false on failure.
bool VDisplay_Resize(int id, uint32_t width, uint32_t height, char* errMsg, size_t errMsgLen);

// Destroy one (id) or all (id <= 0).
void VDisplay_Destroy(int id);

// Fill info; returns false if id invalid.
bool VDisplay_GetInfo(int id, VDisplayInfo* out);

// Blit current window / default framebuffer into the virtual display.
// Requires active GLX context. Returns false if GL not ready.
bool VDisplay_CaptureWindow(int id, char* errMsg, size_t errMsgLen);

// Clear color (RGBA 0–1).
bool VDisplay_Clear(int id, float r, float g, float b, float a);

// True if this build has a real GPU-backed virtual display (Linux GL).
bool VDisplay_IsSupported();

// Tear down all on module unload / VR shutdown.
void VDisplay_Shutdown();
