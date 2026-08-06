-- XR Home Passthrough — green-key void + OpenXR blend.
-- Passthrough is only meaningful on xr_infmap_passthrough with OpenXR backend.
-- Quick-menu toggle is registered only under those conditions.

if not CLIENT then return end

CubeHome = CubeHome or {}
CubeHome.Passthrough = CubeHome.Passthrough or {
	active = false,
	menu = false,
}

-- Default ON for this map (user can toggle off in quick menu while on map + OpenXR).
local cvEnable = CreateClientConVar("cube_home_passthrough", "1", true, FCVAR_ARCHIVE,
	"XR Home Passthrough: green-key void + OpenXR alpha blend (this map only)")
local cvTol = CreateClientConVar("cube_home_passthrough_tol", "0.22", true, FCVAR_ARCHIVE,
	"Green-key distance 0..1 (higher = softer edge / more punched)")
local cvWorld = CreateClientConVar("cube_home_drawworld", "0", true, FCVAR_ARCHIVE,
	"0 = hide brush world (recommended); 1 = show Source world")

local MENU_NAME = "Passthrough"
local MENU_ID = "cube_home_passthrough"
local voidMat

local function isOpenXR()
	if isfunction(VRMOD_GetBackend) then
		local b = VRMOD_GetBackend()
		if isstring(b) and string.lower(b) == "openxr" then return true end
	end
	if vrmod and vrmod.GetNativePolicy then
		local p = vrmod.GetNativePolicy()
		if istable(p) and p.backend == "openxr" then return true end
	end
	-- Module table export after require
	if istable(vrmod) and isfunction(vrmod.GetBackend) then
		local ok, b = pcall(vrmod.GetBackend)
		if ok and isstring(b) and string.lower(b) == "openxr" then return true end
	end
	return false
end

local function onHomeMap()
	return CubeHome.IsHomeMap and CubeHome.IsHomeMap()
end

--- Passthrough controls allowed only on this map + OpenXR + VR live.
function CubeHome.CanUsePassthrough()
	return onHomeMap() and isOpenXR() and g_VR and g_VR.active
end

local function applyOpenXR(on)
	if not isfunction(VRMOD_SetEnvironmentBlendMode) then return false end
	if on then
		local kn = CubeHome.VOID_KEY_N or { r = 0, g = 1, b = 0 }
		if isfunction(VRMOD_SetPassthroughChromaKey) then
			VRMOD_SetPassthroughChromaKey(kn.r or 0, kn.g or 1, kn.b or 0)
		end
		if isfunction(VRMOD_SetPassthroughChroma) then
			VRMOD_SetPassthroughChroma(true, math.Clamp(cvTol:GetFloat(), 0.05, 0.6))
		end
		VRMOD_SetEnvironmentBlendMode(3) -- AUTO prefer ALPHA_BLEND
	else
		if isfunction(VRMOD_SetPassthroughChroma) then
			VRMOD_SetPassthroughChroma(false, 0.22)
		end
		VRMOD_SetEnvironmentBlendMode(0) -- OPAQUE
	end
	return true
end

function CubeHome.EnablePassthrough(reason)
	if not onHomeMap() then return end
	if not isOpenXR() then
		print("[cube_home] passthrough requires OpenXR backend")
		return
	end
	local ok = applyOpenXR(true)
	CubeHome.Passthrough.active = true

	RunConsoleCommand("r_drawworld", cvWorld:GetBool() and "1" or "0")
	RunConsoleCommand("r_3dsky", "0")
	RunConsoleCommand("r_drawskybox", "0")
	RunConsoleCommand("fog_override", "1")
	-- Fog matches void key so edges don't flash wrong color
	RunConsoleCommand("fog_color", "0", "255", "0")
	RunConsoleCommand("fog_start", "99999")
	RunConsoleCommand("fog_end", "99999")

	print(string.format("[cube_home] passthrough ON reason=%s openxr_api=%s",
		tostring(reason or "?"), tostring(ok)))
end

function CubeHome.DisablePassthrough(reason)
	CubeHome.Passthrough.active = false
	applyOpenXR(false)
	if onHomeMap() then
		RunConsoleCommand("r_drawworld", "1")
		RunConsoleCommand("r_3dsky", "1")
		RunConsoleCommand("r_drawskybox", "1")
		RunConsoleCommand("fog_override", "0")
	end
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
	-- Keep convar in sync so rejoin remembers preference for this map session
	RunConsoleCommand("cube_home_passthrough", nextOn and "1" or "0")
	CubeHome.SetPassthrough(nextOn, "quickmenu")
end

-- ── Green void backdrop (color-key only; models stay normal) ──
local function ensureVoidMat()
	if voidMat and not voidMat:IsError() then return voidMat end
	voidMat = Material("cube_home/pt_void")
	if voidMat:IsError() then
		voidMat = Material("vgui/white")
	end
	return voidMat
end

local size = 1e9
local minZ = -1e5
local voidMesh
local function ensureVoidMesh()
	if voidMesh then return voidMesh end
	voidMesh = Mesh()
	voidMesh:BuildFromTriangles({
		{ pos = Vector(size, size, minZ), normal = Vector(0, 0, 1), u = 0, v = 0 },
		{ pos = Vector(size, -size, minZ), normal = Vector(0, 0, 1), u = 1, v = 0 },
		{ pos = Vector(-size, -size, minZ), normal = Vector(0, 0, 1), u = 1, v = 1 },
		{ pos = Vector(size, size, minZ), normal = Vector(0, 0, 1), u = 0, v = 0 },
		{ pos = Vector(-size, -size, minZ), normal = Vector(0, 0, 1), u = 1, v = 1 },
		{ pos = Vector(-size, size, minZ), normal = Vector(0, 0, 1), u = 0, v = 1 },
	})
	return voidMesh
end

hook.Add("PostDraw2DSkyBox", "cube_home_green_void", function()
	if not onHomeMap() or not CubeHome.Passthrough.active then return end
	local mat = ensureVoidMat()
	local mesh = ensureVoidMesh()
	render.OverrideDepthEnable(true, false)
	render.SetMaterial(mat)
	render.ResetModelLighting(1, 1, 1)
	render.SetLocalModelLights()
	-- Force pure green (key) even if material falls back to white
	render.SetColorModulation(0, 1, 0)
	mesh:Draw()
	render.SetColorModulation(1, 1, 1)
	render.OverrideDepthEnable(false, false)
end)

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

-- ── Quick menu: only on this map + OpenXR + VR ──
local function menuHint()
	if CubeHome.Passthrough.active then
		return "ON · green void · room through"
	end
	return "OFF · full VR opaque"
end

function CubeHome.RemovePassthroughMenu()
	if not CubeHome.Passthrough.menu then return end
	if vrmod and vrmod.RemoveInGameMenuItem then
		vrmod.RemoveInGameMenuItem(MENU_NAME, nil, true)
	end
	-- Drop by id if still present
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

	-- Fixed name so layout/id stays stable; hint shows ON/OFF.
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
		-- Leave world look alone until VR; XR blend only when module live
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
		-- World void look even before VR; XR chroma only when VR starts
		if cvEnable:GetBool() then
			RunConsoleCommand("r_drawworld", cvWorld:GetBool() and "1" or "0")
			RunConsoleCommand("r_3dsky", "0")
			RunConsoleCommand("r_drawskybox", "0")
		end
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
		VRMOD_SetPassthroughChroma(false, 0.22)
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
	if not CubeHome.CanUsePassthrough() then
		-- Preference stored; applies next VR session on this map
		return
	end
	CubeHome.SetPassthrough(tobool(new), "cvar")
end, "cube_home_pt")

cvars.AddChangeCallback("cube_home_passthrough_tol", function(_, _, new)
	if CubeHome.Passthrough.active and isfunction(VRMOD_SetPassthroughChroma) then
		VRMOD_SetPassthroughChroma(true, math.Clamp(tonumber(new) or 0.22, 0.05, 0.6))
	end
end, "cube_home_pt_tol")

concommand.Add("cube_home_passthrough_toggle", function()
	if not onHomeMap() then
		print("[cube_home] passthrough only on map " .. tostring(CubeHome.MAP))
		return
	end
	if not isOpenXR() then
		print("[cube_home] passthrough only with OpenXR (vrmod_xr)")
		return
	end
	if not (g_VR and g_VR.active) then
		print("[cube_home] start VR first")
		return
	end
	CubeHome.TogglePassthrough()
end)
