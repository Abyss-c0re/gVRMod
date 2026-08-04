#!/usr/bin/env python3
"""Generate / refresh tests/contracts/*.yaml from Lua inventory + known tiers."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LUA = ROOT / "addon" / "vrmod-x64" / "lua"
CONTRACTS = ROOT / "tests" / "contracts"

# Symbols with dedicated offline unit tests (must stay pure/seam + tested)
# G21: keep this map current when adding pure helpers + unit tests.
PURE_TESTED = {
    "vrmod.utils.VecAlmostEqual": "util.math.vec_almost_equal",
    "vrmod.utils.LengthSqr": "util.math.length_sqr",
    "vrmod.utils.SubVec": "util.math.sub_add_mul",
    "vrmod.utils.AddVec": "util.math.sub_add_mul",
    "vrmod.utils.MulVec": "util.math.sub_add_mul",
    "vrmod.utils.SmoothVector": "util.smooth.vector",
    "vrmod.utils.SmoothValue": "util.smooth.value_number",
    "vrmod.utils.SmoothAngle": "util.smooth.vector",
    "vrmod.utils.IsFloorOrCeilingNormal": "util.collisions.floor_not_wall",
    "vrmod.utils.ParseColor": "util.color.parse_rgba",
    "vrmod.utils.TryParseColor": "util.color.try_parse",
    "vrmod.utils.FormatColor": "util.color.parse_bad",
    "vrmod.utils.FingerDigitIndex": "util.fingers.digit_index",
    "vrmod.utils.LerpFingerAngle": "util.fingers.curl_lerp_unit",
    "vrmod.utils.ApplyFingerCurl": "util.fingers.apply_curl",
    "vrmod.utils.AutoScaleHeight": "util.calib.autoscale_668",
    "vrmod.utils.AutoSeatedOffset": "util.calib.seated_offset",
    "vrmod.utils.IsSettingsRowKind": "util.settings.kinds_complete",
    "vrmod.utils.ComputeDesktopCrop": "util.rendering.desktop_crop",
    "vrmod.utils.ComputeSubmitBounds": "util.rendering.submit_bounds",
    "vrmod.utils.AdjustFOV": "util.rendering.adjust_fov",
    "vrmod.utils.GlideSeatIsDriver": "util.glide.seat_and_steer_sot",
    "vrmod.utils.GlidePreferStickSteer": "util.glide.seat_and_steer_sot",
    "vrmod.utils.GlideSteerSourceLabel": "util.glide.seat_and_steer_sot",
    "vrmod.utils.Glide_StatusLabel": "util.glide.seat_and_steer_sot",
    "vrmod.utils.Glide_HmdExpect": "util.glide.seat_and_steer_sot",
    "vrmod.utils.Glide_ShouldToastEnter": "util.glide.seat_and_steer_sot",
    "vrmod.utils.Glide_EnterToast": "util.glide.seat_and_steer_sot",
    "vrmod.utils.AngAlmostEqual": "util.math.vec_almost_equal",
    "vrmod.utils.LerpAngleWrap": "util.fingers.curl_lerp_unit",
    # G03 pure STAGE pack parse + toast (no origin apply)
    "vrmod.utils.StagePack_NormalizeSpace": "util.stage_pack.parse_and_hint",
    "vrmod.utils.StagePack_Parse": "util.stage_pack.parse_and_hint",
    "vrmod.utils.StagePack_IsUsable": "util.stage_pack.parse_and_hint",
    "vrmod.utils.StagePack_ToastHint": "util.stage_pack.parse_and_hint",
    "vrmod.utils.StagePack_ApplyDecision": "util.stage_pack.parse_and_hint",
    "vrmod.utils.StagePack_ApplyToast": "util.stage_pack.parse_and_hint",
    "vrmod.utils.StagePack_ComputeApplyPlan": "util.stage_pack.parse_and_hint",
    "vrmod.utils.StagePack_PlanToast": "util.stage_pack.parse_and_hint",
    "vrmod.utils.StagePack_MutationsFromPlan": "util.stage_pack.parse_and_hint",
    "vrmod.utils.StagePack_AllowApplyFromFlags": "util.stage_pack.parse_and_hint",
    "vrmod.utils.StagePack_ShouldExecutePlan": "util.stage_pack.parse_and_hint",
    "vrmod.utils.StagePack_ExecuteMutations": "util.stage_pack.parse_and_hint",
    "vrmod.utils.StagePack_ExecuteToast": "util.stage_pack.parse_and_hint",
    "vrmod.utils.StagePack_HmdExpect": "util.stage_pack.parse_and_hint",
    "vrmod.utils.StagePack_HeightJumpRiskIsBad": "util.stage_pack.parse_and_hint",
    # G17 pure mat_queue pin law (Cube pin 1; never VR-write 2)
    "vrmod.utils.MatQueueLaw_CubePin": "util.mat_queue_law.pin_g17",
    "vrmod.utils.MatQueueLaw_ClampRead": "util.mat_queue_law.pin_g17",
    "vrmod.utils.MatQueueLaw_ShouldWrite": "util.mat_queue_law.pin_g17",
    "vrmod.utils.MatQueueLaw_AllowDualEye": "util.mat_queue_law.pin_g17",
    "vrmod.utils.MatQueueLaw_Decide": "util.mat_queue_law.pin_g17",
    "vrmod.utils.MatQueueLaw_StatusLabel": "util.mat_queue_law.pin_g17",
    "vrmod.utils.MatQueueLaw_HmdExpect": "util.mat_queue_law.pin_g17",
    "vrmod.utils.MatQueueLaw_IsWriteRisk": "util.mat_queue_law.pin_g17",
    # G19 pure submit path law (dual OUT RGBA8; never eng IN / virgin OUT)
    "vrmod.utils.SubmitLaw_PreferTextureKind": "util.submit_law.path_g19",
    "vrmod.utils.SubmitLaw_AllowSubmitEngIn": "util.submit_law.path_g19",
    "vrmod.utils.SubmitLaw_AllowVirginOut": "util.submit_law.path_g19",
    "vrmod.utils.SubmitLaw_RequireBlitBeforeSubmit": "util.submit_law.path_g19",
    "vrmod.utils.SubmitLaw_AllowCollect": "util.submit_law.path_g19",
    "vrmod.utils.SubmitLaw_Decide": "util.submit_law.path_g19",
    "vrmod.utils.SubmitLaw_StatusLabel": "util.submit_law.path_g19",
    "vrmod.utils.SubmitLaw_HmdExpect": "util.submit_law.path_g19",
    "vrmod.utils.SubmitLaw_IsEngInRisk": "util.submit_law.path_g19",
    # G27 pure engine blacklist law (never call blocked convars / W2)
    "vrmod.utils.EngineBlacklist_NormalizeName": "util.engine_blacklist_law.never_call_g27",
    "vrmod.utils.EngineBlacklist_IsBlocked": "util.engine_blacklist_law.never_call_g27",
    "vrmod.utils.EngineBlacklist_IsLifecycleBan": "util.engine_blacklist_law.never_call_g27",
    "vrmod.utils.EngineBlacklist_AllowWrite": "util.engine_blacklist_law.never_call_g27",
    "vrmod.utils.EngineBlacklist_AllowRunConsoleCommand": "util.engine_blacklist_law.never_call_g27",
    "vrmod.utils.EngineBlacklist_FilterMap": "util.engine_blacklist_law.never_call_g27",
    "vrmod.utils.EngineBlacklist_BlockedNames": "util.engine_blacklist_law.never_call_g27",
    "vrmod.utils.EngineBlacklist_LifecycleNames": "util.engine_blacklist_law.never_call_g27",
    "vrmod.utils.EngineBlacklist_Decide": "util.engine_blacklist_law.never_call_g27",
    "vrmod.utils.EngineBlacklist_StatusLabel": "util.engine_blacklist_law.never_call_g27",
    "vrmod.utils.EngineBlacklist_HmdExpect": "util.engine_blacklist_law.never_call_g27",
    "vrmod.utils.EngineBlacklist_IsWriteRisk": "util.engine_blacklist_law.never_call_g27",
    # G26 pure menu thrash / QM dedupe law (VRClimb id collapse)
    "vrmod.utils.MenuLaw_NormalizeName": "util.menu_law.dedupe_g26",
    "vrmod.utils.MenuLaw_StableKey": "util.menu_law.dedupe_g26",
    "vrmod.utils.MenuLaw_CanonicalClimbId": "util.menu_law.dedupe_g26",
    "vrmod.utils.MenuLaw_IsClimbName": "util.menu_law.dedupe_g26",
    "vrmod.utils.MenuLaw_ItemsMatch": "util.menu_law.dedupe_g26",
    "vrmod.utils.MenuLaw_DedupList": "util.menu_law.dedupe_g26",
    "vrmod.utils.MenuLaw_Decide": "util.menu_law.dedupe_g26",
    "vrmod.utils.MenuLaw_StatusLabel": "util.menu_law.dedupe_g26",
    "vrmod.utils.MenuLaw_HmdExpect": "util.menu_law.dedupe_g26",
    "vrmod.utils.MenuLaw_IsThrashRisk": "util.menu_law.dedupe_g26",
    # G25 pure pose SoT law (one energy path; no dual-truth pose/angvel forks)
    "vrmod.utils.PoseSoT_PipelineSteps": "util.pose_sot_law.single_path_g25",
    "vrmod.utils.PoseSoT_PublicSource": "util.pose_sot_law.single_path_g25",
    "vrmod.utils.PoseSoT_RawSource": "util.pose_sot_law.single_path_g25",
    "vrmod.utils.PoseSoT_AllowSecondAngvelSoT": "util.pose_sot_law.single_path_g25",
    "vrmod.utils.PoseSoT_AllowDualPublicPose": "util.pose_sot_law.single_path_g25",
    "vrmod.utils.PoseSoT_GunReadsSource": "util.pose_sot_law.single_path_g25",
    "vrmod.utils.PoseSoT_HeadVelSource": "util.pose_sot_law.single_path_g25",
    "vrmod.utils.PoseSoT_NormalizeSource": "util.pose_sot_law.single_path_g25",
    "vrmod.utils.PoseSoT_Decide": "util.pose_sot_law.single_path_g25",
    "vrmod.utils.PoseSoT_StatusLabel": "util.pose_sot_law.single_path_g25",
    "vrmod.utils.PoseSoT_HmdExpect": "util.pose_sot_law.single_path_g25",
    "vrmod.utils.PoseSoT_IsForkRisk": "util.pose_sot_law.single_path_g25",
    # G16 pure laser / menu primary-click sacred law
    "vrmod.utils.LaserLaw_PrimaryHandFromInt": "util.laser_law.sacred_g16",
    "vrmod.utils.LaserLaw_SecondaryHand": "util.laser_law.sacred_g16",
    "vrmod.utils.LaserLaw_IsMenuPrimaryClick": "util.laser_law.sacred_g16",
    "vrmod.utils.LaserLaw_IsMenuSecondaryClick": "util.laser_law.sacred_g16",
    "vrmod.utils.LaserLaw_IsMenuCloseAction": "util.laser_law.sacred_g16",
    "vrmod.utils.LaserLaw_QmAttachModeFromInt": "util.laser_law.sacred_g16",
    "vrmod.utils.LaserLaw_ShouldSolveFocus": "util.laser_law.sacred_g16",
    "vrmod.utils.LaserLaw_HmdExpect": "util.laser_law.sacred_g16",
    "vrmod.utils.LaserLaw_StatusLabel": "util.laser_law.sacred_g16",
    # G15 pure HUD composite law (PROPHECY — no black wall of the Real)
    "vrmod.utils.HudLaw_ClampClearAlpha": "util.hud_law.composite_g15",
    "vrmod.utils.HudLaw_Decide": "util.hud_law.composite_g15",
    "vrmod.utils.HudLaw_MaterialFlags": "util.hud_law.composite_g15",
    "vrmod.utils.HudLaw_StatusLabel": "util.hud_law.composite_g15",
    "vrmod.utils.HudLaw_HmdExpect": "util.hud_law.composite_g15",
    "vrmod.utils.HudLaw_IsBlackSlabRisk": "util.hud_law.composite_g15",
    # G05 pure stereo-load policy (never dual under mat_queue≥2)
    "vrmod.utils.StereoLoadPolicy": "util.stereo_load.policy_g05",
    "vrmod.utils.ShouldPaintStereoThisFrame": "util.stereo_load.policy_g05",
    "vrmod.utils.StereoLoadToastHint": "util.stereo_load.policy_g05",
    "vrmod.utils.StereoLoad_IsLoading": "util.stereo_load.policy_g05",
    "vrmod.utils.StereoLoad_StatusLabel": "util.stereo_load.policy_g05",
    "vrmod.utils.StereoLoad_ShouldToast": "util.stereo_load.policy_g05",
    "vrmod.utils.StereoLoad_HmdExpect": "util.stereo_load.policy_g05",
    "vrmod.utils.StereoLoad_FlashRiskIsBad": "util.stereo_load.policy_g05",
    # G13 pure return-to-Cube reverse protocol
    "vrmod.utils.CubeReturn_NormalizePhase": "util.cube_return.protocol_g13",
    "vrmod.utils.CubeReturn_Format": "util.cube_return.protocol_g13",
    "vrmod.utils.CubeReturn_Parse": "util.cube_return.protocol_g13",
    "vrmod.utils.CubeReturn_PhaseLabel": "util.cube_return.protocol_g13",
    "vrmod.utils.CubeReturn_ShouldNotifyCube": "util.cube_return.protocol_g13",
    "vrmod.utils.CubeReturn_DetailForPhase": "util.cube_return.protocol_g13",
    # G04 pure warm map-attach + careful changelevel plan (default off)
    "vrmod.utils.WarmAttach_NormalizeMap": "util.warm_attach.decide_g04",
    "vrmod.utils.WarmAttach_Parse": "util.warm_attach.decide_g04",
    "vrmod.utils.WarmAttach_Decide": "util.warm_attach.decide_g04",
    "vrmod.utils.WarmAttach_Toast": "util.warm_attach.decide_g04",
    "vrmod.utils.WarmAttach_MapTokenOk": "util.warm_attach.decide_g04",
    "vrmod.utils.WarmAttach_AllowChangelevelFromFlags": "util.warm_attach.decide_g04",
    "vrmod.utils.WarmAttach_ChangelevelPlan": "util.warm_attach.decide_g04",
    "vrmod.utils.WarmAttach_ChangelevelCmd": "util.warm_attach.decide_g04",
    "vrmod.utils.WarmAttach_ShouldExecuteChangelevel": "util.warm_attach.decide_g04",
    "vrmod.utils.WarmAttach_ExecuteChangelevel": "util.warm_attach.decide_g04",
    "vrmod.utils.WarmAttach_ExecuteToast": "util.warm_attach.decide_g04",
    "vrmod.utils.WarmAttach_HmdExpect": "util.warm_attach.decide_g04",
    "vrmod.AddInGameMenuItem": "api.menu.dedupe_name",
    "vrmod.DedupInGameMenuItems": "api.menu.dedup_function",
    "vrmod.RemoveInGameMenuItem": "api.menu.dedupe_name",
    "vrmod.GetVersion": "api.smoke.get_version",
    # G10 pure decision helper (loaded via sh_experience.lua)
    "vrmod.Experience_ShouldRunFromState": "util.experience.g10_wrapper_plus_cal_skips",
}

# Engine / model / filesystem heavy — never auto-promote to pure-pending.
SEAM_FORCE = {
    "vrmod.utils.ComputePhysicsParams",
}

# Thin getters → smoke suite
API_SMOKE_PREFIXES = (
    "GetHMD", "GetLeftHand", "GetRightHand", "GetLeftFoot", "GetRightFoot",
    "GetWaist", "GetOrigin", "GetEye", "GetRaw", "GetDefault", "GetModule",
    "GetInput", "GetHeld", "GetTracked", "GetStartup", "GetConvars",
    "IsPlayer", "ModuleSupports", "UsingEmpty",
)


def scan_functions() -> tuple[list[tuple[str, str]], list[tuple[str, str]]]:
    utils, api = [], []
    for p in LUA.rglob("*.lua"):
        text = p.read_text(errors="ignore")
        rel = str(p.relative_to(ROOT))
        for m in re.finditer(r"function\s+(vrmod\.utils\.\w+)\s*\(", text):
            utils.append((m.group(1), rel))
        for m in re.finditer(r"function\s+(vrmod\.\w+)\s*\(", text):
            name = m.group(1)
            # only vrmod.Foo (not vrmod.pkg.Foo)
            if name.count(".") == 1:
                api.append((name, rel))
    return sorted(set(utils)), sorted(set(api))


def tier_for(name: str) -> tuple[str, list[str], str | None]:
    if name in PURE_TESTED:
        return "pure", [PURE_TESTED[name]], None
    if name in SEAM_FORCE:
        return "seam", ["pending"], "engine/model I/O — not offline-pure"
    short = name.split(".")[-1]
    if name.startswith("vrmod.utils."):
        # default utils: classify seam unless known pure-ish name
        # Note: avoid bare "Compute" — ComputePhysicsParams is engine-bound (SEAM_FORCE).
        pure_hints = (
            "Almost", "Lerp", "Smooth", "Length", "Sub", "Add", "Mul",
            "Parse", "Format", "Finger", "Auto", "IsFloor", "IsSettings",
            "ComputeDesktop", "ComputeSubmit", "AdjustFOV",
        )
        if any(h in short for h in pure_hints):
            return "pure", ["pending"], "needs unit test"
        return "seam", ["pending"], "engine or state"
    # API
    if name in PURE_TESTED:
        return "pure", [PURE_TESTED[name]], None
    if any(short.startswith(p.replace("Get", "").replace("Is", "").replace("Module", "")) or short.startswith(p) for p in API_SMOKE_PREFIXES):
        return "seam", [f"api.smoke.{short}"], "thin getter / smoke"
    if short.startswith("Set") or short.startswith("Add") or short.startswith("Start") or short.startswith("Stop"):
        return "seam", ["pending"], "mutator"
    return "integration", ["pending"], "needs scenario or waive"


def emit_yaml(path: Path, symbols: list[dict]) -> None:
    lines = ["# Auto-generated by scripts/gen_contracts.py — re-run after API changes", "version: 1", "symbols:"]
    for s in symbols:
        lines.append(f"  - id: {s['id']}")
        lines.append(f"    file: {s['file']}")
        lines.append(f"    tier: {s['tier']}")
        tests = ", ".join(s["tests"])
        lines.append(f"    tests: [{tests}]")
        if s.get("notes"):
            notes = s["notes"].replace('"', "'")
            lines.append(f'    notes: "{notes}"')
    path.write_text("\n".join(lines) + "\n")


def main() -> None:
    CONTRACTS.mkdir(parents=True, exist_ok=True)
    utils, api = scan_functions()
    inv = {"utils": utils, "api": api}
    (CONTRACTS / "_inventory.json").write_text(json.dumps(inv, indent=2))

    util_syms = []
    for name, file in utils:
        tier, tests, notes = tier_for(name)
        util_syms.append({"id": name, "file": file, "tier": tier, "tests": tests, "notes": notes})
    emit_yaml(CONTRACTS / "utils.yaml", util_syms)

    api_syms = []
    for name, file in api:
        tier, tests, notes = tier_for(name)
        api_syms.append({"id": name, "file": file, "tier": tier, "tests": tests, "notes": notes})
    emit_yaml(CONTRACTS / "api.yaml", api_syms)

    # C++ module exports (hand-maintained core list + scan lua_interface)
    cpp_exports = [
        "GetVersion", "Init", "Shutdown", "GetPoses", "GetActions", "UpdatePosesAndActions",
        "ShareTextureBegin", "ShareTextureFinish", "GetDisplayInfo", "SubmitSharedTexture",
        "KeyboardOpen", "KeyboardClose", "KeyboardIsOpen", "KeyboardGetText", "KeyboardSetText",
        "KeyboardAppend", "KeyboardBackspace", "KeyboardHitTest", "KeyboardPointerClick",
        "VirtualDisplayCreate", "VirtualDisplayDestroy", "VirtualDisplayGetInfo",
        "VirtualDisplayCaptureWindow", "VirtualDisplayIsSupported",
    ]
    lines = [
        "# C++ / Lua module export contracts",
        "version: 1",
        "exports:",
    ]
    for e in cpp_exports:
        lines.append(f"  - id: VRMOD_{e}")
        lines.append("    tier: seam")
        lines.append("    tests: [cpp.exports.registry]")
    (CONTRACTS / "cpp_module.yaml").write_text("\n".join(lines) + "\n")

    launcher = """# Cube native launcher contracts
version: 1
symbols:
  - id: math3d.Normalize
    file: native_launcher/src/math3d.hpp
    tier: pure
    tests: [launcher.math3d.normalize]
  - id: math3d.Dot
    file: native_launcher/src/math3d.hpp
    tier: pure
    tests: [launcher.math3d.dot]
  - id: desktopView.cycle
    file: native_launcher/src/ui_panel.cpp
    tier: pure
    tests: [launcher.desktop.cycle_1_to_4]
    notes: "1=none 2=left 3=right 4=follow-cam"
  - id: handoff.phase_detail
    file: native_launcher/src/gmod_spawn.hpp
    tier: pure
    tests: [launcher.handoff.detail_known_phases]
  - id: handoff.phase_progress
    file: native_launcher/src/gmod_spawn.hpp
    tier: pure
    tests: [launcher.handoff.progress_monotone]
  - id: last_play.roundtrip
    file: native_launcher/src/last_play.hpp
    tier: pure
    tests: [launcher.last_play.roundtrip]
  - id: last_play.desktopview_clamp
    file: native_launcher/src/last_play.hpp
    tier: pure
    tests: [launcher.last_play.clamps_desktopview]
"""
    (CONTRACTS / "launcher.yaml").write_text(launcher)

    pure_pending = sum(1 for s in util_syms + api_syms if s["tier"] == "pure" and s["tests"] == ["pending"])
    pure_tested = sum(1 for s in util_syms + api_syms if s["tier"] == "pure" and s["tests"] != ["pending"])
    print(f"Wrote contracts: utils={len(util_syms)} api={len(api_syms)} cpp={len(cpp_exports)}")
    print(f"Pure utils/api: {pure_tested} tested, {pure_pending} pending unit tests")


if __name__ == "__main__":
    main()
