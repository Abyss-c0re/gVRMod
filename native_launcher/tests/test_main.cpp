#include "../../tests/test_framework.h"
#include "math3d.hpp"
#include "gmod_spawn.hpp"
#include "last_play.hpp"
#include "stage_pack.hpp"
#include "ambient_clip.hpp"
#include "cube_return.hpp"
#include "warm_reuse.hpp"
#include "window_chrome.hpp"
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

    // G13 careful XR plan: panel refresh only; never restart session
    auto xrOff = CubeReclaimXrPlanDecide(notify, false, false);
    ASSERT_TRUE(!xrOff.do_anything);
    ASSERT_EQ(xrOff.method, std::string("none"));
    ASSERT_TRUE(!CubeReclaimShouldExecuteXrPlan(xrOff, false));
    auto xrOn = CubeReclaimXrPlanDecide(autoR, true, false);
    ASSERT_TRUE(xrOn.do_anything);
    ASSERT_TRUE(xrOn.refresh_panel);
    ASSERT_TRUE(!xrOn.restart_session);
    ASSERT_TRUE(!xrOn.rebind_actions);
    ASSERT_EQ(xrOn.method, std::string("panel_refresh"));
    ASSERT_TRUE(CubeReclaimShouldExecuteXrPlan(xrOn, true));
    ASSERT_TRUE(CubeReclaimXrPlanLabel(xrOn).find("REFRESH") != std::string::npos);
    auto xrRebind = CubeReclaimXrPlanDecide(autoR, true, true);
    ASSERT_EQ(xrRebind.method, std::string("full_rebind_deferred"));
    ASSERT_TRUE(!xrRebind.restart_session);
    auto heSoft = CubeReclaim_HmdExpect(notify, xrOff);
    ASSERT_EQ(heSoft.verdict, std::string("expect_soft_ack"));
    ASSERT_TRUE(heSoft.expect_session_kept);
    auto heXr = CubeReclaim_HmdExpect(autoR, xrOn);
    ASSERT_EQ(heXr.verdict, std::string("expect_panel_refresh"));

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
    // G04 HMD warm expect
    auto heDef = CubeWarm_HmdExpect(defer, &planDef, false, false);
    ASSERT_EQ(heDef.verdict, std::string("expect_deferred"));
    ASSERT_TRUE(heDef.expect_no_changelevel);
    ASSERT_TRUE(heDef.checklist.find("DEFERRED") != std::string::npos);
    auto heSame = CubeWarm_HmdExpect(same, nullptr, false, false);
    ASSERT_EQ(heSame.verdict, std::string("expect_same_map"));
    auto heChg = CubeWarm_HmdExpect(chg, &planOn, true, false);
    ASSERT_EQ(heChg.verdict, std::string("expect_changelevel"));
    ASSERT_TRUE(heChg.checklist.find("CHANGELEVEL") != std::string::npos);
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

    // G12 master taste: default 0.55; env override; clamp
    unsetenv("GVRMOD_AMBIENT_MASTER");
    ASSERT_NEAR(CubeAmbient_DefaultComfortMaster(), 0.55f, 1e-4f);
    ASSERT_NEAR(CubeAmbient_MasterFromEnv(nullptr), 0.55f, 1e-4f);
    ASSERT_NEAR(CubeAmbient_MasterFromEnv(""), 0.55f, 1e-4f);
    ASSERT_NEAR(CubeAmbient_MasterFromEnv("0.35"), 0.35f, 1e-4f);
    ASSERT_NEAR(CubeAmbient_MasterFromEnv("2"), 1.f, 1e-4f);
    ASSERT_NEAR(CubeAmbient_MasterFromEnv("0"), 0.05f, 1e-4f);
    ASSERT_NEAR(CubeAmbient_MasterFromEnv("nope"), 0.55f, 1e-4f);
    ASSERT_NEAR(CubeAmbient_ComfortMaster(), 0.55f, 1e-4f);
    setenv("GVRMOD_AMBIENT_MASTER", "0.4", 1);
    ASSERT_NEAR(CubeAmbient_ComfortMaster(), 0.4f, 1e-4f);
    unsetenv("GVRMOD_AMBIENT_MASTER");
    ASSERT_EQ(std::string(CubeAmbient_TasteBand(0.2f)), std::string("soft"));
    ASSERT_EQ(std::string(CubeAmbient_TasteBand(0.55f)), std::string("comfort"));
    ASSERT_EQ(std::string(CubeAmbient_TasteBand(0.9f)), std::string("present"));
    auto he = CubeAmbient_HmdVolumeExpect(0.55f, 0.9f, true, true, true);
    ASSERT_TRUE(he.expect_audible);
    ASSERT_TRUE(he.checklist.find("HOLD") != std::string::npos);
    ASSERT_NEAR(he.sample_volume, 0.9f * 0.55f, 1e-4f);
    auto heOff = CubeAmbient_HmdVolumeExpect(0.55f, 1.f, true, false, true);
    ASSERT_TRUE(!heOff.expect_audible);
    ASSERT_TRUE(heOff.checklist.find("SILENT") != std::string::npos);

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

// G30: FOV archive write-only-when-touched (never clobber Vision cal)
TEST(launcher_fov_archive_write_touched_g30) {
    ASSERT_NEAR(CubeFov_DefaultScale(), 1.0f, 1e-4f);
    ASSERT_NEAR(CubeFov_MinScale(), 0.1f, 1e-4f);
    ASSERT_NEAR(CubeFov_MaxScale(), 2.0f, 1e-4f);
    ASSERT_TRUE(!CubeFov_ShouldWrite(false));
    ASSERT_TRUE(CubeFov_ShouldWrite(true));
    ASSERT_NEAR(CubeFov_ClampScale(0.05f), 0.1f, 1e-4f);
    ASSERT_NEAR(CubeFov_ClampScale(3.0f), 2.0f, 1e-4f);
    ASSERT_NEAR(CubeFov_ClampScale(1.1f), 1.1f, 1e-4f);
    auto keep = CubeFov_Decide(false, 1.0f, 1.0f);
    ASSERT_TRUE(!keep.write);
    ASSERT_EQ(keep.risk, std::string("keep_archive"));
    ASSERT_EQ(CubeFov_StatusLabel(keep), std::string("FOV · KEEP ARCHIVE"));
    ASSERT_TRUE(CubeFov_OmitComment().find("preserve") != std::string::npos);
    auto heKeep = CubeFov_HmdExpect(keep);
    ASSERT_EQ(heKeep.verdict, std::string("expect_keep_archive"));
    ASSERT_TRUE(heKeep.expect_vision_preserved);
    ASSERT_TRUE(heKeep.checklist.find("G30") != std::string::npos);
    auto write = CubeFov_Decide(true, 0.95f, 1.05f);
    ASSERT_TRUE(write.write);
    ASSERT_NEAR(write.scale_x, 0.95f, 1e-4f);
    ASSERT_NEAR(write.scale_y, 1.05f, 1e-4f);
    ASSERT_EQ(write.risk, std::string("write_user"));
    ASSERT_EQ(CubeFov_StatusLabel(write), std::string("FOV · WRITE USER"));
    auto heW = CubeFov_HmdExpect(write);
    ASSERT_EQ(heW.verdict, std::string("expect_write_user"));
    ASSERT_TRUE(!heW.expect_vision_preserved);
    auto clamp = CubeFov_Decide(true, 0.01f, 9.0f);
    ASSERT_TRUE(clamp.write);
    ASSERT_NEAR(clamp.scale_x, 0.1f, 1e-4f);
    ASSERT_NEAR(clamp.scale_y, 2.0f, 1e-4f);
    ASSERT_EQ(clamp.risk, std::string("clamp_write"));
    ASSERT_EQ(CubeFov_StatusLabel(clamp), std::string("FOV · WRITE CLAMP"));
}

// G29: supersample cold-start cap (never crank SS at Start)
TEST(launcher_supersample_cold_cap_g29) {
    ASSERT_NEAR(CubeSs_ColdStartCap(), 1.4f, 1e-4f);
    ASSERT_NEAR(CubeSs_CubeDefault(), 1.5f, 1e-4f);
    ASSERT_NEAR(CubeSs_LadderFromIdx(3), 1.5f, 1e-4f);
    ASSERT_NEAR(CubeSs_LadderFromIdx(5), 2.0f, 1e-4f);
    ASSERT_NEAR(CubeSs_ClampColdStart(2.0f), 1.4f, 1e-4f);
    ASSERT_NEAR(CubeSs_ClampColdStart(1.25f), 1.25f, 1e-4f);
    ASSERT_NEAR(CubeSs_ClampColdStart(1.5f), 1.4f, 1e-4f);
    ASSERT_NEAR(CubeSs_ClampLive(2.0f), 2.0f, 1e-4f);
    ASSERT_NEAR(CubeSs_ClampLive(3.0f), 2.0f, 1e-4f);
    auto cold = CubeSs_Decide(2.0f, true);
    ASSERT_TRUE(cold.capped);
    ASSERT_NEAR(cold.applied, 1.4f, 1e-4f);
    ASSERT_EQ(cold.risk, std::string("cold_capped"));
    ASSERT_EQ(CubeSs_StatusLabel(cold), std::string("SS · COLD CAP 1.4"));
    auto he = CubeSs_HmdExpect(cold);
    ASSERT_EQ(he.verdict, std::string("expect_cold_cap"));
    ASSERT_TRUE(he.checklist.find("G29") != std::string::npos);
    auto ok = CubeSs_Decide(1.0f, true);
    ASSERT_TRUE(!ok.capped);
    ASSERT_EQ(ok.risk, std::string("none"));
    auto live = CubeSs_Decide(1.75f, false);
    ASSERT_EQ(live.risk, std::string("live"));
    ASSERT_NEAR(live.applied, 1.75f, 1e-4f);
}

// G28: soft handoff timeout law (90s soft / 180s hard; never racey)
TEST(launcher_handoff_timeout_law_g28) {
    ASSERT_NEAR(CubeHandoffSoftReleaseSeconds(), 90.f, 1e-3f);
    ASSERT_NEAR(CubeHandoffHardTimeoutSeconds(), 180.f, 1e-3f);
    auto hold = CubeHandoffTimeout_Decide(false, false, 30.f);
    ASSERT_TRUE(!hold.should_release);
    ASSERT_TRUE(!hold.racey);
    ASSERT_EQ(hold.risk, std::string("hold"));
    ASSERT_EQ(CubeHandoffTimeout_StatusLabel(hold), std::string("HAND · HOLD"));
    auto take = CubeHandoffTimeout_Decide(true, true, 5.f);
    ASSERT_TRUE(take.should_release);
    ASSERT_EQ(take.risk, std::string("take_xr"));
    auto softEarly = CubeHandoffTimeout_Decide(false, true, 50.f);
    ASSERT_TRUE(!softEarly.should_release); // need >90 with gmod up
    auto soft = CubeHandoffTimeout_Decide(false, true, 91.f);
    ASSERT_TRUE(soft.should_release);
    ASSERT_EQ(soft.risk, std::string("soft"));
    ASSERT_EQ(CubeHandoffTimeout_StatusLabel(soft), std::string("HAND · SOFT 90S"));
    auto hard = CubeHandoffTimeout_Decide(false, false, 181.f);
    ASSERT_TRUE(hard.should_release);
    ASSERT_EQ(hard.risk, std::string("hard"));
    // Soft without process must NOT fire before hard (race window)
    auto noSoft = CubeHandoffTimeout_Decide(false, false, 100.f);
    ASSERT_TRUE(!noSoft.should_release);
    ASSERT_TRUE(!noSoft.soft_due);
    auto he = CubeHandoffTimeout_HmdExpect(take);
    ASSERT_EQ(he.verdict, std::string("expect_take_xr"));
    ASSERT_TRUE(he.checklist.find("G28") != std::string::npos);
    auto heHold = CubeHandoffTimeout_HmdExpect(hold);
    ASSERT_EQ(heHold.verdict, std::string("expect_hold"));
}

// G18: framed window chrome law (never force -noborder)
TEST(launcher_window_chrome_cube_default) {
    ASSERT_TRUE(WindowChrome_CubeWindowed());
    ASSERT_TRUE(!WindowChrome_CubeNoborder());
    ASSERT_TRUE(!WindowChrome_ShouldForceNoborder("product"));
    ASSERT_TRUE(!WindowChrome_ShouldForceNoborder(nullptr));
    ASSERT_TRUE(WindowChrome_ShouldForceNoborder("force_test")); // unit only
    auto d = WindowChrome_CubeDefault();
    ASSERT_TRUE(d.valid);
    ASSERT_TRUE(d.windowed);
    ASSERT_TRUE(!d.noborder);
    ASSERT_TRUE(d.force_noborder_forbidden);
    ASSERT_EQ(d.risk, std::string("none"));
    ASSERT_EQ(WindowChrome_StatusLabel(d), std::string("WIN · FRAMED"));
}

TEST(launcher_window_chrome_sanitize_and_args) {
    ASSERT_TRUE(!WindowChrome_SanitizeNoborder(true, false)); // missing key → framed
    ASSERT_TRUE(WindowChrome_SanitizeNoborder(true, true));
    ASSERT_TRUE(!WindowChrome_SanitizeNoborder(false, true));
    std::string framed = WindowChrome_BuildArgs(true, false, 720, 480);
    ASSERT_TRUE(framed.find("-windowed") != std::string::npos);
    ASSERT_TRUE(framed.find("-w 720") != std::string::npos);
    ASSERT_TRUE(framed.find("-h 480") != std::string::npos);
    ASSERT_TRUE(framed.find("-noborder") == std::string::npos);
    std::string borderless = WindowChrome_BuildArgs(true, true, 1280, 720);
    ASSERT_TRUE(borderless.find("-noborder") != std::string::npos);
    std::string full = WindowChrome_BuildArgs(false, false, 1920, 1080);
    ASSERT_TRUE(full.find("-fullscreen") != std::string::npos);
    ASSERT_TRUE(full.find("-windowed") == std::string::npos);
}

TEST(launcher_window_chrome_hmd_expect) {
    auto framed = WindowChrome_CubeDefault();
    auto he = WindowChrome_HmdExpect(framed);
    ASSERT_EQ(he.verdict, std::string("expect_framed"));
    ASSERT_TRUE(he.expect_title_chrome);
    ASSERT_TRUE(he.checklist.find("G18") != std::string::npos);
    ASSERT_TRUE(he.checklist.find("FRAMED") != std::string::npos);

    auto bl = WindowChrome_Decide(true, true, 720, 480, true);
    auto heBl = WindowChrome_HmdExpect(bl);
    ASSERT_EQ(heBl.verdict, std::string("expect_borderless_opt_in"));
    ASSERT_TRUE(!heBl.expect_title_chrome);

    auto missing = WindowChrome_Decide(true, true, 720, 480, false);
    ASSERT_TRUE(!missing.noborder); // invent-forbidden
    ASSERT_EQ(WindowChrome_StatusLabel(missing), std::string("WIN · FRAMED"));
    ASSERT_TRUE(!WindowChrome_IsForceRisk(framed));
}

int main() {
    return RunAllTests();
}
