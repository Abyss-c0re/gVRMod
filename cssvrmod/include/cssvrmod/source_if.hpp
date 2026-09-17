#pragma once
// Source CreateInterface probe. No guessed vtable calls unless a self-test passes.
#include <string>

namespace cssvr {

using CreateInterfaceFn = void* (*)(const char* name, int* returnCode);

struct EngineIf {
  void* engine = nullptr; // IVEngineClient
  void* client = nullptr; // IBaseClientDLL
  void* trace = nullptr;  // IEngineTrace
  void* cvar = nullptr;
  const char* engine_ver = "";
  const char* client_ver = "";
  const char* trace_ver = "";
  bool screen_ok = false; // GetScreenSize self-test
  int screen_w = 0, screen_h = 0;
  bool ok = false;
  const char* reason = "idle";
};

/// Try well-known CSS 64-bit interface names against a factory (testable).
void* ProbeNamed(CreateInterfaceFn fn, const char* const* names, const char** used);

bool ProbeEngineFromFactories(CreateInterfaceFn engineFn, CreateInterfaceFn clientFn,
                              EngineIf& out);

/// Live process: dlsym CreateInterface from already-loaded engine/client.
bool ProbeLiveEngine(EngineIf& out);

/// ClientCmd if GetScreenSize(index 5) self-test passed. Never call blindly.
bool EngineClientCmd(const EngineIf& e, const char* cmd);

} // namespace cssvr
