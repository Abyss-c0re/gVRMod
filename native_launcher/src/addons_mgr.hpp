#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>

// GMod workshop/local addon manager (addonnomount.txt + local .disabled)

struct AddonEntry {
  std::string id;       // workshop id or "local:<folder>"
  std::string title;
  std::string kind;     // "workshop" | "local"
  bool enabled = true;
  std::string dirPath;  // absolute folder for previews
  // Tiny RGBA thumb (optional). w/h 0 = none.
  int thumbW = 0, thumbH = 0;
  std::vector<unsigned char> thumbRgba; // thumbW*thumbH*4
};

struct AddonManager {
  std::string gmodRoot;
  std::string workshopRoot;
  std::string thumbCache; // ~/.cache/gvrmod/thumbs
  std::vector<AddonEntry> addons;
  std::unordered_set<std::string> nomount;
  int selected = 0;
  int page = 0;
  int pageSize = 8; // product grid: 8 rows per page
  int filter = 0;   // 0=all 1=on 2=off 3=workshop 4=local
  std::string status;
};

std::string FindWorkshopContent4000(const std::string& gmodRoot);
void Addons_Load(AddonManager& m, const std::string& gmodRoot);

// Filtered index list (into m.addons)
void Addons_FilteredIndices(const AddonManager& m, std::vector<int>& out);
int Addons_PageCount(const AddonManager& m);
void Addons_ClampPage(AddonManager& m);

bool Addons_ToggleSelected(AddonManager& m, std::string& err);
bool Addons_ToggleIndex(AddonManager& m, int absIndex, std::string& err);
bool Addons_WriteNomount(const AddonManager& m, std::string& err);

int Addons_EnabledCount(const AddonManager& m);
int Addons_DisabledCount(const AddonManager& m);

// Load local preview files + optional Steam CDN fetch for visible page.
// safe to call each frame; rate-limited fetch (one request per call max).
void Addons_EnsureThumbsForPage(AddonManager& m);
