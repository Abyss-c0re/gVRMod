-- VPK, VTF, and map UV proofs. Real GarrysMod bytes are read only when the install is present.
return function(T)
	local vpk = require("pure.vpk")
	local vtf = require("pure.vtf")
	local vmt = require("pure.vmt")
	local zipread = require("pure.zipread")
	local bsp = require("pure.bsp")
	local content = require("pure.content")

	local function le16(n)
		n = math.floor(n)
		return string.char(n % 256, math.floor(n / 256) % 256)
	end
	local function le32(n)
		n = math.floor(n)
		local s = ""
		for _ = 1, 4 do
			s = s .. string.char(n % 256)
			n = math.floor(n / 256)
		end
		return s
	end

	local back = zipread.inflate_raw(zipread.deflate_raw("engine-zip"), 10)
	T.eq(back, "engine-zip", "raw deflate roundtrip")

	local payload = "hello-pak"
	local name = "materials/unit.vmt"
	local local_hdr = "PK\003\004" .. le16(20) .. le16(0) .. le16(0) .. le16(0) .. le16(0)
		.. le32(0) .. le32(#payload) .. le32(#payload) .. le16(#name) .. le16(0)
		.. name .. payload
	local cd = "PK\001\002" .. le16(20) .. le16(20) .. le16(0) .. le16(0) .. le16(0) .. le16(0)
		.. le32(0) .. le32(#payload) .. le32(#payload) .. le16(#name) .. le16(0) .. le16(0)
		.. le16(0) .. le16(0) .. le32(0) .. le32(#local_hdr - #name - #payload)
		.. name
	-- local offset is 0. The field above used a wrong expression. Rebuild cd with offset 0.
	cd = "PK\001\002" .. le16(20) .. le16(20) .. le16(0) .. le16(0) .. le16(0) .. le16(0)
		.. le32(0) .. le32(#payload) .. le32(#payload) .. le16(#name) .. le16(0) .. le16(0)
		.. le16(0) .. le16(0) .. le32(0) .. le32(0)
		.. name
	local eocd = "PK\005\006" .. le16(0) .. le16(0) .. le16(1) .. le16(1)
		.. le32(#cd) .. le32(#local_hdr) .. le16(0)
	local zip_path = os.tmpname()
	local zf = assert(io.open(zip_path, "wb"))
	zf:write(local_hdr, cd, eocd)
	zf:close()
	local zr = zipread.open(zip_path, 0, #local_hdr + #cd + #eocd)
	T.eq(zr:read("materials/unit.vmt"), payload, "stored zip")
	os.remove(zip_path)

	local body = "basetexture-body"
	local tree = "vmt\0materials/demo\0one\0" .. le32(0) .. le16(0) .. le16(0x7fff)
		.. le32(0) .. le32(#body) .. le16(0xffff)
		.. "\0\0\0"
	local header = le32(0x55aa1234) .. le32(2) .. le32(#tree) .. le32(#body) .. le32(0) .. le32(0) .. le32(0)
	local vpk_path = os.tmpname()
	local vf = assert(io.open(vpk_path, "wb"))
	vf:write(header, tree, body)
	vf:close()
	local pack = vpk.open(vpk_path)
	T.eq(pack:read("materials/demo/one.vmt"), body, "embedded vpk file")
	T.ok(pack:read("materials/missing.vmt") == nil, "missing vpk key")
	os.remove(vpk_path)

	local function vtf_dxt1(block, fmt)
		local hdr = "VTF\0" .. le32(7) .. le32(1) .. le32(64)
			.. le16(4) .. le16(4) .. le32(0) .. le16(1) .. le16(0)
			.. string.rep("\0", 20) .. le32(0) .. le32(fmt)
			.. string.char(1) .. le32(0) .. string.char(0, 0, 0)
		return hdr .. block
	end
	local red = string.char(0x00, 0xF8, 0x1F, 0x00, 0, 0, 0, 0)
	local w, h, rgba = vtf.decode(vtf_dxt1(red, 13), 4)
	T.eq(w, 4, "dxt1 w")
	T.eq(h, 4, "dxt1 h")
	T.eq(rgba:byte(1), 255, "dxt1 r")
	T.eq(rgba:byte(2), 0, "dxt1 g")
	T.eq(rgba:byte(3), 0, "dxt1 b")
	T.eq(rgba:byte(4), 255, "dxt1 a")

	local dxt5 = string.char(255, 0, 0, 0, 0, 0, 0, 0) .. red
	w, h, rgba = vtf.decode(vtf_dxt1(dxt5, 15), 4)
	T.eq(w, 4, "dxt5 w")
	T.eq(rgba:byte(1), 255, "dxt5 r")
	T.eq(rgba:byte(4), 255, "dxt5 a")

	local xf = vmt.transform("center .5 .5 scale 2 2 rotate 0 translate 0 0")
	local u, v = vmt.apply_uv(0, 0, xf)
	T.near(u, -0.5, 1e-6, "uv scale u")
	T.near(v, -0.5, 1e-6, "uv scale v")
	u, v = vmt.apply_uv(0.5, 0.5, xf)
	T.near(u, 0.5, 1e-6, "uv center")
	T.ok(vmt.transform("center .5 .5 scale 1 1 rotate 0 translate 0 0") == nil, "identity transform")

	u, v = bsp.tex_uv(64, 32, 9, { 1, 0, 0, 0, 0, 1, 0, 0 }, 128, 128)
	T.near(u, 0.5, 1e-6, "tex u")
	T.near(v, 0.25, 1e-6, "tex v")

	local gmod = os.getenv("ENGINE_GMOD")
		or "/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod"
	local construct = gmod .. "/maps/gm_construct.bsp"
	local cf = io.open(construct, "rb")
	if not cf then
		T.ok(false, "missing gm_construct")
		return
	end
	cf:close()

	local dir = gmod .. "/garrysmod_dir.vpk"
	local real = vpk.open(dir)
	local bytes = real:read("materials/gm_construct/construct_concrete_ground.vtf")
	T.ok(bytes and #bytes > 1000, "concrete vtf bytes")
	w, h, rgba = vtf.decode(bytes, 32)
	T.eq(w, 32, "concrete mip w")
	T.eq(h, 32, "concrete mip h")
	T.eq(#rgba, 32 * 32 * 4, "concrete rgba")
	local n = 32 * 32
	local sr, var = 0, 0
	for i = 0, n - 1 do
		sr = sr + rgba:byte(i * 4 + 1)
	end
	local mean = sr / n
	for i = 0, n - 1 do
		local d = rgba:byte(i * 4 + 1) - mean
		var = var + d * d
	end
	var = var / n
	T.ok(var > 1, "concrete mip is not flat (" .. tostring(var) .. ")")
	T.ok(mean > 40 and mean < 230, "concrete mip has albedo (" .. tostring(mean) .. ")")

	local world = bsp.load(construct, { uv = true })
	T.ok(world.surfaces and #world.surfaces > 5, "construct surfaces " .. tostring(world.surfaces and #world.surfaces))
	T.ok(world.tri_count > 20000 and world.tri_count < 80000, "construct tris stay a brush mesh " .. tostring(world.tri_count))
	T.ok(world.lightmap ~= nil, "lightmap atlas")
	if world.lightmap then
		T.eq(world.lightmap.w, bsp.LIGHTMAP_SIZE, "atlas width")
		T.eq(world.lightmap.h, bsp.LIGHTMAP_SIZE, "atlas height")
		T.eq(world.lightmap.dropped, 0, "every lit face fits")
		T.ok(world.lightmap.faces > 100, "packed faces " .. tostring(world.lightmap.faces))
		local rgba = world.lightmap.rgba
		local dark, lit = 0, 0
		local step = 64
		local pixels = world.lightmap.w * world.lightmap.h
		for i = 0, pixels - 1, step do
			local o = i * 4
			local r, g, b = rgba:byte(o + 1, o + 3)
			local y = r + g + b
			if y < 40 then
				dark = dark + 1
			elseif y > 300 and y < 750 then
				lit = lit + 1
			end
		end
		T.ok(dark > 10, "atlas has shadow luxels " .. tostring(dark))
		T.ok(lit > 10, "atlas has lit luxels " .. tostring(lit))
		world.lightmap.rgba = nil
	end
	local sky, concrete = false, nil
	for i = 1, #world.surfaces do
		local sn = world.surfaces[i].name:lower()
		if sn:find("toolsskybox", 1, true) or sn:find("toolsnodraw", 1, true) then
			sky = true
		end
		if sn:find("construct_concrete_ground", 1, true) then
			concrete = world.surfaces[i]
		end
	end
	T.ok(not sky, "sky and nodraw are not drawn")
	T.ok(concrete and #concrete.verts >= 15, "concrete ground triangles")
	if concrete then
		local umin, umax = math.huge, -math.huge
		local vmin, vmax = math.huge, -math.huge
		local count = #concrete.verts / bsp.VERT_STRIDE
		local lumin, lumax = math.huge, -math.huge
		local lvmin, lvmax = math.huge, -math.huge
		for i = 0, count - 1 do
			local uu = concrete.verts[i * bsp.VERT_STRIDE + 4]
			local vv = concrete.verts[i * bsp.VERT_STRIDE + 5]
			local lu = concrete.verts[i * bsp.VERT_STRIDE + 6]
			local lv = concrete.verts[i * bsp.VERT_STRIDE + 7]
			if uu < umin then umin = uu end
			if uu > umax then umax = uu end
			if vv < vmin then vmin = vv end
			if vv > vmax then vmax = vv end
			if lu < lumin then lumin = lu end
			if lu > lumax then lumax = lu end
			if lv < lvmin then lvmin = lv end
			if lv > lvmax then lvmax = lv end
		end
		T.ok(umax - umin > 0.05, "concrete u span " .. tostring(umax - umin))
		T.ok(vmax - vmin > 0.05, "concrete v span " .. tostring(vmax - vmin))
		T.ok(lumin > 0 and lumax < 1, "concrete light u inside atlas " .. tostring(lumin) .. ".." .. tostring(lumax))
		T.ok(lvmin > 0 and lvmax < 1, "concrete light v inside atlas " .. tostring(lvmin) .. ".." .. tostring(lvmax))
		T.ok(lumax - lumin > 1e-4, "concrete light u varies " .. tostring(lumax - lumin))
		T.ok(lvmax - lvmin > 1e-4, "concrete light v varies " .. tostring(lvmax - lvmin))
	end
	local lr, lg, lb = bsp.display_light(128, 128, 128, 0)
	local expect = (128 / 255) ^ (1 / 2.2)
	T.near(lr, expect, 1e-5, "display light r")
	T.near(lg, expect, 1e-5, "display light g")
	T.near(lb, expect, 1e-5, "display light b")
	local sx = bsp.sky_place(0, 0, 0, { x = 10, y = 0, z = 0, scale = 16 })
	T.near(sx, -160, 1e-6, "sky place x")
	local bx, by, bz = bsp.bmodel_point(1240, 968, -4, { x = -2048, y = -3600, z = 156 })
	T.near(bx, -808, 1e-4, "bmodel x")
	T.near(by, -2632, 1e-4, "bmodel y")
	T.near(bz, 152, 1e-4, "bmodel z")
	local rx, ry, rz = bsp.bmodel_point(1, 0, 0, { x = 10, y = 0, z = 0, yaw = 90 })
	T.near(rx, 10, 1e-4, "bmodel yaw x")
	T.near(ry, 1, 1e-4, "bmodel yaw y")
	T.near(rz, 0, 1e-4, "bmodel yaw z")
	T.near(bsp.shift_plane(0, 0, 1, -4, 0, 0, 156), 152, 1e-4, "shifted plane")
	local color_room
	for i = 1, #world.surfaces do
		if world.surfaces[i].name:find("COLOR_ROOM", 1, true) then
			color_room = world.surfaces[i]
			break
		end
	end
	T.ok(color_room ~= nil, "color room mesh exists")
	if color_room then
		local stride = bsp.VERT_STRIDE
		local count = #color_room.verts / stride
		local near_spawn = 0
		local placed = false
		local ex, ey, ez = 704, 132, -79
		for i = 0, count - 1 do
			local x = color_room.verts[i * stride + 1]
			local y = color_room.verts[i * stride + 2]
			local z = color_room.verts[i * stride + 3]
			local dx, dy, dz = x - ex, y - ey, z - ez
			if dx * dx + dy * dy + dz * dz < 400 * 400 then
				near_spawn = near_spawn + 1
			end
			if math.abs(x + 808) < 0.2 and math.abs(y + 2632) < 0.2 and math.abs(z - 152) < 0.2 then
				placed = true
			end
		end
		T.eq(near_spawn, 0, "color room stays off the construct spawn")
		T.ok(placed, "color room corner follows the func_brush origin")
	end

	local mount = content.mount({
		gmod = gmod,
		bsp = construct,
		pak_ofs = world.pak_ofs,
		pak_len = world.pak_len,
	})
	local key, txf = mount:describe("GM_CONSTRUCT/CONSTRUCT_CONCRETE_GROUND")
	T.eq(key, "materials/gm_construct/construct_concrete_ground.vtf", "concrete basetexture")
	T.ok(txf and math.abs(txf.sx - 1.25) < 1e-4, "concrete texture scale")
	local patched = mount:describe("maps/gm_construct/concrete/concretefloor028a_-2408_-2702_329")
	T.eq(patched, "materials/concrete/concretefloor028a.vtf", "pak patch include")
	local gk, ge = mount:describe("GLASS/REFLECTIVEGLASS001")
	T.ok(gk == nil and ge == "transparent", "glass is not drawn as magenta (" .. tostring(ge) .. ")")
	local wk, we = mount:describe("gm_construct/water_13_beneath")
	T.ok(wk ~= nil and wk ~= "", "water beneath resolves (" .. tostring(we) .. ")")
	local cards = mount:read("materials/models/props_foliage/tree_springers_cards_01.vmt")
	T.ok(vmt.alphatest(vmt.pairs(cards)), "springer cards are alphatest")
	T.ok(not vmt.alphatest(vmt.pairs('"LightmappedGeneric" { "$basetexture" "brick/brick" }')), "opaque is not alphatest")
	T.ok(vmt.transition({ __shader = "worldvertextransition", ["$basetexture2"] = "grass2" }), "wvt is a transition")
	T.ok(not vmt.transition({ __shader = "lightmappedgeneric", ["$basetexture"] = "brick" }), "lightmapped is not a transition")
	T.eq(bsp.disp_odd(0), false, "even disp quad")
	T.eq(bsp.disp_odd(1), true, "odd disp quad")
	T.eq(bsp.disp_odd(17), true, "power-4 row start")
	T.near(bsp.disp_blend(0), 0, 1e-6, "blend 0")
	T.near(bsp.disp_blend(255), 1, 1e-6, "blend 255")
	T.near(bsp.disp_blend(255.003), 1, 1e-6, "blend clamp")
	T.near(bsp.disp_blend(127.5), 0.5, 1e-4, "blend mid")

	local lo, hi, nblend = 1, 0, 0
	for i = 1, #world.surfaces do
		local sn = world.surfaces[i].name:lower()
		if sn:find("grass_13", 1, true) and not sn:find("grass-sand", 1, true) then
			local stride = bsp.VERT_STRIDE
			local count = #world.surfaces[i].verts / stride
			for v = 0, count - 1 do
				local b = world.surfaces[i].verts[v * stride + 9]
				nblend = nblend + 1
				if b < lo then lo = b end
				if b > hi then hi = b end
			end
		end
	end
	T.ok(nblend > 100, "grass_13 verts " .. tostring(nblend))
	T.ok(hi - lo > 0.5, "grass_13 blend spans both textures " .. tostring(lo) .. ".." .. tostring(hi))

	local grass = mount:material("gm_construct/grass_13", 32)
	T.ok(grass and grass.blend, "grass_13 blends")
	if grass then
		T.eq(grass.key, "materials/gm_construct/grass1.vtf", "grass basetexture")
		T.eq(grass.key2, "materials/gm_construct/grass2.vtf", "grass basetexture2")
		T.ok(grass.mask_key == nil, "grass_13 has no blend mask")
		T.near(grass.detail_scale, 0.12, 1e-4, "grass detail scale")
		T.near(grass.detail_blend, 0.5, 1e-4, "grass detail blend")
	end
	local sand = mount:material("gm_construct/grass-sand_13", 32)
	T.ok(sand and sand.blend and sand.mask_key ~= nil, "grass-sand uses the blend mask")
	if sand and sand.transform2 then
		T.near(sand.transform2.rot, 30, 1e-3, "sand texture rotate")
		T.eq(sand.key2, "materials/gm_construct/construct_sand.vtf", "sand basetexture2")
	end
end
