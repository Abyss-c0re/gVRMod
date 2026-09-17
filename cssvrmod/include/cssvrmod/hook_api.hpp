#pragma once
// LD_PRELOAD hook surface (CSS process).
namespace cssvr {

void HookOnLoad();
void HookOnUnload();
void HookOnSwap(); // called from SDL_GL_SwapWindow / glXSwapBuffers
void HookCallGlxSwap(void* dpy, unsigned long drawable);
void HookCallSdlSwap(void* window);
const char* HookStatus();

} // namespace cssvr
