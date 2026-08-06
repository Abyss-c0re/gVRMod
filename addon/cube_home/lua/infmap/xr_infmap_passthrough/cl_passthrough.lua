-- XR Home Passthrough — DEPTH void (no color key of any kind).
--
-- Black/green/magenta keys punch models that share that color. Wrong.
--
-- Method: hide world/sky so empty pixels keep *clear depth* (~1.0). Props write
-- closer depth. Module sets alpha=0 only where depth is at far/clear — RGB ignored.
-- OpenXR ALPHA_BLEND composites the room under those holes.
--
-- If depth RT is unavailable, void is refused (opaque) rather than color-keying.

if not CLIENT then return end

CubeHome = CubeHome or {}
CubeHome.Passthrough = CubeHome.Passthrough or {
	active = false,
	menu = false,
}

local cvEnable = CreateClientConVar("cube_home_passthrough", "1", true, FCVAR_ARCHIVE,
	"XR Home: depth void + OpenXR alpha (this map only; no color key)")
local cvTol = CreateClientConVar("cube_home_passthrough_tol", "0.04", true, FCVAR_ARCHIVE,
	"Depth far-threshold softness (maps to ~0.998–0.999; not a color thr)")
local cvWorld = CreateClientConVar("cube_home_drawworld", "0", true, FCVAR_ARCHIVE,
	"0 = hide brush world (depth stays clear); 1 = show world")

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
	if on then
		if isfunction(VRMOD_SetPassthroughChroma) then
			-- Enable depth void; tol only softens far-depth edge (not RGB).
			VRMOD_SetPassthroughChroma(true, math.Clamp(cvTol:GetFloat(), 0.01, 0.1))
		end
		VRMOD_SetEnvironmentBlendMode(3)
	else
		if isfunction(VRMOD_SetPassthroughChroma) then
			VRMOD_SetPassthroughChroma(false, 0.04)
		end
		VRMOD_SetEnvironmentBlendMode(0)
	end
	return true
end

local function applyVoidWorld(on)
	if on then
		RunConsoleCommand("r_drawworld", cvWorld:GetBool() and "1" or "0")
		RunConsoleCommand("r_3dsky", "0")
		RunConsoleCommand("r_drawskybox", "0")
		RunConsoleCommand("fog_override", "1")
		RunConsoleCommand("fog_start", "99999")
		RunConsoleCommand("fog_end", "99999")
	else
		RunConsoleCommand("r_drawworld", "1")
		RunConsoleCommand("r_3dsky", "1")
		RunConsoleCommand("r_drawskybox", "1")
		RunConsoleCommand("fog_override", "0")
	end
end

function CubeHome.EnablePassthrough(reason)
	if not onHomeMap() then return end
	if not isOpenXR() then
		print("[cube_home] passthrough requires OpenXR")
		return
	end
	local ok = applyOpenXR(true)
	CubeHome.Passthrough.active = true
	applyVoidWorld(true)
	print(string.format("[cube_home] passthrough ON (DEPTH void, no color key) reason=%s api=%s",
		tostring(reason or "?"), tostring(ok)))
end

function CubeHome.DisablePassthrough(reason)
	CubeHome.Passthrough.active = false
	applyOpenXR(false)
	if onHomeMap() then applyVoidWorld(false) end
	print("[cube_home] passthrough OFF reason=" .. tostring(reason or "?"))
end

function CubeHome.SetPassthrough(on, reason)
	if on then
		CubeHome.EnablePassthrough(reason)
	else
		CubeHome.DisablePassthrough(reason)
	end
	CubeHome.RefreshPassthroughMenu()
end

function CubeHome.TogglePassthrough()
	if not CubeHome.CanUsePassthrough() then return end
	local nextOn = not CubeHome.Passthrough.active
	RunConsoleCommand("cube_home_passthrough", nextOn and "1" or "0")
	CubeHome.SetPassthrough(nextOn, "quickmenu")
end

hook.Add("SetupWorldFog", "cube_home_pt_fog", function()
	if not onHomeMap() or not CubeHome.Passthrough.active then return end
	render.FogMode(MATERIAL_FOG_NONE)
	return true
end)

hook.Add("PreDrawOpaqueRenderables", "cube_home_pt_hide_terrain", function()
	if not onHomeMap() or not CubeHome.Passthrough.active then return end
	if InfMap and InfMap.client_chunks then
		for _, row in pairs(InfMap.client_chunks) do
			if istable(row) then
				for _, e in pairs(row) do
					if IsValid(e) then e:SetNoDraw(true) end
				end
			end
		end
	end
end)

local function menuHint()
	if CubeHome.Passthrough.active then
		return "ON · depth void · room through sky"
	end
	return "OFF · full VR opaque"
end

function CubeHome.RemovePassthroughMenu()
	if not CubeHome.Passthrough.menu then return end
	if vrmod and vrmod.RemoveInGameMenuItem then
		vrmod.RemoveInGameMenuItem(MENU_NAME, nil, true)
	end
	if g_VR and istable(g_VR.menuItems) then
		for i = #g_VR.menuItems, 1, -1 do
			local it = g_VR.menuItems[i]
			if it and it.id == MENU_ID then
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
		timer.Simple(0.05, function()
			CubeHome.RefreshPassthroughMenu()
		end)
	end, false, menuHint(), MENU_ID)
	CubeHome.Passthrough.menu = true
end

local function syncFromCvar()
	if not onHomeMap() then
		CubeHome.RemovePassthroughMenu()
		if CubeHome.Passthrough.active then
			CubeHome.DisablePassthrough("left_map")
		end
		return
	end
	if not isOpenXR() or not (g_VR and g_VR.active) then
		CubeHome.RemovePassthroughMenu()
		if CubeHome.Passthrough.active and isfunction(VRMOD_SetEnvironmentBlendMode) then
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
	timer.Simple(1, function()
		if cvEnable:GetBool() then applyVoidWorld(true) end
	end)
end)

hook.Add("VRMod_Start", "cube_home_pt_vrstart", function()
	timer.Simple(0.4, function()
		syncFromCvar()
	end)
end)

hook.Add("VRMod_Exit", "cube_home_pt_vrexit", function()
	CubeHome.RemovePassthroughMenu()
	if isfunction(VRMOD_SetEnvironmentBlendMode) then
		VRMOD_SetEnvironmentBlendMode(0)
	end
	if isfunction(VRMOD_SetPassthroughChroma) then
		VRMOD_SetPassthroughChroma(false, 0.04)
	end
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
	if not onHomeMap() then return end
	if not CubeHome.CanUsePassthrough() then return end
	CubeHome.SetPassthrough(tobool(new), "cvar")
end, "cube_home_pt")

cvars.AddChangeCallback("cube_home_passthrough_tol", function(_, _, new)
	if CubeHome.Passthrough.active and isfunction(VRMOD_SetPassthroughChroma) then
		VRMOD_SetPassthroughChroma(true, math.Clamp(tonumber(new) or 0.04, 0.01, 0.1))
	end
end, "cube_home_pt_tol")

concommand.Add("cube_home_passthrough_toggle", function()
	if not CubeHome.CanUsePassthrough() then
		print("[cube_home] need map " .. tostring(CubeHome.MAP) .. " + OpenXR + VR")
		return
	end
	CubeHome.TogglePassthrough()
end)
