-- Garry's Mod addon archive (GMAD). Headers are indexed; file bytes are read on demand.
-- Nothing in the archive is copied into git.
local M = {}

local cache = {}

local function u32(data, i)
	local a, b, c, d = data:byte(i, i + 3)
	if not d then
		return nil
	end
	return a + b * 256 + c * 65536 + d * 16777216
end

function M.open(path)
	if cache[path] then
		return cache[path]
	end
	local f = io.open(path, "rb")
	if not f then
		return nil
	end
	local data = f:read(1024 * 1024) or ""
	local function pull(n)
		while #data < n do
			local more = f:read(1024 * 1024)
			if not more or more == "" then
				return false
			end
			data = data .. more
			if #data > 32 * 1024 * 1024 then
				return false
			end
		end
		return true
	end
	if #data < 5 or data:sub(1, 4) ~= "GMAD" then
		f:close()
		return nil
	end
	local i = 6
	local version = data:byte(5) or 0
	local function need(n)
		return pull(i + n - 1)
	end
	local function str()
		local start = i
		while true do
			if not need(1) then
				return nil
			end
			if data:byte(i) == 0 then
				local s = data:sub(start, i - 1)
				i = i + 1
				if #s > 1024 * 1024 then
					return nil
				end
				return s
			end
			i = i + 1
			if i - start > 1024 * 1024 then
				return nil
			end
		end
	end
	local function num32()
		if not need(4) then
			return nil
		end
		local n = u32(data, i)
		i = i + 4
		return n
	end
	local function num64()
		local lo = num32()
		local hi = num32()
		if not lo or not hi then
			return nil
		end
		return lo + hi * 4294967296
	end
	-- steamid, timestamp
	if not num64() or not num64() then
		f:close()
		return nil
	end
	if version > 1 then
		while true do
			local s = str()
			if s == nil then
				f:close()
				return nil
			end
			if s == "" then
				break
			end
		end
	end
	-- Title, description, author, addon version. Not part of the file table.
	if not str() or not str() or not str() or not num32() then
		f:close()
		return nil
	end
	local files = {}
	local order = {}
	while true do
		local id = num32()
		if not id then
			f:close()
			return nil
		end
		if id == 0 then
			break
		end
		local name = str()
		local size = num64()
		local crc = num32()
		if not name or not size or not crc or size < 0 or size > 512 * 1024 * 1024 then
			f:close()
			return nil
		end
		local key = name:gsub("\\", "/"):lower()
		order[#order + 1] = { key = key, size = size }
	end
	local cursor = i - 1
	for n = 1, #order do
		local e = order[n]
		e.offset = cursor
		files[e.key] = e
		cursor = cursor + e.size
	end
	f:close()
	data = nil
	local pack = { path = path, files = files }
	function pack:read(key)
		if not key then
			return nil
		end
		local e = self.files[key:lower():gsub("\\", "/")]
		if not e then
			return nil
		end
		if e.size == 0 then
			return ""
		end
		local rf = io.open(self.path, "rb")
		if not rf then
			return nil
		end
		rf:seek("set", e.offset)
		local body = rf:read(e.size)
		rf:close()
		if not body or #body ~= e.size then
			return nil
		end
		return body
	end
	cache[path] = pack
	return pack
end

return M
