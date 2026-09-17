#include "cssvrmod/source_if.hpp"
#include <dlfcn.h>
#include <cstdio>

namespace cssvr {

static const char* kEngineNames[] = {"VEngineClient014", "VEngineClient013", nullptr};
static const char* kClientNames[] = {"VClient017", "VClient016", "VClient015", nullptr};
static const char* kTraceNames[] = {"EngineTraceClient003", "EngineTraceClient004", nullptr};
static const char* kCvarNames[] = {"VEngineCvar007", "VEngineCvar004", nullptr};

void* ProbeNamed(CreateInterfaceFn fn, const char* const* names, const char** used) {
  if (!fn || !names) return nullptr;
  for (int i = 0; names[i]; ++i) {
    int rc = 1;
    void* p = fn(names[i], &rc);
    if (p) {
      if (used) *used = names[i];
      return p;
    }
  }
  return nullptr;
}

static bool ScreenSelfTest(void* engine, int* w, int* h) {
  if (!engine) return false;
  // IVEngineClient::GetScreenSize is index 5 on 2013 / CSS 64-bit.
  using GetScreenSizeFn = void (*)(void*, int&, int&);
  auto** vt = *reinterpret_cast<GetScreenSizeFn**>(engine);
  if (!vt || !vt[5]) return false;
  int ww = -1, hh = -1;
  vt[5](engine, ww, hh);
  if (w) *w = ww;
  if (h) *h = hh;
  return ww > 0 && ww < 16384 && hh > 0 && hh < 16384;
}

bool ProbeEngineFromFactories(CreateInterfaceFn engineFn, CreateInterfaceFn clientFn,
                              EngineIf& out) {
  out = EngineIf{};
  if (!engineFn) {
    out.reason = "no_engine_factory";
    return false;
  }
  out.engine = ProbeNamed(engineFn, kEngineNames, &out.engine_ver);
  out.trace = ProbeNamed(engineFn, kTraceNames, &out.trace_ver);
  out.cvar = ProbeNamed(engineFn, kCvarNames, nullptr);
  if (clientFn) out.client = ProbeNamed(clientFn, kClientNames, &out.client_ver);
  if (!out.engine) {
    out.reason = "no_vengineclient";
    return false;
  }
  out.ok = true;
  out.reason = "probed";
  return true;
}

bool ProbeLiveEngine(EngineIf& out) {
  out = EngineIf{};
  CreateInterfaceFn eng = nullptr;
  CreateInterfaceFn cli = nullptr;
  // Prefer already-loaded modules (hook is inside CSS).
  void* e = dlopen("engine.so", RTLD_NOW | RTLD_NOLOAD);
  if (!e) e = dlopen(nullptr, RTLD_NOW);
  if (e) eng = reinterpret_cast<CreateInterfaceFn>(dlsym(e, "CreateInterface"));
  void* c = dlopen("client.so", RTLD_NOW | RTLD_NOLOAD);
  if (c) cli = reinterpret_cast<CreateInterfaceFn>(dlsym(c, "CreateInterface"));
  if (!eng) {
    out.reason = "no_createinterface";
    return false;
  }
  if (!ProbeEngineFromFactories(eng, cli, out)) return false;
  out.screen_ok = ScreenSelfTest(out.engine, &out.screen_w, &out.screen_h);
  out.reason = out.screen_ok ? "probed_screen_ok" : "probed_no_screen";
  return true;
}

bool EngineClientCmd(const EngineIf& e, const char* cmd) {
  if (!e.engine || !e.screen_ok || !cmd) return false;
  using ClientCmdFn = void (*)(void*, const char*);
  auto** vt = *reinterpret_cast<ClientCmdFn**>(e.engine);
  if (!vt || !vt[7]) return false;
  vt[7](e.engine, cmd);
  return true;
}

} // namespace cssvr
