#include "openxr_shell_input.hpp"
#include "openxr_paths.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

namespace cube_xr {

bool ShellInputSetup(const XrApi& api, XrSession session, ShellInput& in,
                     const char* actionSetName) {
  if (CreateActionSet(api, actionSetName, "Cube Shell", &in.set) != XR_SUCCESS)
    return false;

  auto mk = [&](const char* name, const char* loc, XrActionType ty, XrAction* out) {
    return CreateAction(api, in.set, name, loc, ty, out) == XR_SUCCESS;
  };
  if (!mk("aim_pose", "Aim Pose", XR_ACTION_TYPE_POSE_INPUT, &in.pose)) return false;
  if (!mk("trigger", "Trigger Click", XR_ACTION_TYPE_BOOLEAN_INPUT, &in.trigger)) return false;
  if (!mk("trigger_axis", "Trigger Axis", XR_ACTION_TYPE_FLOAT_INPUT, &in.triggerAxis)) return false;
  if (!mk("grab", "Grab", XR_ACTION_TYPE_FLOAT_INPUT, &in.grab)) return false;
  if (!mk("grab_click", "Grab Click", XR_ACTION_TYPE_BOOLEAN_INPUT, &in.grabClick)) return false;
  if (!mk("stick", "Thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, &in.stick)) return false;
  if (!mk("menu", "Menu", XR_ACTION_TYPE_BOOLEAN_INPUT, &in.menu)) return false;

  XrActionSuggestedBinding binds[16];
  int n = 0;
  auto fillTouchLike = [&]() {
    n = 0;
    PushBinding(api, binds, &n, 16, in.pose, path::rightAimPose);
    PushBinding(api, binds, &n, 16, in.trigger, path::rightTriggerClick);
    PushBinding(api, binds, &n, 16, in.triggerAxis, path::rightTriggerValue);
    PushBinding(api, binds, &n, 16, in.grab, path::rightSqueezeValue);
    PushBinding(api, binds, &n, 16, in.grabClick, path::rightSqueezeClick);
    PushBinding(api, binds, &n, 16, in.stick, path::rightThumbstick);
    PushBinding(api, binds, &n, 16, in.menu, path::rightMenuClick);
  };

  fillTouchLike();
  SuggestProfile(api, kProfileOculusTouch, binds, (uint32_t)n);
  fillTouchLike();
  // Index: menu on A
  n = 0;
  PushBinding(api, binds, &n, 16, in.pose, path::rightAimPose);
  PushBinding(api, binds, &n, 16, in.trigger, path::rightTriggerClick);
  PushBinding(api, binds, &n, 16, in.triggerAxis, path::rightTriggerValue);
  PushBinding(api, binds, &n, 16, in.grab, path::rightSqueezeValue);
  PushBinding(api, binds, &n, 16, in.grabClick, path::rightSqueezeClick);
  PushBinding(api, binds, &n, 16, in.stick, path::rightThumbstick);
  PushBinding(api, binds, &n, 16, in.menu, path::rightAClick);
  SuggestProfile(api, kProfileIndex, binds, (uint32_t)n);

  fillTouchLike();
  SuggestProfile(api, kProfileTouchPro, binds, (uint32_t)n);
  fillTouchLike();
  SuggestProfile(api, kProfileTouchPlus, binds, (uint32_t)n);

  n = 0;
  PushBinding(api, binds, &n, 16, in.pose, path::rightAimPose);
  PushBinding(api, binds, &n, 16, in.trigger, path::rightSelectClick);
  SuggestProfile(api, kProfileKhrSimple, binds, (uint32_t)n);

  if (AttachActionSets(api, session, &in.set, 1) != XR_SUCCESS) {
    fprintf(stderr, "[cube_xr] attach action sets failed\n");
    return false;
  }
  if (CreateActionSpace(api, session, in.pose, &in.aimSpace) != XR_SUCCESS) {
    fprintf(stderr, "[cube_xr] aim space failed\n");
    return false;
  }
  in.attached = true;
  fprintf(stderr, "[cube_xr] shell input ready (shared paths/bindings)\n");
  return true;
}

void ShellInputDestroy(const XrApi& api, ShellInput& in) {
  if (in.aimSpace) {
    if (api.destroySpace) api.destroySpace(in.aimSpace);
    else xrDestroySpace(in.aimSpace);
  }
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

bool ShellInputLocateAim(const XrApi& api, XrSession session, const ShellInput& in,
                         XrSpace base, XrTime time, XrPosef* outPose) {
  (void)session;
  if (!in.attached || !in.aimSpace || !outPose) return false;
  XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
  XrResult r = api.locateSpace ? api.locateSpace(in.aimSpace, base, time, &loc)
                               : xrLocateSpace(in.aimSpace, base, time, &loc);
  if (r != XR_SUCCESS) return false;
  // Untracked often returns origin (0,0,0) — ray from room center. Reject that.
  const XrSpaceLocationFlags need =
      XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
      XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
  if ((loc.locationFlags & need) != need) {
    // Soft fallback: valid only (some runtimes omit TRACKED briefly)
    const XrSpaceLocationFlags soft =
        XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    if ((loc.locationFlags & soft) != soft) return false;
    // Still reject near-origin untracked junk
    float px = loc.pose.position.x, py = loc.pose.position.y, pz = loc.pose.position.z;
    if (px * px + py * py + pz * pz < 1e-6f) return false;
  }
  *outPose = loc.pose;
  return true;
}

} // namespace cube_xr
