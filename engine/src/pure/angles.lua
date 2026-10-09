-- Source AngleVectors / VectorAngles (QAngle: pitch, yaw, roll in degrees).
-- Port of ValveSoftware/source-sdk-2013 game/shared/util_shared.cpp formulas
-- used by CGameMovement::AirMove. Yaw 0 looks +X. Positive yaw turns toward +Y (left).
local M = {}

local function sincos_deg(d)
	local r = math.rad(d)
	return math.sin(r), math.cos(r)
end

function M.angle_vectors(pitch, yaw, roll)
	local sr, cr = sincos_deg(roll or 0)
	local sp, cp = sincos_deg(pitch or 0)
	local sy, cy = sincos_deg(yaw or 0)
	local forward = { x = cp * cy, y = cp * sy, z = -sp }
	local right = {
		x = (-1 * sr * sp * cy + -1 * cr * -sy),
		y = (-1 * sr * sp * sy + -1 * cr * cy),
		z = -1 * sr * cp,
	}
	local up = {
		x = (cr * sp * cy + -sr * -sy),
		y = (cr * sp * sy + -sr * cy),
		z = cr * cp,
	}
	return forward, right, up
end

function M.vector_angles(x, y, z)
	local pitch, yaw
	if y == 0 and x == 0 then
		yaw = 0
		if z > 0 then
			pitch = 270
		else
			pitch = 90
		end
	else
		yaw = math.atan2(y, x) * 180 / math.pi
		if yaw < 0 then
			yaw = yaw + 360
		end
		local tmp = math.sqrt(x * x + y * y)
		pitch = math.atan2(-z, tmp) * 180 / math.pi
		if pitch < 0 then
			pitch = pitch + 360
		end
	end
	return pitch, yaw, 0
end

-- Horizontal wish basis after Source zeros forward.z and right.z and renormalizes.
function M.wish_basis(yaw_deg)
	local forward, right = M.angle_vectors(0, yaw_deg, 0)
	return forward, right
end

-- Studio vertex → world. Model +X is forward, +Y is left, +Z is up.
-- AngleVectors' right is model -Y, so the left axis is -right.
function M.rotate(pitch, yaw, roll, x, y, z)
	local forward, right, up = M.angle_vectors(pitch, yaw, roll)
	return forward.x * x - right.x * y + up.x * z,
		forward.y * x - right.y * y + up.y * z,
		forward.z * x - right.z * y + up.z * z
end

return M
