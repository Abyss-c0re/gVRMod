#include "addons_mgr.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

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

static std::string ParseJsonTitle(const std::string& json) {
  // crude "title" : "..."
  auto pos = json.find("\"title\"");
  if (pos == std::string::npos) pos = json.find("\"Title\"");
  if (pos == std::string::npos) return {};
  pos = json.find(':', pos);
  if (pos == std::string::npos) return {};
  pos = json.find('"', pos);
  if (pos == std::string::npos) return {};
  auto end = json.find('"', pos + 1);
  if (end == std::string::npos) return {};
  return json.substr(pos + 1, end - pos - 1);
}

std::string FindWorkshopContent4000(const std::string& gmodRoot) {
  // gmodRoot = .../steamapps/common/GarrysMod
  auto pos = gmodRoot.rfind("/common/");
  if (pos == std::string::npos) pos = gmodRoot.rfind("\\common\\");
  if (pos != std::string::npos) {
    std::string base = gmodRoot.substr(0, pos);
    std::string w = base + "/workshop/content/4000";
    if (IsDir(w)) return w;
  }
  // fallback relative
  const char* home = getenv("HOME");
  if (home) {
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

static void ParseNomount(const std::string& path, std::unordered_set<std::string>& out) {
  out.clear();
  std::string body = ReadFile(path);
  // VDF: "N" "wsid" pairs
  size_t i = 0;
  while (i < body.size()) {
    if (body[i] == '"') {
      auto j = body.find('"', i + 1);
      if (j == std::string::npos) break;
      std::string a = body.substr(i + 1, j - i - 1);
      i = j + 1;
      while (i < body.size() && (body[i] == ' ' || body[i] == '\t' || body[i] == '\n' || body[i] == '\r'))
        ++i;
      if (i < body.size() && body[i] == '"') {
        auto k = body.find('"', i + 1);
        if (k == std::string::npos) break;
        std::string b = body.substr(i + 1, k - i - 1);
        i = k + 1;
        // values that look like workshop ids (digits)
        bool digits = !b.empty() && std::all_of(b.begin(), b.end(), [](char c) { return c >= '0' && c <= '9'; });
        if (digits) out.insert(b);
        else if (std::all_of(a.begin(), a.end(), [](char c) { return c >= '0' && c <= '9'; }) && a.size() > 5)
          out.insert(a);
      }
    } else {
      ++i;
    }
  }
}

bool Addons_WriteNomount(const AddonManager& m, std::string& err) {
  std::string path = m.gmodRoot + "/garrysmod/cfg/addonnomount.txt";
  std::ostringstream ss;
  ss << "\"addonnomount\"\n{\n";
  int n = 1;
  // Sort for stable file
  std::vector<std::string> ids(m.nomount.begin(), m.nomount.end());
  std::sort(ids.begin(), ids.end());
  for (const auto& id : ids) {
    ss << "\t\"" << n++ << "\"\t\t\"" << id << "\"\n";
  }
  ss << "}\n";
  std::ofstream f(path);
  if (!f) {
    err = "cannot write " + path;
    return false;
  }
  f << ss.str();
  return true;
}

void Addons_Load(AddonManager& m, const std::string& gmodRoot) {
  m = AddonManager{};
  m.gmodRoot = gmodRoot;
  m.workshopRoot = FindWorkshopContent4000(gmodRoot);
  ParseNomount(gmodRoot + "/garrysmod/cfg/addonnomount.txt", m.nomount);

  // Local addons
  std::string localDir = gmodRoot + "/garrysmod/addons";
  if (DIR* d = opendir(localDir.c_str())) {
    while (dirent* e = readdir(d)) {
      if (e->d_name[0] == '.') continue;
      std::string name = e->d_name;
      std::string full = localDir + "/" + name;
      if (!IsDir(full)) continue;
      bool disabledFolder = name.size() > 9 && name.compare(name.size() - 9, 9, ".disabled") == 0;
      AddonEntry a;
      a.kind = "local";
      a.id = "local:" + name;
      a.enabled = !disabledFolder;
      std::string title = name;
      if (disabledFolder) title = name.substr(0, name.size() - 9);
      std::string aj = ReadFile(full + "/addon.json");
      if (aj.empty() && disabledFolder) {
        // try without suffix path already is .disabled folder
      }
      std::string t = ParseJsonTitle(aj);
      if (!t.empty()) title = t;
      a.title = title + (disabledFolder ? " [local off]" : " [local]");
      m.addons.push_back(std::move(a));
    }
    closedir(d);
  }

  // Workshop content folders
  if (!m.workshopRoot.empty()) {
    if (DIR* d = opendir(m.workshopRoot.c_str())) {
      while (dirent* e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        std::string id = e->d_name;
        bool digits = !id.empty() && std::all_of(id.begin(), id.end(), [](char c) {
          return c >= '0' && c <= '9';
        });
        if (!digits) continue;
        std::string full = m.workshopRoot + "/" + id;
        if (!IsDir(full)) continue;
        AddonEntry a;
        a.kind = "workshop";
        a.id = id;
        a.enabled = m.nomount.count(id) == 0;
        std::string title = id;
        // prefer addon.json inside
        std::string aj = ReadFile(full + "/addon.json");
        if (aj.empty()) {
          // search one level for addon.json
          if (DIR* sub = opendir(full.c_str())) {
            while (dirent* se = readdir(sub)) {
              if (std::strcmp(se->d_name, "addon.json") == 0) {
                aj = ReadFile(full + "/addon.json");
                break;
              }
            }
            closedir(sub);
          }
        }
        std::string t = ParseJsonTitle(aj);
        if (!t.empty()) title = t;
        a.title = title;
        m.addons.push_back(std::move(a));
      }
      closedir(d);
    }
  }

  std::sort(m.addons.begin(), m.addons.end(), [](const AddonEntry& a, const AddonEntry& b) {
    if (a.enabled != b.enabled) return a.enabled > b.enabled; // enabled first
    return a.title < b.title;
  });
  m.selected = 0;
  m.scroll = 0;
  char st[128];
  snprintf(st, sizeof(st), "ADDONS %zu total · %d on · %d off",
           m.addons.size(), Addons_EnabledCount(m), Addons_DisabledCount(m));
  m.status = st;
}

int Addons_EnabledCount(const AddonManager& m) {
  int n = 0;
  for (auto& a : m.addons) if (a.enabled) ++n;
  return n;
}
int Addons_DisabledCount(const AddonManager& m) {
  return (int)m.addons.size() - Addons_EnabledCount(m);
}

bool Addons_ToggleSelected(AddonManager& m, std::string& err) {
  if (m.addons.empty()) {
    err = "no addons";
    return false;
  }
  m.selected = std::clamp(m.selected, 0, (int)m.addons.size() - 1);
  AddonEntry& a = m.addons[m.selected];

  if (a.kind == "workshop") {
    if (a.enabled) {
      m.nomount.insert(a.id);
      a.enabled = false;
    } else {
      m.nomount.erase(a.id);
      a.enabled = true;
    }
    if (!Addons_WriteNomount(m, err)) return false;
    m.status = a.enabled ? ("ENABLED " + a.title) : ("DISABLED " + a.title);
    return true;
  }

  // Local: rename folder <-> .disabled
  if (a.id.rfind("local:", 0) != 0) {
    err = "bad local id";
    return false;
  }
  std::string folder = a.id.substr(6);
  std::string base = m.gmodRoot + "/garrysmod/addons/";
  if (a.enabled) {
    // disable
    std::string from = base + folder;
    std::string to = base + folder + ".disabled";
    if (folder.size() > 9 && folder.compare(folder.size() - 9, 9, ".disabled") == 0) {
      err = "already disabled path";
      return false;
    }
    if (rename(from.c_str(), to.c_str()) != 0) {
      err = "rename failed (local disable)";
      return false;
    }
    a.enabled = false;
    a.id = "local:" + folder + ".disabled";
    a.title = folder + " [local off]";
  } else {
    std::string from = base + folder;
    std::string bare = folder;
    if (bare.size() > 9 && bare.compare(bare.size() - 9, 9, ".disabled") == 0)
      bare = bare.substr(0, bare.size() - 9);
    std::string to = base + bare;
    if (rename(from.c_str(), to.c_str()) != 0) {
      err = "rename failed (local enable)";
      return false;
    }
    a.enabled = true;
    a.id = "local:" + bare;
    a.title = bare + " [local]";
  }
  m.status = a.enabled ? ("ENABLED " + a.title) : ("DISABLED " + a.title);
  return true;
}
