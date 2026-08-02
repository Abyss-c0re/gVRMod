#include "addons_mgr.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "../third_party/stb_image.h"

static bool IsDir(const std::string& p) {
  struct stat st{};
  return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
static bool IsFile(const std::string& p) {
  struct stat st{};
  return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}
static std::string ReadFile(const std::string& path) {
  std::ifstream f(path);
  if (!f) return {};
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
static void MkDirP(const std::string& p) {
  // one-level + parents via mkdir -p
  std::string cmd = "mkdir -p '" + p + "'";
  system(cmd.c_str());
}

static std::string ParseJsonTitle(const std::string& json) {
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

static void ParseNomount(const std::string& path, std::unordered_set<std::string>& out) {
  out.clear();
  std::string body = ReadFile(path);
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
        bool digits = !b.empty() && std::all_of(b.begin(), b.end(), [](char c) { return c >= '0' && c <= '9'; });
        if (digits) out.insert(b);
        else if (!a.empty() && a.size() > 5 &&
                 std::all_of(a.begin(), a.end(), [](char c) { return c >= '0' && c <= '9'; }))
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
  std::vector<std::string> ids(m.nomount.begin(), m.nomount.end());
  std::sort(ids.begin(), ids.end());
  for (const auto& id : ids)
    ss << "\t\"" << n++ << "\"\t\t\"" << id << "\"\n";
  ss << "}\n";
  std::ofstream f(path);
  if (!f) {
    err = "cannot write " + path;
    return false;
  }
  f << ss.str();
  return true;
}

static bool LoadThumbFile(const std::string& path, AddonEntry& a) {
  if (!IsFile(path)) return false;
  int w = 0, h = 0, ch = 0;
  unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
  if (!data || w <= 0 || h <= 0) {
    if (data) stbi_image_free(data);
    return false;
  }
  // Downscale to 48x48 into entry
  const int tw = 48, th = 48;
  a.thumbW = tw;
  a.thumbH = th;
  a.thumbRgba.assign(tw * th * 4, 0);
  for (int y = 0; y < th; ++y) {
    for (int x = 0; x < tw; ++x) {
      int sx = x * w / tw;
      int sy = y * h / th;
      int si = (sy * w + sx) * 4;
      int di = (y * tw + x) * 4;
      a.thumbRgba[di + 0] = data[si + 0];
      a.thumbRgba[di + 1] = data[si + 1];
      a.thumbRgba[di + 2] = data[si + 2];
      a.thumbRgba[di + 3] = data[si + 3];
    }
  }
  stbi_image_free(data);
  return true;
}

static bool FindLocalPreview(const std::string& dir, AddonEntry& a) {
  if (dir.empty()) return false;
  const char* names[] = {
    "addon.jpg", "addon.png", "preview.jpg", "preview.png",
    "thumb.jpg", "thumb.png", "icon.jpg", "icon.png",
    "logo.jpg", "logo.png",
  };
  for (auto n : names) {
    if (LoadThumbFile(dir + "/" + n, a)) return true;
  }
  // shallow scan for first jpg/png
  if (DIR* d = opendir(dir.c_str())) {
    while (dirent* e = readdir(d)) {
      std::string n = e->d_name;
      if (n.size() < 5) continue;
      auto lower = n;
      for (char& c : lower) if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
      if (lower.size() > 4 &&
          (lower.compare(lower.size() - 4, 4, ".jpg") == 0 ||
           lower.compare(lower.size() - 4, 4, ".png") == 0 ||
           lower.compare(lower.size() - 5, 5, ".jpeg") == 0)) {
        if (LoadThumbFile(dir + "/" + n, a)) {
          closedir(d);
          return true;
        }
      }
    }
    closedir(d);
  }
  return false;
}

static bool FetchSteamPreview(const std::string& wsid, const std::string& cachePath) {
  // Steam public API — no key required for GetPublishedFileDetails
  std::string tmpJson = cachePath + ".json";
  std::string cmd =
      "curl -fsSL --max-time 4 -X POST "
      "-d 'itemcount=1&publishedfileids[0]=" + wsid + "' "
      "'https://api.steampowered.com/ISteamRemoteStorage/GetPublishedFileDetails/v1/' "
      "-o '" + tmpJson + "' 2>/dev/null";
  if (system(cmd.c_str()) != 0) return false;
  std::string json = ReadFile(tmpJson);
  unlink(tmpJson.c_str());
  // "preview_url":"https://..."
  auto pos = json.find("\"preview_url\"");
  if (pos == std::string::npos) return false;
  pos = json.find("http", pos);
  if (pos == std::string::npos) return false;
  auto end = json.find_first_of("\"", pos);
  if (end == std::string::npos) return false;
  std::string url = json.substr(pos, end - pos);
  // unescape \/
  std::string clean;
  for (size_t i = 0; i < url.size(); ++i) {
    if (url[i] == '\\' && i + 1 < url.size() && url[i + 1] == '/') {
      clean.push_back('/');
      ++i;
    } else clean.push_back(url[i]);
  }
  std::string dl =
      "curl -fsSL --max-time 6 -o '" + cachePath + "' '" + clean + "' 2>/dev/null";
  return system(dl.c_str()) == 0 && IsFile(cachePath);
}

void Addons_Load(AddonManager& m, const std::string& gmodRoot) {
  m = AddonManager{};
  m.gmodRoot = gmodRoot;
  m.workshopRoot = FindWorkshopContent4000(gmodRoot);
  if (const char* home = getenv("HOME")) {
    m.thumbCache = std::string(home) + "/.cache/gvrmod/thumbs";
    MkDirP(m.thumbCache);
  } else {
    m.thumbCache = "/tmp/cube_webui_thumbs";
    MkDirP(m.thumbCache);
  }
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
      a.dirPath = full;
      std::string title = disabledFolder ? name.substr(0, name.size() - 9) : name;
      std::string t = ParseJsonTitle(ReadFile(full + "/addon.json"));
      if (!t.empty()) title = t;
      a.title = title;
      m.addons.push_back(std::move(a));
    }
    closedir(d);
  }

  // Workshop folders (all of them — pagination handles display)
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
        a.dirPath = full;
        std::string title = id;
        std::string aj = ReadFile(full + "/addon.json");
        if (aj.empty()) {
          // one-level search
          if (DIR* sub = opendir(full.c_str())) {
            while (dirent* se = readdir(sub)) {
              if (se->d_type == DT_DIR && se->d_name[0] != '.') {
                std::string subp = full + "/" + se->d_name + "/addon.json";
                if (IsFile(subp)) {
                  aj = ReadFile(subp);
                  a.dirPath = full + "/" + se->d_name;
                  break;
                }
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
    if (a.enabled != b.enabled) return a.enabled > b.enabled;
    return a.title < b.title;
  });
  m.selected = 0;
  m.page = 0;
  char st[160];
  snprintf(st, sizeof(st), "ADDONS %zu · ON %d · OFF %d · page 1/%d",
           m.addons.size(), Addons_EnabledCount(m), Addons_DisabledCount(m),
           std::max(1, Addons_PageCount(m)));
  m.status = st;
  fprintf(stderr, "[cube_webui] addons loaded: %zu (workshop root %s)\n",
          m.addons.size(), m.workshopRoot.empty() ? "(none)" : m.workshopRoot.c_str());
}

void Addons_FilteredIndices(const AddonManager& m, std::vector<int>& out) {
  out.clear();
  out.reserve(m.addons.size());
  for (int i = 0; i < (int)m.addons.size(); ++i) {
    const auto& a = m.addons[i];
    if (m.filter == 1 && !a.enabled) continue;
    if (m.filter == 2 && a.enabled) continue;
    if (m.filter == 3 && a.kind != "workshop") continue;
    if (m.filter == 4 && a.kind != "local") continue;
    out.push_back(i);
  }
}

int Addons_PageCount(const AddonManager& m) {
  std::vector<int> idx;
  Addons_FilteredIndices(m, idx);
  if (idx.empty()) return 1;
  return (int)((idx.size() + m.pageSize - 1) / m.pageSize);
}

void Addons_ClampPage(AddonManager& m) {
  int pc = Addons_PageCount(m);
  if (m.page < 0) m.page = 0;
  if (m.page >= pc) m.page = pc - 1;
}

int Addons_EnabledCount(const AddonManager& m) {
  int n = 0;
  for (auto& a : m.addons) if (a.enabled) ++n;
  return n;
}
int Addons_DisabledCount(const AddonManager& m) {
  return (int)m.addons.size() - Addons_EnabledCount(m);
}

bool Addons_ToggleIndex(AddonManager& m, int absIndex, std::string& err) {
  if (absIndex < 0 || absIndex >= (int)m.addons.size()) {
    err = "bad index";
    return false;
  }
  m.selected = absIndex;
  AddonEntry& a = m.addons[absIndex];

  if (a.kind == "workshop") {
    if (a.enabled) {
      m.nomount.insert(a.id);
      a.enabled = false;
    } else {
      m.nomount.erase(a.id);
      a.enabled = true;
    }
    if (!Addons_WriteNomount(m, err)) return false;
    m.status = (a.enabled ? "ENABLED " : "DISABLED ") + a.title;
    return true;
  }

  if (a.id.rfind("local:", 0) != 0) {
    err = "bad local id";
    return false;
  }
  std::string folder = a.id.substr(6);
  std::string base = m.gmodRoot + "/garrysmod/addons/";
  if (a.enabled) {
    std::string from = base + folder;
    std::string to = base + folder + ".disabled";
    if (rename(from.c_str(), to.c_str()) != 0) {
      err = "rename failed (disable local)";
      return false;
    }
    a.enabled = false;
    a.id = "local:" + folder + ".disabled";
    a.dirPath = to;
  } else {
    std::string from = base + folder;
    std::string bare = folder;
    if (bare.size() > 9 && bare.compare(bare.size() - 9, 9, ".disabled") == 0)
      bare = bare.substr(0, bare.size() - 9);
    std::string to = base + bare;
    if (rename(from.c_str(), to.c_str()) != 0) {
      err = "rename failed (enable local)";
      return false;
    }
    a.enabled = true;
    a.id = "local:" + bare;
    a.dirPath = to;
  }
  m.status = (a.enabled ? "ENABLED " : "DISABLED ") + a.title;
  return true;
}

bool Addons_ToggleSelected(AddonManager& m, std::string& err) {
  std::vector<int> idx;
  Addons_FilteredIndices(m, idx);
  if (idx.empty()) {
    err = "no addons";
    return false;
  }
  // selected is absolute index; if out of filter, use first on page
  int abs = m.selected;
  bool ok = false;
  for (int i : idx) if (i == abs) { ok = true; break; }
  if (!ok) abs = idx[0];
  return Addons_ToggleIndex(m, abs, err);
}

void Addons_EnsureThumbsForPage(AddonManager& m) {
  if (m.addons.empty()) return;
  Addons_ClampPage(m);
  std::vector<int> idx;
  Addons_FilteredIndices(m, idx);
  int start = m.page * m.pageSize;
  int end = std::min((int)idx.size(), start + m.pageSize);

  // One network fetch per call max (avoid hitch storms)
  static int s_fetchCooldown = 0;
  if (s_fetchCooldown > 0) --s_fetchCooldown;

  for (int n = start; n < end; ++n) {
    int ai = idx[n];
    AddonEntry& a = m.addons[ai];
    if (a.thumbW > 0) continue;

    // 1) local preview in addon dir
    if (FindLocalPreview(a.dirPath, a)) continue;

    // 2) cached steam thumb
    if (a.kind == "workshop" && !m.thumbCache.empty()) {
      std::string cache = m.thumbCache + "/" + a.id + ".jpg";
      if (LoadThumbFile(cache, a)) continue;
      std::string cachePng = m.thumbCache + "/" + a.id + ".png";
      if (LoadThumbFile(cachePng, a)) continue;

      // 3) fetch one per call
      if (s_fetchCooldown == 0) {
        if (FetchSteamPreview(a.id, cache)) {
          LoadThumbFile(cache, a);
        }
        s_fetchCooldown = 8; // ~8 frames between fetches when called per frame
        break;
      }
    }
  }
}
