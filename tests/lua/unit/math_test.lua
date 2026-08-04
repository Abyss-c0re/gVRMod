return function(H, env)
	local u = env.vrmod.utils

	H.TEST("util.math.length_sqr", function()
		H.assert_eq(u.LengthSqr(Vector(3, 4, 0)), 25)
		H.assert_eq(u.LengthSqr(nil), 0)
	end)

	H.TEST("util.math.vec_almost_equal", function()
		local a, b = Vector(1, 2, 3), Vector(1.01, 2, 3)
		H.assert_true(u.VecAlmostEqual(a, b, 0.2))
		H.assert_false(u.VecAlmostEqual(a, Vector(10, 0, 0), 0.05))
	end)

	H.TEST("util.math.sub_add_mul", function()
		local a, b = Vector(5, 5, 5), Vector(2, 1, 0)
		local s = u.SubVec(a, b)
		H.assert_vec_near(s, Vector(3, 4, 5))
		H.assert_vec_near(u.AddVec(b, s), a)
		H.assert_vec_near(u.MulVec(b, 2), Vector(4, 2, 0))
	end)

	H.TEST("util.smooth.vector", function()
		local c = u.SmoothVector(Vector(0, 0, 0), Vector(10, 0, 0), 0.5)
		H.assert_vec_near(c, Vector(5, 0, 0))
	end)

	H.TEST("util.smooth.value_number", function()
		H.assert_near(u.SmoothValue(0, 10, 0.5), 5, 1e-6)
		H.assert_eq(u.SmoothValue(1, 2, 0), 2)
	end)

	H.TEST("util.collisions.floor_not_wall", function()
		H.assert_true(u.IsFloorOrCeilingNormal(Vector(0, 0, 1)))
		H.assert_true(u.IsFloorOrCeilingNormal(Vector(0, 0, -0.9)))
		H.assert_false(u.IsFloorOrCeilingNormal(Vector(1, 0, 0)))
		H.assert_false(u.IsFloorOrCeilingNormal(Vector(0.2, 0, 0.3)))
	end)

	H.TEST("util.color.parse_rgba", function()
		local c = u.ParseColor("10,20,30,40")
		H.assert_eq(c.r, 10)
		H.assert_eq(c.g, 20)
		H.assert_eq(c.b, 30)
		H.assert_eq(c.a, 40)
		local c3 = u.ParseColor("1,2,3")
		H.assert_eq(c3.a, 255)
	end)

	H.TEST("util.color.parse_bad", function()
		local c = u.ParseColor("nope", Color(9, 8, 7, 6))
		H.assert_eq(c.r, 9)
		H.assert_eq(u.FormatColor(Color(1, 2, 3, 4)), "1,2,3,4")
	end)

	H.TEST("util.color.try_parse", function()
		H.assert_true(u.TryParseColor("nope") == nil)
		H.assert_true(u.TryParseColor("") == nil)
		local c = u.TryParseColor("196,30,58")
		H.assert_true(c ~= nil)
		H.assert_eq(c.r, 196)
		H.assert_eq(c.g, 30)
		H.assert_eq(c.b, 58)
		H.assert_eq(c.a, 255)
	end)

	H.TEST("util.fingers.digit_index", function()
		H.assert_eq(u.FingerDigitIndex(1), 1)
		H.assert_eq(u.FingerDigitIndex(3), 1)
		H.assert_eq(u.FingerDigitIndex(4), 2)
		H.assert_eq(u.FingerDigitIndex(15), 5)
	end)

	H.TEST("util.fingers.curl_lerp_unit", function()
		local open = Angle(0, 0, 0)
		local closed = Angle(90, 0, 0)
		local mid = u.LerpFingerAngle(0.5, open, closed)
		H.assert_near(mid.p, 45, 0.5)
	end)

	H.TEST("util.fingers.apply_curl", function()
		local open, closed = {}, {}
		for i = 1, 6 do
			open[i] = Angle(0, 0, 0)
			closed[i] = Angle(90, 0, 0)
		end
		local out = u.ApplyFingerCurl(open, closed, { 0, 1 }, {})
		H.assert_near(out[1].p, 0, 0.5) -- digit 1 curl 0
		H.assert_near(out[4].p, 90, 0.5) -- digit 2 curl 1
	end)

	H.TEST("util.calib.autoscale_668", function()
		H.assert_near(u.AutoScaleHeight(66.8, 66.8), 1, 1e-6)
		H.assert_near(u.AutoScaleHeight(33.4, 66.8), 2, 1e-4)
	end)

	H.TEST("util.calib.seated_offset", function()
		H.assert_near(u.AutoSeatedOffset(50, 66.8), 16.8, 0.01)
	end)

	H.TEST("util.settings.kinds_complete", function()
		for _, k in ipairs({ "header", "help", "bool", "slider", "combo", "color", "action" }) do
			H.assert_true(u.IsSettingsRowKind(k), k)
		end
		H.assert_false(u.IsSettingsRowKind("banana"))
	end)

	H.TEST("util.desktop.follow_pose", function()
		local DC = env.vrmod.DesktopCam
		H.assert_true(DC ~= nil and isfunction(DC.ComputeFollowPose))
		local pos, ang = DC.ComputeFollowPose(Vector(0, 0, 64), Angle(0, 0, 0), 72, 28)
		H.assert_true(pos.x < -10, "camera behind +X forward")
		H.assert_near(pos.z, 64 + 28, 0.1)
	end)

	H.TEST("util.desktop.view_enum_g23", function()
		local DC = env.vrmod.DesktopCam
		H.assert_true(DC.IsFollowMode(4))
		H.assert_true(not DC.IsFollowMode(3))
		H.assert_true(DC.IsEyeCropMode(2))
		H.assert_true(DC.IsEyeCropMode(3))
		H.assert_true(not DC.IsEyeCropMode(4))
		H.assert_true(not DC.IsEyeCropMode(1))
		H.assert_eq(DC.ClampDesktopView(0), 1)
		H.assert_eq(DC.ClampDesktopView(9), 4)
		H.assert_eq(DC.CycleDesktopView(3, 1), 4)
		H.assert_eq(DC.CycleDesktopView(4, 1), 1)
		H.assert_eq(DC.DesktopViewLabel(4), "follow_cam")
		H.assert_eq(DC.DesktopViewLabel(2), "left")
	end)

	H.TEST("util.rendering.desktop_crop", function()
		local vm, ho = env.vrmod.utils.ComputeDesktopCrop(2, 2048, 1024)
		H.assert_eq(ho, 0)
		local _, ho3 = env.vrmod.utils.ComputeDesktopCrop(3, 2048, 1024)
		H.assert_eq(ho3, 0.5)
		local vm4, ho4 = env.vrmod.utils.ComputeDesktopCrop(4, 2048, 1024)
		H.assert_eq(vm4, 0)
		H.assert_eq(ho4, 0)
		local vm1, ho1 = env.vrmod.utils.ComputeDesktopCrop(1, 2048, 1024)
		H.assert_eq(vm1, 0)
		H.assert_eq(ho1, 0)
	end)

	-- G21: ComputeSubmitBounds pure offline (mock is Linux/OpenGL — v flips)
	H.TEST("util.rendering.submit_bounds", function()
		local L = { Width = 1.0, Height = 1.0, HorizontalOffset = 0.0, VerticalOffset = 0.0 }
		local R = { Width = 1.0, Height = 1.0, HorizontalOffset = 0.0, VerticalOffset = 0.0 }
		local uMinL, vMinL, uMaxL, vMaxL, uMinR, vMinR, uMaxR, vMaxR =
			env.vrmod.utils.ComputeSubmitBounds(L, R, 0, 0, 1.0, false)
		H.assert_true(uMinL < uMaxL)
		H.assert_true(uMinR < uMaxR)
		H.assert_true(uMaxL <= uMinR + 0.001, "left half ends at seam")
		H.assert_true(uMinL >= 0 and uMaxR <= 1.01)
		-- asymmetric auto offset should shift U when renderOffset=true
		local La = { Width = 1.0, Height = 1.0, HorizontalOffset = 0.1, VerticalOffset = 0.0 }
		local Ra = { Width = 1.0, Height = 1.0, HorizontalOffset = -0.1, VerticalOffset = 0.0 }
		local aUMinL = env.vrmod.utils.ComputeSubmitBounds(La, Ra, 0, 0, 1.0, true)
		local bUMinL = env.vrmod.utils.ComputeSubmitBounds(La, Ra, 0, 0, 1.0, false)
		H.assert_true(math.abs(aUMinL - bUMinL) > 1e-6, "auto FOV offset changes U mins")
	end)

	H.TEST("util.rendering.adjust_fov", function()
		local proj = {
			{ 1, 0, 0.1, 0 },
			{ 0, 1, -0.2, 0 },
			{ 0, 0, 1, 0 },
			{ 0, 0, 0, 1 },
		}
		local out = env.vrmod.utils.AdjustFOV(proj, 2.0, 0.5)
		H.assert_near(out[1][1], 2.0, 1e-6)
		H.assert_near(out[2][2], 0.5, 1e-6)
		H.assert_near(out[1][3], 0.2, 1e-6) -- offset scaled by X
		H.assert_near(out[2][3], -0.1, 1e-6)
		-- original not mutated
		H.assert_near(proj[1][1], 1.0, 1e-6)
	end)

	-- G14 / W3 pure Glide SoT helpers
	H.TEST("util.glide.seat_and_steer_sot", function()
		local u = env.vrmod.utils
		H.assert_true(u.GlideSeatIsDriver(1))
		H.assert_true(u.GlideSeatIsDriver(0)) -- not ready → driver until recheck
		H.assert_true(not u.GlideSeatIsDriver(2))
		local s1, src1 = u.GlidePreferStickSteer(0.4, 0.9)
		H.assert_near(s1, 0.4, 1e-6)
		H.assert_eq(src1, "stick")
		local s2, src2 = u.GlidePreferStickSteer(0.01, 0.5)
		H.assert_near(s2, 0.5, 1e-6)
		H.assert_eq(src2, "wheel")
		local s3, src3 = u.GlidePreferStickSteer(0, 0)
		H.assert_eq(src3, "stick")
		H.assert_near(s3, 0, 1e-6)
		-- G14 HMD smoke expect
		H.assert_eq(u.GlideSteerSourceLabel("wheel"), "WHEEL ASSIST")
		local idle = u.Glide_HmdExpect({})
		H.assert_eq(idle.verdict, "idle")
		local drv = u.Glide_HmdExpect({
			in_vehicle = true,
			is_glide = true,
			is_driver = true,
			steer_source = "stick",
			has_steer_action = true,
		})
		H.assert_eq(drv.verdict, "expect_driver_stick")
		H.assert_true(drv.expect_stick_sot)
		H.assert_true(string.find(drv.checklist, "stick", 1, true))
		H.assert_eq(u.Glide_StatusLabel(drv), "GLIDE · DRIVER · STICK")
		H.assert_true(u.Glide_ShouldToastEnter(drv, false))
		H.assert_true(not u.Glide_ShouldToastEnter(drv, true))
		local t = u.Glide_EnterToast(drv)
		H.assert_true(type(t) == "string" and string.find(t, "thumbstick", 1, true))
		local wh = u.Glide_HmdExpect({
			in_vehicle = true,
			is_glide = true,
			is_driver = true,
			steer_source = "wheel",
			has_steer_action = true,
		})
		H.assert_eq(wh.verdict, "expect_driver_wheel")
		local pass = u.Glide_HmdExpect({
			in_vehicle = true,
			is_glide = true,
			is_driver = false,
		})
		H.assert_eq(pass.verdict, "expect_passenger")
		local unbound = u.Glide_HmdExpect({
			in_vehicle = true,
			is_glide = true,
			is_driver = true,
			has_steer_action = false,
		})
		H.assert_eq(unbound.verdict, "expect_unbound")
		H.assert_true(string.find(u.Glide_EnterToast(unbound) or "", "unbound", 1, true))
	end)

	-- G03 pure STAGE pack parse + toast (no origin apply)
	H.TEST("util.stage_pack.parse_and_hint", function()
		local u = env.vrmod.utils
		H.assert_eq(u.StagePack_NormalizeSpace("stage"), "STAGE")
		H.assert_true(u.StagePack_Parse("") == nil)
		H.assert_true(u.StagePack_Parse("v=1\nmap=gm_construct\n") == nil)
		local body = table.concat({
			"v=1",
			"ref_space=stage",
			"head_x_m=0.1",
			"head_y_m=1.65",
			"head_z_m=-0.2",
			"head_ok=1",
			"viewscale=1",
			"scalefactor=1.05",
			"supersample=1.5",
			"map=gm_construct",
			"source=cube_webui",
			"ts=123",
		}, "\n")
		local p = u.StagePack_Parse(body)
		H.assert_true(p ~= nil)
		H.assert_eq(p.ref_space, "STAGE")
		H.assert_near(p.head_y, 1.65, 1e-4)
		H.assert_true(p.head_ok)
		H.assert_true(u.StagePack_IsUsable(p))
		local hint = u.StagePack_ToastHint(p)
		H.assert_true(type(hint) == "string" and #hint > 8)
		H.assert_true(string.find(hint, "STAGE", 1, true) ~= nil)
		H.assert_true(string.find(hint, "deferred", 1, true) ~= nil)
		-- Extreme head Y clears head_ok but pack stays usable
		local badY = u.StagePack_Parse("v=1\nref_space=LOCAL\nhead_y_m=9.0\nhead_ok=1\n")
		H.assert_true(u.StagePack_IsUsable(badY))
		H.assert_true(not badY.head_ok)
		H.assert_true(u.StagePack_ToastHint(nil) == nil)
		-- G03 apply gate (default allow_apply=false)
		local decNone = u.StagePack_ApplyDecision(nil, {})
		H.assert_eq(decNone.reason, "unusable")
		local decClose = u.StagePack_ApplyDecision(p, {
			measured_head_y_m = 1.66,
			allow_apply = false,
		})
		H.assert_eq(decClose.reason, "already_close")
		H.assert_eq(decClose.action, "none")
		H.assert_true(decClose.safe)
		local decFar = u.StagePack_ApplyDecision(p, {
			measured_head_y_m = 2.3,
			allow_apply = false,
		})
		H.assert_eq(decFar.reason, "too_far")
		H.assert_true(not decFar.safe)
		local decElig = u.StagePack_ApplyDecision(p, {
			measured_head_y_m = 1.80,
			allow_apply = false,
		})
		H.assert_eq(decElig.reason, "eligible_deferred")
		H.assert_eq(decElig.action, "hint_only")
		H.assert_true(decElig.safe)
		local decApply = u.StagePack_ApplyDecision(p, {
			measured_head_y_m = 1.80,
			allow_apply = true,
		})
		H.assert_eq(decApply.action, "apply_scale")
		H.assert_eq(decApply.reason, "eligible")
		local at = u.StagePack_ApplyToast(decElig)
		H.assert_true(type(at) == "string" and string.find(at, "deferred", 1, true))
		-- G03 apply plan preview (no mutation when allow_apply false)
		local planDef = u.StagePack_ComputeApplyPlan(p, decElig, {
			world_scale = 40,
			current_seatedoffset = 0,
			allow_apply = false,
		})
		H.assert_true(planDef.valid)
		H.assert_eq(planDef.method, "seated_offset")
		H.assert_true(not planDef.do_apply)
		H.assert_true(#u.StagePack_MutationsFromPlan(planDef) == 0)
		local pt = u.StagePack_PlanToast(planDef)
		H.assert_true(type(pt) == "string" and string.find(pt, "deferred", 1, true))
		local planOn = u.StagePack_ComputeApplyPlan(p, decApply, {
			world_scale = 40,
			current_seatedoffset = 0,
			allow_apply = true,
		})
		H.assert_true(planOn.do_apply)
		local muts = u.StagePack_MutationsFromPlan(planOn)
		H.assert_true(#muts == 1)
		H.assert_eq(muts[1].convar, "vrmod_seatedoffset")
		-- Close → no seated plan
		local planClose = u.StagePack_ComputeApplyPlan(p, decClose, {
			world_scale = 40,
			allow_apply = true,
		})
		H.assert_eq(planClose.method, "none")
		-- G03 careful executor (injectable applier)
		H.assert_true(not u.StagePack_AllowApplyFromFlags({}))
		H.assert_true(u.StagePack_AllowApplyFromFlags({ convar_on = true }))
		H.assert_true(u.StagePack_AllowApplyFromFlags({ file_enable = true }))
		H.assert_true(not u.StagePack_ShouldExecutePlan(planOn, false))
		H.assert_true(u.StagePack_ShouldExecutePlan(planOn, true))
		H.assert_true(not u.StagePack_ShouldExecutePlan(planDef, true))
		local wrote = {}
		local er = u.StagePack_ExecuteMutations(muts, function(name, val)
			wrote[name] = val
			return true
		end)
		H.assert_true(er.ok)
		H.assert_eq(er.applied, 1)
		H.assert_eq(wrote["vrmod_seatedoffset"], muts[1].value)
		local erEmpty = u.StagePack_ExecuteMutations({}, function() return true end)
		H.assert_eq(erEmpty.applied, 0)
		local et = u.StagePack_ExecuteToast(er, planOn)
		H.assert_true(type(et) == "string" and string.find(et, "applied", 1, true))
		-- G03 HMD stage-apply expect (observer contract)
		local heDef = u.StagePack_HmdExpect(decElig, planDef, nil)
		H.assert_eq(heDef.verdict, "expect_deferred")
		H.assert_true(heDef.expect_no_jump)
		H.assert_true(type(heDef.checklist) == "string" and string.find(heDef.checklist, "DEFERRED", 1, true))
		H.assert_true(not u.StagePack_HeightJumpRiskIsBad(heDef))
		local heClose = u.StagePack_HmdExpect(decClose, planClose, nil)
		H.assert_eq(heClose.verdict, "expect_close")
		local heApp = u.StagePack_HmdExpect(decApply, planOn, er)
		H.assert_eq(heApp.verdict, "expect_applied")
		H.assert_true(string.find(heApp.checklist, "APPLIED", 1, true))
		local heFar = u.StagePack_HmdExpect(decFar, nil, nil)
		H.assert_eq(heFar.verdict, "expect_blocked")
	end)

	-- G13 pure return-to-Cube reverse protocol
	H.TEST("util.cube_return.protocol_g13", function()
		local u = env.vrmod.utils
		H.assert_true(not u.CubeReturn_ShouldNotifyCube(false, true))
		H.assert_true(u.CubeReturn_ShouldNotifyCube(true, true))
		H.assert_true(not u.CubeReturn_ShouldNotifyCube(true, false))
		local body = u.CubeReturn_Format("vr_exit", { map = "gm_construct", ts = 9, source = "t" })
		local p = u.CubeReturn_Parse(body)
		H.assert_true(p ~= nil)
		H.assert_eq(p.phase, "vr_exit")
		H.assert_eq(p.map, "gm_construct")
		H.assert_eq(u.CubeReturn_PhaseLabel("xr_released"), "XR RELEASED")
		H.assert_true(string.find(u.CubeReturn_DetailForPhase("vr_exit"), "reclaim", 1, true))
		H.assert_true(u.CubeReturn_Parse("") == nil)
	end)

	-- G04 pure warm map-attach decide (no auto changelevel)
	H.TEST("util.warm_attach.decide_g04", function()
		local u = env.vrmod.utils
		H.assert_eq(u.WarmAttach_NormalizeMap("maps/GM_Construct.bsp"), "gm_construct")
		H.assert_true(u.WarmAttach_Parse("") == nil)
		local body = table.concat({
			"v=1",
			"action=warm_request",
			"reason=eligible_deferred",
			"map=gm_flatgrass",
			"source=cube_webui",
			"ts=7",
		}, "\n")
		local req = u.WarmAttach_Parse(body)
		H.assert_true(req ~= nil)
		H.assert_eq(req.map, "gm_flatgrass")
		local same = u.WarmAttach_Decide(req, {
			current_map = "gm_flatgrass",
			allow_changelevel = false,
		})
		H.assert_eq(same.action, "same_map")
		H.assert_true(not same.would_changelevel)
		local defer = u.WarmAttach_Decide(req, {
			current_map = "gm_construct",
			allow_changelevel = false,
		})
		H.assert_eq(defer.action, "deferred")
		H.assert_eq(defer.reason, "eligible_deferred")
		H.assert_true(not defer.would_changelevel)
		local chg = u.WarmAttach_Decide(req, {
			current_map = "gm_construct",
			allow_changelevel = true,
		})
		H.assert_eq(chg.action, "changelevel")
		H.assert_true(chg.would_changelevel)
		local toast = u.WarmAttach_Toast(defer)
		H.assert_true(type(toast) == "string" and string.find(toast, "deferred", 1, true))
		H.assert_true(u.WarmAttach_Toast(u.WarmAttach_Decide(nil, {})) == nil)
		-- G04 careful changelevel plan executor (default off)
		H.assert_true(u.WarmAttach_MapTokenOk("gm_construct"))
		H.assert_true(not u.WarmAttach_MapTokenOk("gm_construct; quit"))
		H.assert_true(not u.WarmAttach_MapTokenOk(""))
		H.assert_true(not u.WarmAttach_AllowChangelevelFromFlags({}))
		H.assert_true(u.WarmAttach_AllowChangelevelFromFlags({ convar_on = true }))
		H.assert_true(u.WarmAttach_AllowChangelevelFromFlags({ file_enable = true }))
		local planDef = u.WarmAttach_ChangelevelPlan(defer)
		H.assert_true(not planDef.do_changelevel)
		H.assert_true(not u.WarmAttach_ShouldExecuteChangelevel(planDef, true))
		local planOn = u.WarmAttach_ChangelevelPlan(chg)
		H.assert_true(planOn.do_changelevel)
		H.assert_eq(planOn.method, "changelevel")
		H.assert_eq(u.WarmAttach_ChangelevelCmd(planOn), "changelevel gm_flatgrass")
		H.assert_true(not u.WarmAttach_ShouldExecuteChangelevel(planOn, false))
		H.assert_true(u.WarmAttach_ShouldExecuteChangelevel(planOn, true))
		local ran = {}
		local er = u.WarmAttach_ExecuteChangelevel(planOn, function(map)
			ran[#ran + 1] = map
			return true
		end)
		H.assert_true(er.applied and er.ok)
		H.assert_eq(ran[1], "gm_flatgrass")
		local et = u.WarmAttach_ExecuteToast(er, planOn)
		H.assert_true(type(et) == "string" and string.find(et, "changelevel", 1, true))
		local erNo = u.WarmAttach_ExecuteChangelevel(planDef, function() return true end)
		H.assert_true(not erNo.applied)
		-- G04 HMD warm expect
		local heDef = u.WarmAttach_HmdExpect(defer, planDef, nil, false)
		H.assert_eq(heDef.verdict, "expect_deferred")
		H.assert_true(heDef.expect_no_changelevel)
		H.assert_true(string.find(heDef.checklist, "DEFERRED", 1, true))
		local heSame = u.WarmAttach_HmdExpect(same, nil, nil, false)
		H.assert_eq(heSame.verdict, "expect_same_map")
		local heChg = u.WarmAttach_HmdExpect(chg, planOn, er, false)
		H.assert_eq(heChg.verdict, "expect_changelevel")
		H.assert_true(string.find(heChg.checklist, "CHANGELEVEL", 1, true))
	end)

	-- G31 pure bindings self-heal law (W6 force-rewrite + honest toast)
	H.TEST("util.bindings_law.self_heal_g31", function()
		local u = env.vrmod.utils
		H.assert_eq(u.BindingsLaw_ManifestRelPath(), "vrmod/vrmod_action_manifest.txt")
		H.assert_true(u.BindingsLaw_ForceRewriteOnStart())
		H.assert_eq(u.BindingsLaw_MaxSetAttempts(), 2)
		H.assert_true(u.BindingsLaw_ShouldRetryAfterFail(1))
		H.assert_true(not u.BindingsLaw_ShouldRetryAfterFail(2))
		H.assert_true(not u.BindingsLaw_AbortVrOnFail())
		H.assert_true(u.BindingsLaw_RequireToastOnFail())
		H.assert_true(string.find(u.BindingsLaw_ToastMessage(), "manifest", 1, true)
			or string.find(u.BindingsLaw_ToastMessage(), "bindings", 1, true)
			or string.find(u.BindingsLaw_ToastMessage(), "Bindings", 1, true)
			or string.find(u.BindingsLaw_ToastMessage(), "module", 1, true))
		local ok = u.BindingsLaw_Decide({
			force_rewrite = true,
			first_ok = true,
			has_file = true,
		})
		H.assert_true(ok.ok)
		H.assert_true(ok.path_ok)
		H.assert_eq(u.BindingsLaw_StatusLabel(ok), "BIND · OK")
		local he = u.BindingsLaw_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_ok")
		H.assert_true(string.find(he.checklist, "G31", 1, true))
		local retry = u.BindingsLaw_Decide({
			force_rewrite = true,
			first_ok = false,
			retry_ok = true,
			has_file = true,
		})
		H.assert_true(retry.ok)
		H.assert_eq(u.BindingsLaw_StatusLabel(retry), "BIND · RETRY OK")
		local failToast = u.BindingsLaw_Decide({
			force_rewrite = true,
			first_ok = false,
			retry_ok = false,
			has_file = true,
			toast_shown = true,
		})
		H.assert_true(not failToast.ok)
		H.assert_true(failToast.should_toast)
		H.assert_true(not failToast.abort_vr)
		H.assert_eq(failToast.risk, "set_fail")
		H.assert_eq(u.BindingsLaw_StatusLabel(failToast), "BIND · SET FAIL")
		local heF = u.BindingsLaw_HmdExpect(failToast)
		H.assert_eq(heF.verdict, "expect_fail_honest")
		local silent = u.BindingsLaw_Decide({
			force_rewrite = true,
			first_ok = false,
			retry_ok = false,
			has_file = true,
			toast_shown = false,
		})
		H.assert_eq(silent.risk, "silent_fail")
		H.assert_true(u.BindingsLaw_IsSilentFailRisk(silent))
		H.assert_eq(u.BindingsLaw_StatusLabel(silent), "BIND · SILENT FAIL")
		local noRw = u.BindingsLaw_Decide({
			force_rewrite = false,
			first_ok = true,
			has_file = true,
		})
		H.assert_eq(noRw.risk, "no_rewrite")
		H.assert_true(not noRw.path_ok)
	end)

	-- G27 pure engine blacklist law (never call blocked convars / W2)
	H.TEST("util.engine_blacklist_law.never_call_g27", function()
		local u = env.vrmod.utils
		H.assert_true(u.EngineBlacklist_IsBlocked("viewmodel_fov"))
		H.assert_true(u.EngineBlacklist_IsBlocked("r_shadowrendertotexture"))
		H.assert_true(u.EngineBlacklist_IsBlocked("mat_reduceparticles"))
		H.assert_true(u.EngineBlacklist_IsLifecycleBan("mat_queue_mode"))
		H.assert_true(u.EngineBlacklist_IsLifecycleBan("gmod_mcore_test"))
		H.assert_true(not u.EngineBlacklist_AllowWrite("viewmodel_fov"))
		H.assert_true(not u.EngineBlacklist_AllowWrite("mat_queue_mode"))
		H.assert_true(u.EngineBlacklist_AllowWrite("engine_no_focus_sleep"))
		H.assert_true(not u.EngineBlacklist_AllowRunConsoleCommand("viewmodel_fov"))
		local filtered, dropped = u.EngineBlacklist_FilterMap({
			mat_disable_bloom = "1",
			viewmodel_fov = "54",
			mat_queue_mode = "1",
			engine_no_focus_sleep = "0",
		})
		H.assert_true(filtered.mat_disable_bloom == "1")
		H.assert_true(filtered.engine_no_focus_sleep == "0")
		H.assert_true(filtered.viewmodel_fov == nil)
		H.assert_true(filtered.mat_queue_mode == nil)
		H.assert_true(#dropped >= 2)
		local clean = u.EngineBlacklist_Decide({
			vr_active = true,
			performance_map = { mat_disable_bloom = "1", engine_no_focus_sleep = "0" },
			attempted = { "engine_no_focus_sleep" },
		})
		H.assert_true(clean.path_ok)
		H.assert_eq(u.EngineBlacklist_StatusLabel(clean), "ENG · CLEAN")
		local he = u.EngineBlacklist_HmdExpect(clean)
		H.assert_eq(he.verdict, "expect_clean")
		H.assert_true(string.find(he.checklist, "G27", 1, true))
		local bad = u.EngineBlacklist_Decide({
			attempted = { "viewmodel_fov", "r_shadowrendertotexture" },
		})
		H.assert_true(not bad.path_ok)
		H.assert_eq(bad.risk, "blocked_spam")
		H.assert_true(u.EngineBlacklist_IsWriteRisk(bad))
		local life = u.EngineBlacklist_Decide({ attempted = { "mat_queue_mode" } })
		H.assert_eq(life.risk, "lifecycle_write")
		local names = u.EngineBlacklist_BlockedNames()
		H.assert_true(type(names) == "table" and #names >= 3)
	end)

	-- G26 pure menu thrash / QM dedupe law (VRClimb id collapse)
	H.TEST("util.menu_law.dedupe_g26", function()
		local u = env.vrmod.utils
		H.assert_eq(u.MenuLaw_NormalizeName("  VRClimb "), "vrclimb")
		H.assert_eq(u.MenuLaw_StableKey("Spawn Menu", "spawn"), "id:spawn")
		H.assert_eq(u.MenuLaw_StableKey("Chat", nil), "name:chat")
		H.assert_eq(u.MenuLaw_CanonicalClimbId(), "vrclimb")
		H.assert_true(u.MenuLaw_IsClimbName("VR Climb"))
		H.assert_true(u.MenuLaw_ItemsMatch({ name = "VRClimb", id = "vrclimb" }, "VR Climb", "vrclimb"))
		H.assert_true(u.MenuLaw_ItemsMatch({ name = "Spawn Menu", id = "spawn" }, "Spawn Menu", nil))
		H.assert_true(not u.MenuLaw_ItemsMatch({ name = "Chat", id = "chat" }, "Settings", "settings"))
		local list, dropped = u.MenuLaw_DedupList({
			{ name = "VRClimb", id = "vrclimb" },
			{ name = "VR Climb", id = "vrclimb" },
			{ name = "Chat", id = "chat" },
		})
		H.assert_eq(#list, 2)
		H.assert_eq(dropped, 1)
		local dirty = u.MenuLaw_Decide({
			items = {
				{ name = "VRClimb", id = "vrclimb" },
				{ name = "VR Climbing", id = "vrclimb" },
			},
		})
		H.assert_true(not dirty.path_ok or dirty.climb_dupes > 0 or dirty.dropped > 0)
		H.assert_true(u.MenuLaw_IsThrashRisk(dirty) or dirty.climb_dupes > 0)
		local clean = u.MenuLaw_Decide({
			items = {
				{ name = "VRClimb", id = "vrclimb" },
				{ name = "Chat", id = "chat" },
			},
		})
		H.assert_true(clean.path_ok)
		H.assert_eq(u.MenuLaw_StatusLabel(clean), "MENU · DEDUPED")
		local he = u.MenuLaw_HmdExpect(clean)
		H.assert_eq(he.verdict, "expect_clean")
		H.assert_true(string.find(he.checklist, "G26", 1, true))
	end)

	-- G25 pure pose SoT law (no dual-truth pose/angvel forks)
	H.TEST("util.pose_sot_law.single_path_g25", function()
		local u = env.vrmod.utils
		local steps = u.PoseSoT_PipelineSteps()
		H.assert_true(type(steps) == "table" and #steps >= 4)
		H.assert_eq(u.PoseSoT_PublicSource(), "tracking")
		H.assert_eq(u.PoseSoT_RawSource(), "rawTracking")
		H.assert_true(not u.PoseSoT_AllowSecondAngvelSoT())
		H.assert_true(not u.PoseSoT_AllowDualPublicPose())
		H.assert_eq(u.PoseSoT_GunReadsSource(), "tracking")
		H.assert_eq(u.PoseSoT_HeadVelSource(), "raw")
		H.assert_eq(u.PoseSoT_NormalizeSource("g_VR.tracking"), "tracking")
		H.assert_eq(u.PoseSoT_NormalizeSource("rawTracking"), "raw")
		local ok = u.PoseSoT_Decide({
			vr_active = true,
			has_raw = true,
			has_tracking = true,
			gun_reads = "tracking",
			head_vel_from = "raw",
			second_angvel_sot = false,
			dual_public = false,
			modifiers_in_place = true,
		})
		H.assert_true(ok.path_ok)
		H.assert_eq(ok.risk, "none")
		H.assert_eq(u.PoseSoT_StatusLabel(ok), "POSE · SINGLE PATH")
		local he = u.PoseSoT_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_single_path")
		H.assert_true(he.expect_single_path)
		H.assert_true(string.find(he.checklist, "G25", 1, true))
		local fork = u.PoseSoT_Decide({
			vr_active = true,
			has_tracking = true,
			gun_reads = "fork",
			second_angvel_sot = false,
		})
		H.assert_true(not fork.path_ok)
		H.assert_true(u.PoseSoT_IsForkRisk(fork))
		local ang = u.PoseSoT_Decide({
			vr_active = true,
			has_tracking = true,
			gun_reads = "tracking",
			second_angvel_sot = true,
		})
		H.assert_eq(ang.risk, "second_angvel")
		H.assert_eq(u.PoseSoT_StatusLabel(ang), "POSE · ANGVEL FORK")
		local heBad = u.PoseSoT_HmdExpect(ang)
		H.assert_eq(heBad.verdict, "expect_fork_fail")
	end)

	-- G19 pure submit path law (never eng IN / virgin OUT)
	H.TEST("util.submit_law.path_g19", function()
		local u = env.vrmod.utils
		H.assert_eq(u.SubmitLaw_PreferTextureKind(), "dual_out_rgba8")
		H.assert_true(not u.SubmitLaw_AllowSubmitEngIn())
		H.assert_true(not u.SubmitLaw_AllowVirginOut())
		H.assert_true(u.SubmitLaw_AllowCollect({ mat_queue_mode = 1 }))
		H.assert_true(not u.SubmitLaw_AllowCollect({ mat_queue_mode = 2 }))
		local ok = u.SubmitLaw_Decide({
			mat_queue_mode = 1,
			vr_active = true,
			painted = true,
			collected = true,
			keep_submit = true,
			submit_texture = "dual_out_rgba8",
		})
		H.assert_true(ok.allow_submit)
		H.assert_eq(ok.risk, "none")
		H.assert_eq(u.SubmitLaw_StatusLabel(ok), "SUBMIT · BLIT OUT")
		local he = u.SubmitLaw_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_dual_out")
		H.assert_true(he.expect_dual_out)
		H.assert_true(string.find(he.checklist, "G19", 1, true))
		local eng = u.SubmitLaw_Decide({
			vr_active = true,
			keep_submit = true,
			painted = true,
			submit_texture = "eng_in",
		})
		H.assert_true(not eng.allow_submit)
		H.assert_eq(eng.risk, "eng_in")
		H.assert_true(u.SubmitLaw_IsEngInRisk(eng))
		local heEng = u.SubmitLaw_HmdExpect(eng)
		H.assert_eq(heEng.verdict, "expect_eng_in_fail")
		local virgin = u.SubmitLaw_Decide({
			vr_active = true,
			keep_submit = true,
			submit_texture = "virgin_out",
		})
		H.assert_true(not virgin.allow_submit)
		H.assert_eq(virgin.risk, "virgin_out")
		local mq2 = u.SubmitLaw_Decide({
			mat_queue_mode = 2,
			vr_active = true,
			painted = true,
			keep_submit = true,
			submit_texture = "dual_out_rgba8",
		})
		H.assert_true(mq2.allow_submit)
		H.assert_eq(mq2.risk, "mq2_no_collect")
		H.assert_eq(u.SubmitLaw_StatusLabel(mq2), "SUBMIT · MQ2 OUT")
	end)

	-- G17 pure mat_queue pin law (never write 2 from VR)
	H.TEST("util.mat_queue_law.pin_g17", function()
		local u = env.vrmod.utils
		H.assert_eq(u.MatQueueLaw_CubePin(), 1)
		H.assert_eq(u.MatQueueLaw_ClampRead(9), 2)
		H.assert_eq(u.MatQueueLaw_ClampRead(-3), 0)
		H.assert_true(not u.MatQueueLaw_ShouldWrite("vr_session"))
		H.assert_true(not u.MatQueueLaw_ShouldWrite("exit"))
		H.assert_true(u.MatQueueLaw_AllowDualEye(1))
		H.assert_true(not u.MatQueueLaw_AllowDualEye(2))
		local pin = u.MatQueueLaw_Decide({ live_mode = 1, prefer = 1 })
		H.assert_true(pin.dual_ok)
		H.assert_true(pin.write_forbidden)
		H.assert_eq(u.MatQueueLaw_StatusLabel(pin), "MQ · LIVE 1 · PIN")
		local he = u.MatQueueLaw_HmdExpect(pin)
		H.assert_eq(he.verdict, "expect_pin")
		H.assert_true(string.find(he.checklist, "MQ1", 1, true) or string.find(he.checklist, "dual", 1, true))
		local mq2 = u.MatQueueLaw_Decide({ live_mode = 2, prefer = 1 })
		H.assert_true(not mq2.dual_ok)
		H.assert_eq(mq2.risk, "mq2_single")
		local he2 = u.MatQueueLaw_HmdExpect(mq2)
		H.assert_eq(he2.verdict, "expect_mq2_single")
		H.assert_true(not he2.expect_dual)
	end)

	-- G16 pure laser / primary-click sacred law
	H.TEST("util.laser_law.sacred_g16", function()
		local u = env.vrmod.utils
		H.assert_eq(u.LaserLaw_PrimaryHandFromInt(0), "right")
		H.assert_eq(u.LaserLaw_PrimaryHandFromInt(1), "left")
		H.assert_eq(u.LaserLaw_SecondaryHand("right"), "left")
		H.assert_true(u.LaserLaw_IsMenuPrimaryClick("boolean_primaryfire", "right"))
		H.assert_true(not u.LaserLaw_IsMenuPrimaryClick("boolean_left_primaryfire", "right"))
		H.assert_true(u.LaserLaw_IsMenuPrimaryClick("boolean_left_primaryfire", "left"))
		H.assert_true(u.LaserLaw_IsMenuPrimaryClick("boolean_car_mouse_left", "right"))
		H.assert_true(u.LaserLaw_IsMenuSecondaryClick("boolean_secondaryfire"))
		H.assert_true(u.LaserLaw_IsMenuCloseAction("boolean_chat"))
		H.assert_eq(u.LaserLaw_QmAttachModeFromInt(2), "float")
		H.assert_true(u.LaserLaw_ShouldSolveFocus({ stereo_eye = "left", stereo_frame = 1, focus_frame = 1 }))
		H.assert_true(not u.LaserLaw_ShouldSolveFocus({ stereo_eye = "right", stereo_frame = 1, focus_frame = 1 }))
		H.assert_true(u.LaserLaw_ShouldSolveFocus({ stereo_eye = "right", stereo_frame = 2, focus_frame = 1 }))
		local he = u.LaserLaw_HmdExpect({
			vr_active = true,
			laser_on = true,
			has_primary_pose = true,
			menu_focus = true,
			primary_hand = "right",
		})
		H.assert_eq(he.verdict, "expect_focus")
		H.assert_true(string.find(he.checklist, "FOCUS", 1, true))
		H.assert_eq(u.LaserLaw_StatusLabel(he), "LASER · FOCUS")
	end)

	-- G15 pure HUD composite law (never black wall of the Real)
	H.TEST("util.hud_law.composite_g15", function()
		local u = env.vrmod.utils
		H.assert_eq(u.HudLaw_ClampClearAlpha(300), 255)
		H.assert_eq(u.HudLaw_ClampClearAlpha(-1), 0)
		local clear = u.HudLaw_Decide({ clear_alpha = 0 })
		H.assert_eq(clear.composite, "translucent")
		H.assert_eq(clear.additive, 0)
		H.assert_true(not clear.black_slab_risk)
		local dim = u.HudLaw_Decide({ clear_alpha = 120 })
		H.assert_eq(dim.composite, "additive")
		H.assert_eq(dim.additive, 1)
		H.assert_true(not dim.black_slab_risk)
		local flags = u.HudLaw_MaterialFlags(dim)
		H.assert_eq(flags.additive, 1)
		H.assert_eq(flags.translucent, 1)
		local bad = u.HudLaw_Decide({ force_opaque = true })
		H.assert_true(u.HudLaw_IsBlackSlabRisk(bad))
		H.assert_eq(u.HudLaw_StatusLabel(dim), "HUD · ADDITIVE")
		local he = u.HudLaw_HmdExpect(dim)
		H.assert_eq(he.verdict, "expect_additive")
		H.assert_true(he.expect_real_visible)
		H.assert_true(string.find(he.checklist, "ADDITIVE", 1, true))
		local heClear = u.HudLaw_HmdExpect(clear)
		H.assert_eq(heClear.verdict, "expect_translucent")
	end)

	-- G05 pure stereo-load policy (never dual under mat_queue 2)
	H.TEST("util.stereo_load.policy_g05", function()
		local u = env.vrmod.utils
		-- Loading detector
		H.assert_true(u.StereoLoad_IsLoading({ is_in_game = false }))
		H.assert_true(u.StereoLoad_IsLoading({ is_in_game = true, local_player_valid = false }))
		H.assert_true(u.StereoLoad_IsLoading({ is_in_game = true, local_player_valid = true, map_name = "" }))
		H.assert_true(u.StereoLoad_IsLoading({ map_changing = true }))
		H.assert_true(not u.StereoLoad_IsLoading({
			is_in_game = true,
			local_player_valid = true,
			map_name = "gm_construct",
		}))
		local dual = u.StereoLoadPolicy({
			mat_queue_mode = 1,
			vr_active = true,
			loading = true,
			openxr_should_render = false,
		})
		H.assert_true(dual.dual_eye)
		H.assert_true(dual.prefer_paint_while_load)
		H.assert_true(dual.keep_submit)
		H.assert_true(dual.loading)
		H.assert_true(u.ShouldPaintStereoThisFrame(dual, false))
		H.assert_eq(u.StereoLoad_StatusLabel(dual), "STEREO · DUAL HOLD LOAD")
		H.assert_true(u.StereoLoad_ShouldToast(dual, false))
		H.assert_true(not u.StereoLoad_ShouldToast(dual, true))
		local mq2 = u.StereoLoadPolicy({
			mat_queue_mode = 2,
			vr_active = true,
			loading = true,
			openxr_should_render = true,
		})
		H.assert_true(not mq2.dual_eye)
		H.assert_true(mq2.single_pass)
		H.assert_true(not mq2.prefer_paint_while_load)
		H.assert_true(u.ShouldPaintStereoThisFrame(mq2, true))
		H.assert_true(not u.ShouldPaintStereoThisFrame(mq2, false))
		H.assert_true(string.find(u.StereoLoad_StatusLabel(mq2), "MQ2", 1, true))
		local idle = u.StereoLoadPolicy({
			mat_queue_mode = 1,
			vr_active = false,
			loading = false,
		})
		H.assert_true(not idle.keep_submit)
		local hint = u.StereoLoadToastHint(dual)
		H.assert_true(type(hint) == "string" and string.find(hint, "dual", 1, true))
		-- G05 HMD load-flash expect (observer contract; offline only)
		local he = u.StereoLoad_HmdExpect(dual)
		H.assert_true(he.expect_both_eyes)
		H.assert_eq(he.flash_risk, "none")
		H.assert_eq(he.verdict, "expect_dual_hold")
		H.assert_true(type(he.checklist) == "string" and string.find(he.checklist, "DUAL HOLD", 1, true))
		H.assert_true(not u.StereoLoad_FlashRiskIsBad(he))
		local he2 = u.StereoLoad_HmdExpect(mq2)
		H.assert_true(not he2.expect_both_eyes)
		H.assert_eq(he2.flash_risk, "mq2_right_clear")
		H.assert_eq(he2.verdict, "expect_mq2_single")
		H.assert_true(not u.StereoLoad_FlashRiskIsBad(he2)) -- mq2 clear is law, not mono void
		local heIdle = u.StereoLoad_HmdExpect(idle)
		H.assert_eq(heIdle.verdict, "expect_no_submit")
		H.assert_true(u.StereoLoad_FlashRiskIsBad(heIdle))
	end)
end
