#include "cssvrmod/calib.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace cssvr {
namespace {

Calib g_live{};
bool g_inited = false;
time_t g_mtime = 0;
int g_stat_skip = 0;
std::string g_path;

std::string Home() {
  const char* h = std::getenv("HOME");
  return h ? std::string(h) : std::string();
}

void MkDirP(const std::string& dir) {
  if (dir.empty()) return;
  std::string acc;
  for (size_t i = 0; i < dir.size(); ++i) {
    acc.push_back(dir[i]);
    if (dir[i] == '/' && acc.size() > 1) mkdir(acc.c_str(), 0755);
  }
  mkdir(dir.c_str(), 0755);
}

void ApplyEnv(Calib* c) {
  auto grab = [](const char* k, float* dst) {
    const char* e = std::getenv(k);
    if (e && e[0]) *dst = std::strtof(e, nullptr);
  };
  grab("CSSVR_EYESCALE", &c->eyescale);
  grab("CSSVR_HOFFSET", &c->hoffset);
  grab("CSSVR_VOFFSET", &c->voffset);
  grab("CSSVR_SCALEFACTOR", &c->scalefactor);
  grab("CSSVR_LENS_BEND", &c->lens_bend);
  grab("CSSVR_IPD", &c->ipd_m);
  if (const char* e = std::getenv("CSSVR_SWAP_EYES"))
    c->swap_eyes = !(e[0] == '0' && e[1] == 0);
}

} // namespace

const char* CalibPath() {
  if (g_path.empty()) {
    if (const char* e = std::getenv("CSSVR_CALIB")) {
      g_path = e;
    } else {
      g_path = Home() + "/.config/gvrmod/cssvr_calib.cfg";
    }
  }
  return g_path.c_str();
}

bool CalibWriteDefault(const char* path) {
  if (!path || !path[0]) return false;
  std::string p(path);
  auto slash = p.find_last_of('/');
  if (slash != std::string::npos) MkDirP(p.substr(0, slash));
  std::ofstream out(p);
  if (!out) return false;
  out << "# CSSVR video calibration (vrmod Vision knobs)\n"
         "# Edit while the game is running — reloads in under a second.\n"
         "# Dial order: scalefactor → verticaloffset → horizontaloffset → eyescale\n"
         "# Right eye too far apart → lower eyescale (0.15–0.25). Too flat → raise toward 0.5.\n"
         "\n"
         "eyescale 0.25\n"
         "horizontaloffset 0\n"
         "verticaloffset 0\n"
         "scalefactor 1\n"
         "lens_bend 0\n"
         "ipd_m 0.064\n"
         "swap_eyes 0\n";
  return true;
}

const Calib& CalibLive() {
  if (++g_stat_skip >= 30 || !g_inited) {
    g_stat_skip = 0;
    const char* path = CalibPath();
    struct stat st {};
    if (stat(path, &st) != 0) {
      CalibWriteDefault(path);
      stat(path, &st);
    }
    if (!g_inited || st.st_mtime != g_mtime) {
      std::ifstream in(path);
      std::ostringstream ss;
      if (in) ss << in.rdbuf();
      Calib c{};
      ParseCalibText(ss.str().c_str(), &c);
      ApplyEnv(&c);
      g_live = ClampCalib(c);
      g_mtime = st.st_mtime;
      g_inited = true;
    }
  }
  return g_live;
}

} // namespace cssvr
