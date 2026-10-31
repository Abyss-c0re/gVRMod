#include "paths.hpp"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

std::string FindGModRoot() {
  if (const char* e = getenv("GMOD_DIR")) return e;
  const char* home = getenv("HOME");
  if (!home) return {};
  const char* cands[] = {
    "/.steam/steam/steamapps/common/GarrysMod",
    "/.local/share/Steam/steamapps/common/GarrysMod",
    "/.steam/root/steamapps/common/GarrysMod",
  };
  for (auto rel : cands) {
    std::string p = std::string(home) + rel;
    if (access((p + "/hl2.sh").c_str(), X_OK) == 0) return p;
  }
  return {};
}

// Distro libopenxr_wivrn.so cannot complete IPC with a newer wivrn-server.
// Prefer the manifest that sits next to the running service.
static std::string RuntimeBesideRunningServer() {
  FILE* fp = popen("pgrep -xo wivrn-server", "r");
  if (!fp) return {};
  char buf[64] = {};
  if (!fgets(buf, sizeof buf, fp)) {
    pclose(fp);
    return {};
  }
  pclose(fp);
  std::string pid = buf;
  while (!pid.empty() && (pid.back() == '\n' || pid.back() == '\r' || pid.back() == ' '))
    pid.pop_back();
  if (pid.empty()) return {};

  std::string link = "/proc/" + pid + "/exe";
  char exe[4096];
  ssize_t n = readlink(link.c_str(), exe, sizeof exe - 1);
  if (n <= 0) return {};
  exe[n] = '\0';

  std::string path = exe;
  auto slash = path.rfind('/');
  if (slash == std::string::npos) return {};
  path.resize(slash);
  slash = path.rfind('/');
  if (slash == std::string::npos) return {};
  path.resize(slash);
  path += "/openxr_wivrn-dev.json";
  if (access(path.c_str(), R_OK) != 0) return {};
  return path;
}

std::string FindXrRuntimeJson() {
  if (const char* e = getenv("XR_RUNTIME_JSON")) return e;
  if (std::string beside = RuntimeBesideRunningServer(); !beside.empty()) return beside;
  const char* cands[] = {
    "/usr/share/openxr/1/openxr_wivrn.json",
    "/usr/local/share/openxr/1/openxr_wivrn.json",
    "/usr/share/openxr/1/openxr_monado.json",
  };
  for (auto c : cands)
    if (access(c, R_OK) == 0) return c;
  return {};
}
