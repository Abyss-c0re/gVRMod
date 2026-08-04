#pragma once
#include "math3d.hpp"
#include "world_panel.hpp"
#include <GL/gl.h>
#include <openxr/openxr.h>

// GL helpers for Cube WebUI stereo render path.
void GlLoadFboProcs();
bool GlBindSwapchainFbo(GLuint colorTex, GLuint* fboOut);
void GlUnbindFbo();
GLuint GlMakeRgbaTex(int w, int h, const void* rgba);
void GlUpdateRgbaTex(GLuint tex, int w, int h, const void* rgba);

// Modelview: LOCAL verts → eye camera.
void GlLoadModelviewLocal(const XrPosef& eyeLocal);

// Projection from OpenXR FOV (tan-space, no near factor bug).
void GlLoadProjectionFov(const XrFovf& fov, float nearZ = 0.05f, float farZ = 50.f);

// eyeWorld: current eye pose so we can UV-correct when viewing the back face
void GlDrawWorldPanel(GLuint tex, const XrPosef& eyeWorld);
void GlDrawLaser(Vec3 a, Vec3 b, float cr, float cg, float cb);

// G02: full-eye black overlay after content (panel + lasers). fade 0..1.
// Covers the whole swapchain view so take_xr feels intentional before release.
void GlFadeEyeBufferTowardBlack(float fade);
