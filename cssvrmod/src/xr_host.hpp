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
// Submit current GL backbuffer (or tex) to both eyes. Mono capture until dual RenderView.
bool XrHostSubmitBackbuffer(unsigned int gl_tex, int src_w, int src_h, bool vflip);
void XrHostEndFrame();
bool XrHostPollInput(struct XrSample* out);
const XrHostInfo& XrHostStatus();

} // namespace cssvr
