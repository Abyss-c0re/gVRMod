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

local MODE = os.getenv("ENGINE_MODE") or "flat"
local STEREO = MODE == "stereo" or MODE == "simulator"
local CAPTURE = os.getenv("ENGINE_CAPTURE") == "1"
local FRAME = os.getenv("ENGINE_FRAME") == "1"
local SCALE = tonumber(os.getenv("ENGINE_VIEW_SCALE")) or units.VRMOD_VIEW_SCALE
local IPD = coords.DEFAULT_IPD_METERS
local EYE_H = 64

-- Capture fixture. Same numbers as pure.coords.stereo_disparity_px.
local CAP_W, CAP_H = 640, 480
local CAP_FOV_Y = 90
local CAP_DEPTH = 2

local player
local world
local crate
local acc = 0
local shader
local left_tex, right_tex
local eye_pass = {}
local map_mesh
local map_name

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
	pass:setShader(shader)
	if map_mesh then
		pass:setColor(1, 1, 1)
		pass:mesh(map_mesh)
	else
		-- Visual floor is smaller than the collision brush. A 125 m box
		-- blows LÖVR's GPU buffer pool when drawn into extra passes.
		draw_source_box(pass, 0, 0, -8, 512, 512, 8, 0.25, 0.42, 0.28)
		draw_source_box(pass, 200, 80, 64, 8, 8, 64, 0.25, 0.35, 0.7)
	end
	if crate then
		local p, h = crate.pos, crate.half
		draw_source_box(pass, p.x, p.y, p.z, h.x, h.y, h.z, 0.85, 0.45, 0.12)
	end
end

local function draw_marker(pass)
	pass:setShader(shader)
	pass:setColor(1, 0, 0)
	pass:sphere(0, 0, -CAP_DEPTH, 0.12)
end

local function render_target(slot, tex, x, y, z, q, fov, draw)
	lovr.graphics.setBackgroundColor(0.02, 0.02, 0.05)
	local pass = eye_pass[slot]
	if not pass then
		pass = lovr.graphics.newPass(tex)
		eye_pass[slot] = pass
	else
		pass:reset()
	end
	if q then
		pass:setViewPose(1, x, y, z, q)
	else
		pass:setViewPose(1, x, y, z, 0, 0, 1, 0)
	end
	set_proj(pass, 1, fov, tex:getWidth(), tex:getHeight())
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
	pass:setShader(shader)
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
		{ name = "lovrPosition", type = "vec3" },
		{ name = "lovrVertexColor", type = "vec4" },
	}, verts)
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
	local path = os.getenv("ENGINE_MAP")
	if not path or path == "" then
		return false
	end
	local ok, loaded = pcall(bsp.load, path, { mesh = true })
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
		print(string.format("spawn %.1f %.1f %.1f", nudged.x, nudged.y, nudged.z))
	end
	player.on_ground = false
	if loaded.mesh and #loaded.mesh > 0 then
		local mok, mesh = pcall(upload_map, loaded.mesh)
		if mok then
			map_mesh = mesh
			map_name = path
			print(string.format("map tris %d", loaded.tri_count))
		else
			print("map mesh upload failed: " .. tostring(mesh))
		end
	end
	return true
end

function lovr.load()
	shader = lovr.graphics.newShader([[
		vec4 lovrmain() { return DefaultPosition; }
	]], [[
		vec4 lovrmain() { return Color * getPixel(ColorTexture, UV); }
	]])
	if CAPTURE or FRAME or STEREO then
		local function eye_tex(w, h)
			return lovr.graphics.newTexture(w, h, {
				usage = { "render", "transfer", "sample" },
				mipmaps = false,
			})
		end
		left_tex = eye_tex(CAP_W, CAP_H)
		right_tex = eye_tex(CAP_W, CAP_H)
	end
	if not try_map() then
		builtin_world()
	end
	print(string.format(
		"engine mode=%s stereo=%s capture=%s frame=%s views=%s scale=%.3f",
		MODE, tostring(STEREO), tostring(CAPTURE), tostring(FRAME),
		tostring(lovr.headset.getViewCount()), SCALE
	))
end

function lovr.update(dt)
	if CAPTURE or not player then
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
	if STEREO then
		local q = view_quat(player.yaw, player.pitch)
		local lx, ly, lz = eye_xyz(-1)
		local rx, ry, rz = eye_xyz(1)
		render_target("L", left_tex, lx, ly, lz, q, 74, draw_world)
		render_target("R", right_tex, rx, ry, rz, q, 74, draw_world)
		lovr.graphics.wait()
		draw_sbs(pass)
		return
	end
	lovr.graphics.setBackgroundColor(0.45, 0.62, 0.78)
	local x, y, z = eye_xyz(0)
	pass:setViewPose(1, x, y, z, view_quat(player.yaw, player.pitch))
	set_proj(pass, 1, 74, pass:getWidth(), pass:getHeight())
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
