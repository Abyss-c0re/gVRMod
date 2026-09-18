#pragma once
// Slim OpenXR host for the CSS hook. Game paints; we submit. No GMod Lua.
#include <cstdint>

namespace cssvr {

struct XrHostInfo {
  bool loader = false;
  bool instance = false;
  bool session = false;
  bool swapchain = false;
  uint32_t width = 0;
  uint32_t height = 0;
  const char* reason = "idle";
};

bool XrHostInit();
void XrHostShutdown();
bool XrHostBeginFrame();
// Submit the same CSS frame to both eyes with a slight IPD offset (gmod synthetic).
bool XrHostSubmitBackbuffer(unsigned int gl_tex, int src_w, int src_h, bool vflip);
// Upload CPU 8-bit pixels (top-left origin). bgra=true skips a CPU swizzle from DXVK.
bool XrHostSubmitRgba(const unsigned char* rgba, int w, int h);
bool XrHostSubmitPixels(const unsigned char* px, int w, int h, bool bgra);
void XrHostEndFrame();
bool XrHostPollInput(struct XrSample* out);
const XrHostInfo& XrHostStatus();

} // namespace cssvr
