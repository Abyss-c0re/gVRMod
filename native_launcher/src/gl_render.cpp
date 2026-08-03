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

// WiVRn shot 050018: without this, menu is fully upside-down + mirrored.
// Paint buffer: y=0 = NEW GAME (top). After 180° upload + standard UV, visual is upright.
// WorldPanelRayHit MUST apply the same 180° to px/py (see world_panel.cpp).
static void UploadRgbaRotated180(GLuint tex, int w, int h, const void* rgba) {
  static std::vector<unsigned char> rot;
  rot.resize((size_t)w * (size_t)h * 4);
  const auto* s = static_cast<const unsigned char*>(rgba);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const int si = (y * w + x) * 4;
      const int di = ((h - 1 - y) * w + (w - 1 - x)) * 4;
      rot[di + 0] = s[si + 0];
      rot[di + 1] = s[si + 1];
      rot[di + 2] = s[si + 2];
      rot[di + 3] = s[si + 3];
    }
  }
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rot.data());
}

GLuint GlMakeRgbaTex(int w, int h, const void* rgba) {
  GLuint t = 0;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  if (rgba) UploadRgbaRotated180(t, w, h, rgba);
  return t;
}

void GlUpdateRgbaTex(GLuint tex, int w, int h, const void* rgba) {
  UploadRgbaRotated180(tex, w, h, rgba);
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
  // 180° already applied in upload — identity UV, front winding.
  glBegin(GL_QUADS);
  glTexCoord2f(0.f, 0.f);
  glVertex3f(bl.x, bl.y, bl.z);
  glTexCoord2f(1.f, 0.f);
  glVertex3f(br.x, br.y, br.z);
  glTexCoord2f(1.f, 1.f);
  glVertex3f(tr.x, tr.y, tr.z);
  glTexCoord2f(0.f, 1.f);
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
  glLineWidth(3.f);
  glBegin(GL_LINES);
  glColor4f(cr, cg, cb, 1.f);
  glVertex3f(a.x, a.y, a.z);
  glVertex3f(b.x, b.y, b.z);
  glEnd();
  glPointSize(16.f);
  glBegin(GL_POINTS);
  glColor4f(1.f, 1.f, 1.f, 1.f);
  glVertex3f(b.x, b.y, b.z);
  glEnd();
  glColor4f(1.f, 1.f, 1.f, 1.f);
  glDisable(GL_BLEND);
}
