#pragma once

#include "core/vrmod_common.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>

typedef struct {
    void ClearEntryPoints();
    uint64_t m_nTotalGLCycles, m_nTotalGLCalls;
    int unknown1;
    int unknown2;
    int m_nOpenGLVersionMajor;
    int m_nOpenGLVersionMinor;
    int m_nOpenGLVersionPatch;
    bool m_bHave_OpenGL;
    char *m_pGLDriverStrings[4];
    int m_nDriverProvider;
    void *firstFunc;
} COpenGLEntryPoints;

typedef void *(*GL_GetProcAddressCallbackFunc_t)(const char *, bool &, const bool, void *);
typedef COpenGLEntryPoints*(*GetOpenGLEntryPoints_t)(GL_GetProcAddressCallbackFunc_t callback);
typedef void (*glGenTextures_t)(GLsizei n, GLuint *textures);

// ── Global GL state ──
extern char            g_createTextureOrigBytes[14];
extern void*           g_createTexture;
extern GLuint          g_sharedTexture;
extern COpenGLEntryPoints* g_GL;
extern bool            g_glIsPatched;
extern bool            g_captureStealActive;
extern bool            g_fbIsPatched;

// ── Error callback for rendering functions that need to report errors ──
typedef void (*ErrorFunc)(const char* msg);

// ── Hook patch construction (unchanged logic) ──
void BuildCreateTextureHookPatch(void* CreateTextureHook, uint8_t outPatch[HOOK_SIZE]);

// ── The hook target (unchanged logic) ──
void CreateTextureHook(GLsizei n, GLuint *textures);

// ── Patch removal (unchanged logic) ──
// Returns true on success. On failure calls errFunc with the error message.
bool RemoveTexturePatch(ErrorFunc errFunc);

// ── Shared texture lifecycle (unchanged logic) ──
// Returns 0 on success, calls errFunc on failure.
int ShareTextureBegin(uint32_t texWidth, uint32_t texHeight, ErrorFunc errFunc);

// ── Capture texture (clean side-by-side RT for submit, no overlays) ──
extern GLuint g_captureTexture;

// Discovered via framebuffer attachment observation during the share window.
// This is more reliable than the glGenTextures vtable patch alone when the engine
// (togl / GetRenderTargetEx) allocates RT backing stores through internal paths.
extern GLuint g_vrRtFBO;
extern GLuint g_vrRtColorTex;

int ShareCaptureTextureBegin(uint32_t texWidth, uint32_t texHeight, ErrorFunc errFunc);
bool ShareCaptureTextureFinish(ErrorFunc errFunc);  // note: name collides with LUA wrapper in other TU; use :: when calling from Lua bridge
