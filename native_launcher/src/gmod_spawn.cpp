#include "gmod_spawn.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
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

  // Cube OpenXR boot cfg (in-game module autostart — after native StartGame)
  const std::string cfg =
      "// written by cube_webui_launcher (reversed WebUI StartGame)\n"
      "vrmod_prefer_backend openxr\n"
      "vrmod_autostart 1\n"
      "vrmod_hub 1\n"
      "vrmod_menu_vr 0\n"
      "vrmod_require_window_focus 0\n"
      "vrmod_laserpointer 1\n"
      "engine_no_focus_sleep 0\n"
      "snd_mute_losefocus 0\n"
      "fps_max 0\n"
      "sv_pausable 0\n"
      "sv_lan " + std::string(req.svLan ? "1" : "0") + "\n"
      "hostname " + req.hostname + "\n"
      "vrmod_start force\n";

  if (!WriteFile(cfgDir + "/gvrmod_cube.cfg", cfg)) {
    errOut = "failed to write gvrmod_cube.cfg";
    return 2;
  }
  WriteFile(cfgDir + "/gvrmod_hub.cfg", cfg);
  WriteFile(cfgDir + "/gvrmod_menu.cfg", cfg);

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

  // Build command — reverse of control.NewGame.js StartGame + OpenXR wrapper
  std::ostringstream cmd;
  if (req.useSteam && system("command -v steam >/dev/null 2>&1") == 0) {
    cmd << "env -u LD_LIBRARY_PATH";
    if (!req.xrRuntimeJson.empty())
      cmd << " XR_RUNTIME_JSON='" << req.xrRuntimeJson << "'";
    cmd << " steam -applaunch 4000 -novid"
        << " -windowed -w " << req.winW << " -h " << req.winH << " -noborder"
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
        << " -windowed -w " << req.winW << " -h " << req.winH << " -noborder"
        << " +maxplayers " << req.maxPlayers
        << " +sv_lan " << (req.svLan ? 1 : 0)
        << " +hostname \"" << req.hostname << "\""
        << " +gamemode " << req.gamemode
        << " +exec " << req.cubeCfg
        << " +map " << req.map
        << " >/tmp/cube_webui_gmod.log 2>&1 &";
  }

  fprintf(stderr, "[cube_webui] StartGame → %s\n", cmd.str().c_str());
  int rc = system(cmd.str().c_str());
  if (rc != 0) {
    errOut = "spawn returned " + std::to_string(rc);
    return 4;
  }
  return 0;
}
