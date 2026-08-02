#include "gl_render.hpp"
#include "panel_config.hpp"

#include <GL/glx.h>
#include <cmath>
#include <cstdio>

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

void GlLoadModelviewLocal(const XrPosef& eyeLocal) {
  Vec3 t = {eyeLocal.position.x, eyeLocal.position.y, eyeLocal.position.z};
  float x = eyeLocal.orientation.x, y = eyeLocal.orientation.y, z = eyeLocal.orientation.z, w = eyeLocal.orientation.w;
  float R[9] = {
    1 - 2*y*y - 2*z*z, 2*x*y - 2*z*w, 2*x*z + 2*y*w,
    2*x*y + 2*z*w, 1 - 2*x*x - 2*z*z, 2*y*z - 2*x*w,
    2*x*z - 2*y*w, 2*y*z + 2*x*w, 1 - 2*x*x - 2*y*y
  };
  float M[16] = {
    R[0], R[3], R[6], 0,
    R[1], R[4], R[7], 0,
    R[2], R[5], R[8], 0,
    -(R[0]*t.x + R[3]*t.y + R[6]*t.z),
    -(R[1]*t.x + R[4]*t.y + R[7]*t.z),
    -(R[2]*t.x + R[5]*t.y + R[8]*t.z),
    1
  };
  glLoadMatrixf(M);
}

void GlLoadProjectionFov(const XrFovf& fov, float nearZ, float farZ) {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  const float tanL = tanf(fov.angleLeft);
  const float tanR = tanf(fov.angleRight);
  const float tanU = tanf(fov.angleUp);
  const float tanD = tanf(fov.angleDown);
  float m[16] = {};
  const float invRL = 1.f / (tanR - tanL);
  const float invUD = 1.f / (tanU - tanD);
  m[0]  = 2.f * invRL;
  m[5]  = 2.f * invUD;
  m[8]  = (tanR + tanL) * invRL;
  m[9]  = (tanU + tanD) * invUD;
  m[10] = -(farZ + nearZ) / (farZ - nearZ);
  m[11] = -1.f;
  m[14] = -(2.f * farZ * nearZ) / (farZ - nearZ);
  glLoadMatrixf(m);
}

void GlDrawWorldPanel(GLuint tex) {
  const auto& wp = WorldPanelState();
  if (!wp.ready) return;
  const auto& cfg = PanelCfgConst();
  // Prefer frozen panel size (meters); fall back to conf
  const float hw = (wp.widthM > 0.05f) ? wp.widthM * 0.5f : cfg.halfW;
  const float hh = (wp.heightM > 0.05f) ? wp.heightM * 0.5f : cfg.halfH;
  Vec3 bl = wp.c - wp.right * hw - wp.up * hh;
  Vec3 br = wp.c + wp.right * hw - wp.up * hh;
  Vec3 tr = wp.c + wp.right * hw + wp.up * hh;
  Vec3 tl = wp.c - wp.right * hw + wp.up * hh;
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST); // always on top of clear, never lost behind floor
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tex);
  glColor4f(1.f, 1.f, 1.f, cfg.panelAlpha);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 1); glVertex3f(bl.x, bl.y, bl.z);
  glTexCoord2f(1, 1); glVertex3f(br.x, br.y, br.z);
  glTexCoord2f(1, 0); glVertex3f(tr.x, tr.y, tr.z);
  glTexCoord2f(0, 0); glVertex3f(tl.x, tl.y, tl.z);
  glEnd();
  glDisable(GL_TEXTURE_2D);
  glColor4f(1, 1, 1, 1);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
}

void GlDrawLaser(Vec3 a, Vec3 b, float cr, float cg, float cb) {
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
