// Cube WebUI OpenXR host — GLX context + stereo blit of reversed New Game panel
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>

#define XR_USE_PLATFORM_XLIB
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "xr_app.hpp"
#include "gmod_spawn.hpp"
#include "ui_panel.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <unistd.h>

// FBO entry points (GLX)
static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers_ = nullptr;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_ = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_ = nullptr;
static void LoadFBO() {
  if (glGenFramebuffers_) return;
  glGenFramebuffers_ = (PFNGLGENFRAMEBUFFERSPROC)glXGetProcAddress((const GLubyte*)"glGenFramebuffers");
  glBindFramebuffer_ = (PFNGLBINDFRAMEBUFFERPROC)glXGetProcAddress((const GLubyte*)"glBindFramebuffer");
  glFramebufferTexture2D_ = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glXGetProcAddress((const GLubyte*)"glFramebufferTexture2D");
}

static void Die(const char* m) {
  fprintf(stderr, "[cube_webui] FATAL: %s\n", m);
}

// --- GLX window (required for OpenXR OpenGL binding; not the product UI) ---
struct GlxCtx {
  Display* dpy = nullptr;
  Window win = 0;
  GLXContext ctx = nullptr;
  Colormap cmap = 0;
};

static bool MakeGlx(GlxCtx& g) {
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
  // Map briefly so drawable is valid; keep tiny (not product surface)
  XMapWindow(g.dpy, g.win);
  XFlush(g.dpy);
  g.ctx = glXCreateContext(g.dpy, vi, nullptr, GL_TRUE);
  XFree(vi);
  if (!g.ctx) return false;
  if (!glXMakeCurrent(g.dpy, g.win, g.ctx)) return false;
  return true;
}

static void DestroyGlx(GlxCtx& g) {
  if (g.dpy && g.ctx) {
    glXMakeCurrent(g.dpy, None, nullptr);
    glXDestroyContext(g.dpy, g.ctx);
  }
  if (g.dpy && g.win) XDestroyWindow(g.dpy, g.win);
  if (g.dpy && g.cmap) XFreeColormap(g.dpy, g.cmap);
  if (g.dpy) XCloseDisplay(g.dpy);
  g = {};
}

// --- simple quad texturing helpers ---
static GLuint MakeTex(int w, int h, const void* rgba) {
  GLuint t = 0;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  return t;
}

static void DrawTexturedQuad(float z = -1.2f) {
  // Panel in view space: ~1.6 x 0.9 m at z
  const float hw = 0.80f, hh = 0.45f;
  glEnable(GL_TEXTURE_2D);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 1); glVertex3f(-hw, -hh, z);
  glTexCoord2f(1, 1); glVertex3f( hw, -hh, z);
  glTexCoord2f(1, 0); glVertex3f( hw,  hh, z);
  glTexCoord2f(0, 0); glVertex3f(-hw,  hh, z);
  glEnd();
  glDisable(GL_TEXTURE_2D);
}

int RunCubeWebUILauncher(const std::string& gmodRoot, const std::string& xrJson) {
  if (!xrJson.empty())
    setenv("XR_RUNTIME_JSON", xrJson.c_str(), 1);

  // Ensure WiVRn active_runtime is readable
  fprintf(stderr, "[cube_webui] OpenXR WebUI reverse launcher\n");
  fprintf(stderr, "[cube_webui] GMOD=%s XR=%s\n", gmodRoot.c_str(),
          getenv("XR_RUNTIME_JSON") ? getenv("XR_RUNTIME_JSON") : "(default)");

  GlxCtx glx{};
  if (!MakeGlx(glx)) {
    Die("GLX context failed");
    return 1;
  }

  // OpenXR instance
  const char* exts[] = { XR_KHR_OPENGL_ENABLE_EXTENSION_NAME };
  XrInstanceCreateInfo ici{XR_TYPE_INSTANCE_CREATE_INFO};
  std::strncpy(ici.applicationInfo.applicationName, "CubeWebUILauncher", XR_MAX_APPLICATION_NAME_SIZE - 1);
  ici.applicationInfo.applicationVersion = 1;
  std::strncpy(ici.applicationInfo.engineName, "gVRMod", XR_MAX_ENGINE_NAME_SIZE - 1);
  ici.applicationInfo.engineVersion = 1;
  ici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
  ici.enabledExtensionCount = 1;
  ici.enabledExtensionNames = exts;

  XrInstance instance = XR_NULL_HANDLE;
  XrResult r = xrCreateInstance(&ici, &instance);
  if (XR_FAILED(r)) {
    Die("xrCreateInstance failed — is WiVRn/Monado running + headset connected?");
    DestroyGlx(glx);
    return 2;
  }

  XrSystemGetInfo sgi{XR_TYPE_SYSTEM_GET_INFO};
  sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  XrSystemId systemId = XR_NULL_SYSTEM_ID;
  r = xrGetSystem(instance, &sgi, &systemId);
  if (XR_FAILED(r)) {
    Die("xrGetSystem failed — no HMD / runtime");
    xrDestroyInstance(instance);
    DestroyGlx(glx);
    return 3;
  }

  PFN_xrGetOpenGLGraphicsRequirementsKHR pfnReq = nullptr;
  xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&pfnReq);
  if (pfnReq) {
    XrGraphicsRequirementsOpenGLKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    pfnReq(instance, systemId, &req);
  }

  XrGraphicsBindingOpenGLXlibKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
  binding.xDisplay = glx.dpy;
  binding.glxContext = glx.ctx;
  binding.glxDrawable = glx.win;
  // FBConfig optional on many runtimes
  binding.visualid = 0;
  binding.glxFBConfig = nullptr;

  XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
  sci.next = &binding;
  sci.systemId = systemId;
  XrSession session = XR_NULL_HANDLE;
  r = xrCreateSession(instance, &sci, &session);
  if (XR_FAILED(r)) {
    Die("xrCreateSession failed");
    xrDestroyInstance(instance);
    DestroyGlx(glx);
    return 4;
  }

  XrReferenceSpaceCreateInfo rsci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  rsci.poseInReferenceSpace.orientation.w = 1.f;
  XrSpace space = XR_NULL_HANDLE;
  xrCreateReferenceSpace(session, &rsci, &space);

  // Views / swapchains
  uint32_t viewCount = 0;
  xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
  std::vector<XrViewConfigurationView> viewConfigs(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                    viewCount, &viewCount, viewConfigs.data());

  struct Eye {
    XrSwapchain swap = XR_NULL_HANDLE;
    uint32_t w = 0, h = 0;
    std::vector<XrSwapchainImageOpenGLKHR> images;
  };
  Eye eyes[2]{};
  for (uint32_t i = 0; i < 2 && i < viewCount; ++i) {
    eyes[i].w = viewConfigs[i].recommendedImageRectWidth;
    eyes[i].h = viewConfigs[i].recommendedImageRectHeight;
    XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    sc.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    sc.format = 0x8058; // GL_RGBA8
    sc.sampleCount = 1;
    sc.width = eyes[i].w;
    sc.height = eyes[i].h;
    sc.faceCount = 1;
    sc.arraySize = 1;
    sc.mipCount = 1;
    r = xrCreateSwapchain(session, &sc, &eyes[i].swap);
    if (XR_FAILED(r)) {
      Die("xrCreateSwapchain failed");
      return 5;
    }
    uint32_t nImg = 0;
    xrEnumerateSwapchainImages(eyes[i].swap, 0, &nImg, nullptr);
    eyes[i].images.resize(nImg, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
    xrEnumerateSwapchainImages(eyes[i].swap, nImg, &nImg, (XrSwapchainImageBaseHeader*)eyes[i].images.data());
  }

  // Session begin
  XrSessionBeginInfo sbi{XR_TYPE_SESSION_BEGIN_INFO};
  sbi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  XrSessionState state = XR_SESSION_STATE_UNKNOWN;
  bool running = true;
  bool sessionRunning = false;

  WebUIState ui{};
  WebUI_Init(ui, gmodRoot);
  std::vector<unsigned char> panel(UI_W * UI_H * 4);
  WebUI_Rasterize(ui, panel.data());
  GLuint panelTex = MakeTex(UI_W, UI_H, panel.data());

  // Input: keyboard on host as fallback + simple timed demo
  // Stick/trigger via OpenXR actions would be phase 2; use X11 key events + auto-focus
  fprintf(stderr, "[cube_webui] Controls: keys 1/2 columns, arrows maps, Enter=START, Esc=quit\n");
  fprintf(stderr, "[cube_webui] Put on headset — panel is native OpenGL in OpenXR (not GMod)\n");

  auto pollKeys = [&]() {
    while (XPending(glx.dpy)) {
      XEvent ev;
      XNextEvent(glx.dpy, &ev);
    }
    // Also poll keyboard via XQueryKeymap is heavy; use simple stdin nonblock? skip
  };

  // Host keyboard: select + start without controllers for bring-up
  // We use a crude approach: every frame check for /tmp/cube_webui_cmd
  auto pollCmdFile = [&]() {
    FILE* f = fopen("/tmp/cube_webui_cmd", "r");
    if (!f) return;
    char buf[64] = {};
    if (fgets(buf, sizeof(buf), f)) {
      if (std::strncmp(buf, "start", 5) == 0) ui.wantStart = true;
      if (std::strncmp(buf, "quit", 4) == 0) ui.wantQuit = true;
      if (std::strncmp(buf, "addons", 6) == 0) {
        ui.page = WebUIPage::Addons;
        ui.status = "ADDON MANAGER";
      }
      if (std::strncmp(buf, "newgame", 7) == 0) {
        ui.page = WebUIPage::NewGame;
        ui.status = "NEW GAME";
      }
      if (std::strncmp(buf, "up", 2) == 0) WebUI_Input(ui, 0, -1, false, false);
      if (std::strncmp(buf, "down", 4) == 0) WebUI_Input(ui, 0, 1, false, false);
      if (std::strncmp(buf, "left", 4) == 0) WebUI_Input(ui, -1, 0, false, false);
      if (std::strncmp(buf, "right", 5) == 0) WebUI_Input(ui, 1, 0, false, false);
      if (std::strncmp(buf, "click", 5) == 0) WebUI_Input(ui, 0, 0, true, false);
      if (std::strncmp(buf, "toggle", 6) == 0) WebUI_Input(ui, 0, 0, true, false);
    }
    fclose(f);
    unlink("/tmp/cube_webui_cmd");
  };

  int frame = 0;
  while (running) {
    // Events
    XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(instance, &ev) == XR_SUCCESS) {
      if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
        auto* ssc = (XrEventDataSessionStateChanged*)&ev;
        state = ssc->state;
        if (state == XR_SESSION_STATE_READY && !sessionRunning) {
          xrBeginSession(session, &sbi);
          sessionRunning = true;
          fprintf(stderr, "[cube_webui] session RUNNING\n");
        }
        if (state == XR_SESSION_STATE_STOPPING) {
          if (sessionRunning) {
            xrEndSession(session);
            sessionRunning = false;
          }
        }
        if (state == XR_SESSION_STATE_EXITING || state == XR_SESSION_STATE_LOSS_PENDING)
          running = false;
      }
      ev = {XR_TYPE_EVENT_DATA_BUFFER};
    }

    pollKeys();
    pollCmdFile();

    if (ui.wantQuit) break;

    if (ui.wantStart) {
      LaunchRequest lr;
      lr.gmodRoot = gmodRoot;
      lr.map = WebUI_SelectedMap(ui);
      lr.maxPlayers = WebUI_MaxPlayers(ui);
      lr.hostname = ui.hostname;
      lr.svLan = ui.svLan;
      lr.p2p = ui.p2p;
      lr.p2pFriends = ui.p2pFriends;
      lr.gamemode = ui.gamemode;
      if (const char* xr = getenv("XR_RUNTIME_JSON")) lr.xrRuntimeJson = xr;
      std::string err;
      int rc = SpawnGModFromWebUI(lr, err);
      fprintf(stderr, "[cube_webui] StartGame map=%s rc=%d %s\n", lr.map.c_str(), rc, err.c_str());
      ui.status = rc == 0 ? "SPAWNED GMOD — PUT ON HMD" : ("SPAWN FAIL: " + err);
      ui.wantStart = false;
      // Exit native launcher so OpenXR runtime is free for GMod module
      if (rc == 0) {
        sleep(1);
        break;
      }
    }

    if (!sessionRunning) {
      usleep(10000);
      continue;
    }

    XrFrameWaitInfo fwi{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState fs{XR_TYPE_FRAME_STATE};
    xrWaitFrame(session, &fwi, &fs);
    XrFrameBeginInfo fbi{XR_TYPE_FRAME_BEGIN_INFO};
    xrBeginFrame(session, &fbi);

    std::vector<XrCompositionLayerProjectionView> projViews;
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space = space;

    if (fs.shouldRender) {
      // Update UI texture occasionally
      if ((frame++ % 2) == 0) {
        WebUI_Rasterize(ui, panel.data());
        glBindTexture(GL_TEXTURE_2D, panelTex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, UI_W, UI_H, GL_RGBA, GL_UNSIGNED_BYTE, panel.data());
      }

      XrViewLocateInfo vli{XR_TYPE_VIEW_LOCATE_INFO};
      vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
      vli.displayTime = fs.predictedDisplayTime;
      vli.space = space;
      XrViewState vs{XR_TYPE_VIEW_STATE};
      uint32_t vc = 0;
      XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
      xrLocateViews(session, &vli, &vs, 2, &vc, views);

      projViews.resize(2);
      for (int eye = 0; eye < 2; ++eye) {
        XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t idx = 0;
        xrAcquireSwapchainImage(eyes[eye].swap, &ai, &idx);
        XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wi.timeout = XR_INFINITE_DURATION;
        xrWaitSwapchainImage(eyes[eye].swap, &wi);

        GLuint color = eyes[eye].images[idx].image;
        LoadFBO();
        static GLuint fbo = 0;
        if (!fbo && glGenFramebuffers_) glGenFramebuffers_(1, &fbo);
        if (glBindFramebuffer_ && glFramebufferTexture2D_ && fbo) {
          glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
          glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        }
        glViewport(0, 0, eyes[eye].w, eyes[eye].h);
        glClearColor(0.04f, 0.02f, 0.03f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float l = tanf(views[eye].fov.angleLeft);
        float rgt = tanf(views[eye].fov.angleRight);
        float u = tanf(views[eye].fov.angleUp);
        float d = tanf(views[eye].fov.angleDown);
        glFrustum(l * 0.1f, rgt * 0.1f, d * 0.1f, u * 0.1f, 0.1f, 100.f);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glBindTexture(GL_TEXTURE_2D, panelTex);
        glColor3f(1, 1, 1);
        DrawTexturedQuad(-1.15f);

        if (glBindFramebuffer_) glBindFramebuffer_(GL_FRAMEBUFFER, 0);

        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xrReleaseSwapchainImage(eyes[eye].swap, &ri);

        projViews[eye] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        projViews[eye].pose = views[eye].pose;
        projViews[eye].fov = views[eye].fov;
        projViews[eye].subImage.swapchain = eyes[eye].swap;
        projViews[eye].subImage.imageRect.offset = {0, 0};
        projViews[eye].subImage.imageRect.extent = {(int32_t)eyes[eye].w, (int32_t)eyes[eye].h};
      }
      layer.viewCount = 2;
      layer.views = projViews.data();
    }

    XrFrameEndInfo fei{XR_TYPE_FRAME_END_INFO};
    fei.displayTime = fs.predictedDisplayTime;
    fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    XrCompositionLayerBaseHeader* layers[] = {(XrCompositionLayerBaseHeader*)&layer};
    if (fs.shouldRender && !projViews.empty()) {
      fei.layerCount = 1;
      fei.layers = layers;
    } else {
      fei.layerCount = 0;
      fei.layers = nullptr;
    }
    xrEndFrame(session, &fei);
  }

  // Teardown
  for (int i = 0; i < 2; ++i)
    if (eyes[i].swap) xrDestroySwapchain(eyes[i].swap);
  if (space) xrDestroySpace(space);
  if (session) xrDestroySession(session);
  if (instance) xrDestroyInstance(instance);
  if (panelTex) glDeleteTextures(1, &panelTex);
  DestroyGlx(glx);
  fprintf(stderr, "[cube_webui] exit\n");
  return 0;
}
