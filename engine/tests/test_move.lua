return function(T)
	local move = require("pure.move")
	local dt = 1 / 66

	-- Friction, one frame, speed 200, sv_friction 8, stopspeed 100.
	-- control = 200, drop = 200 * 8 / 66, new speed = 200 - drop.
	local st = move.new_player("gmod")
	st.on_ground = true
	st.vel.x = 200
	move.friction(st, dt)
	local drop = 200 * 8 * dt
	T.near(st.vel.x, 200 - drop, 1e-6, "friction")

	-- Ground accelerate from rest. accelspeed = 10 * dt * 200 = 2000/66.
	st = move.new_player("gmod")
	st.on_ground = true
	st.forward_move = 200
	st.yaw = 0
	move.tick(st, dt, nil)
	T.near(st.vel.x, 2000 / 66, 1e-4, "ground accel")
	T.ok(st.on_ground, "stays grounded without a world")

	-- Air accelerate from rest reaches the 30 u/s cap in one tick
	-- because 10 * dt * wish 200 > 30, and the cap is the add target.
	st = move.new_player("gmod")
	st.on_ground = false
	st.forward_move = 200
	st.vel.x = 0
	st.vel.z = 0
	move.air_accelerate(st, { x = 1, y = 0, z = 0 }, 200, 10, dt)
	T.near(st.vel.x, 30, 1e-6, "air cap")

	-- GMod jump, first frame, no horizontal input.
	-- StartGravity: -300/66
	-- add 200, FinishGravity inside CheckJumpButton: another -300/66
	-- velocity during the move: 200 - 600/66
	-- position uses that velocity
	-- FullWalkMove FinishGravity: another -300/66
	st = move.new_player("gmod")
	st.on_ground = true
	st.jump = true
	local z0 = st.pos.z
	move.tick(st, dt, nil)
	local vz_move = 200 - 600 / 66
	T.near(st.pos.z - z0, vz_move * dt, 1e-4, "gmod jump dz")
	T.near(st.vel.z, vz_move - 300 / 66, 1e-4, "gmod jump vz")
	T.ok(not st.on_ground, "left the ground")

	-- Held jump does not add another impulse.
	local z1 = st.pos.z
	local vz = st.vel.z
	move.tick(st, dt, nil)
	T.near(st.pos.z - z1, (vz - 300 / 66) * dt, 1e-3, "second frame is gravity only")

	-- HL2 impulse is 160, not 200.
	st = move.new_player("hl2")
	st.on_ground = true
	st.jump = true
	move.tick(st, dt, nil)
	local hl = 160 - 600 / 66
	T.near(st.pos.z, hl * dt, 1e-4, "hl2 jump dz")

	-- sv_maxvelocity clamps a absurd component on the way through StartGravity.
	st = move.new_player("gmod")
	st.on_ground = false
	st.vel.z = -100000
	move.tick(st, dt, nil)
	T.ok(st.vel.z >= -3500 - 1, "maxvelocity floor")
	T.ok(st.vel.z <= 3500 + 1, "maxvelocity ceil")

	-- ClipVelocity: into a floor, overbounce 1, vertical component dies.
	local out = move.clip_velocity({ x = 10, y = 0, z = -50 }, { x = 0, y = 0, z = 1 }, 1)
	T.near(out.z, 0, 1e-6, "floor clip z")
	T.near(out.x, 10, 1e-6, "floor clip x")
end
