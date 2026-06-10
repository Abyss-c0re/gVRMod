#pragma once

#include "core/vrmod_common.h"

// We dynamically load all OpenXR functions, so no prototypes from headers
#define XR_NO_PROTOTYPES
#include <openxr/openxr/openxr.h>

// ── OpenXR function pointers (loaded from libopenxr_loader.so) ──
// Core instance
extern PFN_xrGetInstanceProcAddr             g_xrGetInstanceProcAddr;
extern PFN_xrEnumerateInstanceExtensionProperties g_xrEnumerateInstanceExtensionProperties;
extern PFN_xrCreateInstance                  g_xrCreateInstance;
extern PFN_xrDestroyInstance                 g_xrDestroyInstance;
extern PFN_xrGetSystem                       g_xrGetSystem;
extern PFN_xrGetSystemProperties             g_xrGetSystemProperties;
extern PFN_xrCreateSession                   g_xrCreateSession;
extern PFN_xrDestroySession                  g_xrDestroySession;
extern PFN_xrBeginSession                    g_xrBeginSession;
extern PFN_xrEndSession                      g_xrEndSession;
extern PFN_xrRequestExitSession              g_xrRequestExitSession;
extern PFN_xrPollEvent                       g_xrPollEvent;
extern PFN_xrWaitFrame                       g_xrWaitFrame;
extern PFN_xrBeginFrame                      g_xrBeginFrame;
extern PFN_xrEndFrame                        g_xrEndFrame;
extern PFN_xrLocateViews                     g_xrLocateViews;
extern PFN_xrResultToString                  g_xrResultToString;
// Swapchain
extern PFN_xrEnumerateSwapchainFormats       g_xrEnumerateSwapchainFormats;
extern PFN_xrCreateSwapchain                 g_xrCreateSwapchain;
extern PFN_xrDestroySwapchain                g_xrDestroySwapchain;
extern PFN_xrEnumerateSwapchainImages        g_xrEnumerateSwapchainImages;
extern PFN_xrAcquireSwapchainImage           g_xrAcquireSwapchainImage;
extern PFN_xrWaitSwapchainImage              g_xrWaitSwapchainImage;
extern PFN_xrReleaseSwapchainImage           g_xrReleaseSwapchainImage;
// Reference spaces
extern PFN_xrCreateReferenceSpace            g_xrCreateReferenceSpace;
extern PFN_xrDestroySpace                    g_xrDestroySpace;
extern PFN_xrEnumerateReferenceSpaces        g_xrEnumerateReferenceSpaces;
// Views
extern PFN_xrEnumerateViewConfigurations     g_xrEnumerateViewConfigurations;
extern PFN_xrEnumerateViewConfigurationViews g_xrEnumerateViewConfigurationViews;
extern PFN_xrLocateViews                     g_xrLocateViews;
// Actions
extern PFN_xrCreateActionSet                 g_xrCreateActionSet;
extern PFN_xrDestroyActionSet                g_xrDestroyActionSet;
extern PFN_xrCreateAction                    g_xrCreateAction;
extern PFN_xrDestroyAction                   g_xrDestroyAction;
extern PFN_xrSuggestInteractionProfileBindings g_xrSuggestInteractionProfileBindings;
extern PFN_xrAttachSessionActionSets         g_xrAttachSessionActionSets;
extern PFN_xrSyncActions                     g_xrSyncActions;
extern PFN_xrGetActionStateBoolean           g_xrGetActionStateBoolean;
extern PFN_xrGetActionStateFloat             g_xrGetActionStateFloat;
extern PFN_xrGetActionStateVector2f          g_xrGetActionStateVector2f;
extern PFN_xrGetActionStatePose              g_xrGetActionStatePose;
extern PFN_xrCreateActionSpace               g_xrCreateActionSpace;
extern PFN_xrLocateSpace                     g_xrLocateSpace;
extern PFN_xrApplyHapticFeedback             g_xrApplyHapticFeedback;
extern PFN_xrStringToPath                    g_xrStringToPath;
extern PFN_xrPathToString                    g_xrPathToString;

// ── Global OpenXR state ──
extern XrInstance       g_xrInstance;
extern XrSystemId       g_xrSystemId;
extern XrSession        g_xrSession;
extern XrSpace          g_xrStageSpace;
extern XrSpace          g_xrViewSpace;
extern bool             g_xrSessionRunning;
extern bool             g_xrSessionReady;
extern XrSessionState   g_xrSessionState;
extern XrFrameState     g_xrFrameState;
extern XrViewConfigurationView g_xrViewConfigs[2];

// ── Recommended render size ──
extern uint32_t         g_xrRecommendedWidth;
extern uint32_t         g_xrRecommendedHeight;

// ── Error callback ──
typedef void (*XrErrorFunc)(const char* msg);

// ── Lifecycle ──
// Load the OpenXR loader library and resolve core function pointers.
// Returns true on success.
bool XR_LoadLoader();

// Initialize OpenXR: create instance, get system (session GL binding is deferred).
// Returns true on success. On failure sets errMsg.
bool XR_Init(char* errMsg, int errMsgLen);

// Create the XrSession + reference spaces using the *current* GLX context (call only when
// a real game render context is active, e.g. from inside ShareTextureBegin / RenderScene).
// This is the key step to make stolen/capture textures valid for the FBO blit into swapchains.
bool XR_CreateSessionWithCurrentGL(char* errMsg, int errMsgLen);

// Pump the event loop. Returns true if session is still alive.
bool XR_PollEvents();

// Shutdown: destroy session, instance, unload loader.
void XR_Shutdown();

// Get a human-readable error string for an XrResult.
const char* XR_ResultToString(XrResult result);

// ── Frame lifecycle ──
// Returns true if we should render this frame.
bool XR_WaitAndBeginFrame();
// End frame with the composed layers.
void XR_EndFrame();

// ── View/display info ──
struct XrDisplayInfo {
    float projLeft[16];   // 4x4 column-major
    float projRight[16];  // 4x4 column-major
    float transformLeft[12];  // 3x4 row-major (eye-to-head)
    float transformRight[12]; // 3x4 row-major (eye-to-head)
    uint32_t recommendedWidth;
    uint32_t recommendedHeight;
};

bool XR_GetDisplayInfo(float nearZ, float farZ, XrDisplayInfo* out);

// ── HMD detection ──
bool XR_IsHMDPresent();