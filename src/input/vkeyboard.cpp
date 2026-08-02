#include "vkeyboard.h"
#include "core/vrmod_log.h"

#include <cstdio>
#include <cstring>
#include <algorithm>

struct VKeySlot {
    float x, y, w, h;
    int action;
    char label[VRMOD_VKB_LABEL_LEN];
    char insertLower[8];
    char insertUpper[8];
    bool special;
};

struct VKeyboardSession {
    bool open = false;
    bool upper = false;
    int width = 555;
    int height = 300;
    int headerH = 48;
    char title[VRMOD_VKB_TITLE_LEN] = {};
    char text[VRMOD_VKB_MAX_TEXT] = {};
    int keyCount = 0;
    VKeySlot keys[VRMOD_VKB_MAX_KEYS] = {};
    int lastAction = VKB_ACT_NONE;
    char lastChar[8] = {};
};

static VKeyboardSession g_kb[VRMOD_VKB_MAX_SESSIONS];

// Same control layout as legacy Lua keyboards (chat + shared)
// \1 Del  \2 Done/Enter  \3 Cancel/Close  \4 Shift  \n row
static const char* kLayoutLower =
    "1234567890\1\n"
    "qwertyuiop\n"
    "asdfghjkl\2\n"
    "\3zxcvbnm?\4\3\n"
    " ";
static const char* kLayoutUpper =
    "!@%\"*+=-_:\1\n"
    "QWERTYUIOP\n"
    "ASDFGHJKL\2\n"
    "\3ZXCVBNM/\4\3\n"
    " ";

static void SafeCopy(char* dst, size_t n, const char* src) {
    if (!dst || n == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    std::snprintf(dst, n, "%s", src);
}

static void BuildLayout(VKeyboardSession& s) {
    s.keyCount = 0;
    const char* layout = s.upper ? kLayoutUpper : kLayoutLower;
    const float KEY_W = 45.f;
    const float KEY_H = 42.f;
    const float SPACE_W = 545.f;
    const float ENTER_W = 65.f;
    const float SPEC_W = 48.f;
    const float SPACING = 1.5f;

    float x = SPACING;
    float y = (float)s.headerH + SPACING;
    int rowIndex = 0;
    int closeKeyCount = 0;

    for (const char* p = layout; *p; ++p) {
        char ch = *p;
        if (ch == '\n') {
            y += KEY_H + SPACING;
            rowIndex++;
            if (rowIndex == 1) x = 20.f;
            else if (rowIndex == 2) x = 35.f;
            else if (rowIndex == 3) x = 5.f;
            else if (rowIndex == 4) x = 127.f;
            else x = 5.f;
            continue;
        }
        if (s.keyCount >= VRMOD_VKB_MAX_KEYS) break;

        VKeySlot& k = s.keys[s.keyCount];
        std::memset(&k, 0, sizeof(k));
        k.special = false;

        if (ch == '\1') {
            SafeCopy(k.label, sizeof(k.label), "Del");
            k.action = VKB_ACT_BACKSPACE;
            k.w = KEY_W;
            k.special = true;
        } else if (ch == '\2') {
            SafeCopy(k.label, sizeof(k.label), "Done");
            k.action = VKB_ACT_DONE;
            k.w = ENTER_W;
            k.special = true;
        } else if (ch == '\4') {
            SafeCopy(k.label, sizeof(k.label), "Shift");
            k.action = VKB_ACT_SHIFT;
            k.w = SPEC_W;
            k.special = true;
        } else if (ch == '\3') {
            closeKeyCount++;
            if (closeKeyCount == 1) {
                SafeCopy(k.label, sizeof(k.label), "Cancel");
                k.action = VKB_ACT_CANCEL;
            } else {
                SafeCopy(k.label, sizeof(k.label), "Close");
                k.action = VKB_ACT_CLOSE;
            }
            k.w = SPEC_W;
            k.special = true;
        } else if (ch == ' ') {
            SafeCopy(k.label, sizeof(k.label), "space");
            k.action = VKB_ACT_SPACE;
            SafeCopy(k.insertLower, sizeof(k.insertLower), " ");
            SafeCopy(k.insertUpper, sizeof(k.insertUpper), " ");
            k.w = SPACE_W;
            k.special = true;
        } else {
            char lab[2] = { ch, 0 };
            SafeCopy(k.label, sizeof(k.label), lab);
            k.action = VKB_ACT_CHAR;
            SafeCopy(k.insertLower, sizeof(k.insertLower), lab);
            // upper layout already has uppercase glyphs in string
            SafeCopy(k.insertUpper, sizeof(k.insertUpper), lab);
            k.w = KEY_W;
        }

        k.h = KEY_H;
        if (y + k.h > (float)s.height - 2.f)
            k.h = std::max(18.f, (float)s.height - 2.f - y);
        k.x = x;
        k.y = y;

        x += k.w + SPACING;
        s.keyCount++;
    }
}

bool VKB_IsSupported() {
    return true;
}

bool VKB_SystemKeyboardAvailable() {
    // Future: XR_META_virtual_keyboard / runtime soft keyboard
    return false;
}

int VKB_Open(const char* title, const char* initialText,
             int width, int height, int slotHint,
             char* errMsg, size_t errMsgLen) {
    int slot = -1;
    if (slotHint >= 1 && slotHint <= VRMOD_VKB_MAX_SESSIONS) {
        slot = slotHint - 1;
    } else {
        for (int i = 0; i < VRMOD_VKB_MAX_SESSIONS; ++i) {
            if (!g_kb[i].open) {
                slot = i;
                break;
            }
        }
        // Prefer reuse of closed slots; if all open, replace last
        if (slot < 0) slot = VRMOD_VKB_MAX_SESSIONS - 1;
    }

    VKeyboardSession& s = g_kb[slot];
    s = VKeyboardSession{};
    s.open = true;
    s.upper = false;
    s.width = width > 64 ? width : 555;
    s.height = height > 64 ? height : 300;
    s.headerH = 48;
    SafeCopy(s.title, sizeof(s.title), title && title[0] ? title : "KEYBOARD");
    SafeCopy(s.text, sizeof(s.text), initialText ? initialText : "");
    BuildLayout(s);

    VRMOD_LOG_INFO("VKeyboard open id=%d \"%s\" %dx%d keys=%d",
                   slot + 1, s.title, s.width, s.height, s.keyCount);
    (void)errMsg;
    (void)errMsgLen;
    return slot + 1;
}

void VKB_Close(int id) {
    if (id < 1 || id > VRMOD_VKB_MAX_SESSIONS) return;
    if (!g_kb[id - 1].open) return;
    VRMOD_LOG_INFO("VKeyboard close id=%d", id);
    g_kb[id - 1] = VKeyboardSession{};
}

void VKB_Shutdown() {
    for (int i = 0; i < VRMOD_VKB_MAX_SESSIONS; ++i)
        g_kb[i] = VKeyboardSession{};
    VRMOD_LOG_INFO("VKeyboard shutdown");
}

bool VKB_IsOpen(int id) {
    if (id < 1 || id > VRMOD_VKB_MAX_SESSIONS) return false;
    return g_kb[id - 1].open;
}

int VKB_FirstOpen() {
    for (int i = 0; i < VRMOD_VKB_MAX_SESSIONS; ++i)
        if (g_kb[i].open) return i + 1;
    return 0;
}

bool VKB_GetInfo(int id, VKeyboardInfo* out) {
    if (!out || id < 1 || id > VRMOD_VKB_MAX_SESSIONS || !g_kb[id - 1].open)
        return false;
    const VKeyboardSession& s = g_kb[id - 1];
    out->id = id;
    out->open = true;
    out->upper = s.upper;
    out->width = s.width;
    out->height = s.height;
    out->headerH = s.headerH;
    out->keyCount = s.keyCount;
    SafeCopy(out->title, sizeof(out->title), s.title);
    SafeCopy(out->text, sizeof(out->text), s.text);
    out->lastAction = s.lastAction;
    SafeCopy(out->lastChar, sizeof(out->lastChar), s.lastChar);
    return true;
}

const char* VKB_GetText(int id) {
    if (id < 1 || id > VRMOD_VKB_MAX_SESSIONS || !g_kb[id - 1].open)
        return "";
    return g_kb[id - 1].text;
}

bool VKB_SetText(int id, const char* text) {
    if (id < 1 || id > VRMOD_VKB_MAX_SESSIONS || !g_kb[id - 1].open)
        return false;
    SafeCopy(g_kb[id - 1].text, sizeof(g_kb[id - 1].text), text ? text : "");
    return true;
}

bool VKB_Append(int id, const char* utf8OrAscii) {
    if (id < 1 || id > VRMOD_VKB_MAX_SESSIONS || !g_kb[id - 1].open)
        return false;
    if (!utf8OrAscii || !utf8OrAscii[0]) return true;
    char* t = g_kb[id - 1].text;
    size_t cur = std::strlen(t);
    size_t add = std::strlen(utf8OrAscii);
    if (cur + add >= VRMOD_VKB_MAX_TEXT)
        add = VRMOD_VKB_MAX_TEXT - 1 - cur;
    if (add == 0) return false;
    std::memcpy(t + cur, utf8OrAscii, add);
    t[cur + add] = '\0';
    return true;
}

bool VKB_Backspace(int id) {
    if (id < 1 || id > VRMOD_VKB_MAX_SESSIONS || !g_kb[id - 1].open)
        return false;
    char* t = g_kb[id - 1].text;
    size_t n = std::strlen(t);
    if (n == 0) return true;
    // Simple ASCII/back one byte (GMod chat mostly ASCII; UTF-8 multi-byte later)
    t[n - 1] = '\0';
    return true;
}

bool VKB_GetShift(int id) {
    if (id < 1 || id > VRMOD_VKB_MAX_SESSIONS || !g_kb[id - 1].open)
        return false;
    return g_kb[id - 1].upper;
}

bool VKB_SetShift(int id, bool upper) {
    if (id < 1 || id > VRMOD_VKB_MAX_SESSIONS || !g_kb[id - 1].open)
        return false;
    g_kb[id - 1].upper = upper;
    BuildLayout(g_kb[id - 1]);
    return true;
}

bool VKB_SetTitle(int id, const char* title) {
    if (id < 1 || id > VRMOD_VKB_MAX_SESSIONS || !g_kb[id - 1].open)
        return false;
    SafeCopy(g_kb[id - 1].title, sizeof(g_kb[id - 1].title),
             title && title[0] ? title : "KEYBOARD");
    return true;
}

void VKB_RebuildLayout(int id) {
    if (id < 1 || id > VRMOD_VKB_MAX_SESSIONS || !g_kb[id - 1].open)
        return;
    BuildLayout(g_kb[id - 1]);
}

int VKB_HitTest(int id, float px, float py) {
    if (id < 1 || id > VRMOD_VKB_MAX_SESSIONS || !g_kb[id - 1].open)
        return -1;
    const VKeyboardSession& s = g_kb[id - 1];
    for (int i = 0; i < s.keyCount; ++i) {
        const VKeySlot& k = s.keys[i];
        if (px >= k.x && px < k.x + k.w && py >= k.y && py < k.y + k.h)
            return i;
    }
    return -1;
}

int VKB_PointerClick(int id, float px, float py) {
    if (id < 1 || id > VRMOD_VKB_MAX_SESSIONS || !g_kb[id - 1].open)
        return VKB_ACT_NONE;
    VKeyboardSession& s = g_kb[id - 1];
    s.lastAction = VKB_ACT_NONE;
    s.lastChar[0] = '\0';

    int idx = VKB_HitTest(id, px, py);
    if (idx < 0) return VKB_ACT_NONE;

    const VKeySlot& k = s.keys[idx];
    s.lastAction = k.action;

    switch (k.action) {
    case VKB_ACT_BACKSPACE:
        VKB_Backspace(id);
        break;
    case VKB_ACT_SHIFT:
        s.upper = !s.upper;
        BuildLayout(s);
        break;
    case VKB_ACT_SPACE:
        SafeCopy(s.lastChar, sizeof(s.lastChar), " ");
        VKB_Append(id, " ");
        break;
    case VKB_ACT_CHAR: {
        const char* ins = s.upper ? k.insertUpper : k.insertLower;
        // After rebuild on shift, insert strings match label
        SafeCopy(s.lastChar, sizeof(s.lastChar),
                 (ins && ins[0]) ? ins : k.label);
        VKB_Append(id, s.lastChar);
        break;
    }
    case VKB_ACT_DONE:
    case VKB_ACT_CANCEL:
    case VKB_ACT_CLOSE:
        // Lua closes / commits
        break;
    default:
        break;
    }
    return s.lastAction;
}

int VKB_GetKeyCount(int id) {
    if (id < 1 || id > VRMOD_VKB_MAX_SESSIONS || !g_kb[id - 1].open)
        return 0;
    return g_kb[id - 1].keyCount;
}

bool VKB_GetKey(int id, int index, VKeyInfo* out) {
    if (!out || id < 1 || id > VRMOD_VKB_MAX_SESSIONS || !g_kb[id - 1].open)
        return false;
    const VKeyboardSession& s = g_kb[id - 1];
    if (index < 0 || index >= s.keyCount) return false;
    const VKeySlot& k = s.keys[index];
    out->x = k.x;
    out->y = k.y;
    out->w = k.w;
    out->h = k.h;
    out->action = k.action;
    out->special = k.special;
    SafeCopy(out->label, sizeof(out->label), k.label);
    return true;
}
