#include "openxr_bindings.hpp"
#include <cstdio>
#include <cstring>

namespace cube_xr {

static XrResult CallStringToPath(const XrApi& api, const char* s, XrPath* p) {
  if (api.stringToPath) return api.stringToPath(api.instance, s, p);
  return xrStringToPath(api.instance, s, p);
}
static XrResult CallSuggest(const XrApi& api, const XrInteractionProfileSuggestedBinding* sp) {
  if (api.suggestBindings) return api.suggestBindings(api.instance, sp);
  return xrSuggestInteractionProfileBindings(api.instance, sp);
}

XrApi XrApiLinked(XrInstance instance) {
  XrApi a{};
  a.instance = instance;
  return a;
}

bool StringToPath(const XrApi& api, const char* pathStr, XrPath* out) {
  if (!out || !pathStr || api.instance == XR_NULL_HANDLE) return false;
  return CallStringToPath(api, pathStr, out) == XR_SUCCESS;
}

bool PushBinding(const XrApi& api, XrActionSuggestedBinding* out, int* n, int max,
                 XrAction action, const char* pathStr) {
  if (!out || !n || action == XR_NULL_HANDLE || !pathStr || *n >= max) return false;
  XrPath path = XR_NULL_PATH;
  if (!StringToPath(api, pathStr, &path)) return false;
  out[*n].action = action;
  out[*n].binding = path;
  (*n)++;
  return true;
}

bool SuggestProfile(const XrApi& api, const char* profilePath,
                    const XrActionSuggestedBinding* binds, uint32_t count) {
  if (!profilePath || count == 0 || !binds) return true;
  XrPath prof = XR_NULL_PATH;
  if (!StringToPath(api, profilePath, &prof)) return false;
  XrInteractionProfileSuggestedBinding sp{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
  sp.interactionProfile = prof;
  sp.countSuggestedBindings = count;
  sp.suggestedBindings = binds;
  XrResult r = CallSuggest(api, &sp);
  if (r != XR_SUCCESS) {
    fprintf(stderr, "[cube_xr] suggest %s failed (%d) n=%u\n", profilePath, (int)r, count);
    return false;
  }
  return true;
}

XrResult CreateActionSet(const XrApi& api, const char* name, const char* localized,
                         XrActionSet* out) {
  XrActionSetCreateInfo asci{XR_TYPE_ACTION_SET_CREATE_INFO};
  std::strncpy(asci.actionSetName, name, XR_MAX_ACTION_SET_NAME_SIZE - 1);
  std::strncpy(asci.localizedActionSetName, localized, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE - 1);
  if (api.createActionSet) return api.createActionSet(api.instance, &asci, out);
  return xrCreateActionSet(api.instance, &asci, out);
}

XrResult CreateAction(const XrApi& api, XrActionSet set, const char* name,
                      const char* localized, XrActionType type, XrAction* out) {
  XrActionCreateInfo aci{XR_TYPE_ACTION_CREATE_INFO};
  aci.actionType = type;
  std::strncpy(aci.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
  std::strncpy(aci.localizedActionName, localized, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
  if (api.createAction) return api.createAction(set, &aci, out);
  return xrCreateAction(set, &aci, out);
}

XrResult CreateActionSpace(const XrApi& api, XrSession session, XrAction poseAction,
                           XrSpace* out) {
  XrActionSpaceCreateInfo sci{XR_TYPE_ACTION_SPACE_CREATE_INFO};
  sci.action = poseAction;
  sci.poseInActionSpace.orientation.w = 1.f;
  if (api.createActionSpace) return api.createActionSpace(session, &sci, out);
  return xrCreateActionSpace(session, &sci, out);
}

XrResult AttachActionSets(const XrApi& api, XrSession session, XrActionSet* sets, uint32_t count) {
  XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attach.countActionSets = count;
  attach.actionSets = sets;
  if (api.attachSessionActionSets) return api.attachSessionActionSets(session, &attach);
  return xrAttachSessionActionSets(session, &attach);
}

} // namespace cube_xr
