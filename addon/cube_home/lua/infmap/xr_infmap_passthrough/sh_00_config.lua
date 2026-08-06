-- XR Home Passthrough — map-local InfMap scripts (game.GetMap() == xr_infmap_passthrough).
-- InfMap requires second map token "infmap" → xr_infmap_passthrough.

CubeHome = CubeHome or {}
CubeHome.MAP = "xr_infmap_passthrough"
CubeHome.TITLE = "XR Home Passthrough"
CubeHome.INFMAP_WORKSHOP = "2905327911"
-- InfMap by Meetric — https://github.com/meetric1/gmod-infinite-map (GPL-3.0)
CubeHome.INFMAP_AUTHOR = "Meetric"
CubeHome.INFMAP_REPO = "https://github.com/meetric1/gmod-infinite-map"
CubeHome.INFMAP_WORKSHOP_URL = "https://steamcommunity.com/workshop/filedetails/?id=2905327911"
-- Pure green void key (module chroma). Not black — preserves dark model pixels.
CubeHome.VOID_KEY = { r = 0, g = 255, b = 0 }
CubeHome.VOID_KEY_N = { r = 0, g = 1, b = 0 } -- 0..1 for module
CubeHome.VOID_TOLERANCE = 0.22
CubeHome.DATA_DIR = "cube_home"
CubeHome.LAYOUT_FILE = "cube_home/layout.json"
CubeHome.LAYOUT_DEFAULT = "cube_home/layout_default.json"

function CubeHome.IsHomeMap(map)
	map = string.lower(map or game.GetMap() or "")
	return map == CubeHome.MAP
end

function CubeHome.DefaultLayout()
	return {
		v = 1,
		name = "XR Home Passthrough",
		seed = 1,
		plaza_radius = 2.5,
		hill_scale = 0.0,
		spawn = { pos = { 0, 0, 48 }, ang = { 0, 90, 0 } },
		zones = {
			{ id = "plaza", label = "Home Plaza", kind = "spawn", pos = { 0, 0, 8 }, size = 512 },
			{ id = "sandbox", label = "Sandbox Pad", kind = "platform", pos = { 800, 0, 8 }, size = 384, color = { 80, 140, 220 } },
			{ id = "range", label = "Practice Range", kind = "platform", pos = { 0, 800, 8 }, size = 384, color = { 220, 140, 80 } },
			{ id = "build", label = "Build Yard", kind = "platform", pos = { -800, 0, 8 }, size = 512, color = { 120, 200, 120 } },
		},
		props = {
			{ id = "center_pad", model = "models/hunter/plates/plate8x8.mdl", pos = { 0, 0, 4 }, ang = { 0, 0, 0 }, frozen = true, color = { 40, 44, 56 } },
			{ id = "center_ring", model = "models/hunter/tubes/circle4x4.mdl", pos = { 0, 0, 6 }, ang = { 0, 0, 0 }, frozen = true, color = { 90, 160, 255 } },
			{ id = "sandbox_pad", model = "models/hunter/plates/plate8x8.mdl", pos = { 800, 0, 4 }, ang = { 0, 0, 0 }, frozen = true, color = { 80, 140, 220 } },
			{ id = "range_pad", model = "models/hunter/plates/plate8x8.mdl", pos = { 0, 800, 4 }, ang = { 0, 0, 0 }, frozen = true, color = { 220, 140, 80 } },
			{ id = "build_pad", model = "models/hunter/plates/plate8x8.mdl", pos = { -800, 0, 4 }, ang = { 0, 0, 0 }, frozen = true, color = { 120, 200, 120 } },
		},
	}
end

local function vec3(t, fallback)
	if isvector(t) then return t end
	if istable(t) and t[1] ~= nil then
		return Vector(tonumber(t[1]) or 0, tonumber(t[2]) or 0, tonumber(t[3]) or 0)
	end
	return fallback or Vector(0, 0, 0)
end

local function ang3(t, fallback)
	if isangle(t) then return t end
	if istable(t) and t[1] ~= nil then
		return Angle(tonumber(t[1]) or 0, tonumber(t[2]) or 0, tonumber(t[3]) or 0)
	end
	return fallback or Angle(0, 0, 0)
end

function CubeHome.Vec(t, fallback)
	return vec3(t, fallback)
end

function CubeHome.Ang(t, fallback)
	return ang3(t, fallback)
end

function CubeHome.NormalizeLayout(raw)
	local d = CubeHome.DefaultLayout()
	if not istable(raw) then return d end
	if raw.v then d.v = tonumber(raw.v) or d.v end
	if raw.name then d.name = tostring(raw.name) end
	if raw.seed ~= nil then d.seed = tonumber(raw.seed) or d.seed end
	if raw.plaza_radius ~= nil then d.plaza_radius = tonumber(raw.plaza_radius) or d.plaza_radius end
	if raw.hill_scale ~= nil then d.hill_scale = tonumber(raw.hill_scale) or d.hill_scale end
	if istable(raw.spawn) then
		d.spawn = d.spawn or {}
		if raw.spawn.pos then d.spawn.pos = { CubeHome.Vec(raw.spawn.pos):Unpack() } end
		if raw.spawn.ang then
			local a = CubeHome.Ang(raw.spawn.ang)
			d.spawn.ang = { a.p, a.y, a.r }
		end
	end
	if istable(raw.zones) then d.zones = raw.zones end
	if istable(raw.props) then d.props = raw.props end
	return d
end

function CubeHome.LoadLayoutFromDisk()
	local path = CubeHome.LAYOUT_FILE
	if file.Exists(path, "DATA") then
		local raw = file.Read(path, "DATA")
		if raw and #raw > 0 then
			local ok, parsed = pcall(util.JSONToTable, raw)
			if ok and istable(parsed) then
				return CubeHome.NormalizeLayout(parsed), "data"
			end
		end
	end
	local defPath = "data/" .. CubeHome.LAYOUT_DEFAULT
	if file.Exists(defPath, "GAME") then
		local raw = file.Read(defPath, "GAME")
		if raw and #raw > 0 then
			local ok, parsed = pcall(util.JSONToTable, raw)
			if ok and istable(parsed) then
				return CubeHome.NormalizeLayout(parsed), "addon"
			end
		end
	end
	return CubeHome.DefaultLayout(), "builtin"
end

function CubeHome.SaveLayout(layout)
	layout = CubeHome.NormalizeLayout(layout or CubeHome.Layout)
	file.CreateDir(CubeHome.DATA_DIR)
	local json = util.TableToJSON(layout, true)
	if not json then return false, "json encode failed" end
	file.Write(CubeHome.LAYOUT_FILE, json)
	CubeHome.Layout = layout
	return true
end

CubeHome.Layout = CubeHome.Layout or CubeHome.DefaultLayout()
