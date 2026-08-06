-- XR Home Passthrough — AR content layer only.
--
-- Prophecy path (not color keys):
--   1) Do not draw the map / InfMap / world / sky (empty RT = clear depth)
--   2) Draw only: CubeHome layout props, local player, VR hands/weapons
--   3) Before XR submit: module clears swapchain alpha=0, depth→alpha holes,
--      OpenXR ALPHA_BLEND composites the real room under the content layer
--
-- Signal to native: VRMOD_SetPassthroughChroma(true) + EnvironmentBlendMode AUTO.

if not CLIENT then return end

CubeHome = CubeHome or {}
CubeHome.Passthrough = CubeHome.Passthrough or {
	active = false,
	menu = false,
}

-- Product default ON for this map.
local cvEnable = CreateClientConVar("cube_home_passthrough", "1", true, FCVAR_ARCHIVE,
	"XR Home AR layer (depth void + OpenXR; no color key)")
local cvTol = CreateClientConVar("cube_home_passthrough_tol", "0.04", true, FCVAR_ARCHIVE,
	"Depth far-edge soft (not a color thr)")

local MENU_NAME = "Passthrough"
local MENU_ID = "cube_home_passthrough"

local HIDDEN_CLASSES = {
	infmap_terrain_collider = true,
	infmap_terrain_render = true,
	infmap_planet = true,
	infmap_clone = true,
	infmap_obj_collider = true,
}

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

--- Entities that form the AR content layer (drawn over the room).
function CubeHome.IsArContentEnt(ent)
	if not IsValid(ent) then return false end
	if ent.CubeHomeManaged or ent.CubeHomeAnchor then return true end
	if ent.CubeHomeInvisibleCollider then return false end
	if ent:IsPlayer() then return true end
	if ent:IsWeapon() then return true end
	local cls = ent:GetClass() or ""
	if HIDDEN_CLASSES[cls] then return false end
	if string.find(cls, "infmap", 1, true) then return false end
	if string.find(cls, "viewmodel", 1, true) then return true end
	if string.find(cls, "gmod_hands", 1, true) then return true end
	if string.find(cls, "physgun", 1, true) then return true end
	-- VRMod hand / avatar twins
	if ent.IsVRHand or ent.vrmod_hand then return true end
	local owner = ent:GetOwner()
	if IsValid(owner) and owner:IsPlayer() then return true end
	local parent = ent:GetParent()
	if IsValid(parent) and (parent:IsPlayer() or parent.CubeHomeManaged) then return true end
	return false
end

local function applyOpenXR(on)
	if not isfunction(VRMOD_SetEnvironmentBlendMode) then return false end
	if on then
		if isfunction(VRMOD_SetPassthroughChroma) then
			VRMOD_SetPassthroughChroma(true, math.Clamp(cvTol:GetFloat(), 0.01, 0.1))
		end
		-- Sticky product signal for submit path
		if g_VR then g_VR.cubeHomeArLayer = true end
		VRMOD_SetEnvironmentBlendMode(3)
	else
		if isfunction(VRMOD_SetPassthroughChroma) then
			VRMOD_SetPassthroughChroma(false, 0.04)
		end
		if g_VR then g_VR.cubeHomeArLayer = false end
		VRMOD_SetEnvironmentBlendMode(0)
	end
	return true
end

--- Kill map layer: world, sky, InfMap, everything not AR content.
local function applyArWorldLayer(on)
	if on then
		RunConsoleCommand("r_drawworld", "0")
		RunConsoleCommand("r_3dsky", "0")
		RunConsoleCommand("r_drawskybox", "0")
		RunConsoleCommand("r_drawdetailprops", "0")
		RunConsoleCommand("fog_override", "1")
		RunConsoleCommand("fog_start", "99999")
		RunConsoleCommand("fog_end", "99999")
		-- Suppress InfMap client mesh distance
		if InfMap then
			InfMap.render_distance = 0
			InfMap.planet_render_distance = 0
		end
	else
		RunConsoleCommand("r_drawworld", "1")
		RunConsoleCommand("r_3dsky", "1")
		RunConsoleCommand("r_drawskybox", "1")
		RunConsoleCommand("r_drawdetailprops", "1")
		RunConsoleCommand("fog_override", "0")
	end
end

local function sweepEntityVisibility(arOn)
	for _, ent in ipairs(ents.GetAll()) do
		if not IsValid(ent) then continue end
		if ent.CubeHomeInvisibleCollider then
			ent:SetNoDraw(true)
			continue
		end
		if arOn then
			local keep = CubeHome.IsArContentEnt(ent)
			if keep then
				if ent._cubeHomeSavedNoDraw == nil then
					ent._cubeHomeSavedNoDraw = ent:GetNoDraw()
				end
				ent:SetNoDraw(false)
			else
				if ent._cubeHomeSavedNoDraw == nil then
					ent._cubeHomeSavedNoDraw = ent:GetNoDraw()
				end
				ent:SetNoDraw(true)
			end
		else
			if ent._cubeHomeSavedNoDraw ~= nil then
				ent:SetNoDraw(ent._cubeHomeSavedNoDraw)
				ent._cubeHomeSavedNoDraw = nil
			end
		end
	end
	-- Nuke leftover InfMap client terrain ents
	if arOn and InfMap and InfMap.client_chunks then
		for _, row in pairs(InfMap.client_chunks) do
			if istable(row) then
				for _, e in pairs(row) do
					if IsValid(e) then SafeRemoveEntity(e) end
				end
			end
		end
		InfMap.client_chunks = {}
	end
end

function CubeHome.EnablePassthrough(reason)
	if not onHomeMap() then return end
	if not isOpenXR() then
		print("[cube_home] AR layer needs OpenXR")
		return
	end
	local ok = applyOpenXR(true)
	CubeHome.Passthrough.active = true
	applyArWorldLayer(true)
	sweepEntityVisibility(true)
	print(string.format("[cube_home] AR content layer ON reason=%s native=%s",
		tostring(reason or "?"), tostring(ok)))
end

function CubeHome.DisablePassthrough(reason)
	CubeHome.Passthrough.active = false
	applyOpenXR(false)
	if onHomeMap() then
		applyArWorldLayer(false)
		sweepEntityVisibility(false)
	end
	print("[cube_home] AR layer OFF reason=" .. tostring(reason or "?"))
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

-- Continuous enforcement while AR layer is live (ents spawn mid-session).
hook.Add("Think", "cube_home_ar_layer_think", function()
	if not CubeHome.Passthrough.active or not onHomeMap() then return end
	if (FrameNumber() % 15) ~= 0 then return end
	sweepEntityVisibility(true)
end)

hook.Add("NetworkEntityCreated", "cube_home_ar_layer_netent", function(ent)
	if not CubeHome.Passthrough.active or not onHomeMap() then return end
	timer.Simple(0, function()
		if not IsValid(ent) or not CubeHome.Passthrough.active then return end
		if not CubeHome.IsArContentEnt(ent) then
			ent:SetNoDraw(true)
		end
	end)
end)

hook.Add("SetupWorldFog", "cube_home_pt_fog", function()
	if not onHomeMap() or not CubeHome.Passthrough.active then return end
	render.FogMode(MATERIAL_FOG_NONE)
	return true
end)

-- Clear color before engine sky dirties the RT (alpha ignored by Source, depth is clear).
hook.Add("PreRender", "cube_home_ar_prerender", function()
	if not CubeHome.Passthrough.active or not onHomeMap() then return end
	if not (g_VR and g_VR.active) then return end
	-- Keep empty buffer at clear depth; no colored void plane.
end)

local function menuHint()
	if CubeHome.Passthrough.active then
		return "ON · AR layer · room under platforms"
	end
	return "OFF · full VR world"
end

function CubeHome.RemovePassthroughMenu()
	if not CubeHome.Passthrough.menu then return end
	if vrmod and vrmod.RemoveInGameMenuItem then
		vrmod.RemoveInGameMenuItem(MENU_NAME, nil, true)
	end
	if g_VR and istable(g_VR.menuItems) then
		for i = #g_VR.menuItems, 1, -1 do
			local it = g_VR.menuItems[i]
			if it and it.id == MENU_ID then table.remove(g_VR.menuItems, i) end
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
	if not isOpenXR() or not (g_VR and g_VR.active) then
		CubeHome.RemovePassthroughMenu()
		if CubeHome.Passthrough.active then
			applyOpenXR(false)
			CubeHome.Passthrough.active = false
			applyArWorldLayer(false)
			sweepEntityVisibility(false)
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
		if cvEnable:GetBool() then
			applyArWorldLayer(true)
			sweepEntityVisibility(true)
		end
	end)
end)

hook.Add("VRMod_Start", "cube_home_pt_vrstart", function()
	timer.Simple(0.5, function() syncFromCvar() end)
end)

hook.Add("VRMod_Exit", "cube_home_pt_vrexit", function()
	CubeHome.RemovePassthroughMenu()
	if isfunction(VRMOD_SetEnvironmentBlendMode) then VRMOD_SetEnvironmentBlendMode(0) end
	if isfunction(VRMOD_SetPassthroughChroma) then VRMOD_SetPassthroughChroma(false, 0.04) end
	if g_VR then g_VR.cubeHomeArLayer = false end
	CubeHome.Passthrough.active = false
	if onHomeMap() then
		applyArWorldLayer(false)
		sweepEntityVisibility(false)
	end
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
		print("[cube_home] need " .. tostring(CubeHome.MAP) .. " + OpenXR + VR")
		return
	end
	CubeHome.TogglePassthrough()
end)
