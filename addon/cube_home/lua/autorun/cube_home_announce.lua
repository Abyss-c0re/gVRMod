-- Always-loaded: advertise home map identity to Cube / VRMod (even before InfMap map scripts).

CubeHome = CubeHome or {}
CubeHome.MAP = CubeHome.MAP or "gm_infmap_home"
CubeHome.INFMAP_WORKSHOP = CubeHome.INFMAP_WORKSHOP or "2905327911"
CubeHome.INFMAP_AUTHOR = CubeHome.INFMAP_AUTHOR or "Meetric"
CubeHome.INFMAP_REPO = CubeHome.INFMAP_REPO or "https://github.com/meetric1/gmod-infinite-map"
CubeHome.INFMAP_WORKSHOP_URL = CubeHome.INFMAP_WORKSHOP_URL
	or "https://steamcommunity.com/workshop/filedetails/?id=2905327911"

function CubeHome.IsHomeMap(map)
	map = string.lower(map or (game.GetMap and game.GetMap()) or "")
	return map == CubeHome.MAP
end

if SERVER then
	-- Help operators spot missing InfMap base.
	hook.Add("InitPostEntity", "cube_home_check_infmap", function()
		if not CubeHome.IsHomeMap() then return end
		if InfMap == nil then
			ErrorNoHalt("[cube_home] InfMap base missing — subscribe Workshop "
				.. CubeHome.INFMAP_WORKSHOP
				.. " by Meetric (Infinite Map Base) or extract it so terrain/entities load.\n"
				.. "  " .. tostring(CubeHome.INFMAP_REPO) .. "\n")
		end
	end)
end

if CLIENT then
	list.Set("DesktopWindows", "CubeHomeHelp", {
		title = "gVRMod Home",
		icon = "icon16/world.png",
		width = 360,
		height = 280,
		onewindow = true,
		init = function(icon, window)
			window:SetTitle("gVRMod Home")
			local l = vgui.Create("DLabel", window)
			l:Dock(FILL)
			l:DockMargin(12, 12, 12, 12)
			l:SetWrap(true)
			l:SetText(
				"Home map: gm_infmap_home\n\n"
					.. "Built on InfMap by Meetric (GPL-3.0)\n"
					.. "  github.com/meetric1/gmod-infinite-map\n"
					.. "  Workshop " .. tostring(CubeHome.INFMAP_WORKSHOP) .. "\n\n"
					.. "Improve live:\n"
					.. "  cube_home_set_spawn\n"
					.. "  cube_home_add_prop [model]\n"
					.. "  cube_home_reload / cube_home_save / cube_home_reset\n"
					.. "  cube_home_goto plaza|sandbox|range|build\n\n"
					.. "Layout: garrysmod/data/cube_home/layout.json"
			)
		end,
	})
end
