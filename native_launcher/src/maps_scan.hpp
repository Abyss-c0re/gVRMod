#pragma once
#include <string>
#include <vector>
#include <map>

struct MapCategory {
  std::string name;
  std::vector<std::string> maps; // bare names, no .bsp
  int order = 100;
};

// Scan maps GMod can load: base + download + enabled addons (local/workshop GMA)
// + mounted Source games from mountdepots.txt / mount.cfg. Classify like getmaps.lua.
std::vector<MapCategory> ScanGModMaps(const std::string& gmodRoot);

// Prefix / known campaign → category.
std::string CategoryForMap(const std::string& mapName);

// Infer GMod gamemode from map name (ttt_ → terrortown, etc.). Falls back to "sandbox".
std::string InferGamemodeForMap(const std::string& mapName, const std::string& fallback = "sandbox");

// Ensure maps/<name>.bsp is loadable at cold +map time.
// Workshop maps often live only inside .gma (TOC listed by ScanGModMaps) and are NOT
// mounted when steam -applaunch runs +map — GMod drops to main menu.
// Extracts the whole GMA into garrysmod/addons/cube_ws_<id>/ when needed.
// Returns true if a loose BSP is present after (or was already).
bool EnsureMapAvailable(const std::string& gmodRoot, const std::string& mapBare, std::string& errOut);
