#include "xr_input.hpp"

static cube_xr::XrApi g_api{};

bool XrInputSetup(XrInstance instance, XrSession session, XrInputState& in) {
  g_api = cube_xr::XrApiLinked(instance);
  return cube_xr::ShellInputSetup(g_api, session, in, "cube_shell");
}

void XrInputDestroy(XrInputState& in) {
  cube_xr::ShellInputDestroy(g_api, in);
}

void XrInputSync(XrSession session, XrInputState& in) {
  cube_xr::ShellInputSync(g_api, session, in);
}

bool XrInputReadTrigger(XrSession session, const XrInputState& in, float axisThresh) {
  return cube_xr::ShellInputReadTrigger(g_api, session, in, axisThresh);
}

float XrInputReadGrab(XrSession session, const XrInputState& in) {
  return cube_xr::ShellInputReadGrab(g_api, session, in);
}

bool XrInputReadMenu(XrSession session, const XrInputState& in) {
  return cube_xr::ShellInputReadMenu(g_api, session, in);
}

bool XrInputReadStick(XrSession session, const XrInputState& in, float* outX, float* outY) {
  return cube_xr::ShellInputReadStick(g_api, session, in, outX, outY);
}

bool XrInputLocateAim(XrSession session, const XrInputState& in, XrSpace base,
                      XrTime time, XrPosef* outPose) {
  return cube_xr::ShellInputLocateAim(g_api, session, in, base, time, outPose);
}
