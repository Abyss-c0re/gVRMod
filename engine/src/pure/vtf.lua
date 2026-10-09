-- VTF albedo decode to tightly packed RGBA8, row 0 at the top.
-- High-res formats used here: DXT1 (13, 20), DXT3 (14), DXT5 (15), RGBA/BGRA/BGR/RGB.
-- Mips are smallest-first. The returned image is the largest mip whose edge is <= max_edge.
local ffi = require("ffi")
local bin = require("pure.bin")

local M = {}

local function image_bytes(w, h, fmt)
	if fmt == 13 or fmt == 20 or fmt == 14 or fmt == 15 then
		local bw = math.max(1, math.floor((w + 3) / 4))
		local bh = math.max(1, math.floor((h + 3) / 4))
		local block = (fmt == 13 or fmt == 20) and 8 or 16
		return bw * bh * block
	end
	if fmt == 0 or fmt == 1 or fmt == 11 or fmt == 12 or fmt == 16 then
		return w * h * 4
	end
	if fmt == 2 or fmt == 3 or fmt == 9 or fmt == 10 then
		return w * h * 3
	end
	return nil
end

local function rgb565(c)
	local r = math.floor(c / 2048)
	local g = math.floor(c / 32) % 64
	local b = c % 32
	r = r * 8 + math.floor(r / 4)
	g = g * 4 + math.floor(g / 16)
	b = b * 8 + math.floor(b / 4)
	return r, g, b
end

local function mix(a, b, n, d)
	return math.floor((a * (d - n) + b * n) / d)
end

local function dxt1_colors(c0, c1, four)
	local r0, g0, b0 = rgb565(c0)
	local r1, g1, b1 = rgb565(c1)
	local r, g, b, a = {}, {}, {}, {}
	r[0], g[0], b[0], a[0] = r0, g0, b0, 255
	r[1], g[1], b[1], a[1] = r1, g1, b1, 255
	if four then
		r[2], g[2], b[2], a[2] = mix(r0, r1, 1, 3), mix(g0, g1, 1, 3), mix(b0, b1, 1, 3), 255
		r[3], g[3], b[3], a[3] = mix(r0, r1, 2, 3), mix(g0, g1, 2, 3), mix(b0, b1, 2, 3), 255
	else
		r[2], g[2], b[2], a[2] = mix(r0, r1, 1, 2), mix(g0, g1, 1, 2), mix(b0, b1, 1, 2), 255
		r[3], g[3], b[3], a[3] = 0, 0, 0, 0
	end
	return r, g, b, a
end

local function put(out, w, h, x, y, r, g, b, a)
	if x >= w or y >= h then
		return
	end
	local i = (y * w + x) * 4
	out[i] = r
	out[i + 1] = g
	out[i + 2] = b
	out[i + 3] = a
end

local function paint_dxt1(src, bi, out, w, h, ox, oy, force_four)
	local c0 = src[bi] + src[bi + 1] * 256
	local c1 = src[bi + 2] + src[bi + 3] * 256
	local bits = src[bi + 4] + src[bi + 5] * 256 + src[bi + 6] * 65536 + src[bi + 7] * 16777216
	local four = force_four or c0 > c1
	local r, g, b, a = dxt1_colors(c0, c1, four)
	local pixel = 0
	for y = 0, 3 do
		for x = 0, 3 do
			local idx = math.floor(bits / (2 ^ (pixel * 2))) % 4
			put(out, w, h, ox + x, oy + y, r[idx], g[idx], b[idx], a[idx])
			pixel = pixel + 1
		end
	end
end

local function alpha_bits3(src, base, pixel)
	local bitpos = pixel * 3
	local byte = math.floor(bitpos / 8)
	local shift = bitpos - byte * 8
	local v = src[base + byte]
	if shift > 5 then
		v = v + src[base + byte + 1] * 256
	end
	return math.floor(v / (2 ^ shift)) % 8
end

local function decode_chunk(chunk, fmt, w, h)
	local need = image_bytes(w, h, fmt)
	if not need or #chunk < need then
		return nil, "short image"
	end
	local src = ffi.new("uint8_t[?]", #chunk)
	ffi.copy(src, chunk, #chunk)
	local out = ffi.new("uint8_t[?]", w * h * 4)
	if fmt == 13 or fmt == 20 or fmt == 14 or fmt == 15 then
		local bw = math.max(1, math.floor((w + 3) / 4))
		local bh = math.max(1, math.floor((h + 3) / 4))
		local block = (fmt == 13 or fmt == 20) and 8 or 16
		local bi = 0
		for by = 0, bh - 1 do
			for bx = 0, bw - 1 do
				local ox, oy = bx * 4, by * 4
				if fmt == 15 then
					local a0 = src[bi]
					local a1 = src[bi + 1]
					local av = {}
					av[0], av[1] = a0, a1
					if a0 > a1 then
						av[2] = mix(a0, a1, 1, 7)
						av[3] = mix(a0, a1, 2, 7)
						av[4] = mix(a0, a1, 3, 7)
						av[5] = mix(a0, a1, 4, 7)
						av[6] = mix(a0, a1, 5, 7)
						av[7] = mix(a0, a1, 6, 7)
					else
						av[2] = mix(a0, a1, 1, 5)
						av[3] = mix(a0, a1, 2, 5)
						av[4] = mix(a0, a1, 3, 5)
						av[5] = mix(a0, a1, 4, 5)
						av[6] = 0
						av[7] = 255
					end
					paint_dxt1(src, bi + 8, out, w, h, ox, oy, true)
					local pixel = 0
					for y = 0, 3 do
						for x = 0, 3 do
							local ai = alpha_bits3(src, bi + 2, pixel)
							if ox + x < w and oy + y < h then
								local i = ((oy + y) * w + (ox + x)) * 4
								out[i + 3] = av[ai]
							end
							pixel = pixel + 1
						end
					end
				elseif fmt == 14 then
					paint_dxt1(src, bi + 8, out, w, h, ox, oy, false)
					local pixel = 0
					for y = 0, 3 do
						for x = 0, 3 do
							local byte = src[bi + math.floor(pixel / 2)]
							local nib = (pixel % 2 == 0) and (byte % 16) or math.floor(byte / 16)
							local al = nib * 17
							if ox + x < w and oy + y < h then
								local i = ((oy + y) * w + (ox + x)) * 4
								out[i + 3] = al
							end
							pixel = pixel + 1
						end
					end
				else
					paint_dxt1(src, bi, out, w, h, ox, oy, false)
				end
				bi = bi + block
			end
		end
	elseif fmt == 0 or fmt == 12 or fmt == 11 or fmt == 16 or fmt == 1 then
		local i = 0
		for p = 0, w * h - 1 do
			local b0, b1, b2, b3 = src[i], src[i + 1], src[i + 2], src[i + 3]
			local r, g, b, a
			if fmt == 12 or fmt == 16 then
				r, g, b, a = b2, b1, b0, b3
			elseif fmt == 11 then
				a, r, g, b = b0, b1, b2, b3
			elseif fmt == 1 then
				a, b, g, r = b0, b1, b2, b3
			else
				r, g, b, a = b0, b1, b2, b3
			end
			if fmt == 16 then
				a = 255
			end
			out[p * 4] = r
			out[p * 4 + 1] = g
			out[p * 4 + 2] = b
			out[p * 4 + 3] = a
			i = i + 4
		end
	elseif fmt == 3 or fmt == 2 or fmt == 10 or fmt == 9 then
		local i = 0
		for p = 0, w * h - 1 do
			local b0, b1, b2 = src[i], src[i + 1], src[i + 2]
			if fmt == 3 or fmt == 10 then
				out[p * 4] = b2
				out[p * 4 + 1] = b1
				out[p * 4 + 2] = b0
			else
				out[p * 4] = b0
				out[p * 4 + 1] = b1
				out[p * 4 + 2] = b2
			end
			out[p * 4 + 3] = 255
			i = i + 3
		end
	else
		return nil, "format " .. tostring(fmt)
	end
	return ffi.string(out, w * h * 4)
end

local function mip_chain(w, h, mips)
	local chain = {}
	local mw, mh = w, h
	for i = 1, mips do
		chain[i] = { w = mw, h = mh }
		mw = math.max(1, math.floor(mw / 2))
		mh = math.max(1, math.floor(mh / 2))
	end
	return chain
end

-- data is the whole VTF. max_edge defaults to the full texture.
function M.decode(data, max_edge)
	if not data or #data < 64 or data:sub(1, 3) ~= "VTF" then
		return nil, "not a vtf"
	end
	local major = bin.u32(data, 5)
	local minor = bin.u32(data, 9)
	local header_size = bin.u32(data, 13)
	local w = bin.u16(data, 17)
	local h = bin.u16(data, 19)
	local flags = bin.u32(data, 21)
	local frames = bin.u16(data, 25)
	local fmt = bin.u32(data, 53)
	local mips = data:byte(57) or 1
	if frames < 1 then
		frames = 1
	end
	if mips < 1 then
		mips = 1
	end
	if bit.band(flags, 0x4000) ~= 0 then
		return nil, "cubemap"
	end
	if w < 1 or h < 1 then
		return nil, "size"
	end
	local start0 = header_size
	if major > 7 or (major == 7 and minor >= 3) then
		-- 7.3 stores a resource dictionary as the last nres*8 bytes of headerSize.
		-- Citizen sheets pad 8 bytes after numResources, so the dictionary is not at byte 72.
		-- Tag 0x30 is the high-res image. Flag 0x02 means the offset field is not a file offset.
		if #data < 72 then
			return nil, "short v7.3 header"
		end
		local nres = bin.u32(data, 69)
		if nres < 1 or nres > 32 or header_size < 72 or header_size > #data then
			return nil, "bad resource header"
		end
		local dict = header_size - nres * 8
		if dict < 72 then
			return nil, "bad resource header"
		end
		local found = nil
		local p = dict + 1
		for _ = 1, nres do
			if p + 7 > #data then
				break
			end
			local tag = data:byte(p)
			local flags = data:byte(p + 3) or 0
			local ofs = bin.u32(data, p + 4)
			if tag == 0x30 and math.floor(flags / 2) % 2 == 0 and ofs and ofs > 0 and ofs < #data then
				found = ofs
			end
			p = p + 8
		end
		if not found then
			return nil, "no image resource"
		end
		start0 = found
	else
		local low_fmt = bin.u32(data, 58)
		local low_w = data:byte(62) or 0
		local low_h = data:byte(63) or 0
		if low_w > 0 and low_h > 0 then
			local low_n = image_bytes(low_w, low_h, low_fmt)
			if not low_n then
				return nil, "lowres format " .. tostring(low_fmt)
			end
			start0 = start0 + low_n
		end
	end
	max_edge = max_edge or math.max(w, h)
	local chain = mip_chain(w, h, mips)
	local target = mips
	for i = 1, mips do
		if chain[i].w <= max_edge and chain[i].h <= max_edge then
			target = i
			break
		end
	end
	local off = start0
	for i = mips, target + 1, -1 do
		local n = image_bytes(chain[i].w, chain[i].h, fmt)
		if not n then
			return nil, "format " .. tostring(fmt)
		end
		off = off + n * frames
	end
	local tw, th = chain[target].w, chain[target].h
	local n = image_bytes(tw, th, fmt)
	if not n then
		return nil, "format " .. tostring(fmt)
	end
	if off + n > #data then
		return nil, "mip past end"
	end
	local rgba, err = decode_chunk(data:sub(off + 1, off + n), fmt, tw, th)
	if not rgba then
		return nil, err
	end
	return tw, th, rgba
end

return M
