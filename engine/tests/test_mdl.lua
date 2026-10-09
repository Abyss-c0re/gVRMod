-- Studio reference mesh and the construct static-prop lump.
return function(T)
	local vpk = require("pure.vpk")
	local mdl = require("pure.mdl")
	local props = require("pure.props")
	local gmod = os.getenv("ENGINE_GMOD")
		or "/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod"
	local pack = vpk.open(gmod .. "/../sourceengine/hl2_misc_dir.vpk")
	local function grab(name)
		return pack:read(name .. ".mdl"), pack:read(name .. ".vvd"), pack:read(name .. ".dx90.vtx")
	end
	local base = "models/props_buildings/short_building001a"
	local mesh = mdl.load(grab(base))
	T.ok(mesh and mesh.tris == 380, "short building tris " .. tostring(mesh and mesh.tris))
	if mesh then
		T.near(mesh.hull_min.x, -75.436, 0.01, "hull min x")
		T.ok(mesh.meshes[1] and mesh.meshes[1].material:find("short_building001a", 1, true),
			"building material " .. tostring(mesh.meshes[1] and mesh.meshes[1].material))
		local n = #mesh.meshes[1].verts / 5
		T.eq(n, 380 * 3, "building corners")
	end
	local list = props.read(gmod .. "/maps/gm_construct.bsp")
	T.eq(#list, 182, "construct static props")
	local buildings = 0
	for i = 1, #list do
		if list[i].model:find("short_building001a", 1, true) then
			buildings = buildings + 1
			T.ok(list[i].z > 10000, "sky building height")
		end
	end
	T.eq(buildings, 3, "short building instances")
end
