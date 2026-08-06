-- gVRMod Home terrain: large flat plaza for VR, soft hills beyond (config-driven).
-- Requires InfMap base by Meetric (Workshop 2905327911 / github.com/meetric1/gmod-infinite-map)
-- so InfMap.simplex + terrain ents exist. Pattern adapted from InfMap's gm_infmap map scripts.

InfMap.simplex = InfMap.simplex or include("simplex.lua")
InfMap.chunk_resolution = 3

InfMap.filter = InfMap.filter or {}
InfMap.disable_pickup = InfMap.disable_pickup or {}
InfMap.filter["infmap_terrain_collider"] = true
InfMap.filter["infmap_planet"] = true
InfMap.disable_pickup["infmap_terrain_collider"] = true
InfMap.disable_pickup["infmap_planet"] = true

-- No distant planets on the home hub (keeps FPS / VR comfort).
InfMap.planet_render_distance = 0
InfMap.planet_spacing = 50
InfMap.planet_uv_scale = 40
InfMap.planet_resolution = 20
InfMap.planet_tree_resolution = 32
InfMap.planet_data = InfMap.planet_data or {}

local max = 2 ^ 28
local noise2d = InfMap.simplex and InfMap.simplex.Noise2D

local function plazaRadius()
	local r = 2.5
	if CubeHome and CubeHome.Layout and CubeHome.Layout.plaza_radius then
		r = tonumber(CubeHome.Layout.plaza_radius) or r
	end
	return math.max(0.5, r)
end

local function hillScale()
	local s = 1.0
	if CubeHome and CubeHome.Layout and CubeHome.Layout.hill_scale then
		s = tonumber(CubeHome.Layout.hill_scale) or s
	end
	return math.max(0, s)
end

-- Height-function units match InfMap mesh sampling (see base gm_infmap).
function InfMap.height_function(x, y)
	local r = math.sqrt(x * x + y * y)
	local pad = plazaRadius()

	-- Flat home plaza (VR-safe, level floor).
	if r < pad then
		return 0
	end

	-- Soft ramp out of plaza so you don't fall off a cliff.
	if r < pad + 1.2 then
		local t = (r - pad) / 1.2
		return t * t * 80 * hillScale()
	end

	if not noise2d then
		return 0
	end

	-- Calmer hills than stock InfMap (home hub, not mountaineering).
	x = x - 1.5
	local final = (noise2d(x / 30, y / 30 + 100000)) * 12000 * hillScale()
	final = final / math.max((noise2d(x / 120, y / 120) * 12) ^ 2, 1)
	return math.min(final, max * 0.01)
end

function InfMap.planet_height_function(x, y)
	if not noise2d then return 0 end
	return (noise2d(x / 15000, y / 15000) * 2000) / math.max(noise2d(x / 9000, y / 9000) * 10, 1)
end

function InfMap.planet_info(x, y)
	-- Disabled via planet_render_distance = 0; stubs keep API happy.
	return Vector(0, 0, 0), 1, 1
end

if CLIENT then return end

hook.Add("InitPostEntity", "cube_home_physenv", function()
	if not CubeHome or not CubeHome.IsHomeMap() then return end
	local mach = 270079
	physenv.SetPerformanceSettings({ MaxVelocity = mach, MaxAngularVelocity = mach })
	RunConsoleCommand("sv_maxvelocity", tostring(mach))
end)
