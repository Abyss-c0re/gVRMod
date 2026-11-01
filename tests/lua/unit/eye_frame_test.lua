-- Eye-frame wire, relative-frame copy, and flashlight aim.
-- Offline: no net library, no headset. The production tick calls the same helpers.
return function(H, env)
	local u = env.vrmod.utils

	local function nearAng(p, y, r, ep, ey, er, eps)
		eps = eps or 0.05
		H.assert_near(p, ep, eps)
		H.assert_near(y, ey, eps)
		H.assert_near(r, er, eps)
	end

	H.TEST("util.eye_frame.server_allowed", function()
		H.assert_true(u.EyeFrame_ServerAllowed(nil))
		H.assert_true(u.EyeFrame_ServerAllowed(true))
		H.assert_true(u.EyeFrame_ServerAllowed(1))
		H.assert_true(u.EyeFrame_ServerAllowed("1"))
		H.assert_false(u.EyeFrame_ServerAllowed(false))
		H.assert_false(u.EyeFrame_ServerAllowed(0))
		H.assert_false(u.EyeFrame_ServerAllowed("0"))
	end)

	H.TEST("util.eye_frame.should_send", function()
		local base = {
			server_allow = true,
			client_enabled = true,
			has_pose = true,
			pitch = 10,
			yaw = -20,
		}
		local ok, why = u.EyeFrame_ShouldSend(base)
		H.assert_true(ok)
		H.assert_eq(why, "send")

		-- Missing server convar allows. A real "0" does not.
		ok, why = u.EyeFrame_ShouldSend({
			client_enabled = true, has_pose = true, pitch = 0, yaw = 0,
		})
		H.assert_true(ok)
		H.assert_eq(why, "send")

		ok, why = u.EyeFrame_ShouldSend({
			server_allow = "0", client_enabled = true, has_pose = true, pitch = 1, yaw = 1,
		})
		H.assert_false(ok)
		H.assert_eq(why, "server")

		-- Server is checked before the client, so both off reports the server.
		ok, why = u.EyeFrame_ShouldSend({
			server_allow = false, client_enabled = false, has_pose = true, pitch = 1, yaw = 1,
		})
		H.assert_false(ok)
		H.assert_eq(why, "server")

		-- "0" is truthy in Lua. The helper must still treat it as off.
		ok, why = u.EyeFrame_ShouldSend({
			server_allow = true, client_enabled = "0", has_pose = true, pitch = 1, yaw = 1,
		})
		H.assert_false(ok)
		H.assert_eq(why, "client")

		ok, why = u.EyeFrame_ShouldSend({
			server_allow = true, client_enabled = 0, has_pose = true, pitch = 1, yaw = 1,
		})
		H.assert_false(ok)
		H.assert_eq(why, "client")

		ok, why = u.EyeFrame_ShouldSend({
			server_allow = 1, client_enabled = "1", has_pose = false, pitch = 1, yaw = 1,
		})
		H.assert_false(ok)
		H.assert_eq(why, "no_pose")

		-- No pose wins over a NaN sample.
		ok, why = u.EyeFrame_ShouldSend({
			server_allow = true, client_enabled = true, has_pose = false, pitch = 0 / 0, yaw = 0,
		})
		H.assert_false(ok)
		H.assert_eq(why, "no_pose")

		ok, why = u.EyeFrame_ShouldSend({
			server_allow = true, client_enabled = true, has_pose = true, pitch = 0 / 0, yaw = 0,
		})
		H.assert_false(ok)
		H.assert_eq(why, "nan")

		ok, why = u.EyeFrame_ShouldSend({
			server_allow = true, client_enabled = true, has_pose = true, pitch = 1, yaw = math.huge,
		})
		H.assert_false(ok)
		H.assert_eq(why, "nan")

		ok, why = u.EyeFrame_ShouldSend({
			server_allow = true, client_enabled = true, has_pose = true, finite = false, pitch = 1, yaw = 1,
		})
		H.assert_false(ok)
		H.assert_eq(why, "nan")

		ok, why = u.EyeFrame_ShouldSend({
			server_allow = true, client_enabled = true, has_pose = true,
		})
		H.assert_false(ok)
		H.assert_eq(why, "nan")

		H.assert_eq(select(2, u.EyeFrame_ShouldSend(nil)), "client")
	end)

	H.TEST("util.eye_frame.pack_roundtrip", function()
		local function roundtrip(p, y)
			local pu, yu = u.EyeFrame_PackLocal(p, y)
			H.assert_true(pu ~= nil and yu ~= nil, "pack " .. tostring(p) .. "," .. tostring(y))
			return u.EyeFrame_UnpackLocal(pu, yu)
		end

		local p, y = roundtrip(0, 0)
		H.assert_near(p, 0, 1e-6)
		H.assert_near(y, 0, 1e-6)

		p, y = roundtrip(-89, 90)
		H.assert_near(p, -89, 0.01)
		H.assert_near(y, 90, 0.01)

		p, y = roundtrip(89, -90)
		H.assert_near(p, 89, 0.01)
		H.assert_near(y, -90, 0.01)

		-- Pitch outside the legal range clamps. Yaw wraps.
		p, y = roundtrip(120, 540)
		H.assert_near(p, 89, 0.01)
		H.assert_near(y, 180, 0.01)

		p, y = roundtrip(-100, -270)
		H.assert_near(p, -89, 0.01)
		H.assert_near(y, 90, 0.01)

		p, y = roundtrip(0, -180)
		H.assert_near(p, 0, 0.01)
		H.assert_near(y, -180, 1e-6)

		-- +180 and -180 are the same direction. +180 lands on the last step.
		p, y = roundtrip(0, 180)
		H.assert_near(p, 0, 0.01)
		H.assert_true(math.abs(y) > 179.9)

		local pu, yu = u.EyeFrame_PackLocal(0 / 0, 0)
		H.assert_true(pu == nil and yu == nil)
		pu, yu = u.EyeFrame_PackLocal(0, math.huge)
		H.assert_true(pu == nil and yu == nil)
		pu, yu = u.EyeFrame_PackLocal("no", 0)
		H.assert_true(pu == nil and yu == nil)

		H.assert_true(u.EyeFrame_UnpackLocal(nil, 1) == nil)
		H.assert_true(u.EyeFrame_UnpackLocal(-1, 1) == nil)
		H.assert_true(u.EyeFrame_UnpackLocal(0, 70000) == nil)

		-- A hostile pitch word still comes back inside [-89, 89].
		p, y = u.EyeFrame_UnpackLocal(65535, 0)
		H.assert_near(p, 89, 0.01)
		H.assert_near(y, -180, 1e-4)
	end)

	H.TEST("util.eye_frame.wire_tail", function()
		H.assert_eq(u.EyeFrame_Magic(), 225)
		H.assert_eq(u.EyeFrame_TailBits(), 40)
		H.assert_eq(u.EyeFrame_TailBytes(), 5)
		H.assert_eq(u.EyeFrame_LegacyTickMaxBits(), 1200)
		H.assert_eq(u.EyeFrame_NetTickMaxBits(), 2048)
		H.assert_true(u.EyeFrame_NetTickMaxBits() >= u.EyeFrame_LegacyTickMaxBits() + u.EyeFrame_TailBits())

		local function capture()
			local ops = {}
			local function write(v, n)
				ops[#ops + 1] = { v = v, n = n }
			end
			return write, ops
		end

		local write, ops = capture()
		H.assert_eq(u.EyeFrame_WriteTail(write, {}), 0)
		H.assert_eq(#ops, 0)
		H.assert_eq(u.EyeFrame_WriteTail(write, { eyePitchU = 10 }), 0)
		H.assert_eq(#ops, 0)
		H.assert_eq(u.EyeFrame_WriteTail(write, { eyePitchU = 0 / 0, eyeYawU = 1 }), 0)
		H.assert_eq(u.EyeFrame_WriteTail(write, { eyePitchU = 70000, eyeYawU = 1 }), 0)
		H.assert_eq(#ops, 0)

		-- Eye off: the writer is not called. That is the old-client bit stream.
		local reads = 0
		local function countRead()
			reads = reads + 1
			return 0
		end
		H.assert_true(u.EyeFrame_ReadTail(0, countRead) == nil)
		H.assert_eq(reads, 0)
		H.assert_true(u.EyeFrame_ReadTail(4, countRead) == nil)
		H.assert_eq(reads, 0)

		reads = 0
		local function badMagic()
			reads = reads + 1
			return 1
		end
		H.assert_true(u.EyeFrame_ReadTail(5, badMagic) == nil)
		H.assert_eq(reads, 1)

		local pu, yu = u.EyeFrame_PackLocal(12.5, -40)
		write, ops = capture()
		H.assert_eq(u.EyeFrame_WriteTail(write, { eyePitchU = pu, eyeYawU = yu }), 40)
		H.assert_eq(#ops, 3)
		H.assert_eq(ops[1].v, 225)
		H.assert_eq(ops[1].n, 8)
		H.assert_eq(ops[2].n, 16)
		H.assert_eq(ops[3].n, 16)
		H.assert_eq(ops[2].v, pu)
		H.assert_eq(ops[3].v, yu)

		local i = 0
		local function read(n)
			i = i + 1
			H.assert_eq(n, ops[i].n)
			return ops[i].v
		end
		local tail = u.EyeFrame_ReadTail(5, read)
		H.assert_eq(tail.eyePitchU, pu)
		H.assert_eq(tail.eyeYawU, yu)
		H.assert_eq(i, 3)

		-- Zero is a legal word (it is -180 degrees, not "missing").
		write, ops = capture()
		H.assert_eq(u.EyeFrame_WriteTail(write, { eyePitchU = 0, eyeYawU = 0 }), 40)
		H.assert_eq(ops[2].v, 0)
		H.assert_eq(ops[3].v, 0)
	end)

	H.TEST("util.eye_frame.fields_and_frame", function()
		H.assert_true(u.EyeFrame_FieldsEqual({ eyePitchU = 3, eyeYawU = 4 }, { eyePitchU = 3, eyeYawU = 4 }))
		H.assert_true(u.EyeFrame_FieldsEqual({}, {}))
		H.assert_true(u.EyeFrame_FieldsEqual(nil, nil))
		H.assert_false(u.EyeFrame_FieldsEqual({}, { eyePitchU = 1, eyeYawU = 1 }))
		H.assert_false(u.EyeFrame_FieldsEqual({ eyePitchU = 3, eyeYawU = 4 }, {}))
		H.assert_false(u.EyeFrame_FieldsEqual({}, { eyePitchU = 3, eyeYawU = 4 }))
		H.assert_false(u.EyeFrame_FieldsEqual({ eyePitchU = 3, eyeYawU = 4 }, { eyePitchU = 3, eyeYawU = 5 }))
		-- One word is not a sample.
		H.assert_true(u.EyeFrame_FieldsEqual({ eyePitchU = 3 }, {}))

		local dst = { eyePitchU = 9, eyeYawU = 9 }
		u.EyeFrame_CopyFields(dst, { eyePitchU = 2, eyeYawU = 8 })
		H.assert_eq(dst.eyePitchU, 2)
		H.assert_eq(dst.eyeYawU, 8)
		u.EyeFrame_CopyFields(dst, {})
		H.assert_true(dst.eyePitchU == nil and dst.eyeYawU == nil)
		u.EyeFrame_CopyFields(dst, { eyePitchU = 4, eyeYawU = 5 })
		u.EyeFrame_Clear(dst)
		H.assert_true(dst.eyePitchU == nil and dst.eyeYawU == nil)
		H.assert_true(u.EyeFrame_Clear(nil) == nil)

		local function posed(eyeP, eyeY)
			local f = {
				characterYaw = 15,
				hmdPos = Vector(1, 2, 64),
				hmdAng = Angle(5, 20, 0),
				lefthandPos = Vector(0, 1, 40),
				lefthandAng = Angle(0, 10, 0),
				righthandPos = Vector(0, -1, 40),
				righthandAng = Angle(0, -10, 0),
			}
			for i = 1, 10 do
				f["finger" .. i] = i * 0.01
			end
			if eyeP ~= nil then
				f.eyePitchU = eyeP
				f.eyeYawU = eyeY
			end
			return f
		end

		local a = posed(100, 200)
		local b = posed(100, 200)
		H.assert_true(u.FramesAreEqual(a, b))
		local copy = u.CopyFrame(a)
		H.assert_eq(copy.eyePitchU, 100)
		H.assert_eq(copy.eyeYawU, 200)
		H.assert_true(u.FramesAreEqual(a, copy))

		-- Turning gaze off must not compare equal, in either argument order.
		local off = posed(nil, nil)
		H.assert_false(u.FramesAreEqual(a, off))
		H.assert_false(u.FramesAreEqual(off, a))
		H.assert_true(u.FramesAreEqual(off, posed(nil, nil)))

		local other = posed(100, 201)
		H.assert_false(u.FramesAreEqual(a, other))

		local prevWL, prevLT = WorldToLocal, LocalToWorld
		_G.WorldToLocal = function(pos, ang, opos, oang)
			return Vector(pos.x - opos.x, pos.y - opos.y, pos.z - opos.z), Angle(ang.p, ang.y - oang.y, ang.r)
		end
		_G.LocalToWorld = function(pos, ang, opos, oang)
			return Vector(pos.x + opos.x, pos.y + opos.y, pos.z + opos.z), Angle(ang.p, ang.y + oang.y, ang.r)
		end
		local rel = u.FrameToRelative(a, Vector(8, 0, 0), Angle(0, 90, 0))
		H.assert_eq(rel.eyePitchU, 100)
		H.assert_eq(rel.eyeYawU, 200)
		H.assert_near(rel.hmdAng.y, a.hmdAng.y - 90, 1e-4)
		local absf = u.FrameToAbsolute(rel, Vector(8, 0, 0), Angle(0, 90, 0))
		H.assert_eq(absf.eyePitchU, 100)
		H.assert_eq(absf.eyeYawU, 200)
		H.assert_near(absf.hmdAng.y, a.hmdAng.y, 1e-4)
		-- Tailless relative frame stays tailless (older client) while poses still convert.
		local relOff = u.FrameToRelative(off, Vector(0, 0, 0), Angle(0, 45, 0))
		H.assert_true(relOff.eyePitchU == nil and relOff.eyeYawU == nil)
		_G.WorldToLocal, _G.LocalToWorld = prevWL, prevLT
	end)

	H.TEST("util.eye_frame.compose", function()
		-- The offline mock's LocalToWorld returns the local angle unchanged.
		-- Nil it so this test locks the Source-style fallback the game uses
		-- when the engine function is absent. In GMod the engine wins.
		local prevL, prevW = LocalToWorld, WorldToLocal
		_G.LocalToWorld, _G.WorldToLocal = nil, nil
		local p, y, r = u.EyeFrame_ComposeAng(nil, nil, nil, 10, 20)
		nearAng(p, y, r, 10, 20, 0)

		p, y, r = u.EyeFrame_ComposeAng(0, 0, 0, 0, 0)
		nearAng(p, y, r, 0, 0, 0)

		p, y, r = u.EyeFrame_ComposeAng(0, 90, 0, 0, 0)
		nearAng(p, y, r, 0, 90, 0)

		p, y, r = u.EyeFrame_ComposeAng(0, 90, 0, 0, 10)
		nearAng(p, y, r, 0, 100, 0)

		p, y, r = u.EyeFrame_ComposeAng(30, 0, 0, 10, 0)
		nearAng(p, y, r, 40, 0, 0)

		p, y, r = u.EyeFrame_ComposeAng(0, 0, 40, 0, 0)
		nearAng(p, y, r, 0, 0, 40)

		-- HMD yaw 90, local pitch +10: look along +Y, slightly down.
		p, y, r = u.EyeFrame_ComposeAng(0, 90, 0, 10, 0)
		local cp = math.cos(math.rad(p))
		local fx = cp * math.cos(math.rad(y))
		local fy = cp * math.sin(math.rad(y))
		local fz = -math.sin(math.rad(p))
		H.assert_near(fx, 0, 1e-3)
		H.assert_near(fy, math.cos(math.rad(10)), 1e-3)
		H.assert_near(fz, -math.sin(math.rad(10)), 1e-3)

		p, y, r = u.EyeFrame_ComposeAng(0, 20, 90, 15, -10)
		H.assert_true(p == p and y == y and r == r)

		H.assert_true(select(1, u.EyeFrame_ComposeAng(0, 0, 0, 0 / 0, 0)) == nil)
		H.assert_true(select(1, u.EyeFrame_ComposeAng(0 / 0, 0, 0, 1, 2)) == nil)
		H.assert_true(select(1, u.EyeFrame_ComposeAng(0, math.huge, 0, 1, 2)) == nil)
		_G.LocalToWorld, _G.WorldToLocal = prevL, prevW
	end)

	H.TEST("util.eye_frame.fill_world", function()
		local prevL, prevW = LocalToWorld, WorldToLocal
		_G.LocalToWorld, _G.WorldToLocal = nil, nil
		local pu, yu = u.EyeFrame_PackLocal(0, 0)
		local dst = { hmdPos = Vector(10, 20, 30), hmdAng = Angle(0, 90, 0) }
		u.EyeFrame_FillWorld(dst, { eyePitchU = pu, eyeYawU = yu }, true)
		H.assert_true(dst.eyeActive)
		H.assert_near(dst.eyeAng.y, 90, 0.05)
		H.assert_near(dst.eyeAng.p, 0, 0.05)
		H.assert_eq(dst.eyePos.x, 10)
		dst.eyePos.x = 0
		H.assert_eq(dst.hmdPos.x, 10)

		-- Next tick on the same table, gaze gone: the stale look must die.
		u.EyeFrame_FillWorld(dst, {}, true)
		H.assert_false(dst.eyeActive)
		H.assert_true(dst.eyePos == nil and dst.eyeAng == nil)

		pu, yu = u.EyeFrame_PackLocal(5, 0)
		dst.hmdPos = Vector(1, 2, 3)
		u.EyeFrame_FillWorld(dst, { eyePitchU = pu, eyeYawU = yu }, false)
		H.assert_false(dst.eyeActive)
		H.assert_true(dst.eyePos == nil)

		-- nil allow is the missing server convar: keep the sample.
		u.EyeFrame_FillWorld(dst, { eyePitchU = pu, eyeYawU = yu }, nil)
		H.assert_true(dst.eyeActive)
		H.assert_near(dst.eyePitch, 5, 0.01)

		u.EyeFrame_FillWorld(dst, { eyePitchU = "no", eyeYawU = 1 }, true)
		H.assert_false(dst.eyeActive)

		u.EyeFrame_FillWorld(dst, { eyePitchU = pu, eyeYawU = yu }, "0")
		H.assert_false(dst.eyeActive)

		local bare = {}
		H.assert_true(u.EyeFrame_FillWorld(bare, { eyePitchU = pu, eyeYawU = yu }, true).eyeActive == false)
		H.assert_true(u.EyeFrame_FillWorld(nil, {}, true) == nil)
		_G.LocalToWorld, _G.WorldToLocal = prevL, prevW
	end)

	H.TEST("util.flashlight_aim.decide", function()
		local function d(opts)
			return u.FlashlightAim_Decide(opts)
		end

		local r = d({ aim = 0, fallback = 1, eye_enabled = true, eye_active = true })
		H.assert_eq(r.source, "right")
		H.assert_false(r.used_fallback)
		H.assert_eq(r.reason, "aim")

		r = d({ aim = 1 })
		H.assert_eq(r.source, "left")
		H.assert_eq(r.aim, 1)
		H.assert_false(r.used_fallback)

		r = d({ aim = 2.9 })
		H.assert_eq(r.source, "hmd")
		H.assert_eq(r.aim, 2)

		r = d({ aim = 4 })
		H.assert_eq(r.source, "right")
		H.assert_false(r.used_fallback)

		r = d({ aim = -1 })
		H.assert_eq(r.source, "right")

		r = d({ aim = nil })
		H.assert_eq(r.source, "right")

		r = d({ aim = "3", fallback = "1", eye_enabled = true, eye_active = true })
		H.assert_eq(r.source, "eye")
		H.assert_false(r.used_fallback)
		H.assert_eq(r.reason, "eye")
		H.assert_eq(r.aim, 3)

		r = d({ aim = 3, fallback = 1, eye_enabled = true, eye_active = false })
		H.assert_eq(r.source, "left")
		H.assert_true(r.used_fallback)
		H.assert_eq(r.reason, "eye_inactive")
		H.assert_eq(r.aim, 1)

		r = d({ aim = 3, fallback = 0, eye_enabled = false, eye_active = true })
		H.assert_eq(r.source, "right")
		H.assert_true(r.used_fallback)
		H.assert_eq(r.reason, "eye_off")

		r = d({ aim = 3, fallback = 5, eye_enabled = false, eye_active = false })
		H.assert_eq(r.source, "right")
		H.assert_true(r.used_fallback)
		H.assert_eq(r.reason, "eye_off")

		r = d({ aim = 3, fallback = -3, eye_enabled = "0", eye_active = "1" })
		H.assert_eq(r.source, "right")
		H.assert_eq(r.reason, "eye_off")

		r = d({ aim = 3, fallback = 1, eye_enabled = "1", eye_active = "1" })
		H.assert_eq(r.source, "eye")

		r = d(nil)
		H.assert_eq(r.source, "right")
		H.assert_false(r.used_fallback)
	end)

	H.TEST("util.eye_frame.settings_rows", function()
		local chunk = loadfile("addon/vrmod-x64/lua/vrmod/ui/sh_settings_catalog.lua")
		H.assert_true(chunk ~= nil, "catalog load")
		chunk()
		local function find(cvar)
			for _, cat in ipairs(vrmod.SettingsCatalog) do
				for _, row in ipairs(cat.rows or {}) do
					if row.cvar == cvar then return row, cat.id end
				end
			end
		end

		local aim, aimCat = find("vrmod_flashlight_aim")
		H.assert_eq(aimCat, "gameplay")
		H.assert_eq(aim.kind, "combo")
		local values = {}
		for _, ch in ipairs(aim.choices) do
			values[ch.value] = ch.text
		end
		H.assert_eq(values[0], "Right hand")
		H.assert_eq(values[1], "Left hand")
		H.assert_eq(values[2], "HMD")
		H.assert_true(values[3] ~= nil)
		H.assert_true(string.find(string.lower(values[3]), "eye", 1, true) ~= nil)

		local fb = find("vrmod_flashlight_eye_fallback")
		H.assert_eq(fb.kind, "combo")
		H.assert_eq(#fb.choices, 2)
		H.assert_eq(fb.choices[1].value, 0)
		H.assert_eq(fb.choices[2].value, 1)

		-- The old bool wrote 0/1 into a 0..2 int. It is not a settings row anymore.
		H.assert_true(find("vrmod_flashlight_attachment") == nil)

		local eye, eyeCat = find("vrmod_eyetracking")
		H.assert_eq(eyeCat, "character")
		H.assert_eq(eye.kind, "bool")
		local sv, svCat = find("vrmod_sv_eyetracking")
		H.assert_eq(svCat, "character")
		H.assert_eq(sv.kind, "bool")
	end)
end
