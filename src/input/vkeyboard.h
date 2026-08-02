#pragma once
// =============================================================================
// VR Keyboard driver — module-owned text buffer + QWERTY layout + hit-test
//
// Reusable between OpenXR launcher, in-game chat, actions panel, map browser.
// Software driver always available; system/OpenXR soft-keyboard is optional later.
//
// Lua paints the surface (panel2vr / VRUtilMenu); module owns truth for:
//   text buffer, shift state, key geometry, pointer → key actions.
// =============================================================================

#include "core/vrmod_common.h"
#include <cstdint>
#include <cstddef>

#define VRMOD_VKB_MAX_SESSIONS 4
#define VRMOD_VKB_MAX_TEXT     512
#define VRMOD_VKB_MAX_KEYS     96
#define VRMOD_VKB_TITLE_LEN    64
#define VRMOD_VKB_LABEL_LEN    16

// Action codes returned by PointerClick / stored on keys
enum VKeyAction {
    VKB_ACT_NONE      = 0,
    VKB_ACT_CHAR      = 1,
    VKB_ACT_BACKSPACE = 2,
    VKB_ACT_DONE      = 3,
    VKB_ACT_CANCEL    = 4,
    VKB_ACT_SHIFT     = 5,
    VKB_ACT_CLOSE     = 6, // alias of cancel for dual Exit/Close keys
    VKB_ACT_SPACE     = 7,
};

struct VKeyInfo {
    float x, y, w, h;          // panel pixel coords
    int action;                // VKeyAction
    char label[VRMOD_VKB_LABEL_LEN];
    bool special;
};

struct VKeyboardInfo {
    int id;                    // 1-based; 0 invalid
    bool open;
    bool upper;
    int width;
    int height;
    int headerH;
    int keyCount;
    char title[VRMOD_VKB_TITLE_LEN];
    char text[VRMOD_VKB_MAX_TEXT];
    int lastAction;            // last PointerClick result
    char lastChar[8];          // for CHAR/SPACE
};

// Software driver always true. System keyboard path reserved.
bool VKB_IsSupported();
bool VKB_SystemKeyboardAvailable(); // false until XR soft-KB wired

// Open session. slotHint 0 = free slot, or 1..MAX to reuse.
// Returns id (>0) or 0. Default panel 555×300.
int VKB_Open(const char* title, const char* initialText,
             int width, int height, int slotHint,
             char* errMsg, size_t errMsgLen);

void VKB_Close(int id);
void VKB_Shutdown(); // all sessions

bool VKB_IsOpen(int id);
int  VKB_FirstOpen(); // first open id or 0

bool VKB_GetInfo(int id, VKeyboardInfo* out);

const char* VKB_GetText(int id);
bool VKB_SetText(int id, const char* text);
bool VKB_Append(int id, const char* utf8OrAscii);
bool VKB_Backspace(int id);
bool VKB_GetShift(int id);
bool VKB_SetShift(int id, bool upper);
bool VKB_SetTitle(int id, const char* title);

// Rebuild key rects for current shift / size (call after SetShift if needed).
void VKB_RebuildLayout(int id);

// Hit-test only (key index 0..n-1 or -1).
int VKB_HitTest(int id, float px, float py);

// Hit-test + apply (mutates buffer for char/space/backspace/shift).
// Returns VKeyAction. Fills lastAction / lastChar on session.
int VKB_PointerClick(int id, float px, float py);

int  VKB_GetKeyCount(int id);
bool VKB_GetKey(int id, int index, VKeyInfo* out);
