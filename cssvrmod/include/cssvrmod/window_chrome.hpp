#pragma once
// Desktop CSS window must stay decorated unless the user opts into --noborder.
// SDL2 flag values match SDL_video.h.
#include <cstdint>

namespace cssvr {

constexpr uint32_t kSdlWindowFullscreen = 0x00000001u;
constexpr uint32_t kSdlWindowBorderless = 0x00000010u;
constexpr uint32_t kSdlWindowResizable = 0x00000020u;
constexpr uint32_t kSdlWindowFullscreenDesktopBit = 0x00001000u;

inline uint32_t SanitizeSdlWindowFlags(uint32_t flags, bool allow_noborder) {
  if (allow_noborder) return flags;
  flags &= ~kSdlWindowBorderless;
  flags &= ~kSdlWindowFullscreen;
  flags &= ~kSdlWindowFullscreenDesktopBit;
  flags |= kSdlWindowResizable;
  return flags;
}

} // namespace cssvr
