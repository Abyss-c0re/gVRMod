#pragma once
// Shared OpenXR interaction path strings — single SoT for Cube launcher + vrmod module.
// Not product branding; standard OpenXR / Quest / Index paths only.

namespace cube_xr {

// ── Interaction profiles ──
inline constexpr const char* kProfileOculusTouch =
    "/interaction_profiles/oculus/touch_controller";
inline constexpr const char* kProfileIndex =
    "/interaction_profiles/valve/index_controller";
inline constexpr const char* kProfileTouchPro =
    "/interaction_profiles/facebook/touch_controller_pro";
inline constexpr const char* kProfileTouchPlus =
    "/interaction_profiles/meta/touch_controller_plus";
inline constexpr const char* kProfileKhrSimple =
    "/interaction_profiles/khr/simple_controller";

// ── Hands ──
namespace path {
inline constexpr const char* leftAimPose   = "/user/hand/left/input/aim/pose";
inline constexpr const char* rightAimPose  = "/user/hand/right/input/aim/pose";
inline constexpr const char* leftGripPose  = "/user/hand/left/input/grip/pose";
inline constexpr const char* rightGripPose = "/user/hand/right/input/grip/pose";

inline constexpr const char* leftTriggerValue  = "/user/hand/left/input/trigger/value";
inline constexpr const char* rightTriggerValue = "/user/hand/right/input/trigger/value";
inline constexpr const char* leftTriggerClick  = "/user/hand/left/input/trigger/click";
inline constexpr const char* rightTriggerClick = "/user/hand/right/input/trigger/click";

inline constexpr const char* leftSqueezeValue  = "/user/hand/left/input/squeeze/value";
inline constexpr const char* rightSqueezeValue = "/user/hand/right/input/squeeze/value";
inline constexpr const char* leftSqueezeClick  = "/user/hand/left/input/squeeze/click";
inline constexpr const char* rightSqueezeClick = "/user/hand/right/input/squeeze/click";

inline constexpr const char* leftThumbstick  = "/user/hand/left/input/thumbstick";
inline constexpr const char* rightThumbstick = "/user/hand/right/input/thumbstick";
inline constexpr const char* leftThumbClick  = "/user/hand/left/input/thumbstick/click";
inline constexpr const char* rightThumbClick = "/user/hand/right/input/thumbstick/click";

inline constexpr const char* leftMenuClick  = "/user/hand/left/input/menu/click";
inline constexpr const char* rightMenuClick = "/user/hand/right/input/menu/click";
inline constexpr const char* leftXClick     = "/user/hand/left/input/x/click";
inline constexpr const char* leftYClick     = "/user/hand/left/input/y/click";
inline constexpr const char* rightAClick    = "/user/hand/right/input/a/click";
inline constexpr const char* rightBClick    = "/user/hand/right/input/b/click";

inline constexpr const char* leftSelectClick  = "/user/hand/left/input/select/click";
inline constexpr const char* rightSelectClick = "/user/hand/right/input/select/click";

inline constexpr const char* leftHaptic  = "/user/hand/left/output/haptic";
inline constexpr const char* rightHaptic = "/user/hand/right/output/haptic";
} // namespace path

} // namespace cube_xr
