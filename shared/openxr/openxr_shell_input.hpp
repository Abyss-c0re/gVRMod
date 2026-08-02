#pragma once
// Minimal Cube shell controller set: aim laser, trigger, grip move, stick, menu.
// Both hands are first-class: actions use L/R subaction paths; UI can fire async.
#include "openxr_bindings.hpp"
#include <openxr/openxr.h>

namespace cube_xr {

enum class Hand : int { Left = 0, Right = 1 };

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

// Per-hand (async dual-hand UI). Prefer these for laser/click/grab.
bool ShellInputReadTriggerHand(const XrApi& api, XrSession session, const ShellInput& in,
                               Hand hand, float axisThresh = 0.55f);
float ShellInputReadGrabHand(const XrApi& api, XrSession session, const ShellInput& in,
                             Hand hand);
bool ShellInputLocateAimHand(const XrApi& api, XrSession session, const ShellInput& in,
                             Hand hand, XrSpace base, XrTime time, XrPosef* outPose);
bool ShellInputReadStickHand(const XrApi& api, XrSession session, const ShellInput& in,
                             Hand hand, float* outX, float* outY);

// Combined: true / max if EITHER hand satisfies (legacy + hotkeys).
bool ShellInputReadTrigger(const XrApi& api, XrSession session, const ShellInput& in,
                           float axisThresh = 0.55f);
float ShellInputReadGrab(const XrApi& api, XrSession session, const ShellInput& in);
bool ShellInputReadMenu(const XrApi& api, XrSession session, const ShellInput& in);
bool ShellInputReadStick(const XrApi& api, XrSession session, const ShellInput& in,
                         float* outX, float* outY);
// Best tracked aim (prefer higher tracking score) — for single-ray draw only.
bool ShellInputLocateAim(const XrApi& api, XrSession session, const ShellInput& in,
                         XrSpace base, XrTime time, XrPosef* outPose);

} // namespace cube_xr
