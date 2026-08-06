-- XR Home Passthrough — InfMap terrain is already solid magenta (#FF00FF).
-- This file only toggles chroma punch + OpenXR blend. Does NOT paint black.

if not CLIENT then return end

CubeHome = CubeHome or {}
CubeHome.Passthrough = CubeHome.Passthrough or { active = false, menu = false }

CubeHome.VOID_KEY = { r = 255, g = 0, b = 255 }
CubeHome.VOID_KEY_N = { r = 1, g = 0, b = 1 }

local cvEnable = CreateClientConVar("cube_home_passthrough", "1", true, FCVAR_ARCHIVE,
	"XR Home: punch InfMap magenta → room passthrough")
local cvTol = CreateClientConVar("cube_home_passthrough_tol", "0.22", true, FCVAR_ARCHIVE,
	"Magenta key distance")

local MENU_NAME = "Passthrough"
local MENU_ID = "cube_home_passthrough"

local function isOpenXR()
	if isfunction(VRMOD_GetBackend) then
		local b = VRMOD_GetBackend()
		if isstring(b) and string.lower(b) == "openxr" then return true end
	end
	if vrmod and vrmod.GetNativePolicy then
		local p = vrmod.GetNativePolicy()
		if istable(p) and p.backend == "openxr" then return true end
	end
	if istable(vrmod) and isfunction(vrmod.GetBackend) then
		local ok, b = pcall(vrmod.GetBackend)
		if ok and isstring(b) and string.lower(b) == "openxr" then return true end
	end
	return false
end

local function onHomeMap()
	return CubeHome.IsHomeMap and CubeHome.IsHomeMap()
end

function CubeHome.CanUsePassthrough()
	return onHomeMap() and isOpenXR() and g_VR and g_VR.active
end

local function applyOpenXR(on)
	if not isfunction(VRMOD_SetEnvironmentBlendMode) then return false end
	local kn = CubeHome.VOID_KEY_N
	if on then
		if isfunction(VRMOD_SetPassthroughChromaKey) then
			VRMOD_SetPassthroughChromaKey(1, 0, 1)
		end
		if isfunction(VRMOD_SetPassthroughChromaMask) then
			VRMOD_SetPassthroughChromaMask(1) -- magenta only
		end
		if isfunction(VRMOD_SetPassthroughChroma) then
			VRMOD_SetPassthroughChroma(true, math.Clamp(cvTol:GetFloat(), 0.12, 0.40))
		end
		VRMOD_SetEnvironmentBlendMode(1)
	else
		if isfunction(VRMOD_SetPassthroughChroma) then
			VRMOD_SetPassthroughChroma(false, 0.22)
		end
		VRMOD_SetEnvironmentBlendMode(0)
	end
	return true
end

-- Keep map magenta always; PT only toggles key punch (not map color).
local function applyWorldForPT(on)
	-- Never leave black: brush world off; InfMap magenta mesh is the map.
	RunConsoleCommand("r_drawworld", "0")
	RunConsoleCommand("r_3dsky", "0")
	RunConsoleCommand("r_drawskybox", "0")
	RunConsoleCommand("fog_override", "1")
	RunConsoleCommand("fog_color", "255", "0", "255")
	RunConsoleCommand("fog_start", "99999")
	RunConsoleCommand("fog_end", "99999")
	if InfMap then
		InfMap.terrain_material = "cube_home/pt_void"
		InfMap.render_distance = math.max(InfMap.render_distance or 0, 2)
		InfMap.planet_render_distance = 0
	end
end

function CubeHome.EnablePassthrough(reason)
	if not onHomeMap() then return end
	if not isOpenXR() then
		print("[cube_home] passthrough needs OpenXR")
		return
	end
	applyWorldForPT(true)
	local ok = applyOpenXR(true)
	CubeHome.Passthrough.active = true
	print(string.format("[cube_home] PT ON (InfMap magenta punch) reason=%s ok=%s",
		tostring(reason or "?"), tostring(ok)))
end

function CubeHome.DisablePassthrough(reason)
	CubeHome.Passthrough.active = false
	applyOpenXR(false)
	-- Map stays magenta even with PT off
	if onHomeMap() then applyWorldForPT(false) end
	print("[cube_home] PT OFF reason=" .. tostring(reason or "?"))
end

function CubeHome.SetPassthrough(on, reason)
	if on then CubeHome.EnablePassthrough(reason) else CubeHome.DisablePassthrough(reason) end
	CubeHome.RefreshPassthroughMenu()
end

function CubeHome.TogglePassthrough()
	if not CubeHome.CanUsePassthrough() then return end
	local nextOn = not CubeHome.Passthrough.active
	RunConsoleCommand("cube_home_passthrough", nextOn and "1" or "0")
	CubeHome.SetPassthrough(nextOn, "quickmenu")
end

-- Always keep clear/sky magenta on this map (no black start).
hook.Add("PreDrawSkyBox", "cube_home_always_magenta", function()
	if not onHomeMap() then return end
	render.Clear(255, 0, 255, 255, true, true)
	return true
end)

hook.Add("SetupWorldFog", "cube_home_pt_fog", function()
	if not onHomeMap() then return end
	render.FogMode(MATERIAL_FOG_NONE)
	return true
end)

local function menuHint()
	if CubeHome.Passthrough.active then
		return "ON · magenta InfMap → room"
	end
	return "OFF · magenta world (no punch)"
end

function CubeHome.RemovePassthroughMenu()
	if not CubeHome.Passthrough.menu then return end
	if vrmod and vrmod.RemoveInGameMenuItem then
		vrmod.RemoveInGameMenuItem(MENU_NAME, nil, true)
	end
	if g_VR and istable(g_VR.menuItems) then
		for i = #g_VR.menuItems, 1, -1 do
			if g_VR.menuItems[i] and g_VR.menuItems[i].id == MENU_ID then
				table.remove(g_VR.menuItems, i)
			end
		end
	end
	CubeHome.Passthrough.menu = false
end

function CubeHome.RefreshPassthroughMenu()
	CubeHome.RemovePassthroughMenu()
	if not CubeHome.CanUsePassthrough() then return end
	if not vrmod or not vrmod.AddInGameMenuItem then return end
	vrmod.AddInGameMenuItem(MENU_NAME, 5, 2, function()
		CubeHome.TogglePassthrough()
		timer.Simple(0.05, function() CubeHome.RefreshPassthroughMenu() end)
	end, false, menuHint(), MENU_ID)
	CubeHome.Passthrough.menu = true
end

local function syncFromCvar()
	if not onHomeMap() then
		CubeHome.RemovePassthroughMenu()
		if CubeHome.Passthrough.active then CubeHome.DisablePassthrough("left_map") end
		return
	end
	applyWorldForPT(true) -- always magenta map
	if not isOpenXR() or not (g_VR and g_VR.active) then
		CubeHome.RemovePassthroughMenu()
		if CubeHome.Passthrough.active then
			applyOpenXR(false)
			CubeHome.Passthrough.active = false
		end
		return
	end
	if cvEnable:GetBool() then
		CubeHome.EnablePassthrough("sync")
	else
		CubeHome.DisablePassthrough("sync")
	end
	CubeHome.RefreshPassthroughMenu()
end

hook.Add("InitPostEntity", "cube_home_pt_boot", function()
	if not onHomeMap() then return end
	applyWorldForPT(true)
end)

hook.Add("VRMod_Start", "cube_home_pt_vrstart", function()
	timer.Simple(0.8, function() syncFromCvar() end)
end)

hook.Add("VRMod_Exit", "cube_home_pt_vrexit", function()
	CubeHome.RemovePassthroughMenu()
	if isfunction(VRMOD_SetEnvironmentBlendMode) then VRMOD_SetEnvironmentBlendMode(0) end
	if isfunction(VRMOD_SetPassthroughChroma) then VRMOD_SetPassthroughChroma(false, 0.22) end
	CubeHome.Passthrough.active = false
end)

hook.Add("VRMod_OpenQuickMenu", "cube_home_pt_qm", function()
	if CubeHome.CanUsePassthrough() then
		CubeHome.RefreshPassthroughMenu()
	else
		CubeHome.RemovePassthroughMenu()
	end
end)

cvars.AddChangeCallback("cube_home_passthrough", function(_, _, new)
	if not onHomeMap() or not CubeHome.CanUsePassthrough() then return end
	CubeHome.SetPassthrough(tobool(new), "cvar")
end, "cube_home_pt")

cvars.AddChangeCallback("cube_home_passthrough_tol", function(_, _, new)
	if CubeHome.Passthrough.active and isfunction(VRMOD_SetPassthroughChroma) then
		VRMOD_SetPassthroughChroma(true, math.Clamp(tonumber(new) or 0.22, 0.12, 0.40))
	end
end, "cube_home_pt_tol")

concommand.Add("cube_home_passthrough_toggle", function()
	if not CubeHome.CanUsePassthrough() then
		print("[cube_home] need " .. tostring(CubeHome.MAP) .. " + OpenXR + VR")
		return
	end
	CubeHome.TogglePassthrough()
end)
