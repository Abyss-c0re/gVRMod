return function(H, env)
	local u = env.vrmod.utils

	local function still()
		return {
			moved = 0.2,
			vert_speed = 0,
			frame_age = 1,
			jump_pressed = false,
			same_frame = false,
			has_cache = true,
		}
	end

	H.TEST("util.hand_retrace.should", function()
		H.assert_eq(u.HandRetrace_MoveUnits(), 1.5)
		H.assert_eq(u.HandRetrace_VertUnits(), 120)
		H.assert_eq(u.HandRetrace_ReuseFrames(), 1)
		H.assert_eq(u.HandRetrace_AngDegrees(), 4)

		-- Still hand, calm vertical speed, cache is one frame old: reuse.
		H.assert_false(u.HandRetrace_Should(still()))
		local under = still()
		under.moved = 1.49
		H.assert_false(u.HandRetrace_Should(under))

		-- At the move threshold the hull is new.
		local step = still()
		step.moved = 1.5
		H.assert_true(u.HandRetrace_Should(step))

		-- Jump speed traces even if the hand sample did not move.
		local hop = still()
		hop.vert_speed = 120
		H.assert_true(u.HandRetrace_Should(hop))
		hop.vert_speed = -200
		H.assert_true(u.HandRetrace_Should(hop))
		hop.vert_speed = 119
		H.assert_false(u.HandRetrace_Should(hop))

		-- Just-pressed jump traces before velocity catches up.
		local edge = still()
		edge.jump_pressed = true
		edge.vert_speed = 0
		edge.moved = 0
		H.assert_true(u.HandRetrace_Should(edge))

		-- Same frame: do not trace that hull again, even on a jump.
		local dup = still()
		dup.same_frame = true
		dup.jump_pressed = true
		dup.vert_speed = 400
		dup.moved = 80
		dup.frame_age = 0
		dup.ang_deg = 90
		H.assert_false(u.HandRetrace_Should(dup))

		-- Cache older than one frame must trace again.
		local stale = still()
		stale.frame_age = 2
		H.assert_true(u.HandRetrace_Should(stale))

		-- No result yet, or unknown vertical speed: trace.
		H.assert_true(u.HandRetrace_Should({
			moved = 0,
			vert_speed = 0,
			frame_age = 1,
			has_cache = false,
			same_frame = true,
		}))
		local unknown = still()
		unknown.vert_speed = nil
		H.assert_true(u.HandRetrace_Should(unknown))

		-- Gun twist with a still grip still traces. A small wrist bob does not.
		local twist = still()
		twist.ang_deg = 4
		H.assert_true(u.HandRetrace_Should(twist))
		twist.ang_deg = 3.9
		H.assert_false(u.HandRetrace_Should(twist))

		-- age 0 is "already traced this frame" when a cache exists.
		local now = still()
		now.frame_age = 0
		now.jump_pressed = true
		H.assert_false(u.HandRetrace_Should(now))
	end)
end
