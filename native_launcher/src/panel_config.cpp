#include "panel_config.hpp"
#include "world_panel.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <algorithm>

static PanelConfig g_cfg{};

PanelConfig& PanelCfg() { return g_cfg; }
const PanelConfig& PanelCfgConst() { return g_cfg; }

std::string PathDirname(const std::string& p) {
  auto s = p.find_last_of('/');
  if (s == std::string::npos) return ".";
  if (s == 0) return "/";
  return p.substr(0, s);
}

std::string PathExeDir() {
  char buf[4096];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) return {};
  buf[n] = 0;
  return PathDirname(buf);
}

static void ApplyConfigKey(const char* key, float val) {
  auto& c = g_cfg;
  if (strcmp(key, "panel_dist") == 0 || strcmp(key, "dist") == 0) {
    if (val >= 0.25f && val <= 4.f) c.dist = val;
  } else if (strcmp(key, "panel_w") == 0) {
    if (val > 0.2f && val < 4.f) c.halfW = val * 0.5f;
  } else if (strcmp(key, "panel_h") == 0) {
    if (val > 0.15f && val < 3.f) c.halfH = val * 0.5f;
  } else if (strcmp(key, "panel_half_w") == 0 || strcmp(key, "half_w") == 0) {
    if (val > 0.1f && val < 2.f) c.halfW = val;
  } else if (strcmp(key, "panel_half_h") == 0 || strcmp(key, "half_h") == 0) {
    if (val > 0.08f && val < 1.5f) c.halfH = val;
  } else if (strcmp(key, "panel_x") == 0 || strcmp(key, "offset_x") == 0) {
    if (val > -2.f && val < 2.f) c.offsetX = val;
  } else if (strcmp(key, "panel_y") == 0 || strcmp(key, "offset_y") == 0) {
    if (val > -2.f && val < 2.f) c.offsetY = val;
  } else if (strcmp(key, "panel_z") == 0 || strcmp(key, "offset_z") == 0) {
    if (val > -2.f && val < 2.f) c.offsetZ = val;
  } else if (strcmp(key, "grab_thresh") == 0 || strcmp(key, "grab_threshold") == 0) {
    if (val >= 0.2f && val <= 1.f) c.grabThresh = val;
  } else if (strcmp(key, "grab_enable") == 0 || strcmp(key, "grip_move") == 0) {
    c.grabEnable = (val != 0.f);
  } else if (strcmp(key, "trigger_thresh") == 0 || strcmp(key, "trigger_threshold") == 0) {
    if (val >= 0.15f && val <= 1.f) c.triggerThresh = val;
  } else if (strcmp(key, "panel_alpha") == 0) {
    if (val >= 0.4f && val <= 1.f) c.panelAlpha = val;
  } else if (strcmp(key, "passthrough") == 0 || strcmp(key, "ar") == 0) {
    c.passthrough = (val != 0.f);
  } else if (strcmp(key, "view_lock") == 0 || strcmp(key, "viewlock") == 0 ||
             strcmp(key, "head_lock") == 0) {
    c.viewLock = (val != 0.f);
  } else if (strcmp(key, "world_lock") == 0) {
    c.viewLock = (val == 0.f);
  }
}

static bool LoadConfigFile(const std::string& path) {
  FILE* f = fopen(path.c_str(), "r");
  if (!f) return false;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '#' || line[0] == ';' || line[0] == '\n') continue;
    char key[64] = {};
    float val = 0.f;
    if (sscanf(line, " %63[^=]=%f", key, &val) != 2) continue;
    ApplyConfigKey(key, val);
  }
  fclose(f);
  fprintf(stderr, "[cube_webui] config %s\n", path.c_str());
  return true;
}

void LoadPanelConfig(const std::string& gmodRoot) {
  g_cfg = PanelConfig{};
  std::string exe = PathExeDir();
  if (!exe.empty()) {
    LoadConfigFile(exe + "/cube_webui.conf");
    LoadConfigFile(PathDirname(PathDirname(exe)) + "/native_launcher/cube_webui.conf");
  }
  LoadConfigFile("cube_webui.conf");
  if (const char* home = getenv("HOME"))
    LoadConfigFile(std::string(home) + "/.config/gvrmod/cube_webui.conf");
  if (!gmodRoot.empty())
    LoadConfigFile(gmodRoot + "/garrysmod/data/vrmod/cube_webui.conf");

  auto tryEnvF = [](const char* k, float& out, float lo, float hi) {
    if (const char* v = getenv(k)) {
      float f = strtof(v, nullptr);
      if (f > lo && f < hi) out = f;
    }
  };
  auto tryEnvB = [](const char* k, bool& out) {
    if (const char* v = getenv(k)) out = !(v[0] == '0' && v[1] == 0);
  };
  if (const char* v = getenv("CUBE_PANEL_W")) {
    float f = strtof(v, nullptr);
    if (f > 0.3f && f < 4.f) g_cfg.halfW = f * 0.5f;
  }
  if (const char* v = getenv("CUBE_PANEL_H")) {
    float f = strtof(v, nullptr);
    if (f > 0.2f && f < 3.f) g_cfg.halfH = f * 0.5f;
  }
  tryEnvF("CUBE_PANEL_DIST", g_cfg.dist, 0.25f, 4.f);
  tryEnvF("CUBE_PANEL_HALF_W", g_cfg.halfW, 0.1f, 2.f);
  tryEnvF("CUBE_PANEL_HALF_H", g_cfg.halfH, 0.08f, 1.5f);
  tryEnvF("CUBE_PANEL_X", g_cfg.offsetX, -2.f, 2.f);
  tryEnvF("CUBE_PANEL_Y", g_cfg.offsetY, -2.f, 2.f);
  tryEnvF("CUBE_PANEL_Z", g_cfg.offsetZ, -2.f, 2.f);
  tryEnvF("CUBE_PANEL_ALPHA", g_cfg.panelAlpha, 0.4f, 1.01f);
  tryEnvF("CUBE_GRAB_THRESH", g_cfg.grabThresh, 0.2f, 1.01f);
  tryEnvB("CUBE_PASSTHROUGH", g_cfg.passthrough);
  // Env overrides conf only when explicitly set (no silent force-off — research-3 heresy).
  if (getenv("CUBE_VIEW_LOCK"))
    tryEnvB("CUBE_VIEW_LOCK", g_cfg.viewLock);
  if (const char* v = getenv("CUBE_WORLD_LOCK")) {
    if (!(v[0] == '0' && v[1] == 0)) g_cfg.viewLock = false;
  }
  if (g_cfg.grabThresh < 0.2f) g_cfg.grabThresh = 0.2f;
  if (g_cfg.grabThresh > 1.f) g_cfg.grabThresh = 1.f;
  if (g_cfg.triggerThresh < 0.15f) g_cfg.triggerThresh = 0.15f;
  if (g_cfg.triggerThresh > 1.f) g_cfg.triggerThresh = 1.f;
  WorldPanelReset();
  fprintf(stderr,
          "[cube_webui] panel size=%.2fx%.2fm dist=%.2f world_lock=%d passthrough=%d "
          "grab=%s(>=%.2f) trig>=%.2f\n",
          g_cfg.halfW * 2.f, g_cfg.halfH * 2.f, g_cfg.dist,
          g_cfg.viewLock ? 0 : 1, g_cfg.passthrough ? 1 : 0,
          g_cfg.grabEnable ? "on" : "off", g_cfg.grabThresh, g_cfg.triggerThresh);
}
