-- VBSP 20 reader. Seeks lumps, so a 500 MB pak is not loaded.
-- Brushes: dbrush_t 12 bytes, dbrushside_t 8 bytes, dplane_t 20 bytes.
-- Faces: 56 bytes. Displacements: ddispinfo_t 176 bytes, CDispVert 20 bytes.
-- Halfspace convention n·p <= d was checked against gm_flatgrass spawn.
local bin = require("pure.bin")
local trace = require("pure.trace")

local M = {}

M.PLAYER_MASK = trace.PLAYER_MASK

local function lump_info(header, index)
	local base = 9 + index * 16 -- header is 1-based; ident+version = 8 bytes, then lumps
	-- byte index of lump: 8 + index*16, 1-based = 9 + index*16
	local o = 9 + index * 16
	return bin.i32(header, o), bin.i32(header, o + 4), bin.i32(header, o + 8)
end

local function read_at(f, ofs, len)
	if len <= 0 then
		return ""
	end
	f:seek("set", ofs)
	local s = f:read(len)
	if not s or #s ~= len then
		error("short lump read " .. tostring(len))
	end
	return s
end

local function parse_entities(s)
	if s:byte(#s) == 0 then
		s = s:sub(1, #s - 1)
	end
	local list = {}
	local counts = {}
	for block in s:gmatch("%b{}") do
		local e = {}
		for k, v in block:gmatch('"([^"]+)"%s*"([^"]*)"') do
			if e[k] == nil then
				e[k] = v
			end
		end
		if e.origin then
			local x, y, z = e.origin:match("([%-%d%.]+)%s+([%-%d%.]+)%s+([%-%d%.]+)")
			e.ox, e.oy, e.oz = tonumber(x), tonumber(y), tonumber(z)
		end
		local cn = e.classname or "?"
		counts[cn] = (counts[cn] or 0) + 1
		list[#list + 1] = e
	end
	return list, counts
end

local function solve_vertex(planes, i, j, k)
	local function n(idx)
		local o = (idx - 1) * 4
		return planes[o + 1], planes[o + 2], planes[o + 3], planes[o + 4]
	end
	local a1, b1, c1, d1 = n(i)
	local a2, b2, c2, d2 = n(j)
	local a3, b3, c3, d3 = n(k)
	local det = a1 * (b2 * c3 - b3 * c2) - b1 * (a2 * c3 - a3 * c2) + c1 * (a2 * b3 - a3 * b2)
	if math.abs(det) < 1e-6 then
		return nil
	end
	local function det3(x1, y1, z1, x2, y2, z2, x3, y3, z3)
		return x1 * (y2 * z3 - y3 * z2) - y1 * (x2 * z3 - x3 * z2) + z1 * (x2 * y3 - x3 * y2)
	end
	local x = det3(d1, b1, c1, d2, b2, c2, d3, b3, c3) / det
	local y = det3(a1, d1, c1, a2, d2, c2, a3, d3, c3) / det
	local z = det3(a1, b1, d1, a2, b2, d2, a3, b3, d3) / det
	return x, y, z
end

local function brush_aabb(p)
	local nplanes = #p / 4
	local mins = { x = math.huge, y = math.huge, z = math.huge }
	local maxs = { x = -math.huge, y = -math.huge, z = -math.huge }
	local count = 0
	for i = 1, nplanes - 2 do
		for j = i + 1, nplanes - 1 do
			for k = j + 1, nplanes do
				local x, y, z = solve_vertex(p, i, j, k)
				if x then
					local inside = true
					for t = 1, nplanes do
						local o = (t - 1) * 4
						local dist = p[o + 1] * x + p[o + 2] * y + p[o + 3] * z - p[o + 4]
						if dist > 0.1 then
							inside = false
							break
						end
					end
					if inside then
						count = count + 1
						if x < mins.x then mins.x = x end
						if y < mins.y then mins.y = y end
						if z < mins.z then mins.z = z end
						if x > maxs.x then maxs.x = x end
						if y > maxs.y then maxs.y = y end
						if z > maxs.z then maxs.z = z end
					end
				end
			end
		end
	end
	if count == 0 then
		return nil, nil
	end
	return mins, maxs
end

local function face_points(faces, edges, surfedges, vertexes, fi)
	local base = fi * 56
	local firstedge = bin.i32(faces, base + 5)
	local numedges = bin.i16(faces, base + 9)
	local dispinfo = bin.i16(faces, base + 13)
	local pts = {}
	for i = 0, numedges - 1 do
		local se = bin.i32(surfedges, (firstedge + i) * 4 + 1)
		local v
		if se >= 0 then
			v = bin.u16(edges, se * 4 + 1)
		else
			v = bin.u16(edges, (-se) * 4 + 3)
		end
		local vb = v * 12 + 1
		pts[#pts + 1] = {
			x = bin.f32(vertexes, vb),
			y = bin.f32(vertexes, vb + 4),
			z = bin.f32(vertexes, vb + 8),
		}
	end
	return pts, dispinfo
end

local function push_tri(mesh, a, b, c, r, g, bcol)
	mesh[#mesh + 1] = a.x
	mesh[#mesh + 1] = a.y
	mesh[#mesh + 1] = a.z
	mesh[#mesh + 1] = r
	mesh[#mesh + 1] = g
	mesh[#mesh + 1] = bcol
	mesh[#mesh + 1] = b.x
	mesh[#mesh + 1] = b.y
	mesh[#mesh + 1] = b.z
	mesh[#mesh + 1] = r
	mesh[#mesh + 1] = g
	mesh[#mesh + 1] = bcol
	mesh[#mesh + 1] = c.x
	mesh[#mesh + 1] = c.y
	mesh[#mesh + 1] = c.z
	mesh[#mesh + 1] = r
	mesh[#mesh + 1] = g
	mesh[#mesh + 1] = bcol
end

local function shade(n, salt)
	local nz = n.z
	if nz < 0 then
		nz = 0
	end
	local wobble = (salt % 5) * 0.03
	return 0.28 + 0.22 * math.abs(n.x) + wobble, 0.32 + 0.40 * nz, 0.30 + 0.18 * math.abs(n.y)
end

local function tri_normal(a, b, c)
	local ux, uy, uz = b.x - a.x, b.y - a.y, b.z - a.z
	local vx, vy, vz = c.x - a.x, c.y - a.y, c.z - a.z
	local x = uy * vz - uz * vy
	local y = uz * vx - ux * vz
	local z = ux * vy - uy * vx
	local l = math.sqrt(x * x + y * y + z * z)
	if l < 1e-8 then
		return { x = 0, y = 0, z = 1 }
	end
	return { x = x / l, y = y / l, z = z / l }
end

function M.load(path, opts)
	opts = opts or {}
	local f = assert(io.open(path, "rb"))
	local header = f:read(8 + 64 * 16)
	assert(header and #header == 8 + 64 * 16, "short bsp header")
	local ident = header:sub(1, 4)
	assert(ident == "VBSP", "not a VBSP: " .. path)
	local version = bin.u32(header, 5)
	local function grab(index)
		local ofs, len = lump_info(header, index)
		if len <= 0 then
			return "", ofs, len
		end
		return read_at(f, ofs, len), ofs, len
	end
	local ent_s = grab(0)
	local plane_s = grab(1)
	local vert_s, edge_s, surf_s, face_s, brush_s, side_s, disp_s, dvert_s
	if opts.mesh then
		vert_s = grab(3)
		edge_s = grab(12)
		surf_s = grab(13)
		face_s = grab(7)
		disp_s = grab(26)
		dvert_s = grab(33)
	end
	brush_s = grab(18)
	side_s = grab(19)
	f:close()

	local entities, class_counts = parse_entities(ent_s)
	local brushes = {}
	local nbrush = #brush_s / 12
	for i = 0, nbrush - 1 do
		local o = i * 12 + 1
		local first = bin.i32(brush_s, o)
		local ns = bin.i32(brush_s, o + 4)
		local contents = bin.i32(brush_s, o + 8)
		local p = {}
		for s = 0, ns - 1 do
			local so = (first + s) * 8 + 1
			local planenum = bin.u16(side_s, so)
			local po = planenum * 20 + 1
			p[#p + 1] = bin.f32(plane_s, po)
			p[#p + 1] = bin.f32(plane_s, po + 4)
			p[#p + 1] = bin.f32(plane_s, po + 8)
			p[#p + 1] = bin.f32(plane_s, po + 12)
		end
		local mins, maxs = brush_aabb(p)
		local solid = bit.band(contents, M.PLAYER_MASK) ~= 0
		brushes[#brushes + 1] = {
			contents = contents,
			player_solid = solid,
			p = p,
			mins = mins,
			maxs = maxs,
		}
	end

	local mesh = nil
	local tri_count = 0
	local disp_count = 0
	if opts.mesh and face_s ~= "" then
		mesh = {}
		local nfaces = #face_s / 56
		local disp_by_face = {}
		if disp_s ~= "" then
			disp_count = #disp_s / 176
			for i = 0, disp_count - 1 do
				local rec = i * 176 + 1
				local face = bin.u16(disp_s, rec + 36)
				disp_by_face[face] = i
			end
		end
		for fi = 0, nfaces - 1 do
			local di = disp_by_face[fi]
			if di then
				local rec = di * 176 + 1
				local sx = bin.f32(disp_s, rec)
				local sy = bin.f32(disp_s, rec + 4)
				local sz = bin.f32(disp_s, rec + 8)
				local vertstart = bin.i32(disp_s, rec + 12)
				local power = bin.i32(disp_s, rec + 20)
				local pts = face_points(face_s, edge_s, surf_s, vert_s, fi)
				if power >= 2 and power <= 4 and #pts >= 4 then
					local c = { pts[1], pts[2], pts[3], pts[4] }
					local k = 1
					local best = math.huge
					for ci = 1, 4 do
						local dx, dy, dz = c[ci].x - sx, c[ci].y - sy, c[ci].z - sz
						local d2 = dx * dx + dy * dy + dz * dz
						if d2 < best then
							best = d2
							k = ci
						end
					end
					local rot = {}
					for i = 0, 3 do
						rot[i + 1] = c[(k - 1 + i) % 4 + 1]
					end
					local size = 2 ^ power
					local n = size + 1
					local grid = {}
					for y = 0, size do
						local ty = y / size
						grid[y] = {}
						for x = 0, size do
							local tx = x / size
							local function lerp(a, b, t)
								return {
									x = a.x + (b.x - a.x) * t,
									y = a.y + (b.y - a.y) * t,
									z = a.z + (b.z - a.z) * t,
								}
							end
							local a = lerp(rot[1], rot[2], tx)
							local b = lerp(rot[4], rot[3], tx)
							local basep = lerp(a, b, ty)
							local vo = (vertstart + y * n + x) * 20 + 1
							local vx = bin.f32(dvert_s, vo)
							local vy = bin.f32(dvert_s, vo + 4)
							local vz = bin.f32(dvert_s, vo + 8)
							local dist = bin.f32(dvert_s, vo + 12)
							grid[y][x] = {
								x = basep.x + vx * dist,
								y = basep.y + vy * dist,
								z = basep.z + vz * dist,
							}
						end
					end
					for y = 0, size - 1 do
						for x = 0, size - 1 do
							local p00, p10 = grid[y][x], grid[y][x + 1]
							local p01, p11 = grid[y + 1][x], grid[y + 1][x + 1]
							local nrm = tri_normal(p00, p10, p11)
							local r, g, b = shade(nrm, fi)
							push_tri(mesh, p00, p10, p11, r, g, b)
							push_tri(mesh, p00, p11, p01, r, g, b)
							tri_count = tri_count + 2
						end
					end
				end
			else
				local pts = face_points(face_s, edge_s, surf_s, vert_s, fi)
				if #pts >= 3 then
					local nrm = tri_normal(pts[1], pts[2], pts[3])
					local r, g, b = shade(nrm, fi)
					for i = 2, #pts - 1 do
						push_tri(mesh, pts[1], pts[i], pts[i + 1], r, g, b)
						tri_count = tri_count + 1
					end
				end
			end
		end
	elseif disp_s == nil then
		-- count disps without building a mesh
		local ofs, len = lump_info(header, 26)
		if len > 0 then
			disp_count = len / 176
		end
	end

	local spawns = {}
	for i = 1, #entities do
		local e = entities[i]
		if e.classname == "info_player_start" and e.ox then
			spawns[#spawns + 1] = e
		end
	end

	return {
		path = path,
		version = version,
		entities = entities,
		class_counts = class_counts,
		brushes = brushes,
		brush_count = #brushes,
		spawns = spawns,
		mesh = mesh,
		tri_count = tri_count,
		disp_count = disp_count,
	}
end

return M
