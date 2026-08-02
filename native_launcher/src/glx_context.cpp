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
  XFree(vi);
  if (!g.ctx) return false;
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
