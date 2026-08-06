-- Client hints for gVRMod Home map.

if not CLIENT then return end

CubeHome = CubeHome or {}

hook.Add("InitPostEntity", "cube_home_welcome", function()
	if not CubeHome.IsHomeMap or not CubeHome.IsHomeMap() then return end
	timer.Simple(2, function()
		chat.AddText(Color(90, 160, 255), "[gVRMod Home] ", color_white,
			"AR void hub (passthrough). Platforms only — room shows through black.")
		chat.AddText(Color(180, 200, 220), "  ", color_white,
			"cube_home_passthrough 0/1 · cube_home_passthrough_key · cube_home_add_prop · cube_home_goto")
		chat.AddText(Color(140, 160, 180), "  InfMap by Meetric: ", Color(120, 180, 255),
			"https://github.com/meetric1/gmod-infinite-map")
	end)
end)

-- Simple world labels for zones (screen-space when close).
hook.Add("HUDPaint", "cube_home_zone_labels", function()
	if not CubeHome.IsHomeMap or not CubeHome.IsHomeMap() then return end
	local layout = CubeHome.Layout
	if not istable(layout) or not istable(layout.zones) then return end
	local ply = LocalPlayer()
	if not IsValid(ply) then return end
	local eye = ply:EyePos()
	for _, z in ipairs(layout.zones) do
		if not z.label then continue end
		local pos = CubeHome.Vec and CubeHome.Vec(z.pos, Vector()) or Vector(0, 0, 0)
		pos = pos + Vector(0, 0, 64)
		local dist = eye:Distance(pos)
		if dist > 2500 or dist < 32 then continue end
		local scr = pos:ToScreen()
		if not scr.visible then continue end
		local a = math.Clamp(255 - (dist / 2500) * 200, 40, 220)
		draw.SimpleTextOutlined(z.label, "DermaDefaultBold", scr.x, scr.y,
			Color(220, 230, 255, a), TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER, 1, Color(0, 0, 0, a))
	end
end)

-- Keep client layout mirror in sync when server rebuilds (singleplayer friendly).
hook.Add("InitPostEntity", "cube_home_client_layout", function()
	if not CubeHome.IsHomeMap or not CubeHome.IsHomeMap() then return end
	if CubeHome.LoadLayoutFromDisk then
		local layout = CubeHome.LoadLayoutFromDisk()
		CubeHome.Layout = layout
	end
end)
