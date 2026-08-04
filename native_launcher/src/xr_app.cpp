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
#include "stage_pack.hpp"
#include "ambient_clip.hpp"
#include "cube_return.hpp"
#include "warm_reuse.hpp"
#include "ui_panel.hpp"
#include "smx_player.hpp"
#include "math3d.hpp"

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>

#define XR_USE_PLATFORM_XLIB
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <algorithm>
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
  fprintf(stderr, "[cube_webui] TRIGGER=click · MENU=re-place · CLOSE=exit · grab=%s\n",
          cfg.grabEnable ? "on" : "off");
  fprintf(stderr, "[cube_webui] seamless: eye-pose seed · dual-hand L+R · Cube theme · SMX\n");

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
  // G03: last valid head sample in content space (for stage pack continuity)
  float lastHeadX = 0.f, lastHeadY = 0.f, lastHeadZ = 0.f;
  bool lastHeadOk = false;
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

  bool axisLatchedL = false, axisLatchedR = false; // hysteresis re-arm for stuck float
  bool prevMenu = false;
  bool grabbing = false;
  XrHand grabHand = XrHand::Right;
  float grabArmL = 0.f, grabArmR = 0.f; // sustained-grip arming (Meta Cam: thrash-grab)
  float grabCooldown = 0.f;              // post-end lockout so grip flicker cannot re-grab
  bool worldInitPending = true;
  bool emergencySeedOnly = false; // true after fake seed — re-anchor on first real HMD

  SmxPeerState smx{};
  SmxInit(smx, "gvrmod-cube-shell");
  Vec3 grabOff{0, 0, 0};
  float stickCooldown = 0.f;
  // Primary laser (best tracking / active hand) + optional second laser
  Vec3 aimO{0, 0, 0}, aimD{0, 0, -1}, hitPt{0, 0, -1.f};
  Vec3 aimOL{0, 0, 0}, aimDL{0, 0, -1}, hitPtL{0, 0, -1.f};
  Vec3 aimOR{0, 0, 0}, aimDR{0, 0, -1}, hitPtR{0, 0, -1.f};
  bool aimValid = false, aimValidL = false, aimValidR = false;
  bool panelHit = false, panelHitL = false, panelHitR = false;
  int hitPx = 0, hitPy = 0, hitPxL = 0, hitPyL = 0, hitPxR = 0, hitPyR = 0;

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
    // Orderly quit: request exit session first — mid-frame destroy aborts ("terminate…")
    static bool quitRequested = false;
    if (ui.wantQuit && !quitRequested) {
      quitRequested = true;
      fprintf(stderr, "[cube_webui] quit requested — orderly xrRequestExitSession\n");
      if (sessionRunning) xrRequestExitSession(session);
      else running = false;
    }
    if (quitRequested && (!sessionRunning)) {
      running = false;
      break;
    }

    // StartGame → keep XR until take_xr handoff
    if (ui.wantStart && !ui.handoff) {
      WebUI_SaveBindingsIfDirty(ui);
      LaunchRequest lr = LaunchRequestFromUI(ui, gmodRoot);
      ClearCubeHandoffMarkers(gmodRoot);
      // G04: pure warm-reuse decision (feature hard-off → warm_request + still cold-spawn)
      const bool alreadyUp = GModProcessRunning();
      WarmReuseDecision warm = CubeWarmReuseDecide(alreadyUp, /*forceCold=*/false, lr.map);
      ui.handoffBootKind = CubeWarmReuseBootKind(warm);
      if (warm.action == "warm_request" || warm.action == "warm_reuse") {
        WarmRequestSnapshot wr;
        wr.action = warm.action;
        wr.reason = warm.reason;
        wr.map = lr.map;
        wr.source = "cube_webui";
        WriteCubeWarmRequest(gmodRoot, wr);
      }
      if (CubeLaunchShouldSkipSpawn(warm)) {
        // Only when CubeWarmReuseEnabled() — not active; keep branch for future attach
        ui.wantStart = false;
        ui.status = "WARM REUSE · MAP ATTACH (EXPERIMENTAL)";
        ui.handoff = true;
        ui.handoffMap = lr.map;
        ui.handoffPhase = "gmod_process";
        ui.handoffDetail = CubeWarmReuseDetail(warm);
        ui.handoffElapsed = 0.f;
      } else {
      std::string err;
      int rc = SpawnGModFromWebUI(lr, err);
      fprintf(stderr, "[cube_webui] StartGame map=%s rc=%d boot=%s warm=%s %s\n",
              lr.map.c_str(), rc, ui.handoffBootKind.c_str(), warm.reason.c_str(), err.c_str());
      ui.wantStart = false;
      if (rc == 0) {
        // G11: remember map + gfx for next Cube session Quick Play
        WebUI_SaveLastPlay(ui);
        ui.handoff = true;
        ui.handoffMap = lr.map;
        ui.handoffPhase = "SPAWNED";
        ui.handoffDetail = CubeWarmReuseDetail(warm);
        ui.handoffElapsed = 0.f;
        ui.status = "HANDOFF — STAY IN VR";
        // G03: pack STAGE/LOCAL + head sample for GMod (no apply yet — continuity data only)
        {
          StagePackSnapshot pack;
          pack.refSpace = spaceName ? spaceName : "LOCAL";
          pack.headX = lastHeadX;
          pack.headY = lastHeadY;
          pack.headZ = lastHeadZ;
          pack.headOk = lastHeadOk;
          pack.viewScale = lr.gfx.xrViewScale;
          pack.scaleFactor = lr.gfx.xrScaleFactor;
          pack.supersample = lr.gfx.xrSupersample;
          pack.map = lr.map;
          pack.source = "cube_webui";
          WriteCubeStagePack(gmodRoot, pack);
          ui.handoffRefSpace = StagePack_NormalizeSpace(pack.refSpace);
          ui.handoffHeadY = pack.headY;
          ui.handoffHeadOk = pack.headOk;
        }
      } else {
        ui.status = "SPAWN FAIL: " + err;
      }
      } // else cold-spawn path
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
      // G01: phase-aware detail (status file tokens → intentional copy, not opaque hold)
      ui.handoffDetail = CubeHandoffDetailForPhase(phase, gmodUp);
      // G02: panel-side fade amount (phase pre-dim + ramp during orderly exit)
      ui.handoffFade = CubeHandoffFadeAmount(phase, handoffExitRequested, handoffExitWait);
      // G12: ambient gain law + status file (clip contract; no OpenAL yet)
      ui.handoffAudioGain = CubeHandoffAudioGain(phase, handoffExitRequested, handoffExitWait);
      {
        static float lastAmbGain = -1.f;
        static bool lastAmbPlay = false;
        AmbientClipSnapshot amb;
        amb.gain = ui.handoffAudioGain;
        amb.handoff = true;
        amb.playing = CubeAmbient_ShouldPlay(amb.gain, true);
        amb.clip_rel = CubeAmbient_DefaultClipRel();
        amb.source = "cube_webui_handoff";
        // Throttle disk writes: on play edge or gain step ≥5%
        const bool edge = (amb.playing != lastAmbPlay);
        const bool step = (lastAmbGain < 0.f) || (std::fabs(amb.gain - lastAmbGain) >= 0.05f);
        if (edge || step) {
          WriteCubeAmbientStatus(gmodRoot, amb);
          lastAmbGain = amb.gain;
          lastAmbPlay = amb.playing;
        }
      }
      WebUI_MarkDirty(ui);
      bool takeXr = (phase == "take_xr" || phase == "vr_active" || phase == "ready");
      // Soft only after long wait if process is up but never signaled (was 40s — race window)
      bool soft = gmodUp && ui.handoffElapsed > 90.f;
      bool timeout = ui.handoffElapsed > 180.f;
      if ((takeXr || soft || timeout) && !handoffExitRequested) {
        handoffExitRequested = true;
        handoffExitWait = 0.f;
        fprintf(stderr, "[cube_webui] handoff release phase=%s t=%.1f (orderly xrRequestExitSession)\n",
                phase.c_str(), ui.handoffElapsed);
        // G03: refresh stage pack with final head sample before runtime release
        {
          StagePackSnapshot pack;
          pack.refSpace = spaceName ? spaceName : "LOCAL";
          pack.headX = lastHeadX;
          pack.headY = lastHeadY;
          pack.headZ = lastHeadZ;
          pack.headOk = lastHeadOk;
          pack.viewScale = ui.gfx.xr.viewScale;
          pack.scaleFactor = ui.gfx.xr.scaleFactor;
          {
            static const float kSs[] = {0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
            int i = ui.gfx.xr.ssIdx;
            if (i < 0) i = 0;
            if (i > 5) i = 5;
            pack.supersample = kSs[i];
          }
          pack.map = ui.handoffMap;
          pack.source = "cube_webui_take_xr";
          WriteCubeStagePack(gmodRoot, pack);
          ui.handoffRefSpace = StagePack_NormalizeSpace(pack.refSpace);
          ui.handoffHeadY = pack.headY;
          ui.handoffHeadOk = pack.headOk;
        }
        if (sessionRunning) xrRequestExitSession(session);
        else running = false;
      }
      if (handoffExitRequested) {
        handoffExitWait += 1.f / 72.f;
        ui.handoffDetail = "coordinated fade · releasing OpenXR for GMod…";
        ui.handoffFade = CubeHandoffFadeAmount(phase, true, handoffExitWait);
        ui.handoffAudioGain = CubeHandoffAudioGain(phase, true, handoffExitWait);
        // Session STOPPING handler ends session; leave when ended or hard cap
        if (!sessionRunning || handoffExitWait > 3.f) {
          fprintf(stderr, "[cube_webui] handoff complete sessionRunning=%d wait=%.2f\n",
                  sessionRunning ? 1 : 0, handoffExitWait);
          running = false;
          continue;
        }
      }
    } else {
      // G13: poll cube_return.txt while Cube panel is live (not during Start handoff).
      // Auto reclaim remains hard-off — surface RETURN line only.
      static float returnPollAccum = 0.f;
      static std::string lastReturnKey;
      returnPollAccum += 1.f / 72.f;
      if (returnPollAccum >= 1.f) {
        returnPollAccum = 0.f;
        CubeReturnSnapshot ret;
        const bool have = ReadCubeReturnMarker(gmodRoot, ret);
        CubeReclaimDecision dec = CubeReclaimDecide(have, ret);
        // Feature branch reserved: never execute auto reclaim while hard-off
        if (dec.auto_reclaim && CubeReclaimEnabled()) {
          // Future: orderly reverse claim path. Intentionally empty.
        }
        const bool active = dec.show_panel;
        std::string label = CubeReclaimPanelLabel(dec);
        std::string detail = CubeReclaimDetail(dec);
        std::string key = active ? (dec.phase + "|" + dec.map + "|" + dec.action) : std::string();
        if (key != lastReturnKey) {
          lastReturnKey = key;
          ui.returnActive = active;
          ui.returnPhase = active ? dec.phase : std::string();
          ui.returnMap = active ? dec.map : std::string();
          ui.returnLabel = label;
          ui.returnDetail = detail;
          if (active) {
            fprintf(stderr, "[cube_webui] G13 return poll phase=%s action=%s map=%s reclaim=%d\n",
                    dec.phase.c_str(), dec.action.c_str(), dec.map.c_str(),
                    dec.auto_reclaim ? 1 : 0);
            // Soft status nudge once per marker key (do not thrash user status)
            if (ui.status.find("RETURN") == std::string::npos)
              ui.status = label.empty() ? "RETURN · POLL" : label;
          }
          WebUI_MarkDirty(ui);
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

    // Head pose in WORLD (STAGE/LOCAL) — seed + MENU re-place only (not per-frame follow).
    XrPosef headWorld = IdentityPose();
    bool headOk = false;
    auto acceptHead = [&](const XrPosef& p, XrSpaceLocationFlags flags) {
      const XrSpaceLocationFlags need =
          XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
      if ((flags & need) != need) return false;
      // Reject origin-stuck junk (WiVRn pre-track often reports 0,0,0)
      float d2 = p.position.x * p.position.x + p.position.y * p.position.y +
                 p.position.z * p.position.z;
      if (d2 < 0.0001f && std::fabs(p.orientation.w) > 0.99f) return false;
      headWorld = p;
      return true;
    };
    if (viewSpace) {
      XrSpaceLocation vloc{XR_TYPE_SPACE_LOCATION};
      if (XR_SUCCEEDED(xrLocateSpace(viewSpace, space, fs.predictedDisplayTime, &vloc)))
        headOk = acceptHead(vloc.pose, vloc.locationFlags);
    }
    // Seamless fallback: eye poses from LocateViews (often valid when VIEW space is not)
    XrViewLocateInfo vliHead{XR_TYPE_VIEW_LOCATE_INFO};
    vliHead.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    vliHead.displayTime = fs.predictedDisplayTime;
    vliHead.space = space;
    XrViewState vsHead{XR_TYPE_VIEW_STATE};
    uint32_t vcHead = 0;
    XrView viewsHead[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    if (XR_SUCCEEDED(xrLocateViews(session, &vliHead, &vsHead, 2, &vcHead, viewsHead)) &&
        vcHead >= 1) {
      if (!headOk) {
        // Mid-eye between L/R if both present
        if (vcHead >= 2) {
          XrPosef mid = viewsHead[0].pose;
          mid.position.x = 0.5f * (viewsHead[0].pose.position.x + viewsHead[1].pose.position.x);
          mid.position.y = 0.5f * (viewsHead[0].pose.position.y + viewsHead[1].pose.position.y);
          mid.position.z = 0.5f * (viewsHead[0].pose.position.z + viewsHead[1].pose.position.z);
          mid.orientation = viewsHead[0].pose.orientation;
          headOk = acceptHead(mid, XR_SPACE_LOCATION_POSITION_VALID_BIT |
                                       XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
        } else {
          headOk = acceptHead(viewsHead[0].pose, XR_SPACE_LOCATION_POSITION_VALID_BIT |
                                                     XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
        }
      }
    }
    // G03: remember last good head for stage pack (Start + take_xr)
    if (headOk) {
      lastHeadX = headWorld.position.x;
      lastHeadY = headWorld.position.y;
      lastHeadZ = headWorld.position.z;
      lastHeadOk = true;
      if (ui.handoff) {
        ui.handoffHeadY = lastHeadY;
        ui.handoffHeadOk = true;
      }
    }

    // Product law: freeze after seed. Emergency origin seed re-anchors when head is real.
    if (worldInitPending || emergencySeedOnly) {
      if (headOk) {
        if (WorldPanelSeed(headWorld, /*force=*/emergencySeedOnly || worldInitPending)) {
          worldInitPending = false;
          emergencySeedOnly = false;
          ui.status = "PANEL LIVE · trigger=click · MENU=re-place";
          WebUI_MarkDirty(ui);
          fprintf(stderr, "[cube_webui] PANEL VISIBLE mesh space=%s seed#%d pos=(%.2f,%.2f,%.2f)\n",
                  spaceName, WorldPanelState().seedCount,
                  WorldPanelState().c.x, WorldPanelState().c.y, WorldPanelState().c.z);
        }
      } else if (worldInitPending && ++framesNoHead > 60) {
        XrPosef fake = IdentityPose();
        fake.position.y = 1.5f;
        if (WorldPanelSeed(fake, /*force=*/false)) {
          worldInitPending = false;
          emergencySeedOnly = true;
          ui.status = "PANEL · waiting track (seamless re-anchor)";
          WebUI_MarkDirty(ui);
          fprintf(stderr, "[cube_webui] emergency panel seed — will re-anchor on first head/eye pose\n");
        }
      }
    }
    // Soft catch-up: if head is valid and panel is absurdly far, re-anchor once (no every-frame)
    static bool farReseedDone = false;
    if (headOk && WorldPanelState().ready && WorldPanelState().frozen && !ui.handoff) {
      float dx = headWorld.position.x - WorldPanelState().c.x;
      float dy = headWorld.position.y - WorldPanelState().c.y;
      float dz = headWorld.position.z - WorldPanelState().c.z;
      float dist2 = dx * dx + dy * dy + dz * dz;
      if (dist2 > 9.f && !farReseedDone) { // >3m
        if (WorldPanelSeed(headWorld, /*force=*/true)) {
          farReseedDone = true;
          ui.status = "PANEL RE-ANCHORED (seamless catch-up)";
          WebUI_MarkDirty(ui);
          fprintf(stderr, "[cube_webui] seamless far re-anchor dist=%.2f\n", std::sqrt(dist2));
        }
      }
      if (dist2 < 4.f) farReseedDone = false; // allow again if they walk away later
    }

    auto& wp = WorldPanelState();
    // If HMD is looking at the back, flip so readable image + front-only hits match
    if (headOk && wp.ready && wp.frozen && !ui.handoff) {
      static float faceCd = 0.f;
      if (faceCd > 0.f) faceCd -= 1.f / 72.f;
      else if (WorldPanelEnsureFaceToward(
                   {headWorld.position.x, headWorld.position.y, headWorld.position.z})) {
        faceCd = 1.0f;
        ui.status = "PANEL FACING YOU (front = image + clicks)";
        WebUI_MarkDirty(ui);
      }
    }
    // ── Dual-hand async: each hand has its own aim ray + trigger + grab ──
    aimValidL = aimValidR = aimValid = false;
    panelHitL = panelHitR = panelHit = false;
    hitPx = hitPy = hitPxL = hitPyL = hitPxR = hitPyR = 0;
    hitPt = hitPtL = hitPtR = wp.c;

    auto locateHand = [&](XrHand hand, Vec3& o, Vec3& d, bool& valid, bool& hit,
                          int& px, int& py, Vec3& hp) {
      XrPosef pose{};
      if (!XrInputLocateAimHand(session, input, hand, space, fs.predictedDisplayTime, &pose))
        return;
      // Aim pose origin = laser root (OpenXR touch aim). Same o/d for draw + hit.
      o = {pose.position.x, pose.position.y, pose.position.z};
      valid = true;
      // OpenXR aim forward is always -Z. Do NOT try +Z: two-sided hits made the
      // laser tip land on one face while click used the opposite intersection.
      d = Normalize(QuatRotate(pose.orientation, V3(0, 0, -1)));
      hit = WorldPanelRayHit(o, d, &px, &py, &hp, 1.20f);
      if (!hit) {
        px = 0;
        py = 0;
        hp = o + d * 1.8f;
      }
    };
    locateHand(XrHand::Left, aimOL, aimDL, aimValidL, panelHitL, hitPxL, hitPyL, hitPtL);
    locateHand(XrHand::Right, aimOR, aimDR, aimValidR, panelHitR, hitPxR, hitPyR, hitPtR);

    // Head-gaze fallback: if both controllers miss but user looks at panel, use head ray
    // for cursor/dwell (still need trigger/dwell to click).
    bool panelHitHead = false;
    int hitPxH = 0, hitPyH = 0;
    Vec3 hitPtH = wp.c;
    if (headOk && !panelHitL && !panelHitR) {
      Vec3 ho = {headWorld.position.x, headWorld.position.y, headWorld.position.z};
      Vec3 hd = Normalize(QuatRotate(headWorld.orientation, V3(0, 0, -1)));
      panelHitHead = WorldPanelRayHit(ho, hd, &hitPxH, &hitPyH, &hitPtH, 1.2f);
      if (panelHitHead) {
        aimO = ho; aimD = hd; aimValid = true; panelHit = true;
        hitPx = hitPxH; hitPy = hitPyH; hitPt = hitPtH;
      }
    }

    // Primary laser for cursor: prefer hand that hits panel; else head; else best tracked.
    if (panelHitL && !panelHitR) {
      aimO = aimOL; aimD = aimDL; aimValid = true; panelHit = true;
      hitPx = hitPxL; hitPy = hitPyL; hitPt = hitPtL;
    } else if (panelHitR && !panelHitL) {
      aimO = aimOR; aimD = aimDR; aimValid = true; panelHit = true;
      hitPx = hitPxR; hitPy = hitPyR; hitPt = hitPtR;
    } else if (panelHitL && panelHitR) {
      // Both hit: prefer right for cursor stability, left still independent for click
      aimO = aimOR; aimD = aimDR; aimValid = true; panelHit = true;
      hitPx = hitPxR; hitPy = hitPyR; hitPt = hitPtR;
    } else if (panelHitHead) {
      // head ray already filled aimO/hitPx — keep for dwell/cursor
    } else if (aimValidR) {
      aimO = aimOR; aimD = aimDR; aimValid = true;
    } else if (aimValidL) {
      aimO = aimOL; aimD = aimDL; aimValid = true;
    }

    static int aimLog = 0;
    if ((aimLog++ % 600) == 0)
      fprintf(stderr,
              "[cube_webui] aim L hit=%d R hit=%d primary=%d head=%d trigL=%d trigR=%d\n",
              panelHitL ? 1 : 0, panelHitR ? 1 : 0, panelHit ? 1 : 0, headOk ? 1 : 0,
              XrInputReadTriggerHand(session, input, XrHand::Left, cfg.triggerThresh) ? 1 : 0,
              XrInputReadTriggerHand(session, input, XrHand::Right, cfg.triggerThresh) ? 1 : 0);

    float grabL = XrInputReadGrabHand(session, input, XrHand::Left);
    float grabR = XrInputReadGrabHand(session, input, XrHand::Right);
    const float grabOn = cfg.grabThresh;
    const float grabOffHyst = std::max(0.40f, grabOn - 0.18f);
    const bool grabEngageL = cfg.grabEnable && (grabL >= grabOn);
    const bool grabEngageR = cfg.grabEnable && (grabR >= grabOn);
    const bool grabHeldL = grabbing && grabHand == XrHand::Left ? (grabL >= grabOffHyst)
                                                               : grabEngageL;
    const bool grabHeldR = grabbing && grabHand == XrHand::Right ? (grabR >= grabOffHyst)
                                                                : grabEngageR;
    const float dt = 1.f / 72.f;
    if (grabCooldown > 0.f) grabCooldown = std::max(0.f, grabCooldown - dt);
    // Short arm (~120ms) so deliberate squeeze grabs without resting thrash
    const float grabArmNeed = 0.12f;
    auto armGrip = [&](bool engage, float& arm) {
      if (engage) arm = std::min(grabArmNeed + 0.05f, arm + dt);
      else arm = 0.f;
    };
    armGrip(grabEngageL, grabArmL);
    armGrip(grabEngageR, grabArmR);

    // Trigger sample: float axis + face-button EDGE only.
    // NEVER treat clickDown level as held — WiVRn stuck clk=1 spammed enter-clicks
    // and cancelled grab every frame.
    const float pressTh = cfg.triggerThresh;
    const float releaseTh = std::max(0.12f, pressTh * 0.45f);
    auto sampL = XrInputSampleTriggerHand(session, input, XrHand::Left, pressTh);
    auto sampR = XrInputSampleTriggerHand(session, input, XrHand::Right, pressTh);
    // Axis for hysteresis: float only (not face-button level)
    auto axisEdge = [](float axis, bool ok, float press, float release, bool& latched) -> bool {
      const float a = ok ? axis : 0.f;
      if (!latched && a > press) {
        latched = true;
        return true;
      }
      if (latched && a < release) latched = false;
      return false;
    };
    const bool edgeL =
        sampL.clickEdge || axisEdge(sampL.axis, sampL.axisOk, pressTh, releaseTh, axisLatchedL);
    const bool edgeR =
        sampR.clickEdge || axisEdge(sampR.axis, sampR.axisOk, pressTh, releaseTh, axisLatchedR);
    // Level "held" for press-then-aim: axis latch only, never stuck face-button level
    bool trigLEarly = axisLatchedL || (sampL.axisOk && sampL.axis > pressTh);
    bool trigREarly = axisLatchedR || (sampR.axisOk && sampR.axis > pressTh);
    // Only EDGE cancels grab — stuck clk/trig level must not freeze grip-move
    const bool anyTrigEdge = edgeL || edgeR;

    // Always write lightweight live log (~5 Hz) — no MarkDirty (was killing FPS)
    {
      static int dbgN = 0;
      if ((dbgN++ % 14) == 0) {
        FILE* df = fopen("/tmp/cube_live.txt", "w");
        if (df) {
          fprintf(df,
                  "t=%.2f head=%d sess=%d\n"
                  "L aim=%d hit=%d px=%d py=%d ax=%.2f clk=%d edge=%d grab=%.2f\n"
                  "R aim=%d hit=%d px=%d py=%d ax=%.2f clk=%d edge=%d grab=%.2f\n"
                  "headHit=%d px=%d py=%d\n"
                  "panel=(%.2f,%.2f,%.2f) Lpos=(%.2f,%.2f,%.2f) Rpos=(%.2f,%.2f,%.2f)\n",
                  (double)(dbgN / 72.f), headOk ? 1 : 0, sessionRunning ? 1 : 0,
                  aimValidL ? 1 : 0, panelHitL ? 1 : 0, hitPxL, hitPyL, sampL.axis,
                  sampL.clickDown ? 1 : 0, edgeL ? 1 : 0, grabL,
                  aimValidR ? 1 : 0, panelHitR ? 1 : 0, hitPxR, hitPyR, sampR.axis,
                  sampR.clickDown ? 1 : 0, edgeR ? 1 : 0, grabR,
                  panelHitHead ? 1 : 0, hitPxH, hitPyH, wp.c.x, wp.c.y, wp.c.z,
                  aimOL.x, aimOL.y, aimOL.z, aimOR.x, aimOR.y, aimOR.z);
          fclose(df);
        }
      }
    }

    if (!cfg.grabEnable) {
      grabbing = false;
      grabArmL = grabArmR = 0.f;
    } else if (grabbing) {
      bool still = (grabHand == XrHand::Left) ? grabHeldL : grabHeldR;
      // Trigger EDGE aborts grab (not stuck level)
      if (anyTrigEdge) {
        grabbing = false;
        grabArmL = grabArmR = 0.f;
        grabCooldown = 0.35f;
        ui.status = "STATIC · click priority";
        WebUI_MarkDirty(ui);
      } else if (still) {
        Vec3 go = (grabHand == XrHand::Left) ? aimOL : aimOR;
        Vec3 gd = (grabHand == XrHand::Left) ? aimDL : aimDR;
        bool gvalid = (grabHand == XrHand::Left) ? aimValidL : aimValidR;
        if (gvalid) {
          WorldPanelSetCenter(go + grabOff);
          int gpx = 0, gpy = 0;
          Vec3 ghp = wp.c;
          bool ghit = WorldPanelRayHit(go, gd, &gpx, &gpy, &ghp, 1.35f);
          aimO = go; aimD = gd; aimValid = true; panelHit = ghit;
          hitPx = gpx; hitPy = gpy; hitPt = ghp;
        }
      } else {
        grabbing = false;
        grabCooldown = 0.25f;
        ui.status = "STATIC (room locked)";
        WebUI_MarkDirty(ui);
        fprintf(stderr, "[cube_webui] grab end hand=%s pos=(%.3f,%.3f,%.3f)\n",
                grabHand == XrHand::Left ? "L" : "R", wp.c.x, wp.c.y, wp.c.z);
      }
    } else if (wp.ready && wp.frozen && !anyTrigEdge && grabCooldown <= 0.f) {
      // Start grab only with sustained squeeze WHILE laser hits panel
      if (grabArmL >= grabArmNeed && aimValidL && panelHitL) {
        grabbing = true;
        grabHand = XrHand::Left;
        grabOff = wp.c - aimOL;
        ui.status = "MOVING panel (left grip)";
        WebUI_MarkDirty(ui);
        fprintf(stderr, "[cube_webui] grab start hand=L\n");
      } else if (grabArmR >= grabArmNeed && aimValidR && panelHitR) {
        grabbing = true;
        grabHand = XrHand::Right;
        grabOff = wp.c - aimOR;
        ui.status = "MOVING panel (right grip)";
        WebUI_MarkDirty(ui);
        fprintf(stderr, "[cube_webui] grab start hand=R\n");
      }
    }

    // Cursor from primary panel hit (either hand)
    if (aimValid && panelHit && !grabbing)
      WebUI_SetCursor(ui, hitPx, hitPy, true);
    else
      WebUI_SetCursor(ui, 0, 0, false);

    if (ui.page == WebUIPage::Addons) {
      if (Addons_PumpAsync(ui.addons)) WebUI_MarkDirty(ui);
    }
    // Tick idle paint budget (research-2 dirty/heartbeat)
    ui.paintFrame++;

    // Reuse early eye locate when possible (seamless: one LocateViews per frame)
    XrViewState vs0 = vsHead;
    uint32_t vc0 = vcHead;
    XrView views0[2] = {viewsHead[0], viewsHead[1]};
    if (vc0 < 1) {
      XrViewLocateInfo vli0{XR_TYPE_VIEW_LOCATE_INFO};
      vli0.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
      vli0.displayTime = fs.predictedDisplayTime;
      vli0.space = space;
      vs0 = {XR_TYPE_VIEW_STATE};
      vc0 = 0;
      views0[0] = {XR_TYPE_VIEW};
      views0[1] = {XR_TYPE_VIEW};
      xrLocateViews(session, &vli0, &vs0, 2, &vc0, views0);
    }

    bool trigL = trigLEarly;
    bool trigR = trigREarly;
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

    // Ray-plane click: EDGE only while on panel (+ dwell). No enter-while-held —
    // stuck face-button level made that spam CLICK every hit flicker.
    static float clickCd = 0.f;
    if (clickCd > 0.f) clickCd -= dt;
    auto fireClick = [&](int px, int py, const char* which) {
      if (clickCd > 0.f || grabbing) return;
      clickCd = 0.28f;
      fprintf(stderr, "[cube_webui] CLICK %s px=%d py=%d (ray on plane)\n", which, px, py);
      WebUI_PointerClick(ui, px, py);
      char st[96];
      snprintf(st, sizeof st, "CLICK %s @ %d,%d", which, px, py);
      ui.status = st;
      WebUI_MarkDirty(ui);
    };
    if (edgeL && panelHitL) fireClick(hitPxL, hitPyL, "L");
    if (edgeR && panelHitR) fireClick(hitPxR, hitPyR, "R");
    if (!panelHitL && !panelHitR && panelHitHead && (edgeL || edgeR))
      fireClick(hitPxH, hitPyH, "HEAD");

    // Dwell click: stay on roughly same panel pixel → click (no button needed)
    static float dwellT = 0.f;
    static int dwellPx = -1, dwellPy = -1;
    static bool dwellFired = false;
    int dpx = -1, dpy = -1;
    bool dhit = false;
    if (panelHitR) {
      dhit = true;
      dpx = hitPxR;
      dpy = hitPyR;
    } else if (panelHitL) {
      dhit = true;
      dpx = hitPxL;
      dpy = hitPyL;
    } else if (panelHitHead) {
      dhit = true;
      dpx = hitPxH;
      dpy = hitPyH;
    }
    // No dwell on CLOSE (bottom-left). Dwell 0.75s elsewhere when buttons are dead.
    const bool onClose = (dpy >= UI_H - 40 && dpx <= 110);
    if (dhit && !grabbing && !onClose) {
      const int cell = 36;
      if (dwellPx >= 0 && std::abs(dpx - dwellPx) < cell && std::abs(dpy - dwellPy) < cell) {
        dwellT += dt;
        if (dwellT >= 0.75f && !dwellFired) {
          dwellFired = true;
          fireClick(dpx, dpy, "DWELL");
        } else if (!dwellFired && (static_cast<int>(dwellT * 10) % 3 == 0)) {
          char st[80];
          snprintf(st, sizeof st, "HOLD… %.0f%% @%d,%d",
                   std::min(100.f, dwellT / 0.75f * 100.f), dpx, dpy);
          ui.status = st;
          WebUI_MarkDirty(ui);
        }
      } else {
        dwellPx = dpx;
        dwellPy = dpy;
        dwellT = 0.f;
        dwellFired = false;
      }
    } else {
      dwellT = 0.f;
      dwellPx = dwellPy = -1;
      dwellFired = false;
    }

    float sx = 0.f, sy = 0.f;
    if (XrInputReadStick(session, input, &sx, &sy)) {
      if (grabbing && aimValid) {
        const float spd = 0.015f;
        WorldPanelSetCenter(wp.c + wp.right * (sx * spd) + wp.up * (sy * spd));
        Vec3 go = (grabHand == XrHand::Left) ? aimOL : aimOR;
        grabOff = wp.c - go;
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

    // SMX matrix bus — only if configured; rate-limited inside SmxPump (default 20Hz)
    if (SmxEnabled(smx)) {
      SmxPlayerMatrix mat{};
      std::strncpy(mat.player_id, smx.self_id, sizeof mat.player_id - 1);
      if (headOk) {
        SmxFillPose(mat.head, headWorld.position.x, headWorld.position.y, headWorld.position.z,
                    headWorld.orientation.x, headWorld.orientation.y, headWorld.orientation.z,
                    headWorld.orientation.w);
        mat.valid_mask |= 1;
      }
      if (aimValidL) {
        SmxFillPose(mat.aim_l, aimOL.x, aimOL.y, aimOL.z, aimDL.x, aimDL.y, aimDL.z, 0.f);
        mat.valid_mask |= 2;
      }
      if (aimValidR) {
        SmxFillPose(mat.aim_r, aimOR.x, aimOR.y, aimOR.z, aimDR.x, aimDR.y, aimDR.z, 0.f);
        mat.valid_mask |= 4;
      }
      mat.trigger_l = trigL ? 1.f : 0.f;
      mat.trigger_r = trigR ? 1.f : 0.f;
      mat.grab_l = grabL;
      mat.grab_r = grabR;
      mat.menu = menuBtn ? 1 : 0;
      mat.ui_tex_w = (uint32_t)UI_W;
      mat.ui_tex_h = (uint32_t)UI_H;
      // Cheap hash: seq + paint frame — do NOT scan full RGBA every XR tick
      mat.ui_tex_hash = (uint32_t)ui.paintFrame * 2654435761u ^ (uint32_t)smx.tx_seq;
      SmxPlayerMatrix peer{};
      SmxPump(smx, mat, &peer);
    }

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
          glClearColor(0.f, 0.f, 0.f, 1.f); // pure black — never red wash
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        GlLoadProjectionFov(views0[eye].fov);
        glMatrixMode(GL_MODELVIEW);
        GlLoadModelviewLocal(views0[eye].pose);
        if (wp.ready)
          GlDrawWorldPanel(panelTex, views0[eye].pose);
        // One ray per hand. Tip only stops on the panel when WorldPanelRayHit says so.
        auto drawHandLaser = [](bool valid, bool hit, const Vec3& o, const Vec3& d,
                                const Vec3& hp, float cr, float cg, float cb) {
          if (!valid) return;
          Vec3 tip = hit ? hp : (o + d * 1.8f);
          if (hit) {
            cr = 1.f;
            cg = 0.25f;
            cb = 0.35f; // crimson hit = plane touch confirmed
          }
          GlDrawLaser(o, tip, cr, cg, cb);
        };
        drawHandLaser(aimValidL, panelHitL, aimOL, aimDL, hitPtL, 0.55f, 0.55f, 0.6f);
        drawHandLaser(aimValidR, panelHitR, aimOR, aimDR, hitPtR, 0.55f, 0.55f, 0.6f);
        // G02: full eye-buffer fade (panel dim + world content) during take_xr release
        if (ui.handoff && ui.handoffFade > 0.001f)
          GlFadeEyeBufferTowardBlack(CubeHandoffLayerFadeAlpha(ui.handoffFade));
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

  // Tear down without C++ exception escape (WiVRn: "terminate called without an active exception")
  try {
    if (sessionRunning && session) {
      xrRequestExitSession(session);
      // Drain a few events so runtime can STOPPING → END
      for (int n = 0; n < 32; ++n) {
        XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
        if (xrPollEvent(instance, &ev) != XR_SUCCESS) break;
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
          auto* sc = reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
          if (sc->state == XR_SESSION_STATE_STOPPING) {
            xrEndSession(session);
            sessionRunning = false;
          }
          if (sc->state == XR_SESSION_STATE_EXITING || sc->state == XR_SESSION_STATE_LOSS_PENDING)
            break;
        }
      }
      if (sessionRunning) {
        xrEndSession(session);
        sessionRunning = false;
      }
    }
    for (int i = 0; i < 2; ++i)
      if (eyes[i].swap) xrDestroySwapchain(eyes[i].swap);
    if (ptLayerHandle != XR_NULL_HANDLE && pfnDestroyPtLayer) pfnDestroyPtLayer(ptLayerHandle);
    if (passthrough != XR_NULL_HANDLE) {
      if (pfnPausePt) pfnPausePt(passthrough);
      if (pfnDestroyPt) pfnDestroyPt(passthrough);
    }
    SmxShutdown(smx);
    XrInputDestroy(input);
    if (viewSpace) xrDestroySpace(viewSpace);
    if (space) xrDestroySpace(space);
    if (session) xrDestroySession(session);
    if (instance) xrDestroyInstance(instance);
    if (panelTex) glDeleteTextures(1, &panelTex);
    GlxDestroy(glx);
  } catch (...) {
    fprintf(stderr, "[cube_webui] teardown exception swallowed\n");
  }
  fprintf(stderr, "[cube_webui] exit\n");
  return 0;
}
