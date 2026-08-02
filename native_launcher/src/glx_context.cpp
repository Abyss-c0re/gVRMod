#include "glx_context.hpp"
#include <cstdio>

bool GlxCreate(GlxContext& g) {
  g.dpy = XOpenDisplay(nullptr);
  if (!g.dpy) return false;
  int scr = DefaultScreen(g.dpy);
  int attribs[] = {
    GLX_RGBA, GLX_DOUBLEBUFFER,
    GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_DEPTH_SIZE, 16,
    None
  };
  XVisualInfo* vi = glXChooseVisual(g.dpy, scr, attribs);
  if (!vi) return false;
  g.visualid = (uint32_t)vi->visualid;
  g.cmap = XCreateColormap(g.dpy, RootWindow(g.dpy, scr), vi->visual, AllocNone);
  XSetWindowAttributes swa{};
  swa.colormap = g.cmap;
  swa.event_mask = StructureNotifyMask;
  g.win = XCreateWindow(g.dpy, RootWindow(g.dpy, scr), 0, 0, 64, 64, 0,
                        vi->depth, InputOutput, vi->visual,
                        CWColormap | CWEventMask, &swa);
  XStoreName(g.dpy, g.win, "cube_webui_glx");
  XMapWindow(g.dpy, g.win);
  XFlush(g.dpy);
  g.ctx = glXCreateContext(g.dpy, vi, nullptr, GL_TRUE);
  // Resolve FBConfig matching this visual (OpenXR binding requires it — research-3)
  int fbId = 0;
  if (g.ctx && glXQueryContext(g.dpy, g.ctx, GLX_FBCONFIG_ID, &fbId) == Success && fbId) {
    int fbAttr[] = {GLX_FBCONFIG_ID, fbId, None};
    int n = 0;
    GLXFBConfig* fbs = glXChooseFBConfig(g.dpy, scr, fbAttr, &n);
    if (fbs && n > 0) {
      g.fbConfig = fbs[0];
      XFree(fbs);
    }
  }
  if (!g.fbConfig) {
    // Fallback: any FBConfig with RGBA double-buffer
    int fbAttr[] = {
      GLX_RENDER_TYPE, GLX_RGBA_BIT,
      GLX_DOUBLEBUFFER, True,
      GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
      GLX_DEPTH_SIZE, 16,
      None
    };
    int n = 0;
    GLXFBConfig* fbs = glXChooseFBConfig(g.dpy, scr, fbAttr, &n);
    if (fbs && n > 0) {
      g.fbConfig = fbs[0];
      XVisualInfo* vi2 = glXGetVisualFromFBConfig(g.dpy, g.fbConfig);
      if (vi2) {
        g.visualid = (uint32_t)vi2->visualid;
        XFree(vi2);
      }
      XFree(fbs);
    }
  }
  XFree(vi);
  if (!g.ctx) return false;
  fprintf(stderr, "[cube_webui] GLX visualid=%u fbConfig=%p\n", g.visualid, (void*)g.fbConfig);
  return glXMakeCurrent(g.dpy, g.win, g.ctx);
}

void GlxDestroy(GlxContext& g) {
  if (g.dpy && g.ctx) {
    glXMakeCurrent(g.dpy, None, nullptr);
    glXDestroyContext(g.dpy, g.ctx);
  }
  if (g.dpy && g.win) XDestroyWindow(g.dpy, g.win);
  if (g.dpy && g.cmap) XFreeColormap(g.dpy, g.cmap);
  if (g.dpy) XCloseDisplay(g.dpy);
  g = {};
}
