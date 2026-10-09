-- Studio reference mesh and the construct static-prop lump.
return function(T)
	local content = require("pure.content")
	local mdl = require("pure.mdl")
	local props = require("pure.props")
	local gmod = os.getenv("ENGINE_GMOD")
		or "/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod"
	local mount = content.mount({ gmod = gmod })
	local function grab(name)
		return mount:read(name .. ".mdl"), mount:read(name .. ".vvd"), mount:read(name .. ".dx90.vtx")
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

	-- row_res_1 stores a VVD fixup table. The VTX indexes the reordered array.
	local row, err = mdl.load(grab("models/props_buildings/row_res_1"))
	T.ok(row and row.tris > 100, "row house fixups " .. tostring(err or (row and row.tris)))
	if row and row.meshes[1] then
		local src = row.meshes[1].verts
		local worst = 0
		local ntri = math.floor(#src / 15)
		for t = 0, ntri - 1 do
			local function p(k)
				local o = (t * 3 + k) * 5
				return src[o + 1], src[o + 2], src[o + 3]
			end
			local ax, ay, az = p(0)
			local bx, by, bz = p(1)
			local cx, cy, cz = p(2)
			local function dist(x1, y1, z1, x2, y2, z2)
				local dx, dy, dz = x1 - x2, y1 - y2, z1 - z2
				return math.sqrt(dx * dx + dy * dy + dz * dz)
			end
			local e = dist(ax, ay, az, bx, by, bz)
			if dist(bx, by, bz, cx, cy, cz) > e then
				e = dist(bx, by, bz, cx, cy, cz)
			end
			if dist(cx, cy, cz, ax, ay, az) > e then
				e = dist(cx, cy, cz, ax, ay, az)
			end
			if e > worst then
				worst = e
			end
		end
		local hx = row.hull_max.x - row.hull_min.x
		local hy = row.hull_max.y - row.hull_min.y
		local hz = row.hull_max.z - row.hull_min.z
		local span = math.sqrt(hx * hx + hy * hy + hz * hz)
		T.ok(worst < span * 0.85, string.format("row house edges stay on the shell %.1f < %.1f", worst, span))
	end
end
