#include "gmod_spawn.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <ctime>
#include <unistd.h>

static bool WriteFile(const std::string& path, const std::string& body) {
  std::ofstream f(path);
  if (!f) return false;
  f << body;
  return true;
}

static bool DirExists(const std::string& p) {
  struct stat st {};
  return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

int SpawnGModFromWebUI(const LaunchRequest& req, std::string& errOut) {
  if (req.gmodRoot.empty() || !DirExists(req.gmodRoot + "/garrysmod")) {
    errOut = "GMod root invalid";
    return 1;
  }

  const std::string cfgDir = req.gmodRoot + "/garrysmod/cfg";
  const std::string dataDir = req.gmodRoot + "/garrysmod/data/vrmod";
  mkdir((req.gmodRoot + "/garrysmod/data").c_str(), 0755);
  mkdir(dataDir.c_str(), 0755);

  // Cube boot cfg: GMod/Source graphics + OpenXR autostart
  const auto& g = req.gfx;
  std::ostringstream cfg;
  cfg << "// written by cube_webui_launcher — GMod native graphics + Cube OpenXR\n"
      << "// --- Source / GMod graphics (from Cube SETTINGS tab) ---\n"
      << "mat_picmip " << g.matPicmip << "\n"
      << "r_rootlod " << g.rRootLod << "\n"
      << "r_lod " << g.rRootLod << "\n"
      << "mat_antialias " << g.matAntialias << "\n"
      << "mat_aaquality 0\n"
      << "mat_forceaniso " << g.matForceAniso << "\n"
      << "mat_hdr_level " << g.matHdrLevel << "\n"
      << "r_shadows " << (g.shadows ? 1 : 0) << "\n"
      << "r_shadowrendertotexture " << (g.shadows ? 1 : 0) << "\n"
      << "r_flashlightdepthtexture " << (g.flashlightShadows ? 1 : 0) << "\n"
      << "mat_specular " << (g.specular ? 1 : 0) << "\n"
      << "mat_bumpmap " << (g.bumpmap ? 1 : 0) << "\n"
      << "r_waterforceexpensive " << (g.waterExpensive ? 1 : 0) << "\n"
      << "r_waterforcereflectentities " << (g.waterExpensive ? 1 : 0) << "\n"
      << "fps_max " << g.fpsMax << "\n"
      << "mat_queue_mode " << (g.multicore ? 2 : 0) << "\n"
      << "cl_threaded_bone_setup " << (g.multicore ? 1 : 0) << "\n"
      << "r_threaded_particles " << (g.multicore ? 1 : 0) << "\n"
      << "r_threaded_renderables " << (g.multicore ? 1 : 0) << "\n"
      << "cl_forcepreload 1\n"
      << "engine_no_focus_sleep 0\n"
      << "snd_mute_losefocus 0\n"
      << "sv_pausable 0\n"
      << "sv_lan " << (req.svLan ? 1 : 0) << "\n"
      << "hostname \"" << req.hostname << "\"\n"
      << "// --- Cube VRMod ---\n"
      << "vrmod_prefer_backend openxr\n"
      << "vrmod_autostart 1\n"
      << "vrmod_hub 1\n"
      << "vrmod_menu_vr 0\n"
      << "vrmod_require_window_focus 0\n"
      << "vrmod_laserpointer 1\n"
      << "vrmod_start force\n";

  const std::string cfgBody = cfg.str();
  if (!WriteFile(cfgDir + "/gvrmod_cube.cfg", cfgBody)) {
    errOut = "failed to write gvrmod_cube.cfg";
    return 2;
  }
  WriteFile(cfgDir + "/gvrmod_hub.cfg", cfgBody);
  WriteFile(cfgDir + "/gvrmod_menu.cfg", cfgBody);
  // Also drop a pure graphics snippet for manual +exec
  WriteFile(cfgDir + "/gvrmod_graphics.cfg", cfgBody);

  // Marker for openxr_launch.lua
  std::ostringstream mark;
  mark << "mode=hub\nprefer_backend=openxr\nautostart=1\nmenu_vr=0\nhub=1\n"
       << "bg_map=" << req.map << "\nmap_mode=full\nnative_wrapper=1\n"
       << "webui_reversed=1\nts=" << (long)time(nullptr) << "\n";
  if (!WriteFile(dataDir + "/openxr_launch.txt", mark.str())) {
    errOut = "failed to write openxr_launch.txt";
    return 3;
  }

  // Steam appid
  WriteFile(req.gmodRoot + "/steam_appid.txt", "4000\n");

  // Build command — reverse of control.NewGame.js StartGame + window size from Settings
  std::ostringstream win;
  if (req.windowed) win << " -windowed";
  else win << " -fullscreen";
  win << " -w " << req.winW << " -h " << req.winH;
  if (req.noborder) win << " -noborder";

  std::ostringstream cmd;
  if (req.useSteam && system("command -v steam >/dev/null 2>&1") == 0) {
    cmd << "env -u LD_LIBRARY_PATH";
    if (!req.xrRuntimeJson.empty())
      cmd << " XR_RUNTIME_JSON='" << req.xrRuntimeJson << "'";
    cmd << " steam -applaunch 4000 -novid"
        << win.str()
        << " +maxplayers " << req.maxPlayers
        << " +sv_lan " << (req.svLan ? 1 : 0)
        << " +hostname \"" << req.hostname << "\""
        << " +p2p_enabled " << (req.p2p ? 1 : 0)
        << " +p2p_friendsonly " << (req.p2pFriends ? 1 : 0)
        << " +gamemode " << req.gamemode
        << " +exec " << req.cubeCfg
        << " +map " << req.map
        << " >/tmp/cube_webui_gmod.log 2>&1 &";
  } else {
    cmd << "env -u LD_LIBRARY_PATH";
    if (!req.xrRuntimeJson.empty())
      cmd << " XR_RUNTIME_JSON='" << req.xrRuntimeJson << "'";
    cmd << " \"" << req.gmodRoot << "/hl2.sh\" -game garrysmod -novid"
        << win.str()
        << " +maxplayers " << req.maxPlayers
        << " +sv_lan " << (req.svLan ? 1 : 0)
        << " +hostname \"" << req.hostname << "\""
        << " +gamemode " << req.gamemode
        << " +exec " << req.cubeCfg
        << " +map " << req.map
        << " >/tmp/cube_webui_gmod.log 2>&1 &";
  }

  // Handoff: native launcher keeps OpenXR until GMod signals take_xr
  WriteFile(dataDir + "/cube_handoff.txt",
            "phase=spawned\nts=" + std::to_string((long)time(nullptr)) + "\n");
  unlink((dataDir + "/cube_ready.txt").c_str());

  fprintf(stderr, "[cube_webui] StartGame → %s\n", cmd.str().c_str());
  int rc = system(cmd.str().c_str());
  if (rc != 0) {
    errOut = "spawn returned " + std::to_string(rc);
    return 4;
  }
  return 0;
}

bool GModProcessRunning() {
  // hl2_linux / gmod — exclude our own cube_webui_launcher
  FILE* p = popen("pgrep -af 'hl2_linux|garrysmod' 2>/dev/null | grep -v cube_webui | head -1", "r");
  if (!p) return false;
  char buf[256] = {};
  bool ok = (fgets(buf, sizeof(buf), p) != nullptr) && buf[0] != 0;
  pclose(p);
  return ok;
}

std::string ReadCubeHandoffPhase(const std::string& gmodRoot) {
  if (gmodRoot.empty()) return {};
  std::ifstream f(gmodRoot + "/garrysmod/data/vrmod/cube_handoff.txt");
  if (!f) return {};
  std::string line, phase;
  while (std::getline(f, line)) {
    if (line.rfind("phase=", 0) == 0) {
      phase = line.substr(6);
      // trim
      while (!phase.empty() && (phase.back() == '\r' || phase.back() == ' '))
        phase.pop_back();
    }
  }
  return phase;
}

void ClearCubeHandoffMarkers(const std::string& gmodRoot) {
  if (gmodRoot.empty()) return;
  const std::string d = gmodRoot + "/garrysmod/data/vrmod/";
  unlink((d + "cube_handoff.txt").c_str());
  unlink((d + "cube_ready.txt").c_str());
}
