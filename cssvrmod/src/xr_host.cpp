#include "xr_host.hpp"
#include "cssvrmod/calib.hpp"
#include "cssvrmod/input.hpp"
#include "openxr_paths.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

#define GL_GLEXT_PROTOTYPES 1

#define XR_NO_PROTOTYPES
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_XLIB
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

namespace cssvr {
namespace {

#define XR_FN(name) PFN_##name name = nullptr
XR_FN(xrGetInstanceProcAddr);
XR_FN(xrEnumerateInstanceExtensionProperties);
XR_FN(xrCreateInstance);
XR_FN(xrDestroyInstance);
XR_FN(xrGetSystem);
XR_FN(xrGetSystemProperties);
XR_FN(xrCreateSession);
XR_FN(xrDestroySession);
XR_FN(xrBeginSession);
XR_FN(xrEndSession);
XR_FN(xrPollEvent);
XR_FN(xrWaitFrame);
XR_FN(xrBeginFrame);
XR_FN(xrEndFrame);
XR_FN(xrLocateViews);
XR_FN(xrEnumerateViewConfigurationViews);
XR_FN(xrCreateReferenceSpace);
XR_FN(xrDestroySpace);
XR_FN(xrCreateSwapchain);
XR_FN(xrDestroySwapchain);
XR_FN(xrEnumerateSwapchainFormats);
XR_FN(xrEnumerateSwapchainImages);
XR_FN(xrAcquireSwapchainImage);
XR_FN(xrWaitSwapchainImage);
XR_FN(xrReleaseSwapchainImage);
XR_FN(xrCreateActionSet);
XR_FN(xrCreateAction);
XR_FN(xrStringToPath);
XR_FN(xrSuggestInteractionProfileBindings);
XR_FN(xrAttachSessionActionSets);
XR_FN(xrSyncActions);
XR_FN(xrGetActionStateBoolean);
XR_FN(xrGetActionStateFloat);
XR_FN(xrGetActionStateVector2f);
XR_FN(xrCreateActionSpace);
XR_FN(xrLocateSpace);
XR_FN(xrGetOpenGLGraphicsRequirementsKHR);
#undef XR_FN

void* g_loader = nullptr;
XrInstance g_inst = XR_NULL_HANDLE;
XrSystemId g_sys = XR_NULL_SYSTEM_ID;
XrSession g_sess = XR_NULL_HANDLE;
XrSpace g_stage = XR_NULL_HANDLE;
XrSpace g_view = XR_NULL_HANDLE;
XrSwapchain g_sc[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
uint32_t g_scW = 0, g_scH = 0;
struct ScImg {
  uint32_t n = 0;
  XrSwapchainImageOpenGLKHR img[8]{};
};
ScImg g_img[2];
XrSessionState g_state = XR_SESSION_STATE_UNKNOWN;
bool g_running = false;
bool g_begun = false;
XrFrameState g_fs{};
XrHostInfo g_info{};

XrActionSet g_set = XR_NULL_HANDLE;
XrAction g_pose = XR_NULL_HANDLE, g_trig = XR_NULL_HANDLE, g_grab = XR_NULL_HANDLE;
XrAction g_stick = XR_NULL_HANDLE, g_menu = XR_NULL_HANDLE, g_abxy = XR_NULL_HANDLE;
XrPath g_hand[2]{};
XrSpace g_aim[2]{};

void Log(const char* fmt, ...) {
  FILE* f = std::fopen("/tmp/cssvrmod.log", "a");
  if (!f) return;
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(f, fmt, ap);
  va_end(ap);
  std::fputc('\n', f);
  std::fclose(f);
}

bool LoadFn(XrInstance inst, const char* name, PFN_xrVoidFunction* out) {
  return xrGetInstanceProcAddr &&
         xrGetInstanceProcAddr(inst, name, out) == XR_SUCCESS && *out;
}

#define LOAD(inst, fn) LoadFn(inst, #fn, reinterpret_cast<PFN_xrVoidFunction*>(&fn))

bool LoadLoader() {
  if (g_loader) return true;
  g_loader = dlopen("libopenxr_loader.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!g_loader) g_loader = dlopen("libopenxr_loader.so", RTLD_NOW | RTLD_LOCAL);
  if (!g_loader) {
    g_info.reason = "no_loader";
    return false;
  }
  xrGetInstanceProcAddr =
      reinterpret_cast<PFN_xrGetInstanceProcAddr>(dlsym(g_loader, "xrGetInstanceProcAddr"));
  if (!xrGetInstanceProcAddr) {
    g_info.reason = "no_getproc";
    return false;
  }
  LOAD(XR_NULL_HANDLE, xrEnumerateInstanceExtensionProperties);
  LOAD(XR_NULL_HANDLE, xrCreateInstance);
  g_info.loader = true;
  return xrCreateInstance != nullptr;
}

bool CreateInst() {
  const char* exts[] = {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
  XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
  std::strncpy(ci.applicationInfo.applicationName, "CSSVRMod", XR_MAX_APPLICATION_NAME_SIZE);
  ci.applicationInfo.applicationVersion = 1;
  std::strncpy(ci.applicationInfo.engineName, "cssvrmod", XR_MAX_ENGINE_NAME_SIZE);
  ci.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
  ci.enabledExtensionCount = 1;
  ci.enabledExtensionNames = exts;
  if (xrCreateInstance(&ci, &g_inst) != XR_SUCCESS) {
    g_info.reason = "create_instance";
    return false;
  }
  LOAD(g_inst, xrDestroyInstance);
  LOAD(g_inst, xrGetSystem);
  LOAD(g_inst, xrGetSystemProperties);
  LOAD(g_inst, xrCreateSession);
  LOAD(g_inst, xrDestroySession);
  LOAD(g_inst, xrBeginSession);
  LOAD(g_inst, xrEndSession);
  LOAD(g_inst, xrPollEvent);
  LOAD(g_inst, xrWaitFrame);
  LOAD(g_inst, xrBeginFrame);
  LOAD(g_inst, xrEndFrame);
  LOAD(g_inst, xrLocateViews);
  LOAD(g_inst, xrEnumerateViewConfigurationViews);
  LOAD(g_inst, xrCreateReferenceSpace);
  LOAD(g_inst, xrDestroySpace);
  LOAD(g_inst, xrCreateSwapchain);
  LOAD(g_inst, xrDestroySwapchain);
  LOAD(g_inst, xrEnumerateSwapchainFormats);
  LOAD(g_inst, xrEnumerateSwapchainImages);
  LOAD(g_inst, xrAcquireSwapchainImage);
  LOAD(g_inst, xrWaitSwapchainImage);
  LOAD(g_inst, xrReleaseSwapchainImage);
  LOAD(g_inst, xrCreateActionSet);
  LOAD(g_inst, xrCreateAction);
  LOAD(g_inst, xrStringToPath);
  LOAD(g_inst, xrSuggestInteractionProfileBindings);
  LOAD(g_inst, xrAttachSessionActionSets);
  LOAD(g_inst, xrSyncActions);
  LOAD(g_inst, xrGetActionStateBoolean);
  LOAD(g_inst, xrGetActionStateFloat);
  LOAD(g_inst, xrGetActionStateVector2f);
  LOAD(g_inst, xrCreateActionSpace);
  LOAD(g_inst, xrLocateSpace);
  LOAD(g_inst, xrGetOpenGLGraphicsRequirementsKHR);
  g_info.instance = true;
  return true;
}

void PollEvents() {
  if (!xrPollEvent || !g_inst) return;
  XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
  while (xrPollEvent(g_inst, &ev) == XR_SUCCESS) {
    if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
      auto* s = reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
      g_state = s->state;
      if (g_state == XR_SESSION_STATE_READY && xrBeginSession) {
        XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
        bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        if (xrBeginSession(g_sess, &bi) == XR_SUCCESS) g_running = true;
      }
      if (g_state == XR_SESSION_STATE_STOPPING && xrEndSession) {
        xrEndSession(g_sess);
        g_running = false;
      }
    }
    ev = XrEventDataBuffer{XR_TYPE_EVENT_DATA_BUFFER};
  }
}

bool CreateSess() {
  XrSystemGetInfo gi{XR_TYPE_SYSTEM_GET_INFO};
  gi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  XrResult sys_rc = xrGetSystem(g_inst, &gi, &g_sys);
  if (sys_rc != XR_SUCCESS) {
    g_info.reason = "no_hmd";
    Log("cssvr xrGetSystem %d", (int)sys_rc);
    return false;
  }
  if (xrGetOpenGLGraphicsRequirementsKHR) {
    XrGraphicsRequirementsOpenGLKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    xrGetOpenGLGraphicsRequirementsKHR(g_inst, g_sys, &req);
    Log("cssvr xr GL req min=%llu max=%llu", (unsigned long long)req.minApiVersionSupported,
        (unsigned long long)req.maxApiVersionSupported);
  }
  Display* dpy = glXGetCurrentDisplay();
  GLXContext ctx = glXGetCurrentContext();
  GLXDrawable draw = glXGetCurrentDrawable();
  if (!dpy || !ctx || !draw) {
    g_info.reason = "no_glx";
    return false;
  }
  int fbConfigId = 0;
  glXQueryContext(dpy, ctx, GLX_FBCONFIG_ID, &fbConfigId);
  int attribs[] = {GLX_FBCONFIG_ID, fbConfigId, None};
  int ncfg = 0;
  GLXFBConfig* cfgs = glXChooseFBConfig(dpy, DefaultScreen(dpy), attribs, &ncfg);
  XVisualInfo* vi = nullptr;
  GLXFBConfig fb = nullptr;
  if (cfgs && ncfg > 0) {
    fb = cfgs[0];
    vi = glXGetVisualFromFBConfig(dpy, fb);
  }
  XrGraphicsBindingOpenGLXlibKHR bind{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
  bind.xDisplay = dpy;
  bind.visualid = vi ? (uint32_t)vi->visualid : 0;
  bind.glxFBConfig = fb;
  bind.glxDrawable = draw;
  bind.glxContext = ctx;
  if (vi) XFree(vi);
  if (cfgs) XFree(cfgs);
  Log("cssvr xr bind dpy=%p ctx=%p draw=%lu vis=%u fb=%p", (void*)dpy, (void*)ctx,
      (unsigned long)draw, bind.visualid, (void*)fb);
  XrSessionCreateInfo si{XR_TYPE_SESSION_CREATE_INFO};
  si.next = &bind;
  si.systemId = g_sys;
  XrResult sess_rc = xrCreateSession(g_inst, &si, &g_sess);
  if (sess_rc != XR_SUCCESS) {
    g_info.reason = "create_session";
    Log("cssvr xrCreateSession failed %d", (int)sess_rc);
    return false;
  }
  (void)vi;
  XrReferenceSpaceCreateInfo rci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  rci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
  rci.poseInReferenceSpace.orientation.w = 1.f;
  if (xrCreateReferenceSpace(g_sess, &rci, &g_stage) != XR_SUCCESS) {
    rci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    if (xrCreateReferenceSpace(g_sess, &rci, &g_stage) != XR_SUCCESS) {
      g_info.reason = "no_space";
      return false;
    }
  }
  rci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  rci.poseInReferenceSpace = {};
  rci.poseInReferenceSpace.orientation.w = 1.f;
  if (xrCreateReferenceSpace(g_sess, &rci, &g_view) != XR_SUCCESS) {
    Log("cssvr xr VIEW space failed — falling back to stage");
    g_view = g_stage;
  }
  // Dual eye swapchains at HMD recommended size. Same CSS frame is blitted
  // into both with a slight horizontal offset (gmod synthetic stereo).
  g_scW = 1280;
  g_scH = 720;
  if (xrEnumerateViewConfigurationViews) {
    uint32_t nv = 0;
    xrEnumerateViewConfigurationViews(g_inst, g_sys, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
                                      &nv, nullptr);
    XrViewConfigurationView vc[2]{};
    vc[0].type = vc[1].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    if (nv > 2) nv = 2;
    if (nv >= 1 &&
        xrEnumerateViewConfigurationViews(g_inst, g_sys, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                          nv, &nv, vc) == XR_SUCCESS &&
        vc[0].recommendedImageRectWidth > 0) {
      g_scW = vc[0].recommendedImageRectWidth;
      g_scH = vc[0].recommendedImageRectHeight;
    }
  }
  g_info.width = g_scW;
  g_info.height = g_scH;

  int64_t fmt = GL_SRGB8_ALPHA8;
  for (int eye = 0; eye < 2; ++eye) {
    XrSwapchainCreateInfo sci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    sci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    sci.format = fmt;
    sci.sampleCount = 1;
    sci.width = g_scW;
    sci.height = g_scH;
    sci.faceCount = 1;
    sci.arraySize = 1;
    sci.mipCount = 1;
    if (xrCreateSwapchain(g_sess, &sci, &g_sc[eye]) != XR_SUCCESS) {
      sci.format = GL_RGBA8;
      if (xrCreateSwapchain(g_sess, &sci, &g_sc[eye]) != XR_SUCCESS) {
        g_info.reason = "swapchain";
        return false;
      }
    }
    xrEnumerateSwapchainImages(g_sc[eye], 0, &g_img[eye].n, nullptr);
    if (g_img[eye].n > 8) g_img[eye].n = 8;
    for (uint32_t i = 0; i < g_img[eye].n; ++i)
      g_img[eye].img[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
    xrEnumerateSwapchainImages(g_sc[eye], g_img[eye].n, &g_img[eye].n,
                               reinterpret_cast<XrSwapchainImageBaseHeader*>(g_img[eye].img));
  }
  g_info.swapchain = true;
  g_info.session = true;
  g_info.reason = "session_ok";
  return true;
}

bool SetupInput() {
  if (!xrCreateActionSet) return false;
  XrActionSetCreateInfo asci{XR_TYPE_ACTION_SET_CREATE_INFO};
  std::strncpy(asci.actionSetName, "cssvr", XR_MAX_ACTION_SET_NAME_SIZE);
  std::strncpy(asci.localizedActionSetName, "CSSVR", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
  if (xrCreateActionSet(g_inst, &asci, &g_set) != XR_SUCCESS) return false;
  xrStringToPath(g_inst, "/user/hand/left", &g_hand[0]);
  xrStringToPath(g_inst, "/user/hand/right", &g_hand[1]);
  auto mk = [&](XrActionType t, const char* n, XrAction* a) {
    XrActionCreateInfo ai{XR_TYPE_ACTION_CREATE_INFO};
    ai.actionType = t;
    std::strncpy(ai.actionName, n, XR_MAX_ACTION_NAME_SIZE);
    std::strncpy(ai.localizedActionName, n, XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
    ai.countSubactionPaths = 2;
    ai.subactionPaths = g_hand;
    return xrCreateAction(g_set, &ai, a) == XR_SUCCESS;
  };
  mk(XR_ACTION_TYPE_POSE_INPUT, "aim", &g_pose);
  mk(XR_ACTION_TYPE_FLOAT_INPUT, "trigger", &g_trig);
  mk(XR_ACTION_TYPE_FLOAT_INPUT, "grab", &g_grab);
  mk(XR_ACTION_TYPE_VECTOR2F_INPUT, "stick", &g_stick);
  mk(XR_ACTION_TYPE_BOOLEAN_INPUT, "menu", &g_menu);
  mk(XR_ACTION_TYPE_BOOLEAN_INPUT, "abxy", &g_abxy);

  XrPath prof{};
  xrStringToPath(g_inst, cube_xr::kProfileOculusTouch, &prof);
  XrActionSuggestedBinding binds[16];
  int nb = 0;
  auto bind = [&](XrAction a, const char* p) {
    XrPath path{};
    xrStringToPath(g_inst, p, &path);
    binds[nb++] = {a, path};
  };
  bind(g_pose, cube_xr::path::leftAimPose);
  bind(g_pose, cube_xr::path::rightAimPose);
  bind(g_trig, cube_xr::path::leftTriggerValue);
  bind(g_trig, cube_xr::path::rightTriggerValue);
  bind(g_grab, cube_xr::path::leftSqueezeValue);
  bind(g_grab, cube_xr::path::rightSqueezeValue);
  bind(g_stick, cube_xr::path::leftThumbstick);
  bind(g_stick, cube_xr::path::rightThumbstick);
  bind(g_menu, cube_xr::path::leftMenuClick);
  bind(g_abxy, cube_xr::path::rightAClick);
  XrInteractionProfileSuggestedBinding sug{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
  sug.interactionProfile = prof;
  sug.countSuggestedBindings = static_cast<uint32_t>(nb);
  sug.suggestedBindings = binds;
  xrSuggestInteractionProfileBindings(g_inst, &sug);

  XrSessionActionSetsAttachInfo ai{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  ai.countActionSets = 1;
  ai.actionSets = &g_set;
  xrAttachSessionActionSets(g_sess, &ai);
  for (int h = 0; h < 2; ++h) {
    XrActionSpaceCreateInfo as{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    as.action = g_pose;
    as.subactionPath = g_hand[h];
    as.poseInActionSpace.orientation.w = 1.f;
    xrCreateActionSpace(g_sess, &as, &g_aim[h]);
  }
  return true;
}

void BlitToSwapchain(unsigned int src, int srcW, int srcH, int eye, bool vflip) {
  uint32_t idx = 0;
  XrSwapchainImageAcquireInfo ac{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  if (xrAcquireSwapchainImage(g_sc[eye], &ac, &idx) != XR_SUCCESS) return;
  XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  wi.timeout = XR_INFINITE_DURATION;
  xrWaitSwapchainImage(g_sc[eye], &wi);
  GLuint dst = (idx < g_img[eye].n) ? g_img[eye].img[idx].image : 0;
  if (dst && src) {
    GLint prevFbo = 0, prevRead = 0, prevDraw = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);
    GLuint rf = 0, df = 0;
    glGenFramebuffers(1, &rf);
    glGenFramebuffers(1, &df);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, rf);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, src, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, df);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst, 0);
    const EyeBlit crop = CalibEye(CalibLive(), eye);
    const GLint sx0 = (GLint)(crop.u0 * (float)srcW);
    const GLint sx1 = (GLint)(crop.u1 * (float)srcW);
    GLint sy0 = (GLint)(crop.v0 * (float)srcH);
    GLint sy1 = (GLint)(crop.v1 * (float)srcH);
    if (vflip) {
      sy0 = srcH - (GLint)(crop.v0 * (float)srcH);
      sy1 = srcH - (GLint)(crop.v1 * (float)srcH);
    }
    glBlitFramebuffer(sx0, sy0, sx1, sy1, 0, 0, (GLint)g_scW, (GLint)g_scH, GL_COLOR_BUFFER_BIT,
                      GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, prevRead);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDraw);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
    glDeleteFramebuffers(1, &rf);
    glDeleteFramebuffers(1, &df);
  }
  XrSwapchainImageReleaseInfo rel{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  xrReleaseSwapchainImage(g_sc[eye], &rel);
}

} // namespace

bool XrHostInit() {
  if (g_info.session) return true;
  Log("cssvr xr init begin");
  if (!LoadLoader()) return false;
  if (!g_inst && !CreateInst()) return false;
  if (!g_sess && !CreateSess()) return false;
  SetupInput();
  {
    const Calib c = CalibLive();
    Log("cssvr xr init %s stereo-offset %ux%u calib=%s eye=%.2f", g_info.reason, g_info.width,
        g_info.height, CalibPath(), c.eyescale);
  }
  return g_info.session;
}

void XrHostShutdown() {
  // Orderly: no mid-frame destroy.
  if (g_begun && xrEndFrame && g_sess) {
    XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};
    ei.displayTime = g_fs.predictedDisplayTime ? g_fs.predictedDisplayTime : 1;
    ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    xrEndFrame(g_sess, &ei);
    g_begun = false;
  }
  if (g_running && xrEndSession && g_sess) xrEndSession(g_sess);
  g_running = false;
  if (g_inst && xrDestroyInstance) xrDestroyInstance(g_inst);
  g_inst = XR_NULL_HANDLE;
  g_sess = XR_NULL_HANDLE;
  g_info = XrHostInfo{};
  g_info.reason = "shutdown";
}

bool XrHostBeginFrame() {
  PollEvents();
  if (!g_running || !xrWaitFrame) return false;
  g_fs = XrFrameState{XR_TYPE_FRAME_STATE};
  XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};
  if (xrWaitFrame(g_sess, &wi, &g_fs) != XR_SUCCESS) return false;
  XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};
  if (xrBeginFrame(g_sess, &bi) != XR_SUCCESS) return false;
  g_begun = true;
  return g_fs.shouldRender;
}

bool XrHostSubmitBackbuffer(unsigned int gl_tex, int src_w, int src_h, bool vflip) {
  if (!g_begun || !g_fs.shouldRender) return false;
  if (!g_sc[0] || !g_sc[1]) return false;
  BlitToSwapchain(gl_tex, src_w, src_h, 0, vflip);
  BlitToSwapchain(gl_tex, src_w, src_h, 1, vflip);

  // Map the same CSS frame onto each eye (projection + HMD FOV). Slight VIEW-space
  // IPD only — same angles, same frame, gmod synthetic stereo. Full world-space
  // eye poses on this 2D present is what made two squares float apart.
  XrView located[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
  uint32_t nloc = 0;
  if (xrLocateViews && g_view) {
    XrViewLocateInfo vli{XR_TYPE_VIEW_LOCATE_INFO};
    vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    vli.displayTime = g_fs.predictedDisplayTime;
    vli.space = g_view;
    XrViewState vst{XR_TYPE_VIEW_STATE};
    xrLocateViews(g_sess, &vli, &vst, 2, &nloc, located);
  }
  XrFovf fallback{-0.85f, 0.85f, 0.85f, -0.85f};

  const Calib cal = CalibLive();
  XrCompositionLayerProjectionView pv[2]{};
  for (int e = 0; e < 2; ++e) {
    const EyeBlit crop = CalibEye(cal, e);
    pv[e].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
    pv[e].pose.orientation.w = 1.f;
    pv[e].pose.position.x = crop.pose_x;
    pv[e].fov = (nloc >= 2) ? located[e].fov : fallback;
    pv[e].subImage.swapchain = g_sc[e];
    pv[e].subImage.imageRect.offset = {0, 0};
    pv[e].subImage.imageRect.extent = {(int32_t)g_scW, (int32_t)g_scH};
  }
  XrCompositionLayerProjection proj{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  proj.space = g_view ? g_view : g_stage;
  proj.viewCount = 2;
  proj.views = pv;
  const XrCompositionLayerBaseHeader* layers[] = {
      reinterpret_cast<const XrCompositionLayerBaseHeader*>(&proj)};
  XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};
  ei.displayTime = g_fs.predictedDisplayTime;
  ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  ei.layerCount = 1;
  ei.layers = layers;
  const XrResult rc = xrEndFrame(g_sess, &ei);
  g_begun = false;
  static int ends = 0;
  ends++;
  if (ends <= 3 || (ends % 300) == 0)
    Log("cssvr xr endframe #%d rc=%d stereo-offset %ux%u eye=%.2f h=%.2f v=%.2f sc=%.2f", ends,
        (int)rc, g_scW, g_scH, cal.eyescale, cal.hoffset, cal.voffset, cal.scalefactor);
  return rc == XR_SUCCESS;
}

void XrHostEndFrame() {
  if (!g_begun) return;
  XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};
  ei.displayTime = g_fs.predictedDisplayTime ? g_fs.predictedDisplayTime : 1;
  ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  xrEndFrame(g_sess, &ei);
  g_begun = false;
}

bool XrHostPollInput(XrSample* out) {
  if (!out || !g_sess || !g_set || !g_running) return false;
  *out = XrSample{};
  XrActiveActionSet aas{g_set, XR_NULL_PATH};
  XrActionsSyncInfo si{XR_TYPE_ACTIONS_SYNC_INFO};
  si.countActiveActionSets = 1;
  si.activeActionSets = &aas;
  if (xrSyncActions(g_sess, &si) != XR_SUCCESS) return false;

  auto fval = [&](XrAction a, int hand) {
    XrActionStateFloat st{XR_TYPE_ACTION_STATE_FLOAT};
    XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = a;
    gi.subactionPath = g_hand[hand];
    xrGetActionStateFloat(g_sess, &gi, &st);
    return st.isActive ? st.currentState : 0.f;
  };
  auto bval = [&](XrAction a, int hand) {
    XrActionStateBoolean st{XR_TYPE_ACTION_STATE_BOOLEAN};
    XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = a;
    gi.subactionPath = g_hand[hand];
    xrGetActionStateBoolean(g_sess, &gi, &st);
    return st.isActive && st.currentState;
  };
  auto stick = [&](int hand, float* x, float* y) {
    XrActionStateVector2f st{XR_TYPE_ACTION_STATE_VECTOR2F};
    XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = g_stick;
    gi.subactionPath = g_hand[hand];
    xrGetActionStateVector2f(g_sess, &gi, &st);
    if (st.isActive) {
      *x = st.currentState.x;
      *y = st.currentState.y;
    }
  };
  auto pose = [&](int hand, Pose* p) {
    if (!g_aim[hand] || !g_fs.predictedDisplayTime) return;
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
    xrLocateSpace(g_aim[hand], g_stage, g_fs.predictedDisplayTime, &loc);
    if (!(loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) return;
    p->pos = XrPosToSource(loc.pose.position.x, loc.pose.position.y, loc.pose.position.z);
    p->ang = QuatToAng(loc.pose.orientation.x, loc.pose.orientation.y, loc.pose.orientation.z,
                       loc.pose.orientation.w);
    p->valid = true;
  };
  out->trigger_l = fval(g_trig, 0);
  out->trigger_r = fval(g_trig, 1);
  out->grab_l = fval(g_grab, 0);
  out->grab_r = fval(g_grab, 1);
  stick(0, &out->stick_lx, &out->stick_ly);
  stick(1, &out->stick_rx, &out->stick_ry);
  out->menu = bval(g_menu, 0) || bval(g_menu, 1);
  out->a_click = bval(g_abxy, 1);
  pose(0, &out->left);
  pose(1, &out->right);
  // HMD ≈ average of eyes via view space: use right-hand yaw if no view space.
  if (out->right.valid) {
    out->hmd = out->right;
    out->hmd.pos.z += 8.f; // crude head above right hand if only controllers
  }
  if (out->left.valid && out->right.valid) {
    out->hmd.pos = (out->left.pos + out->right.pos) * 0.5f;
    out->hmd.pos.z += 12.f;
    out->hmd.ang.y = out->right.ang.y;
    out->hmd.valid = true;
  }
  return out->left.valid || out->right.valid || out->trigger_r > 0.f;
}

const XrHostInfo& XrHostStatus() { return g_info; }

namespace {
Display* g_xr_dpy = nullptr;
GLXContext g_xr_ctx = nullptr;
GLXPbuffer g_xr_pbuf = 0;
GLuint g_upload = 0;
int g_upload_w = 0, g_upload_h = 0;
int g_upload_bgra = -1;
bool g_xr_fail_logged = false;

Window g_xr_win = 0;

bool EnsureXrGl() {
  if (glXGetCurrentContext() && glXGetCurrentDrawable()) return true;
  if (!g_xr_dpy) g_xr_dpy = XOpenDisplay(nullptr);
  if (!g_xr_dpy) {
    g_info.reason = "no_x_display";
    return false;
  }
  int dummy = 0;
  if (!glXQueryExtension(g_xr_dpy, &dummy, &dummy)) {
    g_info.reason = "no_glx";
    return false;
  }
  int fb_attrs[] = {GLX_X_RENDERABLE,
                    True,
                    GLX_DRAWABLE_TYPE,
                    GLX_WINDOW_BIT,
                    GLX_RENDER_TYPE,
                    GLX_RGBA_BIT,
                    GLX_RED_SIZE,
                    8,
                    GLX_GREEN_SIZE,
                    8,
                    GLX_BLUE_SIZE,
                    8,
                    GLX_DOUBLEBUFFER,
                    True,
                    None};
  int ncfg = 0;
  int screen = DefaultScreen(g_xr_dpy);
  GLXFBConfig* cfgs = glXChooseFBConfig(g_xr_dpy, screen, fb_attrs, &ncfg);
  if (!cfgs || ncfg < 1) {
    g_info.reason = "no_fbconfig";
    return false;
  }
  GLXFBConfig cfg = cfgs[0];
  XVisualInfo* vi = glXGetVisualFromFBConfig(g_xr_dpy, cfg);
  XFree(cfgs);
  if (!vi) {
    g_info.reason = "no_visual";
    return false;
  }
  Window root = RootWindow(g_xr_dpy, vi->screen);
  XSetWindowAttributes swa{};
  swa.colormap = XCreateColormap(g_xr_dpy, root, vi->visual, AllocNone);
  swa.override_redirect = True;
  g_xr_win = XCreateWindow(g_xr_dpy, root, 0, 0, 256, 256, 0, vi->depth, InputOutput, vi->visual,
                           CWColormap | CWOverrideRedirect, &swa);
  XMapWindow(g_xr_dpy, g_xr_win);
  XFlush(g_xr_dpy);
  g_xr_ctx = glXCreateNewContext(g_xr_dpy, cfg, GLX_RGBA_TYPE, nullptr, True);
  XFree(vi);
  if (!g_xr_ctx || !g_xr_win ||
      !glXMakeContextCurrent(g_xr_dpy, g_xr_win, g_xr_win, g_xr_ctx)) {
    g_info.reason = "glx_make_current";
    return false;
  }
  Log("cssvr xr hidden GLX window ctx ok");
  return true;
}
} // namespace

bool XrHostSubmitPixels(const unsigned char* px, int w, int h, bool bgra) {
  if (!px || w < 2 || h < 2) return false;
  if (!EnsureXrGl()) {
    if (!g_xr_fail_logged) {
      Log("cssvr xr gl fail %s", g_info.reason);
      g_xr_fail_logged = true;
    }
    return false;
  }
  static int init_fails = 0;
  if (!g_info.session) {
    if (init_fails > 3) return false; // do not hammer create_session
    if (!XrHostInit()) {
      init_fails++;
      Log("cssvr xr init fail %s (n=%d)", g_info.reason, init_fails);
      return false;
    }
  }
  const GLenum ext = bgra ? GL_BGRA : GL_RGBA;
  if (!g_upload || g_upload_w != w || g_upload_h != h || g_upload_bgra != (int)bgra) {
    if (g_upload) glDeleteTextures(1, &g_upload);
    glGenTextures(1, &g_upload);
    glBindTexture(GL_TEXTURE_2D, g_upload);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, ext, GL_UNSIGNED_BYTE, nullptr);
    g_upload_w = w;
    g_upload_h = h;
    g_upload_bgra = bgra ? 1 : 0;
  }
  glBindTexture(GL_TEXTURE_2D, g_upload);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, ext, GL_UNSIGNED_BYTE, px);
  if (!XrHostBeginFrame()) {
    XrHostEndFrame();
    return false;
  }
  // Vulkan copy is top-left; GL/XR blit wants a flip.
  return XrHostSubmitBackbuffer(g_upload, w, h, true);
}

bool XrHostSubmitRgba(const unsigned char* rgba, int w, int h) {
  return XrHostSubmitPixels(rgba, w, h, false);
}

} // namespace cssvr
