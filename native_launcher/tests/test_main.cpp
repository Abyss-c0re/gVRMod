#include "../../tests/test_framework.h"
#include "math3d.hpp"
#include "gmod_spawn.hpp"
#include "last_play.hpp"
#include "stage_pack.hpp"
#include "ambient_clip.hpp"

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
    // G04: cold Steam/hl2 wording (still means booting while panel holds XR)
    ASSERT_TRUE(d_wait.find("cold") != std::string::npos || d_wait.find("holds OpenXR") != std::string::npos);
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
    ASSERT_EQ(CubeHandoffPhaseLabel("take_xr"), std::string("TAKE XR · FADE"));
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

// G02: panel fade amount — pre-dim at take_xr, ramp on exit
TEST(launcher_handoff_fade_amount) {
    ASSERT_NEAR(CubeHandoffFadeAmount("boot", false, 0.f), 0.f, 1e-5f);
    float take = CubeHandoffFadeAmount("take_xr", false, 0.f);
    ASSERT_TRUE(take > 0.1f && take < 0.5f);
    float mid = CubeHandoffFadeAmount("take_xr", true, 1.25f);
    float end = CubeHandoffFadeAmount("take_xr", true, 2.5f);
    ASSERT_TRUE(mid > take);
    ASSERT_NEAR(end, 1.f, 1e-5f);
    ASSERT_TRUE(CubeHandoffPhaseLabel("take_xr").find("FADE") != std::string::npos);
}

// G02: eye-layer black overlay alpha clamp
TEST(launcher_handoff_layer_fade_alpha) {
    ASSERT_NEAR(CubeHandoffLayerFadeAlpha(0.f), 0.f, 1e-5f);
    ASSERT_NEAR(CubeHandoffLayerFadeAlpha(0.5f), 0.5f, 1e-5f);
    ASSERT_NEAR(CubeHandoffLayerFadeAlpha(1.f), 1.f, 1e-5f);
    ASSERT_NEAR(CubeHandoffLayerFadeAlpha(-1.f), 0.f, 1e-5f);
    ASSERT_NEAR(CubeHandoffLayerFadeAlpha(2.f), 1.f, 1e-5f);
    // Exit ramp end → solid black overlay before session drop
    float end = CubeHandoffFadeAmount("take_xr", true, 2.5f);
    ASSERT_NEAR(CubeHandoffLayerFadeAlpha(end), 1.f, 1e-5f);
}

// G13: reverse handoff pure labels (Cube reclaim not auto)
TEST(launcher_reverse_handoff_phases) {
    ASSERT_EQ(CubeReversePhaseLabel("vr_exit"), std::string("VR EXIT"));
    ASSERT_EQ(CubeReversePhaseLabel("panel_live"), std::string("PANEL LIVE"));
    ASSERT_TRUE(CubeReverseDetailForPhase("xr_released").find("relaunch") != std::string::npos);
    ASSERT_TRUE(CubeReverseProgressForPhase("vr_exit") > 0.f);
    ASSERT_TRUE(CubeReverseProgressForPhase("panel_live") >= 0.99f);
    ASSERT_TRUE(CubeReverseProgressForPhase("unknown") < 0.f);
}

// G04: cold Start inventory — boot kind labels; skip-spawn never true yet
TEST(launcher_cold_start_boot_kind) {
    ASSERT_EQ(CubeLaunchBootKind(false, false), std::string("COLD_SPAWN"));
    ASSERT_EQ(CubeLaunchBootKind(true, false), std::string("WARM_DETECTED"));
    ASSERT_EQ(CubeLaunchBootKind(true, true), std::string("COLD_SPAWN"));
    ASSERT_TRUE(CubeLaunchBootLabel("COLD_SPAWN").find("COLD") != std::string::npos);
    ASSERT_TRUE(CubeLaunchBootLabel("WARM_DETECTED").find("WARM") != std::string::npos);
    ASSERT_TRUE(!CubeLaunchShouldSkipSpawn("WARM_DETECTED"));
    ASSERT_TRUE(!CubeLaunchShouldSkipSpawn("COLD_SPAWN"));
    ASSERT_TRUE(CubeColdStartProgressSeconds() >= 40.f);
    auto d = CubeHandoffDetailForPhase("waiting_process", false);
    ASSERT_TRUE(d.find("cold") != std::string::npos || d.find("Steam") != std::string::npos);
}

// G12: ambient clip contract — pure should-play + format/parse + status label
TEST(launcher_ambient_clip_contract) {
    ASSERT_TRUE(CubeAmbient_ShouldPlay(1.f, true));
    ASSERT_TRUE(!CubeAmbient_ShouldPlay(0.f, true));
    ASSERT_TRUE(!CubeAmbient_ShouldPlay(1.f, false));
    ASSERT_NEAR(CubeAmbient_EffectiveVolume(0.5f, 0.5f), 0.25f, 1e-5f);
    AmbientClipSnapshot a;
    a.gain = 0.88f;
    a.handoff = true;
    a.playing = CubeAmbient_ShouldPlay(a.gain, true);
    a.clip_rel = CubeAmbient_DefaultClipRel();
    a.ts = 42;
    std::string body = CubeAmbient_Format(a);
    AmbientClipSnapshot b;
    ASSERT_TRUE(CubeAmbient_Parse(body, b));
    ASSERT_NEAR(b.gain, 0.88f, 1e-4f);
    ASSERT_TRUE(b.playing);
    ASSERT_TRUE(b.handoff);
    ASSERT_EQ(b.clip_rel, std::string(CubeAmbient_DefaultClipRel()));
    auto path = CubeAmbient_ResolveClipPath("/opt/cube/assets", "ambient/x.ogg");
    ASSERT_EQ(path, std::string("/opt/cube/assets/ambient/x.ogg"));
    ASSERT_TRUE(CubeAmbient_StatusLabel(0.f, false, true).find("SILENT") != std::string::npos);
    ASSERT_TRUE(CubeAmbient_StatusLabel(1.f, true, false).find("MISSING") != std::string::npos);
}

// G12: ambient gain law — full during hold, duck at take_xr, 0 on exit complete
TEST(launcher_handoff_audio_gain) {
    ASSERT_NEAR(CubeHandoffAudioGain("waiting_process", false, 0.f), 1.f, 1e-5f);
    ASSERT_NEAR(CubeHandoffAudioGain("boot", false, 0.f), 0.88f, 1e-5f);
    float mapG = CubeHandoffAudioGain("map_ready", false, 0.f);
    float takeG = CubeHandoffAudioGain("take_xr", false, 0.f);
    ASSERT_TRUE(mapG > takeG && takeG > 0.f);
    ASSERT_NEAR(CubeHandoffAudioGain("vr_active", false, 0.f), 0.f, 1e-5f);
    float mid = CubeHandoffAudioGain("take_xr", true, 1.25f);
    float end = CubeHandoffAudioGain("take_xr", true, 2.5f);
    ASSERT_TRUE(mid < 0.6f && mid > 0.f);
    ASSERT_NEAR(end, 0.f, 1e-5f);
    // Inverse of visual fade: when fade is high, audio gain is low
    float fadeEnd = CubeHandoffFadeAmount("take_xr", true, 2.5f);
    ASSERT_NEAR(fadeEnd + end, 1.f, 1e-5f);
}

// G03: STAGE/cal pack — pure format/parse + usability (no auto-apply height)
TEST(launcher_stage_pack_roundtrip) {
    StagePackSnapshot a;
    a.refSpace = "stage";
    a.headX = 0.1f;
    a.headY = 1.65f;
    a.headZ = -0.2f;
    a.headOk = true;
    a.viewScale = 1.0f;
    a.scaleFactor = 1.05f;
    a.supersample = 1.5f;
    a.map = "gm_construct";
    a.source = "cube_webui";
    a.ts = 12345;
    std::string body = StagePack_Format(a);
    StagePackSnapshot b;
    ASSERT_TRUE(StagePack_Parse(body, b));
    ASSERT_EQ(b.refSpace, std::string("STAGE"));
    ASSERT_NEAR(b.headY, 1.65f, 1e-4f);
    ASSERT_TRUE(b.headOk);
    ASSERT_NEAR(b.scaleFactor, 1.05f, 1e-4f);
    ASSERT_EQ(b.map, std::string("gm_construct"));
    ASSERT_TRUE(StagePack_IsUsable(b));
}

TEST(launcher_stage_pack_rejects_empty) {
    StagePackSnapshot b;
    ASSERT_TRUE(!StagePack_Parse("v=1\nmap=gm_construct\n", b));
    ASSERT_TRUE(!StagePack_IsUsable(b));
}

TEST(launcher_stage_pack_head_y_sanity) {
    // Extreme head Y clears head_ok (still valid space pack)
    StagePackSnapshot b;
    ASSERT_TRUE(StagePack_Parse("v=1\nref_space=LOCAL\nhead_y_m=9.0\nhead_ok=1\n", b));
    ASSERT_TRUE(StagePack_IsUsable(b));
    ASSERT_TRUE(!b.headOk);
    ASSERT_EQ(StagePack_NormalizeSpace("local"), std::string("LOCAL"));
}

int main() {
    return RunAllTests();
}
