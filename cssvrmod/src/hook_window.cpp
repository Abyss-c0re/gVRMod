// Force a decorated X11/SDL window. Source + DXVK often create BORDERLESS
// even when -noborder is omitted; Motif hints then hide the title bar.
#include "cssvrmod/window_chrome.hpp"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <cstdlib>
#include <cstdint>

namespace {

void Log(const char* fmt, ...) {
  FILE* f = std::fopen("/tmp/cssvrmod.log", "a");
  if (!f) return;
  std::fputs("win: ", f);
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(f, fmt, ap);
  va_end(ap);
  std::fputc('\n', f);
  std::fclose(f);
}

bool AllowNoborder() {
  const char* e = std::getenv("CSSVR_NOBORDER");
  return e && e[0] == '1' && e[1] == 0;
}

using CreateWinFn = void* (*)(const char*, int, int, int, int, uint32_t);
using SetBorderFn = void (*)(void*, int);
using SetFsFn = int (*)(void*, uint32_t);
using SetReszFn = void (*)(void*, int);
using XChangePropFn = int (*)(Display*, Window, Atom, Atom, int, int, const unsigned char*, int);

extern "C" void* SDL_CreateWindow(const char*, int, int, int, int, uint32_t);
extern "C" void SDL_SetWindowBordered(void*, int);
extern "C" int SDL_SetWindowFullscreen(void*, uint32_t);
extern "C" int XChangeProperty(Display*, Window, Atom, Atom, int, int, const unsigned char*, int);

CreateWinFn g_create = nullptr;
SetBorderFn g_set_border = nullptr;
SetFsFn g_set_fs = nullptr;
SetReszFn g_set_resz = nullptr;
XChangePropFn g_xchange = nullptr;
void* g_last_win = nullptr;
int g_creates = 0;

void* OpenLib(const char* name) {
  void* h = dlopen(name, RTLD_NOW | RTLD_NOLOAD);
  if (!h) h = dlopen(name, RTLD_NOW | RTLD_LOCAL);
  return h;
}

void* SymIn(void* lib, const char* name, void* self) {
  if (!lib || !name) return nullptr;
  void* p = dlsym(lib, name);
  if (p == self) return nullptr;
  return p;
}

void EnsureSdl() {
  if (g_create && g_set_border && g_set_fs && g_set_resz) return;
  void* sdl = OpenLib("libSDL2-2.0.so.0");
  if (!sdl) sdl = OpenLib("libSDL2.so");
  if (!g_create) {
    g_create = (CreateWinFn)SymIn(sdl, "SDL_CreateWindow", (void*)SDL_CreateWindow);
    if (!g_create) g_create = (CreateWinFn)dlsym(RTLD_NEXT, "SDL_CreateWindow");
    if (g_create == (CreateWinFn)SDL_CreateWindow) g_create = nullptr;
  }
  if (!g_set_border) {
    g_set_border = (SetBorderFn)SymIn(sdl, "SDL_SetWindowBordered", (void*)SDL_SetWindowBordered);
    if (!g_set_border) g_set_border = (SetBorderFn)dlsym(RTLD_NEXT, "SDL_SetWindowBordered");
    if (g_set_border == (SetBorderFn)SDL_SetWindowBordered) g_set_border = nullptr;
  }
  if (!g_set_fs) {
    g_set_fs = (SetFsFn)SymIn(sdl, "SDL_SetWindowFullscreen", (void*)SDL_SetWindowFullscreen);
    if (!g_set_fs) g_set_fs = (SetFsFn)dlsym(RTLD_NEXT, "SDL_SetWindowFullscreen");
    if (g_set_fs == (SetFsFn)SDL_SetWindowFullscreen) g_set_fs = nullptr;
  }
  if (!g_set_resz)
    g_set_resz = (SetReszFn)(sdl ? dlsym(sdl, "SDL_SetWindowResizable")
                                 : dlsym(RTLD_NEXT, "SDL_SetWindowResizable"));
}

void EnsureX11() {
  if (g_xchange) return;
  void* x11 = OpenLib("libX11.so.6");
  if (!x11) x11 = OpenLib("libX11.so");
  g_xchange = (XChangePropFn)SymIn(x11, "XChangeProperty", (void*)XChangeProperty);
  if (!g_xchange) g_xchange = (XChangePropFn)dlsym(RTLD_NEXT, "XChangeProperty");
  if (g_xchange == (XChangePropFn)XChangeProperty) g_xchange = nullptr;
}

void ForceDecorated(void* win) {
  if (!win || AllowNoborder()) return;
  EnsureSdl();
  if (g_set_fs) g_set_fs(win, 0);
  if (g_set_border) g_set_border(win, 1);
  if (g_set_resz) g_set_resz(win, 1);
}

} // namespace

extern "C" {

void* SDL_CreateWindow(const char* title, int x, int y, int w, int h, uint32_t flags) {
  EnsureSdl();
  const uint32_t raw = flags;
  if (!AllowNoborder()) flags = cssvr::SanitizeSdlWindowFlags(flags, false);
  if (!g_create) {
    Log("SDL_CreateWindow missing (no RTLD_NEXT)");
    return nullptr;
  }
  void* win = g_create(title, x, y, w, h, flags);
  g_last_win = win;
  g_creates++;
  Log("SDL_CreateWindow #%d '%s' %dx%d flags 0x%x->0x%x win=%p", g_creates,
      title ? title : "", w, h, raw, flags, win);
  ForceDecorated(win);
  return win;
}

void SDL_SetWindowBordered(void* window, int bordered) {
  EnsureSdl();
  if (!AllowNoborder()) bordered = 1;
  if (g_creates < 6) Log("SDL_SetWindowBordered %p %d", window, bordered);
  if (g_set_border) g_set_border(window, bordered);
}

int SDL_SetWindowFullscreen(void* window, uint32_t flags) {
  EnsureSdl();
  if (!AllowNoborder()) flags = 0;
  if (g_creates < 8) Log("SDL_SetWindowFullscreen %p 0x%x", window, flags);
  return g_set_fs ? g_set_fs(window, flags) : 0;
}

int XChangeProperty(Display* dpy, Window w, Atom property, Atom type, int format, int mode,
                    const unsigned char* data, int nelements) {
  EnsureX11();
  if (!g_xchange) return 0;
  if (!AllowNoborder() && dpy && property && format == 32 && data && nelements >= 3) {
    Atom motif = XInternAtom(dpy, "_MOTIF_WM_HINTS", True);
    if (motif != None && property == motif) {
      unsigned long hints[5] = {0, 0, 0, 0, 0};
      const int n = nelements < 5 ? nelements : 5;
      for (int i = 0; i < n; ++i) hints[i] = ((const unsigned long*)data)[i];
      constexpr unsigned long kDecor = 1ul << 1;
      constexpr unsigned long kDecorAll = 1ul;
      hints[0] |= kDecor;
      hints[2] = kDecorAll;
      Log("XChangeProperty motif decorations forced on win=0x%lx", (unsigned long)w);
      return g_xchange(dpy, w, property, type, format, mode,
                       reinterpret_cast<const unsigned char*>(hints), nelements);
    }
    Atom state = XInternAtom(dpy, "_NET_WM_STATE", True);
    Atom fs = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", True);
    if (state != None && fs != None && property == state && type == XA_ATOM) {
      bool stripped = false;
      unsigned long buf[16];
      int out_n = 0;
      const int n = nelements < 16 ? nelements : 16;
      for (int i = 0; i < n; ++i) {
        unsigned long a = ((const unsigned long*)data)[i];
        if (a == (unsigned long)fs) {
          stripped = true;
          continue;
        }
        buf[out_n++] = a;
      }
      if (stripped) {
        Log("XChangeProperty stripped _NET_WM_STATE_FULLSCREEN win=0x%lx", (unsigned long)w);
        return g_xchange(dpy, w, property, type, format, mode,
                         reinterpret_cast<const unsigned char*>(buf), out_n);
      }
    }
  }
  return g_xchange(dpy, w, property, type, format, mode, data, nelements);
}

} // extern "C"

__attribute__((constructor)) static void win_ctor() {
  Log("chrome hook ready force_decorated=%d", AllowNoborder() ? 0 : 1);
}
