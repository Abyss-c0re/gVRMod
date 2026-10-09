-- VPK v2 directory reader. Bytes stay in memory for the caller. Nothing is copied into git.
-- Tree entry is 18 bytes: crc u32, preload u16, archive u16, offset u32, length u32, term u16.
-- Archive index 0x7fff means the payload sits in this dir file, after the header and the tree.
local bin = require("pure.bin")

local M = {}

local EMBEDDED = 0x7fff

local function cstr(data, i)
	local z = data:find("\0", i, true)
	if not z then
		error("vpk: truncated name at " .. tostring(i))
	end
	return data:sub(i, z - 1), z + 1
end

function M.open(path)
	local f = assert(io.open(path, "rb"))
	local lead = f:read(12)
	if not lead or #lead < 12 then
		f:close()
		error("vpk: short header " .. path)
	end
	local sig = bin.u32(lead, 1)
	local ver = bin.u32(lead, 5)
	local tree_size = bin.u32(lead, 9)
	if sig ~= 0x55aa1234 then
		f:close()
		error(string.format("vpk: bad signature %08x %s", sig, path))
	end
	local header_len = 12
	if ver >= 2 then
		local rest = f:read(16)
		if not rest or #rest < 16 then
			f:close()
			error("vpk: short v2 header " .. path)
		end
		header_len = 28
	end
	local tree = f:read(tree_size)
	f:close()
	if not tree or #tree ~= tree_size then
		error("vpk: short tree " .. path)
	end

	local files = {}
	local i = 1
	while true do
		local ext
		ext, i = cstr(tree, i)
		if ext == "" then
			break
		end
		while true do
			local dir
			dir, i = cstr(tree, i)
			if dir == "" then
				break
			end
			while true do
				local name
				name, i = cstr(tree, i)
				if name == "" then
					break
				end
				if i + 17 > #tree then
					error("vpk: truncated entry in " .. path)
				end
				local preload = bin.u16(tree, i + 4)
				local arch = bin.u16(tree, i + 6)
				local offset = bin.u32(tree, i + 8)
				local length = bin.u32(tree, i + 12)
				i = i + 18
				local pre = nil
				if preload > 0 then
					if i + preload - 1 > #tree then
						error("vpk: truncated preload in " .. path)
					end
					pre = tree:sub(i, i + preload - 1)
					i = i + preload
				end
				local full
				if dir == "" or dir == " " then
					full = name .. "." .. ext
				else
					full = dir .. "/" .. name .. "." .. ext
				end
				files[full:lower()] = {
					arch = arch,
					offset = offset,
					length = length,
					preload = pre,
				}
			end
		end
	end

	local pack = {
		path = path,
		version = ver,
		data_start = header_len + tree_size,
		files = files,
	}

	function pack:read(key)
		local e = self.files[key:lower():gsub("\\", "/")]
		if not e then
			return nil
		end
		local pre = e.preload or ""
		if e.length == 0 then
			return pre
		end
		local rf, at, label
		if e.arch == EMBEDDED then
			rf = io.open(self.path, "rb")
			at = self.data_start + e.offset
			label = self.path
		else
			label = self.path:gsub("_dir%.vpk$", string.format("_%03d.vpk", e.arch))
			rf = io.open(label, "rb")
			at = e.offset
		end
		if not rf then
			return nil
		end
		rf:seek("set", at)
		local body = rf:read(e.length)
		rf:close()
		if not body or #body ~= e.length then
			error("vpk: short read " .. label .. " @" .. tostring(at))
		end
		if pre ~= "" then
			return pre .. body
		end
		return body
	end

	return pack
end

return M
