-- Terrain colliders for home map (adapted from InfMap base gm_infmap by Meetric).
-- Upstream: https://github.com/meetric1/gmod-infinite-map

InfMap.chunk_table = InfMap.chunk_table or {}

local function try_invalid_chunk(chunk, filter)
	if not chunk then return end
	local invalid = InfMap.chunk_table[InfMap.ezcoord(chunk)]
	for _, v in ipairs(ents.GetAll()) do
		if InfMap.filter_entities(v) or not v:IsSolid() or v == filter then continue end
		if v.CHUNK_OFFSET == chunk then
			invalid = nil
			break
		end
	end
	SafeRemoveEntity(invalid)
end

local function update_chunk(ent, chunk, oldchunk)
	if IsValid(ent) and not InfMap.filter_entities(ent) and ent:IsSolid() then
		try_invalid_chunk(oldchunk)
		if IsValid(InfMap.chunk_table[InfMap.ezcoord(chunk)]) then return end

		local e = ents.Create("infmap_terrain_collider")
		if not IsValid(e) then return end
		InfMap.prop_update_chunk(e, chunk)
		e:SetModel("models/props_c17/FurnitureCouch002a.mdl")
		e:Spawn()
		InfMap.chunk_table[InfMap.ezcoord(chunk)] = e
	end
end

local function resetAll()
	local e = ents.Create("prop_physics")
	if not IsValid(e) then return end
	e:InfMap_SetPos(Vector(0, 0, -4))
	e:SetModel("models/hunter/blocks/cube8x8x025.mdl")
	e:SetMaterial("models/debug/debugwhite")
	e:SetColor(Color(36, 40, 52))
	e:Spawn()
	local phys = e:GetPhysicsObject()
	if IsValid(phys) then phys:EnableMotion(false) end
	constraint.Weld(e, game.GetWorld(), 0, 0, 0)
	InfMap.prop_update_chunk(e, Vector())
	e.CubeHomeAnchor = true

	for _, v in ipairs(ents.GetAll()) do
		if not v.CHUNK_OFFSET then continue end
		update_chunk(v, v.CHUNK_OFFSET)
	end
end

hook.Add("EntityRemoved", "cube_home_infgen_terrain", function(ent)
	try_invalid_chunk(ent.CHUNK_OFFSET, ent)
end)

hook.Add("PropUpdateChunk", "cube_home_infgen_terrain", function(ent, chunk, oldchunk)
	update_chunk(ent, chunk, oldchunk)
	if chunk[3] <= -100 then
		print("[cube_home] Force removing stray", ent)
		SafeRemoveEntity(ent)
	end
end)

hook.Add("InitPostEntity", "cube_home_terrain_init", resetAll)
hook.Add("PostCleanupMap", "cube_home_cleanup", resetAll)
