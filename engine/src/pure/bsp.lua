-- VBSP 20 reader. Seeks lumps, so a 500 MB pak is not loaded.
-- Brushes: dbrush_t 12 bytes, dbrushside_t 8 bytes, dplane_t 20 bytes.
-- Faces: 56 bytes. Displacements: ddispinfo_t 176 bytes, CDispVert 20 bytes.
-- Halfspace convention n·p <= d was checked against gm_flatgrass spawn.
-- Submodel faces and brushes are relative to the entity origin (func_brush and the like).
local ffi = require("ffi")
local bin = require("pure.bin")
local trace = require("pure.trace")
local angles = require("pure.angles")

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
		-- Source angles are pitch, yaw, roll. Yaw 0 looks along +X.
		if e.angles then
			local p, y, r = e.angles:match("([%-%d%.]+)%s+([%-%d%.]+)%s+([%-%d%.]+)")
			e.apitch, e.ayaw, e.aroll = tonumber(p), tonumber(y), tonumber(r)
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

-- x, y, z, albedo u, v, lightmap u, v, unused, displacement blend.
-- Blend 0 is $basetexture and 1 is $basetexture2. With a lighting lump the
-- lightmap slots are atlas coordinates. Without one they stay 1, 1, 1.
M.VERT_STRIDE = 9

-- One square atlas. gm_construct's flat pages are under a million luxels.
M.LIGHTMAP_SIZE = 4096

-- CDispVert.m_flAlpha is the Hammer blend paint, 0 through 255.
function M.disp_blend(alpha)
	if not alpha then
		return 0
	end
	local k = alpha / 255
	if k < 0 then
		return 0
	end
	if k > 1 then
		return 1
	end
	return k
end

-- ColorRGBExp32. linear = byte * 2^exp / 255, then a 2.2 display gamma so outdoor luxels stay visible.
function M.display_light(r, g, b, exp)
	if not r then
		return 1, 1, 1
	end
	if exp >= 128 then
		exp = exp - 256
	end
	local scale = 2 ^ exp / 255
	local function curve(c)
		c = c * scale
		if c <= 0 then
			return 0
		end
		local out = c ^ (1 / 2.2)
		if out > 1 then
			return 1
		end
		return out
	end
	return curve(r), curve(g), curve(b)
end

-- Move a plane n·p = d by a translation. n stays; d picks up n·origin.
function M.shift_plane(nx, ny, nz, d, ox, oy, oz)
	return d + nx * ox + ny * oy + nz * oz
end

-- Brush-entity vertex. VBSP subtracts the entity origin at compile time.
-- AngleMatrix then puts it back: rotate, then add origin. Nil org is identity.
function M.bmodel_point(x, y, z, org)
	if not org then
		return x, y, z
	end
	local pitch, yaw, roll = org.pitch or 0, org.yaw or 0, org.roll or 0
	if pitch ~= 0 or yaw ~= 0 or roll ~= 0 then
		x, y, z = angles.rotate(pitch, yaw, roll, x, y, z)
	end
	return x + (org.x or 0), y + (org.y or 0), z + (org.z or 0)
end

local function hidden_brush_entity(class)
	if class == "func_vehicleclip" or class == "func_areaportal" or class == "func_areaportalwindow" then
		return true
	end
	if class == "func_occluder" or class == "func_viscluster" then
		return true
	end
	if class and class:sub(1, 8) == "trigger_" then
		return true
	end
	return false
end

-- 3D skybox vertex into world space. sky_camera use_angles is left to the caller.
-- Source places the sky camera at sky_origin + eye/scale, which bakes to (p - origin) * scale.
function M.sky_place(x, y, z, cam)
	local s = cam.scale
	if not s or s == 0 then
		s = 16
	end
	return (x - cam.x) * s, (y - cam.y) * s, (z - cam.z) * s
end

-- textureVecs are two rows of xyz + offset. u = s/width, v = t/height, Source top-left v.
function M.tex_uv(x, y, z, vecs, tw, th)
	local s = x * vecs[1] + y * vecs[2] + z * vecs[3] + vecs[4]
	local t = x * vecs[5] + y * vecs[6] + z * vecs[7] + vecs[8]
	return s / tw, t / th
end

local SURF_SKIP = bit.bor(0x2, 0x4, 0x40, 0x80, 0x100, 0x200)

local function skip_surface(name, flags)
	if bit.band(flags, SURF_SKIP) ~= 0 then
		return true
	end
	local n = name:lower()
	if n:find("tools/", 1, true) then
		return true
	end
	return false
end

local function cstr_from(s, zero_off)
	local i = zero_off + 1
	if i < 1 or i > #s then
		return ""
	end
	local z = s:find("\0", i, true)
	if not z then
		return s:sub(i)
	end
	return s:sub(i, z - 1)
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
	local texinfo_s, texdata_s, sdata, stable
	if opts.mesh or opts.uv then
		vert_s = grab(3)
		edge_s = grab(12)
		surf_s = grab(13)
		face_s = grab(7)
		disp_s = grab(26)
		dvert_s = grab(33)
	end
	local lighting = ""
	if opts.uv then
		texinfo_s = grab(6)
		texdata_s = grab(2)
		sdata = grab(43)
		stable = grab(44)
		lighting = grab(8)
	end
	brush_s = grab(18)
	side_s = grab(19)
	local model_s = grab(14)
	local node_s = grab(5)
	local leaf_s = grab(10)
	local leafbrush_s = grab(17)
	local pak_ofs, pak_len = lump_info(header, 40)
	f:close()

	local entities, class_counts = parse_entities(ent_s)
	local sky = nil
	for i = 1, #entities do
		local e = entities[i]
		if e.classname == "sky_camera" and e.ox then
			sky = {
				x = e.ox,
				y = e.oy,
				z = e.oz,
				scale = tonumber(e.scale) or 16,
				yaw = e.ayaw or 0,
				use_angles = e.use_angles == "1",
			}
			break
		end
	end
	local by_model = {}
	for i = 1, #entities do
		local e = entities[i]
		local n = e.model and e.model:match("^%*(%d+)$")
		n = n and tonumber(n)
		if n and not by_model[n] then
			by_model[n] = {
				x = e.ox or 0,
				y = e.oy or 0,
				z = e.oz or 0,
				pitch = e.apitch or 0,
				yaw = e.ayaw or 0,
				roll = e.aroll or 0,
				skip = hidden_brush_entity(e.classname),
			}
		end
	end

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

	-- dmodel_t is 48 bytes, dnode_t and this file's dleaf_t are 32.
	-- Submodel headnodes own the brushes VBSP stored in entity-local space.
	local face_place = {}
	local leaf_ok = #leaf_s % 32 == 0 and #node_s % 32 == 0 and #model_s % 48 == 0
	if leaf_ok and #model_s >= 48 then
		local function claim_brushes(head, into)
			if head < 0 then
				return
			end
			local seen = {}
			local stack = { head }
			local steps = 0
			while #stack > 0 and steps < 100000 do
				steps = steps + 1
				local n = table.remove(stack)
				if n < 0 then
					local leaf = -n - 1
					if leaf >= 0 and (leaf + 1) * 32 <= #leaf_s then
						local base = leaf * 32
						local firstb = bin.u16(leaf_s, base + 25)
						local numb = bin.u16(leaf_s, base + 27)
						for i = 0, numb - 1 do
							local lb = (firstb + i) * 2 + 1
							if lb + 1 <= #leafbrush_s then
								local b = bin.u16(leafbrush_s, lb)
								if b >= 0 and b < nbrush and into[b] == nil then
									into[b] = true
								end
							end
						end
					end
				elseif not seen[n] and (n + 1) * 32 <= #node_s then
					seen[n] = true
					local base = n * 32
					stack[#stack + 1] = bin.i32(node_s, base + 5)
					stack[#stack + 1] = bin.i32(node_s, base + 9)
				end
			end
		end
		local world_head = bin.i32(model_s, 37)
		local world_brushes = {}
		claim_brushes(world_head, world_brushes)
		local nmodels = #model_s / 48
		for m = 1, nmodels - 1 do
			local org = by_model[m]
			local o = m * 48 + 1
			local firstf = bin.i32(model_s, o + 40)
			local numf = bin.i32(model_s, o + 44)
			if org and numf > 0 and firstf >= 0 then
				for fi = firstf, firstf + numf - 1 do
					face_place[fi] = org
				end
			end
			if org then
				local owned = {}
				claim_brushes(bin.i32(model_s, o + 36), owned)
				for b in pairs(owned) do
					if not world_brushes[b] then
						local brush = brushes[b + 1]
						local src = brush.p
						local moved = {}
						for i = 1, #src, 4 do
							local nx, ny, nz, d = src[i], src[i + 1], src[i + 2], src[i + 3]
							local len2 = nx * nx + ny * ny + nz * nz
							if len2 < 1e-12 then
								moved[#moved + 1] = nx
								moved[#moved + 1] = ny
								moved[#moved + 1] = nz
								moved[#moved + 1] = d
							else
								local s = d / len2
								local px, py, pz = M.bmodel_point(nx * s, ny * s, nz * s, org)
								nx, ny, nz = M.bmodel_point(nx, ny, nz, {
									pitch = org.pitch,
									yaw = org.yaw,
									roll = org.roll,
								})
								moved[#moved + 1] = nx
								moved[#moved + 1] = ny
								moved[#moved + 1] = nz
								moved[#moved + 1] = nx * px + ny * py + nz * pz
							end
						end
						local mins, maxs = brush_aabb(moved)
						if mins then
							brush.p = moved
							brush.mins = mins
							brush.maxs = maxs
						end
					end
				end
			end
		end
	end

	local mesh = nil
	local surfaces = nil
	local tri_count = 0
	local disp_count = 0
	local lightmap = nil
	if (opts.mesh or opts.uv) and face_s ~= "" then
		if opts.mesh then
			mesh = {}
		end
		local groups = {}
		if opts.uv then
			surfaces = {}
		end
		local td_cache = {}
		local info_cache = {}
		local function texdata_info(td)
			local hit = td_cache[td]
			if hit ~= nil then
				return hit or nil
			end
			if not texdata_s or td < 0 or (td + 1) * 32 > #texdata_s then
				td_cache[td] = false
				return nil
			end
			local o = td * 32 + 1
			local sid = bin.i32(texdata_s, o + 12)
			local tw = bin.i32(texdata_s, o + 16)
			local th = bin.i32(texdata_s, o + 20)
			if not stable or sid < 0 or (sid + 1) * 4 > #stable then
				td_cache[td] = false
				return nil
			end
			local name = cstr_from(sdata, bin.i32(stable, sid * 4 + 1))
			hit = { name = name, tw = tw, th = th }
			td_cache[td] = hit
			return hit
		end
		local function face_mat(fi)
			local hit = info_cache[fi]
			if hit ~= nil then
				return hit or nil
			end
			local ti = bin.i16(face_s, fi * 56 + 11)
			if ti < 0 or not texinfo_s or (ti + 1) * 72 > #texinfo_s then
				info_cache[fi] = false
				return nil
			end
			local o = ti * 72 + 1
			local flags = bin.i32(texinfo_s, o + 64)
			local td = bin.i32(texinfo_s, o + 68)
			local info = texdata_info(td)
			if not info or info.tw <= 0 or info.th <= 0 or skip_surface(info.name, flags) then
				info_cache[fi] = false
				return nil
			end
			local vecs = {}
			local lvecs = {}
			for k = 0, 7 do
				vecs[k + 1] = bin.f32(texinfo_s, o + k * 4)
				lvecs[k + 1] = bin.f32(texinfo_s, o + 32 + k * 4)
			end
			hit = {
				name = info.name, vecs = vecs, lvecs = lvecs,
				tw = info.tw, th = info.th, flags = flags,
			}
			info_cache[fi] = hit
			return hit
		end
		-- Shelf-pack each face's flat lightmap page. Pixel (0,0) stays white
		-- for unlit faces. Packing starts at (2,2) so a 1px extrusion cannot
		-- cover that texel, and two pad pixels keep neighbors from blending.
		local AW = M.LIGHTMAP_SIZE
		local AH = AW
		local white_u, white_v = 0.5 / AW, 0.5 / AH
		local atlas, lptr
		local pen_x, pen_y, row_h = 2, 2, 0
		local packed_n, dropped_n = 0, 0
		local use_atlas = false
		if lighting ~= "" then
			local ok, buf = pcall(ffi.new, "uint8_t[?]", AW * AH * 4)
			if ok then
				atlas = buf
				ffi.fill(atlas, AW * AH * 4, 255)
				lptr = ffi.cast("const uint8_t*", lighting)
				use_atlas = true
			end
		end
		local light_cache = {}
		local function blit_page(L)
			local rx, ry, w, h = L.rx, L.ry, L.row, L.height
			local base = L.base
			for y = 0, h - 1 do
				local row_o = base + y * w * 4
				local dst = ((ry + y) * AW + rx) * 4
				for x = 0, w - 1 do
					local o = row_o + x * 4
					local r, g, b = M.display_light(lptr[o], lptr[o + 1], lptr[o + 2], lptr[o + 3])
					local p = dst + x * 4
					atlas[p] = math.floor(r * 255 + 0.5)
					atlas[p + 1] = math.floor(g * 255 + 0.5)
					atlas[p + 2] = math.floor(b * 255 + 0.5)
					atlas[p + 3] = 255
				end
			end
			for y = 0, h - 1 do
				local src = ((ry + y) * AW + rx) * 4
				local left = ((ry + y) * AW + (rx - 1)) * 4
				local src_r = ((ry + y) * AW + (rx + w - 1)) * 4
				local right = ((ry + y) * AW + (rx + w)) * 4
				atlas[left], atlas[left + 1], atlas[left + 2], atlas[left + 3] = atlas[src], atlas[src + 1], atlas[src + 2], 255
				atlas[right], atlas[right + 1], atlas[right + 2], atlas[right + 3] = atlas[src_r], atlas[src_r + 1], atlas[src_r + 2], 255
			end
			for x = -1, w do
				local top_s = (ry * AW + (rx + x)) * 4
				local top_d = ((ry - 1) * AW + (rx + x)) * 4
				local bot_s = ((ry + h - 1) * AW + (rx + x)) * 4
				local bot_d = ((ry + h) * AW + (rx + x)) * 4
				atlas[top_d], atlas[top_d + 1], atlas[top_d + 2], atlas[top_d + 3] = atlas[top_s], atlas[top_s + 1], atlas[top_s + 2], 255
				atlas[bot_d], atlas[bot_d + 1], atlas[bot_d + 2], atlas[bot_d + 3] = atlas[bot_s], atlas[bot_s + 1], atlas[bot_s + 2], 255
			end
		end
		local function face_light(fi, mat)
			local L = light_cache[fi]
			if L ~= nil then
				return L or nil
			end
			local base = fi * 56
			local style0 = face_s:byte(base + 17)
			local lightofs = bin.i32(face_s, base + 21)
			local sx = bin.i32(face_s, base + 37)
			local sy = bin.i32(face_s, base + 41)
			if not style0 or style0 == 255 or lightofs < 0 or sx < 0 or sy < 0 then
				L = false
			else
				local row = sx + 1
				local height = sy + 1
				-- Bumped faces store 3 directional pages, then the flat page.
				local slice = bit.band(mat.flags or 0, 0x800) ~= 0 and 3 or 0
				local bytes = row * height * 4
				local page = lightofs + slice * bytes
				if page < 0 or page + bytes > #lighting then
					L = false
				else
					L = {
						minx = bin.i32(face_s, base + 29),
						miny = bin.i32(face_s, base + 33),
						sx = sx,
						sy = sy,
						row = row,
						height = height,
						base = page,
					}
				end
			end
			light_cache[fi] = L
			return L or nil
		end
		local function pack_light(L)
			if L.placed then
				return L.ok
			end
			L.placed = true
			local w, h = L.row, L.height
			if w < 1 or h < 1 or w + 3 > AW then
				L.ok = false
				dropped_n = dropped_n + 1
				return false
			end
			if pen_x + w + 1 > AW then
				pen_x = 2
				pen_y = pen_y + row_h + 2
				row_h = 0
			end
			if pen_y + h + 1 > AH then
				L.ok = false
				dropped_n = dropped_n + 1
				return false
			end
			L.rx, L.ry, L.ok = pen_x, pen_y, true
			pen_x = pen_x + w + 2
			if h > row_h then
				row_h = h
			end
			packed_n = packed_n + 1
			blit_page(L)
			return true
		end
		local function light_uv(fi, x, y, z, mat)
			local L = face_light(fi, mat)
			if not L or not pack_light(L) then
				return white_u, white_v
			end
			local lv = mat.lvecs
			local s = x * lv[1] + y * lv[2] + z * lv[3] + lv[4] - L.minx
			local t = x * lv[5] + y * lv[6] + z * lv[7] + lv[8] - L.miny
			if s < 0 then
				s = 0
			elseif s > L.sx then
				s = L.sx
			end
			if t < 0 then
				t = 0
			elseif t > L.sy then
				t = L.sy
			end
			return (L.rx + s + 0.5) / AW, (L.ry + t + 0.5) / AH
		end
		local function surface_buf(mat)
			local g = groups[mat.name]
			if not g then
				g = { name = mat.name, verts = {}, lit = use_atlas }
				groups[mat.name] = g
				surfaces[#surfaces + 1] = g
			end
			return g.verts
		end
		local function near_sky(p)
			if not sky then
				return false
			end
			local dx, dy, dz = p.x - sky.x, p.y - sky.y, p.z - sky.z
			return dx * dx + dy * dy + dz * dz < 6000 * 6000
		end
		local function push_uv(buf, p, mat, fi, sky_face, place, sample, blend)
			-- UVs and luxels stay in the vertex space VBSP wrote (local for a bmodel).
			-- A displacement samples the base quad. The lightmap is parameterized on that
			-- quad; the displaced point projects into the wrong luxel.
			local q = sample or p
			local u, v = M.tex_uv(q.x, q.y, q.z, mat.vecs, mat.tw, mat.th)
			local lu, lv, extra = 1, 1, 1
			if use_atlas then
				lu, lv = light_uv(fi, q.x, q.y, q.z, mat)
				extra = 0
			end
			local x, y, z = M.bmodel_point(p.x, p.y, p.z, place)
			if sky_face then
				x, y, z = M.sky_place(x, y, z, sky)
			end
			buf[#buf + 1] = x
			buf[#buf + 1] = y
			buf[#buf + 1] = z
			buf[#buf + 1] = u
			buf[#buf + 1] = v
			buf[#buf + 1] = lu
			buf[#buf + 1] = lv
			buf[#buf + 1] = extra
			buf[#buf + 1] = blend or 0
		end
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
			local place = face_place[fi]
			local hidden = place and place.skip
			local mat = (not hidden) and opts.uv and face_mat(fi) or nil
			if (opts.mesh and not hidden) or mat then
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
								bx = basep.x,
								by = basep.y,
								bz = basep.z,
								blend = M.disp_blend(bin.f32(dvert_s, vo + 16)),
							}
						end
					end
					local uvbuf = mat and surface_buf(mat) or nil
					local sx0, sy0, sz0 = M.bmodel_point(pts[1].x, pts[1].y, pts[1].z, place)
					local sky_face = near_sky({ x = sx0, y = sy0, z = sz0 })
					local function draw_pt(p)
						local x, y, z = M.bmodel_point(p.x, p.y, p.z, place)
						return { x = x, y = y, z = z }
					end
					for y = 0, size - 1 do
						for x = 0, size - 1 do
							local p00, p10 = grid[y][x], grid[y][x + 1]
							local p01, p11 = grid[y + 1][x], grid[y + 1][x + 1]
							if mesh then
								local nrm = tri_normal(p00, p10, p11)
								local r, g, b = shade(nrm, fi)
								push_tri(mesh, draw_pt(p00), draw_pt(p10), draw_pt(p11), r, g, b)
								push_tri(mesh, draw_pt(p00), draw_pt(p11), draw_pt(p01), r, g, b)
							end
							if uvbuf then
								local function push_disp(p)
									push_uv(uvbuf, p, mat, fi, sky_face, place, {
										x = p.bx, y = p.by, z = p.bz,
									}, p.blend)
								end
								push_disp(p00)
								push_disp(p10)
								push_disp(p11)
								push_disp(p00)
								push_disp(p11)
								push_disp(p01)
							end
							tri_count = tri_count + 2
						end
					end
				end
			else
				local pts = face_points(face_s, edge_s, surf_s, vert_s, fi)
				if #pts >= 3 then
					local uvbuf = mat and surface_buf(mat) or nil
					local sx0, sy0, sz0 = M.bmodel_point(pts[1].x, pts[1].y, pts[1].z, place)
					local sky_face = near_sky({ x = sx0, y = sy0, z = sz0 })
					local function draw_pt(p)
						local x, y, z = M.bmodel_point(p.x, p.y, p.z, place)
						return { x = x, y = y, z = z }
					end
					local r, g, b
					if mesh then
						local nrm = tri_normal(pts[1], pts[2], pts[3])
						r, g, b = shade(nrm, fi)
					end
					for i = 2, #pts - 1 do
						if mesh then
							push_tri(mesh, draw_pt(pts[1]), draw_pt(pts[i]), draw_pt(pts[i + 1]), r, g, b)
						end
						if uvbuf then
							push_uv(uvbuf, pts[1], mat, fi, sky_face, place)
							push_uv(uvbuf, pts[i], mat, fi, sky_face, place)
							push_uv(uvbuf, pts[i + 1], mat, fi, sky_face, place)
						end
						tri_count = tri_count + 1
					end
				end
			end
			end
		end
		if use_atlas then
			lightmap = {
				w = AW,
				h = AH,
				rgba = ffi.string(atlas, AW * AH * 4),
				faces = packed_n,
				dropped = dropped_n,
			}
			atlas = nil
			lptr = nil
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
		surfaces = surfaces,
		tri_count = tri_count,
		disp_count = disp_count,
		pak_ofs = pak_ofs,
		pak_len = pak_len,
		sky = sky,
		lightmap = lightmap,
	}
end

return M
