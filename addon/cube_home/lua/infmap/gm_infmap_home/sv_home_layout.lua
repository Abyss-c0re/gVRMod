-- Dynamic home layout: spawn props/zones from layout.json; reload without map change.

CubeHome = CubeHome or {}
CubeHome._spawned = CubeHome._spawned or {}

local function clearSpawned()
	for _, ent in ipairs(CubeHome._spawned) do
		if IsValid(ent) then SafeRemoveEntity(ent) end
	end
	CubeHome._spawned = {}
end

local function track(ent)
	if IsValid(ent) then
		ent.CubeHomeManaged = true
		table.insert(CubeHome._spawned, ent)
	end
	return ent
end

local function applyColor(ent, c)
	if not IsValid(ent) or not istable(c) then return end
	local r = tonumber(c[1] or c.r) or 255
	local g = tonumber(c[2] or c.g) or 255
	local b = tonumber(c[3] or c.b) or 255
	local a = tonumber(c[4] or c.a) or 255
	ent:SetColor(Color(r, g, b, a))
	ent:SetRenderMode(RENDERMODE_TRANSCOLOR)
end

local function spawnProp(spec)
	if not istable(spec) or not isstring(spec.model) or spec.model == "" then return end
	local ent = ents.Create("prop_physics")
	if not IsValid(ent) then return end
	ent:SetModel(spec.model)
	local pos = CubeHome.Vec(spec.pos, Vector(0, 0, 0))
	local ang = CubeHome.Ang(spec.ang, Angle(0, 0, 0))
	if ent.InfMap_SetPos then
		ent:InfMap_SetPos(pos)
	else
		ent:SetPos(pos)
	end
	ent:SetAngles(ang)
	if isstring(spec.material) and spec.material ~= "" then
		ent:SetMaterial(spec.material)
	end
	ent:Spawn()
	ent:Activate()
	local phys = ent:GetPhysicsObject()
	if IsValid(phys) and (spec.frozen ~= false) then
		phys:EnableMotion(false)
	end
	applyColor(ent, spec.color)
	if InfMap and InfMap.prop_update_chunk then
		InfMap.prop_update_chunk(ent, Vector())
	end
	if isstring(spec.id) then ent.CubeHomeId = spec.id end
	return track(ent)
end

local function spawnZoneMarker(zone)
	if not istable(zone) then return end
	local pos = CubeHome.Vec(zone.pos, Vector(0, 0, 8))
	-- Lightweight marker: small plate + optional lamp for VR visibility
	local plate = ents.Create("prop_physics")
	if not IsValid(plate) then return end
	plate:SetModel("models/hunter/plates/plate1x1.mdl")
	if plate.InfMap_SetPos then plate:InfMap_SetPos(pos) else plate:SetPos(pos) end
	plate:SetAngles(Angle(0, 0, 0))
	plate:Spawn()
	local phys = plate:GetPhysicsObject()
	if IsValid(phys) then phys:EnableMotion(false) end
	applyColor(plate, zone.color or { 200, 200, 220 })
	if InfMap and InfMap.prop_update_chunk then
		InfMap.prop_update_chunk(plate, Vector())
	end
	plate.CubeHomeZone = zone.id or zone.label or "zone"
	track(plate)
end

function CubeHome.RebuildLayout(reason)
	if not CubeHome.IsHomeMap() then return false, "not home map" end
	local layout, src = CubeHome.LoadLayoutFromDisk()
	CubeHome.Layout = layout
	clearSpawned()

	for _, p in ipairs(layout.props or {}) do
		spawnProp(p)
	end
	-- Zone markers only for non-spawn kinds (spawn is player teleport target)
	for _, z in ipairs(layout.zones or {}) do
		if z.kind == "platform" or z.kind == "marker" then
			spawnZoneMarker(z)
		end
	end

	print(string.format("[cube_home] layout rebuilt (%s) props=%d zones=%d reason=%s",
		src or "?", #(layout.props or {}), #(layout.zones or {}), tostring(reason or "manual")))
	hook.Run("CubeHome_LayoutRebuilt", layout, src)
	return true, src
end

function CubeHome.TeleportPlayerHome(ply)
	if not IsValid(ply) or not ply:IsPlayer() then return end
	local layout = CubeHome.Layout or CubeHome.DefaultLayout()
	local sp = layout.spawn or {}
	local pos = CubeHome.Vec(sp.pos, Vector(0, 0, 48))
	local ang = CubeHome.Ang(sp.ang, Angle(0, 90, 0))
	if ply.InfMap_SetPos then
		ply:InfMap_SetPos(pos)
	else
		ply:SetPos(pos)
	end
	ply:SetEyeAngles(ang)
	ply:SetLocalVelocity(Vector(0, 0, 0))
	if InfMap and InfMap.prop_update_chunk then
		InfMap.prop_update_chunk(ply, Vector())
	end
end

function CubeHome.AddPropAtPlayer(ply, model)
	if not IsValid(ply) then return false, "no player" end
	model = model or "models/hunter/plates/plate2x2.mdl"
	local layout = CubeHome.Layout or CubeHome.LoadLayoutFromDisk()
	layout.props = layout.props or {}
	local pos = ply:GetPos() + Vector(0, 0, 4)
	local ang = Angle(0, ply:EyeAngles().y, 0)
	local id = "user_" .. tostring(os.time()) .. "_" .. tostring(math.random(1000, 9999))
	table.insert(layout.props, {
		id = id,
		model = model,
		pos = { pos.x, pos.y, pos.z },
		ang = { ang.p, ang.y, ang.r },
		frozen = true,
	})
	CubeHome.SaveLayout(layout)
	CubeHome.RebuildLayout("add_prop")
	return true, id
end

hook.Add("InitPostEntity", "cube_home_layout_boot", function()
	if not CubeHome.IsHomeMap() then return end
	-- Seed editable layout on first visit so users can tweak JSON without hunting defaults.
	if not file.Exists(CubeHome.LAYOUT_FILE, "DATA") then
		CubeHome.SaveLayout(CubeHome.DefaultLayout())
		print("[cube_home] seeded data/cube_home/layout.json from defaults")
	end
	timer.Simple(0.5, function()
		CubeHome.RebuildLayout("InitPostEntity")
	end)
end)

hook.Add("PostCleanupMap", "cube_home_layout_cleanup", function()
	if not CubeHome.IsHomeMap() then return end
	timer.Simple(0.25, function()
		CubeHome.RebuildLayout("PostCleanupMap")
	end)
end)

hook.Add("PlayerInitialSpawn", "cube_home_spawn", function(ply)
	if not CubeHome.IsHomeMap() then return end
	timer.Simple(1, function()
		if IsValid(ply) then CubeHome.TeleportPlayerHome(ply) end
	end)
end)

hook.Add("PlayerSpawn", "cube_home_respawn", function(ply)
	if not CubeHome.IsHomeMap() then return end
	timer.Simple(0.1, function()
		if IsValid(ply) then CubeHome.TeleportPlayerHome(ply) end
	end)
end)

concommand.Add("cube_home_reload", function(ply)
	if IsValid(ply) and not ply:IsSuperAdmin() and not game.SinglePlayer() then return end
	local ok, src = CubeHome.RebuildLayout("console")
	if IsValid(ply) then
		ply:ChatPrint(ok and ("[cube_home] reloaded (" .. tostring(src) .. ")") or "[cube_home] reload failed")
	end
end)

concommand.Add("cube_home_save", function(ply)
	if IsValid(ply) and not ply:IsSuperAdmin() and not game.SinglePlayer() then return end
	local ok, err = CubeHome.SaveLayout(CubeHome.Layout)
	if IsValid(ply) then
		ply:ChatPrint(ok and "[cube_home] layout saved to data/cube_home/layout.json" or ("[cube_home] save failed: " .. tostring(err)))
	end
end)

concommand.Add("cube_home_reset", function(ply)
	if IsValid(ply) and not ply:IsSuperAdmin() and not game.SinglePlayer() then return end
	CubeHome.SaveLayout(CubeHome.DefaultLayout())
	CubeHome.RebuildLayout("reset")
	if IsValid(ply) then ply:ChatPrint("[cube_home] reset to default layout") end
end)

concommand.Add("cube_home_add_prop", function(ply, _, args)
	if not IsValid(ply) then return end
	if not ply:IsSuperAdmin() and not game.SinglePlayer() then return end
	local model = args[1]
	local ok, id = CubeHome.AddPropAtPlayer(ply, model)
	ply:ChatPrint(ok and ("[cube_home] added " .. tostring(id)) or "[cube_home] add failed")
end)

concommand.Add("cube_home_set_spawn", function(ply)
	if not IsValid(ply) then return end
	if not ply:IsSuperAdmin() and not game.SinglePlayer() then return end
	local layout = CubeHome.Layout or CubeHome.LoadLayoutFromDisk()
	local pos = ply:GetPos()
	local ang = ply:EyeAngles()
	layout.spawn = {
		pos = { pos.x, pos.y, pos.z },
		ang = { ang.p, ang.y, ang.r },
	}
	CubeHome.SaveLayout(layout)
	ply:ChatPrint(string.format("[cube_home] spawn set to %.0f %.0f %.0f", pos.x, pos.y, pos.z))
end)

concommand.Add("cube_home_goto", function(ply, _, args)
	if not IsValid(ply) then return end
	local zoneId = args[1]
	if not zoneId or zoneId == "" or zoneId == "spawn" then
		CubeHome.TeleportPlayerHome(ply)
		return
	end
	local layout = CubeHome.Layout or {}
	for _, z in ipairs(layout.zones or {}) do
		if z.id == zoneId then
			local pos = CubeHome.Vec(z.pos, Vector(0, 0, 48))
			pos.z = pos.z + 40
			if ply.InfMap_SetPos then ply:InfMap_SetPos(pos) else ply:SetPos(pos) end
			ply:ChatPrint("[cube_home] → " .. tostring(z.label or zoneId))
			return
		end
	end
	ply:ChatPrint("[cube_home] unknown zone: " .. tostring(zoneId))
end)
