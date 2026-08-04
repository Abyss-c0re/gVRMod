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
end
