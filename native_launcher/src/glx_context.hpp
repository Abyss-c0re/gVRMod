#pragma once
#include <X11/Xlib.h>
#include <GL/glx.h>

struct GlxContext {
  Display* dpy = nullptr;
  Window win = 0;
  GLXContext ctx = nullptr;
  Colormap cmap = 0;
};

bool GlxCreate(GlxContext& g);
void GlxDestroy(GlxContext& g);
