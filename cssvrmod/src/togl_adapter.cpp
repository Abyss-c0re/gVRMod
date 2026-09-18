// libtogl GetAdapterCount → launcher display-DB vtable SEGV on this GPU/SDL
// before any GL context exists. Patch adapter queries so shaderapidx9 can
// reach CreateDevice / SDL_GL_SwapWindow. CreateDevice stays in real togl.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "cssvrmod/hook_api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#ifndef GLX_CONTEXT_MAJOR_VERSION_ARB
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#endif

namespace {

void Log(const char* msg) {
  FILE* f = std::fopen("/tmp/cssvrmod.log", "a");
  if (!f) return;
  std::fputs("togl: ", f);
  std::fputs(msg, f);
  std::fputc('\n', f);
  std::fclose(f);
}

bool Unprotect(void* p, size_t n) {
  long ps = sysconf(_SC_PAGESIZE);
  auto addr = (uintptr_t)p & ~(uintptr_t)(ps - 1);
  return mprotect((void*)addr, (size_t)ps * 2, PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
}

void PatchAbsJump(void* from, void* to) {
  auto* b = static_cast<uint8_t*>(from);
  uint64_t addr = (uint64_t)(uintptr_t)to;
  uint32_t lo = (uint32_t)addr, hi = (uint32_t)(addr >> 32);
  b[0] = 0x68; // push imm32
  std::memcpy(b + 1, &lo, 4);
  b[5] = 0xC7;
  b[6] = 0x44;
  b[7] = 0x24;
  b[8] = 0x04; // mov dword [rsp+4], imm32
  std::memcpy(b + 9, &hi, 4);
  b[13] = 0xC3; // ret
}

// D3D9 constants
constexpr int kOk = 0;
constexpr int kFmtX8R8G8B8 = 22;
constexpr int kFmtA8R8G8B8 = 21;
constexpr int kDevHal = 1;

struct DisplayMode {
  unsigned Width, Height, RefreshRate, Format;
};

// Packed like Microsoft D3DCAPS9 (x64 System-V: this in rdi, rest in regs).
struct D3DCAPS9 {
  unsigned DeviceType, AdapterOrdinal;
  unsigned Caps, Caps2, Caps3, PresentationIntervals;
  unsigned CursorCaps, DevCaps;
  unsigned PrimitiveMiscCaps, RasterCaps, ZCmpCaps;
  unsigned SrcBlendCaps, DestBlendCaps, AlphaCmpCaps, ShadeCaps;
  unsigned TextureCaps, TextureFilterCaps, CubeTextureFilterCaps;
  unsigned VolumeTextureFilterCaps, TextureAddressCaps, VolumeTextureAddressCaps;
  unsigned LineCaps;
  unsigned MaxTextureWidth, MaxTextureHeight, MaxVolumeExtent;
  unsigned MaxTextureRepeat, MaxTextureAspectRatio, MaxAnisotropy;
  float MaxVertexW;
  float GuardBandLeft, GuardBandTop, GuardBandRight, GuardBandBottom, ExtentsAdjust;
  unsigned StencilCaps, FVFCaps, TextureOpCaps;
  unsigned MaxTextureBlendStages, MaxSimultaneousTextures;
  unsigned VertexProcessingCaps, MaxActiveLights, MaxUserClipPlanes;
  unsigned MaxVertexBlendMatrices, MaxVertexBlendMatrixIndex;
  float MaxPointSize;
  unsigned MaxPrimitiveCount, MaxVertexIndex, MaxStreams, MaxStreamStride;
  unsigned VertexShaderVersion, MaxVertexShaderConst, PixelShaderVersion;
  float PixelShader1xMaxValue;
  unsigned DevCaps2;
  float MaxNpatchTessellationLevel;
  unsigned Reserved5;
  unsigned MasterAdapterOrdinal, AdapterOrdinalInGroup, NumberOfAdaptersInGroup;
  unsigned DeclTypes, NumSimultaneousRTs, StretchRectFilterCaps;
  unsigned VS20_caps, VS20_DynamicFlowControlDepth, VS20_NumTemps, VS20_StaticFlowControlDepth;
  unsigned PS20_caps, PS20_DynamicFlowControlDepth, PS20_NumTemps, PS20_StaticFlowControlDepth;
  unsigned PS20_NumInstructionSlots;
  unsigned VertexTextureFilterCaps;
  unsigned MaxVShaderInstructionsExecuted, MaxPShaderInstructionsExecuted;
  unsigned MaxVertexShader30InstructionSlots, MaxPixelShader30InstructionSlots;
};

void FillMode(DisplayMode* m) {
  m->Width = 1280;
  m->Height = 720;
  m->RefreshRate = 60;
  m->Format = kFmtX8R8G8B8;
}

void FillCaps(D3DCAPS9* c) {
  std::memset(c, 0, sizeof(*c));
  c->DeviceType = kDevHal;
  c->AdapterOrdinal = 0;
  c->Caps = 0x20000;      // D3DCAPS_READ_SCANLINE-ish
  c->Caps2 = 0x00080000;  // D3DCAPS2_CANAUTOGENMIPMAP
  c->PresentationIntervals = 0x80000001; // IMMEDIATE | ONE
  c->CursorCaps = 1;
  c->DevCaps = 0x00010000 | 0x00000400 | 0x00000010; // HWTnL | HWRASTER | EXECUTESYSTEMMEMORY
  c->PrimitiveMiscCaps = 0x00280002;
  c->RasterCaps = 0x00C009A1;
  c->ZCmpCaps = 0xFF;
  c->SrcBlendCaps = 0x3FFF;
  c->DestBlendCaps = 0x3FFF;
  c->AlphaCmpCaps = 0xFF;
  c->ShadeCaps = 0x00080000;
  c->TextureCaps = 0x00000005 | 0x00001000; // ALPHA | CUBEMAP
  c->TextureFilterCaps = 0x07030700;
  c->CubeTextureFilterCaps = 0x07030700;
  c->TextureAddressCaps = 0x1F;
  c->LineCaps = 0x1F;
  c->MaxTextureWidth = 16384;
  c->MaxTextureHeight = 16384;
  c->MaxVolumeExtent = 256;
  c->MaxTextureRepeat = 8192;
  c->MaxTextureAspectRatio = 16384;
  c->MaxAnisotropy = 16;
  c->MaxVertexW = 1.0e10f;
  c->StencilCaps = 0x1FF;
  c->FVFCaps = 0x00180008;
  c->TextureOpCaps = 0xFFFFFFFF;
  c->MaxTextureBlendStages = 8;
  c->MaxSimultaneousTextures = 8;
  c->VertexProcessingCaps = 0x00000079;
  c->MaxActiveLights = 8;
  c->MaxUserClipPlanes = 6;
  c->MaxVertexBlendMatrices = 4;
  c->MaxPointSize = 256.f;
  c->MaxPrimitiveCount = 0xFFFFF;
  c->MaxVertexIndex = 0xFFFFF;
  c->MaxStreams = 16;
  c->MaxStreamStride = 256;
  c->VertexShaderVersion = 0xFFFE0300;
  c->MaxVertexShaderConst = 256;
  c->PixelShaderVersion = 0xFFFF0300;
  c->PixelShader1xMaxValue = 8.f;
  c->NumSimultaneousRTs = 4;
  c->StretchRectFilterCaps = 0x07030700;
  c->MaxVertexShader30InstructionSlots = 32768;
  c->MaxPixelShader30InstructionSlots = 32768;
}

void (*g_togl_gl_init)() = nullptr;
using GetOpenGLEntryPointsFn = void* (*)(void*);
GetOpenGLEntryPointsFn g_get_gl_entries = nullptr;
bool g_gl_entries_ok = false;
Display* g_dummy_dpy = nullptr;
GLXContext g_dummy_ctx = nullptr;
Window g_dummy_win = 0;

void* DummyGlx() {
  if (glXGetCurrentContext()) return glXGetCurrentContext();
  g_dummy_dpy = XOpenDisplay(nullptr);
  if (!g_dummy_dpy) {
    Log("XOpenDisplay failed");
    return nullptr;
  }
  int dummy = 0;
  if (!glXQueryExtension(g_dummy_dpy, &dummy, &dummy)) {
    Log("no GLX");
    return nullptr;
  }
  static int fb_attrs[] = {GLX_X_RENDERABLE,
                           True,
                           GLX_DRAWABLE_TYPE,
                           GLX_WINDOW_BIT,
                           GLX_RENDER_TYPE,
                           GLX_RGBA_BIT,
                           GLX_X_VISUAL_TYPE,
                           GLX_TRUE_COLOR,
                           GLX_RED_SIZE,
                           8,
                           GLX_GREEN_SIZE,
                           8,
                           GLX_BLUE_SIZE,
                           8,
                           GLX_ALPHA_SIZE,
                           8,
                           GLX_DEPTH_SIZE,
                           24,
                           GLX_DOUBLEBUFFER,
                           True,
                           None};
  int ncfg = 0;
  GLXFBConfig* cfgs = glXChooseFBConfig(g_dummy_dpy, DefaultScreen(g_dummy_dpy), fb_attrs, &ncfg);
  XVisualInfo* vi = nullptr;
  GLXFBConfig cfg = nullptr;
  if (cfgs && ncfg > 0) {
    cfg = cfgs[0];
    vi = glXGetVisualFromFBConfig(g_dummy_dpy, cfg);
    XFree(cfgs);
  }
  if (!vi) {
    static int attrs[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
                          GLX_BLUE_SIZE, 8, GLX_DEPTH_SIZE, 16, None};
    vi = glXChooseVisual(g_dummy_dpy, DefaultScreen(g_dummy_dpy), attrs);
  }
  if (!vi) {
    Log("no GLX visual");
    return nullptr;
  }
  using Create33 = GLXContext (*)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
  auto create33 = (Create33)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");
  if (create33 && cfg) {
    int ctx_attrs[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                       3,
                       GLX_CONTEXT_MINOR_VERSION_ARB,
                       3,
                       GLX_CONTEXT_PROFILE_MASK_ARB,
                       GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                       None};
    g_dummy_ctx = create33(g_dummy_dpy, cfg, nullptr, True, ctx_attrs);
  }
  if (!g_dummy_ctx) g_dummy_ctx = glXCreateContext(g_dummy_dpy, vi, nullptr, True);
  Window root = RootWindow(g_dummy_dpy, vi->screen);
  Colormap cmap = XCreateColormap(g_dummy_dpy, root, vi->visual, AllocNone);
  XSetWindowAttributes swa{};
  swa.colormap = cmap;
  Window win = XCreateWindow(g_dummy_dpy, root, 0, 0, 64, 64, 0, vi->depth, InputOutput, vi->visual,
                             CWColormap, &swa);
  g_dummy_win = win;
  if (!g_dummy_ctx || !win || !glXMakeCurrent(g_dummy_dpy, win, g_dummy_ctx)) {
    Log("dummy GLX make current failed");
    return nullptr;
  }
  Log("dummy GLX 3.3 context ok");
  return g_dummy_ctx;
}

// togl callback ABI is messy — try name in arg0 or arg1; never report fail (avoids zenity Error()).
extern "C" void* CssvrToglGetProc(void* a, void* b, void* c, void* d) {
  static int nlog = 0;
  if (nlog < 4) {
    FILE* lf = std::fopen("/tmp/cssvrmod_glproc.log", "a");
    if (lf) {
      std::fprintf(lf, "a=%p b=%p c=%p d=%p\n", a, b, c, d);
      std::fclose(lf);
    }
    nlog++;
  }
  (void)c;
  (void)d;
  const char* name = (const char*)a;
  if (!name || name[0] < 32 || name[0] > 126) name = (const char*)b;
  void* p = nullptr;
  if (name && name[0] >= 32 && name[0] <= 126) {
    using SdlGPA = void* (*)(const char*);
    auto sdl = (SdlGPA)dlsym(RTLD_DEFAULT, "SDL_GL_GetProcAddress");
    if (sdl) p = sdl(name);
    if (!p) p = (void*)glXGetProcAddressARB((const GLubyte*)name);
    if (!p) p = (void*)glXGetProcAddress((const GLubyte*)name);
    if (!p) p = dlsym(RTLD_DEFAULT, name);
    if (!p) {
      void* gl = dlopen("libGL.so.1", RTLD_NOW);
      if (gl) p = dlsym(gl, name);
    }
  }
  // Second arg is okay (starts 1). Helper treats *okay==0 as missing.
  if (b && (uintptr_t)b > 0x10000) *static_cast<bool*>(b) = (p != nullptr);
  if (nlog <= 4) {
    FILE* lf = std::fopen("/tmp/cssvrmod_glproc.log", "a");
    if (lf) {
      std::fprintf(lf, "  name=%.40s p=%p\n", name ? name : "(null)", p);
      std::fclose(lf);
    }
  }
  return p;
}

void HookSwapInTable(void* table) {
  if (!table || g_gl_entries_ok) return;
  void* real_glx = dlsym(RTLD_NEXT, "glXSwapBuffers");
  if (!real_glx) real_glx = dlsym(RTLD_DEFAULT, "glXSwapBuffers");
  void* real_sdl = dlsym(RTLD_DEFAULT, "SDL_GL_SwapWindow");
  auto* slots = static_cast<void**>(table);
  int n = 0;
  for (int i = 0; i < (0x710 / 8); ++i) {
    if (real_glx && slots[i] == real_glx) {
      slots[i] = (void*)cssvr::HookCallGlxSwap;
      n++;
    }
    if (real_sdl && slots[i] == real_sdl) {
      slots[i] = (void*)cssvr::HookCallSdlSwap;
      n++;
    }
  }
  if (n > 0) {
    g_gl_entries_ok = true;
    Log("hooked swap in COpenGLEntryPoints");
  }
}

void** GlTableSlot() {
  // GetOpenGLEntryPoints+0x8: mov r12, [rip+rel] → global COpenGLEntryPoints*
  if (!g_get_gl_entries) return nullptr;
  auto* p = reinterpret_cast<uint8_t*>(g_get_gl_entries);
  if (p[8] != 0x4c || p[9] != 0x8b || p[10] != 0x25) return nullptr;
  int32_t rel = 0;
  std::memcpy(&rel, p + 11, 4);
  return reinterpret_cast<void**>(p + 15 + rel);
}

// launcher.so BSS: COpenGLEntryPoints* used by the GL_NVX extension dump
// inside CreateDevice. If this is null, cmpb 0x6f8(%rax) SIGSEGVs.
constexpr uintptr_t kLauncherGGL = 0x58b98;

void* LauncherBase() {
  void* h = dlopen("launcher.so", RTLD_NOW | RTLD_NOLOAD);
  if (!h) return nullptr;
  void* sym = dlsym(h, "LauncherMain");
  if (!sym) return nullptr;
  Dl_info in{};
  if (!dladdr(sym, &in) || !in.dli_fbase) return nullptr;
  return in.dli_fbase;
}

void SeedLauncherGGL(void* table) {
  if (!table) return;
  void* base = LauncherBase();
  if (!base) {
    Log("launcher base missing");
    return;
  }
  *reinterpret_cast<void**>(static_cast<char*>(base) + kLauncherGGL) = table;
}

bool g_seeded = false;
void HookLauncherGetDisplayDB();

void PopulateAndSeed() {
  if (g_seeded || !g_get_gl_entries) return;
  HookLauncherGetDisplayDB();
  if (!glXGetCurrentContext()) DummyGlx();
  void* table = g_get_gl_entries((void*)CssvrToglGetProc);
  if (!table) {
    Log("GetOpenGLEntryPoints returned null");
    return;
  }
  SeedLauncherGGL(table);
  // Constructor stores glGetString at +0x220. Force it if lookup left NULL.
  auto* getstr = reinterpret_cast<void**>(static_cast<char*>(table) + 0x220);
  if (!*getstr) *getstr = (void*)glGetString;
  HookSwapInTable(table);
  g_seeded = true;
  Log("PopulateAndSeed ok");
}

void* g_db_slots[16];
struct FakeDisplayDB {
  void** vptr;
} g_fake_db{};

int DbRet1(void*) { return 1; }
void DbDtor(void*) {}
int DbFill(void*, long a, long b, void* c, void* d, void* e) {
  (void)a;
  (void)b;
  auto fill = [](void* p) {
    if (!p) return;
    std::memset(p, 0, 128);
    auto* u = static_cast<unsigned*>(p);
    u[0] = 1280;
    u[1] = 720;
    u[2] = 60;
  };
  fill(c);
  fill(d);
  fill(e);
  return 1;
}

void* GetDisplayDB_Hook(void*) {
  if (!g_fake_db.vptr) {
    for (int i = 0; i < 16; ++i) g_db_slots[i] = (void*)DbFill;
    g_db_slots[0] = (void*)DbDtor;
    g_db_slots[1] = (void*)DbDtor;
    g_db_slots[2] = (void*)DbRet1;
    g_db_slots[4] = (void*)DbRet1;
    g_db_slots[5] = (void*)DbRet1;
    g_db_slots[7] = (void*)DbRet1;
    g_db_slots[9] = (void*)DbRet1;
    g_db_slots[11] = (void*)DbRet1;
    g_fake_db.vptr = g_db_slots;
  }
  return &g_fake_db;
}

void HookLauncherGetDisplayDB() {
  static bool done = false;
  if (done) return;
  void** table_slot = GlTableSlot();
  if (!table_slot) return;
  void** mgr_slot = table_slot + 1; // togl b4aa0 = g_pLauncherMgr
  void* mgr = *mgr_slot;
  if (!mgr) return;
  void** orig_vt = *reinterpret_cast<void***>(mgr);
  if (!orig_vt) return;
  static void* vtcopy[64];
  std::memcpy(vtcopy, orig_vt, sizeof(vtcopy));
  vtcopy[0xd8 / 8] = (void*)GetDisplayDB_Hook;
  if (!Unprotect(mgr, 8)) return;
  *reinterpret_cast<void***>(mgr) = vtcopy;
  done = true;
  Log("hooked ILauncherMgr::GetDisplayDB");
}

void CallToglGlInit() {
  HookLauncherGetDisplayDB();
  if (g_gl_entries_ok) return;
  void** slot = GlTableSlot();
  if (slot && *slot) {
    SeedLauncherGGL(*slot);
    HookSwapInTable(*slot);
  }
}

void ReleaseDummyGlx() {
  if (g_dummy_dpy) {
    glXMakeCurrent(g_dummy_dpy, None, nullptr);
    if (g_dummy_ctx) glXDestroyContext(g_dummy_dpy, g_dummy_ctx);
    g_dummy_ctx = nullptr;
  }
}

// thiscall-as-SysV: this in rdi. Runs on the shaderapi thread.
extern "C" unsigned Stub_GetAdapterCount(void* /*self*/) {
  HookLauncherGetDisplayDB();
  return 1;
}

extern "C" int Stub_GetAdapterIdentifier(void* /*self*/, unsigned /*adapter*/, unsigned /*flags*/,
                                         void* ident) {
  if (ident) {
    std::memset(ident, 0, 0x440);
    std::strcpy(static_cast<char*>(ident), "togl");
    std::strcpy(static_cast<char*>(ident) + 512, "CSSVR togl adapter");
  }
  return kOk;
}

extern "C" unsigned Stub_GetAdapterModeCount(void* /*self*/, unsigned /*adapter*/, int /*fmt*/) {
  return 1;
}

extern "C" int Stub_EnumAdapterModes(void* /*self*/, unsigned /*adapter*/, int /*fmt*/,
                                     unsigned /*mode*/, DisplayMode* out) {
  if (out) FillMode(out);
  return kOk;
}

extern "C" int Stub_GetAdapterDisplayMode(void* /*self*/, unsigned /*adapter*/, DisplayMode* out) {
  if (out) FillMode(out);
  return kOk;
}

extern "C" int Stub_GetDeviceCaps(void* /*self*/, unsigned /*adapter*/, int /*type*/, D3DCAPS9* out) {
  if (out) FillCaps(out);
  return kOk;
}

extern "C" int Stub_CheckOk(void* /*self*/, ...) { return kOk; }

struct Patch {
  const char* name;
  void* stub;
};

bool g_patched = false;

void PatchCreateDeviceAssert(void* handle) {
  // Create() calls gGL+0x3a8 and raise()s unless the result is 0x8cd5.
  auto* create = static_cast<uint8_t*>(
      dlsym(handle, "_ZN16IDirect3DDevice96CreateEP22IDirect3DDevice9Params"));
  if (!create) return;
  // Known file offset: raise check at Create+0x169a1 relative? Use 0x424d8-0x2b530.
  auto* p = create + (0x424d8 - 0x2b530);
  if (p[0] == 0x74 && p[1] == 0x0c) {
    if (Unprotect(p, 2)) {
      p[0] = 0xeb; // je -> jmp (skip raise)
      Log("patched CreateDevice raise() assert");
    }
  }
}

void PatchTogl(void* handle) {
  if (g_patched || !handle) return;
  g_get_gl_entries = (GetOpenGLEntryPointsFn)dlsym(handle, "GetOpenGLEntryPoints");
  PatchCreateDeviceAssert(handle);
  const Patch k[] = {
      {"_ZN10IDirect3D915GetAdapterCountEv", (void*)Stub_GetAdapterCount},
      {"_ZN10IDirect3D920GetAdapterIdentifierEjjP23_D3DADAPTER_IDENTIFIER9",
       (void*)Stub_GetAdapterIdentifier},
      {"_ZN10IDirect3D919GetAdapterModeCountEj10_D3DFORMAT", (void*)Stub_GetAdapterModeCount},
      {"_ZN10IDirect3D916EnumAdapterModesEj10_D3DFORMATjP15_D3DDISPLAYMODE",
       (void*)Stub_EnumAdapterModes},
      {"_ZN10IDirect3D921GetAdapterDisplayModeEjP15_D3DDISPLAYMODE",
       (void*)Stub_GetAdapterDisplayMode},
      {"_ZN10IDirect3D913GetDeviceCapsEj11_D3DDEVTYPEP9_D3DCAPS9", (void*)Stub_GetDeviceCaps},
      {"_ZN10IDirect3D917CheckDeviceFormatEj11_D3DDEVTYPE10_D3DFORMATj16_D3DRESOURCETYPES1_",
       (void*)Stub_CheckOk},
      {"_ZN10IDirect3D915CheckDeviceTypeEj11_D3DDEVTYPE10_D3DFORMATS1_i", (void*)Stub_CheckOk},
      {"_ZN10IDirect3D922CheckDepthStencilMatchEj11_D3DDEVTYPE10_D3DFORMATS1_S1_",
       (void*)Stub_CheckOk},
      {"_ZN10IDirect3D926CheckDeviceMultiSampleTypeEj11_D3DDEVTYPE10_D3DFORMATi20_D3DMULTISAMPLE_TYPEPj",
       (void*)Stub_CheckOk},
      {nullptr, nullptr},
  };
  int n = 0;
  for (int i = 0; k[i].name; ++i) {
    void* p = dlsym(handle, k[i].name);
    if (!p) {
      Log(k[i].name);
      Log("  missing");
      continue;
    }
    if (std::strcmp(k[i].name, "_ZN10IDirect3D915GetAdapterCountEv") == 0) {
      // e8 rel32 at +4 in original: target = 0x32160 if symbol is 0x254d0
      auto* count = static_cast<uint8_t*>(p);
      if (count[4] == 0xE8) {
        int32_t rel = 0;
        std::memcpy(&rel, count + 5, 4);
        g_togl_gl_init = (void (*)())(count + 9 + rel);
        Log("resolved togl GL entry populate");
      }
    }
    if (!Unprotect(p, 16)) {
      Log("mprotect fail");
      return;
    }
    PatchAbsJump(p, k[i].stub);
    n++;
  }
  g_patched = n > 0;
  Log(g_patched ? "adapter queries patched (GetAdapterCount=1)" : "patch failed");
}

void TryPatch() {
  if (g_patched) return;
  void* t = dlopen("libtogl.so", RTLD_NOW | RTLD_NOLOAD);
  if (t) PatchTogl(t);
}

uint8_t g_sdl_orig[14];
void* g_sdl_target = nullptr;
bool g_sdl_hooked = false;

void PatchSdlSwap();

extern "C" void TrampolineSdlSwap(void* window) {
  if (g_sdl_target) {
    Unprotect(g_sdl_target, 16);
    std::memcpy(g_sdl_target, g_sdl_orig, 14);
  }
  cssvr::HookCallSdlSwap(window);
  PatchSdlSwap();
}

void PatchSdlSwap() {
  if (!g_sdl_target) return;
  Unprotect(g_sdl_target, 16);
  PatchAbsJump(g_sdl_target, (void*)TrampolineSdlSwap);
}

uint8_t g_present_orig[14];
void* g_present_target = nullptr;
bool g_present_hooked = false;

void PatchToglPresent();

extern "C" int TrampolinePresent(void* self, const void* a, const void* b, void* c, const void* d) {
  Log("Present");
  if (g_present_target) {
    Unprotect(g_present_target, 16);
    std::memcpy(g_present_target, g_present_orig, 14);
  }
  cssvr::HookOnSwap();
  using Fn = int (*)(void*, const void*, const void*, void*, const void*);
  int rc = g_present_target ? ((Fn)g_present_target)(self, a, b, c, d) : 0;
  PatchToglPresent();
  return rc;
}

void PatchToglPresent() {
  if (!g_present_target) return;
  Unprotect(g_present_target, 16);
  PatchAbsJump(g_present_target, (void*)TrampolinePresent);
}

bool TryHookToglPresent() {
  if (g_present_hooked) return true;
  void* t = dlopen("libtogl.so", RTLD_NOW | RTLD_NOLOAD);
  if (!t) return false;
  void* p = dlsym(t, "_ZN16IDirect3DDevice97PresentEPK5_RECTS2_PvPK7RGNDATA");
  if (!p) return false;
  g_present_target = p;
  std::memcpy(g_present_orig, p, 14);
  PatchToglPresent();
  g_present_hooked = true;
  Log("patched togl IDirect3DDevice9::Present");
  return true;
}

uint8_t g_glx_orig[14];
void* g_glx_target = nullptr;
bool g_glx_hooked = false;

void PatchGlxSwap();

extern "C" void TrampolineGlxSwap(void* dpy, unsigned long drawable) {
  Log("glXSwapBuffers");
  if (g_glx_target) {
    Unprotect(g_glx_target, 16);
    std::memcpy(g_glx_target, g_glx_orig, 14);
  }
  cssvr::HookCallGlxSwap(dpy, drawable);
  PatchGlxSwap();
}

void PatchGlxSwap() {
  if (!g_glx_target) return;
  Unprotect(g_glx_target, 16);
  PatchAbsJump(g_glx_target, (void*)TrampolineGlxSwap);
}

bool TryHookOneGlx(const char* lib) {
  if (g_glx_hooked) return true;
  void* h = dlopen(lib, RTLD_NOW | RTLD_NOLOAD);
  if (!h) return false;
  void* p = dlsym(h, "glXSwapBuffers");
  if (!p) return false;
  g_glx_target = p;
  std::memcpy(g_glx_orig, p, 14);
  PatchGlxSwap();
  g_glx_hooked = true;
  Log(lib);
  Log("patched glXSwapBuffers");
  return true;
}

bool TryHookGlxSwap() {
  return TryHookOneGlx("libGLX.so.0") || TryHookOneGlx("libGL.so.1") ||
         TryHookOneGlx("libGLX_mesa.so.0");
}

bool TryHookSdlSwap() {
  if (g_sdl_hooked) return true;
  void* sdl = dlopen("libSDL2-2.0.so.0", RTLD_NOW | RTLD_NOLOAD);
  if (!sdl) return false;
  void* p = dlsym(sdl, "SDL_GL_SwapWindow");
  if (!p) return false;
  g_sdl_target = p;
  std::memcpy(g_sdl_orig, p, 14);
  PatchSdlSwap();
  g_sdl_hooked = true;
  Log("patched libSDL2 SDL_GL_SwapWindow");
  return true;
}

void* WatcherSdl(void*) {
  // Byte-patching Present/Swap SIGSEGVs this togl on first frame.
  // Adapter stubs + GetOpenGLEntryPoints are enough to reach a live GL context.
  (void)TryHookSdlSwap;
  (void)TryHookToglPresent;
  (void)TryHookGlxSwap;
  return nullptr;
}

void* WatcherTogl(void*) {
  for (int i = 0; i < 40000; ++i) {
    TryPatch();
    if (g_patched) {
      CallToglGlInit();
      if (!g_seeded) PopulateAndSeed();
    }
    if (g_seeded) break;
    usleep(500);
  }
  return nullptr;
}

extern "C" void* SDL_GL_CreateContext(void* win) {
  using Fn = void* (*)(void*);
  static Fn real = nullptr;
  if (!real) real = (Fn)dlsym(RTLD_NEXT, "SDL_GL_CreateContext");
  void* ctx = real ? real(win) : nullptr;
  if (ctx) {
    Log("SDL_GL_CreateContext");
    PopulateAndSeed();
  }
  return ctx;
}

} // namespace

__attribute__((constructor)) static void togl_ctor() {
  Log("adapter patcher loaded");
  TryPatch();
  pthread_t a, b;
  if (pthread_create(&a, nullptr, WatcherSdl, nullptr) == 0) pthread_detach(a);
  if (pthread_create(&b, nullptr, WatcherTogl, nullptr) == 0) pthread_detach(b);
}
