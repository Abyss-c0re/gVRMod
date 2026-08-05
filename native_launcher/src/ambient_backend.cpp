#include "ambient_backend.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

void AmbientBackend_Poll(AmbientBackendState& s) {
  if (s.pid <= 0) {
    s.running = false;
    return;
  }
  int status = 0;
  pid_t r = waitpid(s.pid, &status, WNOHANG);
  if (r == 0) {
    s.running = true;
    return;
  }
  // exited or error
  s.pid = -1;
  s.running = false;
}

void AmbientBackend_Stop(AmbientBackendState& s) {
  if (s.pid > 0) {
    kill(s.pid, SIGTERM);
    // brief reap
    for (int i = 0; i < 20; ++i) {
      int status = 0;
      pid_t r = waitpid(s.pid, &status, WNOHANG);
      if (r != 0) break;
      usleep(5000);
    }
    if (s.pid > 0) {
      // still alive?
      if (kill(s.pid, 0) == 0) {
        kill(s.pid, SIGKILL);
        waitpid(s.pid, nullptr, 0);
      } else {
        waitpid(s.pid, nullptr, WNOHANG);
      }
    }
  }
  s.pid = -1;
  s.running = false;
  s.volume = 0.f;
}

bool AmbientBackend_Start(AmbientBackendState& s, const std::string& absPath, float volume01,
                          const std::string& backendPref) {
  AmbientBackend_Stop(s);
  if (absPath.empty()) {
    s.last_err = "empty_path";
    return false;
  }
  if (access(absPath.c_str(), R_OK) != 0) {
    s.last_err = "clip_unreadable";
    return false;
  }
  std::string be = backendPref.empty() ? CubeAmbient_DefaultBackend() : backendPref;
  auto argv = CubeAmbient_PlayArgv(be, absPath, volume01);
  if (argv.empty()) {
    s.last_err = "empty_argv";
    return false;
  }
  // Prefer requested backend; if ffplay missing, fall back to paplay once
  if (access(("/usr/bin/" + argv[0]).c_str(), X_OK) != 0 &&
      access(("/bin/" + argv[0]).c_str(), X_OK) != 0) {
    // try PATH via execlp later; also prepare paplay fallback argv
    if (be != "paplay") {
      auto alt = CubeAmbient_PlayArgv("paplay", absPath, volume01);
      if (!alt.empty()) {
        argv = alt;
        be = "paplay";
      }
    }
  }

  pid_t pid = fork();
  if (pid < 0) {
    s.last_err = "fork_failed";
    return false;
  }
  if (pid == 0) {
    // Child: detach from terminal noise
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
    cargv.push_back(nullptr);
    execvp(cargv[0], cargv.data());
    _exit(127);
  }
  s.pid = pid;
  s.path = absPath;
  s.volume = volume01;
  s.backend = be;
  s.running = true;
  s.last_err.clear();
  fprintf(stderr, "[CubeUI] ambient backend start pid=%d backend=%s vol=%.2f path=%s\n",
          (int)pid, be.c_str(), volume01, absPath.c_str());
  return true;
}

bool AmbientBackend_Apply(AmbientBackendState& s, const AmbientPlayerDecision& d,
                          const std::string& absPath) {
  AmbientBackend_Poll(s);
  if (d.action == "stop" || d.action == "idle") {
    if (s.running || s.pid > 0) AmbientBackend_Stop(s);
    return false;
  }
  if (d.action == "deferred") {
    // feature off — ensure silent
    if (s.running || s.pid > 0) AmbientBackend_Stop(s);
    return false;
  }
  if (d.action == "start") {
    return AmbientBackend_Start(s, absPath, d.volume, s.backend.empty() ? "ffplay" : s.backend);
  }
  if (d.action == "set_gain") {
    if (!s.running) {
      return AmbientBackend_Start(s, absPath, d.volume, s.backend.empty() ? "ffplay" : s.backend);
    }
    // ffplay cannot live-duck; restart on meaningful gain step
    if (CubeAmbient_ShouldRestartForGain(s.volume, d.volume)) {
      return AmbientBackend_Start(s, absPath.empty() ? s.path : absPath, d.volume,
                                  s.backend.empty() ? "ffplay" : s.backend);
    }
    return true;
  }
  return s.running;
}
