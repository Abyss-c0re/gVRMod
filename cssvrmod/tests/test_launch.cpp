#include "cssvrmod/launch.hpp"
#include "cssvrmod/source_if.hpp"
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
