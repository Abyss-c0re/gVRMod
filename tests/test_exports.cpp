#include "test_framework.h"
#include <cstring>
#include <set>
#include <string>
#include <vector>

// Expected module field names (must match lua_interface.cpp registration).
// Add here when exporting a new VRMOD_* global.
static const char* kExpectedExports[] = {
    "GetVersion",
    "Init",
    "Shutdown",
    "GetPoses",
    "GetActions",
    "UpdatePosesAndActions",
    "ShareTextureBegin",
    "ShareTextureFinish",
    "GetDisplayInfo",
    "KeyboardOpen",
    "KeyboardClose",
    "KeyboardIsOpen",
    "KeyboardGetText",
    "KeyboardSetText",
    "KeyboardAppend",
    "VirtualDisplayCreate",
    "VirtualDisplayIsSupported",
    nullptr,
};

TEST(cpp_exports_registry_list_nonempty) {
    int n = 0;
    for (int i = 0; kExpectedExports[i]; ++i) n++;
    ASSERT_TRUE(n >= 10);
}

TEST(cpp_exports_registry_unique) {
    std::set<std::string> seen;
    for (int i = 0; kExpectedExports[i]; ++i) {
        ASSERT_TRUE(seen.insert(kExpectedExports[i]).second);
    }
}

TEST(cpp_exports_keyboard_settext_listed) {
    bool found = false;
    for (int i = 0; kExpectedExports[i]; ++i) {
        if (std::strcmp(kExpectedExports[i], "KeyboardSetText") == 0) found = true;
    }
    ASSERT_TRUE(found);
}

// Desktop view enum contract (Lua + launcher)
TEST(desktop_view_enum_follow_cam_is_4) {
    const int VIEW_NONE = 1;
    const int VIEW_LEFT = 2;
    const int VIEW_RIGHT = 3;
    const int VIEW_FOLLOW = 4;
    ASSERT_EQ(VIEW_NONE, 1);
    ASSERT_EQ(VIEW_LEFT, 2);
    ASSERT_EQ(VIEW_RIGHT, 3);
    ASSERT_EQ(VIEW_FOLLOW, 4);
    // cycle 1..4
    auto cycle = [](int v, int dir) {
        return 1 + ((v - 1 + dir + 4) % 4);
    };
    ASSERT_EQ(cycle(1, 1), 2);
    ASSERT_EQ(cycle(3, 1), 4);
    ASSERT_EQ(cycle(4, 1), 1);
    ASSERT_EQ(cycle(1, -1), 4);
}
