-- Swept AABB vs Source brushes.
-- A brush is the intersection of halfspaces n·p <= d (planes face outward).
-- Confirmed on gm_flatgrass / gm_construct: the hull standing on the floor
-- at info_player_start hits a z-up plane one unit down.
--
-- Hull expansion: origin O intersects the solid if for every plane
-- n·O + support_min(n) <= d, i.e. O is inside planes rewritten as
-- d' = d - support_min(n). support_min picks the hull corner most opposite n.
local M = {}

-- Standing hull. Source VEC_HULL_MIN/MAX for a human: (-16,-16,0) (16,16,72).
M.HULL_MINS = { x = -16, y = -16, z = 0 }
M.HULL_MAXS = { x = 16, y = 16, z = 72 }
M.PLAYER_MASK = 0x1 + 0x2 + 0x8 + 0x4000 + 0x10000

local function support_min(n, mins, maxs)
	return (n.x > 0 and mins.x or maxs.x) * n.x
		+ (n.y > 0 and mins.y or maxs.y) * n.y
		+ (n.z > 0 and mins.z or maxs.z) * n.z
end

local function expand(brush, mins, maxs)
	local out = {}
	local p = brush.p
	for i = 1, #p, 4 do
		local n = { x = p[i], y = p[i + 1], z = p[i + 2] }
		local d = p[i + 3] - support_min(n, mins, maxs)
		out[#out + 1] = n
		out[#out + 1] = d
	end
	return out
end

local function aabb_overlap_seg(brush, a, b, mins, maxs)
	if not brush.mins then
		return true
	end
	local function axis(a1, b1, lo, hi, pad0, pad1)
		local smin = math.min(a1, b1) + pad0
		local smax = math.max(a1, b1) + pad1
		return smax >= lo and smin <= hi
	end
	local bm, bM = brush.mins, brush.maxs
	return axis(a.x, b.x, bm.x, bM.x, mins.x, maxs.x)
		and axis(a.y, b.y, bm.y, bM.y, mins.y, maxs.y)
		and axis(a.z, b.z, bm.z, bM.z, mins.z, maxs.z)
end

-- Positive means the point is outside that plane. Touching is 0.
local function signed(n, d, p)
	return n.x * p.x + n.y * p.y + n.z * p.z - d
end

-- >0 strictly outside, 0 touching, <0 means outside was rejected and this is penetration depth.
local function penetration(planes, point)
	local pen = math.huge
	for i = 1, #planes, 2 do
		local dist = signed(planes[i], planes[i + 1], point)
		if dist > 0.05 then
			return -1
		end
		local into = -dist
		if into < pen then
			pen = into
		end
	end
	if pen == math.huge then
		return -1
	end
	return pen
end

-- Clip segment against one convex. Returns t0, t1, enter normal or nil if miss.
local function clip_brush(planes, start, dest)
	local dx = dest.x - start.x
	local dy = dest.y - start.y
	local dz = dest.z - start.z
	local t0, t1 = 0, 1
	local nenter = nil
	for i = 1, #planes, 2 do
		local n = planes[i]
		local d = planes[i + 1]
		local dist = signed(n, d, start)
		local denom = n.x * dx + n.y * dy + n.z * dz
		if math.abs(denom) < 1e-8 then
			if dist > 0.05 then
				return nil
			end
		else
			local t = -dist / denom
			if denom < 0 then
				-- t == 0 is a plane we are resting on and moving into.
				-- The old t > t0 test dropped that normal and the hull sank.
				if t > t1 then
					return nil
				end
				if t > t0 + 1e-9 or math.abs(t - t0) <= 1e-9 then
					if t > t0 then
						t0 = t
					end
					nenter = n
				end
			else
				if t < t1 then
					if t < t0 then
						return nil
					end
					t1 = t
				end
			end
		end
	end
	return t0, t1, nenter
end

function M.hull(world, start, dest, mins, maxs)
	mins = mins or M.HULL_MINS
	maxs = maxs or M.HULL_MAXS
	local best_t, best_n, best_i = nil, nil, nil
	local startsolid = false
	local brushes = world.brushes
	for i = 1, #brushes do
		local brush = brushes[i]
		if brush.player_solid ~= false then
			if aabb_overlap_seg(brush, start, dest, mins, maxs) then
				local planes = expand(brush, mins, maxs)
				local t0, t1, nenter = clip_brush(planes, start, dest)
				if t0 then
					local pen = penetration(planes, start)
					local moving_in = nenter
						and ((dest.x - start.x) * nenter.x + (dest.y - start.y) * nenter.y + (dest.z - start.z) * nenter.z) < -1e-8
					if pen > 0.1 and t0 <= 0 and t1 >= 0 then
						startsolid = true
					elseif t0 > 0 and t0 <= 1 and (not best_t or t0 < best_t) then
						best_t, best_n, best_i = t0, nenter, i
					elseif t0 <= 0 and t1 >= 0 and pen >= 0 and moving_in and (not best_t or 0 < best_t) then
						-- Resting on a plane and moving into it. Block, do not call it stuck.
						best_t, best_n, best_i = 0, nenter, i
					end
				end
			end
		end
	end
	if startsolid and not best_t then
		return {
			hit = true,
			startsolid = true,
			fraction = 0,
			endpos = { x = start.x, y = start.y, z = start.z },
			normal = { x = 0, y = 0, z = 1 },
		}
	end
	if not best_t then
		return {
			hit = false,
			startsolid = startsolid,
			fraction = 1,
			endpos = { x = dest.x, y = dest.y, z = dest.z },
			normal = { x = 0, y = 0, z = 1 },
		}
	end
	local f = best_t
	if f < 0 then
		f = 0
	end
	-- Pull back a hair so the next trace is not startsolid.
	local back = f > 0 and math.max(f - 1e-4, 0) or 0
	return {
		hit = true,
		startsolid = startsolid,
		fraction = f,
		brush = best_i,
		normal = { x = best_n.x, y = best_n.y, z = best_n.z },
		endpos = {
			x = start.x + (dest.x - start.x) * back,
			y = start.y + (dest.y - start.y) * back,
			z = start.z + (dest.z - start.z) * back,
		},
	}
end

function M.startsolid(world, pos, mins, maxs)
	local tr = M.hull(world, pos, pos, mins, maxs)
	return tr.startsolid
end

function M.nudge_up(world, pos, mins, maxs, max_units)
	max_units = max_units or 128
	if not M.startsolid(world, pos, mins, maxs) then
		return { x = pos.x, y = pos.y, z = pos.z }, 0
	end
	for z = 1, max_units do
		local p = { x = pos.x, y = pos.y, z = pos.z + z }
		if not M.startsolid(world, p, mins, maxs) then
			return p, z
		end
	end
	return { x = pos.x, y = pos.y, z = pos.z }, nil
end

-- Axis-aligned box brush. Solid where |x|.. wait: mins/maxs are the box corners.
function M.box_brush(minx, miny, minz, maxx, maxy, maxz)
	return {
		player_solid = true,
		mins = { x = minx, y = miny, z = minz },
		maxs = { x = maxx, y = maxy, z = maxz },
		p = {
			1, 0, 0, maxx,
			-1, 0, 0, -minx,
			0, 1, 0, maxy,
			0, -1, 0, -miny,
			0, 0, 1, maxz,
			0, 0, -1, -minz,
		},
	}
end

return M
