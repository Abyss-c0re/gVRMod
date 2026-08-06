-- XR Home Passthrough — Source error CHECKER void + dual chroma.
--
-- Void is painted as the classic missing-texture mosaic:
--   bright #FF00DC (255,0,220)  +  dark #010001 (1,0,1)
-- Module dual-keys both colors → alpha 0 (room). Keys also work separately:
--   mask 1 = pink only, 2 = black only, 7 = both + black-needs-pink-near (checker).

if not CLIENT then return end

CubeHome = CubeHome or {}
CubeHome.Passthrough = CubeHome.Passthrough or { active = false, menu = false }

CubeHome.VOID_KEY = { r = 255, g = 0, b = 220 }
CubeHome.VOID_KEY_N = { r = 1, g = 0, b = 220 / 255 }
CubeHome.VOID_KEY2 = { r = 1, g = 0, b = 1 }
CubeHome.VOID_KEY2_N = { r = 1 / 255, g = 0, b = 1 / 255 }

local cvEnable = CreateClientConVar("cube_home_passthrough", "1", true, FCVAR_ARCHIVE,
	"XR Home: error-checker void → room passthrough")
local cvTol = CreateClientConVar("cube_home_passthrough_tol", "0.18", true, FCVAR_ARCHIVE,
	"Pink key distance")
local cvTol2 = CreateClientConVar("cube_home_passthrough_tol2", "0.08", true, FCVAR_ARCHIVE,
	"Black-cell key distance")
-- 1=pink 2=black 3=both indep 7=checker (both + black needs pink near)
local cvMask = CreateClientConVar("cube_home_passthrough_mask", "7", true, FCVAR_ARCHIVE,
	"Chroma mask: 1=pink 2=black 3=both 7=checker dual")

local MENU_NAME = "Passthrough"
local MENU_ID = "cube_home_passthrough"

local CHECKER_RT_NAME = "cube_home_error_checker_rt"
local CHECKER_MAT_NAME = "cube_home_error_checker_mat"
local checkerReady = false
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

--- Build Source-style error mosaic once (pink / near-black checker).
function CubeHome.EnsureErrorCheckerMaterial()
	if checkerReady and voidMat and not voidMat:IsError() then return voidMat end

	local sz = 64
	local cell = 8
	local rt = GetRenderTargetEx(
		CHECKER_RT_NAME, sz, sz,
		RT_SIZE_NO_CHANGE, MATERIAL_RT_DEPTH_NONE,
		bit.bor(1, 256), -- clamp + anisotropic off
		0, IMAGE_FORMAT_RGBA8888
	)
	if not rt then
		voidMat = Material("cube_home/pt_void")
		return voidMat
	end

	render.PushRenderTarget(rt)
	render.Clear(0, 0, 0, 255, true, true)
	cam.Start2D()
	for cy = 0, (sz / cell) - 1 do
		for cx = 0, (sz / cell) - 1 do
			if (cx + cy) % 2 == 0 then
				surface.SetDrawColor(255, 0, 220, 255) -- #FF00DC
			else
				surface.SetDrawColor(1, 0, 1, 255) -- #010001
			end
			surface.DrawRect(cx * cell, cy * cell, cell, cell)
		end
	end
	cam.End2D()
	render.PopRenderTarget()

	voidMat = CreateMaterial(CHECKER_MAT_NAME, "UnlitGeneric", {
		["$basetexture"] = rt:GetName(),
		["$nofog"] = "1",
		["$ignorez"] = "0",
		["$nocull"] = "1",
		["$vertexcolor"] = "0",
		["$vertexalpha"] = "0",
		["$model"] = "1",
		["$translucent"] = "0",
	})
	if voidMat and not voidMat:IsError() then
		voidMat:SetTexture("$basetexture", rt)
		-- Tile so world-scale void shows many cells
		voidMat:SetFloat("$detailscale", 1)
	end
	checkerReady = true
	return voidMat
end

local function applyOpenXR(on)
	if not isfunction(VRMOD_SetEnvironmentBlendMode) then return false end
	local kn = CubeHome.VOID_KEY_N
	local kn2 = CubeHome.VOID_KEY2_N
	if on then
		if isfunction(VRMOD_SetPassthroughChromaKey) then
			VRMOD_SetPassthroughChromaKey(kn.r, kn.g, kn.b)
		end
		if isfunction(VRMOD_SetPassthroughChromaKey2) then
			VRMOD_SetPassthroughChromaKey2(kn2.r, kn2.g, kn2.b)
		end
		if isfunction(VRMOD_SetPassthroughChromaTol2) then
			VRMOD_SetPassthroughChromaTol2(math.Clamp(cvTol2:GetFloat(), 0.04, 0.2))
		end
		if isfunction(VRMOD_SetPassthroughChromaMask) then
			VRMOD_SetPassthroughChromaMask(math.floor(cvMask:GetInt()))
		end
		if isfunction(VRMOD_SetPassthroughChroma) then
			VRMOD_SetPassthroughChroma(true, math.Clamp(cvTol:GetFloat(), 0.08, 0.35))
		end
		VRMOD_SetEnvironmentBlendMode(3)
	else
		if isfunction(VRMOD_SetPassthroughChroma) then
			VRMOD_SetPassthroughChroma(false, 0.18)
		end
		VRMOD_SetEnvironmentBlendMode(0)
	end
	return true
end

local function applyWorld(on)
	if on then
		RunConsoleCommand("r_drawworld", "0")
		RunConsoleCommand("r_3dsky", "0")
		RunConsoleCommand("r_drawskybox", "0")
		RunConsoleCommand("fog_override", "1")
		RunConsoleCommand("fog_color", "255", "0", "220")
		RunConsoleCommand("fog_start", "99999")
		RunConsoleCommand("fog_end", "99999")
		if InfMap then
			InfMap.render_distance = 0
			InfMap.planet_render_distance = 0
		end
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
		print("[cube_home] passthrough needs OpenXR")
		return
	end
	CubeHome.EnsureErrorCheckerMaterial()
	local ok = applyOpenXR(true)
	CubeHome.Passthrough.active = true
	applyWorld(true)
	print(string.format("[cube_home] passthrough ON (error CHECKER void, mask=%d) reason=%s ok=%s",
		cvMask:GetInt(), tostring(reason or "?"), tostring(ok)))
end

function CubeHome.DisablePassthrough(reason)
	CubeHome.Passthrough.active = false
	applyOpenXR(false)
	if onHomeMap() then applyWorld(false) end
	print("[cube_home] passthrough OFF reason=" .. tostring(reason or "?"))
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

-- Checker void plane (tiled UVs so mosaic is visible / keyable).
local size = 1e9
local minZ = -1e5
local uvScale = 50000 -- many cells across the void
local voidMesh
local function ensureVoidMesh()
	if voidMesh then return voidMesh end
	voidMesh = Mesh()
	local u = uvScale
	voidMesh:BuildFromTriangles({
		{ pos = Vector(size, size, minZ), normal = Vector(0, 0, 1), u = u, v = 0 },
		{ pos = Vector(size, -size, minZ), normal = Vector(0, 0, 1), u = u, v = u },
		{ pos = Vector(-size, -size, minZ), normal = Vector(0, 0, 1), u = 0, v = u },
		{ pos = Vector(size, size, minZ), normal = Vector(0, 0, 1), u = u, v = 0 },
		{ pos = Vector(-size, -size, minZ), normal = Vector(0, 0, 1), u = 0, v = u },
		{ pos = Vector(-size, size, minZ), normal = Vector(0, 0, 1), u = 0, v = 0 },
	})
	return voidMesh
end

hook.Add("PostDraw2DSkyBox", "cube_home_error_checker_void", function()
	if not onHomeMap() or not CubeHome.Passthrough.active then return end
	local mat = CubeHome.EnsureErrorCheckerMaterial()
	if not mat or mat:IsError() then return end
	local mesh = ensureVoidMesh()
	render.OverrideDepthEnable(true, false)
	render.SetMaterial(mat)
	render.ResetModelLighting(1, 1, 1)
	render.SetLocalModelLights()
	render.SetColorModulation(1, 1, 1)
	mesh:Draw()
	render.OverrideDepthEnable(false, false)
end)

hook.Add("SetupWorldFog", "cube_home_pt_fog", function()
	if not onHomeMap() or not CubeHome.Passthrough.active then return end
	render.FogMode(MATERIAL_FOG_NONE)
	return true
end)

hook.Add("PreDrawOpaqueRenderables", "cube_home_pt_hide_infmap_mesh", function()
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
		return "ON · error checker void → room"
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
	if not isOpenXR() or not (g_VR and g_VR.active) then
		CubeHome.RemovePassthroughMenu()
		if CubeHome.Passthrough.active then
			applyOpenXR(false)
			CubeHome.Passthrough.active = false
			applyWorld(false)
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
	timer.Simple(0.5, function() CubeHome.EnsureErrorCheckerMaterial() end)
	timer.Simple(1, function()
		if cvEnable:GetBool() then applyWorld(true) end
	end)
end)

hook.Add("VRMod_Start", "cube_home_pt_vrstart", function()
	timer.Simple(0.5, function() syncFromCvar() end)
end)

hook.Add("VRMod_Exit", "cube_home_pt_vrexit", function()
	CubeHome.RemovePassthroughMenu()
	if isfunction(VRMOD_SetEnvironmentBlendMode) then VRMOD_SetEnvironmentBlendMode(0) end
	if isfunction(VRMOD_SetPassthroughChroma) then VRMOD_SetPassthroughChroma(false, 0.18) end
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
		VRMOD_SetPassthroughChroma(true, math.Clamp(tonumber(new) or 0.18, 0.08, 0.35))
	end
end, "cube_home_pt_tol")

cvars.AddChangeCallback("cube_home_passthrough_tol2", function(_, _, new)
	if CubeHome.Passthrough.active and isfunction(VRMOD_SetPassthroughChromaTol2) then
		VRMOD_SetPassthroughChromaTol2(math.Clamp(tonumber(new) or 0.08, 0.04, 0.2))
	end
end, "cube_home_pt_tol2")

cvars.AddChangeCallback("cube_home_passthrough_mask", function(_, _, new)
	if CubeHome.Passthrough.active and isfunction(VRMOD_SetPassthroughChromaMask) then
		VRMOD_SetPassthroughChromaMask(math.floor(tonumber(new) or 7))
	end
end, "cube_home_pt_mask")

concommand.Add("cube_home_passthrough_toggle", function()
	if not CubeHome.CanUsePassthrough() then
		print("[cube_home] need " .. tostring(CubeHome.MAP) .. " + OpenXR + VR")
		return
	end
	CubeHome.TogglePassthrough()
end)

-- Quick helpers: key either color alone (still dual-capable when mask=7)
concommand.Add("cube_home_chroma_pink_only", function()
	RunConsoleCommand("cube_home_passthrough_mask", "1")
	print("[cube_home] chroma mask = pink only")
end)
concommand.Add("cube_home_chroma_black_only", function()
	RunConsoleCommand("cube_home_passthrough_mask", "2")
	print("[cube_home] chroma mask = black only (independent)")
end)
concommand.Add("cube_home_chroma_checker", function()
	RunConsoleCommand("cube_home_passthrough_mask", "7")
	print("[cube_home] chroma mask = full error checker")
end)
