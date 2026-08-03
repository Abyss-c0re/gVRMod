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
  glBindFramebuffer_((GLenum)0x8D40, *fboOut);
  glFramebufferTexture2D_((GLenum)0x8D40, (GLenum)0x8CE0, GL_TEXTURE_2D, colorTex, 0);
  return true;
}

void GlUnbindFbo() {
  if (glBindFramebuffer_) glBindFramebuffer_((GLenum)0x8D40, 0);
}

// Upload UI buffer as-is (y=0 = top of menu). Orientation is fixed only in UVs
// so ray-hit px/py stay 1:1 with paint coordinates (no invert / no 180 mess).
static void UploadRgbaRaw(GLuint tex, int w, int h, const void* rgba) {
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
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
  UploadRgbaRaw(tex, w, h, rgba);
}

void GlLoadModelviewLocal(const XrPosef& eyeWorld) {
  const float x = eyeWorld.orientation.x;
  const float y = eyeWorld.orientation.y;
  const float z = eyeWorld.orientation.z;
  const float w = eyeWorld.orientation.w;
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
  const float tx = -(r00 * px + r10 * py + r20 * pz);
  const float ty = -(r01 * px + r11 * py + r21 * pz);
  const float tz = -(r02 * px + r12 * py + r22 * pz);
  const float M[16] = {
      r00, r01, r02, 0.f, r10, r11, r12, 0.f, r20, r21, r22, 0.f, tx, ty, tz, 1.f,
  };
  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixf(M);
}

void GlLoadProjectionFov(const XrFovf& fov, float nearZ, float farZ) {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  const float L = tanf(fov.angleLeft) * nearZ;
  const float Rgt = tanf(fov.angleRight) * nearZ;
  float B = tanf(fov.angleDown) * nearZ;
  float T = tanf(fov.angleUp) * nearZ;
  if (B > T) {
    const float tmp = B;
    B = T;
    T = tmp;
  }
  // Standard OpenXR→GL frustum (no Y flip — 180° handled in texture)
  glFrustum(L, Rgt, B, T, nearZ, farZ);
}

void GlDrawWorldPanel(GLuint tex, const XrPosef& eyeWorld) {
  (void)eyeWorld;
  const auto& wp = WorldPanelState();
  if (!wp.ready) return;
  const auto& cfg = PanelCfgConst();
  const float hw = (wp.widthM > 0.05f) ? wp.widthM * 0.5f : cfg.halfW;
  const float hh = (wp.heightM > 0.05f) ? wp.heightM * 0.5f : cfg.halfH;
  const Vec3 bl = wp.c - wp.right * hw - wp.up * hh;
  const Vec3 br = wp.c + wp.right * hw - wp.up * hh;
  const Vec3 tr = wp.c + wp.right * hw + wp.up * hh;
  const Vec3 tl = wp.c - wp.right * hw + wp.up * hh;

  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tex);
  glColor4f(1.f, 1.f, 1.f, cfg.panelAlpha);
  // Front face toward +normal (user). Winding bl→br→tr→tl = CCW from front.
  // Raw buffer y=0 = UI top. GL V=0 = first row = UI top → put on panel +up (tl/tr).
  // U: left of panel = UI left (no mirror / not inside-out).
  glBegin(GL_QUADS);
  glTexCoord2f(0.f, 1.f); // bl — UI bottom-left
  glVertex3f(bl.x, bl.y, bl.z);
  glTexCoord2f(1.f, 1.f); // br — UI bottom-right
  glVertex3f(br.x, br.y, br.z);
  glTexCoord2f(1.f, 0.f); // tr — UI top-right
  glVertex3f(tr.x, tr.y, tr.z);
  glTexCoord2f(0.f, 0.f); // tl — UI top-left (NEW GAME)
  glVertex3f(tl.x, tl.y, tl.z);
  glEnd();
  glDisable(GL_TEXTURE_2D);
  glColor4f(1.f, 1.f, 1.f, 1.f);
  glDisable(GL_BLEND);
}

void GlDrawLaser(Vec3 a, Vec3 b, float cr, float cg, float cb) {
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  // One clean ray + one tip (no multi-glow "third wheel" points)
  glLineWidth(3.f);
  glBegin(GL_LINES);
  glColor4f(cr, cg, cb, 1.f);
  glVertex3f(a.x, a.y, a.z);
  glVertex3f(b.x, b.y, b.z);
  glEnd();
  glPointSize(14.f);
  glBegin(GL_POINTS);
  glColor4f(1.f, 1.f, 1.f, 1.f);
  glVertex3f(b.x, b.y, b.z);
  glEnd();
  glColor4f(1.f, 1.f, 1.f, 1.f);
  glDisable(GL_BLEND);
}
