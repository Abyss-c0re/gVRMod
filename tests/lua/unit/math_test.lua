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
		-- clampHalf may absorb pure H offset at the SBS seam; V offset still applies
		local La = { Width = 1.0, Height = 1.0, HorizontalOffset = 0.1, VerticalOffset = 0.2 }
		local Ra = { Width = 1.0, Height = 1.0, HorizontalOffset = -0.1, VerticalOffset = -0.2 }
		local _, aVMinL = env.vrmod.utils.ComputeSubmitBounds(La, Ra, 0, 0, 1.0, true)
		local _, bVMinL = env.vrmod.utils.ComputeSubmitBounds(La, Ra, 0, 0, 1.0, false)
		H.assert_true(math.abs(aVMinL - bVMinL) > 1e-6, "auto FOV vertical offset changes V mins")
		-- b1a5e9e path: left U span stays ordered (may cross 0.5 slightly under H offset)
		local c0, _, c2 = env.vrmod.utils.ComputeSubmitBounds(La, Ra, 0, 0, 1.0, true)
		H.assert_true(c0 < c2, "left U span ordered")
		-- mq2 mono-both: right UV mirrors left half (no one-eye black)
		local b = { uMinL, vMinL, uMaxL, vMaxL, uMinR, vMinR, uMaxR, vMaxR }
		H.assert_true(type(env.vrmod.utils.SubmitBounds_MirrorLeftToBoth) == "function")
		local m0, m1, m2, m3, m4, m5, m6, m7 = env.vrmod.utils.SubmitBounds_MirrorLeftToBoth(b)
		H.assert_near(m0, m4, 1e-6)
		H.assert_near(m2, m6, 1e-6)
		H.assert_near(m0, uMinL, 1e-6)
		H.assert_near(m2, uMaxL, 1e-6)
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

	-- G47 pure false per-eye FBO guard (both FBOs; no color+depth dual)
	H.TEST("util.false_per_eye_law.guard_g47", function()
		local u = env.vrmod.utils
		H.assert_true(u.FalsePerEyeLaw_RequireBothFbos())
		H.assert_true(u.FalsePerEyeLaw_RequireDistinctColorTex())
		H.assert_true(not u.FalsePerEyeLaw_AllowColorDepthAsDual())
		H.assert_true(u.FalsePerEyeLaw_FallbackToSbsWhenInvalid())
		H.assert_true(u.FalsePerEyeLaw_IsLegalPair({
			left_tex = 10, right_tex = 11, left_fbo = 1, right_fbo = 2,
		}))
		H.assert_true(not u.FalsePerEyeLaw_IsLegalPair({
			left_tex = 10, right_tex = 11, left_fbo = 1, right_fbo = 0,
		}))
		H.assert_true(not u.FalsePerEyeLaw_IsLegalPair({
			left_tex = 10, right_tex = 10, left_fbo = 1, right_fbo = 2,
		}))
		H.assert_eq(u.FalsePerEyeLaw_ResolvePath({
			left_tex = 10, right_tex = 11, left_fbo = 1, right_fbo = 2,
		}), "per_eye")
		H.assert_eq(u.FalsePerEyeLaw_ResolvePath({
			left_tex = 10, right_tex = 11, left_fbo = 0, right_fbo = 0, sbs_tex = 99,
		}), "sbs")
		H.assert_eq(u.FalsePerEyeLaw_ResolvePath({}), "none")
		local ok = u.FalsePerEyeLaw_Decide({
			left_tex = 10, right_tex = 11, left_fbo = 1, right_fbo = 2,
		})
		H.assert_true(ok.path_ok)
		H.assert_eq(ok.path, "per_eye")
		H.assert_eq(u.FalsePerEyeLaw_StatusLabel(ok), "EYE · PER-EYE OK")
		local he = u.FalsePerEyeLaw_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_per_eye")
		H.assert_true(string.find(he.checklist, "G47", 1, true))
		local bad = u.FalsePerEyeLaw_Decide({
			left_tex = 10, right_tex = 11, left_fbo = 1, right_fbo = 0,
			sbs_tex = 99, claimed_per_eye = true,
		})
		H.assert_eq(bad.risk, "missing_fbo")
		H.assert_eq(bad.path, "sbs")
		H.assert_true(bad.path_ok)
		H.assert_true(u.FalsePerEyeLaw_IsBlackEyeRisk(u.FalsePerEyeLaw_Decide({
			claimed_per_eye = true, left_tex = 5, right_tex = 6,
		})))
		local none = u.FalsePerEyeLaw_Decide({})
		H.assert_eq(none.risk, "no_src")
		H.assert_true(not none.path_ok)
	end)

	-- G46 pure desktop mirror law (b1a5e9e mid-frame legacy + post-submit ban)
	H.TEST("util.desktop_mirror_law.hmd_g46", function()
		local u = env.vrmod.utils
		H.assert_true(not u.DesktopMirror_AllowSampleStereoRtAfterSubmit())
		H.assert_true(u.DesktopMirror_AllowEyeCropFromLiveRt())
		H.assert_true(u.DesktopMirror_PreferFollowPrivateRt())
		H.assert_true(not u.DesktopMirror_PresentOnlyAfterSubmit())
		H.assert_eq(u.DesktopMirror_PreferDesktopViewForHmd(), 1)
		H.assert_true(u.DesktopMirror_IsEyeCropMode(2))
		H.assert_true(u.DesktopMirror_IsFollowMode(4))
		H.assert_true(u.DesktopMirror_IsNoneMode(1))
		-- mid-frame eye-crop allowed (legacy)
		H.assert_true(u.DesktopMirror_AllowPresent({
			vr_active = true, mid_frame = true, desktop_view = 2, sample_stereo_rt = true,
		}))
		-- post-submit live RT sample still forbidden
		H.assert_true(not u.DesktopMirror_AllowPresent({
			vr_active = true, after_submit = true, desktop_view = 2, sample_stereo_rt = true,
		}))
		local post = u.DesktopMirror_Decide({
			vr_active = true, after_submit = true, desktop_view = 2,
			sample_stereo_rt = true, attempt_present = true,
		})
		H.assert_true(not post.allow_present)
		H.assert_eq(post.risk, "live_rt_post")
		H.assert_true(u.DesktopMirror_IsBlackRisk(post))
		H.assert_eq(u.DesktopMirror_HmdExpect(post).verdict, "expect_black_risk")
		local mid = u.DesktopMirror_Decide({
			vr_active = true, mid_frame = true, desktop_view = 2,
			sample_stereo_rt = true, attempt_present = true,
		})
		H.assert_true(mid.allow_present)
		H.assert_eq(mid.risk, "mid_live_rt")
		H.assert_eq(u.DesktopMirror_StatusLabel(mid), "DESK · MID LIVE RT LEGACY")
		H.assert_eq(u.DesktopMirror_HmdExpect(mid).verdict, "expect_legacy_risk")
		local follow = u.DesktopMirror_Decide({
			vr_active = true, mid_frame = true, desktop_view = 4,
		})
		H.assert_true(follow.allow_present)
		H.assert_eq(u.DesktopMirror_StatusLabel(follow), "DESK · FOLLOW")
		local he = u.DesktopMirror_HmdExpect(follow)
		H.assert_eq(he.verdict, "expect_ok")
		H.assert_true(string.find(he.checklist, "G46", 1, true))
	end)

	-- G41 pure HMD walk inventory (manual backlog; never offline smoke claim)
	H.TEST("util.hmd_walk_law.inventory_g41", function()
		local u = env.vrmod.utils
		H.assert_true(u.HmdWalk_NeverClaimFromOfflineAlone())
		H.assert_true(u.HmdWalk_Count() >= 20)
		H.assert_true(u.HmdWalk_CountP0() >= 5)
		local g05 = u.HmdWalk_FindById("G05")
		H.assert_true(g05 ~= nil)
		H.assert_eq(g05.section, "0.1")
		H.assert_eq(g05.snap, "_stereoLoadHmdExpect")
		local g12 = u.HmdWalk_FindById("G12")
		H.assert_true(g12 ~= nil)
		H.assert_eq(g12.section, "0.2")
		local pref = u.HmdWalk_PreferredNextIds()
		H.assert_eq(pref[1], "G05")
		H.assert_eq(pref[2], "G12")
		local line = u.HmdWalk_FormatLine(g05)
		H.assert_true(string.find(line, "G05", 1, true))
		local rep = u.HmdWalk_FormatReport()
		H.assert_true(string.find(rep, "G41", 1, true))
		H.assert_true(string.find(rep, "Prefer next", 1, true))
		local live = u.HmdWalk_CollectLive({
			_stereoLoadHmdExpect = { checklist = "G05 · OK · dual" },
			_initLawHmdExpect = { verdict = "expect_ok" },
		})
		local found = false
		for _, r in ipairs(live) do
			if r.id == "G05" and r.present then found = true end
		end
		H.assert_true(found)
		local liveTxt = u.HmdWalk_FormatLive(live)
		H.assert_true(string.find(liveTxt, "G05", 1, true))
		local ok = u.HmdWalk_Decide({ g = { _stereoLoadHmdExpect = { checklist = "x" } } })
		H.assert_true(ok.path_ok)
		H.assert_true(ok.live_n >= 1)
		H.assert_true(string.find(u.HmdWalk_StatusLabel(ok), "HMDWALK", 1, true))
		local he = u.HmdWalk_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_manual_walks")
		H.assert_true(string.find(he.checklist, "G41", 1, true))
		local bad = u.HmdWalk_Decide({ claimed_hmd_ok_from_offline = true })
		H.assert_true(not bad.path_ok)
		H.assert_eq(bad.risk, "offline_claim")
		H.assert_eq(u.HmdWalk_HmdExpect(bad).verdict, "expect_forbidden")
		H.assert_true(u.HmdWalk_PriorityIsP0(g05))
	end)

	-- G42 pure hands-stuck unstick law (ship bar; identity + raw restore)
	H.TEST("util.hand_stuck_law.unstick_g42", function()
		local u = env.vrmod.utils
		H.assert_eq(u.HandStuckLaw_TrackCollapseThresholdSqr(), 4)
		H.assert_eq(u.HandStuckLaw_RawSeparatedThresholdSqr(), 36)
		H.assert_true(u.HandStuckLaw_SkipUnstickWhenForegrip())
		H.assert_true(u.HandStuckLaw_ShouldSplitIdentity(true))
		H.assert_true(not u.HandStuckLaw_ShouldSplitIdentity(false))
		H.assert_true(u.HandStuckLaw_ShouldUnstickFromRaw({
			track_dist_sqr = 1, raw_dist_sqr = 100, foregrip_active = false,
		}))
		H.assert_true(not u.HandStuckLaw_ShouldUnstickFromRaw({
			track_dist_sqr = 1, raw_dist_sqr = 100, foregrip_active = true,
		}))
		local ok = u.HandStuckLaw_Decide({ track_dist_sqr = 100, raw_dist_sqr = 100 })
		H.assert_true(ok.path_ok)
		H.assert_eq(u.HandStuckLaw_StatusLabel(ok), "HANDS · OK")
		local he = u.HandStuckLaw_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_ok")
		H.assert_true(string.find(he.checklist, "G42", 1, true))
		local coll = u.HandStuckLaw_Decide({
			track_dist_sqr = 1, raw_dist_sqr = 100, foregrip_active = false,
		})
		H.assert_eq(coll.risk, "collapse")
		H.assert_true(u.HandStuckLaw_IsStuckRisk(coll))
	end)

	-- G43 pure nested RT / menu-open crash law
	H.TEST("util.nested_rt_law.menu_open_g43", function()
		local u = env.vrmod.utils
		H.assert_true(not u.NestedRtLaw_AllowNestUnderStereo())
		H.assert_true(u.NestedRtLaw_RequirePopAfterMenuPaint())
		H.assert_true(u.NestedRtLaw_AllowMenuRtPaint({ stereo_rt_active = false }))
		H.assert_true(not u.NestedRtLaw_AllowMenuRtPaint({ stereo_rt_active = true }))
		H.assert_true(not u.NestedRtLaw_AllowNestedWorldCapture({ mat_queue_mode = 2 }))
		local ok = u.NestedRtLaw_Decide({ stereo_rt_active = false })
		H.assert_true(ok.path_ok)
		H.assert_eq(u.NestedRtLaw_StatusLabel(ok), "NEST · OK")
		local he = u.NestedRtLaw_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_ok")
		H.assert_true(string.find(he.checklist, "G43", 1, true))
		local bad = u.NestedRtLaw_Decide({ stereo_rt_active = true, menu_paint_attempt = true })
		H.assert_true(not bad.path_ok)
		H.assert_eq(bad.risk, "nest_menu")
		H.assert_true(u.NestedRtLaw_IsCrashRisk(bad))
	end)

	-- G44 pure grab-end / drop-cooldown law
	H.TEST("util.grab_end_law.storm_g44", function()
		local u = env.vrmod.utils
		H.assert_eq(u.GrabEndLaw_CooldownSeconds(), 0.1)
		H.assert_true(not u.GrabEndLaw_AllowClimbRewrite())
		H.assert_true(u.GrabEndLaw_AllowPickup({ cooldown_active = false }))
		H.assert_true(not u.GrabEndLaw_AllowPickup({ cooldown_active = true }))
		H.assert_true(u.GrabEndLaw_IsStorm({ drop_events = 5 }))
		local ok = u.GrabEndLaw_Decide({ hand = "right", allow_pickup = true })
		H.assert_true(ok.path_ok)
		H.assert_eq(u.GrabEndLaw_StatusLabel(ok), "GRAB · OK")
		local he = u.GrabEndLaw_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_ok")
		H.assert_true(string.find(he.checklist, "G44", 1, true))
		local storm = u.GrabEndLaw_Decide({ drop_events = 4 })
		H.assert_eq(storm.risk, "storm")
		H.assert_true(u.GrabEndLaw_IsStormRisk(storm))
	end)

	-- G40 pure Vision/border fill law (W1; guided path, defaults, bleed risk)
	H.TEST("util.border_law.fill_g40", function()
		local u = env.vrmod.utils
		H.assert_eq(u.BorderLaw_DefaultScale(), 1.0)
		H.assert_eq(u.BorderLaw_DefaultVertical(), 0.0)
		H.assert_eq(u.BorderLaw_DefaultHorizontal(), 0.0)
		H.assert_true(u.BorderLaw_IsGuidedPathOnly())
		H.assert_true(u.BorderLaw_PreferGuideOverZSpam())
		H.assert_true(u.BorderLaw_RequireRenderOffsetOnGuide())
		H.assert_eq(u.BorderLaw_ClampScale(0.01), u.BorderLaw_ScaleMin())
		H.assert_eq(u.BorderLaw_ClampScale(9), u.BorderLaw_ScaleMax())
		H.assert_eq(u.BorderLaw_ClampOffset(-5), u.BorderLaw_OffsetMin())
		H.assert_eq(u.BorderLaw_ClampOffset(5), u.BorderLaw_OffsetMax())
		local steps = u.BorderLaw_StepIds()
		H.assert_eq(steps[1], "reset")
		H.assert_eq(steps[2], "scale")
		H.assert_eq(steps[3], "vertical")
		H.assert_eq(steps[4], "horizontal")
		H.assert_eq(steps[5], "done")
		local base = u.BorderLaw_GuideBaseline()
		H.assert_eq(base.scalefactor, 1.0)
		H.assert_eq(base.verticaloffset, 0.0)
		H.assert_eq(base.horizontaloffset, 0.0)
		H.assert_eq(base.renderoffset, 1)
		H.assert_true(not u.BorderLaw_IsBleedRisk({ scalefactor = 1, verticaloffset = 0, horizontaloffset = 0 }))
		H.assert_true(u.BorderLaw_IsBleedRisk({ scalefactor = 0.5, verticaloffset = 0, horizontaloffset = 0 }))
		H.assert_true(u.BorderLaw_IsBleedRisk({ scalefactor = 1, verticaloffset = 0.5, horizontaloffset = 0 }))
		local ok = u.BorderLaw_Decide({ scalefactor = 1, verticaloffset = 0, horizontaloffset = 0 })
		H.assert_true(ok.path_ok)
		H.assert_eq(u.BorderLaw_StatusLabel(ok), "BORDER · OK")
		local he = u.BorderLaw_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_ok")
		H.assert_true(string.find(he.checklist, "G40", 1, true))
		local guide = u.BorderLaw_Decide({
			scalefactor = 1,
			verticaloffset = 0,
			horizontaloffset = 0,
			guide_active = true,
		})
		H.assert_eq(guide.risk, "guide")
		H.assert_eq(u.BorderLaw_StatusLabel(guide), "BORDER · GUIDE")
		local bleed = u.BorderLaw_Decide({ scalefactor = 0.5, verticaloffset = 0, horizontaloffset = 0 })
		H.assert_eq(bleed.risk, "bleed")
		H.assert_true(u.BorderLaw_IsBleedDecision(bleed))
		local bars = u.BorderLaw_Decide({
			scalefactor = 1,
			verticaloffset = 0,
			horizontaloffset = 0,
			fill_ok = false,
		})
		H.assert_eq(bars.risk, "bars")
		H.assert_true(not bars.path_ok)
		local heB = u.BorderLaw_HmdExpect(bars)
		H.assert_eq(heB.verdict, "expect_bars")
		local san = u.BorderLaw_Sanitize({ scalefactor = 99, verticaloffset = -9, horizontaloffset = 9 })
		H.assert_true(san.clamped)
		H.assert_eq(san.scalefactor, u.BorderLaw_ScaleMax())
		H.assert_eq(san.verticaloffset, u.BorderLaw_OffsetMin())
		H.assert_eq(san.horizontaloffset, u.BorderLaw_OffsetMax())
		H.assert_true(string.find(u.BorderLaw_ProfilePath(), "border_profile", 1, true))
	end)

	-- G39 pure VR_Init human error surface (W11 codes 108/215)
	H.TEST("util.init_law.surface_g39", function()
		local u = env.vrmod.utils
		H.assert_true(u.InitLaw_RequireToastOnFail())
		H.assert_true(u.InitLaw_SilentFailForbidden())
		H.assert_eq(u.InitLaw_MinModuleVersion(), 20)
		H.assert_true(string.find(u.InitLaw_ModuleZipUrl(), "github", 1, true))
		H.assert_true(string.find(u.InitLaw_KnownCodeMessage(108), "HMD", 1, true))
		H.assert_true(string.find(u.InitLaw_KnownCodeMessage(215), "runtime", 1, true)
			or string.find(u.InitLaw_KnownCodeMessage(215), "SteamVR", 1, true)
			or string.find(u.InitLaw_KnownCodeMessage(215), "215", 1, true))
		H.assert_eq(u.InitLaw_ParseCode("VR init failed code 108"), 108)
		H.assert_eq(u.InitLaw_ParseCode("xrCreateInstance failed (215)"), 215)
		H.assert_true(u.InitLaw_IsNoHmdHint("No HMD found"))
		H.assert_true(u.InitLaw_IsRuntimeHint("OpenXR loader missing"))
		local h = u.InitLaw_Humanize({
			err = "HMD not found (108)",
			module_version = 23,
			backend = "openxr",
		})
		H.assert_eq(h.code, 108)
		H.assert_true(string.find(h.toast, "HMD", 1, true) or string.find(h.toast, "108", 1, true))
		H.assert_true(string.find(h.overlay, "Module", 1, true))
		H.assert_true(string.find(h.overlay, "github", 1, true))
		local ok = u.InitLaw_Decide({ ok = true, module_version = 23 })
		H.assert_true(ok.path_ok)
		H.assert_eq(u.InitLaw_StatusLabel(ok), "INIT · OK")
		local he = u.InitLaw_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_ok")
		H.assert_true(string.find(he.checklist, "G39", 1, true))
		local fail = u.InitLaw_Decide({
			ok = false,
			err = "init code 108",
			module_version = 22,
			toast_shown = true,
		})
		H.assert_true(not fail.path_ok)
		H.assert_eq(fail.risk, "init_fail")
		H.assert_eq(u.InitLaw_StatusLabel(fail), "INIT · FAIL 108")
		local heF = u.InitLaw_HmdExpect(fail)
		H.assert_eq(heF.verdict, "expect_fail_honest")
		local silent = u.InitLaw_Decide({
			ok = false,
			err = "fail",
			toast_shown = false,
		})
		H.assert_eq(silent.risk, "silent_fail")
		H.assert_true(u.InitLaw_IsSilentFailRisk(silent))
	end)

	-- G38 pure worldmodel single-path law (W10; no dual ghost)
	H.TEST("util.worldmodel_law.single_path_g38", function()
		local u = env.vrmod.utils
		H.assert_true(u.WorldModelLaw_CubePreferFloatingHands())
		H.assert_true(not u.WorldModelLaw_AllowDualGhost())
		H.assert_true(not u.WorldModelLaw_AllowDualWeaponDraw())
		H.assert_eq(u.WorldModelLaw_ResolvePath({ floating_hands = true }), "floating_hands")
		H.assert_eq(u.WorldModelLaw_ResolvePath({
			use_worldmodels = true,
			floating_hands = false,
			draw_viewmodel = false,
			draw_worldmodel_vm = true,
		}), "worldmodel")
		H.assert_eq(u.WorldModelLaw_ResolvePath({ floating_hands = false }), "player_body")
		H.assert_eq(u.WorldModelLaw_ResolvePath({
			draw_viewmodel = true,
			draw_worldmodel_vm = true,
		}), "dual_ghost")
		H.assert_eq(u.WorldModelLaw_ResolvePath({
			floating_hands = true,
			draw_player_body = true,
		}), "dual_ghost")
		local ok = u.WorldModelLaw_Decide({ floating_hands = true })
		H.assert_true(ok.path_ok)
		H.assert_eq(ok.path, "floating_hands")
		H.assert_eq(u.WorldModelLaw_StatusLabel(ok), "WM · FLOATING")
		local he = u.WorldModelLaw_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_ok")
		H.assert_true(string.find(he.checklist, "G38", 1, true))
		local dual = u.WorldModelLaw_Decide({
			draw_viewmodel = true,
			draw_worldmodel_vm = true,
			floating_hands = false,
		})
		H.assert_eq(dual.raw_path, "dual_ghost")
		H.assert_true(dual.sanitized)
		H.assert_eq(dual.path, "floating_hands") -- Cube prefer
		H.assert_eq(u.WorldModelLaw_StatusLabel(dual), "WM · SANITIZED")
		H.assert_true(not u.WorldModelLaw_IsDualRisk(dual))
		local unsan = {
			valid = true,
			risk = "dual_ghost",
			sanitized = false,
			path = "dual_ghost",
		}
		H.assert_true(u.WorldModelLaw_IsDualRisk(unsan))
		local wm = u.WorldModelLaw_Decide({
			use_world_model = true,
			floating_hands = false,
			draw_viewmodel = false,
			draw_worldmodel_vm = true,
		})
		H.assert_eq(wm.path, "worldmodel")
		H.assert_eq(u.WorldModelLaw_StatusLabel(wm), "WM · WORLDMODEL")
	end)

	-- G37 pure hand vs bullet filter law (W9)
	H.TEST("util.hand_bullet_law.filter_g37", function()
		local u = env.vrmod.utils
		H.assert_eq(u.HandBulletLaw_HandDamageScale(), 0.45)
		H.assert_eq(u.HandBulletLaw_HeadDamageScale(), 10.0)
		H.assert_true(not u.HandBulletLaw_ProxySolidToWorld())
		H.assert_true(u.HandBulletLaw_AllowGrabContact())
		H.assert_true(u.HandBulletLaw_PreventSelfMeleeOnHand())
		H.assert_true(u.HandBulletLaw_AbsorbNonBulletOnProxy())
		H.assert_true(u.HandBulletLaw_IsBulletDamageType(2)) -- DMG_BULLET
		H.assert_true(not u.HandBulletLaw_IsBulletDamageType(0))
		H.assert_true(u.HandBulletLaw_IsHandPart("left"))
		H.assert_true(not u.HandBulletLaw_IsHandPart("head"))
		H.assert_eq(u.HandBulletLaw_RedirectScale("head"), 10.0)
		H.assert_eq(u.HandBulletLaw_RedirectScale("right"), 0.45)
		H.assert_true(u.HandBulletLaw_ShouldDropOnHandBullet("left", true))
		H.assert_true(not u.HandBulletLaw_ShouldDropOnHandBullet("head", true))
		H.assert_true(u.HandBulletLaw_ShouldAbsorbOnProxy({
			is_bullet = false,
			is_self = true,
			part = "left",
		}))
		H.assert_true(not u.HandBulletLaw_ShouldAbsorbOnProxy({
			is_bullet = true,
			is_self = false,
			part = "left",
		}))
		local handHit = u.HandBulletLaw_Decide({
			part = "right",
			is_bullet = true,
			damage = 100,
		})
		H.assert_true(handHit.path_ok)
		H.assert_eq(handHit.player_damage, 45)
		H.assert_true(handHit.drop_weapon)
		H.assert_eq(u.HandBulletLaw_StatusLabel(handHit), "HAND · BULLET REDIRECT")
		local he = u.HandBulletLaw_HmdExpect(handHit)
		H.assert_eq(he.verdict, "expect_bullet_redirect")
		H.assert_true(string.find(he.checklist, "G37", 1, true))
		local block = u.HandBulletLaw_Decide({
			part = "left",
			is_bullet = true,
			blocks_bullets_as_world = true,
		})
		H.assert_true(not block.path_ok)
		H.assert_eq(block.risk, "blocks_bullets")
		H.assert_true(u.HandBulletLaw_IsBlockRisk(block))
		local solid = u.HandBulletLaw_Decide({ solid_to_world = true, part = "left" })
		H.assert_eq(solid.risk, "solid_world")
		local absorb = u.HandBulletLaw_Decide({
			part = "left",
			is_bullet = false,
			is_self = true,
			damage = 10,
		})
		H.assert_true(absorb.absorb)
		H.assert_eq(u.HandBulletLaw_StatusLabel(absorb), "HAND · ABSORB")
	end)

	-- G36 pure FOV/Z soft-refresh law (W5; no mid-frame UV fight)
	H.TEST("util.fovz_law.soft_refresh_g36", function()
		local u = env.vrmod.utils
		H.assert_eq(u.FovZLaw_ClampFovScale(0.05), 0.1)
		H.assert_eq(u.FovZLaw_ClampFovScale(3.0), 2.0)
		H.assert_eq(u.FovZLaw_ClampZnear(0.01), 0.1)
		H.assert_eq(u.FovZLaw_ClampZnear(1.0), 1.0)
		H.assert_true(not u.FovZLaw_AllowMidFrameUvFight())
		H.assert_true(not u.FovZLaw_AllowLiveFovWithoutSoftRefresh())
		H.assert_true(u.FovZLaw_PreferBorderGuideOverZSpam())
		H.assert_eq(u.FovZLaw_RefreshKind("vrmod_horizontaloffset"), "submit_bounds")
		H.assert_eq(u.FovZLaw_RefreshKind("vrmod_fovscale_x"), "soft_display")
		H.assert_eq(u.FovZLaw_RefreshKind("vrmod_znear"), "session")
		H.assert_eq(u.FovZLaw_RefreshKind("vrmod_desktopview"), "desktop_view")
		H.assert_eq(u.FovZLaw_RefreshKind("unknown_cvar"), "none")
		H.assert_true(u.FovZLaw_IsBorderCvar("vrmod_scalefactor"))
		H.assert_true(u.FovZLaw_IsFovProfileCvar("vrmod_viewscale"))
		H.assert_true(not u.FovZLaw_IsFovProfileCvar("vrmod_desktopview"))
		H.assert_true(u.FovZLaw_IsSessionCvar("vrmod_postprocess"))
		local ok = u.FovZLaw_Decide({
			cvar = "vrmod_fovscale_x",
			vr_active = true,
			soft_refreshed = true,
			fov_x = 1.0,
			fov_y = 1.0,
		})
		H.assert_true(ok.path_ok)
		H.assert_eq(ok.refresh_kind, "soft_display")
		H.assert_eq(u.FovZLaw_StatusLabel(ok), "FOVZ · SOFT")
		local he = u.FovZLaw_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_ok")
		H.assert_true(string.find(he.checklist, "G36", 1, true))
		local fight = u.FovZLaw_Decide({ mid_frame_uv_and_fov = true, vr_active = true })
		H.assert_true(not fight.path_ok)
		H.assert_eq(fight.risk, "mid_frame_fight")
		H.assert_true(u.FovZLaw_IsJitterRisk(fight))
		local noSoft = u.FovZLaw_Decide({
			cvar = "vrmod_fovscale_y",
			vr_active = true,
			soft_refreshed = false,
		})
		H.assert_eq(noSoft.risk, "no_soft")
		local extreme = u.FovZLaw_Decide({
			cvar = "vrmod_fovscale_x",
			vr_active = true,
			soft_refreshed = true,
			fov_x = 0.2,
			fov_y = 1.0,
		})
		H.assert_eq(extreme.risk, "extreme_fov")
		local bounds = u.FovZLaw_Decide({ cvar = "vrmod_verticaloffset", vr_active = true })
		H.assert_eq(bounds.refresh_kind, "submit_bounds")
		H.assert_eq(u.FovZLaw_StatusLabel(bounds), "FOVZ · BOUNDS")
	end)

	-- G35 pure viewscale fisheye law (W8)
	H.TEST("util.viewscale_law.fisheye_g35", function()
		local u = env.vrmod.utils
		H.assert_eq(u.ViewScaleLaw_CubeDefault(), 1.0)
		H.assert_eq(u.ViewScaleLaw_Min(), 0.1)
		H.assert_eq(u.ViewScaleLaw_Max(), 2.0)
		H.assert_eq(u.ViewScaleLaw_Clamp(0.05), 0.1)
		H.assert_eq(u.ViewScaleLaw_Clamp(3.0), 2.0)
		H.assert_eq(u.ViewScaleLaw_Clamp(1.0), 1.0)
		H.assert_true(not u.ViewScaleLaw_IsFisheyeRisk(1.0))
		H.assert_true(u.ViewScaleLaw_IsFisheyeRisk(0.5))
		H.assert_true(u.ViewScaleLaw_IsFisheyeRisk(1.5))
		H.assert_true(u.ViewScaleLaw_PreferHmdProjection())
		local ok = u.ViewScaleLaw_Decide({ viewscale = 1.0, proj_live = true })
		H.assert_true(ok.path_ok)
		H.assert_eq(u.ViewScaleLaw_StatusLabel(ok), "VS · OK")
		local he = u.ViewScaleLaw_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_ok")
		H.assert_true(string.find(he.checklist, "G35", 1, true))
		local fish = u.ViewScaleLaw_Decide({ viewscale = 0.5, proj_live = true })
		H.assert_eq(fish.risk, "fisheye")
		H.assert_true(u.ViewScaleLaw_IsFisheyeDecision(fish))
		H.assert_eq(u.ViewScaleLaw_StatusLabel(fish), "VS · FISHEYE RISK")
		local clamp = u.ViewScaleLaw_Decide({ viewscale = 9.0 })
		H.assert_true(clamp.clamped)
		H.assert_eq(clamp.applied, 2.0)
		H.assert_eq(clamp.risk, "clamp")
		local dead = u.ViewScaleLaw_Decide({
			viewscale = 1.0,
			proj_live = false,
			require_proj_live = true,
		})
		H.assert_eq(dead.risk, "dead_proj")
		local heD = u.ViewScaleLaw_HmdExpect(dead)
		H.assert_eq(heD.verdict, "expect_dead_proj")
	end)

	-- G34 pure fly-away / origin snap + action set law (W12)
	H.TEST("util.flyaway_law.origin_action_g34", function()
		local u = env.vrmod.utils
		H.assert_eq(u.FlyAwayLaw_ActionSetMain(), "/actions/main")
		H.assert_eq(u.FlyAwayLaw_ActionSetDriving(), "/actions/driving")
		H.assert_eq(u.FlyAwayLaw_ActionSetBase(), "/actions/base")
		H.assert_eq(u.FlyAwayLaw_ResolveActionSet(false), "/actions/main")
		H.assert_eq(u.FlyAwayLaw_ResolveActionSet(true), "/actions/driving")
		H.assert_eq(u.FlyAwayLaw_InsaneVerticalVel(), 1500)
		H.assert_true(u.FlyAwayLaw_IsInsaneVertical(2000))
		H.assert_true(u.FlyAwayLaw_IsInsaneVertical(-2000))
		H.assert_true(not u.FlyAwayLaw_IsInsaneVertical(100))
		H.assert_true(u.FlyAwayLaw_IsInsaneHorizontal(3000, 0))
		H.assert_true(not u.FlyAwayLaw_ShouldSnapOrigin({
			elapsed_sec = 0.5,
			already_snapped = false,
			vel_z = 100,
			has_player_pos = true,
		}))
		H.assert_true(u.FlyAwayLaw_ShouldSnapOrigin({
			elapsed_sec = 0.5,
			already_snapped = false,
			vel_z = 2000,
			has_player_pos = true,
		}))
		H.assert_true(not u.FlyAwayLaw_ShouldSnapOrigin({
			elapsed_sec = 0.5,
			already_snapped = true,
			vel_z = 2000,
			has_player_pos = true,
		}))
		H.assert_true(not u.FlyAwayLaw_ShouldSnapOrigin({
			elapsed_sec = 10,
			already_snapped = false,
			vel_z = 2000,
			has_player_pos = true,
		}))
		local ok = u.FlyAwayLaw_Decide({
			in_vehicle = false,
			action_set = "/actions/main",
			origin_set_to_feet = true,
			expect_action_set = true,
			vel_z = 0,
			has_player_pos = true,
			elapsed_sec = 0.5,
		})
		H.assert_true(ok.path_ok)
		H.assert_eq(u.FlyAwayLaw_StatusLabel(ok), "FLY · OK")
		local he = u.FlyAwayLaw_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_ok")
		H.assert_true(string.find(he.checklist, "G34", 1, true))
		local fly = u.FlyAwayLaw_Decide({
			origin_set_to_feet = true,
			has_player_pos = true,
			elapsed_sec = 0.2,
			vel_z = 3000,
			action_set = "/actions/main",
			expect_action_set = true,
		})
		H.assert_true(fly.should_snap)
		H.assert_eq(fly.risk, "fly_away")
		H.assert_true(u.FlyAwayLaw_IsFlyAwayRisk(fly))
		H.assert_eq(u.FlyAwayLaw_StatusLabel(fly), "FLY · SNAP ORIGIN")
		local dead = u.FlyAwayLaw_Decide({
			origin_set_to_feet = true,
			action_set = "",
			expect_action_set = true,
		})
		H.assert_eq(dead.risk, "dead_input")
		local heD = u.FlyAwayLaw_HmdExpect(dead)
		H.assert_eq(heD.verdict, "expect_dead_input")
	end)

	-- G33 pure swap-eyes content-only law (W4; no dual pose fork)
	H.TEST("util.swap_eyes_law.content_only_g33", function()
		local u = env.vrmod.utils
		H.assert_true(not u.SwapEyesLaw_CubeDefault())
		H.assert_true(not u.SwapEyesLaw_FromAny(0))
		H.assert_true(not u.SwapEyesLaw_FromAny(false))
		H.assert_true(u.SwapEyesLaw_FromAny(1))
		H.assert_true(u.SwapEyesLaw_FromAny("true"))
		H.assert_true(not u.SwapEyesLaw_AllowDualPoseFork())
		H.assert_true(u.SwapEyesLaw_PreserveIpdFov())
		local lx, rx = u.SwapEyesLaw_ResolveSbsHalves(512, false)
		H.assert_eq(lx, 0)
		H.assert_eq(rx, 512)
		lx, rx = u.SwapEyesLaw_ResolveSbsHalves(512, true)
		H.assert_eq(lx, 512)
		H.assert_eq(rx, 0)
		H.assert_eq(u.SwapEyesLaw_LogicalLeftHalf(false), "left")
		H.assert_eq(u.SwapEyesLaw_LogicalLeftHalf(true), "right")
		local nat = u.SwapEyesLaw_Decide({ swap = false, rt_half_w = 640 })
		H.assert_true(nat.path_ok)
		H.assert_eq(nat.left_x, 0)
		H.assert_eq(nat.right_x, 640)
		H.assert_eq(u.SwapEyesLaw_StatusLabel(nat), "EYE · NATURAL")
		local he = u.SwapEyesLaw_HmdExpect(nat)
		H.assert_eq(he.verdict, "expect_natural")
		H.assert_true(string.find(he.checklist, "G33", 1, true))
		local sw = u.SwapEyesLaw_Decide({ swap = true, rt_half_w = 640 })
		H.assert_true(sw.path_ok)
		H.assert_eq(sw.left_x, 640)
		H.assert_eq(sw.right_x, 0)
		H.assert_eq(u.SwapEyesLaw_StatusLabel(sw), "EYE · SWAP CONTENT")
		local heS = u.SwapEyesLaw_HmdExpect(sw)
		H.assert_eq(heS.verdict, "expect_swap")
		local fork = u.SwapEyesLaw_Decide({ swap = true, dual_pose_fork = true })
		H.assert_true(not fork.path_ok)
		H.assert_eq(fork.risk, "dual_pose")
		H.assert_true(u.SwapEyesLaw_IsForkRisk(fork))
		local ipd = u.SwapEyesLaw_Decide({ swap = true, ipd_mutated = true })
		H.assert_eq(ipd.risk, "ipd_mutated")
		local fov = u.SwapEyesLaw_Decide({ swap = true, fov_swapped = true })
		H.assert_eq(fov.risk, "fov_fork")
	end)

	-- G32 pure stereo ShareTexture / HMD self-test law (W7 honest toast)
	H.TEST("util.stereo_selftest_law.w7_toast_g32", function()
		local u = env.vrmod.utils
		H.assert_eq(u.StereoSelfTest_DelaySeconds(), 2.5)
		H.assert_true(u.StereoSelfTest_RequireToastOnShareFail())
		H.assert_true(u.StereoSelfTest_RequireToastOnNoHmd())
		H.assert_true(not u.StereoSelfTest_AbortVrOnFail())
		H.assert_true(u.StereoSelfTest_ShouldToastShareBegin(false))
		H.assert_true(not u.StereoSelfTest_ShouldToastShareBegin(true))
		H.assert_true(u.StereoSelfTest_ShouldToastShareFinish(false))
		H.assert_true(u.StereoSelfTest_ShareOk(true, true))
		H.assert_true(not u.StereoSelfTest_ShareOk(true, false))
		H.assert_true(u.StereoSelfTest_ShouldToastNoHmd(false, false))
		H.assert_true(not u.StereoSelfTest_ShouldToastNoHmd(true, false))
		H.assert_true(not u.StereoSelfTest_ShouldToastNoHmd(false, true))
		H.assert_true(u.StereoSelfTest_ShouldToastUnhealthyShare(true, false, false))
		H.assert_true(not u.StereoSelfTest_ShouldToastUnhealthyShare(false, false, false))
		H.assert_true(string.find(u.StereoSelfTest_NoHmdToast(), "HMD", 1, true))
		local ok = u.StereoSelfTest_Decide({
			ok_begin = true,
			ok_finish = true,
			has_hmd = true,
			share_ok = true,
			selftest_done = true,
		})
		H.assert_true(ok.path_ok)
		H.assert_eq(u.StereoSelfTest_StatusLabel(ok), "STEREO · OK")
		local he = u.StereoSelfTest_HmdExpect(ok)
		H.assert_eq(he.verdict, "expect_ok")
		H.assert_true(string.find(he.checklist, "G32", 1, true))
		local beginFail = u.StereoSelfTest_Decide({
			ok_begin = false,
			ok_finish = true,
			toast_share_begin = true,
		})
		H.assert_eq(beginFail.risk, "share_begin")
		H.assert_eq(u.StereoSelfTest_StatusLabel(beginFail), "STEREO · SHARE BEGIN FAIL")
		local noHmd = u.StereoSelfTest_Decide({
			ok_begin = true,
			ok_finish = true,
			has_hmd = false,
			selftest_done = false,
			toast_no_hmd = true,
		})
		H.assert_eq(noHmd.risk, "no_hmd")
		H.assert_eq(u.StereoSelfTest_StatusLabel(noHmd), "STEREO · NO HMD")
		local heF = u.StereoSelfTest_HmdExpect(noHmd)
		H.assert_eq(heF.verdict, "expect_fail_honest")
		local silent = u.StereoSelfTest_Decide({
			ok_begin = false,
			ok_finish = true,
			toast_share_begin = false,
		})
		H.assert_eq(silent.risk, "silent_fail")
		H.assert_true(u.StereoSelfTest_IsSilentFailRisk(silent))
		H.assert_eq(u.StereoSelfTest_StatusLabel(silent), "STEREO · SILENT FAIL")
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
		-- G45: primary-left SoT — left laser+click; right must not steal
		H.assert_true(u.LaserLaw_AllowLaserFromHand("left", "left"))
		H.assert_true(not u.LaserLaw_AllowLaserFromHand("right", "left"))
		H.assert_true(u.LaserLaw_IsWrongHandPrimaryClick("boolean_primaryfire", "left"))
		H.assert_true(not u.LaserLaw_IsWrongHandPrimaryClick("boolean_left_primaryfire", "left"))
		local leftOk = u.LaserLaw_Decide({
			vr_active = true,
			laser_on = true,
			has_primary_pose = true,
			menu_focus = true,
			primary_hand = "left",
			laser_hand = "left",
			click_action = "boolean_left_primaryfire",
		})
		H.assert_true(leftOk.path_ok)
		H.assert_eq(leftOk.primary_hand, "left")
		H.assert_true(leftOk.menu_primary_click)
		H.assert_eq(u.LaserLaw_StatusLabel(leftOk), "LASER · FOCUS")
		local steal = u.LaserLaw_Decide({
			vr_active = true,
			laser_on = true,
			has_primary_pose = true,
			primary_hand = "left",
			laser_hand = "left",
			click_action = "boolean_primaryfire",
		})
		H.assert_eq(steal.risk, "steal")
		H.assert_true(not steal.path_ok)
		H.assert_true(u.LaserLaw_IsStealRisk(steal))
		H.assert_eq(u.LaserLaw_HmdExpect(steal).verdict, "expect_steal")
		local dual = u.LaserLaw_Decide({
			vr_active = true,
			laser_on = true,
			has_primary_pose = true,
			primary_hand = "left",
			laser_hand = "right",
		})
		H.assert_eq(dual.risk, "dual")
		H.assert_eq(u.LaserLaw_StatusLabel(dual), "LASER · DUAL FAIL")
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
		-- Product law: both HMD eyes from left UV under mq2 (not one black eye)
		H.assert_true(he2.expect_both_eyes)
		H.assert_eq(he2.flash_risk, "mq2_mono_both")
		H.assert_eq(he2.verdict, "expect_mq2_single")
		H.assert_true(not u.StereoLoad_FlashRiskIsBad(he2)) -- mono-both is law, not void
		local heIdle = u.StereoLoad_HmdExpect(idle)
		H.assert_eq(heIdle.verdict, "expect_no_submit")
		H.assert_true(u.StereoLoad_FlashRiskIsBad(heIdle))
	end)
end
