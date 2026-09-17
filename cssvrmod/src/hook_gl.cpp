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
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <string>

namespace cssvr {
namespace {

const char* g_status = "idle";
bool g_inited = false;
bool g_xr_ok = false;
GLuint g_cap = 0;
int g_capW = 0, g_capH = 0;
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

void CaptureBackbuffer() {
  Display* dpy = glXGetCurrentDisplay();
  GLXDrawable draw = glXGetCurrentDrawable();
  unsigned int w = 0, h = 0;
  if (dpy && draw) {
    glXQueryDrawable(dpy, draw, GLX_WIDTH, &w);
    glXQueryDrawable(dpy, draw, GLX_HEIGHT, &h);
  }
  if (w == 0 || h == 0) {
    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    w = (unsigned)vp[2];
    h = (unsigned)vp[3];
  }
  if (w == 0 || h == 0) return;
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
  }
  GLint prev = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, g_cap);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, g_capW, g_capH);
  glBindFramebuffer(GL_FRAMEBUFFER, prev);
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
  ProbeLiveEngine(g_eng);
  g_wep = FindWeapon("weapon_knife");
  g_xr_ok = XrHostInit();
  g_status = g_xr_ok ? "xr_ok" : XrHostStatus().reason;
  Log(g_status);
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
  Once();
  CaptureBackbuffer();
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
  HookOnSwap();
  if (g_realSdlSwap) g_realSdlSwap(window);
}

void HookCallGlxSwap(Display* dpy, GLXDrawable drawable) {
  EnsureReals();
  HookOnSwap();
  if (g_realGlxSwap) g_realGlxSwap(dpy, drawable);
}

} // namespace cssvr

extern "C" {

void SDL_GL_SwapWindow(void* window) { cssvr::HookCallSdlSwap(window); }

void glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
  cssvr::HookCallGlxSwap(dpy, drawable);
}

} // extern "C"
