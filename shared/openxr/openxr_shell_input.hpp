#pragma once
// Minimal Cube shell controller set: aim laser, trigger, grip move, stick, menu.
#include "openxr_bindings.hpp"
#include <openxr/openxr.h>

namespace cube_xr {

struct ShellInput {
  XrActionSet set = XR_NULL_HANDLE;
  XrAction pose = XR_NULL_HANDLE;
  XrAction trigger = XR_NULL_HANDLE;
  XrAction triggerAxis = XR_NULL_HANDLE;
  XrAction grab = XR_NULL_HANDLE;
  XrAction grabClick = XR_NULL_HANDLE;
  XrAction stick = XR_NULL_HANDLE;
  XrAction menu = XR_NULL_HANDLE;

  XrPath handLeft = XR_NULL_PATH;
  XrPath handRight = XR_NULL_PATH;
  XrSpace aimLeft = XR_NULL_HANDLE;
  XrSpace aimRight = XR_NULL_HANDLE;

  bool attached = false;
};

bool ShellInputSetup(const XrApi& api, XrSession session, ShellInput& in,
                     const char* actionSetName = "cube_shell");
void ShellInputDestroy(const XrApi& api, ShellInput& in);
void ShellInputSync(const XrApi& api, XrSession session, ShellInput& in);

bool ShellInputReadTrigger(const XrApi& api, XrSession session, const ShellInput& in,
                           float axisThresh = 0.55f);
float ShellInputReadGrab(const XrApi& api, XrSession session, const ShellInput& in);
bool ShellInputReadMenu(const XrApi& api, XrSession session, const ShellInput& in);
bool ShellInputReadStick(const XrApi& api, XrSession session, const ShellInput& in,
                         float* outX, float* outY);
// Locates best tracked hand aim (prefer right) in `base` space.
bool ShellInputLocateAim(const XrApi& api, XrSession session, const ShellInput& in,
                         XrSpace base, XrTime time, XrPosef* outPose);

} // namespace cube_xr
