// PC-side gVRLink transport for quest thin module.
#pragma once

#include "gvlink_proto.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace questlink {

static constexpr uint16_t kDefaultPosePort  = 27101; // Quest → PC
static constexpr uint16_t kDefaultVideoPort = 27100; // PC → Quest

struct PoseSample {
  gvlink::PoseHdr hdr{};
  bool valid = false;
  uint64_t recv_ns = 0;
};

class Link {
public:
  Link();
  ~Link();

  // Listen for GVP1 poses; optional fixed Quest host for future video TX.
  bool Start(uint16_t pose_listen_port = kDefaultPosePort);
  void Stop();

  void SetQuestHost(const std::string& host, uint16_t video_port = kDefaultVideoPort);

  // Latest pose (thread-safe copy).
  bool GetPose(PoseSample& out) const;
  bool HasRecentPose(double max_age_sec = 1.0) const;

  uint32_t PoseCount() const { return pose_count_.load(); }

private:
  void RxLoop();

  int sock_ = -1;
  std::thread thr_;
  std::atomic<bool> run_{false};

  mutable std::mutex mu_;
  PoseSample last_{};
  std::string quest_host_;
  uint16_t video_port_ = kDefaultVideoPort;
  std::atomic<uint32_t> pose_count_{0};
};

// OpenXR meters → Source axes (same as ConvertXrPose, no scale).
void QuatToSourceAng(const float q[4], float ang[3]);
void PosOpenXrToSource(const float oxr[3], float src[3]);

} // namespace questlink
