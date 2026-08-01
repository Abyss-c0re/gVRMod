#ifdef _WIN32

#include "d3d_hooks.h"
#include "core/vrmod_log.h"

#include <cstring>
#include <cstdio>

// ── State (mirrors vrmod-module-master main Windows path) ──
char                 g_createTextureOrigBytes[14] = {0};
typedef HRESULT (APIENTRY* CreateTextureFn)(
    IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
    IDirect3DTexture9**, HANDLE*);
static CreateTextureFn g_createTexture = nullptr;

HANDLE               g_sharedTextureHandle = nullptr;
ID3D11Device*        g_d3d11Device = nullptr;
ID3D11DeviceContext* g_d3d11Context = nullptr;
ID3D11Texture2D*     g_d3d11SharedTexture = nullptr;
IDirect3DDevice9*    g_pD3D9Device = nullptr;

int                  g_knownSubmitSrcW = 0;
int                  g_knownSubmitSrcH = 0;
// Flip = 0 on Windows (D3D top-left == OpenXR view origin).
bool                 g_rtTextureNeedsVFlip = false;

unsigned int g_captureTexture = 0;
unsigned int g_sharedTexture = 0;
unsigned int g_leftEyeTexture = 0;
unsigned int g_rightEyeTexture = 0;
unsigned int g_leftEyeFBO = 0;
unsigned int g_leftEyeColorTex = 0;
unsigned int g_rightEyeFBO = 0;
unsigned int g_rightEyeColorTex = 0;
unsigned int g_vrRtFBO = 0;
unsigned int g_vrRtColorTex = 0;

static bool g_patchArmed = false;

void VRMOD_SetKnownSubmitSize(uint32_t w, uint32_t h) {
    if (w > 0 && h > 0) {
        g_knownSubmitSrcW = (int)w;
        g_knownSubmitSrcH = (int)h;
    }
}

void VRMOD_MarkSharedTextureEngineOwned() {}

typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);

// From vrmod-module-master main: force shared handle on the next CreateTexture.
HRESULT APIENTRY CreateTextureHook(
    IDirect3DDevice9* pDevice, UINT w, UINT h, UINT levels, DWORD usage,
    D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** tex, HANDLE* shared_handle)
{
    // Restore original bytes first (one-shot hook), same as master.
    WriteProcessMemory(GetCurrentProcess(), (LPVOID)g_createTexture,
                       g_createTextureOrigBytes, 14, nullptr);
    g_patchArmed = false;

    if (g_sharedTextureHandle == nullptr) {
        shared_handle = &g_sharedTextureHandle;
        pool = D3DPOOL_DEFAULT;
    }
    return g_createTexture(pDevice, w, h, levels, usage, format, pool, tex, shared_handle);
}

static void BuildHookPatch(void* hookFn, uint8_t outPatch[HOOK_SIZE]) {
    uint64_t addr = (uint64_t)(uintptr_t)hookFn;
    uint32_t low = (uint32_t)(addr & 0xffffffffu);
    uint32_t high = (uint32_t)(addr >> 32);
    // push imm32; mov [rsp+4], imm32; ret  — same 14-byte trampoline as master/Linux
    outPatch[0] = 0x68;
    memcpy(outPatch + 1, &low, 4);
    outPatch[5] = 0xC7;
    outPatch[6] = 0x44;
    outPatch[7] = 0x24;
    outPatch[8] = 0x04;
    memcpy(outPatch + 9, &high, 4);
    outPatch[13] = 0xC3;
}

bool D3D_InitDeviceHooks(char* errMsg, int errMsgLen) {
    HMODULE hMod = GetModuleHandleA("shaderapidx9.dll");
    if (!hMod) {
        snprintf(errMsg, errMsgLen, "VRMOD: Missing shaderapidx9.dll");
        return false;
    }
    CreateInterfaceFn CreateInterface =
        (CreateInterfaceFn)GetProcAddress(hMod, "CreateInterface");
    if (!CreateInterface) {
        snprintf(errMsg, errMsgLen, "VRMOD: Missing CreateInterface");
        return false;
    }

#ifdef _WIN64
    // Master main (win64): ShaderDevice001 vtable[5] + RIP-relative load of device ptr.
    DWORD_PTR fnAddr = ((DWORD_PTR**)CreateInterface("ShaderDevice001", nullptr))[0][5];
    g_pD3D9Device = *(IDirect3DDevice9**)(fnAddr + 8 + (*(DWORD_PTR*)(fnAddr + 3) & 0xFFFFFFFF));
#else
    g_pD3D9Device =
        **(IDirect3DDevice9***)(((DWORD_PTR**)CreateInterface("ShaderDevice001", nullptr))[0][5] + 2);
#endif
    if (!g_pD3D9Device) {
        snprintf(errMsg, errMsgLen, "VRMOD: Failed to resolve IDirect3DDevice9");
        return false;
    }

    g_createTexture = ((CreateTextureFn**)g_pD3D9Device)[0][23];
    if (!g_createTexture) {
        snprintf(errMsg, errMsgLen, "VRMOD: CreateTexture vtable[23] null");
        return false;
    }

    g_rtTextureNeedsVFlip = false; // flip=0 on Windows
    VRMOD_LOG_INFO("D3D9 device + CreateTexture hook target ready (Windows, flip=0)");
    return true;
}

bool RemoveTexturePatch(ErrorFunc errFunc) {
    if (!g_patchArmed || !g_createTexture) {
        g_patchArmed = false;
        return true;
    }
    if (!WriteProcessMemory(GetCurrentProcess(), (LPVOID)g_createTexture,
                            g_createTextureOrigBytes, 14, nullptr)) {
        if (errFunc) errFunc("VRMOD: WriteProcessMemory unpatch failed");
        return false;
    }
    g_patchArmed = false;
    return true;
}

int ShareTextureBegin(uint32_t eyeWidth, uint32_t eyeHeight, ErrorFunc errFunc) {
    if (!g_createTexture) {
        if (errFunc) errFunc("VRMOD: CreateTexture not resolved (call Init first)");
        return -1;
    }

    // Record full SBS size for submit (Lua RT is eyeW*2 × eyeH).
    if (eyeWidth > 0 && eyeHeight > 0) {
        VRMOD_SetKnownSubmitSize(eyeWidth * 2, eyeHeight);
    }

    // Reset previous share (do not Release D3D11 shared until Finish re-opens).
    g_sharedTextureHandle = nullptr;

    uint8_t patch[HOOK_SIZE];
    BuildHookPatch((void*)(uintptr_t)CreateTextureHook, patch);

    if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)g_createTexture,
                           g_createTextureOrigBytes, 14, nullptr)) {
        if (errFunc) errFunc("VRMOD: ReadProcessMemory failed");
        return -1;
    }
    if (!WriteProcessMemory(GetCurrentProcess(), (LPVOID)g_createTexture, patch, 14, nullptr)) {
        if (errFunc) errFunc("VRMOD: WriteProcessMemory failed");
        return -1;
    }
    g_patchArmed = true;
    VRMOD_LOG_INFO("ShareTextureBegin armed (D3D9 CreateTexture hook, SBS %dx%d, flip=0)",
                   g_knownSubmitSrcW, g_knownSubmitSrcH);
    return 0;
}

bool ShareTextureFinish(ErrorFunc errFunc) {
    RemoveTexturePatch(errFunc);

    if (!g_sharedTextureHandle) {
        if (errFunc) errFunc("VRMOD: g_sharedTextureHandle is null (CreateTexture hook miss)");
        return false;
    }

    // Master: create a hardware D3D11 device and open the NT shared handle.
    if (g_d3d11SharedTexture) {
        g_d3d11SharedTexture->Release();
        g_d3d11SharedTexture = nullptr;
    }
    if (g_d3d11Context) {
        g_d3d11Context->Release();
        g_d3d11Context = nullptr;
    }
    if (g_d3d11Device) {
        g_d3d11Device->Release();
        g_d3d11Device = nullptr;
    }

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &g_d3d11Device, &featureLevel, &g_d3d11Context);
    if (FAILED(hr) || !g_d3d11Device) {
        if (errFunc) errFunc("VRMOD: D3D11CreateDevice failed");
        return false;
    }

    ID3D11Resource* res = nullptr;
    hr = g_d3d11Device->OpenSharedResource(
        g_sharedTextureHandle, __uuidof(ID3D11Resource), (void**)&res);
    if (FAILED(hr) || !res) {
        if (errFunc) errFunc("VRMOD: OpenSharedResource failed");
        return false;
    }

    hr = res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&g_d3d11SharedTexture);
    res->Release();
    if (FAILED(hr) || !g_d3d11SharedTexture) {
        if (errFunc) errFunc("VRMOD: QueryInterface ID3D11Texture2D failed");
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    g_d3d11SharedTexture->GetDesc(&desc);
    if (desc.Width > 0 && desc.Height > 0) {
        VRMOD_SetKnownSubmitSize(desc.Width, desc.Height);
    }

    // Mark legacy aliases so submit path thinks we have a usable SBS source.
    g_sharedTexture = 1;
    g_vrRtColorTex = 1;

    VRMOD_LOG_INFO("ShareTextureFinish: D3D11 shared RT %ux%u fmt=%u (flip=0)",
                   desc.Width, desc.Height, (unsigned)desc.Format);
    return true;
}

int ShareCaptureTextureBegin(uint32_t, uint32_t, ErrorFunc) {
    // Windows uses the primary shared RT; no separate capture path.
    return 0;
}

bool ShareCaptureTextureFinish(ErrorFunc) {
    return true;
}

void D3D_ShutdownShare() {
    RemoveTexturePatch(nullptr);
    if (g_d3d11SharedTexture) {
        g_d3d11SharedTexture->Release();
        g_d3d11SharedTexture = nullptr;
    }
    if (g_d3d11Context) {
        g_d3d11Context->Release();
        g_d3d11Context = nullptr;
    }
    if (g_d3d11Device) {
        g_d3d11Device->Release();
        g_d3d11Device = nullptr;
    }
    g_sharedTextureHandle = nullptr;
    g_pD3D9Device = nullptr;
    g_sharedTexture = 0;
    g_vrRtColorTex = 0;
    g_createTexture = nullptr;
}

#endif // _WIN32
