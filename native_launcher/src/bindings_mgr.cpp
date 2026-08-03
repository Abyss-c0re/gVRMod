#include "bindings_mgr.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

// Minimal JSON string extractor for our bindings/custom files.
static std::string ExtractString(const std::string& body, size_t from, const char* key) {
  std::string pat = std::string("\"") + key + "\"";
  auto p = body.find(pat, from);
  if (p == std::string::npos) return {};
  p = body.find(':', p);
  if (p == std::string::npos) return {};
  p = body.find('"', p);
  if (p == std::string::npos) return {};
  auto e = body.find('"', p + 1);
  if (e == std::string::npos) return {};
  return body.substr(p + 1, e - p - 1);
}

const std::vector<std::string>& Bindings_AllSources() {
  static const std::vector<std::string> s = {
    "left_x", "left_y", "left_menu", "left_stick_click", "left_thumbrest",
    "left_trigger", "left_squeeze",
    "right_a", "right_b", "right_stick_click", "right_thumbrest",
    "right_trigger", "right_squeeze",
    "left_stick_north", "left_stick_south", "left_stick_east", "left_stick_west",
    "right_stick_north", "right_stick_south", "right_stick_east", "right_stick_west",
  };
  return s;
}

const char* Bindings_SourceLabel(const std::string& id) {
  if (id == "left_x") return "Left X";
  if (id == "left_y") return "Left Y";
  if (id == "left_menu") return "Left Menu";
  if (id == "left_stick_click") return "L Stick Click";
  if (id == "left_thumbrest") return "L Thumbrest";
  if (id == "left_trigger") return "Left Trigger";
  if (id == "left_squeeze") return "Left Grip";
  if (id == "right_a") return "Right A";
  if (id == "right_b") return "Right B";
  if (id == "right_stick_click") return "R Stick Click";
  if (id == "right_thumbrest") return "R Thumbrest";
  if (id == "right_trigger") return "Right Trigger";
  if (id == "right_squeeze") return "Right Grip";
  if (id == "left_stick_north") return "L Stick Up";
  if (id == "left_stick_south") return "L Stick Down";
  if (id == "left_stick_east") return "L Stick Right";
  if (id == "left_stick_west") return "L Stick Left";
  if (id == "right_stick_north") return "R Stick Up";
  if (id == "right_stick_south") return "R Stick Down";
  if (id == "right_stick_east") return "R Stick Right";
  if (id == "right_stick_west") return "R Stick Left";
  return id.c_str();
}

const std::vector<BindActionInfo>& Bindings_LogicalActions() {
  static const std::vector<BindActionInfo> a = {
    {"boolean_primaryfire", "Primary Fire", "main"},
    {"boolean_secondaryfire", "Secondary Fire", "main"},
    {"boolean_left_primaryfire", "Left Primary Fire", "main"},
    {"boolean_jump", "Jump", "main"},
    {"boolean_crouch", "Crouch", "main"},
    {"boolean_use", "Use", "main"},
    {"boolean_spawnmenu", "Spawn Menu", "both"},
    {"boolean_changeweapon", "Weapon Menu", "main"},
    {"boolean_reload", "Reload", "both"},
    {"boolean_sprint", "Sprint", "main"},
    {"boolean_flashlight", "Flashlight", "main"},
    {"boolean_left_pickup", "Left Grip / Pickup", "both"},
    {"boolean_right_pickup", "Right Grip / Pickup", "both"},
    {"boolean_teleport", "Teleport", "main"},
    {"boolean_undo", "Undo", "main"},
    {"boolean_chat", "Chat / Zoom", "main"},
    {"boolean_menucontext", "Context Menu", "main"},
    {"boolean_walkkey", "Walk Key", "main"},
    {"boolean_handbrake", "Handbrake", "driving"},
    {"boolean_turbo", "Turbo", "driving"},
    {"boolean_exit", "Exit Vehicle", "driving"},
    {"boolean_signal_left", "Signal Left", "driving"},
    {"boolean_signal_right", "Signal Right", "driving"},
    {"boolean_switch_weapon", "Switch Weapon", "driving"},
    {"boolean_alt_turret", "Alt Turret", "driving"},
    {"boolean_turret", "Turret", "driving"},
    {"boolean_horn", "Horn", "driving"},
    {"boolean_shift_up", "Shift Up", "driving"},
    {"boolean_shift_down", "Shift Down", "driving"},
    {"boolean_lights", "Lights", "driving"},
    {"boolean_siren", "Siren", "driving"},
    {"boolean_toggle_engine", "Toggle Engine", "driving"},
  };
  return a;
}

static BindRule R(std::initializer_list<const char*> src, const char* mode, const char* set) {
  BindRule r;
  r.mode = mode ? mode : "any";
  r.set = set ? set : "";
  for (auto s : src) r.sources.push_back(s);
  return r;
}

// Lua util.TableToJSON of CustomActions:
//   [ ["name","press","release","0"], ... ]  or with "driving":true on objects
// Also accept legacy line-based files.
static void AppendCustomIfNew(BindingsManager& m, const std::string& name, bool driving) {
  if (name.empty()) return;
  for (auto& L : m.logical)
    if (L.id == name) return;
  m.logical.push_back({name, "Custom: " + name, driving ? "driving" : "custom"});
}

static std::string ExtractQuoted(const std::string& s, size_t& i) {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r' || s[i] == ','))
    ++i;
  if (i >= s.size() || s[i] != '"') return {};
  ++i;
  std::string out;
  while (i < s.size()) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      out.push_back(s[i + 1]);
      i += 2;
      continue;
    }
    if (s[i] == '"') {
      ++i;
      break;
    }
    out.push_back(s[i++]);
  }
  return out;
}

void Bindings_LoadCustomActions(BindingsManager& m) {
  m.logical = Bindings_LogicalActions();
  if (m.gmodRoot.empty()) return;
  std::string path = m.gmodRoot + "/garrysmod/data/vrmod/vrmod_custom_actions.txt";
  std::string body = [&]() {
    std::ifstream f(path);
    if (!f) return std::string{};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
  }();
  if (body.empty()) return;

  // JSON array (Lua product format)
  auto first = body.find_first_not_of(" \t\r\n");
  if (first != std::string::npos && body[first] == '[') {
    size_t i = first + 1;
    while (i < body.size()) {
      while (i < body.size() && (body[i] == ' ' || body[i] == '\t' || body[i] == '\n' ||
                                 body[i] == '\r' || body[i] == ','))
        ++i;
      if (i >= body.size() || body[i] == ']') break;
      if (body[i] == '[') {
        // array row: ["name", "press", "release", "0|1"]
        ++i;
        std::string name = ExtractQuoted(body, i);
        std::string press = ExtractQuoted(body, i);
        std::string release = ExtractQuoted(body, i);
        std::string drive = ExtractQuoted(body, i);
        (void)press;
        (void)release;
        bool isDrv = (drive == "1" || drive == "true" || drive == "driving");
        // skip to end of this array
        int depth = 1;
        while (i < body.size() && depth > 0) {
          if (body[i] == '[') ++depth;
          else if (body[i] == ']') --depth;
          else if (body[i] == '"') {
            ++i;
            while (i < body.size() && body[i] != '"') {
              if (body[i] == '\\') ++i;
              ++i;
            }
          }
          ++i;
        }
        AppendCustomIfNew(m, name, isDrv);
      } else if (body[i] == '{') {
        // object row — pull "1" or "name" and driving
        auto end = body.find('}', i);
        if (end == std::string::npos) break;
        std::string block = body.substr(i, end - i + 1);
        std::string name = ExtractString(block, 0, "1");
        if (name.empty()) name = ExtractString(block, 0, "name");
        bool isDrv = block.find("\"driving\":true") != std::string::npos ||
                     block.find("\"driving\": true") != std::string::npos ||
                     ExtractString(block, 0, "4") == "1";
        AppendCustomIfNew(m, name, isDrv);
        i = end + 1;
      } else {
        ++i;
      }
    }
    fprintf(stderr, "[cube_webui] custom actions (json): %zu total logical\n", m.logical.size());
    return;
  }

  // Legacy line-based
  std::istringstream iss(body);
  std::string line;
  while (std::getline(iss, line)) {
    if (line.empty() || line[0] == '#' || line[0] == '/') continue;
    std::string name, driving;
    if (line.find('\t') != std::string::npos) {
      std::istringstream ls(line);
      std::string a, b, c;
      std::getline(ls, a, '\t');
      std::getline(ls, b, '\t');
      std::getline(ls, c, '\t');
      name = a;
      driving = c;
    } else if (line.find(',') != std::string::npos) {
      name = line.substr(0, line.find(','));
    } else {
      name = line;
    }
    while (!name.empty() && (name.back() == ' ' || name.back() == '\r')) name.pop_back();
    while (!name.empty() && name.front() == ' ') name.erase(name.begin());
    if (name.empty()) continue;
    bool isDrv = (driving == "1" || driving == "driving" || driving == "true");
    AppendCustomIfNew(m, name, isDrv);
  }
}

void Bindings_DefaultMap(BindingsManager& m) {
  m.version = 2;
  m.preset = "quest3_touch";
  m.actions.clear();
  // Quest 3 / Touch gold (must match Lua DefaultMap)
  m.actions["boolean_primaryfire"] = R({"right_trigger"}, "any", "main");
  m.actions["boolean_secondaryfire"] = R({"left_trigger"}, "any", "main");
  m.actions["boolean_left_primaryfire"] = R({"left_trigger"}, "any", "main");
  m.actions["boolean_jump"] = R({"right_b"}, "any", "main");
  m.actions["boolean_crouch"] = R({"right_a"}, "any", "main");
  m.actions["boolean_use"] = R({"left_x"}, "any", "main");
  m.actions["boolean_flashlight"] = R({"left_menu"}, "any", "main");
  m.actions["boolean_sprint"] = R({"left_stick_click"}, "any", "main");
  m.actions["boolean_changeweapon"] = R({"right_stick_click"}, "any", "main");
  m.actions["boolean_teleport"] = R({"left_stick_click", "right_thumbrest"}, "all", "main");
  m.actions["boolean_spawnmenu"] = R({"left_y"}, "any", "");
  m.actions["boolean_left_pickup"] = R({"left_squeeze"}, "any", "");
  m.actions["boolean_right_pickup"] = R({"right_squeeze"}, "any", "");
  m.actions["boolean_reload"] = R({"left_thumbrest", "right_thumbrest"}, "all", "");
  m.actions["boolean_handbrake"] = R({"right_a"}, "any", "driving");
  m.actions["boolean_turbo"] = R({"right_b"}, "any", "driving");
  m.actions["boolean_exit"] = R({"left_x"}, "any", "driving");
  m.actions["boolean_switch_weapon"] = R({"left_thumbrest", "right_thumbrest"}, "all", "driving");
  m.actions["boolean_signal_left"] = R({"left_squeeze", "left_thumbrest"}, "all", "driving");
  m.actions["boolean_signal_right"] = R({"right_squeeze", "right_thumbrest"}, "all", "driving");
  m.actions["boolean_alt_turret"] = R({"right_squeeze", "right_stick_click"}, "all", "driving");
  m.actions["boolean_shift_up"] = R({"right_stick_north"}, "any", "driving");
  m.actions["boolean_shift_down"] = R({"right_stick_south"}, "any", "driving");
  m.actions["boolean_lights"] = R({"right_stick_east"}, "any", "driving");
  m.actions["boolean_siren"] = R({"right_stick_west"}, "any", "driving");
  m.actions["boolean_turret"] = R({"right_stick_click"}, "any", "driving");
  m.actions["boolean_horn"] = R({"left_stick_click"}, "any", "driving");
  Bindings_LoadCustomActions(m);
  m.dirty = false;
  m.status = "QUEST 3 / TOUCH DEFAULTS";
}

// Minimal JSON helpers (bindings file only — no full parser for arbitrary JSON)
static std::string JsonEscape(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '"' || c == '\\') { o.push_back('\\'); o.push_back(c); }
    else o.push_back(c);
  }
  return o;
}

static std::string Serialize(const BindingsManager& m) {
  std::ostringstream ss;
  ss << "{\n  \"version\": " << m.version << ",\n  \"preset\": \"" << JsonEscape(m.preset)
     << "\",\n  \"actions\": {\n";
  bool first = true;
  for (const auto& kv : m.actions) {
    if (!first) ss << ",\n";
    first = false;
    const auto& r = kv.second;
    ss << "    \"" << JsonEscape(kv.first) << "\": {\n";
    ss << "      \"mode\": \"" << (r.mode == "all" ? "all" : "any") << "\",\n";
    if (!r.set.empty())
      ss << "      \"set\": \"" << JsonEscape(r.set) << "\",\n";
    ss << "      \"sources\": [";
    for (size_t i = 0; i < r.sources.size(); ++i) {
      if (i) ss << ", ";
      ss << "\"" << JsonEscape(r.sources[i]) << "\"";
    }
    ss << "]\n    }";
  }
  ss << "\n  }\n}\n";
  return ss.str();
}

static void ParseActions(const std::string& body, BindingsManager& m) {
  // Find each action key under "actions"
  auto actPos = body.find("\"actions\"");
  if (actPos == std::string::npos) return;
  size_t i = body.find('{', actPos);
  if (i == std::string::npos) return;
  ++i;
  while (i < body.size()) {
    // skip to next "
    while (i < body.size() && body[i] != '"' && body[i] != '}') ++i;
    if (i >= body.size() || body[i] == '}') break;
    auto ke = body.find('"', i + 1);
    if (ke == std::string::npos) break;
    std::string action = body.substr(i + 1, ke - i - 1);
    i = ke + 1;
    auto brace = body.find('{', i);
    if (brace == std::string::npos) break;
    auto end = body.find('}', brace);
    if (end == std::string::npos) break;
    std::string block = body.substr(brace, end - brace + 1);
    BindRule r;
    r.mode = ExtractString(block, 0, "mode");
    if (r.mode != "all") r.mode = "any";
    r.set = ExtractString(block, 0, "set");
    // sources array
    auto sp = block.find("\"sources\"");
    if (sp != std::string::npos) {
      auto lb = block.find('[', sp);
      auto rb = block.find(']', lb);
      if (lb != std::string::npos && rb != std::string::npos) {
        size_t j = lb + 1;
        while (j < rb) {
          auto q1 = block.find('"', j);
          if (q1 == std::string::npos || q1 >= rb) break;
          auto q2 = block.find('"', q1 + 1);
          if (q2 == std::string::npos) break;
          r.sources.push_back(block.substr(q1 + 1, q2 - q1 - 1));
          j = q2 + 1;
        }
      }
    }
    if (!r.sources.empty())
      m.actions[action] = r;
    i = end + 1;
  }
}

bool Bindings_Load(BindingsManager& m, const std::string& gmodRoot) {
  m.gmodRoot = gmodRoot;
  m.filePath = gmodRoot + "/garrysmod/data/vrmod/vrmod_openxr_bindings.json";
  Bindings_DefaultMap(m); // includes custom actions + gold defaults
  std::ifstream f(m.filePath);
  if (!f) {
    m.status = "NO FILE — USING DEFAULTS (will save on edit)";
    return true;
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  std::string body = ss.str();
  if (body.empty()) return true;
  // merge user over defaults (same path Lua reads/writes)
  BindingsManager user = m;
  user.actions.clear();
  ParseActions(body, user);
  for (auto& kv : user.actions) {
    m.actions[kv.first] = kv.second;
    // surface unknown action ids (customs bound only in JSON)
    bool known = false;
    for (auto& L : m.logical)
      if (L.id == kv.first) {
        known = true;
        break;
      }
    if (!known)
      AppendCustomIfNew(m, kv.first, kv.second.set == "driving");
  }
  auto pr = ExtractString(body, 0, "preset");
  if (!pr.empty()) m.preset = pr;
  m.dirty = false;
  m.status = "SYNCED " + m.filePath;
  fprintf(stderr, "[cube_webui] bindings loaded %zu actions from %s (logical=%zu)\n",
          m.actions.size(), m.filePath.c_str(), m.logical.size());
  return true;
}

// Snapshot existing file before overwrite (timestamped). Keep last 12.
static void BackupBindingsFile(const std::string& path) {
  struct stat st {};
  if (stat(path.c_str(), &st) != 0 || st.st_size <= 0) return;
  char ts[32];
  std::time_t now = std::time(nullptr);
  std::strftime(ts, sizeof ts, "%Y%m%d_%H%M%S", std::localtime(&now));
  std::string bak = path + ".bak." + ts;
  // best-effort copy
  std::ifstream in(path, std::ios::binary);
  if (!in) return;
  std::ofstream out(bak, std::ios::binary);
  if (!out) return;
  out << in.rdbuf();
  out.close();
  fprintf(stderr, "[cube_webui] bindings backup → %s\n", bak.c_str());
  // prune old backups in same dir
  std::string dir = path;
  auto slash = dir.find_last_of('/');
  std::string base = (slash == std::string::npos) ? dir : dir.substr(0, slash);
  std::string prefix = "vrmod_openxr_bindings.json.bak.";
  DIR* d = opendir(base.c_str());
  if (!d) return;
  std::vector<std::string> baks;
  while (dirent* e = readdir(d)) {
    if (e->d_name[0] == '.') continue;
    std::string name = e->d_name;
    if (name.rfind(prefix, 0) == 0) baks.push_back(base + "/" + name);
  }
  closedir(d);
  std::sort(baks.begin(), baks.end());
  while (baks.size() > 12) {
    ::unlink(baks.front().c_str());
    baks.erase(baks.begin());
  }
}

bool Bindings_Save(BindingsManager& m, std::string& err) {
  if (m.gmodRoot.empty()) {
    err = "no gmod root";
    return false;
  }
  std::string dir = m.gmodRoot + "/garrysmod/data/vrmod";
  mkdir((m.gmodRoot + "/garrysmod/data").c_str(), 0755);
  mkdir(dir.c_str(), 0755);
  m.filePath = dir + "/vrmod_openxr_bindings.json";
  BackupBindingsFile(m.filePath);
  std::ofstream f(m.filePath);
  if (!f) {
    err = "cannot write " + m.filePath;
    return false;
  }
  f << Serialize(m);
  m.dirty = false;
  m.status = "SAVED BINDINGS";
  fprintf(stderr, "[cube_webui] bindings saved %s\n", m.filePath.c_str());
  return true;
}

void Bindings_ResetDefaults(BindingsManager& m) {
  // Quest 3 / Touch GOLD — identical to Lua DefaultMap() in cl_openxr_bindings.lua.
  // Backup, replace, write immediately so launcher + GMod share one file:
  // garrysmod/data/vrmod/vrmod_openxr_bindings.json
  if (!m.filePath.empty()) BackupBindingsFile(m.filePath);
  else if (!m.gmodRoot.empty())
    BackupBindingsFile(m.gmodRoot + "/garrysmod/data/vrmod/vrmod_openxr_bindings.json");
  Bindings_DefaultMap(m);
  m.preset = "quest3_touch";
  m.dirty = true;
  std::string err;
  if (!Bindings_Save(m, err)) {
    m.status = "GOLD IN MEMORY — SAVE FAIL: " + err;
  } else {
    m.status = "QUEST 3 GOLD RESTORED + SAVED (Lua loads same JSON)";
  }
}

void Bindings_RestoreAction(BindingsManager& m, const std::string& actionId) {
  BindingsManager def{};
  Bindings_DefaultMap(def);
  auto it = def.actions.find(actionId);
  if (it != def.actions.end())
    m.actions[actionId] = it->second;
  else
    m.actions.erase(actionId);
  m.dirty = true;
  m.status = "RESTORED " + actionId;
}

void Bindings_Filtered(const BindingsManager& m, std::vector<int>& out) {
  out.clear();
  const auto& list = m.logical.empty() ? Bindings_LogicalActions() : m.logical;
  for (int i = 0; i < (int)list.size(); ++i) {
    const auto& g = list[i].group;
    // 0=all 1=foot (main+both, not pure custom/driving) 2=vehicle 3=custom
    if (m.filter == 1) {
      if (g == "driving" || g == "custom") continue;
    } else if (m.filter == 2) {
      if (g != "driving" && g != "both") continue;
      if (g == "both") {
        /* keep spawnmenu/pickup/reload */
      }
    } else if (m.filter == 3) {
      if (g != "custom" && list[i].label.rfind("Custom:", 0) != 0) continue;
    }
    out.push_back(i);
  }
}

int Bindings_PageCount(const BindingsManager& m) {
  std::vector<int> idx;
  Bindings_Filtered(m, idx);
  if (idx.empty()) return 1;
  return (int)((idx.size() + m.pageSize - 1) / m.pageSize);
}

void Bindings_ClampPage(BindingsManager& m) {
  int pc = Bindings_PageCount(m);
  if (m.page < 0) m.page = 0;
  if (m.page >= pc) m.page = pc - 1;
  std::vector<int> idx;
  Bindings_Filtered(m, idx);
  if (m.selected < 0) m.selected = 0;
  if (m.selected >= (int)idx.size()) m.selected = std::max(0, (int)idx.size() - 1);
}

std::string Bindings_FormatRule(const BindRule& r) {
  if (r.sources.empty()) return "(unbound)";
  std::string s;
  for (size_t i = 0; i < r.sources.size(); ++i) {
    if (i) s += (r.mode == "all") ? " + " : " | ";
    s += Bindings_SourceLabel(r.sources[i]);
  }
  return s;
}

static const BindActionInfo& LogicalAt(const BindingsManager& m, int logicalIdx) {
  const auto& list = m.logical.empty() ? Bindings_LogicalActions() : m.logical;
  return list[logicalIdx];
}

void Bindings_CycleSourceSlot(BindingsManager& m, int filteredIdx, int slot, int dir) {
  std::vector<int> idx;
  Bindings_Filtered(m, idx);
  if (filteredIdx < 0 || filteredIdx >= (int)idx.size()) return;
  const auto& info = LogicalAt(m, idx[filteredIdx]);
  BindRule& r = m.actions[info.id];
  if (r.set.empty()) {
    if (info.group == "main") r.set = "main";
    else if (info.group == "driving") r.set = "driving";
  }
  const auto& srcs = Bindings_AllSources();
  while ((int)r.sources.size() <= slot) r.sources.push_back(srcs[0]);
  int cur = 0;
  for (int i = 0; i < (int)srcs.size(); ++i)
    if (srcs[i] == r.sources[slot]) { cur = i; break; }
  cur = (cur + dir + (int)srcs.size()) % (int)srcs.size();
  r.sources[slot] = srcs[cur];
  if (r.sources.size() >= 2) r.mode = "all"; // multi-source → chord by default
  else if (r.mode.empty()) r.mode = "any";
  m.dirty = true;
  m.status = info.label + " → " + Bindings_FormatRule(r);
}

void Bindings_CyclePrimarySource(BindingsManager& m, int filteredIdx, int dir) {
  Bindings_CycleSourceSlot(m, filteredIdx, m.editSlot, dir);
}

void Bindings_SetEditSlot(BindingsManager& m, int slot) {
  m.editSlot = (slot <= 0) ? 0 : 1;
}

void Bindings_ToggleMode(BindingsManager& m, int filteredIdx) {
  std::vector<int> idx;
  Bindings_Filtered(m, idx);
  if (filteredIdx < 0 || filteredIdx >= (int)idx.size()) return;
  const auto& info = LogicalAt(m, idx[filteredIdx]);
  BindRule& r = m.actions[info.id];
  r.mode = (r.mode == "all") ? "any" : "all";
  // Chord mode with 1 source: ensure second slot exists for editing
  if (r.mode == "all" && r.sources.size() < 2) {
    const auto& srcs = Bindings_AllSources();
    r.sources.push_back(srcs.size() > 1 ? srcs[1] : srcs[0]);
  }
  m.dirty = true;
  m.status = std::string(info.label) + " mode=" + r.mode +
             (r.mode == "all" ? " (chord: hold both)" : "");
}

void Bindings_ToggleChordSlot(BindingsManager& m, int filteredIdx) {
  std::vector<int> idx;
  Bindings_Filtered(m, idx);
  if (filteredIdx < 0 || filteredIdx >= (int)idx.size()) return;
  const auto& info = LogicalAt(m, idx[filteredIdx]);
  BindRule& r = m.actions[info.id];
  if (r.sources.size() >= 2) {
    r.sources.resize(1);
    r.mode = "any";
    m.editSlot = 0;
    m.status = info.label + " single bind";
  } else {
    const auto& srcs = Bindings_AllSources();
    r.sources.push_back(srcs.size() > 6 ? srcs[6] : srcs[0]);
    r.mode = "all";
    m.editSlot = 1;
    m.status = info.label + " chord: set 2nd button";
  }
  m.dirty = true;
}

void Bindings_ClearAction(BindingsManager& m, int filteredIdx) {
  std::vector<int> idx;
  Bindings_Filtered(m, idx);
  if (filteredIdx < 0 || filteredIdx >= (int)idx.size()) return;
  const auto& info = LogicalAt(m, idx[filteredIdx]);
  m.actions.erase(info.id);
  m.dirty = true;
  m.status = info.label + " unbound";
}
