-- Valve material text. Reads $basetexture and the common $basetexturetransform.
-- Patch materials are resolved by the content mount, which follows "include".
local M = {}

function M.header(text)
	if not text then
		return ""
	end
	return (text:match('^%s*"([^"]+)"') or ""):lower()
end

function M.pairs(text)
	local map = {}
	if not text then
		return map
	end
	for key, val in text:gmatch('"([^"]+)"%s*"([^"]*)"') do
		map[key:lower()] = val
	end
	return map
end

-- Source order: translate to -center, scale, rotate Z, translate back by center + translate.
-- Column vector. rot is degrees. Identity (scale 1, rot 0, translate 0) returns nil.
function M.transform(str)
	if not str or str == "" then
		return nil
	end
	local cx, cy = str:match("center%s+([%-%d%.]+)%s+([%-%d%.]+)")
	local sx, sy = str:match("scale%s+([%-%d%.]+)%s+([%-%d%.]+)")
	local rot = str:match("rotate%s+([%-%d%.]+)")
	local tx, ty = str:match("translate%s+([%-%d%.]+)%s+([%-%d%.]+)")
	local xf = {
		cx = tonumber(cx) or 0.5,
		cy = tonumber(cy) or 0.5,
		sx = tonumber(sx) or 1,
		sy = tonumber(sy) or 1,
		rot = tonumber(rot) or 0,
		tx = tonumber(tx) or 0,
		ty = tonumber(ty) or 0,
	}
	if math.abs(xf.sx - 1) < 1e-6 and math.abs(xf.sy - 1) < 1e-6
		and math.abs(xf.rot) < 1e-6 and math.abs(xf.tx) < 1e-6 and math.abs(xf.ty) < 1e-6 then
		return nil
	end
	return xf
end

-- $alphatest "1" punches the card. Absent or 0 stays solid.
function M.alphatest(keys)
	local a = keys and keys["$alphatest"]
	return a == "1" or a == "1.0"
end

function M.apply_uv(u, v, xf)
	if not xf then
		return u, v
	end
	local du = (u - xf.cx) * xf.sx
	local dv = (v - xf.cy) * xf.sy
	local rad = math.rad(xf.rot)
	local c = math.cos(rad)
	local s = math.sin(rad)
	local ru = c * du - s * dv
	local rv = s * du + c * dv
	return ru + xf.cx + xf.tx, rv + xf.cy + xf.ty
end

function M.vtf_key(base)
	if not base then
		return nil
	end
	base = base:gsub("\\", "/"):gsub("^%s+", ""):gsub("%s+$", "")
	if base:lower():sub(-4) == ".vtf" then
		base = base:sub(1, -5)
	end
	if base:lower():sub(1, 10) == "materials/" then
		return base:lower() .. ".vtf"
	end
	return "materials/" .. base:lower() .. ".vtf"
end

return M
