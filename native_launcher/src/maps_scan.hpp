#pragma once
#include <string>
#include <vector>
#include <map>

struct MapCategory {
  std::string name;
  std::vector<std::string> maps; // bare names, no .bsp
  int order = 100;
};

// Scan garrysmod/maps/*.bsp and classify like lua/menu/getmaps.lua (subset).
std::vector<MapCategory> ScanGModMaps(const std::string& gmodRoot);

// Prefix → category (WebUI reverse of getmaps.lua Sandbox defaults).
std::string CategoryForMap(const std::string& mapName);
