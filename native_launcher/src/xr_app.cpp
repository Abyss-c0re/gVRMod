// Cube WebUI OpenXR host — world-locked panel + controller laser + trigger
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
#include <string>
#include <vector>
#include <unistd.h>

// FBO
static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers_ = nullptr;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_ = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_ = nullptr;
static void LoadFBO() {
  if (glGenFramebuffers_) return;
  glGenFramebuffers_ = (PFNGLGENFRAMEBUFFERSPROC)glXGetProcAddress((const GLubyte*)"glGenFramebuffers");
  glBindFramebuffer_ = (PFNGLBINDFRAMEBUFFERPROC)glXGetProcAddress((const GLubyte*)"glBindFramebuffer");
  glFramebufferTexture2D_ = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glXGetProcAddress((const GLubyte*)"glFramebufferTexture2D");
}

static void Die(const char* m) { fprintf(stderr, "[cube_webui] FATAL: %s\n", m); }

// --- math ---
struct Vec3 { float x, y, z; };
static Vec3 V3(float x, float y, float z) { return {x, y, z}; }
static Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec3 Cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static Vec3 Normalize(Vec3 v) {
  float l = std::sqrt(Dot(v, v));
  if (l < 1e-8f) return {0, 0, -1};
  return v * (1.f / l);
}
static Vec3 QuatRotate(XrQuaternionf q, Vec3 v) {
  // q * (0,v) * q^-1
  Vec3 u = {q.x, q.y, q.z};
  float s = q.w;
  return u * (2.f * Dot(u, v)) + v * (s * s - Dot(u, u)) + Cross(u, v) * (2.f * s);
}
static XrPosef IdentityPose() {
  XrPosef p{};
  p.orientation.w = 1.f;
  return p;
}

// Flexible head-facing menu. Tunable without recompile (project conf ships defaults):
//   project:  install/native/cube_webui.conf  or  native_launcher/cube_webui.conf
//   user:     ~/.config/gvrmod/cube_webui.conf
//   gmod:     $GMOD/garrysmod/data/vrmod/cube_webui.conf
//   env:      CUBE_PANEL_*  CUBE_PASSTHROUGH  CUBE_VIEW_LOCK
struct PanelConfig {
  float dist = 1.05f;     // default depth meters (center z = -dist + offset_z)
  float halfW = 0.42f;    // full width 0.84m
  float halfH = 0.24f;    // full height 0.48m
  float offsetX = 0.f;    // VIEW lateral (m)
  float offsetY = 0.f;    // VIEW vertical (m)
  float offsetZ = 0.f;    // extra depth (+ = further / more -Z)
  // WayVR-style: world-locked floating panel (grab to place). Head-follow is optional.
  bool viewLock = false;
  bool passthrough = true;
  float grabThresh = 0.55f;
  float panelAlpha = 0.96f; // UI opacity over passthrough
};

// Runtime placement (meters). Space: VIEW when viewLock, else LOCAL after place.
struct PanelPose {
  float cx = 0.f, cy = 0.f, cz = -1.05f;
};

static PanelConfig g_cfg{};
static PanelPose g_pose{};

static void ResetPanelPoseFromConfig() {
  g_pose.cx = g_cfg.offsetX;
  g_pose.cy = g_cfg.offsetY;
  g_pose.cz = -g_cfg.dist + g_cfg.offsetZ;
}

static std::string Dirname(const std::string& p) {
  auto s = p.find_last_of('/');
  if (s == std::string::npos) return ".";
  if (s == 0) return "/";
  return p.substr(0, s);
}

static std::string ExeDir() {
  char buf[4096];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) return {};
  buf[n] = 0;
  return Dirname(buf);
}

static void ApplyConfigKey(const char* key, float val) {
  if (strcmp(key, "panel_dist") == 0 || strcmp(key, "dist") == 0) {
    if (val >= 0.25f && val <= 4.f) g_cfg.dist = val;
  } else if (strcmp(key, "panel_w") == 0) {
    if (val > 0.2f && val < 4.f) g_cfg.halfW = val * 0.5f;
  } else if (strcmp(key, "panel_h") == 0) {
    if (val > 0.15f && val < 3.f) g_cfg.halfH = val * 0.5f;
  } else if (strcmp(key, "panel_half_w") == 0 || strcmp(key, "half_w") == 0) {
    if (val > 0.1f && val < 2.f) g_cfg.halfW = val;
  } else if (strcmp(key, "panel_half_h") == 0 || strcmp(key, "half_h") == 0) {
    if (val > 0.08f && val < 1.5f) g_cfg.halfH = val;
  } else if (strcmp(key, "panel_x") == 0 || strcmp(key, "offset_x") == 0) {
    if (val > -2.f && val < 2.f) g_cfg.offsetX = val;
  } else if (strcmp(key, "panel_y") == 0 || strcmp(key, "offset_y") == 0) {
    if (val > -2.f && val < 2.f) g_cfg.offsetY = val;
  } else if (strcmp(key, "panel_z") == 0 || strcmp(key, "offset_z") == 0) {
    if (val > -2.f && val < 2.f) g_cfg.offsetZ = val;
  } else if (strcmp(key, "grab_thresh") == 0 || strcmp(key, "grab_threshold") == 0) {
    if (val >= 0.2f && val <= 1.f) g_cfg.grabThresh = val;
  } else if (strcmp(key, "panel_alpha") == 0) {
    if (val >= 0.4f && val <= 1.f) g_cfg.panelAlpha = val;
  } else if (strcmp(key, "passthrough") == 0 || strcmp(key, "ar") == 0) {
    g_cfg.passthrough = (val != 0.f);
  } else if (strcmp(key, "view_lock") == 0 || strcmp(key, "viewlock") == 0 ||
             strcmp(key, "head_lock") == 0) {
    g_cfg.viewLock = (val != 0.f);
  } else if (strcmp(key, "world_lock") == 0) {
    g_cfg.viewLock = (val == 0.f); // world_lock=1 → viewLock false
  }
}

static bool LoadConfigFile(const std::string& path) {
  FILE* f = fopen(path.c_str(), "r");
  if (!f) return false;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '#' || line[0] == ';' || line[0] == '\n') continue;
    char key[64] = {};
    float val = 0.f;
    if (sscanf(line, " %63[^=]=%f", key, &val) != 2) continue;
    ApplyConfigKey(key, val);
  }
  fclose(f);
  fprintf(stderr, "[cube_webui] config %s\n", path.c_str());
  return true;
}

static void LoadPanelConfig(const std::string& gmodRoot) {
  g_cfg = PanelConfig{};
  // 1) Project defaults next to binary / source tree
  std::string exe = ExeDir();
  if (!exe.empty()) {
    LoadConfigFile(exe + "/cube_webui.conf");
    // install/native → ../../native_launcher/cube_webui.conf
    LoadConfigFile(Dirname(Dirname(exe)) + "/native_launcher/cube_webui.conf");
  }
  LoadConfigFile("cube_webui.conf");
  // 2) User override
  if (const char* home = getenv("HOME")) {
    LoadConfigFile(std::string(home) + "/.config/gvrmod/cube_webui.conf");
  }
  // 3) GMod data
  if (!gmodRoot.empty()) {
    LoadConfigFile(gmodRoot + "/garrysmod/data/vrmod/cube_webui.conf");
  }
  // 4) Env wins
  auto tryEnvF = [](const char* k, float& out, float lo, float hi) {
    if (const char* v = getenv(k)) {
      float f = strtof(v, nullptr);
      if (f > lo && f < hi) out = f;
    }
  };
  auto tryEnvB = [](const char* k, bool& out) {
    if (const char* v = getenv(k)) out = !(v[0] == '0' && v[1] == 0);
  };
  if (const char* v = getenv("CUBE_PANEL_W")) {
    float f = strtof(v, nullptr);
    if (f > 0.3f && f < 4.f) g_cfg.halfW = f * 0.5f;
  }
  if (const char* v = getenv("CUBE_PANEL_H")) {
    float f = strtof(v, nullptr);
    if (f > 0.2f && f < 3.f) g_cfg.halfH = f * 0.5f;
  }
  tryEnvF("CUBE_PANEL_DIST", g_cfg.dist, 0.25f, 4.f);
  tryEnvF("CUBE_PANEL_HALF_W", g_cfg.halfW, 0.1f, 2.f);
  tryEnvF("CUBE_PANEL_HALF_H", g_cfg.halfH, 0.08f, 1.5f);
  tryEnvF("CUBE_PANEL_X", g_cfg.offsetX, -2.f, 2.f);
  tryEnvF("CUBE_PANEL_Y", g_cfg.offsetY, -2.f, 2.f);
  tryEnvF("CUBE_PANEL_Z", g_cfg.offsetZ, -2.f, 2.f);
  tryEnvF("CUBE_PANEL_ALPHA", g_cfg.panelAlpha, 0.4f, 1.01f);
  tryEnvF("CUBE_GRAB_THRESH", g_cfg.grabThresh, 0.2f, 1.01f);
  tryEnvB("CUBE_PASSTHROUGH", g_cfg.passthrough);
  tryEnvB("CUBE_VIEW_LOCK", g_cfg.viewLock);
  if (const char* v = getenv("CUBE_WORLD_LOCK")) {
    if (!(v[0] == '0' && v[1] == 0)) g_cfg.viewLock = false;
  }

  ResetPanelPoseFromConfig();
  fprintf(stderr,
          "[cube_webui] panel size=%.2fx%.2fm dist=%.2f offset=(%.2f,%.2f,%.2f) "
          "view_lock=%d passthrough=%d grab>=%.2f\n",
          g_cfg.halfW * 2.f, g_cfg.halfH * 2.f, g_cfg.dist,
          g_cfg.offsetX, g_cfg.offsetY, g_cfg.offsetZ,
          g_cfg.viewLock ? 1 : 0, g_cfg.passthrough ? 1 : 0, g_cfg.grabThresh);
}

// Panel plane in the same space as the ray (VIEW head-lock, or converted VIEW for world).
// Center (cx,cy,cz), faces -Z (head-on). UV: +X right, +Y up → image top-left.
static bool RayPanelHit(Vec3 origin, Vec3 dir, const PanelPose& pose,
                        int* outPx, int* outPy, Vec3* outHit) {
  const float hw = g_cfg.halfW;
  const float hh = g_cfg.halfH;
  Vec3 d = Normalize(dir);
  if (std::fabs(d.z) < 1e-6f) return false;
  float t = (pose.cz - origin.z) / d.z;
  if (t < 0.01f || t > 8.f) return false;
  Vec3 hit = origin + d * t;
  const float margin = 1.05f;
  if (std::fabs(hit.x - pose.cx) > hw * margin ||
      std::fabs(hit.y - pose.cy) > hh * margin) return false;
  float lx = std::max(-hw, std::min(hw, hit.x - pose.cx));
  float ly = std::max(-hh, std::min(hh, hit.y - pose.cy));
  int px = (int)((lx / hw * 0.5f + 0.5f) * (float)UI_W);
  int py = (int)((0.5f - ly / hh * 0.5f) * (float)UI_H);
  px = std::max(0, std::min(UI_W - 1, px));
  py = std::max(0, std::min(UI_H - 1, py));
  if (outPx) *outPx = px;
  if (outPy) *outPy = py;
  if (outHit) *outHit = hit;
  return true;
}

// Column-major 4x4: parent_from_local
static void PoseToMat(const XrPosef& pose, float M[16]) {
  float x = pose.orientation.x, y = pose.orientation.y, z = pose.orientation.z, w = pose.orientation.w;
  // row-major R then pack as columns
  float r00 = 1 - 2*y*y - 2*z*z, r01 = 2*x*y - 2*z*w, r02 = 2*x*z + 2*y*w;
  float r10 = 2*x*y + 2*z*w,     r11 = 1 - 2*x*x - 2*z*z, r12 = 2*y*z - 2*x*w;
  float r20 = 2*x*z - 2*y*w,     r21 = 2*y*z + 2*x*w,     r22 = 1 - 2*x*x - 2*y*y;
  M[0] = r00; M[1] = r10; M[2] = r20; M[3] = 0;
  M[4] = r01; M[5] = r11; M[6] = r21; M[7] = 0;
  M[8] = r02; M[9] = r12; M[10] = r22; M[11] = 0;
  M[12] = pose.position.x; M[13] = pose.position.y; M[14] = pose.position.z; M[15] = 1;
}
static void MatMul4(const float A[16], const float B[16], float O[16]) {
  float T[16];
  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      T[c * 4 + r] = A[0 * 4 + r] * B[c * 4 + 0] + A[1 * 4 + r] * B[c * 4 + 1] +
                     A[2 * 4 + r] * B[c * 4 + 2] + A[3 * 4 + r] * B[c * 4 + 3];
    }
  }
  std::memcpy(O, T, sizeof(T));
}
// inv(eye_local) * view_local → modelview that takes VIEW-space verts into eye camera space
static void LoadModelviewViewSpace(const XrPosef& eyeLocal, const XrPosef& viewLocal) {
  float invEye[16], viewM[16], mv[16];
  // inv(eye) = local_from_parent for eye pose in LOCAL
  Vec3 t = {eyeLocal.position.x, eyeLocal.position.y, eyeLocal.position.z};
  float x = eyeLocal.orientation.x, y = eyeLocal.orientation.y, z = eyeLocal.orientation.z, w = eyeLocal.orientation.w;
  float R[9] = {
    1 - 2*y*y - 2*z*z, 2*x*y - 2*z*w, 2*x*z + 2*y*w,
    2*x*y + 2*z*w, 1 - 2*x*x - 2*z*z, 2*y*z - 2*x*w,
    2*x*z - 2*y*w, 2*y*z + 2*x*w, 1 - 2*x*x - 2*y*y
  };
  invEye[0] = R[0]; invEye[1] = R[3]; invEye[2] = R[6]; invEye[3] = 0;
  invEye[4] = R[1]; invEye[5] = R[4]; invEye[6] = R[7]; invEye[7] = 0;
  invEye[8] = R[2]; invEye[9] = R[5]; invEye[10] = R[8]; invEye[11] = 0;
  invEye[12] = -(R[0]*t.x + R[3]*t.y + R[6]*t.z);
  invEye[13] = -(R[1]*t.x + R[4]*t.y + R[7]*t.z);
  invEye[14] = -(R[2]*t.x + R[5]*t.y + R[8]*t.z);
  invEye[15] = 1;
  PoseToMat(viewLocal, viewM);
  MatMul4(invEye, viewM, mv);
  glLoadMatrixf(mv);
}

// --- GLX ---
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
  XMapWindow(g.dpy, g.win);
  XFlush(g.dpy);
  g.ctx = glXCreateContext(g.dpy, vi, nullptr, GL_TRUE);
  XFree(vi);
  if (!g.ctx) return false;
  return glXMakeCurrent(g.dpy, g.win, g.ctx);
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

static GLuint MakeTex(int w, int h, const void* rgba) {
  GLuint t = 0;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  return t;
}

// Face-on panel at runtime pose (VIEW space verts)
static void DrawPanelAt(GLuint tex, const PanelPose& pose) {
  const float hw = g_cfg.halfW, hh = g_cfg.halfH;
  const float cx = pose.cx, cy = pose.cy, cz = pose.cz;
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tex);
  glColor4f(1.f, 1.f, 1.f, g_cfg.panelAlpha);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 1); glVertex3f(cx - hw, cy - hh, cz);
  glTexCoord2f(1, 1); glVertex3f(cx + hw, cy - hh, cz);
  glTexCoord2f(1, 0); glVertex3f(cx + hw, cy + hh, cz);
  glTexCoord2f(0, 0); glVertex3f(cx - hw, cy + hh, cz);
  glEnd();
  glDisable(GL_TEXTURE_2D);
  glColor4f(1, 1, 1, 1);
  glDisable(GL_BLEND);
}

// Local → VIEW (parent_from_child for vectors via inv view)
static Vec3 PoseInvRotate(const XrPosef& pose, Vec3 v) {
  XrQuaternionf q = pose.orientation;
  q.x = -q.x; q.y = -q.y; q.z = -q.z;
  return QuatRotate(q, v);
}
static Vec3 PoseInvTransform(const XrPosef& pose, Vec3 p) {
  Vec3 t = {pose.position.x, pose.position.y, pose.position.z};
  return PoseInvRotate(pose, p - t);
}
static Vec3 PoseTransform(const XrPosef& pose, Vec3 p) {
  return QuatRotate(pose.orientation, p) +
         V3(pose.position.x, pose.position.y, pose.position.z);
}

// World-lock: store LOCAL; hit/draw need VIEW. View-lock: pose already VIEW.
static PanelPose PanelPoseInView(const XrPosef& viewLocal, bool viewPoseOk) {
  if (g_cfg.viewLock || !viewPoseOk) return g_pose;
  // g_pose is LOCAL center → VIEW
  Vec3 local = V3(g_pose.cx, g_pose.cy, g_pose.cz);
  Vec3 view = PoseInvTransform(viewLocal, local);
  return PanelPose{view.x, view.y, view.z};
}

static void SetPanelFromView(const PanelPose& viewPose, const XrPosef& viewLocal, bool viewPoseOk) {
  if (g_cfg.viewLock || !viewPoseOk) {
    g_pose = viewPose;
    return;
  }
  Vec3 local = PoseTransform(viewLocal, V3(viewPose.cx, viewPose.cy, viewPose.cz));
  g_pose = {local.x, local.y, local.z};
}

static void DrawLaserView(Vec3 a, Vec3 b, float cr, float cg, float cb) {
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_DEPTH_TEST);
  glLineWidth(4.f);
  glBegin(GL_LINES);
  glColor3f(cr, cg, cb);
  glVertex3f(a.x, a.y, a.z);
  glVertex3f(b.x, b.y, b.z);
  glEnd();
  glPointSize(10.f);
  glBegin(GL_POINTS);
  glColor3f(1.f, 1.f, 1.f);
  glVertex3f(b.x, b.y, b.z);
  glEnd();
  glEnable(GL_DEPTH_TEST);
  glColor3f(1, 1, 1);
}

// --- OpenXR actions (laser + grab) ---
struct XrInput {
  XrActionSet set = XR_NULL_HANDLE;
  XrAction pose = XR_NULL_HANDLE;
  XrAction trigger = XR_NULL_HANDLE;      // boolean click
  XrAction triggerAxis = XR_NULL_HANDLE;  // float value (Touch/Index)
  XrAction grab = XR_NULL_HANDLE;         // squeeze float — move menu
  XrAction stick = XR_NULL_HANDLE;
  XrAction menu = XR_NULL_HANDLE;
  XrSpace aimSpace = XR_NULL_HANDLE;
  bool attached = false;
};

static bool SetupInput(XrInstance instance, XrSession session, XrInput& in) {
  XrActionSetCreateInfo asci{XR_TYPE_ACTION_SET_CREATE_INFO};
  std::strncpy(asci.actionSetName, "cube_ui", XR_MAX_ACTION_SET_NAME_SIZE - 1);
  std::strncpy(asci.localizedActionSetName, "Cube UI", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
  if (XR_FAILED(xrCreateActionSet(instance, &asci, &in.set))) return false;

  auto mk = [&](const char* name, const char* loc, XrActionType ty, XrAction* out) {
    XrActionCreateInfo aci{XR_TYPE_ACTION_CREATE_INFO};
    aci.actionType = ty;
    std::strncpy(aci.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
    std::strncpy(aci.localizedActionName, loc, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
    return XR_SUCCEEDED(xrCreateAction(in.set, &aci, out));
  };
  if (!mk("aim_pose", "Aim Pose", XR_ACTION_TYPE_POSE_INPUT, &in.pose)) return false;
  if (!mk("trigger", "Trigger Click", XR_ACTION_TYPE_BOOLEAN_INPUT, &in.trigger)) return false;
  if (!mk("trigger_axis", "Trigger Axis", XR_ACTION_TYPE_FLOAT_INPUT, &in.triggerAxis)) return false;
  if (!mk("grab", "Grab Move Menu", XR_ACTION_TYPE_FLOAT_INPUT, &in.grab)) return false;
  if (!mk("stick", "Thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, &in.stick)) return false;
  if (!mk("menu", "Menu", XR_ACTION_TYPE_BOOLEAN_INPUT, &in.menu)) return false;

  auto suggest = [&](const char* profile, std::vector<XrActionSuggestedBinding> binds) {
    XrPath prof{};
    if (XR_FAILED(xrStringToPath(instance, profile, &prof))) return;
    XrInteractionProfileSuggestedBinding sp{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    sp.interactionProfile = prof;
    sp.suggestedBindings = binds.data();
    sp.countSuggestedBindings = (uint32_t)binds.size();
    xrSuggestInteractionProfileBindings(instance, &sp);
  };

  XrPath pPose{}, pTrigClick{}, pTrigVal{}, pSqueeze{}, pThumb{}, pMenu{}, pA{};
  xrStringToPath(instance, "/user/hand/right/input/aim/pose", &pPose);
  xrStringToPath(instance, "/user/hand/right/input/trigger/click", &pTrigClick);
  xrStringToPath(instance, "/user/hand/right/input/trigger/value", &pTrigVal);
  xrStringToPath(instance, "/user/hand/right/input/squeeze/value", &pSqueeze);
  xrStringToPath(instance, "/user/hand/right/input/thumbstick", &pThumb);
  xrStringToPath(instance, "/user/hand/right/input/menu/click", &pMenu);
  xrStringToPath(instance, "/user/hand/right/input/a/click", &pA);

  suggest("/interaction_profiles/oculus/touch_controller", {
    {in.pose, pPose},
    {in.trigger, pTrigClick},
    {in.triggerAxis, pTrigVal},
    {in.grab, pSqueeze},
    {in.stick, pThumb},
    {in.menu, pMenu},
  });
  suggest("/interaction_profiles/valve/index_controller", {
    {in.pose, pPose},
    {in.trigger, pTrigClick},
    {in.triggerAxis, pTrigVal},
    {in.grab, pSqueeze},
    {in.stick, pThumb},
    {in.menu, pA},
  });
  // Meta Quest Touch Pro / Plus often use same paths as Touch
  suggest("/interaction_profiles/facebook/touch_controller_pro", {
    {in.pose, pPose},
    {in.trigger, pTrigClick},
    {in.triggerAxis, pTrigVal},
    {in.grab, pSqueeze},
    {in.stick, pThumb},
    {in.menu, pMenu},
  });
  XrPath pSelect{};
  xrStringToPath(instance, "/user/hand/right/input/select/click", &pSelect);
  suggest("/interaction_profiles/khr/simple_controller", {
    {in.pose, pPose},
    {in.trigger, pSelect},
  });

  XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attach.countActionSets = 1;
  attach.actionSets = &in.set;
  if (XR_FAILED(xrAttachSessionActionSets(session, &attach))) {
    fprintf(stderr, "[cube_webui] warn: attach action sets failed\n");
    return false;
  }

  XrActionSpaceCreateInfo spaceCi{XR_TYPE_ACTION_SPACE_CREATE_INFO};
  spaceCi.action = in.pose;
  spaceCi.poseInActionSpace = IdentityPose();
  if (XR_FAILED(xrCreateActionSpace(session, &spaceCi, &in.aimSpace))) {
    fprintf(stderr, "[cube_webui] warn: aim space failed\n");
    return false;
  }
  in.attached = true;
  fprintf(stderr, "[cube_webui] actions: aim + trigger + GRAB(squeeze) + stick\n");
  return true;
}

// Prefer ALPHA_BLEND for passthrough; fall back to OPAQUE / ADDITIVE.
static XrEnvironmentBlendMode PickBlendMode(XrInstance instance, XrSystemId systemId, bool wantPassthrough) {
  uint32_t n = 0;
  xrEnumerateEnvironmentBlendModes(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                   0, &n, nullptr);
  std::vector<XrEnvironmentBlendMode> modes(n);
  if (n)
    xrEnumerateEnvironmentBlendModes(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                     n, &n, modes.data());
  auto has = [&](XrEnvironmentBlendMode m) {
    for (auto x : modes) if (x == m) return true;
    return false;
  };
  if (wantPassthrough && has(XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND)) {
    fprintf(stderr, "[cube_webui] blend=ALPHA_BLEND (passthrough)\n");
    return XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
  }
  if (wantPassthrough && has(XR_ENVIRONMENT_BLEND_MODE_ADDITIVE)) {
    fprintf(stderr, "[cube_webui] blend=ADDITIVE (passthrough-ish)\n");
    return XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
  }
  fprintf(stderr, "[cube_webui] blend=OPAQUE%s\n",
          wantPassthrough ? " (runtime has no alpha/additive passthrough)" : "");
  return XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
}

static bool ExtensionAvailable(const char* name) {
  uint32_t n = 0;
  xrEnumerateInstanceExtensionProperties(nullptr, 0, &n, nullptr);
  std::vector<XrExtensionProperties> props(n, {XR_TYPE_EXTENSION_PROPERTIES});
  if (n) xrEnumerateInstanceExtensionProperties(nullptr, n, &n, props.data());
  for (auto& p : props)
    if (std::strcmp(p.extensionName, name) == 0) return true;
  return false;
}

int RunCubeWebUILauncher(const std::string& gmodRoot, const std::string& xrJson) {
  if (!xrJson.empty())
    setenv("XR_RUNTIME_JSON", xrJson.c_str(), 1);

  LoadPanelConfig(gmodRoot);

  fprintf(stderr, "[cube_webui] OpenXR WebUI reverse launcher (WayVR world panel)\n");
  fprintf(stderr, "[cube_webui] GMOD=%s XR=%s\n", gmodRoot.c_str(),
          getenv("XR_RUNTIME_JSON") ? getenv("XR_RUNTIME_JSON") : "(default)");
  fprintf(stderr, "[cube_webui] TRIGGER=click  GRIP=move world panel  MENU=reset\n");
  fprintf(stderr, "[cube_webui] seamless Start: hold XR until GMod take_xr (no gap)\n");
  fprintf(stderr, "[cube_webui] host: echo click|reset|start >/tmp/cube_webui_cmd\n");

  GlxCtx glx{};
  if (!MakeGlx(glx)) {
    Die("GLX context failed");
    return 1;
  }

  std::vector<const char*> extList = { XR_KHR_OPENGL_ENABLE_EXTENSION_NAME };
  bool wantFbPt = g_cfg.passthrough && ExtensionAvailable(XR_FB_PASSTHROUGH_EXTENSION_NAME);
  if (wantFbPt) {
    extList.push_back(XR_FB_PASSTHROUGH_EXTENSION_NAME);
    fprintf(stderr, "[cube_webui] enabling %s\n", XR_FB_PASSTHROUGH_EXTENSION_NAME);
  }

  XrInstanceCreateInfo ici{XR_TYPE_INSTANCE_CREATE_INFO};
  std::strncpy(ici.applicationInfo.applicationName, "CubeWebUILauncher", XR_MAX_APPLICATION_NAME_SIZE - 1);
  ici.applicationInfo.applicationVersion = 1;
  std::strncpy(ici.applicationInfo.engineName, "gVRMod", XR_MAX_ENGINE_NAME_SIZE - 1);
  ici.applicationInfo.engineVersion = 2;
  ici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
  ici.enabledExtensionCount = (uint32_t)extList.size();
  ici.enabledExtensionNames = extList.data();

  XrInstance instance = XR_NULL_HANDLE;
  if (XR_FAILED(xrCreateInstance(&ici, &instance))) {
    // Retry without optional FB passthrough
    if (wantFbPt) {
      fprintf(stderr, "[cube_webui] instance create failed with FB passthrough; retrying without\n");
      extList.resize(1);
      wantFbPt = false;
      ici.enabledExtensionCount = 1;
      ici.enabledExtensionNames = extList.data();
    }
    if (XR_FAILED(xrCreateInstance(&ici, &instance))) {
      Die("xrCreateInstance failed — WiVRn/Monado + headset?");
      DestroyGlx(glx);
      return 2;
    }
  }

  XrSystemGetInfo sgi{XR_TYPE_SYSTEM_GET_INFO};
  sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  XrSystemId systemId = XR_NULL_SYSTEM_ID;
  if (XR_FAILED(xrGetSystem(instance, &sgi, &systemId))) {
    Die("xrGetSystem failed");
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
  binding.visualid = 0;
  binding.glxFBConfig = nullptr;

  XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
  sci.next = &binding;
  sci.systemId = systemId;
  XrSession session = XR_NULL_HANDLE;
  if (XR_FAILED(xrCreateSession(instance, &sci, &session))) {
    Die("xrCreateSession failed");
    xrDestroyInstance(instance);
    DestroyGlx(glx);
    return 4;
  }

  XrInput input{};
  SetupInput(instance, session, input);

  XrEnvironmentBlendMode blendMode = PickBlendMode(instance, systemId, g_cfg.passthrough);
  const bool useAlphaClear =
      (blendMode == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND) ||
      (blendMode == XR_ENVIRONMENT_BLEND_MODE_ADDITIVE);

  // Optional XR_FB_passthrough reconstruction layer under the UI
  XrPassthroughFB passthrough = XR_NULL_HANDLE;
  XrPassthroughLayerFB ptLayerHandle = XR_NULL_HANDLE;
  PFN_xrCreatePassthroughFB pfnCreatePt = nullptr;
  PFN_xrDestroyPassthroughFB pfnDestroyPt = nullptr;
  PFN_xrPassthroughStartFB pfnStartPt = nullptr;
  PFN_xrPassthroughPauseFB pfnPausePt = nullptr;
  PFN_xrCreatePassthroughLayerFB pfnCreatePtLayer = nullptr;
  PFN_xrDestroyPassthroughLayerFB pfnDestroyPtLayer = nullptr;
  if (wantFbPt) {
    xrGetInstanceProcAddr(instance, "xrCreatePassthroughFB", (PFN_xrVoidFunction*)&pfnCreatePt);
    xrGetInstanceProcAddr(instance, "xrDestroyPassthroughFB", (PFN_xrVoidFunction*)&pfnDestroyPt);
    xrGetInstanceProcAddr(instance, "xrPassthroughStartFB", (PFN_xrVoidFunction*)&pfnStartPt);
    xrGetInstanceProcAddr(instance, "xrPassthroughPauseFB", (PFN_xrVoidFunction*)&pfnPausePt);
    xrGetInstanceProcAddr(instance, "xrCreatePassthroughLayerFB", (PFN_xrVoidFunction*)&pfnCreatePtLayer);
    xrGetInstanceProcAddr(instance, "xrDestroyPassthroughLayerFB", (PFN_xrVoidFunction*)&pfnDestroyPtLayer);
    if (pfnCreatePt && pfnCreatePtLayer && pfnStartPt) {
      XrPassthroughCreateInfoFB pci{XR_TYPE_PASSTHROUGH_CREATE_INFO_FB};
      pci.flags = XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB;
      if (XR_SUCCEEDED(pfnCreatePt(session, &pci, &passthrough))) {
        XrPassthroughLayerCreateInfoFB lci{XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB};
        lci.passthrough = passthrough;
        lci.flags = XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB;
        lci.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
        if (XR_SUCCEEDED(pfnCreatePtLayer(session, &lci, &ptLayerHandle))) {
          pfnStartPt(passthrough);
          fprintf(stderr, "[cube_webui] FB passthrough layer active\n");
        } else {
          fprintf(stderr, "[cube_webui] warn: create passthrough layer failed\n");
          if (pfnDestroyPt) pfnDestroyPt(passthrough);
          passthrough = XR_NULL_HANDLE;
        }
      } else {
        fprintf(stderr, "[cube_webui] warn: create passthrough failed (use blend only)\n");
      }
    }
  }

  XrReferenceSpaceCreateInfo rsci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  rsci.poseInReferenceSpace = IdentityPose();
  XrSpace space = XR_NULL_HANDLE;
  xrCreateReferenceSpace(session, &rsci, &space);

  // Also view space for HMD place
  XrSpace viewSpace = XR_NULL_HANDLE;
  rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  xrCreateReferenceSpace(session, &rsci, &viewSpace);

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
    sc.format = 0x8058;
    sc.sampleCount = 1;
    sc.width = eyes[i].w;
    sc.height = eyes[i].h;
    sc.faceCount = 1;
    sc.arraySize = 1;
    sc.mipCount = 1;
    if (XR_FAILED(xrCreateSwapchain(session, &sc, &eyes[i].swap))) {
      Die("xrCreateSwapchain failed");
      return 5;
    }
    uint32_t nImg = 0;
    xrEnumerateSwapchainImages(eyes[i].swap, 0, &nImg, nullptr);
    eyes[i].images.resize(nImg, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
    xrEnumerateSwapchainImages(eyes[i].swap, nImg, &nImg, (XrSwapchainImageBaseHeader*)eyes[i].images.data());
  }

  XrSessionBeginInfo sbi{XR_TYPE_SESSION_BEGIN_INFO};
  sbi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  XrSessionState state = XR_SESSION_STATE_UNKNOWN;
  bool running = true;
  bool sessionRunning = false;

  WebUIState ui{};
  WebUI_Init(ui, gmodRoot);
  ui.status = "WORLD PANEL · GRIP move · TRIGGER click · seamless Start";
  std::vector<unsigned char> panelBuf(UI_W * UI_H * 4);
  WebUI_Rasterize(ui, panelBuf.data(), nullptr);
  GLuint panelTex = MakeTex(UI_W, UI_H, panelBuf.data());

  bool prevTrigger = false;
  bool prevMenu = false;
  bool grabbing = false;
  bool worldInitPending = !g_cfg.viewLock; // convert default VIEW pose → LOCAL once
  Vec3 grabOff{0, 0, 0}; // panel center - aim origin at grab start (VIEW)
  float stickCooldown = 0.f;
  int frame = 0;
  Vec3 viewAimO{0, 0, 0}, viewAimD{0, 0, -1}, viewHitPt{0, 0, -1.f};
  bool viewAimValid = false;
  bool viewHit = false;

  auto pollCmdFile = [&]() {
    FILE* f = fopen("/tmp/cube_webui_cmd", "r");
    if (!f) return;
    char buf[64] = {};
    if (fgets(buf, sizeof(buf), f)) {
      if (std::strncmp(buf, "start", 5) == 0) ui.wantStart = true;
      if (std::strncmp(buf, "quit", 4) == 0) ui.wantQuit = true;
      if (std::strncmp(buf, "addons", 6) == 0) { ui.page = WebUIPage::Addons; }
      if (std::strncmp(buf, "newgame", 7) == 0) { ui.page = WebUIPage::NewGame; }
      if (std::strncmp(buf, "settings", 8) == 0) { ui.page = WebUIPage::Settings; }
      if (std::strncmp(buf, "reset", 5) == 0) {
        ResetPanelPoseFromConfig();
        grabbing = false;
        worldInitPending = !g_cfg.viewLock;
        ui.status = "PANEL RESET";
      }
      if (std::strncmp(buf, "up", 2) == 0) WebUI_Input(ui, 0, -1, false, false);
      if (std::strncmp(buf, "down", 4) == 0) WebUI_Input(ui, 0, 1, false, false);
      if (std::strncmp(buf, "left", 4) == 0) WebUI_Input(ui, -1, 0, false, false);
      if (std::strncmp(buf, "right", 5) == 0) WebUI_Input(ui, 1, 0, false, false);
      if (std::strncmp(buf, "click", 5) == 0 || std::strncmp(buf, "toggle", 6) == 0) {
        if (ui.cursorVisible)
          WebUI_PointerClick(ui, ui.cursorX, ui.cursorY);
        else
          WebUI_Input(ui, 0, 0, true, false);
      }
    }
    fclose(f);
    unlink("/tmp/cube_webui_cmd");
  };

  while (running) {
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
        if (state == XR_SESSION_STATE_STOPPING && sessionRunning) {
          xrEndSession(session);
          sessionRunning = false;
        }
        if (state == XR_SESSION_STATE_EXITING || state == XR_SESSION_STATE_LOSS_PENDING)
          running = false;
      }
      ev = {XR_TYPE_EVENT_DATA_BUFFER};
    }

    pollCmdFile();
    if (ui.wantQuit) break;

    // --- StartGame: spawn GMod but KEEP OpenXR until handoff (Cube hates gaps) ---
    if (ui.wantStart && !ui.handoff) {
      LaunchRequest lr;
      lr.gmodRoot = gmodRoot;
      lr.map = WebUI_SelectedMap(ui);
      lr.maxPlayers = WebUI_MaxPlayers(ui);
      lr.hostname = ui.hostname;
      lr.svLan = ui.svLan;
      lr.p2p = ui.p2p;
      lr.p2pFriends = ui.p2pFriends;
      lr.gamemode = ui.gamemode;
      lr.winW = ui.gfx.winW;
      lr.winH = ui.gfx.winH;
      lr.windowed = ui.gfx.windowed;
      lr.noborder = ui.gfx.noborder;
      lr.gfx.matPicmip = ui.gfx.matPicmip;
      lr.gfx.rRootLod = ui.gfx.rRootLod;
      lr.gfx.matAntialias = ui.gfx.matAntialias;
      lr.gfx.matForceAniso = ui.gfx.matForceAniso;
      lr.gfx.matHdrLevel = ui.gfx.matHdrLevel;
      lr.gfx.shadows = ui.gfx.shadows;
      lr.gfx.flashlightShadows = ui.gfx.flashlightShadows;
      lr.gfx.specular = ui.gfx.specular;
      lr.gfx.bumpmap = ui.gfx.bumpmap;
      lr.gfx.waterExpensive = ui.gfx.waterExpensive;
      lr.gfx.multicore = ui.gfx.multicore;
      lr.gfx.fpsMax = ui.gfx.fpsMax;
      if (const char* xr = getenv("XR_RUNTIME_JSON")) lr.xrRuntimeJson = xr;
      ClearCubeHandoffMarkers(gmodRoot);
      std::string err;
      int rc = SpawnGModFromWebUI(lr, err);
      fprintf(stderr, "[cube_webui] StartGame map=%s rc=%d %s\n", lr.map.c_str(), rc, err.c_str());
      ui.wantStart = false;
      if (rc == 0) {
        ui.handoff = true;
        ui.handoffMap = lr.map;
        ui.handoffPhase = "SPAWNED";
        ui.handoffDetail = "holding OpenXR · GMod booting";
        ui.handoffElapsed = 0.f;
        ui.status = "HANDOFF — STAY IN VR";
        fprintf(stderr, "[cube_webui] seamless handoff: keep XR UI until GMod take_xr\n");
      } else {
        ui.status = "SPAWN FAIL: " + err;
      }
    }

    if (ui.handoff) {
      ui.handoffElapsed += 1.f / 72.f;
      const bool gmodUp = GModProcessRunning();
      std::string phase = ReadCubeHandoffPhase(gmodRoot);
      if (phase.empty()) phase = gmodUp ? "gmod_process" : "waiting_process";
      ui.handoffPhase = phase;
      if (gmodUp)
        ui.handoffDetail = "GMod process up · waiting take_xr";
      else
        ui.handoffDetail = "waiting for GMod process…";

      // Lua writes phase=take_xr before VRUtilClientStart — then we release XR
      bool takeXr = (phase == "take_xr" || phase == "vr_active" || phase == "ready");
      // Soft fallback only if Lua never signals (old addon) — still late enough to cover boot
      bool softHandoff = gmodUp && ui.handoffElapsed > 40.f;
      // Absolute timeout
      bool timeout = ui.handoffElapsed > 180.f;
      // Session lost (another app / runtime switched) — exit clean
      bool sessionGone = !sessionRunning && ui.handoffElapsed > 1.f;

      if (takeXr || softHandoff || timeout || sessionGone) {
        fprintf(stderr,
                "[cube_webui] handoff exit phase=%s gmod=%d t=%.1f take=%d soft=%d timeout=%d gone=%d\n",
                phase.c_str(), gmodUp ? 1 : 0, ui.handoffElapsed,
                takeXr ? 1 : 0, softHandoff ? 1 : 0, timeout ? 1 : 0, sessionGone ? 1 : 0);
        ui.status = "HANDING OFF TO GMOD";
        // Request clean session end so compositor can switch to GMod without a long void
        if (sessionRunning) {
          xrRequestExitSession(session);
        }
        // One more frame path optional; exit loop promptly
        running = false;
        continue;
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

    // Sync actions
    if (input.attached) {
      XrActiveActionSet aas{input.set, XR_NULL_PATH};
      XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
      sync.countActiveActionSets = 1;
      sync.activeActionSets = &aas;
      xrSyncActions(session, &sync);
    }

    // Locate VIEW (head) pose in LOCAL — modelview for stereo panel
    XrPosef viewPoseLocal = IdentityPose();
    bool viewPoseOk = false;
    if (viewSpace) {
      XrSpaceLocation vloc{XR_TYPE_SPACE_LOCATION};
      if (XR_SUCCEEDED(xrLocateSpace(viewSpace, space, fs.predictedDisplayTime, &vloc))) {
        const XrSpaceLocationFlags need =
            XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        if ((vloc.locationFlags & need) == need) {
          viewPoseLocal = vloc.pose;
          viewPoseOk = true;
        }
      }
    }

    // World-lock: first valid head pose freezes default panel into LOCAL
    if (worldInitPending && viewPoseOk) {
      PanelPose viewDefaults = g_pose; // still VIEW-style from config
      SetPanelFromView(viewDefaults, viewPoseLocal, true);
      worldInitPending = false;
      fprintf(stderr, "[cube_webui] world-lock seeded from default pose\n");
    }

    // Panel pose in VIEW for this frame (world-lock converts LOCAL→VIEW)
    PanelPose panelView = PanelPoseInView(viewPoseLocal, viewPoseOk);

    // Aim in VIEW — same space as panel + laser
    viewAimValid = false;
    viewHit = false;
    int hitPx = 0, hitPy = 0;
    viewHitPt = V3(panelView.cx, panelView.cy, panelView.cz);
    if (input.attached && input.aimSpace && viewSpace) {
      XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
      if (XR_SUCCEEDED(xrLocateSpace(input.aimSpace, viewSpace, fs.predictedDisplayTime, &loc))) {
        if (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
          viewAimO = {loc.pose.position.x, loc.pose.position.y, loc.pose.position.z};
          viewAimValid = true;
          Vec3 dNeg = Normalize(QuatRotate(loc.pose.orientation, V3(0, 0, -1)));
          Vec3 dPos = Normalize(QuatRotate(loc.pose.orientation, V3(0, 0, 1)));
          viewAimD = dNeg;
          viewHit = RayPanelHit(viewAimO, dNeg, panelView, &hitPx, &hitPy, &viewHitPt);
          if (!viewHit) {
            viewAimD = dPos;
            viewHit = RayPanelHit(viewAimO, dPos, panelView, &hitPx, &hitPy, &viewHitPt);
          }
          if (!viewHit) viewAimD = dNeg;
          if (viewHit && (frame % 60) == 0)
            fprintf(stderr, "[cube_webui] laser hit %d,%d tip=(%.2f,%.2f,%.2f) grab=%d\n",
                    hitPx, hitPy, viewHitPt.x, viewHitPt.y, viewHitPt.z, grabbing ? 1 : 0);
        }
      }
    }

    // Grip = grab/move menu (panel stays face-on; position follows controller)
    float grabVal = 0.f;
    if (input.attached && input.grab) {
      XrActionStateFloat ft{XR_TYPE_ACTION_STATE_FLOAT};
      XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
      gi.action = input.grab;
      if (XR_SUCCEEDED(xrGetActionStateFloat(session, &gi, &ft)) && ft.isActive)
        grabVal = ft.currentState;
    }
    const bool grabHeld = grabVal >= g_cfg.grabThresh;
    if (grabHeld && viewAimValid) {
      if (!grabbing) {
        // Start grab when aiming at panel, or if already near center (re-grab)
        bool canStart = viewHit;
        if (!canStart) {
          float dx = viewAimO.x - panelView.cx;
          float dy = viewAimO.y - panelView.cy;
          float dz = viewAimO.z - panelView.cz;
          float dist2 = dx * dx + dy * dy + dz * dz;
          canStart = dist2 < 1.2f * 1.2f; // within ~1.2m of panel center
        }
        if (canStart) {
          grabbing = true;
          grabOff = V3(panelView.cx, panelView.cy, panelView.cz) - viewAimO;
          ui.status = "MOVING MENU — release grip to place";
        }
      }
      if (grabbing) {
        PanelPose np;
        np.cx = viewAimO.x + grabOff.x;
        np.cy = viewAimO.y + grabOff.y;
        np.cz = viewAimO.z + grabOff.z;
        // Keep panel in front of head, sensible range
        if (np.cz > -0.35f) np.cz = -0.35f;
        if (np.cz < -3.5f) np.cz = -3.5f;
        if (np.cx < -1.8f) np.cx = -1.8f;
        if (np.cx > 1.8f) np.cx = 1.8f;
        if (np.cy < -1.5f) np.cy = -1.5f;
        if (np.cy > 1.5f) np.cy = 1.5f;
        SetPanelFromView(np, viewPoseLocal, viewPoseOk);
        panelView = PanelPoseInView(viewPoseLocal, viewPoseOk);
        // Refresh hit under new pose
        viewHit = RayPanelHit(viewAimO, viewAimD, panelView, &hitPx, &hitPy, &viewHitPt);
      }
    } else if (grabbing) {
      grabbing = false;
      ui.status = g_cfg.viewLock ? "PLACED (head-lock)" : "PLACED (world-lock)";
      fprintf(stderr, "[cube_webui] panel placed view=(%.2f,%.2f,%.2f) view_lock=%d\n",
              panelView.cx, panelView.cy, panelView.cz, g_cfg.viewLock ? 1 : 0);
    }

    if (viewAimValid && viewHit && !grabbing)
      WebUI_SetCursor(ui, hitPx, hitPy, true);
    else if (grabbing)
      WebUI_SetCursor(ui, UI_W / 2, UI_H / 2, false);
    else
      WebUI_SetCursor(ui, 0, 0, false);

    // Eye poses (LOCAL) for projection + VIEW→eye modelview
    XrViewLocateInfo vli0{XR_TYPE_VIEW_LOCATE_INFO};
    vli0.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    vli0.displayTime = fs.predictedDisplayTime;
    vli0.space = space;
    XrViewState vs0{XR_TYPE_VIEW_STATE};
    uint32_t vc0 = 0;
    XrView views0[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    xrLocateViews(session, &vli0, &vs0, 2, &vc0, views0);

    bool trig = false;
    bool menuBtn = false;
    if (input.attached) {
      XrActionStateBoolean st{XR_TYPE_ACTION_STATE_BOOLEAN};
      XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
      gi.action = input.trigger;
      if (XR_SUCCEEDED(xrGetActionStateBoolean(session, &gi, &st)) && st.isActive)
        trig = st.currentState;
      XrActionStateFloat ft{XR_TYPE_ACTION_STATE_FLOAT};
      gi.action = input.triggerAxis;
      if (XR_SUCCEEDED(xrGetActionStateFloat(session, &gi, &ft)) && ft.isActive)
        trig = trig || (ft.currentState > 0.55f);
      gi.action = input.menu;
      if (XR_SUCCEEDED(xrGetActionStateBoolean(session, &gi, &st)) && st.isActive)
        menuBtn = st.currentState;
    }
    // MENU edge = reset panel to config defaults (not click)
    if (menuBtn && !prevMenu) {
      ResetPanelPoseFromConfig();
      grabbing = false;
      worldInitPending = !g_cfg.viewLock;
      ui.status = "PANEL RESET";
      fprintf(stderr, "[cube_webui] panel reset to config defaults\n");
    }
    prevMenu = menuBtn;

    // No clicks while grabbing
    if (trig && !prevTrigger && !grabbing) {
      if (viewHit) {
        WebUI_PointerClick(ui, hitPx, hitPy);
        fprintf(stderr, "[cube_webui] click %d,%d\n", hitPx, hitPy);
      } else if (ui.cursorVisible) {
        WebUI_PointerClick(ui, ui.cursorX, ui.cursorY);
      } else {
        WebUI_Input(ui, 0, 0, true, false);
        fprintf(stderr, "[cube_webui] click without laser hit (stick/focus fallback)\n");
      }
    }
    prevTrigger = trig;

    // Stick: while grab → fine depth/offset; else UI nav
    if (input.attached) {
      XrActionStateVector2f st{XR_TYPE_ACTION_STATE_VECTOR2F};
      XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
      gi.action = input.stick;
      if (XR_SUCCEEDED(xrGetActionStateVector2f(session, &gi, &st)) && st.isActive) {
        if (grabbing) {
          // Y: closer/farther, X: left/right nudge while holding
          const float spd = 0.012f;
          PanelPose np = panelView;
          np.cz -= st.currentState.y * spd; // stick up → closer (less -Z)
          np.cx += st.currentState.x * spd;
          if (np.cz > -0.35f) np.cz = -0.35f;
          if (np.cz < -3.5f) np.cz = -3.5f;
          SetPanelFromView(np, viewPoseLocal, viewPoseOk);
          panelView = PanelPoseInView(viewPoseLocal, viewPoseOk);
          grabOff = V3(panelView.cx, panelView.cy, panelView.cz) - viewAimO;
        } else if (stickCooldown <= 0.f) {
          int sx = 0, sy = 0;
          if (st.currentState.x < -0.55f) sx = -1;
          if (st.currentState.x > 0.55f) sx = 1;
          if (st.currentState.y < -0.55f) sy = 1;
          if (st.currentState.y > 0.55f) sy = -1;
          if (sx || sy) {
            WebUI_Input(ui, sx, sy, false, false);
            stickCooldown = 0.22f;
          }
        }
      }
    }
    if (stickCooldown > 0.f) stickCooldown -= 1.f / 72.f;

    std::vector<XrCompositionLayerProjectionView> projViews;
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space = space;
    if (useAlphaClear)
      layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;

    XrCompositionLayerPassthroughFB ptComp{};
    if (ptLayerHandle != XR_NULL_HANDLE) {
      ptComp = {XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB};
      ptComp.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
      ptComp.space = space;
      ptComp.layerHandle = ptLayerHandle;
    }

    if (fs.shouldRender) {
      WebUICursor cur{ui.cursorVisible, ui.cursorX, ui.cursorY};
      WebUI_Rasterize(ui, panelBuf.data(), &cur);
      glBindTexture(GL_TEXTURE_2D, panelTex);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, UI_W, UI_H, GL_RGBA, GL_UNSIGNED_BYTE, panelBuf.data());

      XrView views[2] = {views0[0], views0[1]};
      uint32_t vc = vc0;

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
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        if (useAlphaClear)
          glClearColor(0.f, 0.f, 0.f, 0.f); // see-through
        else
          glClearColor(0.04f, 0.02f, 0.03f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        const float tanL = tanf(views[eye].fov.angleLeft);
        const float tanR = tanf(views[eye].fov.angleRight);
        const float tanU = tanf(views[eye].fov.angleUp);
        const float tanD = tanf(views[eye].fov.angleDown);
        const float n = 0.05f, farZ = 50.f;
        float m[16] = {};
        const float invRL = 1.f / (tanR - tanL);
        const float invUD = 1.f / (tanU - tanD);
        m[0]  = 2.f * invRL;
        m[5]  = 2.f * invUD;
        m[8]  = (tanR + tanL) * invRL;
        m[9]  = (tanU + tanD) * invUD;
        m[10] = -(farZ + n) / (farZ - n);
        m[11] = -1.f;
        m[14] = -(2.f * farZ * n) / (farZ - n);
        glLoadMatrixf(m);

        glMatrixMode(GL_MODELVIEW);
        if (viewPoseOk)
          LoadModelviewViewSpace(views[eye].pose, viewPoseLocal);
        else
          glLoadIdentity();

        DrawPanelAt(panelTex, panelView);

        if (viewAimValid) {
          Vec3 tip = viewHit ? viewHitPt : (viewAimO + viewAimD * 2.5f);
          // Brighter laser while grabbing
          float cr = grabbing ? 0.3f : (viewHit ? 1.f : 0.45f);
          float cg = grabbing ? 0.9f : (viewHit ? 0.2f : 0.45f);
          float cb = grabbing ? 0.4f : (viewHit ? 0.35f : 0.5f);
          DrawLaserView(viewAimO, tip, cr, cg, cb);
        }

        if (glBindFramebuffer_) glBindFramebuffer_(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);

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
      (void)vc;
    }
    ++frame;

    XrFrameEndInfo fei{XR_TYPE_FRAME_END_INFO};
    fei.displayTime = fs.predictedDisplayTime;
    fei.environmentBlendMode = blendMode;
    const XrCompositionLayerBaseHeader* layers[2] = {};
    uint32_t layerCount = 0;
    if (fs.shouldRender && !projViews.empty()) {
      if (ptLayerHandle != XR_NULL_HANDLE)
        layers[layerCount++] = (const XrCompositionLayerBaseHeader*)&ptComp;
      layers[layerCount++] = (const XrCompositionLayerBaseHeader*)&layer;
      fei.layerCount = layerCount;
      fei.layers = layers;
    }
    xrEndFrame(session, &fei);
  }

  for (int i = 0; i < 2; ++i)
    if (eyes[i].swap) xrDestroySwapchain(eyes[i].swap);
  if (ptLayerHandle != XR_NULL_HANDLE && pfnDestroyPtLayer) pfnDestroyPtLayer(ptLayerHandle);
  if (passthrough != XR_NULL_HANDLE) {
    if (pfnPausePt) pfnPausePt(passthrough);
    if (pfnDestroyPt) pfnDestroyPt(passthrough);
  }
  if (input.aimSpace) xrDestroySpace(input.aimSpace);
  if (viewSpace) xrDestroySpace(viewSpace);
  if (space) xrDestroySpace(space);
  if (session) xrDestroySession(session);
  if (input.set) xrDestroyActionSet(input.set);
  if (instance) xrDestroyInstance(instance);
  if (panelTex) glDeleteTextures(1, &panelTex);
  DestroyGlx(glx);
  fprintf(stderr, "[cube_webui] exit\n");
  return 0;
}
