#pragma once
// Shared OpenXR binding helpers (launcher links loader; module may pass PFNs).
#include <openxr/openxr.h>
#include <cstdint>

namespace cube_xr {

// Optional dynload bridge (module). Null PFNs → use linked xr* entry points.
struct XrApi {
  XrInstance instance = XR_NULL_HANDLE;
  PFN_xrStringToPath stringToPath = nullptr;
  PFN_xrSuggestInteractionProfileBindings suggestBindings = nullptr;
  PFN_xrCreateActionSet createActionSet = nullptr;
  PFN_xrCreateAction createAction = nullptr;
  PFN_xrCreateActionSpace createActionSpace = nullptr;
  PFN_xrAttachSessionActionSets attachSessionActionSets = nullptr;
  PFN_xrDestroyActionSet destroyActionSet = nullptr;
  PFN_xrDestroySpace destroySpace = nullptr;
  PFN_xrSyncActions syncActions = nullptr;
  PFN_xrGetActionStateBoolean getBool = nullptr;
  PFN_xrGetActionStateFloat getFloat = nullptr;
  PFN_xrGetActionStateVector2f getVec2 = nullptr;
  PFN_xrLocateSpace locateSpace = nullptr;
};

// Default: linked OpenXR loader (native launcher).
XrApi XrApiLinked(XrInstance instance);

bool StringToPath(const XrApi& api, const char* pathStr, XrPath* out);
bool PushBinding(const XrApi& api, XrActionSuggestedBinding* out, int* n, int max,
                 XrAction action, const char* pathStr);
bool SuggestProfile(const XrApi& api, const char* profilePath,
                    const XrActionSuggestedBinding* binds, uint32_t count);

XrResult CreateActionSet(const XrApi& api, const char* name, const char* localized,
                         XrActionSet* out);
XrResult CreateAction(const XrApi& api, XrActionSet set, const char* name,
                      const char* localized, XrActionType type, XrAction* out);
XrResult CreateActionSpace(const XrApi& api, XrSession session, XrAction poseAction,
                           XrSpace* out);
XrResult AttachActionSets(const XrApi& api, XrSession session, XrActionSet* sets, uint32_t count);

} // namespace cube_xr
