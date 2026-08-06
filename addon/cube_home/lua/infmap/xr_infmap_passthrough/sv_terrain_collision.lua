-- Home map collision only — NEVER visible furniture/cubes (those looked like "exploded sim").
-- InfMap still needs chunk colliders; they stay EF_NODRAW / RENDERMODE_NONE.

InfMap.chunk_table = InfMap.chunk_table or {}

local function makeInvisibleSolid(e)
	if not IsValid(e) then return end
	e:SetNoDraw(true)
	e:DrawShadow(false)
	e:SetRenderMode(RENDERMODE_NONE)
	e:SetColor(Color(0, 0, 0, 0))
	e.CubeHomeInvisibleCollider = true
end

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
		-- Tiny invisible hull — never couch / hunter cubes on screen
		e:SetModel("models/hunter/plates/plate025x025.mdl")
		e:Spawn()
		makeInvisibleSolid(e)
		InfMap.chunk_table[InfMap.ezcoord(chunk)] = e
	end
end

local function resetAll()
	-- Invisible walk floor under the plaza (no debugwhite cubes)
	local e = ents.Create("prop_physics")
	if not IsValid(e) then return end
	e:InfMap_SetPos(Vector(0, 0, -4))
	e:SetModel("models/hunter/plates/plate8x8.mdl")
	e:Spawn()
	makeInvisibleSolid(e)
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
		SafeRemoveEntity(ent)
	end
end)

hook.Add("InitPostEntity", "cube_home_terrain_init", resetAll)
hook.Add("PostCleanupMap", "cube_home_cleanup", resetAll)
