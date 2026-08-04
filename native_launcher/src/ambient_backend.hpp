#pragma once
// G12: careful external ambient backend (ffplay/paplay). No OpenAL.
// Spawn only when CubeAmbientPlayerEnabled(); pure argv in ambient_clip.hpp.
#include "ambient_clip.hpp"
#include <string>
#include <sys/types.h>

struct AmbientBackendState {
  pid_t pid = -1;
  std::string path;
  float volume = 0.f;
  std::string backend = "ffplay";
  bool running = false;
  std::string last_err;
};

// Reap exited child; clear running if dead.
void AmbientBackend_Poll(AmbientBackendState& s);

// Stop playing (SIGTERM then SIGKILL). Safe if not running.
void AmbientBackend_Stop(AmbientBackendState& s);

// Start or restart clip at path with volume 0..1. Returns true if spawn ok.
bool AmbientBackend_Start(AmbientBackendState& s, const std::string& absPath, float volume01,
                          const std::string& backendPref = "ffplay");

// Apply pure PlayerDecide action. Returns true if backend considered playing after.
bool AmbientBackend_Apply(AmbientBackendState& s, const AmbientPlayerDecision& d,
                          const std::string& absPath);
