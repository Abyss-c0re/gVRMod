#pragma once

// Platform texture share entry points used by lua_interface / OpenXR submit.
#ifdef _WIN32
#include "rendering/d3d/d3d_hooks.h"
#else
#include "rendering/opengl/gl_hooks.h"
#endif
