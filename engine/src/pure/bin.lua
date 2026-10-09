-- Little-endian readers. LuaJIT 2.1 has no string.unpack; floats go through ffi.
local ffi = require("ffi")
local M = {}

local f1 = ffi.new("float[1]")

function M.f32(s, i)
	ffi.copy(f1, s:sub(i, i + 3), 4)
	return tonumber(f1[0])
end

function M.u32(s, i)
	local a, b, c, d = s:byte(i, i + 3)
	return a + b * 256 + c * 65536 + d * 16777216
end

function M.i32(s, i)
	local n = M.u32(s, i)
	if n >= 2147483648 then
		n = n - 4294967296
	end
	return n
end

function M.u16(s, i)
	local a, b = s:byte(i, i + 1)
	return a + b * 256
end

function M.i16(s, i)
	local n = M.u16(s, i)
	if n >= 32768 then
		n = n - 65536
	end
	return n
end

return M
