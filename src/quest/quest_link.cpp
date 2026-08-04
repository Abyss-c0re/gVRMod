#include "quest/quest_link.h"

#include "core/vrmod_log.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef _WIN32
#include <time.h>
#endif

namespace questlink {

static uint64_t NowNs() {
#ifdef _WIN32
  return 0;
#else
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

Link::Link() = default;
Link::~Link() { Stop(); }

bool Link::Start(uint16_t pose_listen_port) {
  Stop();
  sock_ = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_ < 0) {
    VRMOD_LOG_ERROR("quest link socket: %s", strerror(errno));
    return false;
  }
  int yes = 1;
  setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(pose_listen_port);
  if (bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    VRMOD_LOG_ERROR("quest link bind %u: %s", pose_listen_port, strerror(errno));
    close(sock_);
    sock_ = -1;
    return false;
  }
  timeval tv{};
  tv.tv_usec = 50000;
  setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  run_ = true;
  thr_ = std::thread([this] { RxLoop(); });
  VRMOD_LOG_INFO("quest link listening UDP %u for GVP1 poses", pose_listen_port);
  return true;
}

void Link::Stop() {
  run_ = false;
  if (thr_.joinable()) thr_.join();
  if (sock_ >= 0) {
    close(sock_);
    sock_ = -1;
  }
}

void Link::SetQuestHost(const std::string& host, uint16_t video_port) {
  std::lock_guard<std::mutex> lk(mu_);
  quest_host_ = host;
  video_port_ = video_port;
  VRMOD_LOG_INFO("quest host video peer %s:%u", host.c_str(), video_port);
}

bool Link::GetPose(PoseSample& out) const {
  std::lock_guard<std::mutex> lk(mu_);
  out = last_;
  return out.valid;
}

bool Link::HasRecentPose(double max_age_sec) const {
  PoseSample p;
  if (!GetPose(p) || !p.valid) return false;
  uint64_t now = NowNs();
  if (p.recv_ns == 0 || now < p.recv_ns) return true;
  double age = (now - p.recv_ns) / 1e9;
  return age <= max_age_sec;
}

void Link::RxLoop() {
  uint8_t buf[2048];
  while (run_) {
    sockaddr_storage from{};
    socklen_t fromlen = sizeof(from);
    ssize_t n = recvfrom(sock_, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromlen);
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
      if (run_) VRMOD_LOG_ERROR("quest link recv: %s", strerror(errno));
      break;
    }
    if (n < (ssize_t)sizeof(gvlink::Hdr)) continue;
    const auto* hdr = reinterpret_cast<const gvlink::Hdr*>(buf);
    if (!gvlink::MagicIsPose(hdr->magic) || !gvlink::VersionOk(hdr->version)) continue;
    if (n < (ssize_t)sizeof(gvlink::PoseHdr)) continue;

    PoseSample s;
    memcpy(&s.hdr, buf, sizeof(s.hdr));
    s.valid = true;
    s.recv_ns = NowNs();
    {
      std::lock_guard<std::mutex> lk(mu_);
      last_ = s;
      if (from.ss_family == AF_INET && quest_host_.empty()) {
        char ip[64]{};
        inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(&from)->sin_addr, ip, sizeof(ip));
        quest_host_ = ip;
      }
    }
    pose_count_++;
  }
}

void PosOpenXrToSource(const float oxr[3], float src[3]) {
  // OpenXR: x=right, y=up, z=back → Source: x=forward, y=left, z=up
  src[0] = -oxr[2];
  src[1] = -oxr[0];
  src[2] = oxr[1];
}

void QuatToSourceAng(const float q[4], float ang[3]) {
  // Minimal quat (x,y,z,w) → Source pitch/yaw/roll degrees (same as XR path intent)
  float x = q[0], y = q[1], z = q[2], w = q[3];
  // Convert OpenXR quat to Source axes by swapping basis on matrix
  // Use standard XYZ euler after axis remap of forward
  float sinr = 2.f * (w * x + y * z);
  float cosr = 1.f - 2.f * (x * x + y * y);
  float roll = std::atan2(sinr, cosr);
  float sinp = 2.f * (w * y - z * x);
  float pitch;
  if (std::fabs(sinp) >= 1.f)
    pitch = std::copysign(3.14159265f / 2.f, sinp);
  else
    pitch = std::asin(sinp);
  float siny = 2.f * (w * z + x * y);
  float cosy = 1.f - 2.f * (y * y + z * z);
  float yaw = std::atan2(siny, cosy);
  // Remap roughly to Source after OpenXR→Source rotation convention used elsewhere
  const float r2d = 180.f / 3.14159265f;
  ang[0] = pitch * r2d;
  ang[1] = -yaw * r2d;
  ang[2] = -roll * r2d;
}

} // namespace questlink
