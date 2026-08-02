#pragma once
#include <string>
#include <vector>
#include <map>

// OpenXR controller remaps — same file as Lua vrmod.bindings:
//   garrysmod/data/vrmod/vrmod_openxr_bindings.json

struct BindRule {
  std::vector<std::string> sources;
  std::string mode = "any"; // "any" | "all"
  std::string set;          // "main" | "driving" | "" (both)
};

struct BindActionInfo {
  std::string id;
  std::string label;
  std::string group; // main | driving | both
};

struct BindingsManager {
  std::string gmodRoot;
  std::string filePath;
  int version = 2;
  std::string preset = "quest3_touch";
  std::map<std::string, BindRule> actions; // action id → rule
  int filter = 0;   // 0=all 1=main 2=driving
  int selected = 0; // index into filtered list
  int page = 0;
  int pageSize = 8;
  std::string status;
  bool dirty = false;
};

// Physical source ids (match module g_ctrlSources / Lua)
const std::vector<std::string>& Bindings_AllSources();
const char* Bindings_SourceLabel(const std::string& id);

// Logical actions (match Lua ListLogicalActions)
const std::vector<BindActionInfo>& Bindings_LogicalActions();

void Bindings_DefaultMap(BindingsManager& m);
bool Bindings_Load(BindingsManager& m, const std::string& gmodRoot);
bool Bindings_Save(BindingsManager& m, std::string& err);
void Bindings_ResetDefaults(BindingsManager& m);
void Bindings_RestoreAction(BindingsManager& m, const std::string& actionId);

void Bindings_Filtered(const BindingsManager& m, std::vector<int>& outLogicalIndices);
int Bindings_PageCount(const BindingsManager& m);
void Bindings_ClampPage(BindingsManager& m);

// Cycle primary source (sources[0]) for action at filtered index.
void Bindings_CyclePrimarySource(BindingsManager& m, int filteredIdx, int dir);
void Bindings_ToggleMode(BindingsManager& m, int filteredIdx);
std::string Bindings_FormatRule(const BindRule& r);
