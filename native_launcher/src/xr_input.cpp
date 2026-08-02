#include "xr_input.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

bool XrInputSetup(XrInstance instance, XrSession session, XrInputState& in) {
  XrActionSetCreateInfo asci{XR_TYPE_ACTION_SET_CREATE_INFO};
  std::strncpy(asci.actionSetName, "cube_ui", XR_MAX_ACTION_SET_NAME_SIZE - 1);
  std::strncpy(asci.localizedActionSetName, "Cube UI", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
  if (XR_FAILED(xrCreateActionSet(instance, &asci, &in.set))) return false;

  auto mk = [&](const char* name, const char* loc, XrActionType ty, XrAction* out) {
    XrActionCreateInfo aci{XR_TYPE_ACTION_CREATE_INFO};
    aci.actionType = ty;
    std::strncpy(aci.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
    std::strncpy(aci.localizedActionName, loc, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
    return XR_SUCCEEDED(xrCreateAction(in.set, &aci, out));
  };
  if (!mk("aim_pose", "Aim Pose", XR_ACTION_TYPE_POSE_INPUT, &in.pose)) return false;
  if (!mk("trigger", "Trigger Click", XR_ACTION_TYPE_BOOLEAN_INPUT, &in.trigger)) return false;
  if (!mk("trigger_axis", "Trigger Axis", XR_ACTION_TYPE_FLOAT_INPUT, &in.triggerAxis)) return false;
  if (!mk("grab", "Grab Move Menu", XR_ACTION_TYPE_FLOAT_INPUT, &in.grab)) return false;
  if (!mk("grab_click", "Grab Click", XR_ACTION_TYPE_BOOLEAN_INPUT, &in.grabClick)) return false;
  if (!mk("stick", "Thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, &in.stick)) return false;
  if (!mk("menu", "Menu", XR_ACTION_TYPE_BOOLEAN_INPUT, &in.menu)) return false;

  auto suggest = [&](const char* profile, std::vector<XrActionSuggestedBinding> binds) {
    XrPath prof{};
    if (XR_FAILED(xrStringToPath(instance, profile, &prof))) return;
    XrInteractionProfileSuggestedBinding sp{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    sp.interactionProfile = prof;
    sp.suggestedBindings = binds.data();
    sp.countSuggestedBindings = (uint32_t)binds.size();
    xrSuggestInteractionProfileBindings(instance, &sp);
  };

  XrPath pPose{}, pTrigClick{}, pTrigVal{}, pSqueeze{}, pSqueezeClick{}, pThumb{}, pMenu{}, pA{};
  xrStringToPath(instance, "/user/hand/right/input/aim/pose", &pPose);
  xrStringToPath(instance, "/user/hand/right/input/trigger/click", &pTrigClick);
  xrStringToPath(instance, "/user/hand/right/input/trigger/value", &pTrigVal);
  xrStringToPath(instance, "/user/hand/right/input/squeeze/value", &pSqueeze);
  xrStringToPath(instance, "/user/hand/right/input/squeeze/click", &pSqueezeClick);
  xrStringToPath(instance, "/user/hand/right/input/thumbstick", &pThumb);
  xrStringToPath(instance, "/user/hand/right/input/menu/click", &pMenu);
  xrStringToPath(instance, "/user/hand/right/input/a/click", &pA);

  suggest("/interaction_profiles/oculus/touch_controller", {
    {in.pose, pPose}, {in.trigger, pTrigClick}, {in.triggerAxis, pTrigVal},
    {in.grab, pSqueeze}, {in.grabClick, pSqueezeClick}, {in.stick, pThumb}, {in.menu, pMenu},
  });
  suggest("/interaction_profiles/valve/index_controller", {
    {in.pose, pPose}, {in.trigger, pTrigClick}, {in.triggerAxis, pTrigVal},
    {in.grab, pSqueeze}, {in.grabClick, pSqueezeClick}, {in.stick, pThumb}, {in.menu, pA},
  });
  suggest("/interaction_profiles/facebook/touch_controller_pro", {
    {in.pose, pPose}, {in.trigger, pTrigClick}, {in.triggerAxis, pTrigVal},
    {in.grab, pSqueeze}, {in.grabClick, pSqueezeClick}, {in.stick, pThumb}, {in.menu, pMenu},
  });
  XrPath pSelect{};
  xrStringToPath(instance, "/user/hand/right/input/select/click", &pSelect);
  suggest("/interaction_profiles/khr/simple_controller", {
    {in.pose, pPose}, {in.trigger, pSelect},
  });

  XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attach.countActionSets = 1;
  attach.actionSets = &in.set;
  if (XR_FAILED(xrAttachSessionActionSets(session, &attach))) {
    fprintf(stderr, "[cube_webui] warn: attach action sets failed\n");
    return false;
  }

  XrActionSpaceCreateInfo spaceCi{XR_TYPE_ACTION_SPACE_CREATE_INFO};
  spaceCi.action = in.pose;
  spaceCi.poseInActionSpace = IdentityPose();
  if (XR_FAILED(xrCreateActionSpace(session, &spaceCi, &in.aimSpace))) {
    fprintf(stderr, "[cube_webui] warn: aim space failed\n");
    return false;
  }
  in.attached = true;
  fprintf(stderr, "[cube_webui] actions: aim + trigger + GRAB + stick + menu\n");
  return true;
}

void XrInputDestroy(XrInputState& in) {
  if (in.aimSpace) xrDestroySpace(in.aimSpace);
  if (in.set) xrDestroyActionSet(in.set);
  in = {};
}

void XrInputSync(XrSession session, XrInputState& in) {
  if (!in.attached) return;
  XrActiveActionSet aas{in.set, XR_NULL_PATH};
  XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
  sync.countActiveActionSets = 1;
  sync.activeActionSets = &aas;
  xrSyncActions(session, &sync);
}

bool XrInputReadTrigger(XrSession session, const XrInputState& in, float axisThresh) {
  if (!in.attached) return false;
  bool trig = false;
  XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
  XrActionStateBoolean st{XR_TYPE_ACTION_STATE_BOOLEAN};
  gi.action = in.trigger;
  if (XR_SUCCEEDED(xrGetActionStateBoolean(session, &gi, &st)) && st.isActive)
    trig = st.currentState;
  XrActionStateFloat ft{XR_TYPE_ACTION_STATE_FLOAT};
  gi.action = in.triggerAxis;
  if (XR_SUCCEEDED(xrGetActionStateFloat(session, &gi, &ft)) && ft.isActive)
    trig = trig || (ft.currentState > axisThresh);
  return trig;
}

float XrInputReadGrab(XrSession session, const XrInputState& in) {
  if (!in.attached) return 0.f;
  float g = 0.f;
  XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
  if (in.grab) {
    XrActionStateFloat ft{XR_TYPE_ACTION_STATE_FLOAT};
    gi.action = in.grab;
    if (XR_SUCCEEDED(xrGetActionStateFloat(session, &gi, &ft)) && ft.isActive)
      g = ft.currentState;
  }
  if (in.grabClick) {
    XrActionStateBoolean st{XR_TYPE_ACTION_STATE_BOOLEAN};
    gi.action = in.grabClick;
    if (XR_SUCCEEDED(xrGetActionStateBoolean(session, &gi, &st)) && st.isActive && st.currentState)
      g = 1.f;
  }
  return g;
}

bool XrInputReadMenu(XrSession session, const XrInputState& in) {
  if (!in.attached || !in.menu) return false;
  XrActionStateBoolean st{XR_TYPE_ACTION_STATE_BOOLEAN};
  XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
  gi.action = in.menu;
  return XR_SUCCEEDED(xrGetActionStateBoolean(session, &gi, &st)) && st.isActive && st.currentState;
}

bool XrInputReadStick(XrSession session, const XrInputState& in, float* outX, float* outY) {
  if (!in.attached || !in.stick) return false;
  XrActionStateVector2f st{XR_TYPE_ACTION_STATE_VECTOR2F};
  XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
  gi.action = in.stick;
  if (!XR_SUCCEEDED(xrGetActionStateVector2f(session, &gi, &st)) || !st.isActive) return false;
  if (outX) *outX = st.currentState.x;
  if (outY) *outY = st.currentState.y;
  return true;
}

bool XrInputLocateAim(XrSession session, const XrInputState& in, XrSpace base,
                      XrTime time, XrPosef* outPose) {
  (void)session;
  if (!in.attached || !in.aimSpace || !outPose) return false;
  XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
  if (XR_FAILED(xrLocateSpace(in.aimSpace, base, time, &loc))) return false;
  if (!(loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) return false;
  *outPose = loc.pose;
  return true;
}
