#include "../../tests/test_framework.h"
#include "math3d.hpp"
#include "gmod_spawn.hpp"
#include "last_play.hpp"
#include "stage_pack.hpp"
#include "ambient_clip.hpp"
#include "cube_return.hpp"
#include "warm_reuse.hpp"
#include <algorithm>
#include <cstdlib>

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
    ASSERT_TRUE(CubeReverseDetailForPhase("xr_released").find("OpenXR") != std::string::npos
                || CubeReverseDetailForPhase("xr_released").find("session") != std::string::npos);
    ASSERT_TRUE(CubeReverseProgressForPhase("vr_exit") > 0.f);
    ASSERT_TRUE(CubeReverseProgressForPhase("panel_live") >= 0.99f);
    ASSERT_TRUE(CubeReverseProgressForPhase("unknown") < 0.f);
}

// G13: reclaim poll decide + soft ack plan (XR rebind still env-gated)
TEST(launcher_cube_return_reclaim_poll) {
    ASSERT_TRUE(!CubeReclaimWantEnv(nullptr));
    ASSERT_TRUE(CubeReclaimWantEnv("1"));
    unsetenv("GVRMOD_CUBE_RECLAIM");
    ASSERT_TRUE(!CubeReclaimEnabled());
    ASSERT_TRUE(CubeReclaimSoftAckEnabled());
    ASSERT_TRUE(CubeReclaimSoftAckHoldSeconds() >= 1.f);

    CubeReturnSnapshot empty;
    auto idle = CubeReclaimDecide(false, empty);
    ASSERT_EQ(idle.action, std::string("idle"));
    ASSERT_TRUE(!idle.auto_reclaim);
    ASSERT_TRUE(!idle.show_panel);

    CubeReturnSnapshot a;
    a.phase = "vr_exit";
    a.map = "gm_construct";
    a.source = "vrmod";
    a.ts = 11;
    CubeReturnSnapshot b;
    ASSERT_TRUE(CubeReturn_Parse(CubeReturn_Format(a), b));
    ASSERT_EQ(b.phase, std::string("vr_exit"));
    ASSERT_EQ(b.map, std::string("gm_construct"));

    auto notify = CubeReclaimDecide(true, b);
    ASSERT_EQ(notify.action, std::string("notify"));
    ASSERT_EQ(notify.reason, std::string("eligible_deferred"));
    ASSERT_TRUE(notify.show_panel);
    ASSERT_TRUE(!notify.auto_reclaim);
    ASSERT_TRUE(CubeReclaimPanelLabel(notify).find("SOFT") != std::string::npos
                || CubeReclaimPanelLabel(notify).find("ACK") != std::string::npos);

    // Soft ack: hold then write panel_live
    auto pend = CubeReclaimAckPlanDecide(notify, 0.5f, 2.5f, true);
    ASSERT_TRUE(!pend.should_write);
    auto ready = CubeReclaimAckPlanDecide(notify, 3.0f, 2.5f, true);
    ASSERT_TRUE(ready.should_write);
    ASSERT_EQ(ready.next_phase, std::string("panel_live"));
    ASSERT_TRUE(ready.clear_banner);
    auto noSoft = CubeReclaimAckPlanDecide(notify, 9.f, 2.5f, false);
    ASSERT_TRUE(!noSoft.should_write);

    auto autoR = CubeReclaimDecide(true, b, /*featureEnabled=*/true);
    ASSERT_EQ(autoR.action, std::string("reclaim_auto"));
    ASSERT_TRUE(autoR.auto_reclaim);

    CubeReturnSnapshot live;
    live.phase = "panel_live";
    live.valid = true;
    auto done = CubeReclaimDecide(true, live);
    ASSERT_EQ(done.action, std::string("idle"));
    ASSERT_TRUE(!done.show_panel);

    ASSERT_TRUE(CubeReturn_Parse("", b) == false);
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

// G04: warm reuse pure decision — feature off → warm_request, never skip spawn
TEST(launcher_warm_reuse_decide) {
    ASSERT_TRUE(!CubeWarmReuseWantEnv(nullptr));
    ASSERT_TRUE(CubeWarmReuseWantEnv("1"));
    unsetenv("GVRMOD_WARM_REUSE");
    ASSERT_TRUE(!CubeWarmReuseEnabled());
    auto cold = CubeWarmReuseDecide(false, false, "gm_construct");
    ASSERT_EQ(cold.action, std::string("cold_spawn"));
    ASSERT_TRUE(!cold.skip_spawn);
    auto defer = CubeWarmReuseDecide(true, false, "gm_construct");
    ASSERT_EQ(defer.action, std::string("warm_request"));
    ASSERT_EQ(defer.reason, std::string("eligible_deferred"));
    ASSERT_TRUE(!defer.skip_spawn);
    ASSERT_EQ(CubeWarmReuseBootKind(defer), std::string("WARM_DETECTED"));
    auto forced = CubeWarmReuseDecide(true, true, "gm_construct");
    ASSERT_EQ(forced.action, std::string("cold_spawn"));
    auto nomap = CubeWarmReuseDecide(true, false, "  ");
    ASSERT_EQ(nomap.reason, std::string("no_map"));
    // Feature-on path (unit only — product default off unless GVRMOD_WARM_REUSE=1)
    auto reuse = CubeWarmReuseDecide(true, false, "gm_flatgrass", /*featureEnabled=*/true);
    ASSERT_EQ(reuse.action, std::string("warm_reuse"));
    ASSERT_TRUE(reuse.skip_spawn);
    ASSERT_TRUE(CubeLaunchShouldSkipSpawn(reuse));
    WarmRequestSnapshot a;
    a.action = "warm_request";
    a.reason = "eligible_deferred";
    a.map = "gm_construct";
    a.ts = 7;
    WarmRequestSnapshot b;
    ASSERT_TRUE(CubeWarmReuse_Parse(CubeWarmReuse_Format(a), b));
    ASSERT_EQ(b.map, std::string("gm_construct"));
    ASSERT_TRUE(CubeWarmReuseDetail(defer).find("warm_request") != std::string::npos);
}

// G04: skip-spawn plan — markers + warm_attach phase only when feature on
TEST(launcher_warm_skip_spawn_plan) {
    unsetenv("GVRMOD_WARM_REUSE");
    auto defer = CubeWarmReuseDecide(true, false, "gm_construct", false);
    auto planOff = CubeWarmSkipSpawnPlanDecide(defer, false);
    ASSERT_TRUE(!planOff.skip_spawn);
    ASSERT_TRUE(!planOff.write_markers);

    auto reuse = CubeWarmReuseDecide(true, false, "gm_flatgrass", true);
    auto planOn = CubeWarmSkipSpawnPlanDecide(reuse, true);
    ASSERT_TRUE(planOn.skip_spawn);
    ASSERT_TRUE(planOn.write_markers);
    ASSERT_TRUE(planOn.write_stage_pack);
    ASSERT_EQ(planOn.initial_phase, std::string("warm_attach"));
    ASSERT_TRUE(planOn.detail.find("skip Steam") != std::string::npos
                || planOn.detail.find("attach") != std::string::npos);
    ASSERT_EQ(CubeWarmSkipSpawnPhaseLabel("warm_attach"), std::string("WARM ATTACH"));
    ASSERT_TRUE(CubeHandoffDetailForPhase("warm_attach", true).find("warm") != std::string::npos);
    ASSERT_TRUE(CubeHandoffProgressForPhase("warm_attach") > 0.2f);
    ASSERT_EQ(CubeHandoffPhaseLabel("warm_attach"), std::string("WARM ATTACH"));
}

// G04: map attach decide — default no changelevel; normalize map tokens
TEST(launcher_warm_attach_decide) {
    ASSERT_EQ(CubeWarmAttach_NormalizeMap("maps/GM_Construct.bsp"), std::string("gm_construct"));
    auto idle = CubeWarmAttachDecide(false, "gm_construct", "warm_request", "gm_construct");
    ASSERT_EQ(idle.action, std::string("idle"));
    auto same = CubeWarmAttachDecide(true, "gm_flatgrass", "warm_request", "GM_Flatgrass");
    ASSERT_EQ(same.action, std::string("same_map"));
    ASSERT_TRUE(!same.would_changelevel);
    auto defer = CubeWarmAttachDecide(true, "gm_flatgrass", "warm_request", "gm_construct", false);
    ASSERT_EQ(defer.action, std::string("deferred"));
    ASSERT_EQ(defer.reason, std::string("eligible_deferred"));
    ASSERT_TRUE(!defer.would_changelevel);
    auto chg = CubeWarmAttachDecide(true, "gm_flatgrass", "warm_request", "gm_construct", true);
    ASSERT_EQ(chg.action, std::string("changelevel"));
    ASSERT_TRUE(chg.would_changelevel);
    auto rej = CubeWarmAttachDecide(true, "", "warm_request", "gm_construct");
    ASSERT_EQ(rej.action, std::string("reject"));
    ASSERT_TRUE(CubeWarmAttachToast(defer).find("deferred") != std::string::npos);
    ASSERT_TRUE(CubeWarmAttachToast(idle).empty());
    // G04 careful changelevel plan — default off; map token gate
    ASSERT_TRUE(CubeWarmAttach_MapTokenOk("gm_construct"));
    ASSERT_TRUE(!CubeWarmAttach_MapTokenOk("gm_construct; quit"));
    ASSERT_TRUE(!CubeWarmAttach_MapTokenOk(""));
    ASSERT_TRUE(!CubeWarmChangelevelWantEnv(nullptr));
    ASSERT_TRUE(CubeWarmChangelevelWantEnv("1"));
    unsetenv("GVRMOD_WARM_CHANGELEVEL");
    ASSERT_TRUE(!CubeWarmChangelevelEnabled());
    WarmChangelevelAllowFlags none{};
    ASSERT_TRUE(!CubeWarmAttach_AllowChangelevelFromFlags(none));
    WarmChangelevelAllowFlags con{};
    con.convar_on = true;
    ASSERT_TRUE(CubeWarmAttach_AllowChangelevelFromFlags(con));
    auto planDef = CubeWarmChangelevelPlanDecide(defer);
    ASSERT_TRUE(!planDef.do_changelevel);
    ASSERT_TRUE(!CubeWarmShouldExecuteChangelevel(planDef, true));
    auto planOn = CubeWarmChangelevelPlanDecide(chg);
    ASSERT_TRUE(planOn.do_changelevel);
    ASSERT_EQ(planOn.method, std::string("changelevel"));
    ASSERT_EQ(CubeWarmChangelevelCmd(planOn), std::string("changelevel gm_flatgrass"));
    ASSERT_TRUE(!CubeWarmShouldExecuteChangelevel(planOn, false));
    ASSERT_TRUE(CubeWarmShouldExecuteChangelevel(planOn, true));
    auto et = CubeWarmChangelevelExecuteToast(true, true, "gm_flatgrass", "");
    ASSERT_TRUE(et.find("changelevel") != std::string::npos);
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
    a.clip_present = true;
    a.clip_rel = CubeAmbient_DefaultClipRel();
    a.ts = 42;
    std::string body = CubeAmbient_Format(a);
    AmbientClipSnapshot b;
    ASSERT_TRUE(CubeAmbient_Parse(body, b));
    ASSERT_NEAR(b.gain, 0.88f, 1e-4f);
    ASSERT_TRUE(b.playing);
    ASSERT_TRUE(b.handoff);
    ASSERT_TRUE(b.clip_present);
    ASSERT_EQ(b.clip_rel, std::string(CubeAmbient_DefaultClipRel()));
    auto path = CubeAmbient_ResolveClipPath("/opt/cube/assets", "ambient/x.ogg");
    ASSERT_EQ(path, std::string("/opt/cube/assets/ambient/x.ogg"));
    ASSERT_TRUE(CubeAmbient_StatusLabel(0.f, false, true).find("SILENT") != std::string::npos);
    ASSERT_TRUE(CubeAmbient_StatusLabel(1.f, true, false).find("MISSING") != std::string::npos);
}

// G12: ambient asset candidates + player decide (playback default ON, opt-out)
TEST(launcher_ambient_player_decide) {
    ASSERT_TRUE(!CubeAmbientPlayerWantEnv(nullptr));
    ASSERT_TRUE(!CubeAmbientPlayerWantEnv(""));
    ASSERT_TRUE(!CubeAmbientPlayerWantEnv("0"));
    ASSERT_TRUE(CubeAmbientPlayerWantEnv("1"));
    ASSERT_TRUE(CubeAmbientPlayerWantEnv("true"));
    ASSERT_TRUE(CubeAmbientPlayerEnvIsOff("0"));
    ASSERT_TRUE(CubeAmbientPlayerEnvIsOff("false"));
    ASSERT_TRUE(CubeAmbientPlayerEnvIsOff("off"));
    ASSERT_TRUE(!CubeAmbientPlayerEnvIsOff("1"));
    // Default ON when env unset; explicit 0 silences
    ASSERT_TRUE(CubeAmbientPlayerEnabledFromEnv(nullptr, true));
    ASSERT_TRUE(!CubeAmbientPlayerEnabledFromEnv("0", true));
    ASSERT_TRUE(CubeAmbientPlayerEnabledFromEnv("1", true));
    unsetenv("GVRMOD_AMBIENT_PLAY");
    ASSERT_TRUE(CubeAmbientPlayerEnabled());
    setenv("GVRMOD_AMBIENT_PLAY", "0", 1);
    ASSERT_TRUE(!CubeAmbientPlayerEnabled());
    unsetenv("GVRMOD_AMBIENT_PLAY");

    ASSERT_NEAR(CubeAmbient_ComfortMaster(), 0.55f, 1e-4f);

    auto cands = CubeAmbient_AssetsDirCandidates("/env/assets", "/opt/bin", "/src/assets");
    ASSERT_TRUE(cands.size() >= 3);
    ASSERT_EQ(cands[0], std::string("/env/assets"));
    ASSERT_TRUE(cands[1].find("/opt/bin/assets") != std::string::npos);

    auto miss = CubeAmbient_PlayerDecide(true, 1.f, false, false, true);
    ASSERT_EQ(miss.reason, std::string("clip_missing"));
    ASSERT_EQ(miss.action, std::string("idle"));

    auto defer = CubeAmbient_PlayerDecide(true, 0.9f, true, false, /*featureEnabled=*/false);
    ASSERT_EQ(defer.action, std::string("deferred"));
    ASSERT_EQ(defer.reason, std::string("eligible_deferred"));
    ASSERT_TRUE(defer.want_audible);
    ASSERT_TRUE(CubeAmbient_StatusLabelEx(0.9f, true, true, defer).find("DEFERRED") != std::string::npos);

    auto start = CubeAmbient_PlayerDecide(true, 0.9f, true, false, /*featureEnabled=*/true);
    ASSERT_EQ(start.action, std::string("start"));
    // Comfort master softens volume
    ASSERT_NEAR(start.volume, 0.9f * CubeAmbient_ComfortMaster(), 1e-4f);
    auto gain = CubeAmbient_PlayerDecide(true, 0.5f, true, true, true);
    ASSERT_EQ(gain.action, std::string("set_gain"));
    auto stop = CubeAmbient_PlayerDecide(true, 0.f, true, true, true);
    ASSERT_EQ(stop.action, std::string("stop"));
    auto idle = CubeAmbient_PlayerDecide(false, 1.f, true, false, true);
    ASSERT_EQ(idle.action, std::string("idle"));
}

// G12: pure ffplay/paplay argv + gain restart threshold (no process spawn)
TEST(launcher_ambient_backend_argv) {
    ASSERT_EQ(CubeAmbient_VolumePercent(0.42f), 42);
    ASSERT_EQ(CubeAmbient_VolumePercent(-1.f), 0);
    ASSERT_EQ(CubeAmbient_VolumePercent(2.f), 100);
    auto ff = CubeAmbient_PlayArgv("ffplay", "/tmp/a.ogg", 0.5f);
    ASSERT_TRUE(ff.size() >= 8);
    ASSERT_EQ(ff[0], std::string("ffplay"));
    ASSERT_TRUE(std::find(ff.begin(), ff.end(), "-loop") != ff.end());
    ASSERT_EQ(ff.back(), std::string("/tmp/a.ogg"));
    auto pa = CubeAmbient_PlayArgv("paplay", "/tmp/a.ogg", 0.5f);
    ASSERT_EQ(pa.size(), (size_t)2);
    ASSERT_EQ(pa[0], std::string("paplay"));
    ASSERT_TRUE(CubeAmbient_ShouldRestartForGain(0.9f, 0.5f));
    ASSERT_TRUE(!CubeAmbient_ShouldRestartForGain(0.5f, 0.55f));
    ASSERT_TRUE(CubeAmbient_PlayArgv("ffplay", "", 1.f).empty());
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
