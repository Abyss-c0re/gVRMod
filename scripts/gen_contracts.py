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
    "vrmod.utils.SubmitBounds_MirrorLeftToBoth": "util.rendering.submit_bounds",
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
    # G31 pure bindings self-heal law (W6 force-rewrite + honest toast)
    "vrmod.utils.BindingsLaw_ManifestRelPath": "util.bindings_law.self_heal_g31",
    "vrmod.utils.BindingsLaw_DataDir": "util.bindings_law.self_heal_g31",
    "vrmod.utils.BindingsLaw_ForceRewriteOnStart": "util.bindings_law.self_heal_g31",
    "vrmod.utils.BindingsLaw_MaxSetAttempts": "util.bindings_law.self_heal_g31",
    "vrmod.utils.BindingsLaw_ShouldRetryAfterFail": "util.bindings_law.self_heal_g31",
    "vrmod.utils.BindingsLaw_AbortVrOnFail": "util.bindings_law.self_heal_g31",
    "vrmod.utils.BindingsLaw_RequireToastOnFail": "util.bindings_law.self_heal_g31",
    "vrmod.utils.BindingsLaw_ToastSeconds": "util.bindings_law.self_heal_g31",
    "vrmod.utils.BindingsLaw_ToastMessage": "util.bindings_law.self_heal_g31",
    "vrmod.utils.BindingsLaw_ErrorOverlayText": "util.bindings_law.self_heal_g31",
    "vrmod.utils.BindingsLaw_OverlayClearSeconds": "util.bindings_law.self_heal_g31",
    "vrmod.utils.BindingsLaw_Decide": "util.bindings_law.self_heal_g31",
    "vrmod.utils.BindingsLaw_StatusLabel": "util.bindings_law.self_heal_g31",
    "vrmod.utils.BindingsLaw_HmdExpect": "util.bindings_law.self_heal_g31",
    "vrmod.utils.BindingsLaw_IsSilentFailRisk": "util.bindings_law.self_heal_g31",
    # G32 pure stereo self-test / ShareTexture toast law (W7)
    "vrmod.utils.StereoSelfTest_DelaySeconds": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_ToastSeconds": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_ShareHintSeconds": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_RequireToastOnShareFail": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_RequireToastOnNoHmd": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_AbortVrOnFail": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_ShareBeginToast": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_ShareFinishToast": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_NoHmdToast": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_UnhealthyShareToast": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_ShouldToastShareBegin": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_ShouldToastShareFinish": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_ShareOk": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_ShouldToastNoHmd": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_ShouldToastUnhealthyShare": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_Decide": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_StatusLabel": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_HmdExpect": "util.stereo_selftest_law.w7_toast_g32",
    "vrmod.utils.StereoSelfTest_IsSilentFailRisk": "util.stereo_selftest_law.w7_toast_g32",
    # G33 pure swap-eyes content-only law (W4; no dual pose)
    "vrmod.utils.SwapEyesLaw_CubeDefault": "util.swap_eyes_law.content_only_g33",
    "vrmod.utils.SwapEyesLaw_FromAny": "util.swap_eyes_law.content_only_g33",
    "vrmod.utils.SwapEyesLaw_AllowDualPoseFork": "util.swap_eyes_law.content_only_g33",
    "vrmod.utils.SwapEyesLaw_PreserveIpdFov": "util.swap_eyes_law.content_only_g33",
    "vrmod.utils.SwapEyesLaw_ResolveSbsHalves": "util.swap_eyes_law.content_only_g33",
    "vrmod.utils.SwapEyesLaw_LogicalLeftHalf": "util.swap_eyes_law.content_only_g33",
    "vrmod.utils.SwapEyesLaw_LogicalRightHalf": "util.swap_eyes_law.content_only_g33",
    "vrmod.utils.SwapEyesLaw_Decide": "util.swap_eyes_law.content_only_g33",
    "vrmod.utils.SwapEyesLaw_StatusLabel": "util.swap_eyes_law.content_only_g33",
    "vrmod.utils.SwapEyesLaw_HmdExpect": "util.swap_eyes_law.content_only_g33",
    "vrmod.utils.SwapEyesLaw_IsForkRisk": "util.swap_eyes_law.content_only_g33",
    # G34 pure fly-away / origin snap + action set law (W12)
    "vrmod.utils.FlyAwayLaw_ActionSetMain": "util.flyaway_law.origin_action_g34",
    "vrmod.utils.FlyAwayLaw_ActionSetDriving": "util.flyaway_law.origin_action_g34",
    "vrmod.utils.FlyAwayLaw_ActionSetBase": "util.flyaway_law.origin_action_g34",
    "vrmod.utils.FlyAwayLaw_ResolveActionSet": "util.flyaway_law.origin_action_g34",
    "vrmod.utils.FlyAwayLaw_InsaneVerticalVel": "util.flyaway_law.origin_action_g34",
    "vrmod.utils.FlyAwayLaw_InsaneHorizontalVel": "util.flyaway_law.origin_action_g34",
    "vrmod.utils.FlyAwayLaw_StartWindowSec": "util.flyaway_law.origin_action_g34",
    "vrmod.utils.FlyAwayLaw_IsInsaneVertical": "util.flyaway_law.origin_action_g34",
    "vrmod.utils.FlyAwayLaw_IsInsaneHorizontal": "util.flyaway_law.origin_action_g34",
    "vrmod.utils.FlyAwayLaw_ShouldSnapOrigin": "util.flyaway_law.origin_action_g34",
    "vrmod.utils.FlyAwayLaw_Decide": "util.flyaway_law.origin_action_g34",
    "vrmod.utils.FlyAwayLaw_StatusLabel": "util.flyaway_law.origin_action_g34",
    "vrmod.utils.FlyAwayLaw_HmdExpect": "util.flyaway_law.origin_action_g34",
    "vrmod.utils.FlyAwayLaw_IsFlyAwayRisk": "util.flyaway_law.origin_action_g34",
    # G35 pure viewscale fisheye law (W8)
    "vrmod.utils.ViewScaleLaw_CubeDefault": "util.viewscale_law.fisheye_g35",
    "vrmod.utils.ViewScaleLaw_Min": "util.viewscale_law.fisheye_g35",
    "vrmod.utils.ViewScaleLaw_Max": "util.viewscale_law.fisheye_g35",
    "vrmod.utils.ViewScaleLaw_ComfortMin": "util.viewscale_law.fisheye_g35",
    "vrmod.utils.ViewScaleLaw_ComfortMax": "util.viewscale_law.fisheye_g35",
    "vrmod.utils.ViewScaleLaw_Clamp": "util.viewscale_law.fisheye_g35",
    "vrmod.utils.ViewScaleLaw_IsFisheyeRisk": "util.viewscale_law.fisheye_g35",
    "vrmod.utils.ViewScaleLaw_PreferHmdProjection": "util.viewscale_law.fisheye_g35",
    "vrmod.utils.ViewScaleLaw_IsProjectionLive": "util.viewscale_law.fisheye_g35",
    "vrmod.utils.ViewScaleLaw_Decide": "util.viewscale_law.fisheye_g35",
    "vrmod.utils.ViewScaleLaw_StatusLabel": "util.viewscale_law.fisheye_g35",
    "vrmod.utils.ViewScaleLaw_HmdExpect": "util.viewscale_law.fisheye_g35",
    "vrmod.utils.ViewScaleLaw_IsFisheyeDecision": "util.viewscale_law.fisheye_g35",
    # G36 pure FOV/Z soft-refresh law (W5; no mid-frame UV fight)
    "vrmod.utils.FovZLaw_FovMin": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_FovMax": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_FovComfortMin": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_FovComfortMax": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_ZnearMin": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_ZnearMax": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_ZnearDefault": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_ClampFovScale": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_ClampZnear": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_IsFovExtreme": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_AllowMidFrameUvFight": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_AllowLiveFovWithoutSoftRefresh": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_NormalizeCvar": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_RefreshKind": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_IsBorderCvar": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_IsFovProfileCvar": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_IsSessionCvar": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_PreferBorderGuideOverZSpam": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_Decide": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_StatusLabel": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_HmdExpect": "util.fovz_law.soft_refresh_g36",
    "vrmod.utils.FovZLaw_IsJitterRisk": "util.fovz_law.soft_refresh_g36",
    # G37 pure hand vs bullet filter law (W9)
    "vrmod.utils.HandBulletLaw_HandDamageScale": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_HeadDamageScale": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_ProxySolidToWorld": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_AllowGrabContact": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_PreventSelfMeleeOnHand": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_AbsorbNonBulletOnProxy": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_IsBulletDamageType": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_NormalizePart": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_IsHandPart": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_ShouldAbsorbOnProxy": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_RedirectScale": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_ShouldDropOnHandBullet": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_Decide": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_StatusLabel": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_HmdExpect": "util.hand_bullet_law.filter_g37",
    "vrmod.utils.HandBulletLaw_IsBlockRisk": "util.hand_bullet_law.filter_g37",
    # G38 pure worldmodel single-path law (W10; no dual ghost)
    "vrmod.utils.WorldModelLaw_CubePreferFloatingHands": "util.worldmodel_law.single_path_g38",
    "vrmod.utils.WorldModelLaw_AllowDualGhost": "util.worldmodel_law.single_path_g38",
    "vrmod.utils.WorldModelLaw_AllowDualWeaponDraw": "util.worldmodel_law.single_path_g38",
    "vrmod.utils.WorldModelLaw_FromBool": "util.worldmodel_law.single_path_g38",
    "vrmod.utils.WorldModelLaw_ResolvePath": "util.worldmodel_law.single_path_g38",
    "vrmod.utils.WorldModelLaw_Sanitize": "util.worldmodel_law.single_path_g38",
    "vrmod.utils.WorldModelLaw_Decide": "util.worldmodel_law.single_path_g38",
    "vrmod.utils.WorldModelLaw_StatusLabel": "util.worldmodel_law.single_path_g38",
    "vrmod.utils.WorldModelLaw_HmdExpect": "util.worldmodel_law.single_path_g38",
    "vrmod.utils.WorldModelLaw_IsDualRisk": "util.worldmodel_law.single_path_g38",
    # G39 pure VR_Init human error surface (W11)
    "vrmod.utils.InitLaw_ModuleZipUrl": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_MinModuleVersion": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_CrispModuleVersion": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_KnownCodeMessage": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_ParseCode": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_IsNoHmdHint": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_IsRuntimeHint": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_IsModuleHint": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_Humanize": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_ToastSeconds": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_RequireToastOnFail": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_SilentFailForbidden": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_Decide": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_StatusLabel": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_HmdExpect": "util.init_law.surface_g39",
    "vrmod.utils.InitLaw_IsSilentFailRisk": "util.init_law.surface_g39",
    # G40 pure Vision/border fill law (W1; guided scale→V→H, no slider maze)
    "vrmod.utils.BorderLaw_ProfilePath": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_DefaultScale": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_DefaultVertical": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_DefaultHorizontal": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_ScaleMin": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_ScaleMax": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_OffsetMin": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_OffsetMax": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_ComfortScaleMin": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_ComfortScaleMax": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_ComfortOffsetAbsMax": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_ScaleStep": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_OffsetStep": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_StepIds": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_IsGuidedPathOnly": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_PreferGuideOverZSpam": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_RequireRenderOffsetOnGuide": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_ClampScale": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_ClampOffset": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_IsBleedRisk": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_GuideBaseline": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_Sanitize": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_Decide": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_StatusLabel": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_HmdExpect": "util.border_law.fill_g40",
    "vrmod.utils.BorderLaw_IsBleedDecision": "util.border_law.fill_g40",
    # G41 pure HMD walk inventory (manual smoke backlog; never offline claim)
    "vrmod.utils.HmdWalk_Catalog": "util.hmd_walk_law.inventory_g41",
    "vrmod.utils.HmdWalk_Count": "util.hmd_walk_law.inventory_g41",
    "vrmod.utils.HmdWalk_FindById": "util.hmd_walk_law.inventory_g41",
    "vrmod.utils.HmdWalk_PriorityIsP0": "util.hmd_walk_law.inventory_g41",
    "vrmod.utils.HmdWalk_CountP0": "util.hmd_walk_law.inventory_g41",
    "vrmod.utils.HmdWalk_PreferredNextIds": "util.hmd_walk_law.inventory_g41",
    "vrmod.utils.HmdWalk_FormatLine": "util.hmd_walk_law.inventory_g41",
    "vrmod.utils.HmdWalk_FormatReport": "util.hmd_walk_law.inventory_g41",
    "vrmod.utils.HmdWalk_CollectLive": "util.hmd_walk_law.inventory_g41",
    "vrmod.utils.HmdWalk_FormatLive": "util.hmd_walk_law.inventory_g41",
    "vrmod.utils.HmdWalk_NeverClaimFromOfflineAlone": "util.hmd_walk_law.inventory_g41",
    "vrmod.utils.HmdWalk_Decide": "util.hmd_walk_law.inventory_g41",
    "vrmod.utils.HmdWalk_StatusLabel": "util.hmd_walk_law.inventory_g41",
    "vrmod.utils.HmdWalk_HmdExpect": "util.hmd_walk_law.inventory_g41",
    # G42 pure hands-stuck unstick law (ship bar)
    "vrmod.utils.HandStuckLaw_TrackCollapseThresholdSqr": "util.hand_stuck_law.unstick_g42",
    "vrmod.utils.HandStuckLaw_RawSeparatedThresholdSqr": "util.hand_stuck_law.unstick_g42",
    "vrmod.utils.HandStuckLaw_SkipUnstickWhenForegrip": "util.hand_stuck_law.unstick_g42",
    "vrmod.utils.HandStuckLaw_HealIdentityAlways": "util.hand_stuck_law.unstick_g42",
    "vrmod.utils.HandStuckLaw_PreferRawWhenCollapsed": "util.hand_stuck_law.unstick_g42",
    "vrmod.utils.HandStuckLaw_ShouldSplitIdentity": "util.hand_stuck_law.unstick_g42",
    "vrmod.utils.HandStuckLaw_ShouldUnstickFromRaw": "util.hand_stuck_law.unstick_g42",
    "vrmod.utils.HandStuckLaw_DistSqr": "util.hand_stuck_law.unstick_g42",
    "vrmod.utils.HandStuckLaw_Decide": "util.hand_stuck_law.unstick_g42",
    "vrmod.utils.HandStuckLaw_StatusLabel": "util.hand_stuck_law.unstick_g42",
    "vrmod.utils.HandStuckLaw_HmdExpect": "util.hand_stuck_law.unstick_g42",
    "vrmod.utils.HandStuckLaw_IsStuckRisk": "util.hand_stuck_law.unstick_g42",
    # G43 pure nested RT / menu-open crash law
    "vrmod.utils.NestedRtLaw_AllowNestUnderStereo": "util.nested_rt_law.menu_open_g43",
    "vrmod.utils.NestedRtLaw_RequirePopAfterMenuPaint": "util.nested_rt_law.menu_open_g43",
    "vrmod.utils.NestedRtLaw_HudCaptureBeforeStereoPush": "util.nested_rt_law.menu_open_g43",
    "vrmod.utils.NestedRtLaw_AllowDrawMonitorsInVrRt": "util.nested_rt_law.menu_open_g43",
    "vrmod.utils.NestedRtLaw_AllowPortalRenderViewInStereo": "util.nested_rt_law.menu_open_g43",
    "vrmod.utils.NestedRtLaw_AllowMenuRtPaint": "util.nested_rt_law.menu_open_g43",
    "vrmod.utils.NestedRtLaw_AllowHudCapture": "util.nested_rt_law.menu_open_g43",
    "vrmod.utils.NestedRtLaw_AllowNestedWorldCapture": "util.nested_rt_law.menu_open_g43",
    "vrmod.utils.NestedRtLaw_DeferMenuWhenStereoActive": "util.nested_rt_law.menu_open_g43",
    "vrmod.utils.NestedRtLaw_Decide": "util.nested_rt_law.menu_open_g43",
    "vrmod.utils.NestedRtLaw_StatusLabel": "util.nested_rt_law.menu_open_g43",
    "vrmod.utils.NestedRtLaw_HmdExpect": "util.nested_rt_law.menu_open_g43",
    "vrmod.utils.NestedRtLaw_IsCrashRisk": "util.nested_rt_law.menu_open_g43",
    # G44 pure grab-end / drop-cooldown law
    "vrmod.utils.GrabEndLaw_CooldownSeconds": "util.grab_end_law.storm_g44",
    "vrmod.utils.GrabEndLaw_RequirePerHandCooldown": "util.grab_end_law.storm_g44",
    "vrmod.utils.GrabEndLaw_AllowClimbRewrite": "util.grab_end_law.storm_g44",
    "vrmod.utils.GrabEndLaw_NormalizeHand": "util.grab_end_law.storm_g44",
    "vrmod.utils.GrabEndLaw_ShouldStartCooldown": "util.grab_end_law.storm_g44",
    "vrmod.utils.GrabEndLaw_AllowPickup": "util.grab_end_law.storm_g44",
    "vrmod.utils.GrabEndLaw_IsStorm": "util.grab_end_law.storm_g44",
    "vrmod.utils.GrabEndLaw_PrimaryLeftPreservesPickup": "util.grab_end_law.storm_g44",
    "vrmod.utils.GrabEndLaw_MenuClickIsNotPickup": "util.grab_end_law.storm_g44",
    "vrmod.utils.GrabEndLaw_Decide": "util.grab_end_law.storm_g44",
    "vrmod.utils.GrabEndLaw_StatusLabel": "util.grab_end_law.storm_g44",
    "vrmod.utils.GrabEndLaw_HmdExpect": "util.grab_end_law.storm_g44",
    "vrmod.utils.GrabEndLaw_IsStormRisk": "util.grab_end_law.storm_g44",
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
    "vrmod.utils.LaserLaw_AllowLaserFromHand": "util.laser_law.sacred_g16",
    "vrmod.utils.LaserLaw_IsWrongHandPrimaryClick": "util.laser_law.sacred_g16",
    "vrmod.utils.LaserLaw_Decide": "util.laser_law.sacred_g16",
    "vrmod.utils.LaserLaw_IsStealRisk": "util.laser_law.sacred_g16",
    # G46 pure desktop mirror isolation (no live stereo RT after submit)
    "vrmod.utils.DesktopMirror_AllowSampleStereoRtAfterSubmit": "util.desktop_mirror_law.hmd_g46",
    "vrmod.utils.DesktopMirror_AllowEyeCropFromLiveRt": "util.desktop_mirror_law.hmd_g46",
    "vrmod.utils.DesktopMirror_PreferFollowPrivateRt": "util.desktop_mirror_law.hmd_g46",
    "vrmod.utils.DesktopMirror_PresentOnlyAfterSubmit": "util.desktop_mirror_law.hmd_g46",
    "vrmod.utils.DesktopMirror_IsEyeCropMode": "util.desktop_mirror_law.hmd_g46",
    "vrmod.utils.DesktopMirror_IsFollowMode": "util.desktop_mirror_law.hmd_g46",
    "vrmod.utils.DesktopMirror_IsNoneMode": "util.desktop_mirror_law.hmd_g46",
    "vrmod.utils.DesktopMirror_AllowPresent": "util.desktop_mirror_law.hmd_g46",
    "vrmod.utils.DesktopMirror_Decide": "util.desktop_mirror_law.hmd_g46",
    "vrmod.utils.DesktopMirror_StatusLabel": "util.desktop_mirror_law.hmd_g46",
    "vrmod.utils.DesktopMirror_HmdExpect": "util.desktop_mirror_law.hmd_g46",
    "vrmod.utils.DesktopMirror_IsBlackRisk": "util.desktop_mirror_law.hmd_g46",
    "vrmod.utils.DesktopMirror_PreferDesktopViewForHmd": "util.desktop_mirror_law.hmd_g46",
    # G47 pure false per-eye FBO guard (both FBOs; no color+depth dual)
    "vrmod.utils.FalsePerEyeLaw_RequireBothFbos": "util.false_per_eye_law.guard_g47",
    "vrmod.utils.FalsePerEyeLaw_RequireDistinctColorTex": "util.false_per_eye_law.guard_g47",
    "vrmod.utils.FalsePerEyeLaw_AllowColorDepthAsDual": "util.false_per_eye_law.guard_g47",
    "vrmod.utils.FalsePerEyeLaw_FallbackToSbsWhenInvalid": "util.false_per_eye_law.guard_g47",
    "vrmod.utils.FalsePerEyeLaw_IsLegalPair": "util.false_per_eye_law.guard_g47",
    "vrmod.utils.FalsePerEyeLaw_ResolvePath": "util.false_per_eye_law.guard_g47",
    "vrmod.utils.FalsePerEyeLaw_Decide": "util.false_per_eye_law.guard_g47",
    "vrmod.utils.FalsePerEyeLaw_StatusLabel": "util.false_per_eye_law.guard_g47",
    "vrmod.utils.FalsePerEyeLaw_HmdExpect": "util.false_per_eye_law.guard_g47",
    "vrmod.utils.FalsePerEyeLaw_IsBlackEyeRisk": "util.false_per_eye_law.guard_g47",
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
