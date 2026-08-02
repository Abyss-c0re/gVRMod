#include "bindings_mgr.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

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

// Very small extractor for our file shape (not a general JSON parser).
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
  Bindings_DefaultMap(m);
  std::ifstream f(m.filePath);
  if (!f) {
    m.status = "NO FILE — USING DEFAULTS (will save on edit)";
    return true;
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  std::string body = ss.str();
  if (body.empty()) return true;
  // merge user over defaults
  BindingsManager user = m;
  user.actions.clear();
  ParseActions(body, user);
  for (auto& kv : user.actions)
    m.actions[kv.first] = kv.second;
  auto pr = ExtractString(body, 0, "preset");
  if (!pr.empty()) m.preset = pr;
  m.dirty = false;
  m.status = "LOADED " + m.filePath;
  fprintf(stderr, "[cube_webui] bindings loaded %zu actions from %s\n",
          m.actions.size(), m.filePath.c_str());
  return true;
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
  Bindings_DefaultMap(m);
  m.dirty = true;
  m.status = "RESET TO QUEST 3 DEFAULTS (save on leave/start)";
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
  const auto& list = Bindings_LogicalActions();
  for (int i = 0; i < (int)list.size(); ++i) {
    const auto& g = list[i].group;
    if (m.filter == 1 && g == "driving") continue;
    if (m.filter == 2 && g == "main") continue;
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

void Bindings_CyclePrimarySource(BindingsManager& m, int filteredIdx, int dir) {
  std::vector<int> idx;
  Bindings_Filtered(m, idx);
  if (filteredIdx < 0 || filteredIdx >= (int)idx.size()) return;
  const auto& info = Bindings_LogicalActions()[idx[filteredIdx]];
  BindRule& r = m.actions[info.id];
  if (r.set.empty()) {
    if (info.group == "main") r.set = "main";
    else if (info.group == "driving") r.set = "driving";
  }
  const auto& srcs = Bindings_AllSources();
  int cur = 0;
  if (!r.sources.empty()) {
    for (int i = 0; i < (int)srcs.size(); ++i)
      if (srcs[i] == r.sources[0]) { cur = i; break; }
  }
  cur = (cur + dir + (int)srcs.size()) % (int)srcs.size();
  if (r.sources.empty()) r.sources.push_back(srcs[cur]);
  else r.sources[0] = srcs[cur];
  if (r.mode.empty()) r.mode = "any";
  m.dirty = true;
  m.status = info.label + " → " + Bindings_FormatRule(r);
}

void Bindings_ToggleMode(BindingsManager& m, int filteredIdx) {
  std::vector<int> idx;
  Bindings_Filtered(m, idx);
  if (filteredIdx < 0 || filteredIdx >= (int)idx.size()) return;
  const auto& info = Bindings_LogicalActions()[idx[filteredIdx]];
  BindRule& r = m.actions[info.id];
  r.mode = (r.mode == "all") ? "any" : "all";
  m.dirty = true;
  m.status = std::string(info.label) + " mode=" + r.mode;
}
