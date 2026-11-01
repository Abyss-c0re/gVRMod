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
end
