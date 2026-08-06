-- XR Home Passthrough — stable error-CHECKER void + dual chroma (no flicker).
--
-- Flicker causes we avoid:
--   * black clear fighting a colored plane
--   * LINEAR blit blurring pink/black (module uses NEAREST when PT on)
--   * regenerating the checker RT every frame
--
-- Void = Source error mosaic (#FF00DC / #010001). Module keys both → alpha 0.
-- FB passthrough layer (if runtime supports) puts the real room under holes.

if not CLIENT then return end

CubeHome = CubeHome or {}
CubeHome.Passthrough = CubeHome.Passthrough or { active = false, menu = false }

CubeHome.VOID_KEY = { r = 255, g = 0, b = 220 }
CubeHome.VOID_KEY_N = { r = 1, g = 0, b = 220 / 255 }
CubeHome.VOID_KEY2 = { r = 1, g = 0, b = 1 }
CubeHome.VOID_KEY2_N = { r = 1 / 255, g = 0, b = 1 / 255 }

local cvEnable = CreateClientConVar("cube_home_passthrough", "1", true, FCVAR_ARCHIVE,
	"XR Home: error-checker void → room passthrough")
local cvTol = CreateClientConVar("cube_home_passthrough_tol", "0.22", true, FCVAR_ARCHIVE,
	"Pink key distance (wider = less pink fringe)")
local cvTol2 = CreateClientConVar("cube_home_passthrough_tol2", "0.12", true, FCVAR_ARCHIVE,
	"Black-cell key distance")
-- 7 = full checker (pink + black-with-pink-near)
local cvMask = CreateClientConVar("cube_home_passthrough_mask", "7", true, FCVAR_ARCHIVE,
	"1=pink 2=black 3=both indep 7=checker")

local MENU_NAME = "Passthrough"
local MENU_ID = "cube_home_passthrough"
local CHECKER_RT_NAME = "cube_home_error_checker_rt_v2"
local CHECKER_MAT_NAME = "cube_home_error_checker_mat_v2"
local checkerBuilt = false
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

--- One-shot Source error mosaic (large cells so NEAREST blit keeps pure keys).
function CubeHome.EnsureErrorCheckerMaterial()
	if checkerBuilt and voidMat and not voidMat:IsError() then return voidMat end

	local sz, cell = 128, 32 -- large cells → survive downsample without pink/black mud
	local rt = GetRenderTarget(CHECKER_RT_NAME, sz, sz)
	if not rt then
		voidMat = Material("cube_home/pt_void")
		checkerBuilt = true
		return voidMat
	end

	render.PushRenderTarget(rt)
	render.Clear(255, 0, 220, 255, true, true)
	cam.Start2D()
	for cy = 0, (sz / cell) - 1 do
		for cx = 0, (sz / cell) - 1 do
			if (cx + cy) % 2 == 0 then
				surface.SetDrawColor(255, 0, 220, 255)
			else
				surface.SetDrawColor(1, 0, 1, 255)
			end
			surface.DrawRect(cx * cell, cy * cell, cell, cell)
		end
	end
	cam.End2D()
	render.PopRenderTarget()

	voidMat = CreateMaterial(CHECKER_MAT_NAME, "UnlitGeneric", {
		["$basetexture"] = rt:GetName(),
		["$nofog"] = "1",
		["$nocull"] = "1",
		["$ignorez"] = "0",
		["$model"] = "1",
		["$vertexcolor"] = "0",
		["$vertexalpha"] = "0",
	})
	if voidMat and not voidMat:IsError() then
		voidMat:SetTexture("$basetexture", rt)
	end
	checkerBuilt = true
	return voidMat
end

local function applyOpenXR(on)
	if not isfunction(VRMOD_SetEnvironmentBlendMode) then return false end
	local kn, kn2 = CubeHome.VOID_KEY_N, CubeHome.VOID_KEY2_N
	if on then
		if isfunction(VRMOD_SetPassthroughChromaKey) then
			VRMOD_SetPassthroughChromaKey(kn.r, kn.g, kn.b)
		end
		if isfunction(VRMOD_SetPassthroughChromaKey2) then
			VRMOD_SetPassthroughChromaKey2(kn2.r, kn2.g, kn2.b)
		end
		if isfunction(VRMOD_SetPassthroughChromaTol2) then
			VRMOD_SetPassthroughChromaTol2(math.Clamp(cvTol2:GetFloat(), 0.06, 0.2))
		end
		if isfunction(VRMOD_SetPassthroughChromaMask) then
			VRMOD_SetPassthroughChromaMask(math.floor(cvMask:GetInt()))
		end
		if isfunction(VRMOD_SetPassthroughChroma) then
			VRMOD_SetPassthroughChroma(true, math.Clamp(cvTol:GetFloat(), 0.12, 0.4))
		end
		-- Force alpha blend path (module also forces this when chroma on)
		if isfunction(VRMOD_SetEnvironmentBlendMode) then
			VRMOD_SetEnvironmentBlendMode(1) -- ALPHA_BLEND explicit
		end
	else
		if isfunction(VRMOD_SetPassthroughChroma) then
			VRMOD_SetPassthroughChroma(false, 0.22)
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
		-- Clear/fog match bright key so we never alternate black↔pink (flicker root cause)
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
	print(string.format("[cube_home] PT ON checker void mask=%d reason=%s ok=%s",
		cvMask:GetInt(), tostring(reason or "?"), tostring(ok)))
end

function CubeHome.DisablePassthrough(reason)
	CubeHome.Passthrough.active = false
	applyOpenXR(false)
	if onHomeMap() then applyWorld(false) end
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

-- Stable checker void plane (built once).
local size = 1e9
local minZ = -50000
local uvScale = 200 -- fewer, larger tiles in world space
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

-- Also fill 3D sky/world clear with bright key so no black frames (flicker fix)
hook.Add("PreDrawSkyBox", "cube_home_pt_pink_clear", function()
	if not onHomeMap() or not CubeHome.Passthrough.active then return end
	render.Clear(255, 0, 220, 255, true, true)
	return true -- skip default sky (stable pink clear)
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
		return "ON · error checker → room"
	end
	return "OFF · full VR"
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
	-- After session exists so FB passthrough can attach
	timer.Simple(1.0, function() syncFromCvar() end)
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
		VRMOD_SetPassthroughChroma(true, math.Clamp(tonumber(new) or 0.22, 0.12, 0.4))
	end
end, "cube_home_pt_tol")

cvars.AddChangeCallback("cube_home_passthrough_tol2", function(_, _, new)
	if CubeHome.Passthrough.active and isfunction(VRMOD_SetPassthroughChromaTol2) then
		VRMOD_SetPassthroughChromaTol2(math.Clamp(tonumber(new) or 0.12, 0.06, 0.2))
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

concommand.Add("cube_home_chroma_pink_only", function()
	RunConsoleCommand("cube_home_passthrough_mask", "1")
end)
concommand.Add("cube_home_chroma_black_only", function()
	RunConsoleCommand("cube_home_passthrough_mask", "2")
end)
concommand.Add("cube_home_chroma_checker", function()
	RunConsoleCommand("cube_home_passthrough_mask", "7")
end)
