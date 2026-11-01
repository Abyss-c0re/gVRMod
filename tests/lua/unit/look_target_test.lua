-- Look-target util: entity under the gaze, and the exact point.
return function(H, env)
	local u = env.vrmod.utils
	local mug = { id = "mug" }

	H.TEST("util.look_target.range", function()
		H.assert_eq(u.LookTarget_DefaultRange(), 12)
		H.assert_eq(u.LookTarget_ClampRange(nil), 12)
		H.assert_eq(u.LookTarget_ClampRange(0 / 0), 12)
		H.assert_eq(u.LookTarget_ClampRange("no"), 12)
		H.assert_eq(u.LookTarget_ClampRange(0), 0)
		H.assert_eq(u.LookTarget_ClampRange(-4), 0)
		H.assert_eq(u.LookTarget_ClampRange(100), 32)
		H.assert_eq(u.LookTarget_ClampRange("8.5"), 8.5)

		H.assert_eq(u.LookTarget_Boost(true, true), 5)
		H.assert_eq(u.LookTarget_Boost(false, true), 3)
		H.assert_eq(u.LookTarget_Boost(false, false), 1)

		-- Same inches rule as hand pickup: slider * 12 * boost.
		H.assert_near(u.LookTarget_MaxDistance(3.5, 1), 42, 1e-6)
		H.assert_near(u.LookTarget_MaxDistance(3.5, 5), 210, 1e-6)
		H.assert_eq(u.LookTarget_MaxDistance(0, 5), 0)
		H.assert_near(u.LookTarget_MaxDistance(100, 1), 32 * 12, 1e-6)
		H.assert_near(u.LookTarget_MaxDistance(nil, nil), 12 * 12, 1e-6)

		H.assert_true(u.LookTarget_Within(42, 3.5, 1))
		H.assert_false(u.LookTarget_Within(42.1, 3.5, 1))
		H.assert_true(u.LookTarget_Within(200, 3.5, 5))
		H.assert_false(u.LookTarget_Within(0 / 0, 3.5, 1))
		H.assert_false(u.LookTarget_Within(-1, 3.5, 1))

		H.assert_near(u.LookTarget_Distance({ x = 0, y = 0, z = 0 }, { x = 3, y = 4, z = 0 }), 5, 1e-6)
		H.assert_true(u.LookTarget_Distance(nil, { x = 0, y = 0, z = 0 }) == nil)
		H.assert_true(u.LookTarget_Distance({ x = 0 / 0, y = 0, z = 0 }, { x = 0, y = 0, z = 0 }) == nil)
	end)

	H.TEST("util.look_target.select", function()
		local ent, pos, why = u.LookTarget_Select({
			hit = true,
			ent = mug,
			pos = { x = 10, y = 2, z = 3 },
			origin = { x = 0, y = 2, z = 3 },
			dir = { x = 1, y = 0, z = 0 },
			max_dist = 40,
		})
		H.assert_true(ent == mug)
		H.assert_eq(why, "entity")
		H.assert_near(pos.x, 10, 1e-6)
		H.assert_near(pos.y, 2, 1e-6)
		H.assert_near(pos.z, 3, 1e-6)
		pos.x = 0
		H.assert_eq(mug.id, "mug")

		-- World is a place, not an object.
		ent, pos, why = u.LookTarget_Select({
			hit = true,
			ent = nil,
			pos = { x = 4, y = 0, z = 1 },
			origin = { x = 0, y = 0, z = 1 },
			dir = { x = 1, y = 0, z = 0 },
			max_dist = 20,
		})
		H.assert_true(ent == nil)
		H.assert_eq(why, "world")
		H.assert_near(pos.x, 4, 1e-6)

		ent, pos, why = u.LookTarget_Select({
			hit = true,
			hit_sky = true,
			ent = mug,
			pos = { x = 9, y = 0, z = 0 },
			origin = { x = 0, y = 0, z = 0 },
			dir = { x = 1, y = 0, z = 0 },
			max_dist = 20,
		})
		H.assert_true(ent == nil)
		H.assert_eq(why, "sky")
		H.assert_near(pos.x, 9, 1e-6)

		-- Miss: the point is max_dist along the look, even if dir is not a unit.
		ent, pos, why = u.LookTarget_Select({
			hit = false,
			origin = { x = 1, y = 2, z = 3 },
			dir = { x = 4, y = 0, z = 0 },
			max_dist = 10,
		})
		H.assert_true(ent == nil)
		H.assert_eq(why, "miss")
		H.assert_near(pos.x, 11, 1e-4)
		H.assert_near(pos.y, 2, 1e-4)
		H.assert_near(pos.z, 3, 1e-4)

		ent, pos, why = u.LookTarget_Select({
			hit = true,
			ent = mug,
			pos = { x = 50, y = 0, z = 0 },
			origin = { x = 0, y = 0, z = 0 },
			dir = { x = 1, y = 0, z = 0 },
			max_dist = 20,
		})
		H.assert_true(ent == nil)
		H.assert_eq(why, "past_range")
		H.assert_near(pos.x, 50, 1e-6)

		H.assert_eq(select(3, u.LookTarget_Select({ max_dist = 0, origin = { x = 0, y = 0, z = 0 }, dir = { x = 1, y = 0, z = 0 } })), "range")
		H.assert_eq(select(3, u.LookTarget_Select({ max_dist = 10, dir = { x = 1, y = 0, z = 0 } })), "no_origin")
		H.assert_eq(select(3, u.LookTarget_Select({ max_dist = 10, origin = { x = 0, y = 0, z = 0 } })), "no_dir")
		H.assert_eq(select(3, u.LookTarget_Select({
			max_dist = 10,
			hit = true,
			origin = { x = 0, y = 0, z = 0 },
			dir = { x = 0, y = 0, z = 0 },
		})), "no_dir")
		H.assert_eq(select(3, u.LookTarget_Select({
			max_dist = 10,
			hit = true,
			ent = mug,
			origin = { x = 0, y = 0, z = 0 },
			dir = { x = 1, y = 0, z = 0 },
		})), "no_pos")
		H.assert_eq(select(3, u.LookTarget_Select(nil)), "range")
	end)

	H.TEST("util.look_target.query", function()
		local prevGaze = vrmod.GetEyeGaze
		local prevTrace = util.TraceLine
		local prevHit = u.HitFilter

		vrmod.GetEyeGaze = function()
			return nil
		end
		local ent, pos, why = u.LookTarget_Query({ id = "ply" }, 100)
		H.assert_true(ent == nil and pos == nil)
		H.assert_eq(why, "no_gaze")

		vrmod.GetEyeGaze = function()
			return Vector(0, 0, 64), Angle(0, 0, 0)
		end
		local prop = { id = "crate" }
		util.TraceLine = function(tr)
			H.assert_near(tr.start.z, 64, 1e-4)
			H.assert_near(tr.start.x, u.LookTarget_EyeLead(), 1e-3)
			H.assert_near(tr.endpos.x, 80, 1e-3)
			return {
				Hit = true,
				HitSky = false,
				HitPos = Vector(30, 0, 64),
				Entity = prop,
			}
		end
		ent, pos, why = u.LookTarget_Query({ id = "ply" }, 80)
		H.assert_true(ent == prop)
		H.assert_eq(why, "entity")
		H.assert_near(pos.x, 30, 1e-4)
		H.assert_near(pos.y, 0, 1e-4)
		H.assert_near(pos.z, 64, 1e-4)
		pos.x = -5
		H.assert_eq(prop.id, "crate")

		local world = { IsWorld = function() return true end }
		util.TraceLine = function()
			return {
				Hit = true,
				HitPos = Vector(12, 4, 64),
				Entity = world,
			}
		end
		ent, pos, why = u.LookTarget_Query({ id = "ply" }, 80)
		H.assert_true(ent == nil)
		H.assert_eq(why, "world")
		H.assert_near(pos.x, 12, 1e-4)
		H.assert_near(pos.y, 4, 1e-4)

		util.TraceLine = function()
			return { Hit = true, HitSky = true, HitPos = Vector(70, 0, 64), Entity = prop }
		end
		ent, pos, why = u.LookTarget_Query({ id = "ply" }, 80)
		H.assert_true(ent == nil)
		H.assert_eq(why, "sky")

		-- Held prop and the player are not hits. The filter must skip them.
		local ply = { id = "ply" }
		local held = { id = "held" }
		g_VR.heldEntityRight = held
		local seen = {}
		util.TraceLine = function(tr)
			seen.player = tr.filter(ply)
			seen.held = tr.filter(held)
			seen.prop = tr.filter(prop)
			return { Hit = false }
		end
		u.HitFilter = function(e) return e == prop end
		ent, pos, why = u.LookTarget_Query(ply, 40)
		H.assert_false(seen.player)
		H.assert_false(seen.held)
		H.assert_true(seen.prop)
		H.assert_true(ent == nil)
		H.assert_eq(why, "miss")
		H.assert_near(pos.x, 40, 1e-3)
		H.assert_near(pos.z, 64, 1e-3)

		H.assert_eq(select(3, u.LookTarget_Query(ply, 0)), "range")
		H.assert_eq(select(3, u.LookTarget_Query({ __invalid = true }, 10)), "no_player")

		vrmod.GetEyeGaze = prevGaze
		util.TraceLine = prevTrace
		u.HitFilter = prevHit
		g_VR.heldEntityRight = nil
	end)

	H.TEST("util.look_target.choose", function()
		H.assert_eq(u.LookTarget_CloseRadius(), 18)
		H.assert_true(u.LookTarget_HandCloserThanHmd(10, 40))
		H.assert_false(u.LookTarget_HandCloserThanHmd(40, 10))
		H.assert_false(u.LookTarget_HandCloserThanHmd(20, 20))
		H.assert_false(u.LookTarget_HandCloserThanHmd(nil, 10))
		H.assert_false(u.LookTarget_HandCloserThanHmd(0 / 0, 10))

		H.assert_true(u.LookTarget_AllowHeldWeapon(false, true, false, true))
		H.assert_false(u.LookTarget_AllowHeldWeapon(true, true, true, false))
		H.assert_false(u.LookTarget_AllowHeldWeapon(true, false, false, true))
		H.assert_true(u.LookTarget_AllowHeldWeapon(true, false, true, true))
		H.assert_true(u.LookTarget_AllowHeldWeapon("0", "1", "0", "1"))

		local nearProp = { id = "near" }
		local looked = { id = "looked" }
		local function choose(opts)
			return u.LookTarget_ChoosePickup(opts)
		end

		-- Hand on a prop: gaze does not replace it, even when looking at something else.
		local c = choose({
			eye_enabled = true,
			hand_ent = nearProp,
			hand_dist = 8,
			eye_ent = looked,
			hand_to_eye = 80,
			hmd_to_eye = 30,
		})
		H.assert_eq(c.source, "hand")
		H.assert_eq(c.reason, "close_hand")
		H.assert_true(c.ent == nearProp)

		c = choose({
			eye_enabled = true,
			hand_ent = nearProp,
			hand_dist = 18,
			eye_ent = looked,
			hand_to_eye = 100,
			hmd_to_eye = 20,
		})
		H.assert_eq(c.reason, "close_hand")
		H.assert_true(c.ent == nearProp)

		-- Unmeasured hand distance keeps the hand result.
		c = choose({
			eye_enabled = true,
			hand_ent = nearProp,
			eye_ent = looked,
			hand_to_eye = 90,
			hmd_to_eye = 20,
		})
		H.assert_eq(c.reason, "hand_unmeasured")
		H.assert_true(c.ent == nearProp)

		-- Hand farther than the head: the looked-at object is the grab.
		c = choose({
			eye_enabled = "1",
			hand_ent = { id = "far-hand" },
			hand_dist = 40,
			eye_ent = looked,
			hand_to_eye = 70,
			hmd_to_eye = 25,
		})
		H.assert_eq(c.source, "eye")
		H.assert_eq(c.reason, "eye")
		H.assert_true(c.ent == looked)

		-- Equal distance stays with the eyes. Closer is strict.
		c = choose({
			eye_enabled = true,
			hand_dist = 50,
			eye_ent = looked,
			hand_to_eye = 30,
			hmd_to_eye = 30,
		})
		H.assert_eq(c.source, "eye")

		-- Controllers in front of the head are closer than the HMD to a prop
		-- you are looking at, and still outside the close radius. Gaze holds.
		c = choose({
			eye_enabled = true,
			hand_ent = { id = "far-hand" },
			hand_dist = 40,
			eye_ent = looked,
			hand_to_eye = 60,
			hmd_to_eye = 90,
		})
		H.assert_eq(c.source, "eye")
		H.assert_eq(c.reason, "eye")
		H.assert_true(c.ent == looked)

		-- Hand is on the looked-at prop but the hand search missed it.
		-- Still return that prop so the halo and the grip exist.
		c = choose({
			eye_enabled = true,
			eye_ent = looked,
			hand_to_eye = 15,
			hmd_to_eye = 50,
		})
		H.assert_eq(c.source, "eye")
		H.assert_eq(c.reason, "eye_reached")
		H.assert_true(c.ent == looked)

		-- Hand search found a different prop that is outside the close radius.
		-- The looked-at prop wins.
		local other = { id = "other" }
		c = choose({
			eye_enabled = true,
			hand_ent = other,
			hand_dist = 24,
			eye_ent = looked,
			hand_to_eye = 24,
			hmd_to_eye = 60,
		})
		H.assert_eq(c.source, "eye")
		H.assert_eq(c.reason, "eye")
		H.assert_true(c.ent == looked)

		-- Feature off: the existing hand target, including a far one.
		local far = { id = "far" }
		c = choose({
			eye_enabled = false,
			hand_ent = far,
			hand_dist = 40,
			eye_ent = looked,
			hand_to_eye = 90,
			hmd_to_eye = 10,
		})
		H.assert_eq(c.source, "hand")
		H.assert_eq(c.reason, "hand")
		H.assert_true(c.ent == far)

		c = choose({ eye_enabled = "0", eye_ent = looked, hand_to_eye = 90, hmd_to_eye = 10 })
		H.assert_eq(c.source, "none")
		H.assert_eq(c.reason, "none")

		c = choose(nil)
		H.assert_eq(c.source, "none")
	end)

	H.TEST("util.look_target.pickup_rows", function()
		local chunk = loadfile("addon/vrmod-x64/lua/vrmod/ui/sh_settings_catalog.lua")
		H.assert_true(chunk ~= nil)
		chunk()
		local eye, range
		for _, cat in ipairs(vrmod.SettingsCatalog) do
			for _, row in ipairs(cat.rows or {}) do
				if row.cvar == "vrmod_pickup_eye" then eye = row end
				if row.cvar == "vrmod_pickup_eye_range" then range = row end
			end
		end
		H.assert_eq(eye.kind, "bool")
		H.assert_eq(range.kind, "slider")
		H.assert_eq(range.min, 0)
		H.assert_eq(range.max, 32)
	end)
end
