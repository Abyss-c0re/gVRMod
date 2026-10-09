-- OpenXR (right-handed, x right, y up, z back) ↔ Source (x forward, y left, z up).
-- Same remap as gVRMod ConvertXrPose in src/input/xr_input.cpp, tests/test_input.cpp.
-- The linear map is a proper rotation (det = +1, columns orthonormal). Scale is separate.
local units = require("pure.units")

local M = {}

M.DEFAULT_IPD_METERS = 0.064

function M.xr_to_source_axes(x, y, z)
	return -z, -x, y
end

function M.source_axes_to_xr(sx, sy, sz)
	return -sy, sz, -sx
end

-- Source `fov` is the horizontal angle on a 4:3 window. A wider window grows
-- the horizontal angle and keeps that 4:3 vertical angle. Returns full
-- horizontal and vertical angles in degrees.
function M.source_fov(fov_deg, w, h)
	local aspect = w / h
	local ratio = aspect / (4 / 3)
	local half_h = math.atan(math.tan(math.rad(fov_deg) * 0.5) * ratio)
	local half_v = math.atan(math.tan(half_h) / aspect)
	return math.deg(half_h * 2), math.deg(half_v * 2)
end

-- Source units → LÖVR/OpenXR meters at a view scale (source units per real meter).
function M.source_to_lovr(sx, sy, sz, view_scale)
	local s = view_scale or units.VRMOD_VIEW_SCALE
	local x, y, z = M.source_axes_to_xr(sx, sy, sz)
	return x / s, y / s, z / s
end

function M.lovr_to_source(x, y, z, view_scale)
	local s = view_scale or units.VRMOD_VIEW_SCALE
	local sx, sy, sz = M.xr_to_source_axes(x, y, z)
	return sx * s, sy * s, sz * s
end

-- Columns of the XR→Source matrix, in source coordinates.
M.COLUMNS = {
	{ 0, -1, 0 },
	{ 0, 0, 1 },
	{ -1, 0, 0 },
}

function M.det()
	local c = M.COLUMNS
	-- det of matrix with these columns
	local a, b, d = c[1], c[2], c[3]
	return a[1] * (b[2] * d[3] - b[3] * d[2])
		- a[2] * (b[1] * d[3] - b[3] * d[1])
		+ a[3] * (b[1] * d[2] - b[2] * d[1])
end

function M.columns_orthonormal()
	local function dot(p, q)
		return p[1] * q[1] + p[2] * q[2] + p[3] * q[3]
	end
	local c = M.COLUMNS
	for i = 1, 3 do
		if math.abs(dot(c[i], c[i]) - 1) > 1e-9 then
			return false
		end
		for j = i + 1, 3 do
			if math.abs(dot(c[i], c[j])) > 1e-9 then
				return false
			end
		end
	end
	return true
end

-- Head-right in LÖVR meters for a Source yaw (degrees, positive yaw turns left).
function M.head_right_lovr(yaw_deg)
	local yaw = math.rad(yaw_deg)
	local sx = math.sin(yaw)
	local sy = -math.cos(yaw) -- source right at this yaw: (sin yaw, -cos yaw, 0)
	local x, y, z = M.source_axes_to_xr(sx, sy, 0)
	return x, y, z
end

-- Eye centers in LÖVR meters. ipd is meters. Positive eye_sign is the right eye (+1 / -1).
function M.eye_center(eye_sx, eye_sy, eye_sz, yaw_deg, eye_sign, ipd_m, view_scale)
	local cx, cy, cz = M.source_to_lovr(eye_sx, eye_sy, eye_sz, view_scale)
	local rx, ry, rz = M.head_right_lovr(yaw_deg)
	local h = (ipd_m or M.DEFAULT_IPD_METERS) * 0.5 * eye_sign
	return cx + rx * h, cy + ry * h, cz + rz * h
end

-- Pinhole. Camera looks down -Z. fov_y in degrees. Returns pixel x, y (origin top-left).
function M.project(cam_x, cam_y, cam_z, fov_y_deg, width, height)
	local depth = -cam_z
	local f = (height * 0.5) / math.tan(math.rad(fov_y_deg) * 0.5)
	local px = width * 0.5 + f * cam_x / depth
	local py = height * 0.5 - f * cam_y / depth
	return px, py
end

-- Horizontal disparity in pixels for a point on the optical axis at depth Z meters.
function M.stereo_disparity_px(ipd_m, depth_m, fov_y_deg, height)
	local f = (height * 0.5) / math.tan(math.rad(fov_y_deg) * 0.5)
	return f * ipd_m / depth_m
end

return M
