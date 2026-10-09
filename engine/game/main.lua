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
local studio_anim = require("pure.studio_anim")
local angles = require("pure.angles")
local skybox = require("pure.skybox")
local glua = require("pure.glua")

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
local actor_line = ""
local world
local crate
local acc = 0
local shader
local alpha_shader
local blend_shader
local light_shader
local white_tex
local lightmap_tex
local light_sampler
local send_uv
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
		pass:setColor(1, 1, 1)
		for i = 1, #map_parts do
			local part = map_parts[i]
			if part.tex2 then
				-- Vertex alpha is the displacement blend. 0 keeps $basetexture.
				-- Color.rg is the lightmap coordinate, not a vertex light.
				pass:setShader(blend_shader)
				pass:setMaterial(part.tex)
				pass:send("BlendTexture", part.tex2)
				pass:send("BlendMask", part.mask or white_tex)
				pass:send("DetailTexture", part.detail or white_tex)
				pass:send("UseMask", part.mask and 1 or 0)
				pass:send("DetailScale", part.detail_scale or 1)
				pass:send("DetailBlend", part.detail and (part.detail_blend or 1) or 0)
				send_uv(pass, "Uv1", part.transform)
				send_uv(pass, "Uv2", part.transform2)
				pass:send("Lightmap", lightmap_tex or white_tex)
				pass:send("LightSampler", light_sampler)
			elseif part.alphatest then
				-- Springer cards stay fullbright. Detail and sway are not applied.
				pass:setShader(alpha_shader)
				pass:setMaterial(part.tex)
			elseif part.lit then
				pass:setShader(light_shader)
				pass:setMaterial(part.tex)
				pass:send("Lightmap", lightmap_tex or white_tex)
				pass:send("LightSampler", light_sampler)
			else
				pass:setShader(shader)
				pass:setMaterial(part.tex)
			end
			pass:draw(part.mesh)
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

-- Identity is center 0.5, scale 1, rotate 0. Blend materials apply this in the shader
-- so $basetexture and $basetexture2 can use different transforms of the same BSP uv.
function send_uv(pass, name, xf)
	if not xf then
		pass:send(name, { 0.5, 0.5, 1, 1 })
		pass:send(name .. "B", { 1, 0, 0, 0 })
	else
		local rad = math.rad(xf.rot)
		pass:send(name, { xf.cx, xf.cy, xf.sx, xf.sy })
		pass:send(name .. "B", { math.cos(rad), math.sin(rad), xf.tx, xf.ty })
	end
end

local function upload_uv_mesh(src, xf, use_blend)
	local stride = bsp.VERT_STRIDE
	local n = math.floor(#src / stride)
	local verts = {}
	for i = 0, n - 1 do
		local o = i * stride
		local u, v = src[o + 4], src[o + 5]
		-- WorldVertexTransition transforms both textures in the shader from the raw uv.
		if xf and not use_blend then
			u, v = vmt.apply_uv(u, v, xf)
		end
		local x, y, z = coords.source_to_lovr(src[o + 1], src[o + 2], src[o + 3], SCALE)
		local a = 1
		if use_blend then
			a = src[o + 9] or 0
		end
		verts[i + 1] = { x, y, z, u, v, src[o + 6], src[o + 7], src[o + 8], a }
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
	local function alphatest_name(name)
		local n = name:lower():gsub("\\", "/"):gsub("^materials/", ""):gsub("%.vmt$", "")
		local text = mount:read("materials/" .. n .. ".vmt")
		return vmt.alphatest(vmt.pairs(text))
	end
	for i = 1, #list do
		local p = list[i]
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
					g = { verts = {}, alphatest = alphatest_name(mesh.material) }
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
					local gv = g.verts
					gv[#gv + 1] = ox + x
					gv[#gv + 1] = oy + y
					gv[#gv + 1] = oz + z
					gv[#gv + 1] = src[o + 4]
					gv[#gv + 1] = src[o + 5]
					gv[#gv + 1] = 1
					gv[#gv + 1] = 1
					gv[#gv + 1] = 1
					gv[#gv + 1] = 0
				end
			end
			drawn = drawn + 1
			tris = tris + (model.tris or 0)
		end
	end
	local added = 0
	for name, g in pairs(groups) do
		local mat, err = mount:material(name, max_edge)
		if mat then
			bound[#bound + 1] = {
				key = mat.key,
				w = mat.w,
				h = mat.h,
				rgba = mat.rgba,
				transform = mat.transform,
				verts = g.verts,
				name = name,
				alphatest = g.alphatest,
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
	local function gpu_tex(key, rgba, w, h)
		if not key or not rgba then
			return nil
		end
		local tex = gpu[key]
		if not tex then
			tex = make_texture(rgba, w, h)
			gpu[key] = tex
		end
		return tex
	end
	local parts = {}
	for i = 1, #bound do
		local s = bound[i]
		local tex = gpu_tex(s.key, s.rgba, s.w, s.h)
		parts[#parts + 1] = {
			tex = tex,
			tex2 = gpu_tex(s.key2, s.rgba2, s.w2, s.h2),
			mask = gpu_tex(s.mask_key, s.mask_rgba, s.mask_w, s.mask_h),
			detail = gpu_tex(s.detail_key, s.detail_rgba, s.detail_w, s.detail_h),
			detail_scale = s.detail_scale,
			detail_blend = s.detail_blend,
			lit = s.lit,
			transform = s.blend and s.transform or nil,
			transform2 = s.transform2,
			mesh = upload_uv_mesh(s.verts, s.transform, s.blend),
			name = s.name,
			alphatest = s.alphatest,
		}
		s.rgba = nil
		s.rgba2 = nil
		s.mask_rgba = nil
		s.detail_rgba = nil
	end
	return parts
end

-- Weapon and NPC meshes. NPCs are skinned to one idle frame.
-- Lua Think and PrimaryAttack run once. VR_Shoot runs when that recorded no
-- bullet. A list weapon with a script damage and no Lua records one bullet.
-- A recorded bullet becomes a streak.
local function bake_actors(mount, bound, max_edge)
	local title = (map_name or ""):match("([^/]+)%.bsp$") or "gm_construct"
	local session = glua.boot({ gmod = gmod_dir(), map = title })
	print(session.summary)
	for i = 1, #session.failure_lines do
		print("fail " .. session.failure_lines[i])
	end
	glua.pump(session, 1)
	print(string.format(
		"timers ran %d errors %d",
		session.env.__timer_ran or 0,
		session.env.__timer_errors and #session.env.__timer_errors or 0
	))
	local timer_errors = session.env.__timer_errors or {}
	for i = 1, #timer_errors do
		print("timer " .. timer_errors[i])
	end
	local items, skipped = glua.collect(session, mount)
	local cache = {}
	local function load_model(model, pose)
		local key = tostring(model):lower():gsub("\\", "/")
		if pose then
			key = key .. "#idle"
		end
		if cache[key] ~= nil then
			return cache[key] or nil
		end
		local base = key:gsub("#idle$", ""):gsub("%.mdl$", "")
		local a = mount:read(base .. ".mdl")
		local b = mount:read(base .. ".vvd")
		local c = mount:read(base .. ".dx90.vtx") or mount:read(base .. ".vtx")
		local loaded = nil
		if a and b and c then
			local skin, seq
			if pose then
				local okm, mats, name = pcall(studio_anim.matrices, a, function(path)
					return mount:read(path)
				end, pose ~= true and pose or nil)
				if okm and type(mats) == "table" then
					skin = mats
					seq = name
				end
			end
			local ok, mesh = pcall(mdl.load, a, b, c, skin)
			if ok and type(mesh) == "table" then
				mesh.sequence = seq
				loaded = mesh
			end
		end
		cache[key] = loaded or false
		return loaded
	end
	local function alphatest_name(name)
		local n = name:lower():gsub("\\", "/"):gsub("^materials/", ""):gsub("%.vmt$", "")
		local text = mount:read("materials/" .. n .. ".vmt")
		return vmt.alphatest(vmt.pairs(text))
	end
	local kept = {}
	local missing_model = 0
	for i = 1, #items do
		local it = items[i]
		local loaded = load_model(it.model, it.kind == "npc")
		if (not loaded or not loaded.meshes or #loaded.meshes == 0) and it.view and it.view ~= it.model then
			local alt = load_model(it.view)
			if alt and alt.meshes and #alt.meshes > 0 then
				print("actor view fallback " .. tostring(it.class) .. " " .. tostring(it.model) .. " -> " .. tostring(it.view))
				it.model = it.view
				loaded = alt
			end
		end
		if not loaded or not loaded.meshes or #loaded.meshes == 0 then
			missing_model = missing_model + 1
			print("actor skip " .. tostring(it.kind) .. " " .. tostring(it.class) .. " " .. tostring(it.model))
		else
			local hmin = loaded.hull_min or {}
			local hmax = loaded.hull_max or {}
			local hz = (hmax.z or 0) - (hmin.z or 0)
			if hz > 1 and hz < 10000 and math.abs(hmin.z or 0) < 10000 then
				it.height = hz
				it.z_off = -(hmin.z or 0)
			else
				it.height = 72
				it.z_off = 0
			end
			local x0, x1 = hmin.x or 0, hmax.x or 0
			local y0, y1 = hmin.y or 0, hmax.y or 0
			local reach = 16
			local corners = { x0 * x0 + y0 * y0, x0 * x0 + y1 * y1, x1 * x1 + y0 * y0, x1 * x1 + y1 * y1 }
			for c = 1, 4 do
				local d = math.sqrt(corners[c])
				if d > reach then
					reach = d
				end
			end
			if reach ~= reach or reach > 4000 then
				reach = 256
			end
			it.span = reach
			it.loaded = loaded
			kept[#kept + 1] = it
		end
	end
	glua.layout(kept, player.pos, player.yaw)
	local ex_ok, exercised = pcall(glua.exercise, session, kept, world)
	if not ex_ok then
		print("exercise failed " .. tostring(exercised):gsub("%s+", " "))
		exercised = nil
	end
	exercised = exercised or {
		think_ok = 0,
		think_bad = 0,
		attack_ok = 0,
		attack_bad = 0,
		bullets = 0,
		lines = {},
		miss = {},
	}
	print(string.format(
		"exercise think_ok %d think_bad %d attack_ok %d attack_bad %d bullets %d",
		exercised.think_ok, exercised.think_bad, exercised.attack_ok,
		exercised.attack_bad, exercised.bullets
	))
	local ex_lines = exercised.lines or {}
	for i = 1, #ex_lines do
		print("exercise " .. ex_lines[i])
	end
	local ranked = {}
	for text, n in pairs(exercised.miss or {}) do
		ranked[#ranked + 1] = { n = n, text = text }
	end
	table.sort(ranked, function(a, b)
		return a.n > b.n
	end)
	for i = 1, math.min(8, #ranked) do
		print(string.format("exercise miss %d %s", ranked[i].n, ranked[i].text))
	end
	-- Entities Spawn created during that pass. A box from PhysicsInitBox
	-- takes one engine tick of the open solver. Entity velocity with no body
	-- stays where Spawn left it. A stored velocity is also one 160-unit streak.
	-- Moving entities are kept first so a still attachment cannot crowd them out.
	local tick = 1 / 66
	if session.env and session.env.engine and type(session.env.engine.TickInterval) == "function" then
		tick = session.env.engine.TickInterval()
	end
	local proj_drawn = 0
	local proj_moving = 0
	local proj_list = exercised.projectiles or {}
	local function projectile_mesh(ent)
		if type(ent) ~= "table" or not ent.__spawned or type(ent.GetModel) ~= "function" then
			return nil, nil
		end
		local model = ent:GetModel()
		if type(model) ~= "string" or model == "" then
			return nil, nil
		end
		local loaded = load_model(model, false)
		if loaded and loaded.meshes and #loaded.meshes > 0 then
			return loaded, model
		end
		print("projectile skip " .. tostring(ent.GetClass and ent:GetClass() or "") .. " " .. model)
		return nil, model
	end
	local function keep_projectile(ent, loaded, model, bullet)
		local pos = ent.GetPos and ent:GetPos() or {}
		local ang = ent.GetAngles and ent:GetAngles() or {}
		local item = {
			kind = "projectile",
			class = type(ent.GetClass) == "function" and ent:GetClass() or "",
			model = model or "",
			x = pos.x or 0,
			y = pos.y or 0,
			z = pos.z or 0,
			z_off = 0,
			yaw = ang.y or ang.yaw or 0,
			pitch = ang.p or ang.pitch or 0,
			roll = ang.r or ang.roll or 0,
		}
		if loaded then
			item.loaded = loaded
		end
		if bullet then
			item.bullets = { bullet }
			proj_moving = proj_moving + 1
		end
		kept[#kept + 1] = item
		proj_drawn = proj_drawn + 1
	end
	local seen = {}
	for i = 1, #proj_list do
		if proj_drawn >= 48 then
			break
		end
		local ent = proj_list[i]
		local bullet = glua.velocity_bullet(ent)
		if bullet then
			-- The streak and the mesh share the position after this tick.
			glua.integrate_body(ent, tick)
			bullet = glua.velocity_bullet(ent) or bullet
			local loaded, model = projectile_mesh(ent)
			keep_projectile(ent, loaded, model, bullet)
			seen[ent] = true
		end
	end
	for i = 1, #proj_list do
		if proj_drawn >= 48 then
			break
		end
		local ent = proj_list[i]
		if type(ent) == "table" and not seen[ent] then
			local loaded, model = projectile_mesh(ent)
			if loaded then
				keep_projectile(ent, loaded, model, nil)
			end
		end
	end
	print(string.format("projectiles made %d drawn %d moving %d", #proj_list, proj_drawn, proj_moving))
	local view_fwd = angles.angle_vectors(player.pitch or 0, player.yaw or 0, 0)
	local streak_name = "models/debug/debugwhite"
	local groups = {}
	local tris = 0
	local posed = 0
	local streaks = 0
	for i = 1, #kept do
		local it = kept[i]
		local model = it.loaded
		local oz = it.z + (it.z_off or 0)
		if i <= 8 then
			print(string.format(
				"actor %s %s at %.0f %.0f %.0f yaw %.0f seq %s model %s",
				it.kind, it.class, it.x, it.y, oz, it.yaw or 0,
				tostring(model and model.sequence), it.model
			))
		end
		if model and model.meshes then
		for m = 1, #model.meshes do
			local mesh = model.meshes[m]
			local g = groups[mesh.material]
			if not g then
				g = { verts = {}, alphatest = alphatest_name(mesh.material) }
				groups[mesh.material] = g
			end
			local src = mesh.verts
			local n = math.floor(#src / 5)
			for v = 0, n - 1 do
				local o = v * 5
				local x, y, z = src[o + 1], src[o + 2], src[o + 3]
				x, y, z = angles.rotate(it.pitch or 0, it.yaw or 0, it.roll or 0, x, y, z)
				local gv = g.verts
				gv[#gv + 1] = it.x + x
				gv[#gv + 1] = it.y + y
				gv[#gv + 1] = oz + z
				gv[#gv + 1] = src[o + 4]
				gv[#gv + 1] = src[o + 5]
				gv[#gv + 1] = 1
				gv[#gv + 1] = 1
				gv[#gv + 1] = 1
				gv[#gv + 1] = 0
			end
		end
		if model.sequence then
			posed = posed + 1
		end
		tris = tris + (model.tris or 0)
		end
		local streak = glua.streak_verts(it.bullets and it.bullets[1], view_fwd.x, view_fwd.y, view_fwd.z)
		if streak then
			local g = groups[streak_name]
			if not g then
				g = { verts = {}, alphatest = alphatest_name(streak_name) }
				groups[streak_name] = g
			end
			local gv = g.verts
			local n = math.floor(#streak / 3)
			for v = 0, n - 1 do
				local o = v * 3
				gv[#gv + 1] = streak[o + 1]
				gv[#gv + 1] = streak[o + 2]
				gv[#gv + 1] = streak[o + 3]
				gv[#gv + 1] = 0.5
				gv[#gv + 1] = 0.5
				gv[#gv + 1] = 1
				gv[#gv + 1] = 1
				gv[#gv + 1] = 1
				gv[#gv + 1] = 0
			end
			streaks = streaks + 1
		end
		it.loaded = nil
	end
	local added = 0
	for name, g in pairs(groups) do
		if #g.verts > 0 then
			local mat, err = mount:material(name, max_edge)
			if mat then
				bound[#bound + 1] = {
					key = mat.key,
					w = mat.w,
					h = mat.h,
					rgba = mat.rgba,
					transform = mat.transform,
					verts = g.verts,
					name = name,
					alphatest = g.alphatest,
				}
				added = added + 1
			else
				print("actor material " .. name .. " (" .. tostring(err) .. ")")
			end
		end
	end
	local weapons, npcs, projs = 0, 0, 0
	local near_d, far_d = 1e9, 0
	for i = 1, #kept do
		if kept[i].kind == "weapon" then
			weapons = weapons + 1
		elseif kept[i].kind == "npc" then
			npcs = npcs + 1
		elseif kept[i].kind == "projectile" then
			projs = projs + 1
		end
		local dx = kept[i].x - player.pos.x
		local dy = kept[i].y - player.pos.y
		local dist = math.sqrt(dx * dx + dy * dy)
		if dist < near_d then
			near_d = dist
		end
		if dist > far_d then
			far_d = dist
		end
	end
	if #kept == 0 then
		near_d, far_d = 0, 0
	end
	actor_line = string.format(
		"\nactors draw %d weapons %d npcs %d posed %d attack %d bullets %d streaks %d missing %d tris %d materials %d skipped %d proj %d near %.0f far %.0f\n%s\n",
		#kept, weapons, npcs, posed, exercised.attack_ok, exercised.bullets, streaks,
		missing_model, tris, added, #skipped, projs, near_d, far_d, session.summary
	)
	print(actor_line)
	for i = 1, math.min(8, #skipped) do
		print("skip " .. skipped[i])
	end
	collectgarbage("collect")
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
	lightmap_tex = nil
	if loaded.lightmap and loaded.lightmap.rgba then
		local lm = loaded.lightmap
		local lok, tex = pcall(make_texture, lm.rgba, lm.w, lm.h)
		local faces, dropped = lm.faces or 0, lm.dropped or 0
		local lw, lh = lm.w, lm.h
		lm.rgba = nil
		loaded.lightmap = nil
		if lok then
			lightmap_tex = tex
			print(string.format("lightmap %dx%d packed %d dropped %d", lw, lh, faces, dropped))
		else
			print("lightmap upload failed: " .. tostring(tex))
		end
		collectgarbage("collect")
	end
	local surfaces = loaded.surfaces or {}
	local max_edge = tonumber(os.getenv("ENGINE_TEX_SIZE")) or 512
	local mount = content.mount({
		gmod = gmod_dir(),
		bsp = path,
		pak_ofs = loaded.pak_ofs,
		pak_len = loaded.pak_len,
	})
	print("workshop gmas " .. tostring(mount.gma_count or 0))
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
	local aok, aerr = pcall(bake_actors, mount, bound, max_edge)
	if not aok then
		actor_line = "\nactors failed " .. tostring(aerr):gsub("%s+", " ") .. "\n"
		print(actor_line)
	end
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
	-- World brushes store a lightmap atlas coordinate in Color.rg.
	-- Props keep the shader above and a white vertex color.
	light_shader = lovr.graphics.newShader([[
		vec4 lovrmain() {
			Color = VertexColor;
			return DefaultPosition;
		}
	]], [[
		uniform texture2D Lightmap;
		uniform sampler LightSampler;
		vec4 lovrmain() {
			vec3 light = texture(sampler2D(Lightmap, LightSampler), Color.rg).rgb;
			vec3 albedo = getPixel(ColorTexture, UV).rgb;
			return vec4(light * albedo, 1.0);
		}
	]])
	-- Source $alphatestreference default is 0.7. Below that the card is a hole.
	alpha_shader = lovr.graphics.newShader([[
		vec4 lovrmain() {
			Color = VertexColor;
			return DefaultPosition;
		}
	]], [[
		vec4 lovrmain() {
			vec4 tex = getPixel(ColorTexture, UV);
			if (tex.a < 0.7) { discard; }
			return Color * vec4(tex.rgb, 1.0);
		}
	]])
	-- lightmappedgeneric_ps2_3_x.h: lerp(base, base2, vertexAlpha).
	-- $blendmodulatetexture remaps that alpha. $detail mode 0 is base * lerp(1, detail*2, factor).
	blend_shader = lovr.graphics.newShader([[
		vec4 lovrmain() {
			Color = VertexColor;
			return DefaultPosition;
		}
	]], [[
		uniform texture2D BlendTexture;
		uniform texture2D BlendMask;
		uniform texture2D DetailTexture;
		uniform vec4 Uv1;
		uniform vec4 Uv1B;
		uniform vec4 Uv2;
		uniform vec4 Uv2B;
		uniform float UseMask;
		uniform float DetailScale;
		uniform float DetailBlend;
		uniform texture2D Lightmap;
		uniform sampler LightSampler;

		vec2 apply_uv(vec2 uv, vec4 a, vec4 b) {
			vec2 d = (uv - a.xy) * a.zw;
			vec2 r = vec2(b.x * d.x - b.y * d.y, b.y * d.x + b.x * d.y);
			return r + a.xy + b.zw;
		}

		vec4 lovrmain() {
			vec4 base = getPixel(ColorTexture, apply_uv(UV, Uv1, Uv1B));
			vec4 base2 = getPixel(BlendTexture, apply_uv(UV, Uv2, Uv2B));
			float k = clamp(Color.a, 0.0, 1.0);
			if (UseMask > 0.5) {
				vec4 modt = getPixel(BlendMask, UV);
				float minb = clamp(modt.g - modt.r, 0.0, 1.0);
				float maxb = clamp(modt.g + modt.r, 0.0, 1.0);
				k = smoothstep(minb, max(maxb, minb + 0.0001), k);
			}
			vec3 albedo = mix(base.rgb, base2.rgb, k);
			if (DetailBlend > 0.0) {
				vec3 detail = getPixel(DetailTexture, UV * DetailScale).rgb;
				albedo *= mix(vec3(1.0), detail * 2.0, DetailBlend);
			}
			vec3 light = texture(sampler2D(Lightmap, LightSampler), Color.rg).rgb;
			return vec4(light * albedo, 1.0);
		}
	]])
	white_tex = make_texture(string.char(255, 255, 255, 255), 1, 1)
	sampler = lovr.graphics.newSampler({ wrap = "repeat", filter = "linear" })
	light_sampler = lovr.graphics.newSampler({ wrap = "clamp", filter = "linear" })
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
	if actor_line ~= "" then
		line = line .. actor_line
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
