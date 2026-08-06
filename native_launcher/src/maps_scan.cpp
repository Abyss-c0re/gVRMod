#include "maps_scan.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unordered_set>
#include <vector>

// Scan the same places GMod's menu sees:
//   garrysmod/maps, download/maps, enabled local addons, enabled workshop
//   (loose maps/ + .gma TOC), and mounted Source games from mountdepots.txt.

static bool IsDir(const std::string& p) {
  struct stat st{};
  return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static std::string ReadFile(const std::string& path) {
  std::ifstream f(path);
  if (!f) return {};
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static std::string ToLower(std::string s) {
  for (char& c : s) c = (char)std::tolower((unsigned char)c);
  return s;
}

static bool EndsWithLower(const std::string& n, const char* suf) {
  size_t sl = std::strlen(suf);
  if (n.size() < sl) return false;
  for (size_t i = 0; i < sl; ++i) {
    char a = (char)std::tolower((unsigned char)n[n.size() - sl + i]);
    if (a != suf[i]) return false;
  }
  return true;
}

static bool ShouldIgnoreMap(const std::string& bare) {
  const std::string m = ToLower(bare);
  auto starts = [&](const char* p) {
    size_t n = std::strlen(p);
    return m.size() >= n && m.compare(0, n, p) == 0;
  };
  if (starts("background") || starts("ep1_background") || starts("ep2_background"))
    return true;
  if (starts("devtest") || starts("styleguide") || starts("sdk_") || starts("test_") ||
      starts("vst_"))
    return true;
  if (m == "credits" || m == "intro" || m == "test" || m == "c4a1y" ||
      m == "d2_coast_02" || m == "d3_c17_02_camera" || m == "ep1_citadel_00_demo" ||
      m == "c5m1_waterfront_sndscape")
    return true;
  return false;
}

static void AddBare(std::unordered_set<std::string>& out, const std::string& bareIn) {
  if (bareIn.empty()) return;
  std::string bare = ToLower(bareIn);
  // strip accidental path
  auto slash = bare.find_last_of("/\\");
  if (slash != std::string::npos) bare = bare.substr(slash + 1);
  if (EndsWithLower(bare, ".bsp")) bare = bare.substr(0, bare.size() - 4);
  if (bare.empty() || ShouldIgnoreMap(bare)) return;
  out.insert(std::move(bare));
}

static void CollectBspsInMapsDir(const std::string& mapsDir, std::unordered_set<std::string>& out) {
  DIR* d = opendir(mapsDir.c_str());
  if (!d) return;
  while (dirent* e = readdir(d)) {
    std::string n = e->d_name;
    if (n == "." || n == "..") continue;
    if (!EndsWithLower(n, ".bsp")) continue;
    AddBare(out, n);
  }
  closedir(d);
}

// Some addons nest maps under a content root: maps/, or */maps/
static void CollectBspsUnderAddonRoot(const std::string& root, std::unordered_set<std::string>& out) {
  CollectBspsInMapsDir(root + "/maps", out);
  DIR* d = opendir(root.c_str());
  if (!d) return;
  while (dirent* e = readdir(d)) {
    std::string n = e->d_name;
    if (n == "." || n == ".." || n[0] == '.') continue;
    std::string sub = root + "/" + n;
    if (!IsDir(sub)) continue;
    CollectBspsInMapsDir(sub + "/maps", out);
  }
  closedir(d);
}

// Read maps/*.bsp names from a GMA table-of-contents (no extract).
static void CollectMapsFromGma(const std::string& path, std::unordered_set<std::string>& out) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return;
  auto rd = [&](void* p, size_t n) -> bool { return std::fread(p, 1, n, f) == n; };
  auto rdstr = [&]() -> std::string {
    std::string s;
    char c;
    while (rd(&c, 1)) {
      if (c == 0) break;
      s.push_back(c);
      if (s.size() > 4096) { s.clear(); break; }
    }
    return s;
  };

  char magic[4];
  if (!rd(magic, 4) || std::memcmp(magic, "GMAD", 4) != 0) {
    std::fclose(f);
    return;
  }
  uint8_t ver = 0;
  if (!rd(&ver, 1)) { std::fclose(f); return; }
  uint64_t steamid = 0, ts = 0;
  if (!rd(&steamid, 8) || !rd(&ts, 8)) { std::fclose(f); return; }
  // required content strings until empty
  for (;;) {
    std::string req = rdstr();
    if (req.empty()) break;
  }
  (void)rdstr(); // name
  (void)rdstr(); // desc
  (void)rdstr(); // author
  int32_t addonVer = 0;
  if (!rd(&addonVer, 4)) { std::fclose(f); return; }

  for (;;) {
    uint32_t idx = 0;
    if (!rd(&idx, 4)) break;
    if (idx == 0) break;
    std::string name = rdstr();
    uint64_t size = 0;
    uint32_t crc = 0;
    if (!rd(&size, 8) || !rd(&crc, 4)) break;
    // normalize path
    for (char& c : name) if (c == '\\') c = '/';
    std::string low = ToLower(name);
    if (EndsWithLower(low, ".bsp")) {
      // accept maps/foo.bsp or nested .../maps/foo.bsp
      auto pos = low.rfind("maps/");
      if (pos != std::string::npos) {
        std::string bare = low.substr(pos + 5);
        if (bare.find('/') == std::string::npos)
          AddBare(out, bare);
      }
    }
  }
  std::fclose(f);
}

static void CollectFromDirGmas(const std::string& dir, std::unordered_set<std::string>& out) {
  DIR* d = opendir(dir.c_str());
  if (!d) return;
  while (dirent* e = readdir(d)) {
    std::string n = e->d_name;
    if (!EndsWithLower(n, ".gma")) continue;
    CollectMapsFromGma(dir + "/" + n, out);
  }
  closedir(d);
}

// addonnomount.txt — workshop IDs that are disabled (same as addons_mgr).
static void ParseNomount(const std::string& path, std::unordered_set<std::string>& out) {
  out.clear();
  std::string body = ReadFile(path);
  size_t i = 0;
  while (i < body.size()) {
    if (body[i] != '"') { ++i; continue; }
    auto j = body.find('"', i + 1);
    if (j == std::string::npos) break;
    std::string a = body.substr(i + 1, j - i - 1);
    i = j + 1;
    while (i < body.size() && (body[i] == ' ' || body[i] == '\t' || body[i] == '\n' ||
                               body[i] == '\r'))
      ++i;
    if (i >= body.size() || body[i] != '"') continue;
    auto k = body.find('"', i + 1);
    if (k == std::string::npos) break;
    std::string b = body.substr(i + 1, k - i - 1);
    i = k + 1;
    auto digits = [](const std::string& s) {
      return !s.empty() &&
             std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
    };
    if (digits(b)) out.insert(b);
    else if (digits(a)) out.insert(a);
  }
}

// mountdepots.txt — "hl2" "1" style pairs; only include enabled ("1").
static void ParseMountdepots(const std::string& path, std::vector<std::string>& enabled) {
  enabled.clear();
  std::string body = ReadFile(path);
  size_t i = 0;
  while (i < body.size()) {
    if (body[i] != '"') { ++i; continue; }
    auto j = body.find('"', i + 1);
    if (j == std::string::npos) break;
    std::string a = body.substr(i + 1, j - i - 1);
    i = j + 1;
    while (i < body.size() && (body[i] == ' ' || body[i] == '\t' || body[i] == '\n' ||
                               body[i] == '\r'))
      ++i;
    if (i >= body.size() || body[i] != '"') continue;
    auto k = body.find('"', i + 1);
    if (k == std::string::npos) break;
    std::string b = body.substr(i + 1, k - i - 1);
    i = k + 1;
    if (a == "gamedepotsystem") continue;
    if (b == "1" || ToLower(b) == "true")
      enabled.push_back(ToLower(a));
  }
}

// mount.cfg custom paths: "cstrike" "C:/path/to/cstrike"
static void ParseMountCfg(const std::string& path, std::vector<std::string>& mapRoots) {
  std::string body = ReadFile(path);
  // strip // line comments roughly
  std::string cleaned;
  cleaned.reserve(body.size());
  for (size_t i = 0; i < body.size(); ++i) {
    if (i + 1 < body.size() && body[i] == '/' && body[i + 1] == '/') {
      while (i < body.size() && body[i] != '\n') ++i;
      continue;
    }
    cleaned.push_back(body[i]);
  }
  size_t i = 0;
  while (i < cleaned.size()) {
    if (cleaned[i] != '"') { ++i; continue; }
    auto j = cleaned.find('"', i + 1);
    if (j == std::string::npos) break;
    std::string key = cleaned.substr(i + 1, j - i - 1);
    i = j + 1;
    while (i < cleaned.size() && (cleaned[i] == ' ' || cleaned[i] == '\t' || cleaned[i] == '\n' ||
                                  cleaned[i] == '\r'))
      ++i;
    if (i >= cleaned.size() || cleaned[i] != '"') continue;
    auto k = cleaned.find('"', i + 1);
    if (k == std::string::npos) break;
    std::string val = cleaned.substr(i + 1, k - i - 1);
    i = k + 1;
    if (key == "mountcfg" || val.empty()) continue;
    // path may be game root (with maps/) or already the content dir
    if (IsDir(val + "/maps")) mapRoots.push_back(val);
    else if (IsDir(val) && IsDir(val + "/../maps") == false) {
      // if val itself is a maps parent
      if (IsDir(val)) mapRoots.push_back(val);
    }
  }
}

static std::string ParentDir(const std::string& p) {
  auto pos = p.find_last_of("/\\");
  if (pos == std::string::npos) return {};
  return p.substr(0, pos);
}

// steamapps roots: from GMod install + libraryfolders.vdf
static void CollectSteamAppsRoots(const std::string& gmodRoot, std::vector<std::string>& out) {
  std::unordered_set<std::string> seen;
  auto add = [&](std::string p) {
    while (!p.empty() && (p.back() == '/' || p.back() == '\\')) p.pop_back();
    if (p.empty() || !IsDir(p)) return;
    if (seen.insert(p).second) out.push_back(p);
  };

  // .../steamapps/common/GarrysMod → steamapps
  std::string common = ParentDir(gmodRoot);
  std::string steamapps = ParentDir(common);
  if (!steamapps.empty()) add(steamapps);

  // Also classic home paths
  if (const char* home = getenv("HOME")) {
    const char* cands[] = {
        "/.local/share/Steam/steamapps",
        "/.steam/steam/steamapps",
        "/.steam/root/steamapps",
    };
    for (auto rel : cands) add(std::string(home) + rel);
  }

  // libraryfolders.vdf on each known steamapps
  std::vector<std::string> seed = out;
  for (const auto& sa : seed) {
    std::string vdf = sa + "/libraryfolders.vdf";
    std::string body = ReadFile(vdf);
    if (body.empty()) {
      // sometimes next to steamapps: ../config/libraryfolders.vdf
      body = ReadFile(ParentDir(sa) + "/config/libraryfolders.vdf");
    }
    size_t i = 0;
    while (i < body.size()) {
      // find "path"
      auto p = body.find("\"path\"", i);
      if (p == std::string::npos) break;
      i = p + 6;
      auto q1 = body.find('"', i);
      if (q1 == std::string::npos) break;
      auto q2 = body.find('"', q1 + 1);
      if (q2 == std::string::npos) break;
      std::string lib = body.substr(q1 + 1, q2 - q1 - 1);
      // VDF may escape backslashes as pairs; collapse each pair to one char.
      std::string unesc;
      unesc.reserve(lib.size());
      for (size_t k = 0; k < lib.size(); ++k) {
        if (lib[k] == '\\' && k + 1 < lib.size()) {
          unesc.push_back(lib[k + 1]);
          ++k;
        } else {
          unesc.push_back(lib[k]);
        }
      }
      add(unesc + "/steamapps");
      i = q2 + 1;
    }
  }
}

// Relative content dirs under steamapps/common for a depot key.
static const char* const* DepotRelPaths(const std::string& depot, int* count) {
  // When "hl2" is on, include episodes/lostcoast under the same install — GMod users
  // expect those maps when HL2 content is mounted.
  if (depot == "hl2") {
    static const char* p[] = {
        "Half-Life 2/hl2",
        "Half-Life 2/episodic",
        "Half-Life 2/ep2",
        "Half-Life 2/lostcoast",
    };
    *count = 4;
    return p;
  }
  if (depot == "episodic" || depot == "ep1") {
    static const char* p[] = {"Half-Life 2/episodic"};
    *count = 1;
    return p;
  }
  if (depot == "ep2") {
    static const char* p[] = {"Half-Life 2/ep2"};
    *count = 1;
    return p;
  }
  if (depot == "lostcoast") {
    static const char* p[] = {"Half-Life 2/lostcoast"};
    *count = 1;
    return p;
  }
  if (depot == "cstrike" || depot == "css") {
    static const char* p[] = {"Counter-Strike Source/cstrike"};
    *count = 1;
    return p;
  }
  if (depot == "tf" || depot == "tf2") {
    static const char* p[] = {"Team Fortress 2/tf"};
    *count = 1;
    return p;
  }
  if (depot == "hl2mp") {
    static const char* p[] = {"Half-Life 2 Deathmatch/hl2mp"};
    *count = 1;
    return p;
  }
  if (depot == "dod") {
    static const char* p[] = {"Day of Defeat Source/dod"};
    *count = 1;
    return p;
  }
  if (depot == "left4dead" || depot == "l4d") {
    static const char* p[] = {"Left 4 Dead/left4dead"};
    *count = 1;
    return p;
  }
  if (depot == "left4dead2" || depot == "l4d2") {
    static const char* p[] = {"Left 4 Dead 2/left4dead2"};
    *count = 1;
    return p;
  }
  if (depot == "portal") {
    static const char* p[] = {"Portal/portal"};
    *count = 1;
    return p;
  }
  if (depot == "portal2") {
    static const char* p[] = {"Portal 2/portal2"};
    *count = 1;
    return p;
  }
  if (depot == "hl1" || depot == "hls") {
    static const char* p[] = {"Half-Life 2/hl1"}; // Source SDK / HL:S sometimes
    *count = 1;
    return p;
  }
  *count = 0;
  return nullptr;
}

static std::string FindWorkshopContent4000(const std::string& gmodRoot) {
  auto pos = gmodRoot.rfind("/common/");
  if (pos == std::string::npos) pos = gmodRoot.rfind("\\common\\");
  if (pos != std::string::npos) {
    std::string w = gmodRoot.substr(0, pos) + "/workshop/content/4000";
    if (IsDir(w)) return w;
  }
  if (const char* home = getenv("HOME")) {
    const char* cands[] = {
        "/.steam/steam/steamapps/workshop/content/4000",
        "/.local/share/Steam/steamapps/workshop/content/4000",
    };
    for (auto rel : cands) {
      std::string p = std::string(home) + rel;
      if (IsDir(p)) return p;
    }
  }
  return {};
}

std::string CategoryForMap(const std::string& mapName) {
  const std::string m = ToLower(mapName);
  auto starts = [&](const char* p) {
    return m.size() >= std::strlen(p) && m.compare(0, std::strlen(p), p) == 0;
  };
  auto is = [&](const char* p) { return m == p; };

  // Exact HL2 campaign names → nicer categories
  if (starts("ep1_")) return "HL2 Episode 1";
  if (starts("ep2_")) return "HL2 Episode 2";
  if (is("d2_lostcoast") || starts("d2_lostcoast")) return "HL2 Lost Coast";
  if (starts("d1_") || starts("d2_") || starts("d3_")) return "HL2";

  if (starts("gm_") || starts("gmod_") || starts("phys_")) return "Sandbox";
  if (starts("ttt_") || starts("gm_ttt")) return "TTT";
  if (starts("rp_")) return "Roleplay";
  if (starts("surf_")) return "Surf";
  if (starts("bhop_")) return "Bhop";
  if (starts("deathrun_") || starts("dr_")) return "Deathrun";
  if (starts("ph_")) return "Prop Hunt";
  if (starts("zm_") || starts("zs_") || starts("zombiesurvival_")) return "Zombie Survival";
  if (starts("de_") || starts("cs_") || starts("fy_") || starts("ar_") || starts("es_") ||
      starts("gd_") || starts("dz_") || starts("aim_") || starts("awp_"))
    return "Counter-Strike";
  if (starts("cp_") || starts("ctf_") || starts("pl_") || starts("plr_") || starts("koth_") ||
      starts("mvm_") || starts("arena_") || starts("sd_") || starts("tc_") || starts("tr_") ||
      starts("pd_") || starts("rd_") || starts("pass_") || starts("trade_"))
    return "TF2";
  if (starts("dm_")) return "HL2 Deathmatch";
  if (starts("dod_")) return "Day of Defeat";
  if (starts("testchmb_") || starts("escape_")) return "Portal";
  if (starts("sp_a") || starts("mp_coop_")) return "Portal 2";
  if (starts("l4d_") || starts("c1m") || starts("c2m") || starts("c3m") || starts("c4m") ||
      starts("c5m") || starts("c6m") || starts("c7m") || starts("c8m") || starts("c9m") ||
      starts("c10m") || starts("c11m") || starts("c12m") || starts("c13m") || starts("c14m"))
    return "Left 4 Dead";
  return "Other";
}

static int OrderForCategory(const std::string& c) {
  static const char* order[] = {
      "Sandbox", "Roleplay", "TTT", "Surf", "Bhop", "Deathrun", "Prop Hunt",
      "Zombie Survival", "Counter-Strike", "TF2", "HL2", "HL2 Episode 1",
      "HL2 Episode 2", "HL2 Lost Coast", "HL2 Deathmatch", "Day of Defeat",
      "Portal", "Portal 2", "Left 4 Dead", "Other"};
  for (int i = 0; i < (int)(sizeof(order) / sizeof(order[0])); ++i)
    if (c == order[i]) return i;
  return 50;
}

std::vector<MapCategory> ScanGModMaps(const std::string& gmodRoot) {
  std::unordered_set<std::string> bare;
  if (gmodRoot.empty()) return {};

  // 1) Base + download cache
  CollectBspsInMapsDir(gmodRoot + "/garrysmod/maps", bare);
  CollectBspsInMapsDir(gmodRoot + "/garrysmod/download/maps", bare);

  // 2) Local addons (enabled = folder not ending in .disabled)
  {
    std::string addons = gmodRoot + "/garrysmod/addons";
    if (DIR* d = opendir(addons.c_str())) {
      while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        if (n.size() > 9 && n.compare(n.size() - 9, 9, ".disabled") == 0) continue;
        std::string full = addons + "/" + n;
        if (!IsDir(full)) {
          // loose .gma dropped into addons/
          if (EndsWithLower(n, ".gma")) CollectMapsFromGma(full, bare);
          continue;
        }
        CollectBspsUnderAddonRoot(full, bare);
        CollectFromDirGmas(full, bare);
      }
      closedir(d);
    }
  }

  // 3) Workshop content (skip addonnomount)
  {
    std::unordered_set<std::string> nomount;
    ParseNomount(gmodRoot + "/garrysmod/cfg/addonnomount.txt", nomount);
    std::string ws = FindWorkshopContent4000(gmodRoot);
    if (!ws.empty()) {
      if (DIR* d = opendir(ws.c_str())) {
        while (dirent* e = readdir(d)) {
          std::string id = e->d_name;
          if (id == "." || id == "..") continue;
          if (nomount.count(id)) continue;
          std::string full = ws + "/" + id;
          if (!IsDir(full)) continue;
          CollectBspsUnderAddonRoot(full, bare);
          CollectFromDirGmas(full, bare);
        }
        closedir(d);
      }
    }
  }

  // 4) Mounted Source games (mountdepots + mount.cfg)
  {
    std::vector<std::string> steamApps;
    CollectSteamAppsRoots(gmodRoot, steamApps);

    std::vector<std::string> depots;
    ParseMountdepots(gmodRoot + "/garrysmod/cfg/mountdepots.txt", depots);
    // If file missing, still try common defaults so HL2/CSS show up when installed
    if (depots.empty()) {
      depots = {"hl2", "cstrike", "tf", "hl2mp"};
    }

    for (const auto& depot : depots) {
      int n = 0;
      const char* const* rels = DepotRelPaths(depot, &n);
      for (int i = 0; i < n; ++i) {
        for (const auto& sa : steamApps) {
          std::string root = sa + "/common/" + rels[i];
          CollectBspsInMapsDir(root + "/maps", bare);
        }
      }
    }

    std::vector<std::string> custom;
    ParseMountCfg(gmodRoot + "/garrysmod/cfg/mount.cfg", custom);
    for (const auto& root : custom)
      CollectBspsInMapsDir(root + "/maps", bare);
  }

  // Classify
  std::map<std::string, MapCategory> cats;
  for (const auto& name : bare) {
    std::string cat = CategoryForMap(name);
    auto& c = cats[cat];
    c.name = cat;
    c.order = OrderForCategory(cat);
    c.maps.push_back(name);
  }
  for (auto& kv : cats)
    std::sort(kv.second.maps.begin(), kv.second.maps.end());

  std::vector<MapCategory> out;
  out.reserve(cats.size());
  for (auto& kv : cats) out.push_back(std::move(kv.second));
  std::sort(out.begin(), out.end(), [](const MapCategory& a, const MapCategory& b) {
    return a.order < b.order;
  });

  fprintf(stderr, "[CubeUI] maps scan: %zu unique maps in %zu categories\n", bare.size(),
          out.size());
  return out;
}
