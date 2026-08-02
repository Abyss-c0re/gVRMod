#pragma once
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <cstdint>

struct GlxContext {
  Display* dpy = nullptr;
  Window win = 0;
  GLXContext ctx = nullptr;
  Colormap cmap = 0;
  // Filled for OpenXR Xlib graphics binding (must match module xr_session)
  uint32_t visualid = 0;
  GLXFBConfig fbConfig = nullptr;
};

bool GlxCreate(GlxContext& g);
void GlxDestroy(GlxContext& g);
