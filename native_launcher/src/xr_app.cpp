// Cube WebUI OpenXR host — orchestration only (logic lives in modules).
#include "xr_app.hpp"

#include "panel_config.hpp"
#include "world_panel.hpp"
#include "glx_context.hpp"
#include "gl_render.hpp"
#include "xr_input.hpp"
#include "xr_util.hpp"
#include "host_cmd.hpp"
#include "launch_fill.hpp"
#include "gmod_spawn.hpp"
#include "ui_panel.hpp"
#include "math3d.hpp"

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>

#define XR_USE_PLATFORM_XLIB
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

static void Die(const char* m) { fprintf(stderr, "[cube_webui] FATAL: %s\n", m); }

int RunCubeWebUILauncher(const std::string& gmodRoot, const std::string& xrJson) {
  if (!xrJson.empty())
    setenv("XR_RUNTIME_JSON", xrJson.c_str(), 1);

  LoadPanelConfig(gmodRoot);
  const auto& cfg = PanelCfgConst();

  fprintf(stderr, "[cube_webui] OpenXR WebUI (modular host)\n");
  fprintf(stderr, "[cube_webui] GMOD=%s XR=%s\n", gmodRoot.c_str(),
          getenv("XR_RUNTIME_JSON") ? getenv("XR_RUNTIME_JSON") : "(default)");
  fprintf(stderr, "[cube_webui] TRIGGER=click GRIP=move CLOSE=exit MENU=reseed\n");
  fprintf(stderr, "[cube_webui] paint law: dirty/heartbeat (research-2) · soft_cursor=off · laser reticle\n");

  GlxContext glx{};
  if (!GlxCreate(glx)) {
    Die("GLX context failed");
    return 1;
  }

  std::vector<const char*> extList = {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
  bool wantFbPt = cfg.passthrough && XrExtensionAvailable(XR_FB_PASSTHROUGH_EXTENSION_NAME);
  if (wantFbPt) {
    extList.push_back(XR_FB_PASSTHROUGH_EXTENSION_NAME);
    fprintf(stderr, "[cube_webui] enabling %s\n", XR_FB_PASSTHROUGH_EXTENSION_NAME);
  }

  XrInstanceCreateInfo ici{XR_TYPE_INSTANCE_CREATE_INFO};
  std::strncpy(ici.applicationInfo.applicationName, "CubeWebUILauncher", XR_MAX_APPLICATION_NAME_SIZE - 1);
  ici.applicationInfo.applicationVersion = 1;
  std::strncpy(ici.applicationInfo.engineName, "gVRMod", XR_MAX_ENGINE_NAME_SIZE - 1);
  ici.applicationInfo.engineVersion = 3;
  ici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
  ici.enabledExtensionCount = (uint32_t)extList.size();
  ici.enabledExtensionNames = extList.data();

  XrInstance instance = XR_NULL_HANDLE;
  if (XR_FAILED(xrCreateInstance(&ici, &instance))) {
    if (wantFbPt) {
      fprintf(stderr, "[cube_webui] retry without FB passthrough\n");
      extList.resize(1);
      wantFbPt = false;
      ici.enabledExtensionCount = 1;
      ici.enabledExtensionNames = extList.data();
    }
    if (XR_FAILED(xrCreateInstance(&ici, &instance))) {
      Die("xrCreateInstance failed");
      GlxDestroy(glx);
      return 2;
    }
  }

  XrSystemGetInfo sgi{XR_TYPE_SYSTEM_GET_INFO};
  sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  XrSystemId systemId = XR_NULL_SYSTEM_ID;
  if (XR_FAILED(xrGetSystem(instance, &sgi, &systemId))) {
    Die("xrGetSystem failed");
    xrDestroyInstance(instance);
    GlxDestroy(glx);
    return 3;
  }

  PFN_xrGetOpenGLGraphicsRequirementsKHR pfnReq = nullptr;
  xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&pfnReq);
  if (pfnReq) {
    XrGraphicsRequirementsOpenGLKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    pfnReq(instance, systemId, &req);
  }

  XrGraphicsBindingOpenGLXlibKHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
  binding.xDisplay = glx.dpy;
  binding.glxContext = glx.ctx;
  binding.glxDrawable = glx.win;
  // Match module xr_session: real visualid + FBConfig (null/0 is heresy on some runtimes)
  binding.visualid = glx.visualid;
  binding.glxFBConfig = glx.fbConfig;

  XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
  sci.next = &binding;
  sci.systemId = systemId;
  XrSession session = XR_NULL_HANDLE;
  if (XR_FAILED(xrCreateSession(instance, &sci, &session))) {
    Die("xrCreateSession failed");
    xrDestroyInstance(instance);
    GlxDestroy(glx);
    return 4;
  }

  XrInputState input{};
  XrInputSetup(instance, session, input);

  XrEnvironmentBlendMode blendMode = XrPickBlendMode(instance, systemId, cfg.passthrough);
  const bool useAlphaClear =
      blendMode == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND ||
      blendMode == XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;

  XrPassthroughFB passthrough = XR_NULL_HANDLE;
  XrPassthroughLayerFB ptLayerHandle = XR_NULL_HANDLE;
  PFN_xrCreatePassthroughFB pfnCreatePt = nullptr;
  PFN_xrDestroyPassthroughFB pfnDestroyPt = nullptr;
  PFN_xrPassthroughStartFB pfnStartPt = nullptr;
  PFN_xrPassthroughPauseFB pfnPausePt = nullptr;
  PFN_xrCreatePassthroughLayerFB pfnCreatePtLayer = nullptr;
  PFN_xrDestroyPassthroughLayerFB pfnDestroyPtLayer = nullptr;
  if (wantFbPt) {
    xrGetInstanceProcAddr(instance, "xrCreatePassthroughFB", (PFN_xrVoidFunction*)&pfnCreatePt);
    xrGetInstanceProcAddr(instance, "xrDestroyPassthroughFB", (PFN_xrVoidFunction*)&pfnDestroyPt);
    xrGetInstanceProcAddr(instance, "xrPassthroughStartFB", (PFN_xrVoidFunction*)&pfnStartPt);
    xrGetInstanceProcAddr(instance, "xrPassthroughPauseFB", (PFN_xrVoidFunction*)&pfnPausePt);
    xrGetInstanceProcAddr(instance, "xrCreatePassthroughLayerFB", (PFN_xrVoidFunction*)&pfnCreatePtLayer);
    xrGetInstanceProcAddr(instance, "xrDestroyPassthroughLayerFB", (PFN_xrVoidFunction*)&pfnDestroyPtLayer);
    if (pfnCreatePt && pfnCreatePtLayer && pfnStartPt) {
      XrPassthroughCreateInfoFB pci{XR_TYPE_PASSTHROUGH_CREATE_INFO_FB};
      pci.flags = XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB;
      if (XR_SUCCEEDED(pfnCreatePt(session, &pci, &passthrough))) {
        XrPassthroughLayerCreateInfoFB lci{XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB};
        lci.passthrough = passthrough;
        lci.flags = XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB;
        lci.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
        if (XR_SUCCEEDED(pfnCreatePtLayer(session, &lci, &ptLayerHandle))) {
          pfnStartPt(passthrough);
          fprintf(stderr, "[cube_webui] FB passthrough layer active\n");
        } else if (pfnDestroyPt) {
          pfnDestroyPt(passthrough);
          passthrough = XR_NULL_HANDLE;
        }
      }
    }
  }

  // One reference space for eyes + panel + aim. LOCAL first (stable on WiVRn);
  // STAGE if available for room-scale. Never VIEW for content.
  XrReferenceSpaceCreateInfo rsci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  rsci.poseInReferenceSpace = IdentityPose();
  XrSpace space = XR_NULL_HANDLE;
  const char* spaceName = "LOCAL";
  rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  if (XR_FAILED(xrCreateReferenceSpace(session, &rsci, &space))) {
    Die("xrCreateReferenceSpace LOCAL failed");
    return 4;
  }
  // Prefer STAGE when it works (true room origin)
  {
    XrSpace stage = XR_NULL_HANDLE;
    rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    if (XR_SUCCEEDED(xrCreateReferenceSpace(session, &rsci, &stage))) {
      xrDestroySpace(space);
      space = stage;
      spaceName = "STAGE";
    }
  }
  fprintf(stderr, "[cube_webui] content space=%s (panel+laser+eyes same space)\n", spaceName);
  XrSpace viewSpace = XR_NULL_HANDLE;
  rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  xrCreateReferenceSpace(session, &rsci, &viewSpace);

  uint32_t viewCount = 0;
  xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                    0, &viewCount, nullptr);
  std::vector<XrViewConfigurationView> viewConfigs(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                    viewCount, &viewCount, viewConfigs.data());

  struct Eye {
    XrSwapchain swap = XR_NULL_HANDLE;
    uint32_t w = 0, h = 0;
    std::vector<XrSwapchainImageOpenGLKHR> images;
  };
  Eye eyes[2]{};
  for (uint32_t i = 0; i < 2 && i < viewCount; ++i) {
    eyes[i].w = viewConfigs[i].recommendedImageRectWidth;
    eyes[i].h = viewConfigs[i].recommendedImageRectHeight;
    XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    sc.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    sc.format = 0x8058; // GL_RGBA8
    sc.sampleCount = 1;
    sc.width = eyes[i].w;
    sc.height = eyes[i].h;
    sc.faceCount = 1;
    sc.arraySize = 1;
    sc.mipCount = 1;
    if (XR_FAILED(xrCreateSwapchain(session, &sc, &eyes[i].swap))) {
      Die("xrCreateSwapchain failed");
      return 5;
    }
    uint32_t nImg = 0;
    xrEnumerateSwapchainImages(eyes[i].swap, 0, &nImg, nullptr);
    eyes[i].images.resize(nImg, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
    xrEnumerateSwapchainImages(eyes[i].swap, nImg, &nImg,
                               (XrSwapchainImageBaseHeader*)eyes[i].images.data());
  }

  XrSessionBeginInfo sbi{XR_TYPE_SESSION_BEGIN_INFO};
  sbi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  XrSessionState state = XR_SESSION_STATE_UNKNOWN;
  bool running = true;
  bool sessionRunning = false;

  WebUIState ui{};
  WebUI_Init(ui, gmodRoot);
  ui.status = "STATIC panel · GRIP on panel = move · MENU = re-place";
  std::vector<unsigned char> panelBuf(UI_W * UI_H * 4);
  WebUI_Rasterize(ui, panelBuf.data(), nullptr);
  GLuint panelTex = GlMakeRgbaTex(UI_W, UI_H, panelBuf.data());
  GLuint fbo = 0;
  int framesNoHead = 0;

  bool prevTrigger = false;
  bool prevMenu = false;
  bool grabbing = false;
  bool worldInitPending = true;
  Vec3 grabOff{0, 0, 0};
  float stickCooldown = 0.f;
  Vec3 aimO{0, 0, 0}, aimD{0, 0, -1}, hitPt{0, 0, -1.f};
  bool aimValid = false;
  bool panelHit = false;

  while (running) {
    XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(instance, &ev) == XR_SUCCESS) {
      if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
        auto* ssc = (XrEventDataSessionStateChanged*)&ev;
        state = ssc->state;
        if (state == XR_SESSION_STATE_READY && !sessionRunning) {
          xrBeginSession(session, &sbi);
          sessionRunning = true;
          fprintf(stderr, "[cube_webui] session RUNNING\n");
        }
        if (state == XR_SESSION_STATE_STOPPING && sessionRunning) {
          xrEndSession(session);
          sessionRunning = false;
        }
        if (state == XR_SESSION_STATE_EXITING || state == XR_SESSION_STATE_LOSS_PENDING)
          running = false;
      }
      ev = {XR_TYPE_EVENT_DATA_BUFFER};
    }

    bool resetPanel = false;
    HostCmdPoll(ui, &resetPanel);
    if (resetPanel) {
      grabbing = false;
      worldInitPending = true;
    }
    if (ui.wantQuit) break;

    // StartGame → keep XR until take_xr handoff
    if (ui.wantStart && !ui.handoff) {
      WebUI_SaveBindingsIfDirty(ui);
      LaunchRequest lr = LaunchRequestFromUI(ui, gmodRoot);
      ClearCubeHandoffMarkers(gmodRoot);
      std::string err;
      int rc = SpawnGModFromWebUI(lr, err);
      fprintf(stderr, "[cube_webui] StartGame map=%s rc=%d %s\n", lr.map.c_str(), rc, err.c_str());
      ui.wantStart = false;
      if (rc == 0) {
        ui.handoff = true;
        ui.handoffMap = lr.map;
        ui.handoffPhase = "SPAWNED";
        ui.handoffDetail = "holding OpenXR · GMod booting";
        ui.handoffElapsed = 0.f;
        ui.status = "HANDOFF — STAY IN VR";
      } else {
        ui.status = "SPAWN FAIL: " + err;
      }
    }

    // Orderly OpenXR release (research-3): never destroy mid-frame without exit session.
    static bool handoffExitRequested = false;
    static float handoffExitWait = 0.f;
    if (ui.handoff) {
      ui.handoffElapsed += 1.f / 72.f;
      const bool gmodUp = GModProcessRunning();
      std::string phase = ReadCubeHandoffPhase(gmodRoot);
      if (phase.empty()) phase = gmodUp ? "gmod_process" : "waiting_process";
      ui.handoffPhase = phase;
      ui.handoffDetail = gmodUp ? "GMod up · waiting take_xr" : "waiting for GMod process…";
      bool takeXr = (phase == "take_xr" || phase == "vr_active" || phase == "ready");
      // Soft only after long wait if process is up but never signaled (was 40s — race window)
      bool soft = gmodUp && ui.handoffElapsed > 90.f;
      bool timeout = ui.handoffElapsed > 180.f;
      if ((takeXr || soft || timeout) && !handoffExitRequested) {
        handoffExitRequested = true;
        handoffExitWait = 0.f;
        fprintf(stderr, "[cube_webui] handoff release phase=%s t=%.1f (orderly xrRequestExitSession)\n",
                phase.c_str(), ui.handoffElapsed);
        if (sessionRunning) xrRequestExitSession(session);
        else running = false;
      }
      if (handoffExitRequested) {
        handoffExitWait += 1.f / 72.f;
        ui.handoffDetail = "releasing OpenXR for GMod…";
        // Session STOPPING handler ends session; leave when ended or hard cap
        if (!sessionRunning || handoffExitWait > 3.f) {
          fprintf(stderr, "[cube_webui] handoff complete sessionRunning=%d wait=%.2f\n",
                  sessionRunning ? 1 : 0, handoffExitWait);
          running = false;
          continue;
        }
      }
    }

    if (!sessionRunning) {
      usleep(10000);
      continue;
    }

    XrFrameWaitInfo fwi{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState fs{XR_TYPE_FRAME_STATE};
    xrWaitFrame(session, &fwi, &fs);
    XrFrameBeginInfo fbi{XR_TYPE_FRAME_BEGIN_INFO};
    xrBeginFrame(session, &fbi);

    XrInputSync(session, input);

    // Head pose in WORLD (STAGE/LOCAL) — used ONLY for first seed + MENU re-place.
    // NEVER call WorldPanelSeed every frame (that is HMD-driven flight heresy).
    XrPosef headWorld = IdentityPose();
    bool headOk = false;
    if (viewSpace) {
      XrSpaceLocation vloc{XR_TYPE_SPACE_LOCATION};
      if (XR_SUCCEEDED(xrLocateSpace(viewSpace, space, fs.predictedDisplayTime, &vloc))) {
        const XrSpaceLocationFlags need =
            XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        if ((vloc.locationFlags & need) == need) {
          headWorld = vloc.pose;
          headOk = true;
        }
      }
    }
    // Product law: view_lock is disabled. World freeze only.
    // (cfg.viewLock ignored — head-follow is not Cube quality.)
    if (worldInitPending) {
      if (headOk) {
        if (WorldPanelSeed(headWorld, /*force=*/false)) {
          worldInitPending = false;
          ui.status = "PANEL LIVE · grip=move · menu=re-place";
          WebUI_MarkDirty(ui);
          fprintf(stderr, "[cube_webui] PANEL VISIBLE mesh space=%s seed#%d pos=(%.2f,%.2f,%.2f)\n",
                  spaceName, WorldPanelState().seedCount,
                  WorldPanelState().c.x, WorldPanelState().c.y, WorldPanelState().c.z);
        }
      } else if (++framesNoHead > 120) {
        // Emergency place so user always sees *something*
        XrPosef fake = IdentityPose();
        fake.position.y = 1.5f;
        if (WorldPanelSeed(fake, /*force=*/false)) {
          worldInitPending = false;
          ui.status = "PANEL (no HMD pose yet)";
          WebUI_MarkDirty(ui);
          fprintf(stderr, "[cube_webui] emergency panel seed at y=1.5\n");
        }
      }
    }

    auto& wp = WorldPanelState();
    aimValid = false;
    panelHit = false;
    int hitPx = 0, hitPy = 0;
    hitPt = wp.c;
    XrPosef aimPose{};
    if (XrInputLocateAim(session, input, space, fs.predictedDisplayTime, &aimPose)) {
      aimO = {aimPose.position.x, aimPose.position.y, aimPose.position.z};
      aimValid = true;
      // OpenXR aim: -Z is forward (pointer direction)
      aimD = Normalize(QuatRotate(aimPose.orientation, V3(0, 0, -1)));
      panelHit = WorldPanelRayHit(aimO, aimD, &hitPx, &hitPy, &hitPt);
      static int aimLog = 0;
      if ((aimLog++ % 200) == 0)
        fprintf(stderr, "[cube_webui] aim (%.2f,%.2f,%.2f) dir (%.2f,%.2f,%.2f) hit=%d\n",
                aimO.x, aimO.y, aimO.z, aimD.x, aimD.y, aimD.z, panelHit ? 1 : 0);
    }

    // Grab: only while gripping AND ray hits panel (no proximity auto-grab)
    float grabVal = XrInputReadGrab(session, input);
    const bool grabHeld = grabVal >= cfg.grabThresh;
    if (grabHeld && aimValid && wp.ready && wp.frozen) {
      if (!grabbing) {
        if (panelHit) {
          grabbing = true;
          grabOff = wp.c - aimO;
          ui.status = "MOVING panel (world freeze kept)";
          WebUI_MarkDirty(ui);
        }
      }
      if (grabbing) {
        // ONLY motion path: translate frozen pose center in world meters
        WorldPanelSetCenter(aimO + grabOff);
        panelHit = WorldPanelRayHit(aimO, aimD, &hitPx, &hitPy, &hitPt);
      }
    } else if (grabbing) {
      grabbing = false;
      ui.status = "STATIC (room locked)";
      WebUI_MarkDirty(ui);
      fprintf(stderr, "[cube_webui] grab end pos=(%.3f,%.3f,%.3f) still FROZEN orient\n",
              wp.c.x, wp.c.y, wp.c.z);
    }

    if (aimValid && panelHit && !grabbing)
      WebUI_SetCursor(ui, hitPx, hitPy, true);
    else
      WebUI_SetCursor(ui, 0, 0, false);

    if (ui.page == WebUIPage::Addons) {
      if (Addons_PumpAsync(ui.addons)) WebUI_MarkDirty(ui);
    }
    // Tick idle paint budget (research-2 dirty/heartbeat)
    ui.paintFrame++;

    XrViewLocateInfo vli0{XR_TYPE_VIEW_LOCATE_INFO};
    vli0.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    vli0.displayTime = fs.predictedDisplayTime;
    vli0.space = space;
    XrViewState vs0{XR_TYPE_VIEW_STATE};
    uint32_t vc0 = 0;
    XrView views0[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    xrLocateViews(session, &vli0, &vs0, 2, &vc0, views0);

    bool trig = XrInputReadTrigger(session, input);
    bool menuBtn = XrInputReadMenu(session, input);
    // MENU re-place: edge + cooldown so chatter cannot re-seed every frame (HMD flight)
    static float menuReseedCd = 0.f;
    if (menuReseedCd > 0.f) menuReseedCd -= 1.f / 72.f;
    if (menuBtn && !prevMenu && menuReseedCd <= 0.f && headOk) {
      grabbing = false;
      if (WorldPanelSeed(headWorld, /*force=*/true)) {
        ui.status = "PANEL RE-ANCHORED (intentional)";
        WebUI_MarkDirty(ui);
        menuReseedCd = 1.5f;
      }
    }
    prevMenu = menuBtn;

    if (trig && !prevTrigger && !grabbing) {
      if (panelHit)
        WebUI_PointerClick(ui, hitPx, hitPy);
      else if (ui.cursorVisible)
        WebUI_PointerClick(ui, ui.cursorX, ui.cursorY);
      else
        WebUI_Input(ui, 0, 0, true, false);
    }
    prevTrigger = trig;

    float sx = 0.f, sy = 0.f;
    if (XrInputReadStick(session, input, &sx, &sy)) {
      if (grabbing && aimValid) {
        const float spd = 0.015f;
        WorldPanelSetCenter(wp.c + wp.right * (sx * spd) + wp.up * (sy * spd));
        grabOff = wp.c - aimO;
      } else if (stickCooldown <= 0.f) {
        int isx = 0, isy = 0;
        if (sx < -0.55f) isx = -1;
        if (sx > 0.55f) isx = 1;
        if (sy < -0.55f) isy = 1;
        if (sy > 0.55f) isy = -1;
        if (isx || isy) {
          WebUI_Input(ui, isx, isy, false, false);
          stickCooldown = 0.18f;
        }
      }
    }
    if (stickCooldown > 0.f) stickCooldown -= 1.f / 72.f;

    std::vector<XrCompositionLayerProjectionView> projViews;
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space = space;
    if (useAlphaClear)
      layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;

    XrCompositionLayerPassthroughFB ptComp{};
    if (ptLayerHandle != XR_NULL_HANDLE) {
      ptComp = {XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB};
      ptComp.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
      ptComp.space = space;
      ptComp.layerHandle = ptLayerHandle;
    }

    if (fs.shouldRender) {
      // Always keep content fresh enough; paint when dirty/heartbeat
      if (WebUI_ShouldRepaint(ui) || !wp.ready) {
        WebUI_Rasterize(ui, panelBuf.data(), nullptr);
        GlUpdateRgbaTex(panelTex, UI_W, UI_H, panelBuf.data());
        WebUI_DidRepaint(ui);
      }

      // Primary path: FROZEN world mesh in eye buffers (works on WiVRn).
      // QUAD compositor path was invisible on this runtime — do not rely on it.
      projViews.resize(2);
      for (int eye = 0; eye < 2; ++eye) {
        XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t idx = 0;
        xrAcquireSwapchainImage(eyes[eye].swap, &ai, &idx);
        XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wi.timeout = XR_INFINITE_DURATION;
        xrWaitSwapchainImage(eyes[eye].swap, &wi);

        GLuint color = eyes[eye].images[idx].image;
        GlBindSwapchainFbo(color, &fbo);
        glViewport(0, 0, eyes[eye].w, eyes[eye].h);
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        if (useAlphaClear)
          glClearColor(0.f, 0.f, 0.f, 0.f);
        else
          glClearColor(0.04f, 0.02f, 0.03f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        GlLoadProjectionFov(views0[eye].fov);
        glMatrixMode(GL_MODELVIEW);
        GlLoadModelviewLocal(views0[eye].pose);
        if (wp.ready)
          GlDrawWorldPanel(panelTex);
        if (aimValid) {
          Vec3 tip = panelHit ? hitPt : (aimO + aimD * 2.5f);
          float cr = grabbing ? 0.3f : (panelHit ? 1.f : 0.45f);
          float cg = grabbing ? 0.9f : (panelHit ? 0.2f : 0.45f);
          float cb = grabbing ? 0.4f : (panelHit ? 0.35f : 0.5f);
          GlDrawLaser(aimO, tip, cr, cg, cb);
        }
        GlUnbindFbo();
        glDisable(GL_DEPTH_TEST);

        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xrReleaseSwapchainImage(eyes[eye].swap, &ri);

        projViews[eye] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        projViews[eye].pose = views0[eye].pose;
        projViews[eye].fov = views0[eye].fov;
        projViews[eye].subImage.swapchain = eyes[eye].swap;
        projViews[eye].subImage.imageRect.offset = {0, 0};
        projViews[eye].subImage.imageRect.extent = {(int32_t)eyes[eye].w, (int32_t)eyes[eye].h};
      }
      layer.viewCount = 2;
      layer.views = projViews.data();
    }

    XrFrameEndInfo fei{XR_TYPE_FRAME_END_INFO};
    fei.displayTime = fs.predictedDisplayTime;
    fei.environmentBlendMode = blendMode;
    const XrCompositionLayerBaseHeader* layers[2] = {};
    uint32_t layerCount = 0;
    if (fs.shouldRender && !projViews.empty()) {
      if (ptLayerHandle != XR_NULL_HANDLE)
        layers[layerCount++] = (const XrCompositionLayerBaseHeader*)&ptComp;
      layers[layerCount++] = (const XrCompositionLayerBaseHeader*)&layer;
      fei.layerCount = layerCount;
      fei.layers = layers;
    }
    xrEndFrame(session, &fei);
  }

  for (int i = 0; i < 2; ++i)
    if (eyes[i].swap) xrDestroySwapchain(eyes[i].swap);
  if (ptLayerHandle != XR_NULL_HANDLE && pfnDestroyPtLayer) pfnDestroyPtLayer(ptLayerHandle);
  if (passthrough != XR_NULL_HANDLE) {
    if (pfnPausePt) pfnPausePt(passthrough);
    if (pfnDestroyPt) pfnDestroyPt(passthrough);
  }
  XrInputDestroy(input);
  if (viewSpace) xrDestroySpace(viewSpace);
  if (space) xrDestroySpace(space);
  if (session) xrDestroySession(session);
  if (instance) xrDestroyInstance(instance);
  if (panelTex) glDeleteTextures(1, &panelTex);
  GlxDestroy(glx);
  fprintf(stderr, "[cube_webui] exit\n");
  return 0;
}
