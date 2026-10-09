-- Resolve a Source material name to an albedo image.
-- Search order: the map pak, then garrysmod VPKs, then hl2. Files are read from the
-- local game install. This module does not write those bytes anywhere.
local vpk = require("pure.vpk")
local vmt = require("pure.vmt")
local vtf = require("pure.vtf")
local zipread = require("pure.zipread")

local M = {}

local MISSING = string.rep(string.char(255, 0, 255, 255), 16)

local function add_vpk(list, path)
	local f = io.open(path, "rb")
	if not f then
		return
	end
	f:close()
	list[#list + 1] = vpk.open(path)
end

local function vmt_path(name)
	name = name:lower():gsub("\\", "/"):gsub("^materials/", "")
	name = name:gsub("%.vmt$", ""):gsub("%.vtf$", "")
	return "materials/" .. name .. ".vmt", "materials/" .. name .. ".vtf", name
end

local function tool_skip(name)
	local n = name:lower()
	if not n:find("tools/", 1, true) then
		return false
	end
	if n:find("nodraw", 1, true) or n:find("sky", 1, true) or n:find("clip", 1, true)
		or n:find("trigger", 1, true) or n:find("hint", 1, true) or n:find("origin", 1, true)
		or n:find("block", 1, true) or n:find("invisible", 1, true) or n:find("occluder", 1, true)
		or n:find("fog", 1, true) then
		return true
	end
	return false
end

function M.mount(opts)
	opts = opts or {}
	local gmod = opts.gmod
		or os.getenv("ENGINE_GMOD")
		or "/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod"
	local packs = {}
	add_vpk(packs, gmod .. "/garrysmod_dir.vpk")
	add_vpk(packs, gmod .. "/fallbacks_dir.vpk")
	add_vpk(packs, gmod .. "/../sourceengine/hl2_misc_dir.vpk")
	add_vpk(packs, gmod .. "/../sourceengine/hl2_textures_dir.vpk")
	local pak = nil
	if opts.bsp and opts.pak_len and opts.pak_len > 22 then
		pak = zipread.open(opts.bsp, opts.pak_ofs or 0, opts.pak_len)
	end

	local function blob(key)
		if not key then
			return nil
		end
		key = key:lower():gsub("\\", "/")
		if pak then
			local b = pak:read(key)
			if b then
				return b
			end
		end
		for i = 1, #packs do
			local b = packs[i]:read(key)
			if b then
				return b
			end
		end
		return nil
	end

	local function normalize_include(path)
		path = path:lower():gsub("\\", "/")
		if path:sub(-4) ~= ".vmt" then
			path = path .. ".vmt"
		end
		if path:sub(1, 10) ~= "materials/" then
			path = "materials/" .. path
		end
		return path
	end

	local function resolve_keys(path, depth, seen)
		local text = blob(path)
		if not text then
			return nil
		end
		local map = vmt.pairs(text)
		local shader = vmt.header(text)
		if shader ~= "" and shader ~= "patch" then
			map.__shader = shader
		end
		if not map.include or depth >= 4 then
			return map
		end
		local inc = normalize_include(map.include)
		if seen[inc] then
			return map
		end
		seen[inc] = true
		local base = resolve_keys(inc, depth + 1, seen) or {}
		local shader_keep = base.__shader
		for k, val in pairs(map) do
			if k ~= "include" and k ~= "__shader" then
				base[k] = val
			end
		end
		if map.__shader then
			base.__shader = map.__shader
		elseif shader_keep then
			base.__shader = shader_keep
		end
		return base
	end

	local mount = { packs = packs, pak = pak }
	local decoded = {}

	local TRANSPARENT = {
		lightmappedreflective = true,
		refract = true,
	}

	function mount:read(key)
		return blob(key)
	end

	local function keys_with_albedo(path, depth, seen)
		local keys = resolve_keys(path, 0, seen)
		if not keys then
			return nil
		end
		if keys["$basetexture"] and keys["$basetexture"] ~= "" then
			return keys
		end
		local fb = keys["$fallbackmaterial"]
		if not fb or fb == "" or depth >= 4 then
			return keys
		end
		local inc = normalize_include(fb)
		if seen[inc] then
			return keys
		end
		seen[inc] = true
		local child = keys_with_albedo(inc, depth + 1, seen)
		if child and child["$basetexture"] and child["$basetexture"] ~= "" then
			return child
		end
		return keys
	end

	local function tint_key(keys)
		local fog = keys["$fogcolor"]
		if not fog then
			return nil
		end
		local r, g, b = fog:match("(%d+)%s+(%d+)%s+(%d+)")
		if not r then
			return nil
		end
		return string.format("__tint/%d/%d/%d", tonumber(r), tonumber(g), tonumber(b))
	end

	-- Returns basetexture vtf key and uv transform, without decoding.
	-- A refractive shader with no albedo is "transparent": it is not drawn, and it is not magenta.
	function mount:describe(mat_name)
		if not mat_name or mat_name == "" then
			return nil, "empty"
		end
		if tool_skip(mat_name) then
			return nil, "tool"
		end
		local function try_name(raw)
			local vm, vt = vmt_path(raw)
			local text = blob(vm)
			if text then
				local keys = keys_with_albedo(vm, 0, { [vm] = true }) or vmt.pairs(text)
				local base = keys["$basetexture"]
				local shader = (keys.__shader or vmt.header(text) or ""):lower()
				if base and base ~= "" then
					return vmt.vtf_key(base), vmt.transform(keys["$basetexturetransform"])
				end
				if shader == "water" then
					local tint = tint_key(keys)
					if tint then
						return tint, nil
					end
				end
				if TRANSPARENT[shader] then
					return nil, "transparent"
				end
				return nil, "no basetexture"
			end
			if blob(vt) then
				return vt, nil
			end
			return nil
		end
		local key, xf = try_name(mat_name)
		if key then
			return key, xf
		end
		if xf == "transparent" then
			return nil, "transparent"
		end
		local stripped = mat_name:lower():gsub("\\", "/"):gsub("^maps/[^/]+/", "")
		stripped = stripped:gsub("_-?%d+_-?%d+_-?%d+$", "")
		if stripped ~= mat_name:lower() then
			key, xf = try_name(stripped)
			if key then
				return key, xf
			end
			if xf == "transparent" then
				return nil, "transparent"
			end
		end
		return nil, xf or "missing"
	end

	local function decode_key(key, max_edge)
		if not key or key == "" then
			return nil
		end
		local hit = decoded[key]
		if hit then
			return hit
		end
		local bytes = blob(key)
		if not bytes then
			return nil
		end
		local w, h, rgba = vtf.decode(bytes, max_edge)
		if not w then
			return nil
		end
		hit = { w = w, h = h, rgba = rgba }
		decoded[key] = hit
		return hit
	end

	-- Resolved VMT keys, including patch includes. Nil when the name has no albedo.
	local function material_keys(mat_name)
		if not mat_name or mat_name == "" or tool_skip(mat_name) then
			return nil
		end
		local function try_name(raw)
			local vm = vmt_path(raw)
			if not blob(vm) then
				return nil
			end
			return keys_with_albedo(vm, 0, { [vm] = true })
		end
		local function usable(keys)
			return keys and keys["$basetexture"] and keys["$basetexture"] ~= ""
		end
		local keys = try_name(mat_name)
		if usable(keys) then
			return keys
		end
		local stripped = mat_name:lower():gsub("\\", "/"):gsub("^maps/[^/]+/", "")
		stripped = stripped:gsub("_-?%d+_-?%d+_-?%d+$", "")
		if stripped ~= mat_name:lower() then
			local alt = try_name(stripped)
			if usable(alt) then
				return alt
			end
		end
		return keys
	end

	function mount:material(mat_name, max_edge)
		local key, xf_or_err = self:describe(mat_name)
		if not key then
			return nil, xf_or_err
		end
		local hit = decoded[key]
		if not hit then
			local tint_r, tint_g, tint_b = key:match("^__tint/(%d+)/(%d+)/(%d+)$")
			if tint_r then
				local rgba = string.rep(string.char(tonumber(tint_r), tonumber(tint_g), tonumber(tint_b), 255), 16)
				hit = { w = 4, h = 4, rgba = rgba }
			else
				hit = decode_key(key, max_edge)
				if not hit then
					return nil, "missing " .. key
				end
			end
			decoded[key] = hit
		end
		local out = {
			key = key,
			w = hit.w,
			h = hit.h,
			rgba = hit.rgba,
			transform = xf_or_err,
		}
		if key:sub(1, 7) == "__tint/" then
			return out
		end
		local keys = material_keys(mat_name)
		if not keys or not vmt.transition(keys) then
			return out
		end
		local key2 = vmt.vtf_key(keys["$basetexture2"])
		local hit2 = decode_key(key2, max_edge)
		if not hit2 then
			return out
		end
		out.blend = true
		out.key2 = key2
		out.w2 = hit2.w
		out.h2 = hit2.h
		out.rgba2 = hit2.rgba
		out.transform2 = vmt.transform(keys["$basetexturetransform2"])
		local mask_name = keys["$blendmodulatetexture"]
		if mask_name and mask_name ~= "" then
			local mask_key = vmt.vtf_key(mask_name)
			local mask = decode_key(mask_key, max_edge)
			if mask then
				out.mask_key = mask_key
				out.mask_w = mask.w
				out.mask_h = mask.h
				out.mask_rgba = mask.rgba
			end
		end
		local detail_name = keys["$detail"]
		if detail_name and detail_name ~= "" then
			local detail_key = vmt.vtf_key(detail_name)
			local detail = decode_key(detail_key, max_edge)
			if detail then
				out.detail_key = detail_key
				out.detail_w = detail.w
				out.detail_h = detail.h
				out.detail_rgba = detail.rgba
				out.detail_scale = tonumber(keys["$detailscale"]) or 1
				out.detail_blend = tonumber(keys["$detailblendfactor"]) or 1
			end
		end
		return out
	end

	-- One draw group per surface. Shared albedos keep the same key.
	-- A missing file becomes a magenta stand-in so the hole is visible.
	function mount:bind(surfaces, max_edge)
		local images = {}
		local out = {}
		local missing = {}
		local seen = {}
		for i = 1, #surfaces do
			local s = surfaces[i]
			local mat, err = self:material(s.name, max_edge)
			local group
			if not mat and (err == "transparent" or err == "tool") then
				group = nil
			elseif not mat then
				if #missing < 12 then
					missing[#missing + 1] = s.name .. " (" .. tostring(err) .. ")"
				end
				group = {
					key = "__missing",
					w = 4,
					h = 4,
					rgba = MISSING,
					transform = nil,
					verts = s.verts,
					name = s.name,
					lit = s.lit,
				}
			else
				if not images[mat.key] then
					images[mat.key] = mat
					seen[#seen + 1] = mat.key
					io.write(string.format("albedo %s %dx%d\n", mat.key, mat.w, mat.h))
					io.flush()
				end
				group = {
					key = mat.key,
					w = mat.w,
					h = mat.h,
					rgba = mat.rgba,
					transform = mat.transform,
					verts = s.verts,
					name = s.name,
					blend = mat.blend,
					key2 = mat.key2,
					w2 = mat.w2,
					h2 = mat.h2,
					rgba2 = mat.rgba2,
					transform2 = mat.transform2,
					mask_key = mat.mask_key,
					mask_w = mat.mask_w,
					mask_h = mat.mask_h,
					mask_rgba = mat.mask_rgba,
					detail_key = mat.detail_key,
					detail_w = mat.detail_w,
					detail_h = mat.detail_h,
					detail_rgba = mat.detail_rgba,
					detail_scale = mat.detail_scale,
					detail_blend = mat.detail_blend,
					lit = s.lit,
				}
				if mat.blend then
					io.write(string.format("blend %s + %s%s\n", s.name, mat.key2, mat.mask_key and " mask" or ""))
					io.flush()
				end
			end
			if group then
				out[#out + 1] = group
			end
		end
		return out, { missing = missing, textures = #seen }
	end

	return mount
end

return M
