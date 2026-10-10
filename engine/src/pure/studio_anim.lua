-- One Source sequence frame, skinned with the studio bind matrices.
-- Sequence bytes stay in the MDL or the sibling .ani. No skeleton is invented.
local bin = require("pure.bin")

local M = {}

local BONE = 216
local SEQ = 212
local ANIM = 100
local RAWPOS = 0x01
local RAWROT = 0x02
local ANIMPOS = 0x04
local ANIMROT = 0x08
local ANIMDELTA = 0x10
local RAWROT2 = 0x20
local STUDIO_DELTA = 0x0004
local BONE_FIXED_ALIGNMENT = 0x00100000

local PREFER = {
	"idle_subtle",
	"idle_unarmed",
	"idle_all_01",
	"idle01",
	"idle_01",
	"idle1",
	"idle",
	"idle_angry",
	"reference",
}

local function i32(s, off0)
	return bin.i32(s, off0 + 1)
end

local function u16(s, off0)
	return bin.u16(s, off0 + 1)
end

local function i16(s, off0)
	return bin.i16(s, off0 + 1)
end

local function f32(s, off0)
	return bin.f32(s, off0 + 1)
end

local function cstr(s, off0)
	local i = off0 + 1
	if not s or i < 1 or i > #s then
		return ""
	end
	local z = s:find("\0", i, true)
	if not z then
		return s:sub(i)
	end
	return s:sub(i, z - 1)
end

local function half(h)
	local sign = h >= 32768
	local exp = math.floor((h % 32768) / 1024)
	local mant = h % 1024
	local f
	if exp == 0 then
		f = mant == 0 and 0 or math.ldexp(mant / 1024, -14)
	elseif exp == 31 then
		f = 0
	else
		f = math.ldexp(1 + mant / 1024, exp - 15)
	end
	if sign then
		return -f
	end
	return f
end

local function getbits(lo, hi, start, width)
	if start + width <= 32 then
		return math.floor(lo / 2 ^ start) % 2 ^ width
	end
	if start >= 32 then
		return math.floor(hi / 2 ^ (start - 32)) % 2 ^ width
	end
	local low_w = 32 - start
	local low = math.floor(lo / 2 ^ start) % 2 ^ low_w
	local high = hi % 2 ^ (width - low_w)
	return low + high * 2 ^ low_w
end

local function quat48(s, off0)
	if off0 < 0 or off0 + 6 > #s then
		return nil
	end
	local x = u16(s, off0)
	local y = u16(s, off0 + 2)
	local zw = u16(s, off0 + 4)
	local qx = (x - 32768) * (1 / 32768)
	local qy = (y - 32768) * (1 / 32768)
	local qz = (zw % 32768 - 16384) * (1 / 16384)
	local w2 = 1 - qx * qx - qy * qy - qz * qz
	local qw = w2 > 0 and math.sqrt(w2) or 0
	if zw >= 32768 then
		qw = -qw
	end
	return { qx, qy, qz, qw }
end

local function quat64(s, off0)
	if off0 < 0 or off0 + 8 > #s then
		return nil
	end
	local b1, b2, b3, b4, b5, b6, b7, b8 = s:byte(off0 + 1, off0 + 8)
	local lo = b1 + b2 * 256 + b3 * 65536 + b4 * 16777216
	local hi = b5 + b6 * 256 + b7 * 65536 + b8 * 16777216
	local x = getbits(lo, hi, 0, 21)
	local y = getbits(lo, hi, 21, 21)
	local z = getbits(lo, hi, 42, 21)
	local qx = (x - 1048576) * (1 / 1048576.5)
	local qy = (y - 1048576) * (1 / 1048576.5)
	local qz = (z - 1048576) * (1 / 1048576.5)
	local w2 = 1 - qx * qx - qy * qy - qz * qz
	local qw = w2 > 0 and math.sqrt(w2) or 0
	if getbits(lo, hi, 63, 1) ~= 0 then
		qw = -qw
	end
	return { qx, qy, qz, qw }
end

local function vec48(s, off0)
	if off0 < 0 or off0 + 6 > #s then
		return nil
	end
	return { half(u16(s, off0)), half(u16(s, off0 + 2)), half(u16(s, off0 + 4)) }
end

local function ang_q(rx, ry, rz)
	local sy, cy = math.sin(rz * 0.5), math.cos(rz * 0.5)
	local sp, cp = math.sin(ry * 0.5), math.cos(ry * 0.5)
	local sr, cr = math.sin(rx * 0.5), math.cos(rx * 0.5)
	return {
		sr * cp * cy - cr * sp * sy,
		cr * sp * cy + sr * cp * sy,
		cr * cp * sy - sr * sp * cy,
		cr * cp * cy + sr * sp * sy,
	}
end

local function qalign(base, q)
	local a, b = 0, 0
	for i = 1, 4 do
		local d = base[i] - q[i]
		local s = base[i] + q[i]
		a = a + d * d
		b = b + s * s
	end
	if a > b then
		return { -q[1], -q[2], -q[3], -q[4] }
	end
	return q
end

local function qmat(q, p)
	local x, y, z, w = q[1], q[2], q[3], q[4]
	return {
		1 - 2 * y * y - 2 * z * z, 2 * x * y - 2 * w * z, 2 * x * z + 2 * w * y, p[1],
		2 * x * y + 2 * w * z, 1 - 2 * x * x - 2 * z * z, 2 * y * z - 2 * w * x, p[2],
		2 * x * z - 2 * w * y, 2 * y * z + 2 * w * x, 1 - 2 * x * x - 2 * y * y, p[3],
	}
end

local function mul(a, b)
	local o = {}
	for r = 0, 2 do
		for c = 0, 3 do
			local s = a[r * 4 + 1] * b[c + 1] + a[r * 4 + 2] * b[4 + c + 1] + a[r * 4 + 3] * b[8 + c + 1]
			if c == 3 then
				s = s + a[r * 4 + 4]
			end
			o[r * 4 + c + 1] = s
		end
	end
	return o
end

local function read_bones(mdl)
	if not mdl or mdl:sub(1, 4) ~= "IDST" then
		return nil
	end
	local n = i32(mdl, 156)
	local bi = i32(mdl, 160)
	if not n or n < 1 or n > 512 or not bi or bi < 0 then
		return nil
	end
	local bones = {}
	for i = 0, n - 1 do
		local o = bi + i * BONE
		if o + BONE > #mdl then
			return nil
		end
		local pose = {}
		for k = 0, 11 do
			pose[k + 1] = f32(mdl, o + 96 + k * 4)
		end
		bones[i] = {
			name = cstr(mdl, o + i32(mdl, o)),
			parent = i32(mdl, o + 4),
			pos = { f32(mdl, o + 32), f32(mdl, o + 36), f32(mdl, o + 40) },
			quat = { f32(mdl, o + 44), f32(mdl, o + 48), f32(mdl, o + 52), f32(mdl, o + 56) },
			rot = { f32(mdl, o + 60), f32(mdl, o + 64), f32(mdl, o + 68) },
			posscale = { f32(mdl, o + 72), f32(mdl, o + 76), f32(mdl, o + 80) },
			rotscale = { f32(mdl, o + 84), f32(mdl, o + 88), f32(mdl, o + 92) },
			pose = pose,
			flags = i32(mdl, o + 160),
		}
	end
	return bones
end

function M.bind_residual(mdl_b)
	local bones = read_bones(mdl_b)
	if not bones then
		return nil
	end
	local world = {}
	local worst = 0
	for i = 0, #bones do
		local localm = qmat(bones[i].quat, bones[i].pos)
		local parent = bones[i].parent
		if parent >= 0 and world[parent] then
			world[i] = mul(world[parent], localm)
		else
			world[i] = localm
		end
		local skin = mul(world[i], bones[i].pose)
		local e = math.abs(skin[1] - 1) + math.abs(skin[2]) + math.abs(skin[3]) + math.abs(skin[4])
			+ math.abs(skin[5]) + math.abs(skin[6] - 1) + math.abs(skin[7]) + math.abs(skin[8])
			+ math.abs(skin[9]) + math.abs(skin[10]) + math.abs(skin[11] - 1) + math.abs(skin[12])
		if e > worst then
			worst = e
		end
	end
	return worst
end

-- studiohdr_t numlocalattachments / localattachmentindex, Source SDK 2013.
-- mstudioattachment_t is 92 bytes. The bind-pose bone matrix takes the local
-- attachment into model space. Index 1 is the first attachment.
local ATTACH = 92

function M.attachments(mdl)
	if not mdl or mdl:sub(1, 4) ~= "IDST" then
		return nil
	end
	local n = i32(mdl, 240)
	local ix = i32(mdl, 244)
	if not n or n < 1 or n > 128 or not ix or ix < 0 then
		return nil
	end
	local bones = read_bones(mdl)
	if not bones then
		return nil
	end
	local world = {}
	for i = 0, #bones do
		local localm = qmat(bones[i].quat, bones[i].pos)
		local parent = bones[i].parent
		if parent >= 0 and world[parent] then
			world[i] = mul(world[parent], localm)
		else
			world[i] = localm
		end
	end
	local out = {}
	for i = 0, n - 1 do
		local o = ix + i * ATTACH
		if o < 0 or o + ATTACH > #mdl then
			return nil
		end
		local bone = i32(mdl, o + 8)
		local localm = {}
		for k = 0, 11 do
			localm[k + 1] = f32(mdl, o + 12 + k * 4)
		end
		local space = localm
		if bone and bone >= 0 and world[bone] then
			space = mul(world[bone], localm)
		end
		local name_rel = i32(mdl, o)
		out[i + 1] = {
			name = cstr(mdl, o + (name_rel or 0)),
			x = space[4],
			y = space[8],
			z = space[12],
			fx = space[1],
			fy = space[5],
			fz = space[9],
		}
	end
	return out
end

local function includes_of(mdl)
	local n = i32(mdl, 336)
	local ix = i32(mdl, 340)
	local out = {}
	if not n or n < 1 or n > 32 or not ix or ix < 0 then
		return out
	end
	for i = 0, n - 1 do
		local e = ix + i * 8
		if e + 8 > #mdl then
			break
		end
		local rel = i32(mdl, e + 4)
		local name = cstr(mdl, e + rel):gsub("\\", "/"):lower()
		if name ~= "" then
			out[#out + 1] = name
		end
	end
	return out
end

local function anim_blocks(mdl)
	local n = i32(mdl, 352)
	local ix = i32(mdl, 356)
	local t = {}
	if not n or n < 1 or n > 4096 or not ix or ix < 0 then
		return t
	end
	for i = 0, n - 1 do
		local o = ix + i * 8
		if o + 8 > #mdl then
			break
		end
		t[i] = i32(mdl, o)
	end
	return t
end

local function sequences_of(mdl)
	local n = i32(mdl, 188)
	local si = i32(mdl, 192)
	local out = {}
	if not n or n < 1 or n > 20000 or not si or si < 0 then
		return out
	end
	for i = 0, n - 1 do
		local o = si + i * SEQ
		if o + SEQ > #mdl then
			break
		end
		local label_rel = i32(mdl, o + 4)
		out[#out + 1] = {
			name = cstr(mdl, o + label_rel),
			flags = i32(mdl, o + 12),
			blends = i32(mdl, o + 56),
			animindex = i32(mdl, o + 60),
			gs0 = i32(mdl, o + 68),
			gs1 = i32(mdl, o + 72),
			weight = i32(mdl, o + 156),
			base = o,
		}
	end
	return out
end

local function anim_value(data, off, frame, scale)
	if not data or not off or off < 0 or off + 2 > #data then
		return 0
	end
	local function head(o)
		return data:byte(o + 1) or 0, data:byte(o + 2) or 0
	end
	local valid, total = head(off)
	if total == 1 and valid == 1 then
		if off + 4 > #data then
			return 0
		end
		return i16(data, off + 2) * scale
	end
	local k = frame
	for _ = 1, 10000 do
		if total == 0 then
			return 0
		end
		if total > k then
			break
		end
		k = k - total
		off = off + (valid + 1) * 2
		if off + 2 > #data then
			return 0
		end
		valid, total = head(off)
	end
	if total <= k then
		return 0
	end
	local function sample(index)
		local at = off + index * 2
		if at < 0 or at + 2 > #data then
			return 0
		end
		return i16(data, at) * scale
	end
	if valid > k then
		return sample(k + 1)
	end
	return sample(valid)
end

local function axis_ptr(data, vp, axis)
	if vp < 0 or vp + 6 > #data then
		return nil
	end
	local rel = i16(data, vp + axis * 2)
	if rel <= 0 then
		return nil
	end
	return vp + rel
end

local function decode_frame(group, seq, frame)
	local mdl = group.mdl
	local bones = group.bones
	local ad0 = i32(mdl, 184)
	local anim_i = i16(mdl, seq.base + seq.animindex)
	if anim_i < 0 or not ad0 then
		return nil
	end
	local ad = ad0 + anim_i * ANIM
	if ad < 0 or ad + ANIM > #mdl then
		return nil
	end
	if bit.band(i32(mdl, ad + 12), STUDIO_DELTA) ~= 0 then
		return nil
	end
	local numframes = i32(mdl, ad + 16)
	if not numframes or numframes < 1 then
		return nil
	end
	if frame >= numframes then
		frame = numframes - 1
	end
	if frame < 0 then
		frame = 0
	end
	local sectionframes = i32(mdl, ad + 84)
	local block = i32(mdl, ad + 52)
	local index = i32(mdl, ad + 56)
	local local_frame = frame
	if sectionframes and sectionframes > 0 then
		local section
		if numframes > sectionframes and local_frame == numframes - 1 then
			local_frame = 0
			section = math.floor(numframes / sectionframes) + 1
		else
			section = math.floor(local_frame / sectionframes)
			local_frame = local_frame - section * sectionframes
		end
		local so = ad + i32(mdl, ad + 80) + section * 8
		if so < 0 or so + 8 > #mdl then
			return nil
		end
		block = i32(mdl, so)
		index = i32(mdl, so + 4)
	end
	if not block or block < 0 then
		return nil
	end
	local data, panim
	if block == 0 then
		data = mdl
		panim = ad + index
	else
		data = group.ani
		local start = group.blocks[block]
		if not data or not start then
			return nil
		end
		panim = start + index
	end
	local locals = {}
	for _ = 1, 512 do
		if not panim or panim < 0 or panim + 4 > #data then
			break
		end
		local bone = data:byte(panim + 1) or 0
		local flags = data:byte(panim + 2) or 0
		local nxt = i16(data, panim + 2)
		if bone <= #bones then
			local src = bones[bone]
			local pos = { src.pos[1], src.pos[2], src.pos[3] }
			local quat = { src.quat[1], src.quat[2], src.quat[3], src.quat[4] }
			local delta = bit.band(flags, ANIMDELTA) ~= 0
			if delta then
				pos[1], pos[2], pos[3] = 0, 0, 0
				quat[1], quat[2], quat[3], quat[4] = 0, 0, 0, 1
			end
			local data_at = panim + 4
			if bit.band(flags, RAWROT) ~= 0 then
				local q = quat48(data, data_at)
				if q then
					quat = q
				end
			elseif bit.band(flags, RAWROT2) ~= 0 then
				local q = quat64(data, data_at)
				if q then
					quat = q
				end
			elseif bit.band(flags, ANIMROT) ~= 0 then
				local rx = src.rot[1]
				local ry = src.rot[2]
				local rz = src.rot[3]
				if delta then
					rx, ry, rz = 0, 0, 0
				end
				rx = rx + anim_value(data, axis_ptr(data, data_at, 0), local_frame, src.rotscale[1])
				ry = ry + anim_value(data, axis_ptr(data, data_at, 1), local_frame, src.rotscale[2])
				rz = rz + anim_value(data, axis_ptr(data, data_at, 2), local_frame, src.rotscale[3])
				quat = ang_q(rx, ry, rz)
			end
			if not delta and bit.band(src.flags, BONE_FIXED_ALIGNMENT) ~= 0 then
				quat = qalign(src.quat, quat)
			end
			local pos_at = data_at
			if bit.band(flags, ANIMROT) ~= 0 then
				pos_at = data_at + 6
			end
			if bit.band(flags, RAWPOS) ~= 0 then
				local raw_at = data_at
				if bit.band(flags, RAWROT) ~= 0 then
					raw_at = data_at + 6
				elseif bit.band(flags, RAWROT2) ~= 0 then
					raw_at = data_at + 8
				end
				local p = vec48(data, raw_at)
				if p then
					pos = p
				end
			elseif bit.band(flags, ANIMPOS) ~= 0 then
				if delta then
					pos[1], pos[2], pos[3] = 0, 0, 0
				end
				for axis = 0, 2 do
					pos[axis + 1] = pos[axis + 1] + anim_value(data, axis_ptr(data, pos_at, axis), local_frame, src.posscale[axis + 1])
				end
			elseif delta then
				pos[1], pos[2], pos[3] = 0, 0, 0
			end
			local weight = 1
			if seq.weight and seq.weight > 0 then
				local at = seq.base + seq.weight + bone * 4
				if at >= 0 and at + 4 <= #mdl then
					weight = f32(mdl, at)
				end
			end
			if weight > 0 then
				locals[bone] = { pos = pos, quat = quat }
			end
		end
		if not nxt or nxt < 4 then
			break
		end
		panim = panim + nxt
	end
	return locals
end

local function pick_sequence(groups, prefer)
	if prefer and prefer ~= "" then
		local want = tostring(prefer):lower()
		for g = 1, #groups do
			local seqs = groups[g].seqs
			for i = 1, #seqs do
				if seqs[i].name:lower() == want and bit.band(seqs[i].flags, STUDIO_DELTA) == 0 then
					return groups[g], seqs[i]
				end
			end
		end
	end
	for p = 1, #PREFER do
		local want = PREFER[p]
		for g = 1, #groups do
			local seqs = groups[g].seqs
			for i = 1, #seqs do
				if seqs[i].name:lower() == want and bit.band(seqs[i].flags, STUDIO_DELTA) == 0 then
					return groups[g], seqs[i]
				end
			end
		end
	end
	for g = 1, #groups do
		local seqs = groups[g].seqs
		for i = 1, #seqs do
			local name = seqs[i].name:lower()
			if name:find("idle", 1, true) and not name:find("delta", 1, true) and bit.band(seqs[i].flags, STUDIO_DELTA) == 0 then
				return groups[g], seqs[i]
			end
		end
	end
	return nil
end

local function load_group(mdl, fetch, seen)
	if not mdl or seen[mdl] then
		return nil
	end
	seen[mdl] = true
	local bones = read_bones(mdl)
	if not bones then
		return nil
	end
	local ani
	local name_at = i32(mdl, 348)
	if fetch and name_at and name_at > 0 and name_at < #mdl then
		local nm = cstr(mdl, name_at):gsub("\\", "/"):lower()
		if nm ~= "" then
			ani = fetch(nm)
		end
	end
	return {
		mdl = mdl,
		bones = bones,
		seqs = sequences_of(mdl),
		blocks = anim_blocks(mdl),
		ani = ani,
	}
end

function M.matrices(mdl_b, fetch, prefer)
	local seen = {}
	local root = load_group(mdl_b, fetch, seen)
	if not root then
		return nil
	end
	local groups = { root }
	if fetch then
		local incs = includes_of(mdl_b)
		for i = 1, #incs do
			local bytes = fetch(incs[i])
			local g = load_group(bytes, fetch, seen)
			if g then
				groups[#groups + 1] = g
			end
		end
	end
	local group, seq = pick_sequence(groups, prefer)
	if not group or not seq then
		return nil
	end
	local numframes = 1
	local ad0 = i32(group.mdl, 184)
	local anim_i = i16(group.mdl, seq.base + seq.animindex)
	if ad0 and anim_i >= 0 then
		local ad = ad0 + anim_i * ANIM
		if ad >= 0 and ad + 20 <= #group.mdl then
			numframes = i32(group.mdl, ad + 16) or 1
		end
	end
	local frame = 0
	if numframes > 4 then
		frame = math.floor(numframes * 0.35)
		if frame >= numframes - 1 then
			frame = numframes - 2
		end
	end
	local locals = decode_frame(group, seq, frame)
	if not locals then
		return nil
	end
	local by_name = {}
	for i = 0, #root.bones do
		by_name[root.bones[i].name] = i
	end
	local posed = {}
	for i = 0, #group.bones do
		local sample = locals[i]
		if sample then
			local dst = by_name[group.bones[i].name]
			if dst then
				posed[dst] = sample
			end
		end
	end
	local world = {}
	local bind_world = {}
	local mats = {}
	local moved = 0
	for i = 0, #root.bones do
		local src = posed[i] or root.bones[i]
		local localm = qmat(src.quat or src.quat, src.pos)
		local bindm = qmat(root.bones[i].quat, root.bones[i].pos)
		local parent = root.bones[i].parent
		if parent >= 0 and world[parent] then
			world[i] = mul(world[parent], localm)
			bind_world[i] = mul(bind_world[parent], bindm)
		else
			world[i] = localm
			bind_world[i] = bindm
		end
		local dx = world[i][4] - bind_world[i][4]
		local dy = world[i][8] - bind_world[i][8]
		local dz = world[i][12] - bind_world[i][12]
		local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
		if dist > moved then
			moved = dist
		end
		mats[i] = mul(world[i], root.bones[i].pose)
	end
	if moved ~= moved or moved > 400 then
		return nil
	end
	return mats, seq.name, moved
end

return M
