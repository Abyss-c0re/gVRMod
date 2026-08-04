// gVRLink protocol — public wire layout (versioned).
// PC modules and Quest host both implement these structs.
// NO transport, NO OpenXR, NO Quest app code here.
//
// Law: standalone Quest APK lives outside this tree.
#pragma once
#include <cstdint>

namespace gvlink {

static constexpr uint32_t kMagicVideo = 0x314C5647u; // 'GVL1' LE
static constexpr uint32_t kMagicPose  = 0x31505647u; // 'GVP1' LE
static constexpr uint32_t kMagicCtrl  = 0x31435647u; // 'GVC1' LE
static constexpr uint16_t kProtoVer   = 1;

enum class Space : uint8_t { Stage = 0, Local = 1, View = 2 };
enum class TexFormat : uint8_t { Rgba8 = 0, Bgra8 = 1 };
enum class Eye : uint8_t { Left = 0, Right = 1, Sbs = 2 };
enum class CtrlType : uint8_t {
  Hello = 1, Cap = 2, Start = 3, Stop = 4, TakeXr = 5, Release = 6, Nack = 7, Resync = 8,
};

#pragma pack(push, 1)
struct Hdr {
  uint32_t magic;
  uint16_t version;
  uint16_t type;
  uint32_t seq;
  uint32_t flags;
};

struct VideoHdr {
  Hdr hdr;
  uint16_t w, h;
  uint8_t eye;
  uint8_t format;
  uint16_t reserved;
  uint64_t t_predict_ns;
  uint32_t payload_bytes;
};

struct PoseHdr {
  Hdr hdr;
  uint8_t space;
  uint8_t reserved0;
  uint16_t reserved1;
  uint64_t t_display_ns;
  float hmd_pos[3];
  float hmd_quat[4]; // x,y,z,w
  float eye_l_pos[3];
  float eye_r_pos[3];
  float fov_l[4];
  float fov_r[4];
  float hand_l_pos[3];
  float hand_l_quat[4];
  float hand_r_pos[3];
  float hand_r_quat[4];
  uint32_t buttons;
  float trigger_l, trigger_r;
  float grip_l, grip_r;
  float stick_l[2], stick_r[2];
};

struct CtrlHdr {
  Hdr hdr;
  uint32_t value0;
  uint32_t value1;
};
#pragma pack(pop)

inline bool MagicIsVideo(uint32_t m) { return m == kMagicVideo; }
inline bool MagicIsPose(uint32_t m) { return m == kMagicPose; }
inline bool MagicIsCtrl(uint32_t m) { return m == kMagicCtrl; }
inline bool VersionOk(uint16_t v) { return v == kProtoVer; }

} // namespace gvlink
