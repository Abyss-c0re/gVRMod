#include "xr_session.h"
#include "core/vrmod_log.h"

#include <dlfcn.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>
#include <cstdlib>   // setenv / unsetenv / getenv for LD_LIBRARY_PATH hack when bundling libs

// On Linux we use XR_KHR_opengl_enable for the OpenGL graphics binding
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_XLIB
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <openxr/openxr/openxr_platform.h>

// ── Function pointer definitions ──
PFN_xrGetInstanceProcAddr             g_xrGetInstanceProcAddr = nullptr;
PFN_xrEnumerateInstanceExtensionProperties g_xrEnumerateInstanceExtensionProperties = nullptr;
PFN_xrCreateInstance                  g_xrCreateInstance = nullptr;
PFN_xrDestroyInstance                 g_xrDestroyInstance = nullptr;
PFN_xrGetSystem                       g_xrGetSystem = nullptr;
PFN_xrGetSystemProperties             g_xrGetSystemProperties = nullptr;
PFN_xrCreateSession                   g_xrCreateSession = nullptr;
PFN_xrDestroySession                  g_xrDestroySession = nullptr;
PFN_xrBeginSession                    g_xrBeginSession = nullptr;
PFN_xrEndSession                      g_xrEndSession = nullptr;
PFN_xrRequestExitSession              g_xrRequestExitSession = nullptr;
PFN_xrPollEvent                       g_xrPollEvent = nullptr;
PFN_xrWaitFrame                       g_xrWaitFrame = nullptr;
PFN_xrBeginFrame                      g_xrBeginFrame = nullptr;
PFN_xrEndFrame                        g_xrEndFrame = nullptr;
PFN_xrLocateViews                     g_xrLocateViews = nullptr;
PFN_xrResultToString                  g_xrResultToString = nullptr;
PFN_xrEnumerateSwapchainFormats       g_xrEnumerateSwapchainFormats = nullptr;
PFN_xrCreateSwapchain                 g_xrCreateSwapchain = nullptr;
PFN_xrDestroySwapchain                g_xrDestroySwapchain = nullptr;
PFN_xrEnumerateSwapchainImages        g_xrEnumerateSwapchainImages = nullptr;
PFN_xrAcquireSwapchainImage           g_xrAcquireSwapchainImage = nullptr;
PFN_xrWaitSwapchainImage              g_xrWaitSwapchainImage = nullptr;
PFN_xrReleaseSwapchainImage           g_xrReleaseSwapchainImage = nullptr;
PFN_xrCreateReferenceSpace            g_xrCreateReferenceSpace = nullptr;
PFN_xrDestroySpace                    g_xrDestroySpace = nullptr;
PFN_xrEnumerateReferenceSpaces        g_xrEnumerateReferenceSpaces = nullptr;
PFN_xrEnumerateViewConfigurations     g_xrEnumerateViewConfigurations = nullptr;
PFN_xrEnumerateViewConfigurationViews g_xrEnumerateViewConfigurationViews = nullptr;
PFN_xrCreateActionSet                 g_xrCreateActionSet = nullptr;
PFN_xrDestroyActionSet                g_xrDestroyActionSet = nullptr;
PFN_xrCreateAction                    g_xrCreateAction = nullptr;
PFN_xrDestroyAction                   g_xrDestroyAction = nullptr;
PFN_xrSuggestInteractionProfileBindings g_xrSuggestInteractionProfileBindings = nullptr;
PFN_xrAttachSessionActionSets         g_xrAttachSessionActionSets = nullptr;
PFN_xrSyncActions                     g_xrSyncActions = nullptr;
PFN_xrGetActionStateBoolean           g_xrGetActionStateBoolean = nullptr;
PFN_xrGetActionStateFloat             g_xrGetActionStateFloat = nullptr;
PFN_xrGetActionStateVector2f          g_xrGetActionStateVector2f = nullptr;
PFN_xrGetActionStatePose              g_xrGetActionStatePose = nullptr;
PFN_xrCreateActionSpace               g_xrCreateActionSpace = nullptr;
PFN_xrLocateSpace                     g_xrLocateSpace = nullptr;
PFN_xrApplyHapticFeedback             g_xrApplyHapticFeedback = nullptr;
PFN_xrStringToPath                    g_xrStringToPath = nullptr;
PFN_xrPathToString                    g_xrPathToString = nullptr;

// ── Global state definitions ──
XrInstance       g_xrInstance = XR_NULL_HANDLE;
XrSystemId       g_xrSystemId = XR_NULL_SYSTEM_ID;
XrSession        g_xrSession = XR_NULL_HANDLE;
XrSpace          g_xrStageSpace = XR_NULL_HANDLE;
XrSpace          g_xrViewSpace = XR_NULL_HANDLE;
bool             g_xrSessionRunning = false;
bool             g_xrSessionReady = false;
XrSessionState   g_xrSessionState = XR_SESSION_STATE_UNKNOWN;
XrFrameState     g_xrFrameState = {};
XrViewConfigurationView g_xrViewConfigs[2] = {};
uint32_t         g_xrRecommendedWidth = 0;
uint32_t         g_xrRecommendedHeight = 0;

static void* g_loaderLib = nullptr;
static char g_resultBuf[XR_MAX_RESULT_STRING_SIZE];
static char g_loaderErrBuf[256]; // last dlopen failure reason for better error messages

// Helper macro to load instance proc
#define XR_LOAD(fn) do { \
    if (g_xrGetInstanceProcAddr(g_xrInstance, #fn, (PFN_xrVoidFunction*)&g_##fn) != XR_SUCCESS) { \
        VRMOD_LOG_ERROR("Failed to load " #fn); \
        snprintf(errMsg, errMsgLen, "VRMOD OpenXR: Failed to load " #fn); \
        return false; \
    } \
} while(0)

bool XR_LoadLoader() {
    if (g_loaderLib) return true;
    g_loaderErrBuf[0] = '\0';

    // Discover the directory containing our module (gmcl_vrmod_linux64.dll).
    // This lets us prefer a local bundle of the OpenXR loader + its dependencies,
    // which is the practical solution on Linux because Steam runs GMod inside its
    // own container (pressure-vessel) where host system packages are often not visible.
    std::string moduleDir;
    Dl_info selfInfo{};
    if (dladdr((void*)&XR_LoadLoader, &selfInfo) && selfInfo.dli_fname) {
        const char* modPath = selfInfo.dli_fname;
        const char* lastSlash = strrchr(modPath, '/');
        if (lastSlash) {
            moduleDir.assign(modPath, lastSlash - modPath);
        }
    }

    // Temporarily augment LD_LIBRARY_PATH with the module directory.
    // This greatly increases the chance that the dynamic linker will resolve
    // transitive dependencies (libjsoncpp.so.26, etc.) from a local bundle the
    // user placed next to gmcl_vrmod_linux64.dll.
    const char* oldLdPath = getenv("LD_LIBRARY_PATH");
    std::string newLdPath;
    if (!moduleDir.empty()) {
        newLdPath = moduleDir;
    }
    if (oldLdPath && *oldLdPath) {
        if (!newLdPath.empty()) newLdPath += ":";
        newLdPath += oldLdPath;
    }
    if (!newLdPath.empty()) {
        setenv("LD_LIBRARY_PATH", newLdPath.c_str(), 1);
    }

    // Candidate list. Local full paths first (supports "copy all the libs into lua/bin").
    const char* candidates[32];
    int c = 0;

    if (!moduleDir.empty()) {
        // Absolute paths inside the module dir (user-bundled libs)
        static char local1[512];
        static char local2[512];
        snprintf(local1, sizeof(local1), "%s/libopenxr_loader.so.1", moduleDir.c_str());
        snprintf(local2, sizeof(local2), "%s/libopenxr_loader.so", moduleDir.c_str());
        candidates[c++] = local1;
        candidates[c++] = local2;
    }

    // Bare sonames (let the normal search + the LD_LIBRARY_PATH we just set do the work)
    candidates[c++] = "libopenxr_loader.so.1";
    candidates[c++] = "libopenxr_loader.so";

    // Last resort: well-known distro locations
    candidates[c++] = "/usr/lib/libopenxr_loader.so.1";
    candidates[c++] = "/usr/lib/libopenxr_loader.so";
    candidates[c++] = "/usr/lib64/libopenxr_loader.so.1";
    candidates[c++] = "/usr/lib64/libopenxr_loader.so";
    candidates[c++] = "/usr/lib/x86_64-linux-gnu/libopenxr_loader.so.1";
    candidates[c++] = "/usr/lib/x86_64-linux-gnu/libopenxr_loader.so";

    candidates[c] = nullptr;

    for (int i = 0; candidates[i] != nullptr; ++i) {
        g_loaderLib = dlopen(candidates[i], RTLD_NOW | RTLD_GLOBAL);
        if (g_loaderLib) {
            VRMOD_LOG_INFO("OpenXR loader loaded from: %s", candidates[i]);
            break;
        }
    }

    // Restore the original LD_LIBRARY_PATH (best effort)
    if (oldLdPath) {
        setenv("LD_LIBRARY_PATH", oldLdPath, 1);
    } else {
        unsetenv("LD_LIBRARY_PATH");
    }

    if (!g_loaderLib) {
        const char* derr = dlerror();
        VRMOD_LOG_ERROR("Failed to dlopen libopenxr_loader.so: %s", derr ? derr : "(no dlerror)");
        if (derr) {
            snprintf(g_loaderErrBuf, sizeof(g_loaderErrBuf), "%s", derr);
        } else {
            snprintf(g_loaderErrBuf, sizeof(g_loaderErrBuf), "libopenxr_loader.so not found (tried bundle dir + system paths)");
        }
        return false;
    }
    g_xrGetInstanceProcAddr = (PFN_xrGetInstanceProcAddr)dlsym(g_loaderLib, "xrGetInstanceProcAddr");
    if (!g_xrGetInstanceProcAddr) {
        VRMOD_LOG_ERROR("Failed to resolve xrGetInstanceProcAddr");
        dlclose(g_loaderLib);
        g_loaderLib = nullptr;
        snprintf(g_loaderErrBuf, sizeof(g_loaderErrBuf), "xrGetInstanceProcAddr not exported by loader");
        return false;
    }
    // Load pre-instance functions
    g_xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties",
        (PFN_xrVoidFunction*)&g_xrEnumerateInstanceExtensionProperties);
    g_xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrCreateInstance",
        (PFN_xrVoidFunction*)&g_xrCreateInstance);

    VRMOD_LOG_INFO("OpenXR loader loaded successfully");
    return true;
}

bool XR_IsHMDPresent() {
    if (!XR_LoadLoader()) return false;

    // Try to create a lightweight instance just to query system
    XrInstanceCreateInfo ici = {XR_TYPE_INSTANCE_CREATE_INFO};
    strncpy(ici.applicationInfo.applicationName, "vrmod_probe", XR_MAX_APPLICATION_NAME_SIZE - 1);
    ici.applicationInfo.applicationVersion = 1;
    ici.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);

    XrInstance probeInstance = XR_NULL_HANDLE;
    if (!g_xrCreateInstance) return false;
    XrResult res = g_xrCreateInstance(&ici, &probeInstance);
    if (res != XR_SUCCESS) return false;

    PFN_xrGetSystem probeGetSystem = nullptr;
    g_xrGetInstanceProcAddr(probeInstance, "xrGetSystem", (PFN_xrVoidFunction*)&probeGetSystem);
    PFN_xrDestroyInstance probeDestroy = nullptr;
    g_xrGetInstanceProcAddr(probeInstance, "xrDestroyInstance", (PFN_xrVoidFunction*)&probeDestroy);

    XrSystemGetInfo sgi = {XR_TYPE_SYSTEM_GET_INFO};
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId sysId = XR_NULL_SYSTEM_ID;
    bool found = false;
    if (probeGetSystem) {
        found = (probeGetSystem(probeInstance, &sgi, &sysId) == XR_SUCCESS);
    }
    if (probeDestroy) probeDestroy(probeInstance);
    return found;
}

const char* XR_ResultToString(XrResult result) {
    if (g_xrResultToString && g_xrInstance != XR_NULL_HANDLE) {
        g_xrResultToString(g_xrInstance, result, g_resultBuf);
        return g_resultBuf;
    }
    snprintf(g_resultBuf, sizeof(g_resultBuf), "XrResult(%d)", (int)result);
    return g_resultBuf;
}

bool XR_Init(char* errMsg, int errMsgLen) {
    errMsg[0] = '\0';

    if (!XR_LoadLoader()) {
        if (g_loaderErrBuf[0]) {
            snprintf(errMsg, errMsgLen,
                "VRMOD OpenXR: Failed to load OpenXR loader library: %s\n\n"
                "On Steam Linux the host packages are often invisible to GMod.\n"
                "Solution: copy libopenxr_loader.so + ALL its dependencies (libjsoncpp etc.)\n"
                "into garrysmod/lua/bin/ next to gmcl_vrmod_linux64.dll\n"
                "See the README (\"Linux Bundling\" section) for the exact copy commands using ldd.",
                g_loaderErrBuf);
        } else {
            snprintf(errMsg, errMsgLen, "VRMOD OpenXR: Failed to load OpenXR loader library");
        }
        return false;
    }

    // 1) Create instance with OpenGL extension
    const char* extensions[] = { XR_KHR_OPENGL_ENABLE_EXTENSION_NAME };
    XrInstanceCreateInfo ici = {XR_TYPE_INSTANCE_CREATE_INFO};
    strncpy(ici.applicationInfo.applicationName, "gVRMod", XR_MAX_APPLICATION_NAME_SIZE - 1);
    ici.applicationInfo.applicationVersion = 1;
    ici.applicationInfo.engineName[0] = '\0';
    ici.applicationInfo.engineVersion = 0;
    ici.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    ici.enabledExtensionCount = 1;
    ici.enabledExtensionNames = extensions;

    XrResult res = g_xrCreateInstance(&ici, &g_xrInstance);
    if (res != XR_SUCCESS) {
        snprintf(errMsg, errMsgLen, "VRMOD OpenXR: xrCreateInstance failed (%d)", (int)res);
        return false;
    }
    VRMOD_LOG_INFO("OpenXR instance created");

    // Load all instance functions
    XR_LOAD(xrDestroyInstance);
    XR_LOAD(xrGetSystem);
    XR_LOAD(xrGetSystemProperties);
    XR_LOAD(xrCreateSession);
    XR_LOAD(xrDestroySession);
    XR_LOAD(xrBeginSession);
    XR_LOAD(xrEndSession);
    XR_LOAD(xrRequestExitSession);
    XR_LOAD(xrPollEvent);
    XR_LOAD(xrWaitFrame);
    XR_LOAD(xrBeginFrame);
    XR_LOAD(xrEndFrame);
    XR_LOAD(xrLocateViews);
    XR_LOAD(xrResultToString);
    XR_LOAD(xrEnumerateSwapchainFormats);
    XR_LOAD(xrCreateSwapchain);
    XR_LOAD(xrDestroySwapchain);
    XR_LOAD(xrEnumerateSwapchainImages);
    XR_LOAD(xrAcquireSwapchainImage);
    XR_LOAD(xrWaitSwapchainImage);
    XR_LOAD(xrReleaseSwapchainImage);
    XR_LOAD(xrCreateReferenceSpace);
    XR_LOAD(xrDestroySpace);
    XR_LOAD(xrEnumerateReferenceSpaces);
    XR_LOAD(xrEnumerateViewConfigurations);
    XR_LOAD(xrEnumerateViewConfigurationViews);
    XR_LOAD(xrCreateActionSet);
    XR_LOAD(xrDestroyActionSet);
    XR_LOAD(xrCreateAction);
    XR_LOAD(xrDestroyAction);
    XR_LOAD(xrSuggestInteractionProfileBindings);
    XR_LOAD(xrAttachSessionActionSets);
    XR_LOAD(xrSyncActions);
    XR_LOAD(xrGetActionStateBoolean);
    XR_LOAD(xrGetActionStateFloat);
    XR_LOAD(xrGetActionStateVector2f);
    XR_LOAD(xrGetActionStatePose);
    XR_LOAD(xrCreateActionSpace);
    XR_LOAD(xrLocateSpace);
    XR_LOAD(xrApplyHapticFeedback);
    XR_LOAD(xrStringToPath);
    XR_LOAD(xrPathToString);

    // 2) Get system (HMD)
    XrSystemGetInfo sgi = {XR_TYPE_SYSTEM_GET_INFO};
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    res = g_xrGetSystem(g_xrInstance, &sgi, &g_xrSystemId);
    if (res != XR_SUCCESS) {
        snprintf(errMsg, errMsgLen, "VRMOD OpenXR: No HMD found (%s)", XR_ResultToString(res));
        return false;
    }

    XrSystemProperties sysProp = {XR_TYPE_SYSTEM_PROPERTIES};
    g_xrGetSystemProperties(g_xrInstance, g_xrSystemId, &sysProp);
    VRMOD_LOG_INFO("OpenXR system: %s (vendorId=%u)", sysProp.systemName, sysProp.vendorId);

    // 3) Query view configuration
    uint32_t viewConfigCount = 0;
    g_xrEnumerateViewConfigurationViews(g_xrInstance, g_xrSystemId,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewConfigCount, nullptr);
    if (viewConfigCount < 2) {
        snprintf(errMsg, errMsgLen, "VRMOD OpenXR: Stereo view configuration not available");
        return false;
    }
    XrViewConfigurationView vcViews[2] = {
        {XR_TYPE_VIEW_CONFIGURATION_VIEW},
        {XR_TYPE_VIEW_CONFIGURATION_VIEW}
    };
    g_xrEnumerateViewConfigurationViews(g_xrInstance, g_xrSystemId,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &viewConfigCount, vcViews);
    g_xrViewConfigs[0] = vcViews[0];
    g_xrViewConfigs[1] = vcViews[1];

    uint32_t rawEyeW = vcViews[0].recommendedImageRectWidth;
    uint32_t rawEyeH = vcViews[0].recommendedImageRectHeight;

    // Per-eye recommended size. With the new proper per-eye RT path we no longer pack side-by-side in Lua.
    // Clamp each eye independently to sane GPU limits (recommended images are usually ~1-2k).
    const uint32_t maxTexSize = 4096;
    float wScale = (float)maxTexSize / (float)rawEyeW;
    float hScale = (float)maxTexSize / (float)rawEyeH;
    float scaleFactor = std::min(1.0f, std::min(wScale, hScale));

    uint32_t eyeW = (uint32_t)(rawEyeW * scaleFactor);
    uint32_t eyeH = (uint32_t)(rawEyeH * scaleFactor);

    g_xrRecommendedWidth = eyeW;
    g_xrRecommendedHeight = eyeH;

    VRMOD_LOG_INFO("Per-eye render size: %u x %u (Lua creates one RT per eye at this size)", eyeW, eyeH);

    // 4) Session creation with GL binding is *deferred* until we have a real render GLX context.
    // Creating the XrSession here (during early Lua VRMOD_Init / first GetDisplayInfo) often binds
    // to a loading/menu GL context from togl. The textures stolen/captured during actual
    // RenderScene (the RT backing the two RenderViews) live in the main world GL context.
    // Without a shared context or the correct current one at xrCreateSession time, the FBO
    // blits in xr_render produce black (or invalid texture) in the swapchain images.
    // We create the session lazily from ShareTextureBegin (inside RenderScene) so glXGetCurrent*
    // captures the context that actually owns g_sharedTexture / g_captureTexture.
    VRMOD_LOG_INFO("OpenXR instance/system ready; GL session creation deferred to first render context (Share)");
    return true;
}

// Separated so we can create the XrSession + ref spaces while the *correct* GLX context
// (the one used by GMod/togl for the VR RT and RenderViews) is current.
bool XR_CreateSessionWithCurrentGL(char* errMsg, int errMsgLen) {
    if (g_xrSession) {
        return true; // already have one
    }
    if (!g_xrInstance || g_xrSystemId == XR_NULL_SYSTEM_ID) {
        snprintf(errMsg, errMsgLen, "VRMOD OpenXR: Init not called or no system before session create");
        return false;
    }

    // Get GL requirements (optional info)
    PFN_xrGetOpenGLGraphicsRequirementsKHR xrGetOpenGLGraphicsRequirementsKHR = nullptr;
    g_xrGetInstanceProcAddr(g_xrInstance, "xrGetOpenGLGraphicsRequirementsKHR",
        (PFN_xrVoidFunction*)&xrGetOpenGLGraphicsRequirementsKHR);
    if (xrGetOpenGLGraphicsRequirementsKHR) {
        XrGraphicsRequirementsOpenGLKHR glReqs = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
        xrGetOpenGLGraphicsRequirementsKHR(g_xrInstance, g_xrSystemId, &glReqs);
        VRMOD_LOG_INFO("OpenXR GL requirements: min=%llu max=%llu",
            (unsigned long long)glReqs.minApiVersionSupported,
            (unsigned long long)glReqs.maxApiVersionSupported);
    }

    // Capture the GLX state *right now* -- caller guarantees this is the main render context.
    Display* xDisplay = glXGetCurrentDisplay();
    GLXDrawable glxDrawable = glXGetCurrentDrawable();
    GLXContext glxContext = glXGetCurrentContext();

    if (!xDisplay || !glxContext) {
        snprintf(errMsg, errMsgLen, "VRMOD OpenXR: No active GLX context at session create time (defer failed)");
        return false;
    }

    int fbConfigId = 0;
    glXQueryContext(xDisplay, glxContext, GLX_FBCONFIG_ID, &fbConfigId);

    int screen = DefaultScreen(xDisplay);
    int attribs[] = { GLX_FBCONFIG_ID, fbConfigId, None };
    int numConfigs = 0;
    GLXFBConfig* fbConfigs = glXChooseFBConfig(xDisplay, screen, attribs, &numConfigs);
    XVisualInfo* vi = nullptr;
    if (fbConfigs && numConfigs > 0) {
        vi = glXGetVisualFromFBConfig(xDisplay, fbConfigs[0]);
    }

    uint32_t visualid = 0;
    GLXFBConfig glxFBConfig = nullptr;
    if (vi) {
        visualid = vi->visualid;
        XFree(vi);
    }
    if (fbConfigs && numConfigs > 0) {
        glxFBConfig = fbConfigs[0];
        XFree(fbConfigs);
    }

    XrGraphicsBindingOpenGLXlibKHR glBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    glBinding.xDisplay = xDisplay;
    glBinding.visualid = visualid;
    glBinding.glxFBConfig = glxFBConfig;
    glBinding.glxDrawable = glxDrawable;
    glBinding.glxContext = glxContext;

    XrSessionCreateInfo sci = {XR_TYPE_SESSION_CREATE_INFO};
    sci.systemId = g_xrSystemId;
    sci.next = &glBinding;

    XrResult res = g_xrCreateSession(g_xrInstance, &sci, &g_xrSession);
    if (res != XR_SUCCESS) {
        snprintf(errMsg, errMsgLen, "VRMOD OpenXR: xrCreateSession failed (%s)", XR_ResultToString(res));
        return false;
    }
    VRMOD_LOG_INFO("OpenXR session created (with current render GLX context)");

    // Reference spaces
    XrReferenceSpaceCreateInfo stageInfo = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    stageInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    stageInfo.poseInReferenceSpace = {{0,0,0,1}, {0,0,0}};
    res = g_xrCreateReferenceSpace(g_xrSession, &stageInfo, &g_xrStageSpace);
    if (res != XR_SUCCESS) {
        VRMOD_LOG_WARN("STAGE space not available, falling back to LOCAL");
        stageInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        res = g_xrCreateReferenceSpace(g_xrSession, &stageInfo, &g_xrStageSpace);
        if (res != XR_SUCCESS) {
            snprintf(errMsg, errMsgLen, "VRMOD OpenXR: Failed to create reference space (%s)",
                XR_ResultToString(res));
            return false;
        }
    }

    XrReferenceSpaceCreateInfo viewInfo = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    viewInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    viewInfo.poseInReferenceSpace = {{0,0,0,1}, {0,0,0}};
    g_xrCreateReferenceSpace(g_xrSession, &viewInfo, &g_xrViewSpace);

    VRMOD_LOG_INFO("OpenXR GL session + spaces ready (deferred path)");
    return true;
}

bool XR_PollEvents() {
    XrEventDataBuffer eventData = {XR_TYPE_EVENT_DATA_BUFFER};
    while (g_xrPollEvent(g_xrInstance, &eventData) == XR_SUCCESS) {
        switch (eventData.type) {
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            XrEventDataSessionStateChanged* stateChange =
                (XrEventDataSessionStateChanged*)&eventData;
            g_xrSessionState = stateChange->state;
            VRMOD_LOG_INFO("OpenXR session state changed: %d", (int)g_xrSessionState);

            if (g_xrSessionState == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo bi = {XR_TYPE_SESSION_BEGIN_INFO};
                bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                XrResult res = g_xrBeginSession(g_xrSession, &bi);
                if (res == XR_SUCCESS) {
                    g_xrSessionRunning = true;
                    g_xrSessionReady = true;
                    VRMOD_LOG_INFO("OpenXR session begun");
                } else {
                    VRMOD_LOG_ERROR("xrBeginSession failed: %s", XR_ResultToString(res));
                }
            } else if (g_xrSessionState == XR_SESSION_STATE_STOPPING) {
                g_xrSessionRunning = false;
                g_xrSessionReady = false;
                g_xrEndSession(g_xrSession);
                VRMOD_LOG_INFO("OpenXR session stopped");
            } else if (g_xrSessionState == XR_SESSION_STATE_EXITING ||
                       g_xrSessionState == XR_SESSION_STATE_LOSS_PENDING) {
                g_xrSessionRunning = false;
                g_xrSessionReady = false;
                VRMOD_LOG_INFO("OpenXR session exiting/loss");
            }
            break;
        }
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            VRMOD_LOG_WARN("OpenXR instance loss pending");
            g_xrSessionRunning = false;
            return false;
        default:
            break;
        }
        eventData = {XR_TYPE_EVENT_DATA_BUFFER};
    }
    return true;
}

bool XR_WaitAndBeginFrame() {
    if (!g_xrSessionRunning) return false;

    XrFrameWaitInfo fwi = {XR_TYPE_FRAME_WAIT_INFO};
    g_xrFrameState = {XR_TYPE_FRAME_STATE};
    XrResult res = g_xrWaitFrame(g_xrSession, &fwi, &g_xrFrameState);
    if (res != XR_SUCCESS) {
        VRMOD_LOG_WARN("xrWaitFrame failed: %s", XR_ResultToString(res));
        return false;
    }

    XrFrameBeginInfo fbi = {XR_TYPE_FRAME_BEGIN_INFO};
    res = g_xrBeginFrame(g_xrSession, &fbi);
    if (res != XR_SUCCESS) {
        VRMOD_LOG_WARN("xrBeginFrame failed: %s", XR_ResultToString(res));
        return false;
    }

    return g_xrFrameState.shouldRender;
}

void XR_EndFrame() {
    // This is a no-render EndFrame (actual submission goes through xr_render.cpp)
    XrFrameEndInfo fei = {XR_TYPE_FRAME_END_INFO};
    fei.displayTime = g_xrFrameState.predictedDisplayTime;
    fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    fei.layerCount = 0;
    fei.layers = nullptr;
    g_xrEndFrame(g_xrSession, &fei);
}

// Build a projection matrix from OpenXR fov angles
static void BuildProjectionMatrix(const XrFovf& fov, float nearZ, float farZ, float outMat[16]) {
    float tanLeft   = tanf(fov.angleLeft);
    float tanRight  = tanf(fov.angleRight);
    float tanUp     = tanf(fov.angleUp);
    float tanDown   = tanf(fov.angleDown);

    float tanWidth  = tanRight - tanLeft;
    float tanHeight = tanUp - tanDown;

    memset(outMat, 0, sizeof(float) * 16);

    // Column-major OpenGL projection matrix (infinite far plane variant not needed here)
    outMat[0]  = 2.0f / tanWidth;
    outMat[5]  = 2.0f / tanHeight;
    outMat[8]  = (tanRight + tanLeft) / tanWidth;
    outMat[9]  = (tanUp + tanDown) / tanHeight;
    outMat[10] = -(farZ + nearZ) / (farZ - nearZ);
    outMat[11] = -1.0f;
    outMat[14] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
}

// Convert XrPosef quaternion to a 3x4 row-major matrix (eye-to-head transform)
static void PoseToMatrix34(const XrPosef& pose, float outMat[12]) {
    float x = pose.orientation.x;
    float y = pose.orientation.y;
    float z = pose.orientation.z;
    float w = pose.orientation.w;

    // Rotation part
    outMat[0] = 1 - 2*(y*y + z*z);
    outMat[1] = 2*(x*y - z*w);
    outMat[2] = 2*(x*z + y*w);
    outMat[3] = pose.position.x;

    outMat[4] = 2*(x*y + z*w);
    outMat[5] = 1 - 2*(x*x + z*z);
    outMat[6] = 2*(y*z - x*w);
    outMat[7] = pose.position.y;

    outMat[8]  = 2*(x*z - y*w);
    outMat[9]  = 2*(y*z + x*w);
    outMat[10] = 1 - 2*(x*x + y*y);
    outMat[11] = pose.position.z;
}

bool XR_GetDisplayInfo(float nearZ, float farZ, XrDisplayInfo* out) {
    if (!g_xrSession || !g_xrSessionRunning) return false;

    XrViewLocateInfo vli = {XR_TYPE_VIEW_LOCATE_INFO};
    vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    vli.displayTime = g_xrFrameState.predictedDisplayTime;
    vli.space = g_xrViewSpace;

    XrViewState viewState = {XR_TYPE_VIEW_STATE};
    uint32_t viewCount = 0;
    XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    XrResult res = g_xrLocateViews(g_xrSession, &vli, &viewState, 2, &viewCount, views);
    if (res != XR_SUCCESS || viewCount < 2) {
        VRMOD_LOG_WARN("xrLocateViews failed: %s", XR_ResultToString(res));
        return false;
    }

    // Build projection matrices (column-major, but Lua side reads row-major 4x4)
    // The Lua code reads it as a flat [row][col] table, so we store row-major.
    float projLeftCM[16], projRightCM[16];
    BuildProjectionMatrix(views[0].fov, nearZ, farZ, projLeftCM);
    BuildProjectionMatrix(views[1].fov, nearZ, farZ, projRightCM);

    // Transpose column-major → row-major for the Lua table
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            out->projLeft[r*4+c] = projLeftCM[c*4+r];
            out->projRight[r*4+c] = projRightCM[c*4+r];
        }

    // Eye-to-head transforms: the XrView pose (when located in the VIEW reference space)
    // directly gives the eye pose relative to the head. The translation part (column 4 in the
    // 3x4) contains the eye offset used by Lua for stereo separation (IPD).
    PoseToMatrix34(views[0].pose, out->transformLeft);
    PoseToMatrix34(views[1].pose, out->transformRight);

    // Return per-eye recommended sizes (new proper per-eye RT path).
    // Lua now allocates one RT per eye at this size and renders full viewport per eye.
    out->recommendedWidth  = g_xrRecommendedWidth;
    out->recommendedHeight = g_xrRecommendedHeight;

    return true;
}

void XR_Shutdown() {
    VRMOD_LOG_INFO("Shutting down OpenXR...");

    if (g_xrStageSpace != XR_NULL_HANDLE && g_xrDestroySpace) {
        g_xrDestroySpace(g_xrStageSpace);
        g_xrStageSpace = XR_NULL_HANDLE;
    }
    if (g_xrViewSpace != XR_NULL_HANDLE && g_xrDestroySpace) {
        g_xrDestroySpace(g_xrViewSpace);
        g_xrViewSpace = XR_NULL_HANDLE;
    }
    if (g_xrSession != XR_NULL_HANDLE && g_xrDestroySession) {
        g_xrDestroySession(g_xrSession);
        g_xrSession = XR_NULL_HANDLE;
    }
    if (g_xrInstance != XR_NULL_HANDLE && g_xrDestroyInstance) {
        g_xrDestroyInstance(g_xrInstance);
        g_xrInstance = XR_NULL_HANDLE;
    }

    g_xrSystemId = XR_NULL_SYSTEM_ID;
    g_xrSessionRunning = false;
    g_xrSessionReady = false;
    g_xrSessionState = XR_SESSION_STATE_UNKNOWN;

    if (g_loaderLib) {
        dlclose(g_loaderLib);
        g_loaderLib = nullptr;
    }
    g_xrGetInstanceProcAddr = nullptr;

    VRMOD_LOG_INFO("OpenXR shutdown complete");
}