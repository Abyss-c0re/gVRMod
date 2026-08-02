#include "maps_scan.hpp"
#include <dirent.h>
#include <algorithm>
#include <cstring>

std::string CategoryForMap(const std::string& mapName) {
  const std::string m = mapName;
  auto starts = [&](const char* p) {
    return m.size() >= std::strlen(p) && m.compare(0, std::strlen(p), p) == 0;
  };
  if (starts("gm_") || starts("gmod_") || starts("phys_")) return "Sandbox";
  if (starts("ttt_") || starts("gm_ttt")) return "TTT";
  if (starts("rp_")) return "Roleplay";
  if (starts("surf_")) return "Surf";
  if (starts("bhop_")) return "Bhop";
  if (starts("deathrun_") || starts("dr_")) return "Deathrun";
  if (starts("ph_")) return "Prop Hunt";
  if (starts("zm_") || starts("zs_")) return "Zombie Survival";
  if (starts("de_") || starts("cs_") || starts("fy_") || starts("ar_")) return "Counter-Strike";
  if (starts("cp_") || starts("ctf_") || starts("pl_") || starts("koth_") || starts("mvm_")) return "TF2";
  if (starts("d1_") || starts("d2_") || starts("d3_")) return "HL2";
  return "Other";
}

static int OrderForCategory(const std::string& c) {
  static const char* order[] = {
    "Sandbox", "Roleplay", "TTT", "Surf", "Bhop", "Deathrun", "Prop Hunt",
    "Zombie Survival", "Counter-Strike", "TF2", "HL2", "Other"
  };
  for (int i = 0; i < (int)(sizeof(order) / sizeof(order[0])); ++i)
    if (c == order[i]) return i;
  return 50;
}

std::vector<MapCategory> ScanGModMaps(const std::string& gmodRoot) {
  std::map<std::string, MapCategory> cats;
  std::string dir = gmodRoot + "/garrysmod/maps";
  DIR* d = opendir(dir.c_str());
  if (!d) return {};
  while (dirent* e = readdir(d)) {
    std::string n = e->d_name;
    if (n.size() < 5) continue;
    if (n.compare(n.size() - 4, 4, ".bsp") != 0) continue;
    std::string bare = n.substr(0, n.size() - 4);
    std::string cat = CategoryForMap(bare);
    auto& c = cats[cat];
    c.name = cat;
    c.order = OrderForCategory(cat);
    c.maps.push_back(bare);
  }
  closedir(d);
  for (auto& kv : cats)
    std::sort(kv.second.maps.begin(), kv.second.maps.end());
  std::vector<MapCategory> out;
  out.reserve(cats.size());
  for (auto& kv : cats) out.push_back(std::move(kv.second));
  std::sort(out.begin(), out.end(), [](const MapCategory& a, const MapCategory& b) {
    return a.order < b.order;
  });
  return out;
}
