-- Source 2D skybox cube. Same mapping as engine/gl_warp.cpp MakeSkyVec.
-- Source axes: X forward, Y left, Z up. The cube is centered on the camera.
-- Draw axis 1..6 in order. The texture for that axis is SUFFIX[TEX_ORDER[axis] + 1]
-- among rt, bk, lf, ft, up, dn (R_LoadNamedSkys order, then skytexorder).
-- UVs include the half-texel inset and the V flip from that function.
local M = {}

M.SUFFIX = { "rt", "bk", "lf", "ft", "up", "dn" }
-- 0-based texture index for each draw axis. skytexorder = {0,2,1,3,4,5}.
M.TEX_ORDER = { 0, 2, 1, 3, 4, 5 }

-- 1 = s, 2 = t, 3 = width. Negative selects the negated component.
local ST = {
	{ 3, -1, 2 },
	{ -3, 1, 2 },
	{ 1, 3, 2 },
	{ -1, -3, 2 },
	{ -2, -1, 3 },
	{ 2, -1, -3 },
}

function M.face_suffix(axis)
	local idx = M.TEX_ORDER[axis]
	if not idx then
		return nil
	end
	return M.SUFFIX[idx + 1]
end

-- s,t in [-1, 1]. axis is 1..6. width is the face distance in source units.
-- Returns source-space offset from the eye, then u, v.
function M.make_sky_vec(s, t, axis, width)
	if s < -1 then
		s = -1
	elseif s > 1 then
		s = 1
	end
	if t < -1 then
		t = -1
	elseif t > 1 then
		t = 1
	end
	local b = { s * width, t * width, width }
	local row = ST[axis]
	local v = { 0, 0, 0 }
	for j = 1, 3 do
		local k = row[j]
		if k < 0 then
			v[j] = -b[-k]
		else
			v[j] = b[k]
		end
	end
	local u = (s + 1) * 0.5
	local vv = (t + 1) * 0.5
	if u < 1 / 512 then
		u = 1 / 512
	elseif u > 511 / 512 then
		u = 511 / 512
	end
	if vv < 1 / 512 then
		vv = 1 / 512
	elseif vv > 511 / 512 then
		vv = 511 / 512
	end
	vv = 1 - vv
	return v[1], v[2], v[3], u, vv
end

-- Six vertices, two triangles, order (-1,-1), (-1,1), (1,1), (1,-1) split 0,1,2 and 0,2,3.
-- Flat list: x, y, z, u, v per corner. Source units, eye-relative.
function M.face_tris(axis, width)
	local st = {
		-1, -1,
		-1, 1,
		1, 1,
		-1, -1,
		1, 1,
		1, -1,
	}
	local out = {}
	for i = 0, 5 do
		local x, y, z, u, v = M.make_sky_vec(st[i * 2 + 1], st[i * 2 + 2], axis, width)
		out[#out + 1] = x
		out[#out + 1] = y
		out[#out + 1] = z
		out[#out + 1] = u
		out[#out + 1] = v
	end
	return out
end

return M
