-- ZIP reader for a BSP pak lump. Seeks the central directory. Does not slurp the lump.
-- Method 0 is stored. Method 8 is raw deflate via zlib.
local ffi = require("ffi")
local bin = require("pure.bin")

local M = {}

ffi.cdef[[
typedef unsigned char Bytef;
typedef unsigned int uInt;
typedef unsigned long uLong;
typedef void *voidpf;
typedef voidpf (*alloc_func)(voidpf opaque, uInt items, uInt size);
typedef void (*free_func)(voidpf opaque, voidpf address);
typedef struct z_stream_s {
	const Bytef *next_in;
	uInt avail_in;
	uLong total_in;
	Bytef *next_out;
	uInt avail_out;
	uLong total_out;
	const char *msg;
	void *state;
	alloc_func zalloc;
	free_func zfree;
	voidpf opaque;
	int data_type;
	uLong adler;
	uLong reserved;
} z_stream;
int inflateInit2_(z_stream *strm, int windowBits, const char *version, int stream_size);
int inflate(z_stream *strm, int flush);
int inflateEnd(z_stream *strm);
int deflateInit2_(z_stream *strm, int level, int method, int windowBits, int memLevel, int strategy, const char *version, int stream_size);
int deflate(z_stream *strm, int flush);
int deflateEnd(z_stream *strm);
const char *zlibVersion(void);
]]

local zlib = ffi.load("z")
local Z_OK = 0
local Z_STREAM_END = 1
local Z_FINISH = 4

local function zversion()
	return zlib.zlibVersion()
end

function M.inflate_raw(src, dst_len)
	local inn = ffi.new("uint8_t[?]", #src)
	ffi.copy(inn, src, #src)
	local out = ffi.new("uint8_t[?]", dst_len)
	local strm = ffi.new("z_stream")
	strm.next_in = inn
	strm.avail_in = #src
	strm.next_out = out
	strm.avail_out = dst_len
	local rc = zlib.inflateInit2_(strm, -15, zversion(), ffi.sizeof("z_stream"))
	if rc ~= Z_OK then
		error("inflateInit2 " .. tostring(rc))
	end
	rc = zlib.inflate(strm, Z_FINISH)
	zlib.inflateEnd(strm)
	if rc ~= Z_STREAM_END and rc ~= Z_OK then
		error("inflate " .. tostring(rc))
	end
	local n = tonumber(strm.total_out)
	return ffi.string(out, n)
end

function M.deflate_raw(src)
	local inn = ffi.new("uint8_t[?]", #src)
	ffi.copy(inn, src, #src)
	local cap = #src + 64 + math.floor(#src / 8)
	local out = ffi.new("uint8_t[?]", cap)
	local strm = ffi.new("z_stream")
	strm.next_in = inn
	strm.avail_in = #src
	strm.next_out = out
	strm.avail_out = cap
	-- method 8 = deflate, window -15 = raw, memlevel 8, strategy 0
	local rc = zlib.deflateInit2_(strm, 6, 8, -15, 8, 0, zversion(), ffi.sizeof("z_stream"))
	if rc ~= Z_OK then
		error("deflateInit2 " .. tostring(rc))
	end
	rc = zlib.deflate(strm, Z_FINISH)
	local n = tonumber(strm.total_out)
	zlib.deflateEnd(strm)
	if rc ~= Z_STREAM_END then
		error("deflate " .. tostring(rc))
	end
	return ffi.string(out, n)
end

local function find_eocd(buf)
	for i = #buf - 21, 1, -1 do
		if buf:byte(i) == 0x50 and buf:byte(i + 1) == 0x4b
			and buf:byte(i + 2) == 0x05 and buf:byte(i + 3) == 0x06 then
			return i
		end
	end
	return nil
end

function M.open(path, base, length)
	base = base or 0
	local f = assert(io.open(path, "rb"))
	if not length then
		local cur = f:seek("end")
		length = cur - base
	end
	local tail = math.min(length, 22 + 65535)
	f:seek("set", base + length - tail)
	local buf = f:read(tail)
	local pos = buf and find_eocd(buf) or nil
	if not pos then
		f:close()
		error("zip: no end of central directory in " .. path)
	end
	local total = bin.u16(buf, pos + 10)
	local cd_size = bin.u32(buf, pos + 12)
	local cd_off = bin.u32(buf, pos + 16)
	if cd_size > 32 * 1024 * 1024 then
		f:close()
		error("zip: central directory is too large")
	end
	f:seek("set", base + cd_off)
	local cd = f:read(cd_size)
	f:close()
	if not cd or #cd ~= cd_size then
		error("zip: short central directory")
	end
	local files = {}
	local p = 1
	local seen = 0
	while p + 45 <= #cd and seen < total do
		if cd:byte(p) ~= 0x50 or cd:byte(p + 1) ~= 0x4b then
			break
		end
		local method = bin.u16(cd, p + 10)
		local comp = bin.u32(cd, p + 20)
		local uncomp = bin.u32(cd, p + 24)
		local nlen = bin.u16(cd, p + 28)
		local elen = bin.u16(cd, p + 30)
		local clen = bin.u16(cd, p + 32)
		local local_off = bin.u32(cd, p + 42)
		local name = cd:sub(p + 46, p + 45 + nlen)
		p = p + 46 + nlen + elen + clen
		seen = seen + 1
		files[name:lower():gsub("\\", "/")] = {
			method = method,
			comp = comp,
			uncomp = uncomp,
			local_off = local_off,
		}
	end
	local z = { path = path, base = base, files = files }

	function z:read(name)
		local e = self.files[name:lower():gsub("\\", "/")]
		if not e then
			return nil
		end
		local rf = assert(io.open(self.path, "rb"))
		rf:seek("set", self.base + e.local_off)
		local lh = rf:read(30)
		if not lh or #lh < 30 then
			rf:close()
			error("zip: short local header " .. name)
		end
		local nlen = bin.u16(lh, 27)
		local elen = bin.u16(lh, 29)
		rf:seek("set", self.base + e.local_off + 30 + nlen + elen)
		local body = rf:read(e.comp)
		rf:close()
		if not body or #body ~= e.comp then
			error("zip: short data " .. name)
		end
		if e.method == 0 then
			return body
		end
		if e.method == 8 then
			return M.inflate_raw(body, e.uncomp)
		end
		error("zip: unsupported method " .. tostring(e.method) .. " for " .. name)
	end

	return z
end

return M
