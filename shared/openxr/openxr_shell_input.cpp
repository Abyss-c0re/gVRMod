#include "openxr_shell_input.hpp"
#include "openxr_paths.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>

namespace cube_xr {

static XrResult CreatePoseActionWithHands(const XrApi& api, XrActionSet set,
                                          const char* name, const char* loc,
                                          XrPath hands[2], XrAction* out) {
  XrActionCreateInfo aci{XR_TYPE_ACTION_CREATE_INFO};
  aci.actionType = XR_ACTION_TYPE_POSE_INPUT;
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

  if (CreatePoseActionWithHands(api, in.set, "aim_pose", "Aim Pose", hands, &in.pose) !=
      XR_SUCCESS)
    return false;

  auto mk = [&](const char* name, const char* loc, XrActionType ty, XrAction* out) {
    return CreateAction(api, in.set, name, loc, ty, out) == XR_SUCCESS;
  };
  if (!mk("trigger", "Trigger Click", XR_ACTION_TYPE_BOOLEAN_INPUT, &in.trigger)) return false;
  if (!mk("trigger_axis", "Trigger Axis", XR_ACTION_TYPE_FLOAT_INPUT, &in.triggerAxis))
    return false;
  if (!mk("grab", "Grab", XR_ACTION_TYPE_FLOAT_INPUT, &in.grab)) return false;
  if (!mk("grab_click", "Grab Click", XR_ACTION_TYPE_BOOLEAN_INPUT, &in.grabClick)) return false;
  if (!mk("stick", "Thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, &in.stick)) return false;
  if (!mk("menu", "Menu", XR_ACTION_TYPE_BOOLEAN_INPUT, &in.menu)) return false;

  XrActionSuggestedBinding binds[24];
  int n = 0;
  auto push = [&](XrAction a, const char* p) {
    PushBinding(api, binds, &n, 24, a, p);
  };

  auto fillTouch = [&]() {
    n = 0;
    push(in.pose, path::rightAimPose);
    push(in.pose, path::leftAimPose);
    push(in.trigger, path::rightTriggerClick);
    push(in.triggerAxis, path::rightTriggerValue);
    push(in.grab, path::rightSqueezeValue);
    push(in.grab, path::leftSqueezeValue);
    push(in.grabClick, path::rightSqueezeClick);
    push(in.stick, path::rightThumbstick);
    push(in.menu, path::leftMenuClick); // Quest menu is left
    push(in.menu, path::rightMenuClick);
  };

  fillTouch();
  SuggestProfile(api, kProfileOculusTouch, binds, (uint32_t)n);
  fillTouch();
  SuggestProfile(api, kProfileTouchPro, binds, (uint32_t)n);
  fillTouch();
  SuggestProfile(api, kProfileTouchPlus, binds, (uint32_t)n);

  // Index
  n = 0;
  push(in.pose, path::rightAimPose);
  push(in.pose, path::leftAimPose);
  push(in.trigger, path::rightTriggerClick);
  push(in.triggerAxis, path::rightTriggerValue);
  push(in.grab, path::rightSqueezeValue);
  push(in.stick, path::rightThumbstick);
  push(in.menu, path::rightAClick);
  SuggestProfile(api, kProfileIndex, binds, (uint32_t)n);

  n = 0;
  push(in.pose, path::rightAimPose);
  push(in.trigger, path::rightSelectClick);
  SuggestProfile(api, kProfileKhrSimple, binds, (uint32_t)n);

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
  fprintf(stderr, "[cube_xr] shell input ready (L/R aim spaces)\n");
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

static bool GetBool(const XrApi& api, XrSession session, XrAction a, bool* out) {
  if (a == XR_NULL_HANDLE) return false;
  XrActionStateBoolean st{XR_TYPE_ACTION_STATE_BOOLEAN};
  XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
  gi.action = a;
  XrResult r = api.getBool ? api.getBool(session, &gi, &st) : xrGetActionStateBoolean(session, &gi, &st);
  if (r != XR_SUCCESS || !st.isActive) return false;
  if (out) *out = st.currentState;
  return true;
}
static bool GetFloat(const XrApi& api, XrSession session, XrAction a, float* out) {
  if (a == XR_NULL_HANDLE) return false;
  XrActionStateFloat st{XR_TYPE_ACTION_STATE_FLOAT};
  XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
  gi.action = a;
  XrResult r = api.getFloat ? api.getFloat(session, &gi, &st) : xrGetActionStateFloat(session, &gi, &st);
  if (r != XR_SUCCESS || !st.isActive) return false;
  if (out) *out = st.currentState;
  return true;
}

bool ShellInputReadTrigger(const XrApi& api, XrSession session, const ShellInput& in,
                           float axisThresh) {
  if (!in.attached) return false;
  bool b = false;
  if (GetBool(api, session, in.trigger, &b) && b) return true;
  float f = 0.f;
  if (GetFloat(api, session, in.triggerAxis, &f) && f > axisThresh) return true;
  return false;
}

float ShellInputReadGrab(const XrApi& api, XrSession session, const ShellInput& in) {
  if (!in.attached) return 0.f;
  float g = 0.f;
  GetFloat(api, session, in.grab, &g);
  bool click = false;
  if (GetBool(api, session, in.grabClick, &click) && click) g = 1.f;
  return g;
}

bool ShellInputReadMenu(const XrApi& api, XrSession session, const ShellInput& in) {
  bool b = false;
  return in.attached && GetBool(api, session, in.menu, &b) && b;
}

bool ShellInputReadStick(const XrApi& api, XrSession session, const ShellInput& in,
                         float* outX, float* outY) {
  if (!in.attached || in.stick == XR_NULL_HANDLE) return false;
  XrActionStateVector2f st{XR_TYPE_ACTION_STATE_VECTOR2F};
  XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
  gi.action = in.stick;
  XrResult r = api.getVec2 ? api.getVec2(session, &gi, &st) : xrGetActionStateVector2f(session, &gi, &st);
  if (r != XR_SUCCESS || !st.isActive) return false;
  if (outX) *outX = st.currentState.x;
  if (outY) *outY = st.currentState.y;
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
  // Reject origin junk (untracked often sits at 0)
  if (d2 < 0.01f) return false; // <10cm from space origin — almost always wrong for a held controller
  float score = d2;
  if (loc.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) score += 100.f;
  if (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) score += 100.f;
  *out = loc.pose;
  if (outScore) *outScore = score;
  return true;
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
    *outPose = (sr >= sl) ? pr : pl; // prefer higher track score; right usually wins
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
