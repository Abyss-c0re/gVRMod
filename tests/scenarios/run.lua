#!/usr/bin/env luajit
-- Scenario runner (declarative multi-step product paths, offline-capable)
local ROOT = "./"
if arg and arg[0] then
	local d = arg[0]:match("(.*/)")
	if d then
		-- scenarios/ → repo root
		ROOT = d .. "../../"
	end
end
if not io.open(ROOT .. "addon/vrmod-x64/lua/vrmod/utils/sh_math.lua") then
	ROOT = "./"
end

package.path = ROOT .. "tests/lua/?.lua;" .. package.path
local mock = require("mock.gmod")
mock.install(_G)
g_VR, vrmod = {}, { utils = {} }
vrmod.GetConvars = function() return {}, {} end

local function load_prod(rel)
	assert(loadfile(ROOT .. rel))()
end
load_prod("addon/vrmod-x64/lua/vrmod/utils/sh_math.lua")
load_prod("addon/vrmod-x64/lua/vrmod/api/sh_api.lua")
load_prod("addon/vrmod-x64/lua/vrmod/api/cl_api.lua")
vrmod.QuickMenu = { IdFromName = function(n)
	return string.lower(tostring(n or "item")):gsub("%s+", "_"):gsub("[^%w_]", "")
end }

local failed = 0
local function scenario(name, fn)
	io.write("[SCEN] " .. name .. " ... ")
	local ok, err = pcall(fn)
	if ok then
		print("OK")
	else
		print("FAIL " .. tostring(err))
		failed = failed + 1
	end
end

scenario("smoke.module_version", function()
	assert(vrmod.GetVersion() ~= nil)
end)

scenario("smoke.menu_dedupe", function()
	g_VR.menuItems, g_VR.menuBackup = {}, {}
	vrmod.AddInGameMenuItem("VRClimb", 5, 3, function() end)
	vrmod.AddInGameMenuItem("VRClimb", 5, 3, function() end)
	assert(#g_VR.menuItems == 1)
end)

scenario("smoke.desktop_follow_enum", function()
	assert(vrmod.utils.IsFloorOrCeilingNormal)
	local _, ho = require and nil
	-- load crop
	load_prod("addon/vrmod-x64/lua/vrmod/utils/cl_rendering.lua")
	local vm, h = vrmod.utils.ComputeDesktopCrop(4, 1024, 1024)
	assert(vm == 0 and h == 0)
end)

scenario("smoke.color_roundtrip", function()
	local c = vrmod.utils.ParseColor("12,34,56,78")
	assert(c.r == 12 and c.a == 78)
	assert(vrmod.utils.FormatColor(c) == "12,34,56,78")
end)

print(string.format("\nScenarios: %s", failed == 0 and "all passed" or (failed .. " failed")))
os.exit(failed > 0 and 1 or 0)
