#pragma once
// Thin launcher façade over shared/openxr shell input.
#include "openxr_shell_input.hpp"
#include <openxr/openxr.h>

// Back-compat names used by xr_app.cpp
using XrInputState = cube_xr::ShellInput;
using XrHand = cube_xr::Hand;

bool XrInputSetup(XrInstance instance, XrSession session, XrInputState& in);
void XrInputDestroy(XrInputState& in);
void XrInputSync(XrSession session, XrInputState& in);

bool XrInputReadTriggerHand(XrSession session, const XrInputState& in, XrHand hand,
                            float axisThresh = 0.55f);
float XrInputReadGrabHand(XrSession session, const XrInputState& in, XrHand hand);
bool XrInputLocateAimHand(XrSession session, const XrInputState& in, XrHand hand,
                          XrSpace base, XrTime time, XrPosef* outPose);
bool XrInputReadStickHand(XrSession session, const XrInputState& in, XrHand hand,
                          float* outX, float* outY);

bool XrInputReadTrigger(XrSession session, const XrInputState& in, float axisThresh = 0.55f);
float XrInputReadGrab(XrSession session, const XrInputState& in);
bool XrInputReadMenu(XrSession session, const XrInputState& in);
bool XrInputReadStick(XrSession session, const XrInputState& in, float* outX, float* outY);
bool XrInputLocateAim(XrSession session, const XrInputState& in, XrSpace base,
                      XrTime time, XrPosef* outPose);
