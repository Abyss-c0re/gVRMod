#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>

// Reversed WebUI Addons manager (control.Addons.js + addonnomount.txt)
// Mount state for workshop = NOT listed in cfg/addonnomount.txt

struct AddonEntry {
  std::string id;       // workshop id or "local:<folder>"
  std::string title;
  std::string kind;     // "workshop" | "local"
  bool enabled = true;  // mounted
  uint64_t sizeHint = 0;
};

struct AddonManager {
  std::string gmodRoot;
  std::string workshopRoot; // .../steamapps/workshop/content/4000
  std::vector<AddonEntry> addons;
  std::unordered_set<std::string> nomount; // workshop ids disabled
  int selected = 0;
  int scroll = 0;
  std::string filter; // optional search substring
  std::string status;
};

// Resolve workshop content dir from GMod path
std::string FindWorkshopContent4000(const std::string& gmodRoot);

// Load nomount set + scan local + workshop dirs
void Addons_Load(AddonManager& m, const std::string& gmodRoot);

// Toggle mount for selected entry; writes addonnomount.txt for workshop.
// Local folders: rename to name.disabled (GMod ignores) or restore.
bool Addons_ToggleSelected(AddonManager& m, std::string& err);

// Persist nomount VDF
bool Addons_WriteNomount(const AddonManager& m, std::string& err);

// Count helpers
int Addons_EnabledCount(const AddonManager& m);
int Addons_DisabledCount(const AddonManager& m);
