-- InfMap terrain IS solid magenta (#FF00FF) for this map — not flatgrass, not black.
-- Always on from map load so the world never starts black then "turns" magenta.

InfMap.megachunk_size = 10
InfMap.render_distance = 2
InfMap.render_max_height = 100
InfMap.filter = InfMap.filter or {}
InfMap.filter.infmap_terrain_render = true
-- Solid magenta unlit material (cube_home/pt_void)
InfMap.terrain_material = "cube_home/pt_void"
InfMap.client_chunks = InfMap.client_chunks or {}

local MAGENTA = Color(255, 0, 255)
local last_mega_chunk

local function forceMagentaMat(e)
	if not IsValid(e) then return end
	e:SetMaterial("cube_home/pt_void")
	e:SetColor(MAGENTA)
	e:SetRenderMode(RENDERMODE_NORMAL)
	e:SetNoDraw(false)
end

-- Flat infinite magenta ground (height already 0 in sh_collider_functions).
hook.Add("PropUpdateChunk", "cube_home_magenta_terrain", function(ent, chunk, old_chunk)
	if ent ~= LocalPlayer() then return end
	if chunk[3] > InfMap.render_max_height then return end

	local _, mega_chunk = InfMap.localize_vector(chunk, InfMap.megachunk_size)
	mega_chunk[3] = 0
	local chunk_scale = InfMap.chunk_size * 2
	local delta_chunk = mega_chunk - (last_mega_chunk or mega_chunk)
	local chunk_alloc = table.Copy(InfMap.client_chunks)
	local time = 0

	for y = -InfMap.render_distance, InfMap.render_distance do
		InfMap.client_chunks[y] = InfMap.client_chunks[y] or {}
		for x = -InfMap.render_distance, InfMap.render_distance do
			if math.abs(x - delta_chunk[1]) > InfMap.render_distance
				or math.abs(y - delta_chunk[2]) > InfMap.render_distance then
				SafeRemoveEntity(InfMap.client_chunks[y][x])
				InfMap.client_chunks[y][x] = nil
			end

			if chunk_alloc[y + delta_chunk[2]] and chunk_alloc[y + delta_chunk[2]][x + delta_chunk[1]] then
				InfMap.client_chunks[y][x] = chunk_alloc[y + delta_chunk[2]][x + delta_chunk[1]]
			else
				InfMap.client_chunks[y][x] = nil
			end

			if not IsValid(InfMap.client_chunks[y][x]) then
				local e = ents.CreateClientside("infmap_terrain_render")
				if not IsValid(e) then continue end
				e:Spawn()
				e:SetAngles(Angle())
				forceMagentaMat(e)
				if InfMap.height_function then
					e:GenerateMesh(InfMap.height_function, (Vector(x, y, 0) + mega_chunk) * InfMap.megachunk_size * 2, time)
				end
				e.CHUNK_OFFSET = Vector(x, y, 0) + mega_chunk
				InfMap.client_chunks[y][x] = e
				time = time + 0.01
			else
				forceMagentaMat(InfMap.client_chunks[y][x])
			end

			local e = InfMap.client_chunks[y][x]
			if e.RENDER_MESH then
				e.RENDER_MESH.Matrix:SetTranslation((e.CHUNK_OFFSET * InfMap.megachunk_size * 2 - chunk) * chunk_scale)
			end
		end
	end
	last_mega_chunk = mega_chunk
end)

-- Keep material forced every frame (addons/URL mats can stomp).
hook.Add("Think", "cube_home_magenta_force", function()
	if (FrameNumber() % 30) ~= 0 then return end
	for y, row in pairs(InfMap.client_chunks or {}) do
		if istable(row) then
			for x, e in pairs(row) do
				forceMagentaMat(e)
			end
		end
	end
end)

-- Camera-relative magenta shell (NOT world-scale 5e7 — that loses precision per-eye
-- and left upper FOV black on the right eye). Drawn ignore-Z so void always fills.
local S = 4096
local function addQuad(tris, a, b, c, d)
	tris[#tris + 1] = { pos = a, normal = Vector(0, 0, 1), u = 0, v = 0 }
	tris[#tris + 1] = { pos = b, normal = Vector(0, 0, 1), u = 1, v = 0 }
	tris[#tris + 1] = { pos = c, normal = Vector(0, 0, 1), u = 1, v = 1 }
	tris[#tris + 1] = { pos = a, normal = Vector(0, 0, 1), u = 0, v = 0 }
	tris[#tris + 1] = { pos = c, normal = Vector(0, 0, 1), u = 1, v = 1 }
	tris[#tris + 1] = { pos = d, normal = Vector(0, 0, 1), u = 0, v = 1 }
end
local boxTris = {}
-- Inward-facing box around origin; translated to EyePos each frame.
addQuad(boxTris, Vector(-S, -S, -S), Vector(S, -S, -S), Vector(S, S, -S), Vector(-S, S, -S))
addQuad(boxTris, Vector(-S, -S, S), Vector(-S, S, S), Vector(S, S, S), Vector(S, -S, S))
addQuad(boxTris, Vector(-S, -S, -S), Vector(-S, -S, S), Vector(S, -S, S), Vector(S, -S, -S))
addQuad(boxTris, Vector(S, S, -S), Vector(S, S, S), Vector(-S, S, S), Vector(-S, S, -S))
addQuad(boxTris, Vector(-S, -S, -S), Vector(-S, S, -S), Vector(-S, S, S), Vector(-S, -S, S))
addQuad(boxTris, Vector(S, -S, -S), Vector(S, -S, S), Vector(S, S, S), Vector(S, S, -S))
local magentaBox = Mesh()
magentaBox:BuildFromTriangles(boxTris)

local plane_mat
local function drawMagentaBox()
	plane_mat = plane_mat or Material("cube_home/pt_void")
	if plane_mat:IsError() then plane_mat = Material("vgui/white") end
	local origin = EyePos()
	local m = Matrix()
	m:Translate(origin)
	cam.PushModelMatrix(m)
	-- Inside the shell: disable cull so both windings fill the FOV.
	render.CullMode(MATERIAL_CULLMODE_NONE)
	render.OverrideDepthEnable(true, false)
	cam.IgnoreZ(true)
	render.SetMaterial(plane_mat)
	render.ResetModelLighting(2, 2, 2)
	render.SetLocalModelLights()
	render.SetColorModulation(1, 0, 1)
	magentaBox:Draw()
	render.SetColorModulation(1, 1, 1)
	cam.IgnoreZ(false)
	render.OverrideDepthEnable(false, false)
	render.CullMode(MATERIAL_CULLMODE_CCW)
	cam.PopModelMatrix()
end

hook.Add("PostDraw2DSkyBox", "cube_home_magenta_skybox", drawMagentaBox)
-- Per-eye opaque pass: fills upper FOV if sky path skipped on right eye.
hook.Add("PreDrawOpaqueRenderables", "cube_home_magenta_preopaque", function()
	drawMagentaBox()
end)

hook.Add("InitPostEntity", "cube_home_magenta_boot", function()
	RunConsoleCommand("r_3dsky", "0")
	-- Keep skybox path alive so PreDrawSkyBox runs for both VR eyes.
	RunConsoleCommand("r_drawskybox", "1")
	RunConsoleCommand("fog_override", "1")
	RunConsoleCommand("fog_color", "255", "0", "255")
	RunConsoleCommand("fog_start", "99999")
	RunConsoleCommand("fog_end", "99999")
end)
