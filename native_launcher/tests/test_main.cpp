#include "../../tests/test_framework.h"
#include "math3d.hpp"
#include "gmod_spawn.hpp"
#include "last_play.hpp"

TEST(launcher_math3d_normalize) {
    Vec3 n = Normalize(V3(3.f, 0.f, 0.f));
    ASSERT_NEAR(n.x, 1.f, 1e-5f);
    ASSERT_NEAR(n.y, 0.f, 1e-5f);
    ASSERT_NEAR(n.z, 0.f, 1e-5f);
}

TEST(launcher_math3d_dot) {
    ASSERT_NEAR(Dot(V3(1, 0, 0), V3(0, 1, 0)), 0.f, 1e-6f);
    ASSERT_NEAR(Dot(V3(2, 0, 0), V3(3, 0, 0)), 6.f, 1e-6f);
}

TEST(launcher_math3d_cross) {
    Vec3 c = Cross(V3(1, 0, 0), V3(0, 1, 0));
    ASSERT_NEAR(c.z, 1.f, 1e-5f);
}

TEST(launcher_desktop_cycle_1_to_4) {
    auto cycle = [](int v, int dir) {
        return 1 + ((v - 1 + dir + 4) % 4);
    };
    ASSERT_EQ(cycle(3, 1), 4); // right → follow
    ASSERT_EQ(cycle(4, 1), 1);
}

// G01: handoff phase → panel detail / progress (pure, no I/O)
TEST(launcher_handoff_detail_known_phases) {
    auto d_map = CubeHandoffDetailForPhase("map_ready", true);
    ASSERT_TRUE(d_map.find("map loaded") != std::string::npos);
    auto d_take = CubeHandoffDetailForPhase("take_xr", true);
    ASSERT_TRUE(d_take.find("claims OpenXR") != std::string::npos);
    auto d_wait = CubeHandoffDetailForPhase("waiting_process", false);
    ASSERT_TRUE(d_wait.find("booting GMod") != std::string::npos);
}

TEST(launcher_handoff_progress_monotone) {
    float p_spawn = CubeHandoffProgressForPhase("spawned");
    float p_boot = CubeHandoffProgressForPhase("boot");
    float p_map = CubeHandoffProgressForPhase("map_ready");
    float p_take = CubeHandoffProgressForPhase("take_xr");
    float p_vr = CubeHandoffProgressForPhase("vr_active");
    ASSERT_TRUE(p_spawn > 0.f && p_spawn < p_boot);
    ASSERT_TRUE(p_boot < p_map && p_map < p_take && p_take < p_vr);
    ASSERT_TRUE(p_vr <= 1.f);
    ASSERT_TRUE(CubeHandoffProgressForPhase("unknown_token") < 0.f);
}

TEST(launcher_handoff_phase_label) {
    ASSERT_EQ(CubeHandoffPhaseLabel(""), std::string("SPAWNING"));
    ASSERT_EQ(CubeHandoffPhaseLabel("map_ready"), std::string("MAP READY"));
    ASSERT_EQ(CubeHandoffPhaseLabel("take_xr"), std::string("TAKE XR"));
}

// G11: Quick Play last map + gfx snapshot round-trip
TEST(launcher_last_play_roundtrip) {
    LastPlaySnapshot a;
    a.map = "gm_construct";
    a.gamemode = "sandbox";
    a.maxPlayers = 4;
    a.svLan = true;
    a.p2p = false;
    a.gfxPreset = 2;
    a.matPicmip = 0;
    a.matAntialias = 4;
    a.winW = 720;
    a.winH = 480;
    a.noborder = false;
    a.xrSsIdx = 3;
    a.valid = true;
    std::string body = LastPlay_Format(a);
    LastPlaySnapshot b;
    ASSERT_TRUE(LastPlay_Parse(body, b));
    ASSERT_EQ(b.map, std::string("gm_construct"));
    ASSERT_EQ(b.maxPlayers, 4);
    ASSERT_EQ(b.matAntialias, 4);
    ASSERT_EQ(b.winW, 720);
    ASSERT_TRUE(!b.noborder);
    ASSERT_EQ(b.xrSsIdx, 3);
}

TEST(launcher_last_play_rejects_empty) {
    LastPlaySnapshot b;
    ASSERT_TRUE(!LastPlay_Parse("v=1\ngamemode=sandbox\n", b));
}

TEST(launcher_last_play_clamps_desktopview) {
    LastPlaySnapshot b;
    ASSERT_TRUE(LastPlay_Parse("v=1\nmap=gm_flatgrass\nxr_desktopview=9\n", b));
    ASSERT_EQ(b.xrDesktopView, 4);
    ASSERT_TRUE(LastPlay_Parse("v=1\nmap=gm_flatgrass\nxr_desktopview=0\n", b));
    ASSERT_EQ(b.xrDesktopView, 1);
}

int main() {
    return RunAllTests();
}
