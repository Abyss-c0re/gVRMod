#pragma once
// Renderer backends. OpenGL/togl is the product default (gVRMod Linux).
// DX9 is the original vrmod-module-master CreateTexture path (Windows
// shaderapidx9; Linux togl implements the same IDirect3DDevice9).
// Vulkan is 64-bit CSS's current default when togl will not start.
#include <cstring>

namespace cssvr {

enum class Backend { Gl = 0, Dx9 = 1, Vk = 2 };

inline const char* BackendName(Backend b) {
  switch (b) {
  case Backend::Dx9: return "dx9";
  case Backend::Vk: return "vk";
  case Backend::Gl:
  default: return "gl";
  }
}

inline Backend BackendFromName(const char* s) {
  if (!s || !s[0]) return Backend::Gl;
  if (s[0] == 'v' || s[0] == 'V') return Backend::Vk;
  if (s[0] == 'd' || s[0] == 'D') return Backend::Dx9;
  return Backend::Gl;
}

struct BackendLaunch {
  Backend backend = Backend::Gl;
  const char* engine_flag = "-dx9"; // -dx9 → shaderapidx9+togl; -vulkan → shaderapivk
  const char* sdl_video = "x11";    // togl is GLX; Wayland + togl SEGVs here
  const char* hook = "gl";          // gl swap | d3d9 CreateTexture | vk present
  const char* reason = "togl_opengl_priority";
};

/// Gl and Dx9 both start shaderapidx9. They differ in which hook is SoT:
///   gl  — SDL_GL_SwapWindow / glXSwapBuffers (gVRMod Linux)
///   dx9 — IDirect3DDevice9::CreateTexture (original vrmod module)
inline BackendLaunch BackendPlan(Backend b) {
  BackendLaunch p;
  p.backend = b;
  switch (b) {
  case Backend::Vk:
    p.engine_flag = "-vulkan";
    p.sdl_video = nullptr;
    p.hook = "vk";
    p.reason = "shaderapivk_fallback";
    break;
  case Backend::Dx9:
    p.engine_flag = "-dx9";
    p.sdl_video = "x11";
    p.hook = "d3d9";
    p.reason = "original_vrmod_createtexture";
    break;
  case Backend::Gl:
  default:
    p.engine_flag = "-dx9";
    p.sdl_video = "x11";
    p.hook = "gl";
    p.reason = "togl_opengl_priority";
    break;
  }
  return p;
}

inline bool BackendUsesTogl(Backend b) { return b == Backend::Gl || b == Backend::Dx9; }

} // namespace cssvr
