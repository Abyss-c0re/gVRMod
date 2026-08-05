#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>
#include <atomic>
#include <mutex>
#include <thread>
#include <deque>
#include <condition_variable>

// GMod workshop/local addon manager (addonnomount + Steam metadata cache)

struct AddonEntry {
  std::string id;       // workshop id or "local:<folder>"
  std::string title;
  std::string kind;     // "workshop" | "local"
  bool enabled = true;
  std::string dirPath;
  int thumbW = 0, thumbH = 0;
  std::vector<unsigned char> thumbRgba;
  bool metaPending = false;  // fetch in flight or needed
  bool metaFailed = false;   // permanent fail this session (no requeue thrash)
  bool metaQueued = false;   // currently in worker queue
};

struct AddonManager {
  std::string gmodRoot;
  std::string workshopRoot;
  std::string thumbCache;
  std::vector<AddonEntry> addons;
  std::unordered_set<std::string> nomount;
  int selected = 0;
  int page = 0;
  int pageSize = 8;
  int filter = 0; // 0=all 1=on 2=off 3=workshop 4=local
  std::string status;

  // Async Steam metadata (titles + previews) — owned by manager lifetime
  std::mutex jobMu;
  std::condition_variable jobCv;
  std::deque<std::string> pendingIds;
  std::unordered_set<std::string> inFlight;
  std::vector<std::thread> workers;
  std::atomic<bool> stopWorkers{false};
  int maxWorkers = 4;
  int doneThisSession = 0;
};

std::string FindWorkshopContent4000(const std::string& gmodRoot);
void Addons_Load(AddonManager& m, const std::string& gmodRoot);
void Addons_Shutdown(AddonManager& m);

void Addons_FilteredIndices(const AddonManager& m, std::vector<int>& out);
int Addons_PageCount(const AddonManager& m);
void Addons_ClampPage(AddonManager& m);

bool Addons_ToggleSelected(AddonManager& m, std::string& err);
bool Addons_ToggleIndex(AddonManager& m, int absIndex, std::string& err);
// Bulk enable/disable for current filter list (workshop nomount + local rename).
bool Addons_SetAllFiltered(AddonManager& m, bool enable, std::string& err);
bool Addons_WriteNomount(const AddonManager& m, std::string& err);

int Addons_EnabledCount(const AddonManager& m);
int Addons_DisabledCount(const AddonManager& m);

// Non-blocking: queue missing meta/thumbs for current page; apply finished jobs.
// Call once per frame from UI thread. Returns true if titles/thumbs/status changed
// (caller should CubeUI_MarkDirty).
bool Addons_PumpAsync(AddonManager& m);
