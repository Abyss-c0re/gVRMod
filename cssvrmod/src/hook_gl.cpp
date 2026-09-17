#include "cssvrmod/hook_api.hpp"
#include "cssvrmod/input.hpp"
#include "cssvrmod/launch.hpp"
#include "cssvrmod/source_if.hpp"
#include "cssvrmod/tick.hpp"
#include "cssvrmod/weapons.hpp"
#include "xr_host.hpp"

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace cssvr {
namespace {

const char* g_status = "idle";
bool g_inited = false;
bool g_xr_ok = false;
GLuint g_cap = 0;
int g_capW = 0, g_capH = 0;
int g_swaps = 0;
int g_dumps = 0;
bool g_want_xr = false;
std::string g_dump_dir;
WallState g_leftWall, g_rightWall;
float g_nextMelee = 0.f;
float g_now = 0.f;
EngineIf g_eng;
const WeaponInfo* g_wep = nullptr;
InputConfig g_icfg;
MeleeConfig g_mcfg;

using SwapSdlFn = void (*)(void*);
using SwapGlxFn = void (*)(Display*, GLXDrawable);
SwapSdlFn g_realSdlSwap = nullptr;
SwapGlxFn g_realGlxSwap = nullptr;

using SdlPushEventFn = int (*)(void*);
using SdlGetWindowSizeFn = void (*)(void*, int*, int*);
SdlPushEventFn g_sdlPush = nullptr;

void Log(const char* msg) {
  FILE* f = std::fopen("/tmp/cssvrmod.log", "a");
  if (!f) return;
  std::fputs(msg, f);
  std::fputc('\n', f);
  std::fclose(f);
}

void EnsureReals() {
  if (!g_realSdlSwap)
    g_realSdlSwap = reinterpret_cast<SwapSdlFn>(dlsym(RTLD_NEXT, "SDL_GL_SwapWindow"));
  if (!g_realGlxSwap)
    g_realGlxSwap = reinterpret_cast<SwapGlxFn>(dlsym(RTLD_NEXT, "glXSwapBuffers"));
  if (!g_sdlPush)
    g_sdlPush = reinterpret_cast<SdlPushEventFn>(dlsym(RTLD_DEFAULT, "SDL_PushEvent"));
}

bool EnvOn(const char* k, bool def = false) {
  const char* e = std::getenv(k);
  if (!e || !e[0]) return def;
  return !(e[0] == '0' && e[1] == 0);
}

void Logf(const char* fmt, ...) {
  FILE* f = std::fopen("/tmp/cssvrmod.log", "a");
  if (!f) return;
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(f, fmt, ap);
  va_end(ap);
  std::fputc('\n', f);
  std::fclose(f);
}

void EnsureDumpDir() {
  if (!g_dump_dir.empty()) return;
  if (const char* e = std::getenv("CSSVR_DUMP_DIR")) g_dump_dir = e;
  else g_dump_dir = "/home/voldemar/Dev/GMod/gVRMod/.scratch/cssvrmod";
  mkdir(g_dump_dir.c_str(), 0755);
}

bool QueryDrawableSize(unsigned* w, unsigned* h) {
  *w = *h = 0;
  Display* dpy = glXGetCurrentDisplay();
  GLXDrawable draw = glXGetCurrentDrawable();
  if (dpy && draw) {
    glXQueryDrawable(dpy, draw, GLX_WIDTH, w);
    glXQueryDrawable(dpy, draw, GLX_HEIGHT, h);
  }
  if (*w == 0 || *h == 0) {
    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    *w = (unsigned)vp[2];
    *h = (unsigned)vp[3];
  }
  return *w > 0 && *h > 0;
}

// Sample uniqueness: not a solid clear color (loading black / magenta).
bool PixelsLookLikeScene(const unsigned char* rgba, int w, int h, int* out_nonzero, int* out_unique) {
  const int n = w * h;
  int nonzero = 0;
  unsigned seen[64] = {};
  int unique = 0;
  const int step = n > 4000 ? n / 4000 : 1;
  for (int i = 0; i < n; i += step) {
    const unsigned char* p = rgba + (size_t)i * 4;
    if (p[0] | p[1] | p[2]) nonzero++;
    unsigned key = ((unsigned)p[0] << 16) | ((unsigned)p[1] << 8) | p[2];
    unsigned bucket = key % 64;
    bool have = false;
    for (int b = 0; b < 64; ++b) {
      unsigned s = seen[(bucket + b) % 64];
      if (s == 0) {
        seen[(bucket + b) % 64] = key ? key : 1;
        unique++;
        have = true;
        break;
      }
      if (s == (key ? key : 1)) {
        have = true;
        break;
      }
    }
    (void)have;
  }
  if (out_nonzero) *out_nonzero = nonzero;
  if (out_unique) *out_unique = unique;
  return unique >= 12 && nonzero > 40;
}

bool ReadFbRgba(GLint fbo, int w, int h, std::vector<unsigned char>& out) {
  out.assign((size_t)w * (size_t)h * 4, 0);
  GLint prevRead = 0, prevDraw = 0;
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
  if (fbo == 0) glReadBuffer(GL_FRONT);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
  const GLenum err = glGetError();
  glBindFramebuffer(GL_READ_FRAMEBUFFER, prevRead);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDraw);
  return err == GL_NO_ERROR;
}

void WritePpm(const std::string& path, int w, int h, const unsigned char* rgba) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return;
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  // OpenGL origin is bottom-left; write top-down.
  std::vector<unsigned char> row((size_t)w * 3);
  for (int y = h - 1; y >= 0; --y) {
    const unsigned char* src = rgba + (size_t)y * (size_t)w * 4;
    for (int x = 0; x < w; ++x) {
      row[(size_t)x * 3 + 0] = src[(size_t)x * 4 + 0];
      row[(size_t)x * 3 + 1] = src[(size_t)x * 4 + 1];
      row[(size_t)x * 3 + 2] = src[(size_t)x * 4 + 2];
    }
    std::fwrite(row.data(), 1, row.size(), f);
  }
  std::fclose(f);
}

void MaybeDump(const std::vector<unsigned char>& rgba, int w, int h, GLint fbo, bool scene) {
  if (g_dumps >= 8) return;
  const bool periodic = (g_swaps == 15 || g_swaps == 60 || g_swaps == 180 || (g_swaps % 240) == 0);
  if (!scene && !periodic) return;
  EnsureDumpDir();
  char name[256];
  std::snprintf(name, sizeof(name), "%s/cap_%03d_fbo%d_%dx%d_%s.ppm", g_dump_dir.c_str(), g_dumps,
                (int)fbo, w, h, scene ? "scene" : "raw");
  WritePpm(name, w, h, rgba.data());
  g_dumps++;
  Logf("dump %s scene=%d", name, scene ? 1 : 0);
}

void CaptureBackbuffer() {
  unsigned w = 0, h = 0;
  if (!QueryDrawableSize(&w, &h)) {
    if (g_swaps < 5 || (g_swaps % 120) == 0) Logf("capture skip: no size swap=%d", g_swaps);
    return;
  }
  if (!g_cap || g_capW != (int)w || g_capH != (int)h) {
    if (g_cap) glDeleteTextures(1, &g_cap);
    glGenTextures(1, &g_cap);
    glBindTexture(GL_TEXTURE_2D, g_cap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    g_capW = (int)w;
    g_capH = (int)h;
    Logf("capture tex %u %dx%d", (unsigned)g_cap, g_capW, g_capH);
  }

  GLint drawFbo = 0;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);

  std::vector<unsigned char> pix;
  bool ok = ReadFbRgba(drawFbo, (int)w, (int)h, pix);
  int nz = 0, uniq = 0;
  bool scene = ok && PixelsLookLikeScene(pix.data(), (int)w, (int)h, &nz, &uniq);
  if (!scene && drawFbo != 0) {
    std::vector<unsigned char> pix0;
    if (ReadFbRgba(0, (int)w, (int)h, pix0)) {
      int nz0 = 0, u0 = 0;
      if (PixelsLookLikeScene(pix0.data(), (int)w, (int)h, &nz0, &u0) || nz0 > nz) {
        pix.swap(pix0);
        nz = nz0;
        uniq = u0;
        scene = u0 >= 12 && nz0 > 40;
        drawFbo = 0;
        ok = true;
      }
    }
  }

  if (ok) {
    glBindTexture(GL_TEXTURE_2D, g_cap);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)w, (GLsizei)h, GL_RGBA, GL_UNSIGNED_BYTE,
                    pix.data());
    MaybeDump(pix, (int)w, (int)h, drawFbo, scene);
    if (scene) g_status = "captured";
    if (g_swaps < 8 || scene || (g_swaps % 120) == 0)
      Logf("capture swap=%d fbo=%d %dx%d ok=%d nz=%d uniq=%d scene=%d", g_swaps, (int)drawFbo,
           (int)w, (int)h, ok ? 1 : 0, nz, uniq, scene ? 1 : 0);
  } else if (g_swaps < 8) {
    Logf("capture read fail swap=%d fbo=%d %dx%d", g_swaps, (int)drawFbo, (int)w, (int)h);
  }
}

// Minimal SDL_Event mouse/key injection (SDL2 layout: type at 0, common padding).
// We only fill fields we need via a 128-byte blob — enough for motion/button/keyboard.
void PushSdl(const SdlInjectPlan& p) {
  if (!g_sdlPush) return;
  unsigned char ev[128];
  auto push_type = [&](uint32_t type) {
    std::memset(ev, 0, sizeof(ev));
    std::memcpy(ev, &type, 4);
    g_sdlPush(ev);
  };
  (void)p;
  (void)push_type;
  // Real SDL_PushEvent needs a correct SDL_Event. If SDL isn't resolved, skip.
  // Keyboard/mouse injection is applied via ClientCmd fallback below.
}

void ApplyClientCmd(const UserCmdOverlay& cmd, const UserCmdOverlay& prev) {
  if (!g_eng.screen_ok) return;
  auto edge = [&](int bit, const char* plus, const char* minus) {
    const bool now = (cmd.buttons & bit) != 0;
    const bool was = (prev.buttons & bit) != 0;
    if (now && !was) EngineClientCmd(g_eng, plus);
    if (!now && was) EngineClientCmd(g_eng, minus);
  };
  edge(kInAttack, "+attack", "-attack");
  edge(kInAttack2, "+attack2", "-attack2");
  edge(kInJump, "+jump", "-jump");
  edge(kInReload, "+reload", "-reload");
  edge(kInUse, "+use", "-use");
  edge(kInForward, "+forward", "-forward");
  edge(kInBack, "+back", "-back");
  edge(kInMoveLeft, "+moveleft", "-moveleft");
  edge(kInMoveRight, "+moveright", "-moveright");
  edge(kInScore, "+showscores", "-showscores");
}

UserCmdOverlay g_prevCmd{};

void Once() {
  if (g_inited) return;
  g_inited = true;
  g_status = "init";
  Log("cssvr hook init");
  EnsureDumpDir();
  g_want_xr = EnvOn("CSSVR_XR", false);
  Logf("dump_dir=%s xr=%d", g_dump_dir.c_str(), g_want_xr ? 1 : 0);
  ProbeLiveEngine(g_eng);
  g_wep = FindWeapon("weapon_knife");
  if (g_want_xr) {
    g_xr_ok = XrHostInit();
    g_status = g_xr_ok ? "xr_ok" : XrHostStatus().reason;
    Log(g_status);
  } else {
    g_xr_ok = false;
    g_status = "capture_only";
    Log("capture_only (set CSSVR_XR=1 for OpenXR)");
  }
}

} // namespace

void HookOnLoad() {
  EnsureReals();
  Log("cssvr hook loaded");
}

void HookOnUnload() {
  XrHostShutdown();
  g_status = "unloaded";
}

void HookOnSwap() {
  EnsureReals();
  g_swaps++;
  CaptureBackbuffer(); // dump before engine probe — Once() can block
  Once();
  XrSample xr{};
  const bool got = g_xr_ok && XrHostPollInput(&xr);
  if (g_xr_ok) XrHostBeginFrame();
  if (got) {
    TickIn tin;
    tin.xr = xr;
    tin.wep = g_wep;
    tin.now = g_now;
    tin.dt = 0.011f;
    tin.input = g_icfg;
    tin.melee = g_mcfg;
    tin.current_view = xr.hmd.ang;
    TickOut tout = Tick(tin, g_leftWall, g_rightWall, &g_nextMelee);
    ApplyClientCmd(tout.cmd, g_prevCmd);
    g_prevCmd = tout.cmd;
    PushSdl(tout.sdl);
    g_status = tout.status;
  }
  if (g_xr_ok) {
    if (g_cap)
      XrHostSubmitBackbuffer(g_cap, g_capW, g_capH, true);
    else
      XrHostEndFrame();
  }
  g_now += 0.011f;
}

const char* HookStatus() { return g_status; }

void HookCallSdlSwap(void* window) {
  EnsureReals();
  if (g_realSdlSwap) g_realSdlSwap(window);
  HookOnSwap();
}

void HookCallGlxSwap(void* dpy, unsigned long drawable) {
  EnsureReals();
  if (g_realGlxSwap) g_realGlxSwap((Display*)dpy, (GLXDrawable)drawable);
  HookOnSwap();
}

} // namespace cssvr

extern "C" {

void SDL_GL_SwapWindow(void* window) { cssvr::HookCallSdlSwap(window); }

void glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
  cssvr::HookCallGlxSwap((void*)dpy, (unsigned long)drawable);
}

} // extern "C"
