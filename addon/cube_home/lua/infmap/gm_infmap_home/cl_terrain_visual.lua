-- Client terrain visuals for gVRMod Home (flat plaza + soft hills).

InfMap.megachunk_size = 10
InfMap.render_distance = 2
InfMap.render_max_height = 100
InfMap.filter = InfMap.filter or {}
InfMap.filter.infmap_terrain_render = true
InfMap.terrain_material = "infmap/flatgrass"

local last_mega_chunk
InfMap.client_chunks = InfMap.client_chunks or {}

hook.Add("PropUpdateChunk", "cube_home_terrain_init", function(ent, chunk, old_chunk)
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
				e:SetMaterial(InfMap.terrain_material)
				if InfMap.height_function then
					e:GenerateMesh(InfMap.height_function, (Vector(x, y, 0) + mega_chunk) * InfMap.megachunk_size * 2, time)
				end
				e.CHUNK_OFFSET = Vector(x, y, 0) + mega_chunk
				InfMap.client_chunks[y][x] = e
				time = time + 0.01
			end

			local e = InfMap.client_chunks[y][x]
			if not e.RENDER_MESH then continue end
			e.RENDER_MESH.Matrix:SetTranslation((e.CHUNK_OFFSET * InfMap.megachunk_size * 2 - chunk) * chunk_scale)
		end
	end

	last_mega_chunk = mega_chunk
end)

local chunksize = Vector(1, 1, 0) * InfMap.chunk_size * InfMap.megachunk_size * 2
local switch = false
hook.Add("RenderScene", "cube_home_update_renderbounds", function(eyePos)
	local invalid = (LocalPlayer().CHUNK_OFFSET or Vector())[3] > InfMap.render_max_height
	if invalid and switch then return end
	switch = false
	for y = -InfMap.render_distance, InfMap.render_distance do
		if not InfMap.client_chunks[y] then continue end
		for x = -InfMap.render_distance, InfMap.render_distance do
			local chunk = InfMap.client_chunks[y][x]
			if not IsValid(chunk) or not chunk.RENDER_MESH then continue end
			chunk:SetLocalRenderBounds(eyePos, chunksize)
			chunk:SetNoDraw(invalid)
			chunk:SetMaterial(InfMap.terrain_material)
		end
	end
	if invalid then switch = true end
end)

-- Distant ground plane so the plaza never feels like a void.
local size = 1000000000
local uvsize = 100000
local min = -100000
local big_plane = Mesh()
big_plane:BuildFromTriangles({
	{ pos = Vector(size, size, min), normal = Vector(0, 0, 1), u = uvsize, v = 0, tangent = Vector(1, 0, 0), userdata = { 1, 0, 0, -1 } },
	{ pos = Vector(size, -size, min), normal = Vector(0, 0, 1), u = uvsize, v = uvsize, tangent = Vector(1, 0, 0), userdata = { 1, 0, 0, -1 } },
	{ pos = Vector(-size, -size, min), normal = Vector(0, 0, 1), u = 0, v = uvsize, tangent = Vector(1, 0, 0), userdata = { 1, 0, 0, -1 } },
	{ pos = Vector(size, size, min), normal = Vector(0, 0, 1), u = uvsize, v = 0, tangent = Vector(1, 0, 0), userdata = { 1, 0, 0, -1 } },
	{ pos = Vector(-size, -size, min), normal = Vector(0, 0, 1), u = 0, v = uvsize, tangent = Vector(1, 0, 0), userdata = { 1, 0, 0, -1 } },
	{ pos = Vector(-size, size, min), normal = Vector(0, 0, 1), u = 0, v = 0, tangent = Vector(1, 0, 0), userdata = { 1, 0, 0, -1 } },
})

local default_mat = Material(InfMap.terrain_material)
local plane_matrix = Matrix()
hook.Add("PostDraw2DSkyBox", "cube_home_terrain_skybox", function()
	render.OverrideDepthEnable(true, false)
	render.SetMaterial(default_mat)
	render.ResetModelLighting(2, 2, 2)
	render.SetLocalModelLights()
	local offset = Vector(LocalPlayer().CHUNK_OFFSET or Vector())
	offset[1] = offset[1] % 1000
	offset[2] = offset[2] % 1000
	default_mat:SetFloat("$alpha", 1)
	plane_matrix:SetTranslation(InfMap.unlocalize_vector(Vector(), -offset))
	cam.PushModelMatrix(plane_matrix)
	big_plane:Draw()
	cam.PopModelMatrix()
	render.OverrideDepthEnable(false, false)
end)
