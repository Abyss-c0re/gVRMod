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
