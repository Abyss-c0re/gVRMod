-- Reference-pose studio mesh. MDL + VVD + DX90 VTX, bind pose only.
-- Bones, flexes, and LODs above 0 are not applied. Positions come from the VVD.
local bin = require("pure.bin")

local M = {}

local function i32(s, off0)
	return bin.i32(s, off0 + 1)
end

local function u16(s, off0)
	return bin.u16(s, off0 + 1)
end

local function f32(s, off0)
	return bin.f32(s, off0 + 1)
end

local function cstr(s, off0)
	local i = off0 + 1
	if i < 1 or i > #s then
		return ""
	end
	local z = s:find("\0", i, true)
	if not z then
		return s:sub(i)
	end
	return s:sub(i, z - 1)
end

local function vvd_vert(vvd, vstart, index)
	local o = vstart + index * 48
	return f32(vvd, o + 16), f32(vvd, o + 20), f32(vvd, o + 24), f32(vvd, o + 40), f32(vvd, o + 44)
end

-- vertexFileFixup_t is 12 bytes: lod, sourceVertexID, numVertexes.
-- LOD 0 keeps every fixup (lod >= 0). The copies are concatenated in order.
local function vertex_blob(vvd_b)
	local num_fixups = i32(vvd_b, 48)
	local vstart = i32(vvd_b, 56)
	if num_fixups == 0 then
		return vvd_b, vstart
	end
	if num_fixups < 0 or num_fixups > 100000 then
		return nil, "fixups"
	end
	local fixup_at = i32(vvd_b, 52)
	local parts = {}
	local total = 0
	for i = 0, num_fixups - 1 do
		local o = fixup_at + i * 12
		local lod = i32(vvd_b, o)
		local src = i32(vvd_b, o + 4)
		local n = i32(vvd_b, o + 8)
		if lod >= 0 and n > 0 then
			local from = vstart + src * 48
			if from < 0 or from + n * 48 > #vvd_b then
				return nil, "fixup range"
			end
			parts[#parts + 1] = vvd_b:sub(from + 1, from + n * 48)
			total = total + n
		end
	end
	if total ~= i32(vvd_b, 16) then
		return nil, "fixup count"
	end
	return table.concat(parts), 0
end

function M.load(mdl_b, vvd_b, vtx_b)
	if not mdl_b or mdl_b:sub(1, 4) ~= "IDST" then
		return nil, "mdl"
	end
	if not vvd_b or vvd_b:sub(1, 4) ~= "IDSV" then
		return nil, "vvd"
	end
	if not vtx_b or i32(vtx_b, 0) ~= 7 then
		return nil, "vtx"
	end
	-- numFixups at byte 48. VTX indexes the post-fixup array. numFixups == 0
	-- means the bytes at vertexDataStart are already in that order.
	local verts, vstart = vertex_blob(vvd_b)
	if not verts then
		return nil, vstart
	end
	local lod_verts = i32(vvd_b, 16)

	local num_cd = i32(mdl_b, 212)
	local cd_index = i32(mdl_b, 216)
	local cd = ""
	if num_cd > 0 then
		local abs = i32(mdl_b, cd_index)
		local name = cstr(mdl_b, abs)
		if name == "" then
			name = cstr(mdl_b, cd_index + abs)
		end
		cd = name:gsub("\\", "/"):lower()
		if cd ~= "" and cd:sub(-1) ~= "/" then
			cd = cd .. "/"
		end
	end
	local num_tex = i32(mdl_b, 204)
	local tex_index = i32(mdl_b, 208)
	local textures = {}
	for i = 0, num_tex - 1 do
		local base = tex_index + i * 64
		local rel = i32(mdl_b, base)
		local name = cstr(mdl_b, base + rel):gsub("\\", "/"):lower()
		textures[i + 1] = cd .. name
	end

	local nbody = i32(mdl_b, 232)
	local body_index = i32(mdl_b, 236)
	local vtx_body = i32(vtx_b, 32)
	local meshes = {}
	local tris = 0

	for b = 0, nbody - 1 do
		local bp = body_index + b * 16
		local nmodels = i32(mdl_b, bp + 4)
		local model0 = bp + i32(mdl_b, bp + 12)
		local vbp = vtx_body + b * 8
		local vmodel0 = vbp + i32(vtx_b, vbp + 4)
		for m = 0, nmodels - 1 do
			local model = model0
			-- mstudiomodel_t is not a fixed stride we rely on; bodyparts in these maps have one model.
			if m > 0 then
				break
			end
			local nmeshes = i32(mdl_b, model + 72)
			local mesh0 = model + i32(mdl_b, model + 76)
			local stride = 116
			if nmeshes >= 2 then
				stride = nil
				for s = 48, 220, 4 do
					local rel = i32(mdl_b, mesh0 + s + 4)
					if mesh0 + s + rel == model then
						stride = s
						break
					end
				end
				if not stride then
					return nil, "mesh stride"
				end
			end
			local vm = vmodel0 + m * 8
			local vlod = vm + i32(vtx_b, vm + 4)
			local nvtx_mesh = i32(vtx_b, vlod)
			local mesh_off = i32(vtx_b, vlod + 4)
			local mesh_stride = 9
			if nvtx_mesh >= 2 then
				local function sane(step)
					for i = 0, nvtx_mesh - 1 do
						local at = vlod + mesh_off + i * step
						local ns = i32(vtx_b, at)
						local off = i32(vtx_b, at + 4)
						if ns < 1 or ns > 8 or off < 9 or at + off > #vtx_b then
							return false
						end
					end
					return true
				end
				if not sane(9) and sane(12) then
					mesh_stride = 12
				end
			end
			local count = nmeshes
			if nvtx_mesh < count then
				count = nvtx_mesh
			end
			for mi = 0, count - 1 do
				local mesh = mesh0 + mi * stride
				local mat = i32(mdl_b, mesh)
				local numv = i32(mdl_b, mesh + 8)
				local voff = i32(mdl_b, mesh + 12)
				local vmh = vlod + mesh_off + mi * mesh_stride
				local nsg = i32(vtx_b, vmh)
				local sg_at = vmh + i32(vtx_b, vmh + 4)
				local buf = {}
				local drew = false
				for s = 0, nsg - 1 do
					local sg = sg_at + s * 25
					local flags = vtx_b:byte(sg + 25) or 0
					if bit.band(flags, 0x01) ~= 0 then
						goto continue_sg
					end
					local nv = i32(vtx_b, sg)
					local vert_off = i32(vtx_b, sg + 4)
					local ni = i32(vtx_b, sg + 8)
					local index_off = i32(vtx_b, sg + 12)
					if nv <= 0 or ni < 3 or sg + vert_off + nv * 9 > #vtx_b + 1 then
						goto continue_sg
					end
					local orig = {}
					for v = 0, nv - 1 do
						orig[v] = u16(vtx_b, sg + vert_off + v * 9 + 4)
					end
					for t = 0, ni - 3, 3 do
						local ia = u16(vtx_b, sg + index_off + t * 2)
						local ib = u16(vtx_b, sg + index_off + (t + 1) * 2)
						local ic = u16(vtx_b, sg + index_off + (t + 2) * 2)
						local a, b3, c = orig[ia], orig[ib], orig[ic]
						if a and b3 and c and a < numv and b3 < numv and c < numv
							and voff + a < lod_verts and voff + b3 < lod_verts and voff + c < lod_verts then
							for _, id in ipairs({ a, b3, c }) do
								local x, y, z, u, v = vvd_vert(verts, vstart, voff + id)
								buf[#buf + 1] = x
								buf[#buf + 1] = y
								buf[#buf + 1] = z
								buf[#buf + 1] = u
								buf[#buf + 1] = v
							end
							tris = tris + 1
							drew = true
						end
					end
					::continue_sg::
				end
				if drew then
					local name = textures[mat + 1] or "unknown"
					meshes[#meshes + 1] = { material = name, verts = buf }
				end
			end
		end
	end

	return {
		meshes = meshes,
		tris = tris,
		hull_min = { x = f32(mdl_b, 104), y = f32(mdl_b, 108), z = f32(mdl_b, 112) },
		hull_max = { x = f32(mdl_b, 116), y = f32(mdl_b, 120), z = f32(mdl_b, 124) },
	}
end

return M
