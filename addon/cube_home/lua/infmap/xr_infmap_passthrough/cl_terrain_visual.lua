-- XR Home Passthrough — no flatgrass mesh by default (void + green key).

InfMap.megachunk_size = 10
InfMap.render_distance = 0
InfMap.render_max_height = 100
InfMap.filter = InfMap.filter or {}
InfMap.filter.infmap_terrain_render = true
InfMap.terrain_material = "infmap/flatgrass"
InfMap.client_chunks = InfMap.client_chunks or {}

local cvTerrain = CreateClientConVar("cube_home_draw_terrain", "0", true, FCVAR_ARCHIVE,
	"Draw InfMap height mesh. Off = void AR hub (recommended).")

hook.Add("PropUpdateChunk", "cube_home_terrain_init", function(ent, chunk, old_chunk)
	if ent ~= LocalPlayer() then return end
	if not cvTerrain:GetBool() then
		for _, row in pairs(InfMap.client_chunks) do
			if istable(row) then
				for _, e in pairs(row) do
					if IsValid(e) then SafeRemoveEntity(e) end
				end
			end
		end
		InfMap.client_chunks = {}
		return
	end

	InfMap.render_distance = 1
	if chunk[3] > InfMap.render_max_height then return end
	local _, mega_chunk = InfMap.localize_vector(chunk, InfMap.megachunk_size)
	mega_chunk[3] = 0
	local chunk_scale = InfMap.chunk_size * 2
	for y = -InfMap.render_distance, InfMap.render_distance do
		InfMap.client_chunks[y] = InfMap.client_chunks[y] or {}
		for x = -InfMap.render_distance, InfMap.render_distance do
			if not IsValid(InfMap.client_chunks[y][x]) then
				local e = ents.CreateClientside("infmap_terrain_render")
				if not IsValid(e) then continue end
				e:Spawn()
				e:SetAngles(Angle())
				e:SetMaterial(InfMap.terrain_material)
				if InfMap.height_function then
					e:GenerateMesh(InfMap.height_function, (Vector(x, y, 0) + mega_chunk) * InfMap.megachunk_size * 2, 0)
				end
				e.CHUNK_OFFSET = Vector(x, y, 0) + mega_chunk
				InfMap.client_chunks[y][x] = e
			end
			local e = InfMap.client_chunks[y][x]
			if e.RENDER_MESH then
				e.RENDER_MESH.Matrix:SetTranslation((e.CHUNK_OFFSET * InfMap.megachunk_size * 2 - chunk) * chunk_scale)
			end
		end
	end
end)
