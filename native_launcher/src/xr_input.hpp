#pragma once
#include "math3d.hpp"
#include <openxr/openxr.h>

// Controller action set: aim, trigger, grab, stick, menu.
struct XrInputState {
  XrActionSet set = XR_NULL_HANDLE;
  XrAction pose = XR_NULL_HANDLE;
  XrAction trigger = XR_NULL_HANDLE;
  XrAction triggerAxis = XR_NULL_HANDLE;
  XrAction grab = XR_NULL_HANDLE;
  XrAction grabClick = XR_NULL_HANDLE;
  XrAction stick = XR_NULL_HANDLE;
  XrAction menu = XR_NULL_HANDLE;
  XrSpace aimSpace = XR_NULL_HANDLE;
  bool attached = false;
};

bool XrInputSetup(XrInstance instance, XrSession session, XrInputState& in);
void XrInputDestroy(XrInputState& in);

void XrInputSync(XrSession session, XrInputState& in);

// Poll helpers (after Sync).
bool XrInputReadTrigger(XrSession session, const XrInputState& in, float axisThresh = 0.55f);
float XrInputReadGrab(XrSession session, const XrInputState& in); // 0..1, includes click→1
bool XrInputReadMenu(XrSession session, const XrInputState& in);
bool XrInputReadStick(XrSession session, const XrInputState& in, float* outX, float* outY);

// Locate aim pose in a reference space (LOCAL).
bool XrInputLocateAim(XrSession session, const XrInputState& in, XrSpace base,
                      XrTime time, XrPosef* outPose);
