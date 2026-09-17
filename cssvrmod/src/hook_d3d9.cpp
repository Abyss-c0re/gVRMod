// Original vrmod-module path: IDirect3DDevice9::CreateTexture (shaderapidx9 / togl).
// Windows: vtable patch in src/rendering/d3d/d3d_hooks.cpp (master module).
// Linux togl: same D3D9 ABI. Export interpose below is the symbol half;
// live calls go through the device vtable (same as Windows) — steal that
// slot once GetAdapterCount no longer SEGV. OpenGL swap remains priority.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

namespace {

void Log(const char* fmt, ...) {
  FILE* f = std::fopen("/tmp/cssvrmod.log", "a");
  if (!f) return;
  std::fputs("d3d9: ", f);
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(f, fmt, ap);
  va_end(ap);
  std::fputc('\n', f);
  std::fclose(f);
}

// togl: IDirect3DDevice9::CreateTexture(w,h,levels,usage,fmt,pool,tex**,void**,char*)
using CreateTextureFn = int (*)(void* self, unsigned w, unsigned h, unsigned levels, unsigned usage,
                                int format, int pool, void** tex, void** extra, char* name);
using PresentFn = int (*)(void* self, const void* src, const void* dst, void* hwnd, const void* dirty);

CreateTextureFn g_createTex = nullptr;
PresentFn g_present = nullptr;
int g_creates = 0;
int g_presents = 0;

void* Next(const char* mangled) {
  void* p = dlsym(RTLD_NEXT, mangled);
  if (!p) {
    void* togl = dlopen("libtogl.so", RTLD_NOW | RTLD_NOLOAD);
    if (togl) p = dlsym(togl, mangled);
  }
  return p;
}

} // namespace

extern "C" {

// _ZN16IDirect3DDevice913CreateTextureEjjjj10_D3DFORMAT8_D3DPOOLPP17IDirect3DTexture9PPvPc
int _ZN16IDirect3DDevice913CreateTextureEjjjj10_D3DFORMAT8_D3DPOOLPP17IDirect3DTexture9PPvPc(
    void* self, unsigned w, unsigned h, unsigned levels, unsigned usage, int format, int pool,
    void** tex, void** extra, char* name) {
  if (!g_createTex)
    g_createTex = (CreateTextureFn)Next(
        "_ZN16IDirect3DDevice913CreateTextureEjjjj10_D3DFORMAT8_D3DPOOLPP17IDirect3DTexture9PPvPc");
  g_creates++;
  if (g_creates < 8 || (g_creates % 64) == 0)
    Log("CreateTexture #%d %ux%u usage=%u fmt=%d pool=%d extra=%p", g_creates, w, h, usage, format,
        pool, extra);
  if (!g_createTex) return 0x8876086C; // D3DERR_INVALIDCALL
  return g_createTex(self, w, h, levels, usage, format, pool, tex, extra, name);
}

int _ZN16IDirect3DDevice97PresentEPK5_RECTS2_PvPK7RGNDATA(void* self, const void* src,
                                                          const void* dst, void* hwnd,
                                                          const void* dirty) {
  if (!g_present)
    g_present =
        (PresentFn)Next("_ZN16IDirect3DDevice97PresentEPK5_RECTS2_PvPK7RGNDATA");
  g_presents++;
  if (g_presents < 5 || (g_presents % 120) == 0) Log("Present #%d", g_presents);
  if (!g_present) return 0x8876086C;
  return g_present(self, src, dst, hwnd, dirty);
}

} // extern "C"

__attribute__((constructor)) static void d3d9_ctor() { Log("togl/d3d9 CreateTexture intercept ready"); }
