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
#include <cctype>
#include <chrono>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "stb_image.h"

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
  std::string cmd = "mkdir -p '" + p + "'";
  system(cmd.c_str());
}

static std::string ParseJsonStringField(const std::string& json, const char* key) {
  std::string pat = std::string("\"") + key + "\"";
  auto pos = json.find(pat);
  if (pos == std::string::npos) {
    std::string k2 = key;
    if (k2.size()) k2[0] = (char)std::toupper((unsigned char)k2[0]);
    pat = "\"" + k2 + "\"";
    pos = json.find(pat);
  }
  if (pos == std::string::npos) return {};
  pos = json.find(':', pos);
  if (pos == std::string::npos) return {};
  while (pos + 1 < json.size() && (json[pos + 1] == ' ' || json[pos + 1] == '\t')) ++pos;
  if (pos + 1 >= json.size() || json[pos + 1] != '"') return {};
  pos += 2;
  std::string out;
  for (size_t i = pos; i < json.size(); ++i) {
    if (json[i] == '\\' && i + 1 < json.size()) {
      char n = json[i + 1];
      if (n == 'n') out.push_back(' ');
      else if (n == '/' || n == '"' || n == '\\') out.push_back(n);
      else out.push_back(n);
      ++i;
      continue;
    }
    if (json[i] == '"') break;
    out.push_back(json[i]);
  }
  return out;
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
  };
  for (auto n : names)
    if (LoadThumbFile(dir + "/" + n, a)) return true;
  if (DIR* d = opendir(dir.c_str())) {
    while (dirent* e = readdir(d)) {
      std::string n = e->d_name;
      if (n.size() < 5) continue;
      auto lower = n;
      for (char& c : lower) if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
      if (lower.find(".jpg") != std::string::npos || lower.find(".png") != std::string::npos ||
          lower.find(".jpeg") != std::string::npos) {
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

static bool LoadCachedTitle(const std::string& cacheDir, const std::string& id, std::string& title) {
  std::string p = cacheDir + "/" + id + ".title";
  title = ReadFile(p);
  while (!title.empty() && (title.back() == '\n' || title.back() == '\r')) title.pop_back();
  return !title.empty();
}
static void SaveCachedTitle(const std::string& cacheDir, const std::string& id, const std::string& title) {
  std::ofstream f(cacheDir + "/" + id + ".title");
  if (f) f << title;
}

struct SteamMeta {
  std::string id;
  std::string title;
  std::string previewPath;
  bool ok = false;
};

// Completed jobs buffer (worker → main). Keyed by cache path ownership via id only.
static std::mutex g_doneMu;
static std::vector<SteamMeta> g_doneJobs;

static std::string UnescapeUrl(const std::string& url) {
  std::string clean;
  for (size_t i = 0; i < url.size(); ++i) {
    if (url[i] == '\\' && i + 1 < url.size() && url[i + 1] == '/') {
      clean.push_back('/');
      ++i;
    } else clean.push_back(url[i]);
  }
  return clean;
}

static SteamMeta FetchSteamMetaSync(const std::string& cacheDir, const std::string& wsid) {
  SteamMeta m;
  m.id = wsid;
  std::string tmpJson = cacheDir + "/" + wsid + ".json";
  std::string imgPath = cacheDir + "/" + wsid + ".jpg";

  LoadCachedTitle(cacheDir, wsid, m.title);
  if (IsFile(imgPath)) m.previewPath = imgPath;
  if (!m.title.empty() && !m.previewPath.empty()) {
    m.ok = true;
    return m;
  }

  // Steam published file details (title + preview_url)
  std::string cmd =
      "curl -fsSL --max-time 8 -X POST "
      "-d 'itemcount=1&publishedfileids[0]=" + wsid + "' "
      "'https://api.steampowered.com/ISteamRemoteStorage/GetPublishedFileDetails/v1/' "
      "-o '" + tmpJson + "' 2>/dev/null";
  int rc = system(cmd.c_str());
  if (rc != 0) {
    m.ok = !m.title.empty() || !m.previewPath.empty();
    return m;
  }
  std::string json = ReadFile(tmpJson);
  unlink(tmpJson.c_str());

  // Prefer title inside publishedfiledetails block when present
  std::string title = ParseJsonStringField(json, "title");
  if (!title.empty() && title != "0") {
    m.title = title;
    SaveCachedTitle(cacheDir, wsid, title);
  }
  std::string url = ParseJsonStringField(json, "preview_url");
  if (url.empty()) url = ParseJsonStringField(json, "file_url");
  url = UnescapeUrl(url);
  if (!url.empty() && m.previewPath.empty()) {
    std::string dl =
        "curl -fsSL --max-time 10 -o '" + imgPath + "' '" + url + "' 2>/dev/null";
    if (system(dl.c_str()) == 0 && IsFile(imgPath) && IsFile(imgPath)) {
      struct stat st{};
      if (stat(imgPath.c_str(), &st) == 0 && st.st_size > 64)
        m.previewPath = imgPath;
      else
        unlink(imgPath.c_str());
    }
  }
  m.ok = !m.title.empty() || !m.previewPath.empty();
  return m;
}

static void WorkerLoop(AddonManager* m) {
  while (!m->stopWorkers.load()) {
    std::string id;
    {
      std::unique_lock<std::mutex> lk(m->jobMu);
      m->jobCv.wait_for(lk, std::chrono::milliseconds(50), [&] {
        return m->stopWorkers.load() || !m->pendingIds.empty();
      });
      if (m->stopWorkers.load()) break;
      if (m->pendingIds.empty()) continue;
      id = m->pendingIds.front();
      m->pendingIds.pop_front();
      m->inFlight.insert(id);
    }
    SteamMeta meta = FetchSteamMetaSync(m->thumbCache, id);
    {
      std::lock_guard<std::mutex> lk(m->jobMu);
      m->inFlight.erase(id);
    }
    {
      std::lock_guard<std::mutex> lk(g_doneMu);
      g_doneJobs.push_back(std::move(meta));
    }
  }
}

static void EnsureWorkers(AddonManager& m) {
  if (m.stopWorkers.load()) return;
  while ((int)m.workers.size() < m.maxWorkers) {
    m.workers.emplace_back(WorkerLoop, &m);
  }
}

void Addons_Shutdown(AddonManager& m) {
  m.stopWorkers.store(true);
  m.jobCv.notify_all();
  for (auto& t : m.workers) {
    if (t.joinable()) t.join();
  }
  m.workers.clear();
  {
    std::lock_guard<std::mutex> lk(m.jobMu);
    m.pendingIds.clear();
    m.inFlight.clear();
  }
}

void Addons_Load(AddonManager& m, const std::string& gmodRoot) {
  Addons_Shutdown(m);
  // Clear any leftover done jobs from previous session
  {
    std::lock_guard<std::mutex> lk(g_doneMu);
    g_doneJobs.clear();
  }
  // Reset fields in-place (mutex/cv/atomic are non-assignable)
  m.gmodRoot = gmodRoot;
  m.workshopRoot.clear();
  m.thumbCache.clear();
  m.addons.clear();
  m.nomount.clear();
  m.selected = 0;
  m.page = 0;
  m.pageSize = 8;
  m.filter = 0;
  m.status.clear();
  m.doneThisSession = 0;
  {
    std::lock_guard<std::mutex> lk(m.jobMu);
    m.pendingIds.clear();
    m.inFlight.clear();
  }
  m.workshopRoot = FindWorkshopContent4000(gmodRoot);
  m.stopWorkers.store(false);
  if (const char* home = getenv("HOME")) {
    m.thumbCache = std::string(home) + "/.cache/gvrmod/thumbs";
    MkDirP(m.thumbCache);
  } else {
    m.thumbCache = "/tmp/cube_webui_thumbs";
    MkDirP(m.thumbCache);
  }
  ParseNomount(gmodRoot + "/garrysmod/cfg/addonnomount.txt", m.nomount);

  // Local
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
      std::string t = ParseJsonStringField(ReadFile(full + "/addon.json"), "title");
      if (!t.empty()) title = t;
      a.title = title;
      FindLocalPreview(full, a);
      m.addons.push_back(std::move(a));
    }
    closedir(d);
  }

  // Workshop (often only .bin — titles from disk cache then Steam async)
  int needMeta = 0;
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
        std::string aj = ReadFile(full + "/addon.json");
        std::string t = ParseJsonStringField(aj, "title");
        if (t.empty()) {
          if (DIR* sub = opendir(full.c_str())) {
            while (dirent* se = readdir(sub)) {
              if (se->d_name[0] == '.') continue;
              std::string subp = full + "/" + se->d_name + "/addon.json";
              if (IsFile(subp)) {
                t = ParseJsonStringField(ReadFile(subp), "title");
                if (!t.empty()) a.dirPath = full + "/" + se->d_name;
                break;
              }
            }
            closedir(sub);
          }
        }
        if (t.empty()) LoadCachedTitle(m.thumbCache, id, t);
        a.title = t.empty() ? ("Workshop " + id) : t;
        if (!FindLocalPreview(a.dirPath, a)) {
          std::string img = m.thumbCache + "/" + id + ".jpg";
          if (!LoadThumbFile(img, a)) {
            std::string png = m.thumbCache + "/" + id + ".png";
            LoadThumbFile(png, a);
          }
        }
        // Need Steam if title is still placeholder OR no thumb
        bool titleOk = !t.empty();
        bool thumbOk = a.thumbW > 0;
        if (!titleOk || !thumbOk) {
          a.metaPending = true;
          ++needMeta;
        }
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
  char st[200];
  snprintf(st, sizeof(st), "ADDONS %zu · ON %d · OFF %d · meta %d",
           m.addons.size(), Addons_EnabledCount(m), Addons_DisabledCount(m), needMeta);
  m.status = st;
  fprintf(stderr, "[cube_webui] addons loaded: %zu (ws=%s cache=%s need_meta=%d)\n",
          m.addons.size(), m.workshopRoot.empty() ? "none" : m.workshopRoot.c_str(),
          m.thumbCache.c_str(), needMeta);
  EnsureWorkers(m);
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
      err = "rename failed (disable)";
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
      err = "rename failed (enable)";
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
  return Addons_ToggleIndex(m, m.selected, err);
}

bool Addons_PumpAsync(AddonManager& m) {
  if (m.addons.empty()) return false;
  EnsureWorkers(m);
  Addons_ClampPage(m);
  bool contentChanged = false;

  // Apply finished jobs (UI thread only — never block on curl)
  std::vector<SteamMeta> done;
  {
    std::lock_guard<std::mutex> lk(g_doneMu);
    done.swap(g_doneJobs);
  }
  for (auto& meta : done) {
    for (auto& a : m.addons) {
      if (a.kind != "workshop" || a.id != meta.id) continue;
      a.metaQueued = false;
      if (!meta.title.empty() &&
          (a.title.empty() || a.title.rfind("Workshop ", 0) == 0)) {
        a.title = meta.title;
        contentChanged = true;
      }
      if (!meta.previewPath.empty() && a.thumbW == 0) {
        if (LoadThumbFile(meta.previewPath, a)) contentChanged = true;
      }
      bool titleOk = !a.title.empty() && a.title.rfind("Workshop ", 0) != 0;
      bool thumbOk = a.thumbW > 0;
      if (titleOk && thumbOk) {
        a.metaPending = false;
        a.metaFailed = false;
      } else if (!meta.ok) {
        a.metaPending = false;
        a.metaFailed = true; // don't thrash
      } else {
        // partial — mark done enough for UI
        a.metaPending = false;
        a.metaFailed = !titleOk && !thumbOk;
      }
      ++m.doneThisSession;
      contentChanged = true;
      break;
    }
  }

  // Queue missing meta for current page + next page (prefetch)
  std::vector<int> idx;
  Addons_FilteredIndices(m, idx);
  int start = m.page * m.pageSize;
  int end = std::min((int)idx.size(), start + m.pageSize * 2);
  int queued = 0;
  {
    std::lock_guard<std::mutex> lk(m.jobMu);
    for (int n = start; n < end; ++n) {
      auto& a = m.addons[idx[n]];
      if (a.kind != "workshop") continue;
      if (a.metaFailed) continue;
      bool titleOk = !a.title.empty() && a.title.rfind("Workshop ", 0) != 0;
      bool thumbOk = a.thumbW > 0;
      if (titleOk && thumbOk) {
        a.metaPending = false;
        continue;
      }
      if (a.metaQueued || m.inFlight.count(a.id)) {
        a.metaPending = true;
        continue;
      }
      // already in pending queue?
      bool q = false;
      for (auto& p : m.pendingIds) if (p == a.id) { q = true; break; }
      if (!q) {
        m.pendingIds.push_back(a.id);
        a.metaQueued = true;
        a.metaPending = true;
        ++queued;
      }
    }
  }
  if (queued) m.jobCv.notify_all();

  // Status line (not every frame spam)
  static int frame = 0;
  if ((++frame % 30) == 0) {
    int pending = 0, inflight = 0;
    {
      std::lock_guard<std::mutex> lk(m.jobMu);
      pending = (int)m.pendingIds.size();
      inflight = (int)m.inFlight.size();
    }
    if (pending + inflight > 0) {
      char st[128];
      snprintf(st, sizeof(st), "icons/titles: %d queue · %d active · %d done",
               pending, inflight, m.doneThisSession);
      if (m.status != st) {
        m.status = st;
        contentChanged = true;
      }
    }
  }
  return contentChanged;
}
