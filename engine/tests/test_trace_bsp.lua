return function(T)
	local trace = require("pure.trace")
	local bsp = require("pure.bsp")
	local move = require("pure.move")

	-- Wider than one second at gmod maxspeed 200, plus the 16-unit hull.
	-- A 256-wide pad ends under the player and the fall is real.
	local floor = trace.box_brush(-2048, -2048, -16, 2048, 2048, 0)
	local world = { brushes = { floor } }
	local start = { x = 0, y = 0, z = 64 }
	local dest = { x = 0, y = 0, z = -64 }
	local tr = trace.hull(world, start, dest)
	T.ok(tr.hit, "falls onto the floor")
	T.near(tr.normal.z, 1, 1e-6, "floor normal")
	-- Origin reaches z = 0 halfway down the 128 unit segment.
	T.near(tr.fraction, 0.5, 1e-3, "hit fraction")
	T.ok(not trace.startsolid(world, { x = 0, y = 0, z = 0 }), "standing on the floor is not stuck")
	T.ok(trace.startsolid(world, { x = 0, y = 0, z = -2 }), "inside the floor is stuck")

	-- Walk +jump on the synthetic floor stays on it and moves +X.
	local st = move.new_player("gmod")
	st.pos = { x = 0, y = 0, z = 0 }
	st.on_ground = true
	st.forward_move = 200
	st.yaw = 0
	for _ = 1, 66 do
		move.tick(st, 1 / 66, world)
	end
	T.ok(st.pos.x > 50, "walked forward, x=" .. st.pos.x)
	T.near(st.pos.z, 0, 1.5, "stayed on the floor")
	T.ok(st.on_ground, "still grounded")

	st = move.new_player("gmod")
	st.pos = { x = 0, y = 0, z = 0 }
	st.on_ground = true
	st.jump = true
	local left = false
	for _ = 1, 5 do
		move.tick(st, 1 / 66, world)
		if not st.on_ground and st.pos.z > 1 then
			left = true
		end
	end
	T.ok(left, "jump clears the floor")

	local gmod = os.getenv("ENGINE_GMOD")
		or "/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod"
	local maps = {
		{ gmod .. "/maps/gm_flatgrass.bsp", true },
		{ gmod .. "/maps/gm_construct.bsp", true },
		{ gmod .. "/maps/xr_infmap_passthrough.bsp", false },
		{ gmod .. "/addons/cube_ws_2049617805/maps/gm_novenka.bsp", true },
		{ gmod .. "/addons/cube_ws_2928366506/maps/gm_infmap_vicecity.bsp", false },
		{ gmod .. "/addons/cube_ws_3801628931/maps/gm_postal2_suburbs.bsp", false },
		{ gmod .. "/addons/cube_ws_2905327911/maps/gm_infmap.bsp", false },
	}
	local loaded = 0
	for i = 1, #maps do
		local path, expect_ground = maps[i][1], maps[i][2]
		local f = io.open(path, "rb")
		if not f then
			T.ok(false, "missing map " .. path)
		else
			f:close()
			local world = bsp.load(path, { mesh = false })
			loaded = loaded + 1
			T.eq(world.version, 20, "vbsp 20 " .. path)
			T.ok(world.brush_count > 0, "brushes " .. path)
			if #world.spawns == 0 then
				T.ok(path:find("postal", 1, true) ~= nil, "only postal has no spawn")
			else
				local s = world.spawns[1]
				if path:find("gm_construct.bsp", 1, true) then
					T.eq(s.ayaw, 180, "construct spawn yaw")
					-- The color-room func_brush is a slab in entity-local space.
					-- Left at the origin it seals the air over this spawn.
					T.ok(not trace.startsolid(world, { x = s.ox, y = s.oy, z = 0 }), "construct air above spawn is open")
				end
				local pos = { x = s.ox, y = s.oy, z = s.oz }
				if expect_ground then
					local down = trace.hull(world, pos, { x = pos.x, y = pos.y, z = pos.z - 64 })
					T.ok(down.hit, "ground under spawn " .. path)
					if down.hit then
						T.ok(down.normal.z > 0.7, "walkable normal " .. path)
						local drop = pos.z - down.endpos.z
						T.ok(drop < 8, "spawn is on the floor (" .. drop .. ") " .. path)
					end
				else
					local freed, dz = trace.nudge_up(world, pos)
					T.ok(freed ~= nil, "nudge " .. path)
					if dz and dz > 0 then
						T.ok(not trace.startsolid(world, freed), "nudge cleared " .. path)
					end
				end
			end
		end
	end
	T.ok(loaded == #maps, "every listed map loaded")

	-- One mesh build: flatgrass must produce triangles (faces + displacements).
	local flat = gmod .. "/maps/gm_flatgrass.bsp"
	if io.open(flat, "rb") then
		local world = bsp.load(flat, { mesh = true })
		T.ok(world.tri_count > 1000, "flatgrass mesh " .. tostring(world.tri_count))
		T.ok(world.disp_count == 16, "flatgrass disps")
	end
end
