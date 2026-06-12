#include "gl_hooks.h"
#include "core/vrmod_log.h"

#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

// ── Global GL state definitions ──
char                g_createTextureOrigBytes[14];
void*               g_createTexture = NULL;
GLuint              g_sharedTexture = 0;
GLuint              g_captureTexture = 0;
COpenGLEntryPoints* g_GL = NULL;
bool                g_glIsPatched = false;
bool                g_captureStealActive = false;

// Per-eye support (proper separate RT per eye, no more side-by-side packing hacks).
GLuint              g_leftEyeTexture = 0;
GLuint              g_rightEyeTexture = 0;
GLuint              g_leftEyeFBO = 0;
GLuint              g_leftEyeColorTex = 0;
GLuint              g_rightEyeFBO = 0;
GLuint              g_rightEyeColorTex = 0;
static int          g_eyeStealIndex = 0; // 0 -> left, 1 -> right during share window

// Set from Lua based on system.IsWindows(). True on Linux so the submit path
// can flip V when reading from the engine's GL render targets.
bool                g_rtTextureNeedsVFlip = false;

// Framebuffer attachment observation (used to discover the actual color texture backing
// the VR RT created by GetRenderTargetEx, which may bypass the glGenTextures vtable slot).
char                g_framebufferTexOrigBytes[14];
void*               g_framebufferTexture2D = NULL;
bool                g_fbIsPatched = false;

GLuint              g_vrRtFBO = 0;
GLuint              g_vrRtColorTex = 0;

void BuildCreateTextureHookPatch(void* CreateTextureHook, uint8_t outPatch[HOOK_SIZE]) {
    uint64_t addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(CreateTextureHook));
    uint32_t low  = static_cast<uint32_t>(addr & 0xFFFFFFFF);
    uint32_t high = static_cast<uint32_t>((addr >> 32) & 0xFFFFFFFF);

    // push imm32
    outPatch[0] = 0x68;
    std::memcpy(&outPatch[1], &low, sizeof(low));

    // mov dword ptr [rsp+4], imm32
    outPatch[5] = 0xC7;
    outPatch[6] = 0x44;
    outPatch[7] = 0x24;
    outPatch[8] = 0x04;
    std::memcpy(&outPatch[9], &high, sizeof(high));

    // ret
    outPatch[13] = 0xC3;
}

void CreateTextureHook(GLsizei n, GLuint *textures) {
    memcpy((void*)g_createTexture, (void*)g_createTextureOrigBytes, 14);
    ((glGenTextures_t)g_createTexture)(n, textures);
    // Capture mode for the old "clean capture RT" path.
    if (g_captureStealActive) {
        g_captureTexture = textures[0];
        return;
    }
    // Per-eye steal: first engine RT gen after Begin -> left, second -> right.
    // This supports the new proper per-eye RT path (two GetRenderTargetEx calls).
    if (g_eyeStealIndex == 0) {
        g_leftEyeTexture = textures[0];
        g_eyeStealIndex = 1;
    } else if (g_eyeStealIndex == 1) {
        g_rightEyeTexture = textures[0];
        g_eyeStealIndex = 2;
    } else {
        // Fallback for any extra gens during window: keep classic shared for debug/compat.
        g_sharedTexture = textures[0];
    }
}

// One-shot / windowed hook for glFramebufferTexture2D (resolved via glXGetProcAddress).
// While the steal window is open we record attachments to COLOR_ATTACHMENT0; this catches
// the actual texture the engine intends to render into for the RT even when the GenTextures
// vtable slot inside togl does not see the RT's backing allocation.
void FramebufferTextureHook(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    // Restore real bytes so the original function can run.
    if (g_framebufferTexture2D) {
        memcpy((void*)g_framebufferTexture2D, (void*)g_framebufferTexOrigBytes, HOOK_SIZE);
    }

    // Call the real implementation.
    typedef void (*glFramebufferTexture2D_t)(GLenum, GLenum, GLenum, GLuint, GLint);
    ((glFramebufferTexture2D_t)g_framebufferTexture2D)(target, attachment, textarget, texture, level);

    // While a share/capture steal window is active, a COLOR_ATTACHMENT0 with a real texture
    // belongs to a VR RT (now per-eye). We record both the classic last-seen and the
    // sequential per-eye slots so the new path has authoritative left/right backing textures.
    if (g_glIsPatched && attachment == GL_COLOR_ATTACHMENT0 && texture != 0) {
        g_vrRtColorTex = texture;
        GLint currentFBO = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &currentFBO);
        if (currentFBO != 0) {
            g_vrRtFBO = (GLuint)currentFBO;
        }

        // Per-eye FBO/tex capture (first attach in window = left eye RT, second = right).
        if (g_eyeStealIndex <= 1) {
            // still expecting left or assigning left
            g_leftEyeColorTex = texture;
            g_leftEyeFBO = (GLuint)currentFBO;
            VRMOD_LOG_INFO("Framebuffer attach observed (left eye): COLOR0 tex=%u fbo=%u", texture, (unsigned)currentFBO);
        } else if (g_eyeStealIndex == 2) {
            g_rightEyeColorTex = texture;
            g_rightEyeFBO = (GLuint)currentFBO;
            VRMOD_LOG_INFO("Framebuffer attach observed (right eye): COLOR0 tex=%u fbo=%u", texture, (unsigned)currentFBO);
        } else {
            VRMOD_LOG_INFO("Framebuffer attach observed: COLOR0 tex=%u fbo=%u (captureSteal=%d)", texture, (unsigned)currentFBO, (int)g_captureStealActive);
        }
    }

    // Re-arm for the remainder of the short share window so later attachments (or color after depth) are also seen.
    if (g_glIsPatched && g_framebufferTexture2D) {
        uint8_t patch[HOOK_SIZE];
        BuildCreateTextureHookPatch(reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(FramebufferTextureHook)), patch);

        uintptr_t addr = reinterpret_cast<uintptr_t>(g_framebufferTexture2D);
        size_t pageSize = getpagesize();
        uintptr_t startPg = addr & ~(pageSize - 1);
        uintptr_t endPg = (addr + HOOK_SIZE + pageSize - 1) & ~(pageSize - 1);
        size_t len = endPg - startPg;

        if (mprotect(reinterpret_cast<void*>(startPg), len, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
            memcpy(reinterpret_cast<void*>(addr), patch, HOOK_SIZE);
            g_fbIsPatched = true;
        }
    }
}

bool RemoveTexturePatch(ErrorFunc errFunc) {
    if (!g_glIsPatched) {
        VRMOD_LOG_INFO("Patch not applied, nothing to remove.");
        return true;
    }

    uintptr_t addr     = reinterpret_cast<uintptr_t>(g_createTexture);
    size_t    pageSize = getpagesize();
    uintptr_t start    = addr & ~(pageSize - 1);
    uintptr_t end      = (addr + HOOK_SIZE + pageSize - 1) & ~(pageSize - 1);
    size_t    len      = end - start;

    if (mprotect((void*)start, len, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        if (errFunc) errFunc("VRMOD: Failed to set memory protection to RWX for unpatch.");
        return false;
    }

    // Restore original bytes
    std::memcpy((void*)addr, g_createTextureOrigBytes, HOOK_SIZE);

    // Verify restoration
    if (std::memcmp((void*)addr, g_createTextureOrigBytes, HOOK_SIZE) != 0) {
        if (errFunc) errFunc("VRMOD: Failed to verify unpatch — bytes mismatch.");
        // Still try to set protection back
        mprotect((void*)start, len, PROT_READ | PROT_EXEC);
        return false;
    }

    // Reset memory protection
    if (mprotect((void*)start, len, PROT_READ | PROT_EXEC) != 0) {
        if (errFunc) errFunc("VRMOD: Failed to reset memory protection after unpatch.");
        return false;
    }

    g_glIsPatched = false;
    VRMOD_LOG_INFO("Texture patch removed successfully.");

    // Also remove the framebuffer observation patch if it is still applied.
    if (g_fbIsPatched && g_framebufferTexture2D) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(g_framebufferTexture2D);
        size_t pageSize = getpagesize();
        uintptr_t start = addr & ~(pageSize - 1);
        uintptr_t end = (addr + HOOK_SIZE + pageSize - 1) & ~(pageSize - 1);
        size_t len = end - start;

        if (mprotect((void*)start, len, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
            std::memcpy((void*)addr, g_framebufferTexOrigBytes, HOOK_SIZE);
            mprotect((void*)start, len, PROT_READ | PROT_EXEC);
        }
        g_fbIsPatched = false;
        VRMOD_LOG_INFO("FramebufferTexture2D observation patch removed.");
    }

    return true;
}

int ShareTextureBegin(uint32_t texWidth, uint32_t texHeight, ErrorFunc errFunc) {
    // Tear down previous shared (compat)
    if (glIsTexture(g_sharedTexture)) {
        glDeleteTextures(1, &g_sharedTexture);
        g_sharedTexture = 0;
        glFlush();
    }

    // Reset per-eye state for new proper per-eye RT path.
    if (glIsTexture(g_leftEyeTexture)) { glDeleteTextures(1, &g_leftEyeTexture); g_leftEyeTexture = 0; }
    if (glIsTexture(g_rightEyeTexture)) { glDeleteTextures(1, &g_rightEyeTexture); g_rightEyeTexture = 0; }
    g_leftEyeFBO = g_rightEyeFBO = 0;
    g_leftEyeColorTex = g_rightEyeColorTex = 0;
    g_eyeStealIndex = 0;

    // Pre-allocate a placeholder at per-eye size (the engine RTs will be captured via hook/FBO).
    // We keep one g_sharedTexture at per-eye size for fallback paths.
    glGenTextures(1, &g_sharedTexture);
    glBindTexture(GL_TEXTURE_2D, g_sharedTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    // Allocate per-eye size now (no more *2 side-by-side assumption here).
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        texWidth,
        texHeight,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    // Clamp to transparent border + linear filtering
    GLfloat borderColor[4] = {0,0,0,0};
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, g_sharedTexture);

    // Hook-patch logic unchanged
    if (g_glIsPatched)
        return 0;

    uint8_t patch[HOOK_SIZE];
    void*    hookAddr = reinterpret_cast<void*>(
                           reinterpret_cast<uintptr_t>(CreateTextureHook));
    BuildCreateTextureHookPatch(hookAddr, patch);

    uintptr_t addr     = reinterpret_cast<uintptr_t>(g_createTexture);
    size_t    pageSize = getpagesize();
    uintptr_t startPg  = addr & ~(pageSize - 1);
    uintptr_t endPg    = (addr + HOOK_SIZE + pageSize - 1) & ~(pageSize - 1);
    size_t    length   = endPg - startPg;

    if (mprotect(reinterpret_cast<void*>(startPg),
                 length,
                 PROT_READ|PROT_WRITE|PROT_EXEC) == -1) {
        if (errFunc) errFunc("VRMOD: mprotect RWX failed");
        return -1;
    }

    memcpy(g_createTextureOrigBytes,
           reinterpret_cast<void*>(addr),
           HOOK_SIZE);
    memcpy(reinterpret_cast<void*>(addr),
           patch,
           HOOK_SIZE);

    g_glIsPatched = true;
    VRMOD_LOG_INFO("Texture hook patch applied (per-eye mode armed).");

    // Arm framebuffer attachment observation in the same window. This is the key to reliably
    // capturing the texture that GetRenderTargetEx actually wires up as the color target for
    // the per-eye VR RTs (the glGenTextures vtable slot at +50 can be bypassed by togl's RT paths).
    if (!g_fbIsPatched) {
        if (!g_framebufferTexture2D) {
            g_framebufferTexture2D = (void*)glXGetProcAddress((const GLubyte*)"glFramebufferTexture2D");
            if (!g_framebufferTexture2D) {
                g_framebufferTexture2D = (void*)glXGetProcAddress((const GLubyte*)"glFramebufferTexture2DEXT");
            }
        }
        if (g_framebufferTexture2D) {
            memcpy(g_framebufferTexOrigBytes, (void*)g_framebufferTexture2D, HOOK_SIZE);

            uint8_t patch[HOOK_SIZE];
            BuildCreateTextureHookPatch(reinterpret_cast<void*>(
                reinterpret_cast<uintptr_t>(FramebufferTextureHook)), patch);

            uintptr_t addr = reinterpret_cast<uintptr_t>(g_framebufferTexture2D);
            size_t pageSize = getpagesize();
            uintptr_t startPg = addr & ~(pageSize - 1);
            uintptr_t endPg = (addr + HOOK_SIZE + pageSize - 1) & ~(pageSize - 1);
            size_t len = endPg - startPg;

            if (mprotect(reinterpret_cast<void*>(startPg), len, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
                memcpy(reinterpret_cast<void*>(addr), patch, HOOK_SIZE);
                g_fbIsPatched = true;
                VRMOD_LOG_INFO("FramebufferTexture2D observation hook armed.");
            } else {
                VRMOD_LOG_WARN("mprotect RWX failed for framebuffer observation hook");
            }
        } else {
            VRMOD_LOG_WARN("glFramebufferTexture2D not resolved; FBO-based RT discovery unavailable");
        }
    }

    return 0;
}

// ── Capture texture support (for clean submit path) ──
// Mirrors ShareTexture but targets g_captureTexture and sets steal flag
// so the engine's glGen for the capture RT is recorded.
int ShareCaptureTextureBegin(uint32_t texWidth, uint32_t texHeight, ErrorFunc errFunc) {
    if (glIsTexture(g_captureTexture)) {
        glDeleteTextures(1, &g_captureTexture);
        g_captureTexture = 0;
        glFlush();
    }

    // Pre-allocate a texture of the *final* size the caller (Lua) will use for the capture RT.
    // Unlike the main Share path (which passes "per-eye-ish" and does *2 inside), the capture
    // path is passed the packed size (or we compute here). We use the passed size directly.
    glGenTextures(1, &g_captureTexture);
    glBindTexture(GL_TEXTURE_2D, g_captureTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        texWidth,
        texHeight,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    GLfloat borderColor[4] = {0,0,0,0};
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Re-arm the patch (main path may have removed it) so the *next* glGen hits our hook
    // and we can divert to g_captureTexture via the g_captureStealActive flag.
    if (g_glIsPatched) {
        // already patched from a previous begin without finish; just enable steal
        g_captureStealActive = true;
        VRMOD_LOG_INFO("Capture steal re-armed (patch already active).");
        return 0;
    }

    uint8_t patch[HOOK_SIZE];
    void* hookAddr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(CreateTextureHook));
    BuildCreateTextureHookPatch(hookAddr, patch);

    uintptr_t addr = reinterpret_cast<uintptr_t>(g_createTexture);
    size_t pageSize = getpagesize();
    uintptr_t startPg = addr & ~(pageSize - 1);
    uintptr_t endPg = (addr + HOOK_SIZE + pageSize - 1) & ~(pageSize - 1);
    size_t length = endPg - startPg;

    if (mprotect(reinterpret_cast<void*>(startPg), length, PROT_READ|PROT_WRITE|PROT_EXEC) == -1) {
        if (errFunc) errFunc("VRMOD: mprotect RWX failed for capture");
        return -1;
    }

    memcpy(g_createTextureOrigBytes, reinterpret_cast<void*>(addr), HOOK_SIZE);
    memcpy(reinterpret_cast<void*>(addr), patch, HOOK_SIZE);

    g_glIsPatched = true;
    g_captureStealActive = true;
    VRMOD_LOG_INFO("Capture texture pre-allocated and hook armed for capture steal.");
    return 0;
}

bool ShareCaptureTextureFinish(ErrorFunc errFunc) {
    g_captureStealActive = false;

    if (!g_glIsPatched) {
        VRMOD_LOG_INFO("Capture: patch not active at finish.");
        return true;
    }

    // Remove patch (same as main path) so normal engine glGen is not hooked after capture setup.
    uintptr_t addr = reinterpret_cast<uintptr_t>(g_createTexture);
    size_t pageSize = getpagesize();
    uintptr_t start = addr & ~(pageSize - 1);
    uintptr_t end = (addr + HOOK_SIZE + pageSize - 1) & ~(pageSize - 1);
    size_t len = end - start;

    if (mprotect((void*)start, len, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        if (errFunc) errFunc("VRMOD: Failed mprotect for capture unpatch.");
        return false;
    }

    std::memcpy((void*)addr, g_createTextureOrigBytes, HOOK_SIZE);

    if (std::memcmp((void*)addr, g_createTextureOrigBytes, HOOK_SIZE) != 0) {
        if (errFunc) errFunc("VRMOD: Capture unpatch verify failed.");
        mprotect((void*)start, len, PROT_READ | PROT_EXEC);
        return false;
    }

    if (mprotect((void*)start, len, PROT_READ | PROT_EXEC) != 0) {
        if (errFunc) errFunc("VRMOD: Failed reset prot after capture unpatch.");
        return false;
    }

    g_glIsPatched = false;

    // Clean up framebuffer observation patch if it was armed for the capture window as well.
    if (g_fbIsPatched && g_framebufferTexture2D) {
        uintptr_t faddr = reinterpret_cast<uintptr_t>(g_framebufferTexture2D);
        size_t fpage = getpagesize();
        uintptr_t fstart = faddr & ~(fpage - 1);
        uintptr_t fend = (faddr + HOOK_SIZE + fpage - 1) & ~(fpage - 1);
        size_t flen = fend - fstart;
        if (mprotect((void*)fstart, flen, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
            std::memcpy((void*)faddr, g_framebufferTexOrigBytes, HOOK_SIZE);
            mprotect((void*)fstart, flen, PROT_READ | PROT_EXEC);
        }
        g_fbIsPatched = false;
    }

    VRMOD_LOG_INFO("Capture texture ready (steal finished, patch removed). id=%u", g_captureTexture);
    return true;
}
