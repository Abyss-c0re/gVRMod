#include "paths.hpp"
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

std::string FindXrRuntimeJson() {
  if (const char* e = getenv("XR_RUNTIME_JSON")) return e;
  const char* cands[] = {
    "/usr/share/openxr/1/openxr_wivrn.json",
    "/usr/local/share/openxr/1/openxr_wivrn.json",
    "/usr/share/openxr/1/openxr_monado.json",
  };
  for (auto c : cands)
    if (access(c, R_OK) == 0) return c;
  return {};
}
