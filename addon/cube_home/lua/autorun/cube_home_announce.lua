-- Always-loaded: map identity + credit (even before InfMap map scripts).

CubeHome = CubeHome or {}
-- InfMap second token must be "infmap" → xr_infmap_passthrough (product: XR Home Passthrough).
CubeHome.MAP = CubeHome.MAP or "xr_infmap_passthrough"
CubeHome.TITLE = CubeHome.TITLE or "XR Home Passthrough"
CubeHome.INFMAP_WORKSHOP = CubeHome.INFMAP_WORKSHOP or "2905327911"
CubeHome.INFMAP_AUTHOR = CubeHome.INFMAP_AUTHOR or "Meetric"
CubeHome.INFMAP_REPO = CubeHome.INFMAP_REPO or "https://github.com/meetric1/gmod-infinite-map"
CubeHome.INFMAP_WORKSHOP_URL = CubeHome.INFMAP_WORKSHOP_URL
	or "https://steamcommunity.com/workshop/filedetails/?id=2905327911"
CubeHome.VOID_KEY = CubeHome.VOID_KEY or { r = 255, g = 0, b = 255 }
CubeHome.VOID_KEY_N = CubeHome.VOID_KEY_N or { r = 1, g = 0, b = 1 }

function CubeHome.IsHomeMap(map)
	map = string.lower(map or (game.GetMap and game.GetMap()) or "")
	return map == CubeHome.MAP
end

if SERVER then
	hook.Add("InitPostEntity", "cube_home_check_infmap", function()
		if not CubeHome.IsHomeMap() then return end
		if InfMap == nil then
			ErrorNoHalt("[cube_home] InfMap base missing — subscribe Workshop "
				.. CubeHome.INFMAP_WORKSHOP
				.. " by Meetric or extract it so terrain/entities load.\n"
				.. "  " .. tostring(CubeHome.INFMAP_REPO) .. "\n")
		end
	end)
end

if CLIENT then
	list.Set("DesktopWindows", "CubeHomeHelp", {
		title = "XR Home Passthrough",
		icon = "icon16/world.png",
		width = 380,
		height = 300,
		onewindow = true,
		init = function(icon, window)
			window:SetTitle("XR Home Passthrough")
			local l = vgui.Create("DLabel", window)
			l:Dock(FILL)
			l:DockMargin(12, 12, 12, 12)
			l:SetWrap(true)
			l:SetText(
				"Map: " .. tostring(CubeHome.MAP) .. "\n"
					.. "Product: " .. tostring(CubeHome.TITLE) .. "\n\n"
					.. "Passthrough (OpenXR only, this map only):\n"
					.. "  Quick menu → Passthrough ON/OFF\n"
					.. "  cube_home_passthrough 0/1\n"
					.. "  Invisible void: pure sky black → alpha (no green key)\n\n"
					.. "InfMap by Meetric (GPL-3.0)\n"
					.. "  github.com/meetric1/gmod-infinite-map\n"
					.. "  Workshop " .. tostring(CubeHome.INFMAP_WORKSHOP)
			)
		end,
	})
end
