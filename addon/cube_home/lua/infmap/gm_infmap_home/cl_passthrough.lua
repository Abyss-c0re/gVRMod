-- gVRMod Home → passthrough / AR void map.
-- Black void + platforms only; OpenXR ALPHA_BLEND + dark chroma punches the room through.

if not CLIENT then return end

CubeHome = CubeHome or {}
CubeHome.Passthrough = CubeHome.Passthrough or {
	active = false,
	applied = false,
}

local cvEnable = CreateClientConVar("cube_home_passthrough", "1", true, false,
	"Home map: AR passthrough void (hide world, alpha-blend + chroma black)")
local cvKey = CreateClientConVar("cube_home_passthrough_key", "0.12", true, false,
	"Chroma threshold 0..1 — higher punches more sky/floor to transparent")
local cvWorld = CreateClientConVar("cube_home_drawworld", "0", true, false,
	"0 = hide brush world (recommended for passthrough); 1 = show Source world")

local function want()
	return CubeHome.IsHomeMap and CubeHome.IsHomeMap() and cvEnable:GetBool()
end

local function applyOpenXR()
	if not isfunction(VRMOD_SetEnvironmentBlendMode) then return false end
	-- 3 = AUTO (prefer ALPHA_BLEND)
	local mode = VRMOD_SetEnvironmentBlendMode(3)
	if isfunction(VRMOD_SetPassthroughChroma) then
		VRMOD_SetPassthroughChroma(true, math.Clamp(cvKey:GetFloat(), 0.02, 0.5))
	end
	return true, mode
end

local function clearOpenXR()
	if isfunction(VRMOD_SetEnvironmentBlendMode) then
		VRMOD_SetEnvironmentBlendMode(0)
	end
	if isfunction(VRMOD_SetPassthroughChroma) then
		VRMOD_SetPassthroughChroma(false, 0.1)
	end
end

function CubeHome.EnablePassthrough(reason)
	if not want() then return end
	local ok, mode = applyOpenXR()
	CubeHome.Passthrough.active = true
	CubeHome.Passthrough.applied = ok and true or false

	-- Void: no brush world, pure black clear so chroma keys to room.
	RunConsoleCommand("r_drawworld", cvWorld:GetBool() and "1" or "0")
	RunConsoleCommand("r_3dsky", "0")
	RunConsoleCommand("r_drawskybox", "0")
	RunConsoleCommand("fog_override", "1")
	RunConsoleCommand("fog_color", "0", "0", "0")
	RunConsoleCommand("fog_start", "99999")
	RunConsoleCommand("fog_end", "99999")

	if IsValid(LocalPlayer()) then
		local msg = ok
			and ("AR void ON (blend=" .. tostring(mode) .. ") · platforms only · chroma black→room")
			or "AR void ON (module may need rebuild for blend) · void look still applied"
		chat.AddText(Color(90, 200, 255), "[gVRMod Home] ", color_white, msg)
	end
	print("[cube_home] passthrough enable reason=" .. tostring(reason or "?") .. " ok=" .. tostring(ok))
end

function CubeHome.DisablePassthrough(reason)
	if not CubeHome.Passthrough.active then return end
	CubeHome.Passthrough.active = false
	clearOpenXR()
	RunConsoleCommand("r_drawworld", "1")
	RunConsoleCommand("r_3dsky", "1")
	RunConsoleCommand("r_drawskybox", "1")
	RunConsoleCommand("fog_override", "0")
	print("[cube_home] passthrough disable reason=" .. tostring(reason or "?"))
end

-- Sky / clear: pure black (chroma key).
hook.Add("SetupWorldFog", "cube_home_pt_fog", function()
	if not want() or not CubeHome.Passthrough.active then return end
	render.FogMode(MATERIAL_FOG_NONE)
	return true
end)

hook.Add("SetupSkyboxFog", "cube_home_pt_skyfog", function()
	if not want() or not CubeHome.Passthrough.active then return end
	render.FogMode(MATERIAL_FOG_NONE)
	return true
end)

hook.Add("PostDraw2DSkyBox", "cube_home_pt_kill_sky", function()
	if not want() or not CubeHome.Passthrough.active then return end
	-- Suppress InfMap big-plane sky if it ran; we leave black.
end)

-- Don't draw InfMap terrain meshes in passthrough (void only).
hook.Add("PreDrawOpaqueRenderables", "cube_home_pt_hide_terrain", function()
	if not want() or not CubeHome.Passthrough.active then return end
	if InfMap and InfMap.client_chunks then
		for y, row in pairs(InfMap.client_chunks) do
			if istable(row) then
				for x, e in pairs(row) do
					if IsValid(e) then e:SetNoDraw(true) end
				end
			end
		end
	end
end)

hook.Add("InitPostEntity", "cube_home_pt_boot", function()
	if not want() then return end
	timer.Simple(1, function() CubeHome.EnablePassthrough("InitPostEntity") end)
end)

-- Re-apply when VR starts (module may not have been ready at map load).
hook.Add("VRMod_Start", "cube_home_pt_vrstart", function()
	if want() then
		timer.Simple(0.5, function() CubeHome.EnablePassthrough("VRMod_Start") end)
	end
end)

hook.Add("VRMod_Exit", "cube_home_pt_vrexit", function()
	-- Keep void look on desktop too; only clear XR blend.
	if isfunction(VRMOD_SetEnvironmentBlendMode) then
		VRMOD_SetEnvironmentBlendMode(0)
	end
	if isfunction(VRMOD_SetPassthroughChroma) then
		VRMOD_SetPassthroughChroma(false, 0.1)
	end
end)

cvars.AddChangeCallback("cube_home_passthrough", function(_, _, new)
	if not CubeHome.IsHomeMap or not CubeHome.IsHomeMap() then return end
	if tobool(new) then
		CubeHome.EnablePassthrough("cvar")
	else
		CubeHome.DisablePassthrough("cvar")
		-- Restore flat-ish look without AR
		RunConsoleCommand("r_drawworld", "1")
	end
end, "cube_home_pt")

cvars.AddChangeCallback("cube_home_passthrough_key", function(_, _, new)
	if CubeHome.Passthrough.active and isfunction(VRMOD_SetPassthroughChroma) then
		VRMOD_SetPassthroughChroma(true, math.Clamp(tonumber(new) or 0.12, 0.02, 0.5))
	end
end, "cube_home_pt_key")

concommand.Add("cube_home_passthrough_toggle", function()
	if not CubeHome.IsHomeMap or not CubeHome.IsHomeMap() then
		print("[cube_home] not on home map")
		return
	end
	RunConsoleCommand("cube_home_passthrough", cvEnable:GetBool() and "0" or "1")
end)
