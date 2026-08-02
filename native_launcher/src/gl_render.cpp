#include "gl_render.hpp"
#include "panel_config.hpp"

#include <GL/glx.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers_ = nullptr;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_ = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_ = nullptr;

void GlLoadFboProcs() {
  if (glGenFramebuffers_) return;
  glGenFramebuffers_ = (PFNGLGENFRAMEBUFFERSPROC)glXGetProcAddress((const GLubyte*)"glGenFramebuffers");
  glBindFramebuffer_ = (PFNGLBINDFRAMEBUFFERPROC)glXGetProcAddress((const GLubyte*)"glBindFramebuffer");
  glFramebufferTexture2D_ = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glXGetProcAddress((const GLubyte*)"glFramebufferTexture2D");
}

bool GlBindSwapchainFbo(GLuint colorTex, GLuint* fboOut) {
  GlLoadFboProcs();
  if (!glGenFramebuffers_ || !glBindFramebuffer_ || !glFramebufferTexture2D_) return false;
  if (fboOut && !*fboOut) glGenFramebuffers_(1, fboOut);
  if (!fboOut || !*fboOut) return false;
  glBindFramebuffer_((GLenum)0x8D40 /*GL_FRAMEBUFFER*/, *fboOut);
  glFramebufferTexture2D_((GLenum)0x8D40, (GLenum)0x8CE0 /*COLOR0*/, GL_TEXTURE_2D, colorTex, 0);
  return true;
}

void GlUnbindFbo() {
  if (glBindFramebuffer_) glBindFramebuffer_((GLenum)0x8D40, 0);
}

GLuint GlMakeRgbaTex(int w, int h, const void* rgba) {
  GLuint t = 0;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  return t;
}

void GlUpdateRgbaTex(GLuint tex, int w, int h, const void* rgba) {
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
}

// View matrix from OpenXR eye pose (pose = eye in WORLD).
// world_from_eye = T(p) * R(q);  view = inverse = R^T * T(-p)
void GlLoadModelviewLocal(const XrPosef& eyeWorld) {
  const float x = eyeWorld.orientation.x;
  const float y = eyeWorld.orientation.y;
  const float z = eyeWorld.orientation.z;
  const float w = eyeWorld.orientation.w;
  // R = rotation world ← eye (columns = eye axes in world)
  const float r00 = 1.f - 2.f * (y * y + z * z);
  const float r01 = 2.f * (x * y - z * w);
  const float r02 = 2.f * (x * z + y * w);
  const float r10 = 2.f * (x * y + z * w);
  const float r11 = 1.f - 2.f * (x * x + z * z);
  const float r12 = 2.f * (y * z - x * w);
  const float r20 = 2.f * (x * z - y * w);
  const float r21 = 2.f * (y * z + x * w);
  const float r22 = 1.f - 2.f * (x * x + y * y);

  const float px = eyeWorld.position.x;
  const float py = eyeWorld.position.y;
  const float pz = eyeWorld.position.z;
  // -R^T * p
  const float tx = -(r00 * px + r10 * py + r20 * pz);
  const float ty = -(r01 * px + r11 * py + r21 * pz);
  const float tz = -(r02 * px + r12 * py + r22 * pz);

  // Column-major OpenGL: upper 3x3 = R^T
  // R^T = | r00 r10 r20 |
  //       | r01 r11 r21 |
  //       | r02 r12 r22 |
  const float M[16] = {
      r00, r01, r02, 0.f, // col0
      r10, r11, r12, 0.f, // col1
      r20, r21, r22, 0.f, // col2
      tx,  ty,  tz,  1.f,
  };
  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixf(M);
}

void GlLoadProjectionFov(const XrFovf& fov, float nearZ, float farZ) {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  // OpenXR FOV angles: tan of half-angles from optical axis (can be asymmetric)
  const float tanL = tanf(fov.angleLeft);
  const float tanR = tanf(fov.angleRight);
  const float tanU = tanf(fov.angleUp);
  const float tanD = tanf(fov.angleDown);
  // angleLeft is typically negative; use as OpenXR specifies
  const float L = tanL * nearZ;
  const float R = tanR * nearZ;
  const float B = tanD * nearZ;
  const float T = tanU * nearZ;
  // glFrustum(left, right, bottom, top, near, far)
  glFrustum(L, R, B, T, nearZ, farZ);
}

void GlDrawWorldPanel(GLuint tex, const XrPosef& eyeWorld) {
  const auto& wp = WorldPanelState();
  if (!wp.ready) return;
  const auto& cfg = PanelCfgConst();
  const float hw = (wp.widthM > 0.05f) ? wp.widthM * 0.5f : cfg.halfW;
  const float hh = (wp.heightM > 0.05f) ? wp.heightM * 0.5f : cfg.halfH;
  const Vec3 bl = wp.c - wp.right * hw - wp.up * hh;
  const Vec3 br = wp.c + wp.right * hw - wp.up * hh;
  const Vec3 tr = wp.c + wp.right * hw + wp.up * hh;
  const Vec3 tl = wp.c - wp.right * hw + wp.up * hh;

  // Which side of the panel is the eye on?
  const Vec3 eyeP = {eyeWorld.position.x, eyeWorld.position.y, eyeWorld.position.z};
  const bool backFace = Dot(wp.c - eyeP, wp.normal) > 0.f;

  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tex);
  glColor4f(1.f, 1.f, 1.f, cfg.panelAlpha);

  // CPU y=0 = UI top. Raw glTexImage: row0 → V=0.
  // Empirical ADB on Quest/WiVRn: treat V=0 as UI top on panel TOP.
  // If backFace, flip U so we don't read mirrored.
  auto emit = [&](float u, float v, const Vec3& p) {
    glTexCoord2f(backFace ? (1.f - u) : u, v);
    glVertex3f(p.x, p.y, p.z);
  };
  // ADB series showed inverted text with (bl=V1,tl=V0). Swap V.
  glBegin(GL_QUADS);
  emit(0.f, 0.f, bl);
  emit(1.f, 0.f, br);
  emit(1.f, 1.f, tr);
  emit(0.f, 1.f, tl);
  glEnd();
  glDisable(GL_TEXTURE_2D);
  glColor4f(1.f, 1.f, 1.f, 1.f);
  glDisable(GL_BLEND);
}

void GlDrawLaser(Vec3 a, Vec3 b, float cr, float cg, float cb) {
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_DEPTH_TEST);
  glLineWidth(3.f);
  glBegin(GL_LINES);
  glColor3f(cr, cg, cb);
  glVertex3f(a.x, a.y, a.z);
  glVertex3f(b.x, b.y, b.z);
  glEnd();
  glPointSize(12.f);
  glBegin(GL_POINTS);
  glColor3f(1.f, 1.f, 1.f);
  glVertex3f(b.x, b.y, b.z);
  glEnd();
  glColor3f(1.f, 1.f, 1.f);
}
