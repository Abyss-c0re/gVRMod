-- Static prop lump (prps / sprp) from a VBSP game lump. Origins stay in map space.
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

function M.read(path)
	local f = io.open(path, "rb")
	if not f then
		return {}
	end
	local header = f:read(8 + 64 * 16)
	if not header or #header < 8 + 36 * 16 then
		f:close()
		return {}
	end
	local lump_at = 9 + 35 * 16
	local ofs = bin.i32(header, lump_at)
	local len = bin.i32(header, lump_at + 4)
	if len < 8 then
		f:close()
		return {}
	end
	f:seek("set", ofs)
	local gl = f:read(len)
	local count = i32(gl, 0)
	local fileofs, filelen, ver
	local p = 4
	for _ = 1, count do
		local id = gl:sub(p + 1, p + 4)
		local version = bin.u16(gl, p + 7)
		local fo = i32(gl, p + 8)
		local fl = i32(gl, p + 12)
		if id == "prps" or id == "sprp" then
			fileofs, filelen, ver = fo, fl, version
		end
		p = p + 16
	end
	if not fileofs then
		f:close()
		return {}
	end
	f:seek("set", fileofs)
	local spr = f:read(filelen)
	f:close()
	if not spr or (ver ~= 4 and ver ~= 5 and ver ~= 6) then
		return {}
	end
	local n = i32(spr, 0)
	if n <= 0 or n > 4000 or 4 + n * 128 > #spr then
		return {}
	end
	local names = {}
	for i = 0, n - 1 do
		names[i] = cstr(spr, 4 + i * 128)
	end
	local base = 4 + n * 128
	local leaf_count = i32(spr, base)
	if leaf_count < 0 or base + 4 + leaf_count * 2 > #spr then
		return {}
	end
	local base2 = base + 4 + leaf_count * 2
	local pc = i32(spr, base2)
	local rec = 56
	if ver == 5 then
		rec = 60
	elseif ver == 6 then
		rec = 64
	end
	if pc <= 0 or base2 + 4 + pc * rec > #spr + 4 then
		return {}
	end
	local out = {}
	for i = 0, pc - 1 do
		local o = base2 + 4 + i * rec
		local ptype = u16(spr, o + 24)
		out[#out + 1] = {
			model = names[ptype] or "",
			x = f32(spr, o),
			y = f32(spr, o + 4),
			z = f32(spr, o + 8),
			pitch = f32(spr, o + 12),
			yaw = f32(spr, o + 16),
			roll = f32(spr, o + 20),
		}
	end
	return out
end

return M
