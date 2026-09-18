#include "cssvrmod/backend.hpp"
#include "cssvrmod/launch.hpp"
#include "cssvrmod/source_if.hpp"
#include "cssvrmod/window_chrome.hpp"
#include "test_framework.h"
#include <cstring>
#include <string>
#include <unistd.h>

using namespace cssvr;

TEST(launch_inspect_and_find) {
  auto missing = InspectCssRoot("/no/such/css");
  ASSERT_FALSE(missing.found);
  ASSERT_STREQ(missing.reason, "root_missing");

  auto inst = FindCssInstall();
  // Machine may or may not have CSS; both outcomes are valid.
  if (inst.found) {
    ASSERT_TRUE(inst.root.size() > 0);
    ASSERT_TRUE(inst.engine_so.find("engine.so") != std::string::npos);
    ASSERT_TRUE(inst.client_so.find("client.so") != std::string::npos);
    LaunchOpts o;
    o.hook_so.clear();
    auto plan = PlanSpawn(inst, o);
    ASSERT_TRUE(plan.ok);
    ASSERT_TRUE(plan.argv.size() >= 3);
    ASSERT_TRUE(plan.cwd == inst.root);
  } else {
    ASSERT_TRUE(inst.reason != nullptr);
  }
}

TEST(backend_gl_priority_dx9_original) {
  ASSERT_EQ(static_cast<int>(BackendFromName(nullptr)), static_cast<int>(Backend::Gl));
  ASSERT_STREQ(BackendName(Backend::Gl), "gl");
  auto gl = BackendPlan(Backend::Gl);
  ASSERT_STREQ(gl.engine_flag, "-dx9");
  ASSERT_STREQ(gl.sdl_video, "x11");
  ASSERT_STREQ(gl.hook, "gl");
  ASSERT_TRUE(BackendUsesTogl(Backend::Gl));
  auto dx = BackendPlan(Backend::Dx9);
  ASSERT_STREQ(dx.engine_flag, "-dx9");
  ASSERT_STREQ(dx.hook, "d3d9");
  ASSERT_STREQ(dx.reason, "original_vrmod_createtexture");
  auto vk = BackendPlan(Backend::Vk);
  ASSERT_STREQ(vk.engine_flag, "-vulkan");
  ASSERT_STREQ(vk.hook, "vk");
  ASSERT_FALSE(BackendUsesTogl(Backend::Vk));
}

TEST(launch_default_vk_bordered) {
  LaunchOpts o;
  ASSERT_FALSE(o.noborder);
  ASSERT_EQ(static_cast<int>(o.backend), static_cast<int>(Backend::Vk));
  CssInstall inst;
  inst.found = true;
  inst.root = "/tmp";
  inst.launcher = "/tmp/cstrike.sh";
  inst.linux64 = true;
  o.hook_so.clear();
  auto plan = PlanSpawn(inst, o);
  ASSERT_TRUE(plan.ok);
  ASSERT_STREQ(plan.backend, "vk");
  bool has_vk = false, has_noborder = false;
  for (const auto& a : plan.argv) {
    if (a == "-vulkan") has_vk = true;
    if (a == "-noborder") has_noborder = true;
  }
  ASSERT_TRUE(has_vk);
  ASSERT_FALSE(has_noborder);
  bool has_windowed = false, has_videomode = false;
  for (const auto& a : plan.argv) {
    if (a == "-windowed") has_windowed = true;
    if (a == "+mat_setvideomode") has_videomode = true;
  }
  ASSERT_TRUE(has_windowed);
  ASSERT_TRUE(has_videomode);
}

TEST(sdl_window_flags_force_decorated) {
  using namespace cssvr;
  const uint32_t raw = kSdlWindowFullscreen | kSdlWindowBorderless | kSdlWindowFullscreenDesktopBit;
  const uint32_t got = SanitizeSdlWindowFlags(raw, false);
  ASSERT_EQ(got & kSdlWindowBorderless, 0u);
  ASSERT_EQ(got & kSdlWindowFullscreen, 0u);
  ASSERT_EQ(got & kSdlWindowFullscreenDesktopBit, 0u);
  ASSERT_EQ(got & kSdlWindowResizable, kSdlWindowResizable);
  ASSERT_EQ(SanitizeSdlWindowFlags(raw, true), raw);
}

TEST(launch_plan_default_gl_flag) {
  CssInstall inst;
  inst.found = true;
  inst.root = "/tmp";
  inst.launcher = "/tmp/cstrike.sh";
  inst.linux64 = true;
  LaunchOpts o;
  o.hook_so.clear();
  o.backend = Backend::Gl;
  auto plan = PlanSpawn(inst, o);
  ASSERT_TRUE(plan.ok);
  ASSERT_STREQ(plan.backend, "gl");
  ASSERT_STREQ(plan.sdl_videodriver.c_str(), "x11");
  bool has_dx9 = false;
  for (const auto& a : plan.argv)
    if (a == "-dx9") has_dx9 = true;
  ASSERT_TRUE(has_dx9);
  o.backend = Backend::Vk;
  auto pvk = PlanSpawn(inst, o);
  bool has_vk = false;
  for (const auto& a : pvk.argv)
    if (a == "-vulkan") has_vk = true;
  ASSERT_TRUE(has_vk);
}

TEST(launch_hook_required_when_set) {
  CssInstall inst;
  inst.found = true;
  inst.root = "/tmp";
  inst.launcher = "/tmp/cstrike.sh";
  inst.linux64 = true;
  LaunchOpts o;
  o.hook_so = "/no/such/libcssvrmod_hook.so";
  auto plan = PlanSpawn(inst, o);
  ASSERT_FALSE(plan.ok);
  ASSERT_STREQ(plan.reason, "hook_missing");
  ASSERT_TRUE(SpawnNeedsHook(o));
  o.hook_so.clear();
  ASSERT_FALSE(SpawnNeedsHook(o));
}

static void* FakeFactory(const char* name, int* rc) {
  static int dummy = 1;
  if (std::strcmp(name, "VEngineClient014") == 0) {
    if (rc) *rc = 0;
    return &dummy;
  }
  if (std::strcmp(name, "VClient017") == 0) {
    if (rc) *rc = 0;
    return &dummy;
  }
  if (std::strcmp(name, "EngineTraceClient003") == 0) {
    if (rc) *rc = 0;
    return &dummy;
  }
  if (rc) *rc = 1;
  return nullptr;
}

TEST(source_if_probe_names) {
  EngineIf e;
  ASSERT_TRUE(ProbeEngineFromFactories(FakeFactory, FakeFactory, e));
  ASSERT_STREQ(e.engine_ver, "VEngineClient014");
  ASSERT_STREQ(e.client_ver, "VClient017");
  ASSERT_STREQ(e.trace_ver, "EngineTraceClient003");
  ASSERT_TRUE(e.ok);
  ASSERT_FALSE(e.screen_ok); // self-test only on live
}
