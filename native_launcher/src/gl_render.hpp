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

void GlDrawWorldPanel(GLuint tex);
void GlDrawLaser(Vec3 a, Vec3 b, float cr, float cg, float cb);
