#pragma once
// LD_PRELOAD hook surface (CSS process).
namespace cssvr {

void HookOnLoad();
void HookOnUnload();
void HookOnSwap(); // called from SDL_GL_SwapWindow / glXSwapBuffers
const char* HookStatus();

} // namespace cssvr
