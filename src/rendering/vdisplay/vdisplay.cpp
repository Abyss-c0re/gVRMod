#include "vdisplay.h"
#include "core/vrmod_log.h"

#include <cstring>
#include <algorithm>

#ifndef _WIN32
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#endif

// ── Slot table ──
struct VDisplaySlot {
    bool used = false;
    uint32_t width = 0;
    uint32_t height = 0;
#ifndef _WIN32
    GLuint tex = 0;
    GLuint fbo = 0;
#endif
    bool hasCapture = false;
};

static VDisplaySlot g_slots[VRMOD_VDISPLAY_MAX];

#ifndef _WIN32
// Local GL extension pointers (independent of xr_render TU)
static PFNGLGENFRAMEBUFFERSPROC    glGenFramebuffersPtr = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffersPtr = nullptr;
static PFNGLBINDFRAMEBUFFERPROC    glBindFramebufferPtr = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2DPtr = nullptr;
static PFNGLBLITFRAMEBUFFERPROC    glBlitFramebufferPtr = nullptr;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatusPtr = nullptr;

static bool LoadFBOExt() {
    if (glGenFramebuffersPtr) return true;
    glGenFramebuffersPtr = (PFNGLGENFRAMEBUFFERSPROC)glXGetProcAddress((const GLubyte*)"glGenFramebuffers");
    glDeleteFramebuffersPtr = (PFNGLDELETEFRAMEBUFFERSPROC)glXGetProcAddress((const GLubyte*)"glDeleteFramebuffers");
    glBindFramebufferPtr = (PFNGLBINDFRAMEBUFFERPROC)glXGetProcAddress((const GLubyte*)"glBindFramebuffer");
    glFramebufferTexture2DPtr = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glXGetProcAddress((const GLubyte*)"glFramebufferTexture2D");
    glBlitFramebufferPtr = (PFNGLBLITFRAMEBUFFERPROC)glXGetProcAddress((const GLubyte*)"glBlitFramebuffer");
    glCheckFramebufferStatusPtr = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)glXGetProcAddress((const GLubyte*)"glCheckFramebufferStatus");
    return glGenFramebuffersPtr && glDeleteFramebuffersPtr && glBindFramebufferPtr &&
           glFramebufferTexture2DPtr && glBlitFramebufferPtr && glCheckFramebufferStatusPtr;
}

static void DestroyGL(VDisplaySlot& s) {
    if (s.fbo) {
        glDeleteFramebuffersPtr(1, &s.fbo);
        s.fbo = 0;
    }
    if (s.tex) {
        glDeleteTextures(1, &s.tex);
        s.tex = 0;
    }
}

static bool CreateGL(VDisplaySlot& s, uint32_t w, uint32_t h, char* errMsg, size_t errMsgLen) {
    if (!glXGetCurrentContext()) {
        if (errMsg && errMsgLen)
            snprintf(errMsg, errMsgLen, "VDisplay: no active GLX context");
        return false;
    }
    if (!LoadFBOExt()) {
        if (errMsg && errMsgLen)
            snprintf(errMsg, errMsgLen, "VDisplay: FBO extensions unavailable");
        return false;
    }

    DestroyGL(s);

    w = std::max(64u, std::min(w, 4096u));
    h = std::max(64u, std::min(h, 4096u));
    // Even dimensions play nicer with YUV / CEF
    w &= ~1u;
    h &= ~1u;

    GLint prevDraw = 0, prevRead = 0, prevTex = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);

    glGenTextures(1, &s.tex);
    glBindTexture(GL_TEXTURE_2D, s.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenFramebuffersPtr(1, &s.fbo);
    glBindFramebufferPtr(GL_FRAMEBUFFER, s.fbo);
    glFramebufferTexture2DPtr(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s.tex, 0);

    GLenum st = glCheckFramebufferStatusPtr(GL_FRAMEBUFFER);
    glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, (GLuint)prevDraw);
    glBindFramebufferPtr(GL_READ_FRAMEBUFFER, (GLuint)prevRead);
    glBindTexture(GL_TEXTURE_2D, (GLuint)prevTex);

    if (st != GL_FRAMEBUFFER_COMPLETE) {
        DestroyGL(s);
        if (errMsg && errMsgLen)
            snprintf(errMsg, errMsgLen, "VDisplay: FBO incomplete (0x%x)", (unsigned)st);
        return false;
    }

    s.width = w;
    s.height = h;
    s.hasCapture = false;
    return true;
}
#endif // !_WIN32

bool VDisplay_IsSupported() {
#ifdef _WIN32
    return false; // D3D path TBD — Lua virtual display still works via engine RTs
#else
    return true;
#endif
}

int VDisplay_Create(uint32_t width, uint32_t height, int idHint, char* errMsg, size_t errMsgLen) {
    int slot = -1;
    if (idHint >= 1 && idHint <= VRMOD_VDISPLAY_MAX) {
        slot = idHint - 1;
    } else {
        for (int i = 0; i < VRMOD_VDISPLAY_MAX; ++i) {
            if (!g_slots[i].used) {
                slot = i;
                break;
            }
        }
    }
    if (slot < 0) {
        if (errMsg && errMsgLen)
            snprintf(errMsg, errMsgLen, "VDisplay: no free slots");
        return 0;
    }

#ifdef _WIN32
    (void)width;
    (void)height;
    g_slots[slot].used = true;
    g_slots[slot].width = std::max(64u, std::min(width ? width : 1280u, 4096u));
    g_slots[slot].height = std::max(64u, std::min(height ? height : 720u, 4096u));
    g_slots[slot].hasCapture = false;
    VRMOD_LOG_INFO("VDisplay slot %d registered (Windows stub %ux%u — engine RT path)",
                   slot + 1, g_slots[slot].width, g_slots[slot].height);
    return slot + 1;
#else
    if (!CreateGL(g_slots[slot], width ? width : 1280, height ? height : 720, errMsg, errMsgLen)) {
        g_slots[slot].used = false;
        return 0;
    }
    g_slots[slot].used = true;
    VRMOD_LOG_INFO("VDisplay slot %d created %ux%u tex=%u fbo=%u",
                   slot + 1, g_slots[slot].width, g_slots[slot].height,
                   (unsigned)g_slots[slot].tex, (unsigned)g_slots[slot].fbo);
    return slot + 1;
#endif
}

bool VDisplay_Resize(int id, uint32_t width, uint32_t height, char* errMsg, size_t errMsgLen) {
    if (id < 1 || id > VRMOD_VDISPLAY_MAX || !g_slots[id - 1].used) {
        if (errMsg && errMsgLen)
            snprintf(errMsg, errMsgLen, "VDisplay: invalid id %d", id);
        return false;
    }
#ifdef _WIN32
    g_slots[id - 1].width = std::max(64u, std::min(width, 4096u));
    g_slots[id - 1].height = std::max(64u, std::min(height, 4096u));
    return true;
#else
    return CreateGL(g_slots[id - 1], width, height, errMsg, errMsgLen);
#endif
}

void VDisplay_Destroy(int id) {
    auto kill = [](int i) {
        if (i < 0 || i >= VRMOD_VDISPLAY_MAX) return;
        if (!g_slots[i].used) return;
#ifndef _WIN32
        if (glXGetCurrentContext() && LoadFBOExt())
            DestroyGL(g_slots[i]);
        else {
            // Context gone — abandon GL names (process teardown)
            g_slots[i].tex = 0;
            g_slots[i].fbo = 0;
        }
#endif
        g_slots[i] = VDisplaySlot{};
    };
    if (id <= 0) {
        for (int i = 0; i < VRMOD_VDISPLAY_MAX; ++i)
            kill(i);
        return;
    }
    if (id >= 1 && id <= VRMOD_VDISPLAY_MAX)
        kill(id - 1);
}

bool VDisplay_GetInfo(int id, VDisplayInfo* out) {
    if (!out || id < 1 || id > VRMOD_VDISPLAY_MAX || !g_slots[id - 1].used)
        return false;
    const VDisplaySlot& s = g_slots[id - 1];
    out->id = id;
    out->width = s.width;
    out->height = s.height;
    out->valid = true;
    out->hasCapture = s.hasCapture;
#ifdef _WIN32
    out->glTexture = 0;
    out->glFBO = 0;
#else
    out->glTexture = (unsigned int)s.tex;
    out->glFBO = (unsigned int)s.fbo;
#endif
    return true;
}

bool VDisplay_Clear(int id, float r, float g, float b, float a) {
#ifdef _WIN32
    (void)id;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
    return false;
#else
    if (id < 1 || id > VRMOD_VDISPLAY_MAX || !g_slots[id - 1].used)
        return false;
    if (!glXGetCurrentContext() || !LoadFBOExt())
        return false;
    VDisplaySlot& s = g_slots[id - 1];
    GLint prevDraw = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);
    glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, s.fbo);
    glViewport(0, 0, (GLsizei)s.width, (GLsizei)s.height);
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, (GLuint)prevDraw);
    return true;
#endif
}

bool VDisplay_CaptureWindow(int id, char* errMsg, size_t errMsgLen) {
#ifdef _WIN32
    if (errMsg && errMsgLen)
        snprintf(errMsg, errMsgLen, "VDisplay: CaptureWindow not implemented on Windows yet");
    return false;
#else
    if (id < 1 || id > VRMOD_VDISPLAY_MAX || !g_slots[id - 1].used) {
        if (errMsg && errMsgLen)
            snprintf(errMsg, errMsgLen, "VDisplay: invalid id %d", id);
        return false;
    }
    if (!glXGetCurrentContext()) {
        if (errMsg && errMsgLen)
            snprintf(errMsg, errMsgLen, "VDisplay: no GLX context for capture");
        return false;
    }
    if (!LoadFBOExt()) {
        if (errMsg && errMsgLen)
            snprintf(errMsg, errMsgLen, "VDisplay: FBO ext missing for capture");
        return false;
    }

    VDisplaySlot& s = g_slots[id - 1];
    GLint prevDraw = 0, prevRead = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);

    // Read from default framebuffer (GMod's window backbuffer)
    glBindFramebufferPtr(GL_READ_FRAMEBUFFER, 0);
    glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, s.fbo);

    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    // Prefer drawable size via GLX when possible
    int srcW = vp[2] > 0 ? vp[2] : (int)s.width;
    int srcH = vp[3] > 0 ? vp[3] : (int)s.height;
    Display* dpy = glXGetCurrentDisplay();
    GLXDrawable draw = glXGetCurrentDrawable();
    if (dpy && draw) {
        unsigned int dw = 0, dh = 0;
        // glXQueryDrawable is widely available
        typedef void (*glXQueryDrawable_t)(Display*, GLXDrawable, int, unsigned int*);
        // GLX_WIDTH=0x801D GLX_HEIGHT=0x801E
        auto q = (glXQueryDrawable_t)glXGetProcAddress((const GLubyte*)"glXQueryDrawable");
        if (q) {
            q(dpy, draw, 0x801D, &dw);
            q(dpy, draw, 0x801E, &dh);
            if (dw > 0 && dh > 0) {
                srcW = (int)dw;
                srcH = (int)dh;
            }
        }
    }

    glBlitFramebufferPtr(
        0, 0, srcW, srcH,
        0, 0, (GLint)s.width, (GLint)s.height,
        GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glBindFramebufferPtr(GL_DRAW_FRAMEBUFFER, (GLuint)prevDraw);
    glBindFramebufferPtr(GL_READ_FRAMEBUFFER, (GLuint)prevRead);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        s.hasCapture = false;
        if (errMsg && errMsgLen)
            snprintf(errMsg, errMsgLen, "VDisplay: capture GL error 0x%x", (unsigned)err);
        return false;
    }
    s.hasCapture = true;
    return true;
#endif
}

void VDisplay_Shutdown() {
    VDisplay_Destroy(0);
    VRMOD_LOG_INFO("VDisplay shutdown");
}
