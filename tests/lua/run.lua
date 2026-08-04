#!/usr/bin/env luajit
-- Offline Lua test runner for gVRMod
-- Usage: luajit tests/lua/run.lua   (from gVRMod root)

local ROOT = arg and arg[0] and arg[0]:match("(.*/)") or "./"
-- When run as tests/lua/run.lua, ROOT is tests/lua/
local function find_repo_root()
	local p = ROOT
	for _ = 1, 6 do
		local f = io.open(p .. "../../addon/vrmod-x64/lua/vrmod/utils/sh_math.lua", "r")
			or io.open(p .. "../addon/vrmod-x64/lua/vrmod/utils/sh_math.lua", "r")
			or io.open(p .. "addon/vrmod-x64/lua/vrmod/utils/sh_math.lua", "r")
		if f then
			f:close()
			if io.open(p .. "addon/vrmod-x64/lua/vrmod/utils/sh_math.lua", "r") then
				return p
			end
			if io.open(p .. "../addon/vrmod-x64/lua/vrmod/utils/sh_math.lua", "r") then
				return p .. "../"
			end
			return p .. "../../"
		end
		p = p .. "../"
	end
	return "./"
end

local REPO = os.getenv("GVRMOD_ROOT")
if not REPO or REPO == "" then
	-- prefer cwd if it looks like gVRMod
	local f = io.open("addon/vrmod-x64/lua/vrmod/utils/sh_math.lua", "r")
	if f then
		f:close()
		REPO = "./"
	else
		REPO = find_repo_root()
	end
end

package.path = REPO .. "tests/lua/?.lua;" .. REPO .. "tests/lua/?/init.lua;" .. package.path

local mock = require("mock.gmod")
mock.install(_G)

g_VR = g_VR or {}
vrmod = vrmod or {}
vrmod.utils = vrmod.utils or {}
vrmod.GetConvars = vrmod.GetConvars or function() return {}, {} end

local function dofile_repo(rel)
	local path = REPO .. rel
	local chunk, err = loadfile(path)
	if not chunk then
		error("loadfile failed " .. path .. ": " .. tostring(err))
	end
	return chunk()
end

-- Production pure modules
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_math.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_experience.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_glide_sot.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_stage_pack.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_stereo_load.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_cube_return.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_warm_attach.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_hud_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_laser_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_mat_queue_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_submit_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_pose_sot_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_menu_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_engine_blacklist_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_bindings_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_stereo_selftest_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_swap_eyes_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_flyaway_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_viewscale_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_fovz_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_hand_bullet_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_worldmodel_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_init_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_border_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_hmd_walk_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_hand_stuck_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_nested_rt_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_grab_end_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_desktop_mirror_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/sh_false_per_eye_law.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/cl_rendering.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/utils/cl_desktop_cam.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/api/sh_api.lua")
dofile_repo("addon/vrmod-x64/lua/vrmod/api/cl_api.lua")

-- QuickMenu IdFromName (minimal)
vrmod.QuickMenu = vrmod.QuickMenu or {}
function vrmod.QuickMenu.IdFromName(name)
	if not name then return "item" end
	local id = string.lower(tostring(name)):gsub("%s+", "_"):gsub("[^%w_]", "")
	if id == "" then id = "item" end
	return id
end

local H = require("harness")
local env = { vrmod = vrmod, g_VR = g_VR }

local units = {
	"unit.math_test",
	"unit.experience_test",
	"unit.menu_test",
}

for _, u in ipairs(units) do
	local mod = require(u)
	if type(mod) == "function" then
		mod(H, env)
	end
end

local code = H.run_all()
os.exit(code)
