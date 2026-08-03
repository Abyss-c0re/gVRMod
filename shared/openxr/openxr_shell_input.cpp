#include "openxr_shell_input.hpp"
#include "openxr_paths.hpp"
#include <cstdio>
#include <cstring>

namespace cube_xr {

// Create any action type with both hand subaction paths so GetState can select L/R.
static XrResult CreateActionWithHands(const XrApi& api, XrActionSet set, const char* name,
                                      const char* loc, XrActionType ty, XrPath hands[2],
                                      XrAction* out) {
  XrActionCreateInfo aci{XR_TYPE_ACTION_CREATE_INFO};
  aci.actionType = ty;
  std::strncpy(aci.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
  std::strncpy(aci.localizedActionName, loc, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
  aci.countSubactionPaths = 2;
  aci.subactionPaths = hands;
  if (api.createAction) return api.createAction(set, &aci, out);
  return xrCreateAction(set, &aci, out);
}

static XrResult CreateActionSpaceForHand(const XrApi& api, XrSession session, XrAction poseAction,
                                         XrPath hand, XrSpace* out) {
  XrActionSpaceCreateInfo sci{XR_TYPE_ACTION_SPACE_CREATE_INFO};
  sci.action = poseAction;
  sci.subactionPath = hand;
  sci.poseInActionSpace.orientation.w = 1.f;
  if (api.createActionSpace) return api.createActionSpace(session, &sci, out);
  return xrCreateActionSpace(session, &sci, out);
}

static XrPath HandPath(const ShellInput& in, Hand hand) {
  return hand == Hand::Left ? in.handLeft : in.handRight;
}

bool ShellInputSetup(const XrApi& api, XrSession session, ShellInput& in,
                     const char* actionSetName) {
  if (CreateActionSet(api, actionSetName, "Cube Shell", &in.set) != XR_SUCCESS)
    return false;

  if (!StringToPath(api, "/user/hand/left", &in.handLeft) ||
      !StringToPath(api, "/user/hand/right", &in.handRight)) {
    fprintf(stderr, "[cube_xr] hand paths failed\n");
    return false;
  }
  XrPath hands[2] = {in.handLeft, in.handRight};

  auto mkH = [&](const char* name, const char* loc, XrActionType ty, XrAction* out) {
    return CreateActionWithHands(api, in.set, name, loc, ty, hands, out) == XR_SUCCESS;
  };

  if (!mkH("aim_pose", "Aim Pose", XR_ACTION_TYPE_POSE_INPUT, &in.pose)) return false;
  // Boolean trigger unused on Oculus (no trigger/click) — float axis is primary.
  if (!mkH("trigger", "Trigger Click", XR_ACTION_TYPE_BOOLEAN_INPUT, &in.trigger)) return false;
  if (!mkH("trigger_axis", "Trigger Axis", XR_ACTION_TYPE_FLOAT_INPUT, &in.triggerAxis))
    return false;
  if (!mkH("grab", "Grab", XR_ACTION_TYPE_FLOAT_INPUT, &in.grab)) return false;
  if (!mkH("grab_click", "Grab Click", XR_ACTION_TYPE_BOOLEAN_INPUT, &in.grabClick)) return false;
  if (!mkH("stick", "Thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, &in.stick)) return false;
  // Menu is typically left-hand only on Quest; still create with subactions for Index.
  if (!mkH("menu", "Menu", XR_ACTION_TYPE_BOOLEAN_INPUT, &in.menu)) return false;

  XrActionSuggestedBinding binds[32];
  int n = 0;
  auto push = [&](XrAction a, const char* p) { PushBinding(api, binds, &n, 32, a, p); };

  // ── Oculus / Quest Touch: NO trigger/click, NO squeeze/click ──
  // Both hands fully bound — async dual-hand laser + trigger.
  n = 0;
  push(in.pose, path::rightAimPose);
  push(in.pose, path::leftAimPose);
  push(in.triggerAxis, path::rightTriggerValue);
  push(in.triggerAxis, path::leftTriggerValue);
  push(in.grab, path::rightSqueezeValue);
  push(in.grab, path::leftSqueezeValue);
  push(in.stick, path::rightThumbstick);
  push(in.stick, path::leftThumbstick);
  push(in.menu, path::leftMenuClick);
  // Face buttons + stick click = UI click (WiVRn often has dead trigger floats)
  push(in.trigger, path::rightAClick);
  push(in.trigger, path::leftXClick);
  push(in.trigger, path::rightThumbClick);
  push(in.trigger, path::leftThumbClick);
  if (!SuggestProfile(api, kProfileOculusTouch, binds, (uint32_t)n)) {
    fprintf(stderr, "[cube_xr] FATAL: oculus/touch_controller bindings failed\n");
  } else {
    fprintf(stderr, "[cube_xr] oculus/touch_controller bindings OK (n=%d, dual-hand)\n", n);
  }

  // ── Index (has trigger/click + squeeze) — both hands ──
  n = 0;
  push(in.pose, path::rightAimPose);
  push(in.pose, path::leftAimPose);
  push(in.trigger, path::rightTriggerClick);
  push(in.trigger, path::leftTriggerClick);
  push(in.triggerAxis, path::rightTriggerValue);
  push(in.triggerAxis, path::leftTriggerValue);
  push(in.grab, path::rightSqueezeValue);
  push(in.grab, path::leftSqueezeValue);
  // Index has squeeze/value only (no squeeze/click on this profile)
  push(in.stick, path::rightThumbstick);
  push(in.stick, path::leftThumbstick);
  push(in.menu, path::rightAClick);
  SuggestProfile(api, kProfileIndex, binds, (uint32_t)n);

  // ── KHR simple — both hands where available ──
  n = 0;
  push(in.pose, path::rightAimPose);
  push(in.pose, path::leftAimPose);
  push(in.trigger, path::rightSelectClick);
  push(in.trigger, path::leftSelectClick);
  push(in.menu, path::leftMenuClick);
  SuggestProfile(api, kProfileKhrSimple, binds, (uint32_t)n);

  // Skip Touch Pro / Plus unless extensions enabled (they break suggest with -22)

  if (AttachActionSets(api, session, &in.set, 1) != XR_SUCCESS) {
    fprintf(stderr, "[cube_xr] attach action sets failed\n");
    return false;
  }
  if (CreateActionSpaceForHand(api, session, in.pose, in.handRight, &in.aimRight) != XR_SUCCESS ||
      CreateActionSpaceForHand(api, session, in.pose, in.handLeft, &in.aimLeft) != XR_SUCCESS) {
    fprintf(stderr, "[cube_xr] aim spaces failed\n");
    return false;
  }
  in.attached = true;
  fprintf(stderr, "[cube_xr] shell input ready (async L+R subaction trigger/grab/aim)\n");
  return true;
}

void ShellInputDestroy(const XrApi& api, ShellInput& in) {
  auto destroySp = [&](XrSpace s) {
    if (!s) return;
    if (api.destroySpace) api.destroySpace(s);
    else xrDestroySpace(s);
  };
  destroySp(in.aimLeft);
  destroySp(in.aimRight);
  if (in.set) {
    if (api.destroyActionSet) api.destroyActionSet(in.set);
    else xrDestroyActionSet(in.set);
  }
  in = {};
}

void ShellInputSync(const XrApi& api, XrSession session, ShellInput& in) {
  if (!in.attached) return;
  XrActiveActionSet aas{in.set, XR_NULL_PATH};
  XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
  sync.countActiveActionSets = 1;
  sync.activeActionSets = &aas;
  if (api.syncActions) api.syncActions(session, &sync);
  else xrSyncActions(session, &sync);
}

// WiVRn often leaves isActive=false while currentState is live — read state anyway.
static bool GetBool(const XrApi& api, XrSession session, XrAction a, XrPath sub, bool* out) {
  if (a == XR_NULL_HANDLE) return false;
  XrActionStateBoolean st{XR_TYPE_ACTION_STATE_BOOLEAN};
  XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
  gi.action = a;
  gi.subactionPath = sub;
  XrResult r = api.getBool ? api.getBool(session, &gi, &st) : xrGetActionStateBoolean(session, &gi, &st);
  if (r != XR_SUCCESS) return false;
  if (out) *out = st.currentState != XR_FALSE;
  return st.isActive || st.currentState;
}

static bool GetFloat(const XrApi& api, XrSession session, XrAction a, XrPath sub, float* out) {
  if (a == XR_NULL_HANDLE) return false;
  XrActionStateFloat st{XR_TYPE_ACTION_STATE_FLOAT};
  XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
  gi.action = a;
  gi.subactionPath = sub;
  XrResult r = api.getFloat ? api.getFloat(session, &gi, &st) : xrGetActionStateFloat(session, &gi, &st);
  if (r != XR_SUCCESS) return false;
  if (out) *out = st.currentState;
  return st.isActive || st.currentState != 0.f;
}

bool ShellInputReadTriggerHand(const XrApi& api, XrSession session, const ShellInput& in,
                               Hand hand, float axisThresh) {
  if (!in.attached) return false;
  XrPath sub = HandPath(in, hand);
  bool b = false;
  if (GetBool(api, session, in.trigger, sub, &b) && b) return true;
  // Runtime may only update the action when queried without subaction
  if (GetBool(api, session, in.trigger, XR_NULL_PATH, &b) && b) return true;
  float f = 0.f;
  if (GetFloat(api, session, in.triggerAxis, sub, &f) && f > axisThresh) return true;
  if (GetFloat(api, session, in.triggerAxis, XR_NULL_PATH, &f) && f > axisThresh) return true;
  return false;
}

float ShellInputReadGrabHand(const XrApi& api, XrSession session, const ShellInput& in,
                             Hand hand) {
  if (!in.attached) return 0.f;
  XrPath sub = HandPath(in, hand);
  float g = 0.f, g2 = 0.f;
  GetFloat(api, session, in.grab, sub, &g);
  GetFloat(api, session, in.grab, XR_NULL_PATH, &g2);
  if (g2 > g) g = g2;
  bool click = false;
  if (GetBool(api, session, in.grabClick, sub, &click) && click) g = 1.f;
  return g;
}

bool ShellInputReadStickHand(const XrApi& api, XrSession session, const ShellInput& in,
                             Hand hand, float* outX, float* outY) {
  if (!in.attached || in.stick == XR_NULL_HANDLE) return false;
  XrActionStateVector2f st{XR_TYPE_ACTION_STATE_VECTOR2F};
  XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
  gi.action = in.stick;
  gi.subactionPath = HandPath(in, hand);
  XrResult r = api.getVec2 ? api.getVec2(session, &gi, &st) : xrGetActionStateVector2f(session, &gi, &st);
  if (r != XR_SUCCESS || !st.isActive) return false;
  if (outX) *outX = st.currentState.x;
  if (outY) *outY = st.currentState.y;
  return true;
}

bool ShellInputReadTrigger(const XrApi& api, XrSession session, const ShellInput& in,
                           float axisThresh) {
  return ShellInputReadTriggerHand(api, session, in, Hand::Left, axisThresh) ||
         ShellInputReadTriggerHand(api, session, in, Hand::Right, axisThresh);
}

float ShellInputReadGrab(const XrApi& api, XrSession session, const ShellInput& in) {
  float l = ShellInputReadGrabHand(api, session, in, Hand::Left);
  float r = ShellInputReadGrabHand(api, session, in, Hand::Right);
  return l > r ? l : r;
}

bool ShellInputReadMenu(const XrApi& api, XrSession session, const ShellInput& in) {
  if (!in.attached) return false;
  bool b = false;
  // Menu: try left first (Quest), then right (Index A), then any
  if (GetBool(api, session, in.menu, in.handLeft, &b) && b) return true;
  if (GetBool(api, session, in.menu, in.handRight, &b) && b) return true;
  // Fallback: some runtimes ignore subaction on single-bound menu
  if (GetBool(api, session, in.menu, XR_NULL_PATH, &b) && b) return true;
  return false;
}

bool ShellInputReadStick(const XrApi& api, XrSession session, const ShellInput& in,
                         float* outX, float* outY) {
  // Prefer the stick with larger magnitude so either hand can navigate UI.
  float lx = 0.f, ly = 0.f, rx = 0.f, ry = 0.f;
  bool okL = ShellInputReadStickHand(api, session, in, Hand::Left, &lx, &ly);
  bool okR = ShellInputReadStickHand(api, session, in, Hand::Right, &rx, &ry);
  if (!okL && !okR) return false;
  float mL = okL ? (lx * lx + ly * ly) : -1.f;
  float mR = okR ? (rx * rx + ry * ry) : -1.f;
  if (mR >= mL) {
    if (outX) *outX = rx;
    if (outY) *outY = ry;
  } else {
    if (outX) *outX = lx;
    if (outY) *outY = ly;
  }
  return true;
}

static bool LocateOne(const XrApi& api, XrSpace aim, XrSpace base, XrTime time, XrPosef* out,
                      float* outScore) {
  if (!aim || !out) return false;
  XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
  XrResult r = api.locateSpace ? api.locateSpace(aim, base, time, &loc)
                               : xrLocateSpace(aim, base, time, &loc);
  if (r != XR_SUCCESS) return false;
  const XrSpaceLocationFlags need =
      XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
  if ((loc.locationFlags & need) != need) return false;
  float px = loc.pose.position.x, py = loc.pose.position.y, pz = loc.pose.position.z;
  float d2 = px * px + py * py + pz * pz;
  // Near STAGE origin is valid (was 0.01 — killed WiVRn poses near 0)
  if (d2 < 1e-6f) return false;
  float score = d2;
  if (loc.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) score += 100.f;
  if (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) score += 100.f;
  *out = loc.pose;
  if (outScore) *outScore = score;
  return true;
}

bool ShellInputLocateAimHand(const XrApi& api, XrSession session, const ShellInput& in,
                             Hand hand, XrSpace base, XrTime time, XrPosef* outPose) {
  (void)session;
  if (!in.attached || !outPose) return false;
  XrSpace aim = hand == Hand::Left ? in.aimLeft : in.aimRight;
  return LocateOne(api, aim, base, time, outPose, nullptr);
}

bool ShellInputLocateAim(const XrApi& api, XrSession session, const ShellInput& in,
                         XrSpace base, XrTime time, XrPosef* outPose) {
  (void)session;
  if (!in.attached || !outPose) return false;
  XrPosef pr{}, pl{};
  float sr = -1.f, sl = -1.f;
  bool okR = LocateOne(api, in.aimRight, base, time, &pr, &sr);
  bool okL = LocateOne(api, in.aimLeft, base, time, &pl, &sl);
  if (okR && okL) {
    *outPose = (sr >= sl) ? pr : pl;
    return true;
  }
  if (okR) {
    *outPose = pr;
    return true;
  }
  if (okL) {
    *outPose = pl;
    return true;
  }
  return false;
}

} // namespace cube_xr
