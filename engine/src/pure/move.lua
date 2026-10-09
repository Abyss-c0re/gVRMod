-- Player movement. Order matches CGameMovement::FullWalkMove / CheckJumpButton /
-- Friction / Accelerate / AirAccelerate from Valve source-sdk-2013
-- src/game/shared/gamemovement.cpp (public SDK, not the GMod binary).
--
-- GMod's published difference is jump impulse: Player:SetJumpPower default 200
-- added to vertical velocity, where HL2 adds 160 (sqrt(2*600*21) under the
-- g_bMovementOptimizations assert). Profiles select that constant.
--
-- Water, ladders, ducking, and conveyors are not in this cut.
local angles = require("pure.angles")

local M = {}

M.TICK = 1 / 66
M.PROFILES = {
	hl2 = {
		gravity = 600,
		friction = 8,
		stopspeed = 100,
		accelerate = 10,
		airaccelerate = 10,
		air_speed_cap = 30,
		maxvelocity = 3500,
		jump_impulse = 160,
		maxspeed = 320,
		step = 18,
	},
	gmod = {
		gravity = 600,
		friction = 8,
		stopspeed = 100,
		accelerate = 10,
		airaccelerate = 10,
		air_speed_cap = 30,
		maxvelocity = 3500,
		jump_impulse = 200,
		maxspeed = 200,
		step = 18,
	},
}

local function v3(x, y, z)
	return { x = x or 0, y = y or 0, z = z or 0 }
end

local function add(a, b)
	return v3(a.x + b.x, a.y + b.y, a.z + b.z)
end

local function mul(a, s)
	return v3(a.x * s, a.y * s, a.z * s)
end

local function dot(a, b)
	return a.x * b.x + a.y * b.y + a.z * b.z
end

local function len(a)
	return math.sqrt(a.x * a.x + a.y * a.y + a.z * a.z)
end

local function normalize(a)
	local l = len(a)
	if l < 1e-12 then
		return v3(0, 0, 0), 0
	end
	return mul(a, 1 / l), l
end

function M.clip_velocity(vel, normal, overbounce)
	local backoff = dot(vel, normal) * overbounce
	local out = v3(vel.x - normal.x * backoff, vel.y - normal.y * backoff, vel.z - normal.z * backoff)
	local adjust = dot(out, normal)
	if adjust < 0 then
		out = v3(out.x - normal.x * adjust, out.y - normal.y * adjust, out.z - normal.z * adjust)
	end
	return out
end

function M.new_player(profile_name)
	local profile = M.PROFILES[profile_name or "gmod"]
	return {
		profile = profile,
		pos = v3(0, 0, 0),
		vel = v3(0, 0, 0),
		on_ground = true,
		yaw = 0,
		pitch = 0,
		forward_move = 0,
		side_move = 0,
		jump = false,
		old_jump = false,
		surface_friction = 1,
		maxspeed = profile.maxspeed,
	}
end

local function check_velocity(st)
	local m = st.profile.maxvelocity
	local v = st.vel
	if v.x > m then v.x = m elseif v.x < -m then v.x = -m end
	if v.y > m then v.y = m elseif v.y < -m then v.y = -m end
	if v.z > m then v.z = m elseif v.z < -m then v.z = -m end
end

local function start_gravity(st, dt)
	st.vel.z = st.vel.z - (st.profile.gravity * 0.5 * dt)
	check_velocity(st)
end

local function finish_gravity(st, dt)
	st.vel.z = st.vel.z - (st.profile.gravity * 0.5 * dt)
	check_velocity(st)
end

function M.friction(st, dt)
	local speed = len(st.vel)
	if speed < 0.1 then
		return
	end
	local control = speed < st.profile.stopspeed and st.profile.stopspeed or speed
	local drop = control * st.profile.friction * st.surface_friction * dt
	local newspeed = speed - drop
	if newspeed < 0 then
		newspeed = 0
	end
	if newspeed ~= speed then
		local scale = newspeed / speed
		st.vel = mul(st.vel, scale)
	end
end

function M.accelerate(st, wishdir, wishspeed, accel, dt)
	local currentspeed = dot(st.vel, wishdir)
	local addspeed = wishspeed - currentspeed
	if addspeed <= 0 then
		return
	end
	local accelspeed = accel * dt * wishspeed * st.surface_friction
	if accelspeed > addspeed then
		accelspeed = addspeed
	end
	st.vel = add(st.vel, mul(wishdir, accelspeed))
end

function M.air_accelerate(st, wishdir, wishspeed, accel, dt)
	local wishspd = wishspeed
	if wishspd > st.profile.air_speed_cap then
		wishspd = st.profile.air_speed_cap
	end
	local currentspeed = dot(st.vel, wishdir)
	local addspeed = wishspd - currentspeed
	if addspeed <= 0 then
		return
	end
	-- Source uses the uncapped wishspeed for the accel magnitude.
	local accelspeed = accel * wishspeed * dt * st.surface_friction
	if accelspeed > addspeed then
		accelspeed = addspeed
	end
	st.vel = add(st.vel, mul(wishdir, accelspeed))
end

local function wish(st)
	local forward, right = angles.wish_basis(st.yaw)
	forward.z, right.z = 0, 0
	forward = normalize(forward)
	right = normalize(right)
	local wv = v3(
		forward.x * st.forward_move + right.x * st.side_move,
		forward.y * st.forward_move + right.y * st.side_move,
		0
	)
	local dir, speed = normalize(wv)
	if speed > st.maxspeed and speed > 0 then
		local scale = st.maxspeed / speed
		wv = mul(wv, scale)
		dir, speed = normalize(wv)
		speed = st.maxspeed
	end
	return dir, speed
end

local function integrate(st, dt, world)
	local dest = add(st.pos, mul(st.vel, dt))
	if not world then
		st.pos = dest
		return
	end
	local trace = require("pure.trace")
	local mins, maxs = trace.HULL_MINS, trace.HULL_MAXS
	local function slide(pos, vel, remain)
		local tr = trace.hull(world, pos, add(pos, mul(vel, remain)), mins, maxs)
		if tr.startsolid then
			return pos, vel
		end
		local hitpos = tr.endpos
		local outv = vel
		if tr.hit then
			outv = M.clip_velocity(vel, tr.normal, 1)
			if tr.normal.z > 0.7 and outv.z < 0 then
				outv.z = 0
			end
			local leftover = remain * (1 - tr.fraction)
			if leftover > 1e-6 then
				local tr2 = trace.hull(world, hitpos, add(hitpos, mul(outv, leftover)), mins, maxs)
				if not tr2.startsolid then
					hitpos = tr2.endpos
					if tr2.hit then
						outv = M.clip_velocity(outv, tr2.normal, 1)
					end
				end
			end
			-- Step up if the first plane was a wall.
			if tr.normal.z < 0.7 then
				local up = v3(pos.x, pos.y, pos.z + st.profile.step)
				local uptr = trace.hull(world, pos, up, mins, maxs)
				if not uptr.startsolid and uptr.fraction > 0 then
					local stepped = uptr.endpos
					local flat = v3(dest.x, dest.y, stepped.z)
					local horiz = trace.hull(world, stepped, flat, mins, maxs)
					if not horiz.startsolid and horiz.fraction > tr.fraction then
						local down = v3(horiz.endpos.x, horiz.endpos.y, horiz.endpos.z - st.profile.step)
						local downtr = trace.hull(world, horiz.endpos, down, mins, maxs)
						if downtr.hit and downtr.normal.z > 0.7 then
							hitpos = downtr.endpos
							outv = vel
							outv.z = 0
						end
					end
				end
			end
		end
		return hitpos, outv
	end
	local pos, vel = slide(st.pos, st.vel, dt)
	st.pos, st.vel = pos, vel
	-- Ground snap.
	local down = trace.hull(world, st.pos, v3(st.pos.x, st.pos.y, st.pos.z - 2), mins, maxs)
	if down.hit and down.normal.z > 0.7 and st.vel.z <= 0 then
		st.pos = down.endpos
		st.vel.z = 0
		st.on_ground = true
	else
		st.on_ground = false
	end
end

function M.tick(st, dt, world)
	dt = dt or M.TICK
	start_gravity(st, dt)
	local jumped = false
	if st.jump then
		if st.on_ground and not st.old_jump then
			st.on_ground = false
			st.vel.z = st.vel.z + st.profile.jump_impulse
			finish_gravity(st, dt)
			jumped = true
		end
		st.old_jump = true
	else
		st.old_jump = false
	end
	if st.on_ground then
		st.vel.z = 0
		M.friction(st, dt)
	end
	check_velocity(st)
	local dir, speed = wish(st)
	if st.on_ground then
		if speed > 0 then
			M.accelerate(st, dir, speed, st.profile.accelerate, dt)
		end
	else
		if speed > 0 then
			M.air_accelerate(st, dir, speed, st.profile.airaccelerate, dt)
		end
	end
	integrate(st, dt, world)
	if not jumped then
		finish_gravity(st, dt)
	else
		-- CheckJumpButton already applied one FinishGravity. FullWalkMove applies another.
		finish_gravity(st, dt)
	end
	if st.on_ground then
		st.vel.z = 0
	end
	return st
end

return M
