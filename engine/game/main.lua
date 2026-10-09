-- Flat window, or two eye renders.
-- The simulator reports one view. That view is a mirror. Stereo is two passes,
-- each with its own origin, same orientation, half an IPD along head-right.
-- Gameplay stays in source units. The only meter conversion is pure.coords.

local function engine_root()
	local env = os.getenv("ENGINE_ROOT")
	if env and env ~= "" then
		return env
	end
	local src = debug.getinfo(1, "S").source
	if src:sub(1, 1) == "@" then
		src = src:sub(2)
	end
	return (src:match("^(.*)/game/main%.lua$")) or "."
end

local ROOT = engine_root()
package.path = ROOT .. "/src/?.lua;" .. package.path

local coords = require("pure.coords")
local units = require("pure.units")
local move = require("pure.move")
local trace = require("pure.trace")
local vphysics = require("pure.vphysics")
local bsp = require("pure.bsp")
local content = require("pure.content")
local vmt = require("pure.vmt")
local props = require("pure.props")
local mdl = require("pure.mdl")
local angles = require("pure.angles")
local skybox = require("pure.skybox")

local MODE = os.getenv("ENGINE_MODE") or "flat"
local STEREO = MODE == "stereo" or MODE == "simulator"
local CAPTURE = os.getenv("ENGINE_CAPTURE") == "1"
local FRAME = os.getenv("ENGINE_FRAME") == "1"
local SHOT = os.getenv("ENGINE_SHOT") == "1"
local SCALE = tonumber(os.getenv("ENGINE_VIEW_SCALE")) or units.VRMOD_VIEW_SCALE
local IPD = coords.DEFAULT_IPD_METERS
local EYE_H = 64
-- Source default. The number is horizontal on a 4:3 window, not the vertical angle.
local PLAY_FOV = 75

-- Capture fixture. Same numbers as pure.coords.stereo_disparity_px.
local CAP_W, CAP_H = 640, 480
local CAP_FOV_Y = 90
local CAP_DEPTH = 2

local player
local world
local crate
local acc = 0
local shader
local solid
local left_tex, right_tex
local eye_pass = {}
local map_mesh
local map_parts
local map_name
local sky_draw
local draw_origin_x, draw_origin_y, draw_origin_z = 0, 0, 0
-- Fallback until the map's env_skypaint topcolor is read. Offscreen passes
-- ignore the window background and clear black unless this is applied.
local sky_rgb = { 0.45, 0.62, 0.78 }
local sampler
local shot_tex
local shot_tex_r

local function key_down(k)
	local ok, down = pcall(lovr.system.isKeyDown, k)
	return ok and down or false
end

local function view_quat(yaw_deg, pitch_deg)
	local q = lovr.math.newQuat(math.rad(yaw_deg), 0, 1, 0)
	local p = lovr.math.newQuat(math.rad(pitch_deg or 0), 1, 0, 0)
	q:mul(p)
	return q
end

local function set_proj(pass, view, fov_y_deg, w, h)
	local vh = math.rad(fov_y_deg * 0.5)
	local hh = math.atan(math.tan(vh) * (w / h))
	pass:setProjection(view, hh, hh, vh, vh)
end

-- Source AABB → one LÖVR box. Axis sizes come from the same remap as positions.
local function draw_source_box(pass, cx, cy, cz, hx, hy, hz, r, g, b)
	local x, y, z = coords.source_to_lovr(cx, cy, cz, SCALE)
	pass:setColor(r, g, b)
	pass:box(x, y, z, (hy * 2) / SCALE, (hz * 2) / SCALE, (hx * 2) / SCALE)
end

local function draw_world(pass)
	if sampler then
		pass:setSampler(sampler)
	end
	-- 2D skybox sits on the camera. Depth stays clear so the world overwrites it
	-- and openings keep the painted sky instead of the flat clear color.
	if sky_draw then
		pass:setDepthWrite(false)
		pass:setShader(shader)
		pass:setColor(1, 1, 1)
		for i = 1, #sky_draw do
			pass:setMaterial(sky_draw[i].tex)
			pass:draw(sky_draw[i].mesh, draw_origin_x, draw_origin_y, draw_origin_z)
		end
		pass:setMaterial()
		pass:setDepthWrite(true)
	end
	if map_parts then
		pass:setShader(shader)
		pass:setColor(1, 1, 1)
		for i = 1, #map_parts do
			pass:setMaterial(map_parts[i].tex)
			pass:draw(map_parts[i].mesh)
		end
		pass:setMaterial()
	elseif map_mesh then
		pass:setShader(shader)
		pass:setColor(1, 1, 1)
		pass:draw(map_mesh)
	else
		pass:setShader(solid)
		-- Visual floor is smaller than the collision brush. A 125 m box
		-- blows LÖVR's GPU buffer pool when drawn into extra passes.
		draw_source_box(pass, 0, 0, -8, 512, 512, 8, 0.25, 0.42, 0.28)
		draw_source_box(pass, 200, 80, 64, 8, 8, 64, 0.25, 0.35, 0.7)
	end
	if crate then
		pass:setShader(solid)
		local p, h = crate.pos, crate.half
		draw_source_box(pass, p.x, p.y, p.z, h.x, h.y, h.z, 0.85, 0.45, 0.12)
	end
end

local function draw_marker(pass)
	pass:setShader(solid)
	pass:setColor(1, 0, 0)
	pass:sphere(0, 0, -CAP_DEPTH, 0.12)
end

local function render_target(slot, tex, x, y, z, q, fov, draw)
	local pass = eye_pass[slot]
	if not pass then
		pass = lovr.graphics.newPass(tex)
		eye_pass[slot] = pass
	else
		pass:reset()
	end
	if draw == draw_world then
		pass:setClear(sky_rgb[1], sky_rgb[2], sky_rgb[3], 1)
		lovr.graphics.setBackgroundColor(sky_rgb[1], sky_rgb[2], sky_rgb[3])
	else
		pass:setClear(0.02, 0.02, 0.05, 1)
	end
	if q then
		pass:setViewPose(1, x, y, z, q)
	else
		pass:setViewPose(1, x, y, z, 0, 0, 1, 0)
	end
	set_proj(pass, 1, fov, tex:getWidth(), tex:getHeight())
	draw_origin_x, draw_origin_y, draw_origin_z = x, y, z
	draw(pass)
	lovr.graphics.submit(pass)
end

local function save_png(tex, path)
	lovr.graphics.wait()
	local blob = tex:getPixels():encode("png")
	local f = assert(io.open(path, "wb"))
	f:write(blob:getString())
	f:close()
end

local function scan_channel(img, want_red)
	local w, h = img:getWidth(), img:getHeight()
	local sx, n = 0, 0
	for y = 0, h - 1 do
		for x = 0, w - 1 do
			local r, g = img:getPixel(x, y)
			local hit = want_red and (r > 0.6 and r > g + 0.25) or ((not want_red) and g > 0.6 and g > r + 0.25)
			if hit then
				sx = sx + x
				n = n + 1
			end
		end
	end
	if n == 0 then
		return nil, 0
	end
	return sx / n, n
end

-- Yaw 0 must look along source +X (LÖVR -Z). Source +Y is left, so a green
-- box there lands at a smaller screen x than the red box on the forward axis.
local function write_frame()
	local dir = ROOT .. "/qa/out"
	local x, y, z = coords.eye_center(0, 0, EYE_H, 0, 0, IPD, SCALE)
	local q = view_quat(0, 0)
	lovr.graphics.setBackgroundColor(0.02, 0.02, 0.05)
	local pass = lovr.graphics.newPass(left_tex)
	pass:setViewPose(1, x, y, z, q)
	set_proj(pass, 1, CAP_FOV_Y, CAP_W, CAP_H)
	pass:setShader(solid)
	draw_source_box(pass, 150, 0, EYE_H, 6, 6, 6, 1, 0, 0)
	draw_source_box(pass, 150, 60, EYE_H, 6, 6, 6, 0, 1, 0)
	lovr.graphics.submit(pass)
	lovr.graphics.wait()
	local img = left_tex:getPixels()
	local blob = img:encode("png")
	local f = assert(io.open(dir .. "/frame.png", "wb"))
	f:write(blob:getString())
	f:close()
	local rx, rn = scan_channel(img, true)
	local gx, gn = scan_channel(img, false)
	local ok = rx and gx and gx < rx - 5 and math.abs(rx - CAP_W * 0.5) < 40
	local line = string.format(
		"red_x %s red_n %d green_x %s green_n %d center %g ok %s\n",
		tostring(rx), rn, tostring(gx), gn, CAP_W * 0.5, tostring(ok)
	)
	local meta = assert(io.open(dir .. "/frame.txt", "w"))
	meta:write(line)
	meta:close()
	print(line)
	if not ok then
		error("frame check failed: " .. line)
	end
end

local function write_capture()
	local dir = ROOT .. "/qa/out"
	local lx, ly, lz = -IPD * 0.5, 0, 0
	local rx, ry, rz = IPD * 0.5, 0, 0
	render_target("L", left_tex, lx, ly, lz, nil, CAP_FOV_Y, draw_marker)
	render_target("R", right_tex, rx, ry, rz, nil, CAP_FOV_Y, draw_marker)
	save_png(left_tex, dir .. "/left.png")
	save_png(right_tex, dir .. "/right.png")
	local expected = coords.stereo_disparity_px(IPD, CAP_DEPTH, CAP_FOV_Y, CAP_H)
	local meta = io.open(dir .. "/stereo.txt", "w")
	assert(meta)
	meta:write(string.format(
		"width %d\nheight %d\nfov_y %g\nipd %.6f\ndepth %.6f\nlua_expected %.8f\n",
		CAP_W, CAP_H, CAP_FOV_Y, IPD, CAP_DEPTH, expected
	))
	meta:close()
	print(string.format("capture wrote %s (lua disparity %.4f px)", dir, expected))
end

local function draw_sbs(pass)
	lovr.graphics.setBackgroundColor(0, 0, 0)
	pass:setShader(shader)
	pass:setViewPose(1, 0, 0, 0, 0, 0, 1, 0)
	set_proj(pass, 1, 50, pass:getWidth(), pass:getHeight())
	pass:setColor(1, 1, 1)
	pass:setMaterial(left_tex)
	pass:plane(-0.62, 0, -1, 1.05, 0.78)
	pass:setMaterial(right_tex)
	pass:plane(0.62, 0, -1, 1.05, 0.78)
	pass:setMaterial()
end

local function eye_xyz(sign)
	return coords.eye_center(player.pos.x, player.pos.y, player.pos.z + EYE_H, player.yaw, sign, IPD, SCALE)
end

local function read_stick()
	local ok, x, y = pcall(function()
		return lovr.headset.getAxis("hand/left", "thumbstick")
	end)
	if not ok then
		return 0, 0
	end
	return x or 0, y or 0
end

local function read_input()
	local speed = player.maxspeed
	local f, s = 0, 0
	if key_down("w") or key_down("up") then
		f = f + speed
	end
	if key_down("s") or key_down("down") then
		f = f - speed
	end
	-- side_move is along AngleVectors right. At yaw 0 that is source -Y.
	if key_down("d") or key_down("right") then
		s = s + speed
	end
	if key_down("a") or key_down("left") then
		s = s - speed
	end
	local sx, sy = read_stick()
	if math.abs(sy) > 0.2 then
		f = f - sy * speed
	end
	if math.abs(sx) > 0.2 then
		s = s + sx * speed
	end
	player.forward_move = f
	player.side_move = s
	player.jump = key_down("space")
end

local function upload_map(src)
	local n = #src / 6
	local verts = {}
	for i = 0, n - 1 do
		local o = i * 6
		local x, y, z = coords.source_to_lovr(src[o + 1], src[o + 2], src[o + 3], SCALE)
		verts[i + 1] = { x, y, z, src[o + 4], src[o + 5], src[o + 6], 1 }
	end
	return lovr.graphics.newMesh({
		{ name = "VertexPosition", type = "vec3" },
		{ name = "VertexColor", type = "vec4" },
	}, verts)
end

local function gmod_dir()
	return os.getenv("ENGINE_GMOD")
		or "/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod"
end

-- Capture and the axis frame stay on the builtin scene. Play and ENGINE_SHOT
-- open gm_construct from the Steam install unless ENGINE_MAP says otherwise.
local function map_path()
	local env = os.getenv("ENGINE_MAP")
	if env == "none" or env == "0" then
		return nil
	end
	if env and env ~= "" then
		return env
	end
	if CAPTURE or FRAME then
		return nil
	end
	local path = gmod_dir() .. "/maps/gm_construct.bsp"
	local f = io.open(path, "rb")
	if not f then
		print("gm_construct.bsp not found; builtin room")
		return nil
	end
	f:close()
	return path
end

local function make_texture(rgba, w, h)
	local blob = lovr.data.newBlob(rgba)
	local image = lovr.data.newImage(w, h, "rgba8", blob)
	return lovr.graphics.newTexture(image, { mipmaps = false })
end

local function upload_uv_mesh(src, xf)
	local stride = bsp.VERT_STRIDE
	local n = math.floor(#src / stride)
	local verts = {}
	for i = 0, n - 1 do
		local o = i * stride
		local u, v = src[o + 4], src[o + 5]
		if xf then
			u, v = vmt.apply_uv(u, v, xf)
		end
		local x, y, z = coords.source_to_lovr(src[o + 1], src[o + 2], src[o + 3], SCALE)
		verts[i + 1] = { x, y, z, u, v, src[o + 6], src[o + 7], src[o + 8], 1 }
	end
	return lovr.graphics.newMesh({
		{ name = "VertexPosition", type = "vec3" },
		{ name = "VertexUV", type = "vec2" },
		{ name = "VertexColor", type = "vec4" },
	}, verts)
end

local function static_prop_parts(mount, path, sky, bound, max_edge)
	local list = props.read(path)
	local cache = {}
	local function load_model(model)
		if cache[model] ~= nil then
			return cache[model] or nil
		end
		local base = model:lower():gsub("\\", "/"):gsub("%.mdl$", "")
		local a = mount:read(base .. ".mdl")
		local b = mount:read(base .. ".vvd")
		local c = mount:read(base .. ".dx90.vtx") or mount:read(base .. ".vtx")
		local loaded = nil
		if a and b and c then
			loaded = mdl.load(a, b, c)
		end
		cache[model] = loaded or false
		return loaded
	end
	local groups = {}
	local drawn, skipped, tris = 0, 0, 0
	for i = 1, #list do
		local p = list[i]
		local lname = p.model:lower()
		-- Foliage cards are alphatest quads. Drawn solid, they become green slabs.
		if lname:find("foliage", 1, true) or lname:find("tree_", 1, true) then
			skipped = skipped + 1
		else
			local model = load_model(p.model)
			if not model or not model.meshes then
				skipped = skipped + 1
			else
				local sky_prop = false
				local scale = 1
				local ox, oy, oz = p.x, p.y, p.z
				if sky then
					local dx, dy, dz = p.x - sky.x, p.y - sky.y, p.z - sky.z
					if dx * dx + dy * dy + dz * dz < 6000 * 6000 then
						sky_prop = true
						scale = sky.scale
						ox, oy, oz = bsp.sky_place(p.x, p.y, p.z, sky)
					end
				end
				for m = 1, #model.meshes do
					local mesh = model.meshes[m]
					local g = groups[mesh.material]
					if not g then
						g = {}
						groups[mesh.material] = g
					end
					local src = mesh.verts
					local n = math.floor(#src / 5)
					for v = 0, n - 1 do
						local o = v * 5
						local x = src[o + 1] * scale
						local y = src[o + 2] * scale
						local z = src[o + 3] * scale
						x, y, z = angles.rotate(p.pitch, p.yaw, p.roll, x, y, z)
						g[#g + 1] = ox + x
						g[#g + 1] = oy + y
						g[#g + 1] = oz + z
						g[#g + 1] = src[o + 4]
						g[#g + 1] = src[o + 5]
						g[#g + 1] = 1
						g[#g + 1] = 1
						g[#g + 1] = 1
					end
				end
				drawn = drawn + 1
				tris = tris + (model.tris or 0)
			end
		end
	end
	local added = 0
	for name, verts in pairs(groups) do
		local mat, err = mount:material(name, max_edge)
		if mat then
			bound[#bound + 1] = {
				key = mat.key,
				w = mat.w,
				h = mat.h,
				rgba = mat.rgba,
				transform = mat.transform,
				verts = verts,
				name = name,
			}
			added = added + 1
		else
			print("prop material " .. name .. " (" .. tostring(err) .. ")")
		end
	end
	print(string.format(
		"props %d drawn %d skipped %d tris %d materials %d",
		#list, drawn, skipped, tris, added
	))
end

local function upload_lovr_mesh(src)
	local stride = 8
	local n = math.floor(#src / stride)
	local mesh_verts = {}
	for i = 0, n - 1 do
		local o = i * stride
		mesh_verts[i + 1] = {
			src[o + 1], src[o + 2], src[o + 3],
			src[o + 4], src[o + 5],
			src[o + 6], src[o + 7], src[o + 8], 1,
		}
	end
	return lovr.graphics.newMesh({
		{ name = "VertexPosition", type = "vec3" },
		{ name = "VertexUV", type = "vec2" },
		{ name = "VertexColor", type = "vec4" },
	}, mesh_verts)
end

-- Face distance in source units. The cube is drawn at the eye, so this only
-- has to sit past the near plane. 256 units is about 8 m at the default scale.
local SKY_WIDTH = 256

local function upload_skybox(mount, skyname, max_edge)
	sky_draw = nil
	if not skyname or skyname == "" then
		return
	end
	local parts = {}
	local gpu = {}
	for axis = 1, 6 do
		local suf = skybox.face_suffix(axis)
		local mat, err = mount:material("skybox/" .. skyname .. suf, max_edge)
		if not mat then
			print("skybox " .. skyname .. suf .. " (" .. tostring(err) .. ")")
			return
		end
		local tri = skybox.face_tris(axis, SKY_WIDTH)
		local flat = {}
		local n = math.floor(#tri / 5)
		for i = 0, n - 1 do
			local o = i * 5
			local x, y, z = coords.source_to_lovr(tri[o + 1], tri[o + 2], tri[o + 3], SCALE)
			local u, v = tri[o + 4], tri[o + 5]
			if mat.transform then
				u, v = vmt.apply_uv(u, v, mat.transform)
			end
			flat[#flat + 1] = x
			flat[#flat + 1] = y
			flat[#flat + 1] = z
			flat[#flat + 1] = u
			flat[#flat + 1] = v
			flat[#flat + 1] = 1
			flat[#flat + 1] = 1
			flat[#flat + 1] = 1
		end
		local tex = gpu[mat.key]
		if not tex then
			tex = make_texture(mat.rgba, mat.w, mat.h)
			gpu[mat.key] = tex
		end
		parts[#parts + 1] = { tex = tex, mesh = upload_lovr_mesh(flat) }
	end
	sky_draw = parts
	print(string.format("skybox %s faces %d", skyname, #parts))
end

local function upload_albedo(bound)
	local gpu = {}
	local parts = {}
	for i = 1, #bound do
		local s = bound[i]
		local tex = gpu[s.key]
		if not tex then
			tex = make_texture(s.rgba, s.w, s.h)
			gpu[s.key] = tex
		end
		parts[#parts + 1] = {
			tex = tex,
			mesh = upload_uv_mesh(s.verts, s.transform),
			name = s.name,
		}
		s.rgba = nil
	end
	return parts
end

local function builtin_world()
	world = { brushes = { trace.box_brush(-2048, -2048, -16, 2048, 2048, 0) } }
	player = move.new_player(os.getenv("ENGINE_PROFILE") or "gmod")
	player.pos = { x = 0, y = 0, z = 0 }
	player.on_ground = true
	crate = vphysics.new_box(120, 0, 16, 16, 16, 16)
	crate.floor_z = 0
	crate.vel.x = 80
end

local function try_map()
	local path = map_path()
	if not path then
		return false
	end
	print("loading " .. path)
	local ok, loaded = pcall(bsp.load, path, { uv = true })
	if not ok then
		print("map load failed: " .. tostring(loaded))
		return false
	end
	world = { brushes = loaded.brushes }
	player = move.new_player(os.getenv("ENGINE_PROFILE") or "gmod")
	if #loaded.spawns == 0 then
		print("map has no info_player_start: " .. path)
		player.pos = { x = 0, y = 0, z = 0 }
	else
		local s = loaded.spawns[1]
		local pos = { x = s.ox, y = s.oy, z = s.oz }
		local nudged = trace.nudge_up(world, pos)
		player.pos = nudged
		player.yaw = s.ayaw or 0
		player.pitch = s.apitch or 0
		print(string.format("spawn %.1f %.1f %.1f yaw %.1f", nudged.x, nudged.y, nudged.z, player.yaw))
	end
	player.on_ground = true
	map_name = path
	for i = 1, #loaded.entities do
		local e = loaded.entities[i]
		if e.classname == "env_skypaint" and e.topcolor then
			local r, g, b = e.topcolor:match("([%d%.%-]+)%s+([%d%.%-]+)%s+([%d%.%-]+)")
			r, g, b = tonumber(r), tonumber(g), tonumber(b)
			if r and g and b then
				sky_rgb[1], sky_rgb[2], sky_rgb[3] = r, g, b
			end
			break
		end
	end
	local surfaces = loaded.surfaces or {}
	local max_edge = tonumber(os.getenv("ENGINE_TEX_SIZE")) or 512
	local mount = content.mount({
		gmod = gmod_dir(),
		bsp = path,
		pak_ofs = loaded.pak_ofs,
		pak_len = loaded.pak_len,
	})
	local bound, stats = mount:bind(surfaces, max_edge)
	local skyname
	for i = 1, #loaded.entities do
		local e = loaded.entities[i]
		if e.classname == "worldspawn" and e.skyname and e.skyname ~= "" then
			skyname = e.skyname
			break
		end
	end
	upload_skybox(mount, skyname, max_edge)
	static_prop_parts(mount, path, loaded.sky, bound, max_edge)
	for i = 1, #stats.missing do
		print("missing material " .. stats.missing[i])
	end
	print(string.format("surfaces %d textures %d tris %d", #bound, stats.textures, loaded.tri_count))
	local uok, parts = pcall(upload_albedo, bound)
	if uok then
		map_parts = parts
		collectgarbage("collect")
	else
		print("albedo upload failed: " .. tostring(parts))
	end
	return true
end

function lovr.load()
	solid = lovr.graphics.newShader([[
		vec4 lovrmain() { return DefaultPosition; }
	]], [[
		vec4 lovrmain() { return Color; }
	]])
	shader = lovr.graphics.newShader([[
		vec4 lovrmain() {
			Color = VertexColor;
			return DefaultPosition;
		}
	]], [[
		vec4 lovrmain() { return Color * getPixel(ColorTexture, UV); }
	]])
	sampler = lovr.graphics.newSampler({ wrap = "repeat", filter = "linear" })
	local function eye_tex(w, h)
		return lovr.graphics.newTexture(w, h, {
			usage = { "render", "transfer", "sample" },
			mipmaps = false,
		})
	end
	if CAPTURE or FRAME or STEREO then
		left_tex = eye_tex(CAP_W, CAP_H)
		right_tex = eye_tex(CAP_W, CAP_H)
	end
	if SHOT then
		shot_tex = eye_tex(1280, 720)
		if STEREO then
			shot_tex_r = eye_tex(1280, 720)
		end
	end
	if not try_map() then
		if SHOT then
			error("shot has no map")
		end
		builtin_world()
	end
	print(string.format(
		"engine mode=%s stereo=%s capture=%s frame=%s shot=%s views=%s scale=%.3f",
		MODE, tostring(STEREO), tostring(CAPTURE), tostring(FRAME), tostring(SHOT),
		tostring(lovr.headset.getViewCount()), SCALE
	))
end

function lovr.update(dt)
	if CAPTURE or SHOT or not player then
		return
	end
	read_input()
	acc = acc + dt
	local steps = 0
	while acc >= move.TICK and steps < 5 do
		move.tick(player, move.TICK, world)
		if crate then
			vphysics.step(crate, move.TICK)
		end
		acc = acc - move.TICK
		steps = steps + 1
	end
end

function lovr.mousemoved(x, y, dx, dy)
	if CAPTURE or not player then
		return
	end
	local held = false
	local ok, down = pcall(lovr.system.isMouseDown, 1)
	held = ok and down
	if not held then
		return
	end
	-- Screen +x is a right turn. Positive source yaw turns left.
	player.yaw = player.yaw - dx * 0.12
	player.pitch = math.max(-80, math.min(80, (player.pitch or 0) - dy * 0.12))
end

local function shot_stats(img)
	local w, h = img:getWidth(), img:getHeight()
	local buckets = {}
	local n, content = 0, 0
	local mag_n, white_n = 0, 0
	local sr, sg, sb = 0, 0, 0
	local sl, sl2 = 0, 0
	for y = 0, h - 1, 2 do
		for x = 0, w - 1, 2 do
			local r, g, b = img:getPixel(x, y)
			n = n + 1
			local sky = math.abs(r - 0.45) < 0.07 and math.abs(g - 0.62) < 0.07 and math.abs(b - 0.78) < 0.07
			local magenta = r > 0.75 and b > 0.75 and g < 0.3
			local white = r > 0.97 and g > 0.97 and b > 0.97
			local black = r < 0.04 and g < 0.04 and b < 0.05
			if magenta then
				mag_n = mag_n + 1
			end
			if white then
				white_n = white_n + 1
			end
			if not sky and not magenta and not black then
				content = content + 1
				sr, sg, sb = sr + r, sg + g, sb + b
				local luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
				sl = sl + luma
				sl2 = sl2 + luma * luma
				local q = math.floor(r * 7.99) * 64 + math.floor(g * 7.99) * 8 + math.floor(b * 7.99)
				buckets[q] = true
			end
		end
	end
	local nb = 0
	for _ in pairs(buckets) do
		nb = nb + 1
	end
	local mean_l = content > 0 and (sl / content) or 0
	local var = content > 0 and (sl2 / content - mean_l * mean_l) or 0
	if var < 0 then
		var = 0
	end
	local std = math.sqrt(var)
	local frac = n > 0 and (content / n) or 0
	local mag_frac = n > 0 and (mag_n / n) or 0
	local white_frac = n > 0 and (white_n / n) or 0
	-- Magenta is a missing texture, not a map. White can be the unlit color room.
	local ok = frac > 0.2 and nb >= 8 and std > 0.015 and mag_frac < 0.02
	local line = string.format(
		"samples %d content %.3f buckets %d luma_std %.4f mean %.3f %.3f %.3f magenta %.3f white %.3f ok %s\n",
		n, frac, nb, std,
		content > 0 and sr / content or 0,
		content > 0 and sg / content or 0,
		content > 0 and sb / content or 0,
		mag_frac, white_frac, tostring(ok)
	)
	return ok, line
end

local function mean_abs_diff(a, b)
	local w, h = a:getWidth(), a:getHeight()
	local s, n = 0, 0
	for y = 0, h - 1, 3 do
		for x = 0, w - 1, 3 do
			local r1, g1, b1 = a:getPixel(x, y)
			local r2, g2, b2 = b:getPixel(x, y)
			s = s + math.abs(r1 - r2) + math.abs(g1 - g2) + math.abs(b1 - b2)
			n = n + 3
		end
	end
	if n == 0 then
		return 0
	end
	return s / n
end

local function write_shot()
	local dir = ROOT .. "/qa/out"
	os.execute('mkdir -p "' .. dir .. '"')
	local function eye(sign, tex, name)
		local x, y, z = eye_xyz(sign)
		local q = view_quat(player.yaw, player.pitch)
		local _, vfov = coords.source_fov(PLAY_FOV, tex:getWidth(), tex:getHeight())
		render_target(name, tex, x, y, z, q, vfov, draw_world)
		lovr.graphics.wait()
		return tex:getPixels()
	end
	local img = eye(0, shot_tex, "S")
	save_png(shot_tex, dir .. "/map.png")
	local ok, line = shot_stats(img)
	local extra = ""
	if STEREO and shot_tex_r then
		local left = eye(-1, shot_tex, "L")
		local right = eye(1, shot_tex_r, "R")
		save_png(shot_tex, dir .. "/map_left.png")
		save_png(shot_tex_r, dir .. "/map_right.png")
		local diff = mean_abs_diff(left, right)
		extra = string.format("eye_diff %.5f\n", diff)
		if diff < 0.005 then
			ok = false
		end
		line = line .. extra
	end
	if player then
		line = string.format(
			"map %s\nspawn %.2f %.2f %.2f yaw %.2f pitch %.2f\n%s",
			tostring(map_name), player.pos.x, player.pos.y, player.pos.z,
			player.yaw, player.pitch or 0, line
		)
	end
	local meta = assert(io.open(dir .. "/map.txt", "w"))
	meta:write(line)
	meta:close()
	print(line)
	if not ok then
		error("map shot failed: " .. line)
	end
end

function lovr.draw(pass)
	if CAPTURE then
		write_capture()
		draw_sbs(pass)
		lovr.event.quit(0)
		return
	end
	if FRAME then
		write_frame()
		lovr.event.quit(0)
		return
	end
	if SHOT then
		write_shot()
		pass:setShader(shader)
		pass:setViewPose(1, 0, 0, 0, 0, 0, 1, 0)
		set_proj(pass, 1, 50, pass:getWidth(), pass:getHeight())
		pass:setColor(1, 1, 1)
		pass:setMaterial(shot_tex)
		pass:plane(0, 0, -1, 1.6, 0.9)
		pass:setMaterial()
		lovr.event.quit(0)
		return
	end
	if STEREO then
		local q = view_quat(player.yaw, player.pitch)
		local lx, ly, lz = eye_xyz(-1)
		local rx, ry, rz = eye_xyz(1)
		local _, vfov = coords.source_fov(PLAY_FOV, left_tex:getWidth(), left_tex:getHeight())
		render_target("L", left_tex, lx, ly, lz, q, vfov, draw_world)
		render_target("R", right_tex, rx, ry, rz, q, vfov, draw_world)
		lovr.graphics.wait()
		draw_sbs(pass)
		return
	end
	lovr.graphics.setBackgroundColor(sky_rgb[1], sky_rgb[2], sky_rgb[3])
	local x, y, z = eye_xyz(0)
	draw_origin_x, draw_origin_y, draw_origin_z = x, y, z
	pass:setViewPose(1, x, y, z, view_quat(player.yaw, player.pitch))
	local _, vfov = coords.source_fov(PLAY_FOV, pass:getWidth(), pass:getHeight())
	set_proj(pass, 1, vfov, pass:getWidth(), pass:getHeight())
	draw_world(pass)
end

function lovr.errhand(message)
	io.stderr:write(tostring(message), "\n")
	local f = io.open("/tmp/engine-lovr-err.txt", "w")
	if f then
		f:write(tostring(message), "\n")
		f:close()
	end
end
