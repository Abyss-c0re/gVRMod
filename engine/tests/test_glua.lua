-- Game Lua host. Scripts stay in the Steam install. A registered SWEP is not a fired gun.
return function(T)
	local compat = require("pure.compat")
	local glua = require("pure.glua")
	local content = require("pure.content")

	local env = compat.make_env("server")
	local src = [[
N = 0
for i = 1, 4 do
  if i == 2 then continue end
  if ( i != 0 && !false ) then N = N + i end
end
// not lua
]]
	local ok, err = compat.load_source("dialect.lua", src, env)
	T.ok(ok, "dialect load " .. tostring(err))
	T.eq(env.N, 8, "dialect N")

	local placed = {
		{ kind = "weapon", class = "weapon_pistol", height = 16, span = 20 },
		{ kind = "npc", class = "npc_alyx", height = 72, span = 24 },
		{ kind = "npc", class = "npc_combinegunship", height = 400, span = 600 },
	}
	glua.layout(placed, { x = 704, y = 132, z = -143 }, 180)
	local angles = require("pure.angles")
	local fwd, right = angles.angle_vectors(0, 180, 0)
	local function ahead_lat(it)
		local dx = it.x - 704
		local dy = it.y - 132
		return dx * fwd.x + dy * fwd.y, dx * right.x + dy * right.y
	end
	local function voff(it)
		return (it.z + (it.height or 0) / 2) - (-143 + 64)
	end
	local aw, lw = ahead_lat(placed[1])
	local an, ln = ahead_lat(placed[2])
	local ag, lg = ahead_lat(placed[3])
	T.ok(placed[1].x < 700, "weapon ahead of spawn x=" .. tostring(placed[1].x))
	T.eq(placed[1].yaw, 270, "weapon yaw")
	T.eq(placed[2].yaw % 360, 0, "npc faces +X")
	T.ok(placed[2].y < placed[1].y, "npc stands left of the gun y=" .. tostring(placed[2].y))
	T.ok(aw > 120 and lw > 0 and math.abs(lw) < aw * 1.02, "weapon in the right half " .. aw .. " " .. lw)
	T.ok(an > 120 and ln < 0 and math.abs(ln) < an * 1.02, "npc in the left half " .. an .. " " .. ln)
	T.ok(math.abs(voff(placed[1])) < aw * 0.58, "weapon inside vertical fov")
	T.ok(math.abs(voff(placed[2])) < an * 0.58, "npc inside vertical fov")
	T.ok(ag > aw + 500, "gunship farther down the street x=" .. tostring(placed[3].x))
	T.ok(math.abs(lg) < 200, "gunship stays in the street y=" .. tostring(placed[3].y))
	T.ok(math.abs(voff(placed[3])) < ag * 0.58, "gunship inside vertical fov")

	local streak = glua.streak_verts(
		{ x = 0, y = 0, z = 0, dx = 0, dy = -1, dz = 0 },
		fwd.x, fwd.y, fwd.z
	)
	T.eq(streak and #streak, 18, "streak has two triangles")
	if streak then
		local miny, maxy, minz, maxz, maxx = 0, 0, 0, 0, 0
		for i = 1, #streak, 3 do
			local ax = math.abs(streak[i])
			if ax > maxx then
				maxx = ax
			end
			if streak[i + 1] < miny then
				miny = streak[i + 1]
			end
			if streak[i + 1] > maxy then
				maxy = streak[i + 1]
			end
			if streak[i + 2] < minz then
				minz = streak[i + 2]
			end
			if streak[i + 2] > maxz then
				maxz = streak[i + 2]
			end
		end
		T.ok(maxx < 1e-6, "streak crosses the view")
		T.near(miny, -160, 1e-6, "streak length")
		T.near(maxy, 0, 1e-6, "streak starts at the bullet")
		T.near(maxz - minz, 8, 1e-6, "streak has visible height")
	end

	local sample = '"viewmodel" "models/weapons/v_pistol.mdl"\n"playermodel" "models/weapons/w_pistol.mdl"\n'
	local model, how = glua.script_model(sample)
	T.eq(model, "models/weapons/w_pistol.mdl", "pistol playermodel")
	T.eq(how, "playermodel", "pistol field")

	local gmod = os.getenv("ENGINE_GMOD")
		or "/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod"
	local tree = io.open(gmod .. "/lua/includes/modules/list.lua", "rb")
	if not tree then
		T.ok(false, "missing garrysmod lua")
		return
	end
	tree:close()

	local session = glua.boot({ gmod = gmod, map = "gm_construct" })
	print(session.summary)
	for i = 1, #session.failure_lines do
		print("fail " .. session.failure_lines[i])
	end
	T.ok(session.modules_ok, "stock modules")
	T.eq(session.env.game.GetMap(), "gm_construct", "map name")
	T.eq(session.env.SF_NPC_DROP_HEALTHKIT, 8, "healthkit flag")
	T.eq(session.env.SF_CITIZEN_MEDIC, 131072, "medic flag")
	T.eq(session.env.CT_REBEL, 3, "rebel type")
	T.eq(session.env.ACT_IDLE, 1, "ACT_IDLE")
	T.eq(session.env.ACT_RESET, 0, "ACT_RESET")
	T.eq(session.env.ACT_MP_SWIM_IDLE, 2026, "ACT_MP_SWIM_IDLE")
	T.ok(session.env.ACT_MP_SWIM_IDLE ~= session.env.ACT_HL2MP_RUN_AR2, "swim idle is its own key")
	T.ok(session.env.ACT_HL2MP_IDLE_PISTOL ~= session.env.ACT_MP_STAND_IDLE, "hold acts differ")
	T.eq(session.env.CONTENTS_SOLID, 1, "CONTENTS_SOLID")
	T.eq(session.env.FCVAR_ARCHIVE, 128, "FCVAR_ARCHIVE")
	T.eq(session.env.FCVAR_REPLICATED, 8192, "FCVAR_REPLICATED")
	T.eq(session.env.MAT_CONCRETE, 67, "MAT_CONCRETE")
	T.eq(session.env.MAT_SNOW, 74, "MAT_SNOW")
	T.eq(string.TrimRight("vrmod/", "/"), "vrmod", "TrimRight")
	T.ok(("sh_api.lua"):StartWith("sh_"), "StartWith")
	T.ok(session.env.system and session.env.system.IsLinux(), "system.IsLinux")
	T.near(session.env.engine.TickInterval(), 1 / 66, 1e-12, "engine.TickInterval")
	T.ok(session.env.IsColor(session.env.Color(10, 20, 30)), "IsColor")
	T.ok(not session.env.IsColor(session.env.Vector(1, 0, 0)), "Vector is not IsColor")
	T.ok(not session.env.IsColor("red"), "string is not IsColor")
	T.eq(string.GetFileFromFilename("lua/autorun/foo.lua"), "foo.lua", "GetFileFromFilename")
	T.eq(("a/b/a"):Replace("a", "c"), "c/b/c", "string.Replace")
	T.eq(session.env.CHAN_WEAPON, 1, "CHAN_WEAPON")
	T.eq(session.env.DMG_ENERGYBEAM, 1024, "DMG_ENERGYBEAM")
	T.eq(session.env.engine.ActiveGamemode(), "sandbox", "ActiveGamemode name")
	session.env.sound.Add({ name = "unit.fire", channel = session.env.CHAN_WEAPON, sound = "a.wav" })
	T.eq(session.env.sound.GetProperties("unit.fire").sound, "a.wav", "sound.Add stores the script")
	T.ok(not session.env.ConVarExists("unit_missing_cvar"), "ConVarExists is false until created")
	session.env.CreateConVar("unit_missing_cvar", "1")
	T.ok(session.env.ConVarExists("unit_missing_cvar"), "ConVarExists after CreateConVar")
	local left = session.env.Angle(0, 0, 0):Left()
	T.near(left.y, 1, 1e-9, "Angle:Left is -right at yaw 0")
	local ang = session.env.Angle(10, 20, 30)
	T.eq(ang.pitch, 10, "Angle.pitch")
	ang.yaw = 21
	T.eq(ang.y, 21, "Angle.yaw writes y")
	T.eq(type(session.env.player_manager), "table", "player_manager module")
	T.eq(type(session.env.player_manager.AddValidModel), "function", "AddValidModel")
	local gma_paths = content.gma_paths(content.workshop_dir(gmod))
	if #gma_paths > 0 then
		local ran_gma = session.counts.gma_autorun + session.counts.gma_weapon + session.counts.gma_ent
		T.ok(ran_gma > 0, "workshop lua entry points " .. tostring(ran_gma))
	end
	session.env.file.CreateDir("vrmod_logs")
	T.ok(session.env.file.Exists("vrmod_logs", "DATA"), "CreateDir stays in the session")
	T.eq(type(session.env.g_VR), "table", "g_VR from the loader")
	T.eq(type(session.env.GAMEMODE), "table", "GAMEMODE table")
	T.eq(session.env.MASK_SOLID, bit.bor(1, 0x4000, 2, 0x2000000, 8), "MASK_SOLID")

	local weapons = session.env.weapons
	T.ok(weapons and weapons.GetStored, "weapons.GetStored")
	if weapons and weapons.GetStored then
		local base = weapons.GetStored("weapon_base")
		T.eq(base and base.WorldModel, "models/weapons/w_357.mdl", "weapon_base world model")
		T.eq(type(base and base.GetNPCMinBurst), "function", "weapon_base burst accessor")
		local fists = weapons.GetStored("weapon_fists")
		T.eq(fists and fists.WorldModel, "", "fists have no world model")
		T.eq(fists and fists.ViewModel, "models/weapons/c_arms.mdl", "fists view model")
		local med = weapons.GetStored("weapon_medkit")
		T.eq(med and med.WorldModel, "models/weapons/w_medkit.mdl", "medkit world model")
		local tool = weapons.GetStored("gmod_tool")
		T.eq(tool and tool.WorldModel, "models/weapons/w_toolgun.mdl", "toolgun world model")
		T.ok(tool and tool.Tool and tool.Tool.axis and tool.Tool.balloon and tool.Tool.weld,
			"stools axis balloon weld")
	end

	local listed = session.env.list and session.env.list.Get and session.env.list.Get("NPC")
	T.ok(listed and listed.npc_alyx and listed.npc_alyx.Class == "npc_alyx", "alyx listed")
	T.ok(listed and listed.Rebel and listed.Rebel.Class == "npc_citizen", "rebel listed")
	if listed and listed.Rebel then
		T.eq(glua.npc_model_for(listed.Rebel), "models/Humans/Group03/male_07.mdl", "rebel model")
	end

	local mount = content.mount({ gmod = gmod })
	local text = mount:read("scripts/weapons/weapon_pistol.txt")
	T.ok(text ~= nil, "pistol script bytes")
	local from_script = glua.script_model(text)
	T.eq(from_script, "models/weapons/w_pistol.mdl", "pistol script model")
	T.eq(glua.script_damage(text), 12, "pistol script damage")
	T.eq(glua.script_damage('"damage" "0"'), nil, "zero damage is not a shot")
	T.eq(glua.script_bullets('"bullets" "6"'), 6, "script bullets")
	T.eq(glua.script_bullets(text), nil, "pistol does not invent pellets")

	local items, skipped = glua.collect(session, mount)
	print(string.format("collect %d skipped %d", #items, skipped and #skipped or 0))
	local function has(class)
		for i = 1, #items do
			if items[i].class == class then
				return items[i]
			end
		end
	end
	local med_i = has("weapon_medkit")
	T.eq(med_i and med_i.model, "models/weapons/w_medkit.mdl", "collect medkit")
	local fist_i = has("weapon_fists")
	T.eq(fist_i and fist_i.model, "models/weapons/c_arms.mdl", "collect fists view")
	T.eq(fist_i and fist_i.source, "view", "fists tagged view")
	local pist = has("weapon_pistol")
	T.eq(pist and pist.model, "models/weapons/w_pistol.mdl", "collect pistol")
	T.eq(pist and pist.script_damage, 12, "collect pistol damage")
	local alyx = has("npc_alyx")
	T.eq(alyx and alyx.model, "models/alyx.mdl", "collect alyx")
	local rebel = has("Rebel")
	T.eq(rebel and rebel.model, "models/Humans/Group03/male_07.mdl", "collect rebel")

	T.ok(mount:read("models/alyx.mdl") ~= nil, "alyx mdl")
	T.ok(mount:read("models/zombie/classic.mdl") ~= nil, "zombie mdl")
	T.ok(mount:read("models/humans/group03/male_07.mdl") ~= nil, "rebel mdl bytes")
	T.ok(mount:read("models/combine_soldier.mdl") ~= nil, "combine mdl")
	T.ok(mount:read("models/weapons/w_pistol.mdl") ~= nil, "w_pistol mdl")

	T.eq(type(session.env.FindMetaTable("Entity")), "table", "Entity meta")
	T.eq(type(session.env.FindMetaTable("Player")), "table", "Player meta")
	T.eq(session.meta_player:IsPlayer(), true, "Player:IsPlayer")
	T.eq(session.meta_player:InVehicle(), false, "player has no vehicle")
	T.eq(session.meta_weapon:IsPlayer(), false, "Weapon:IsPlayer")
	local dt = session.make_ent("weapon_base", session.meta_weapon)
	dt:NetworkVar("Bool", 0, "Reloading")
	T.eq(dt:GetReloading(), false, "NetworkVar bool default")
	dt:SetReloading(true)
	T.eq(dt:GetReloading(), true, "NetworkVar bool set")
	local share_a = session.env.util.SharedRandom("weapon_base", -0.2, -0.1, 0)
	local share_b = session.env.util.SharedRandom("weapon_base", -0.2, -0.1, 0)
	T.eq(share_a, share_b, "SharedRandom is stable")
	T.ok(share_a <= -0.1 and share_a >= -0.2, "SharedRandom stays in range")
	local rand = session.env.math.Rand(0, 1)
	T.ok(rand >= 0 and rand < 1, "math.Rand")
	T.eq(session.env.math.Clamp(5, 0, 3), 3, "math.Clamp high")
	T.eq(session.env.math.Clamp(-1, 0, 3), 0, "math.Clamp low")
	local ar = session.env.AngleRand()
	T.ok(ar and ar.p >= -90 and ar.p <= 90, "AngleRand pitch")
	T.ok(ar and ar.y >= -180 and ar.y <= 180, "AngleRand yaw")
	local addons = session.env.engine.GetAddons()
	T.eq(type(addons), "table", "GetAddons")
	if #gma_paths > 0 then
		T.ok(#addons > 0, "GetAddons lists the mounted gmas")
		T.eq(type(addons[1].title), "string", "GetAddons title")
		T.eq(addons[1].mounted, true, "GetAddons mounted")
	end

	local fired_timer = false
	session.env.timer.Simple(0, function()
		fired_timer = true
	end)
	T.ok(not fired_timer, "timer.Simple does not run at Create")

	local demo = {
		{ kind = "weapon", class = "weapon_base", x = 10, y = 20, z = 30, yaw = 270, z_off = 0 },
	}
	local shot = glua.exercise(session, demo)
	T.eq(shot.attack_ok, 1, "weapon_base primary ran")
	T.eq(shot.attack_bad, 0, "weapon_base primary error " .. table.concat(shot.lines, " | "))
	T.ok(shot.bullets >= 1, "weapon_base recorded a bullet")
	T.ok(shot.think_ok >= 2, "weapon_base init and think")
	T.eq(shot.think_bad, 0, "weapon_base think error " .. table.concat(shot.lines, " | "))
	local bullet = demo[1].bullets and demo[1].bullets[1]
	T.eq(bullet and bullet.x, 10, "bullet x is the weapon")
	T.eq(bullet and bullet.y, 20, "bullet y is the weapon")
	T.eq(bullet and bullet.z, 30, "bullet z is the mesh origin")
	T.ok(bullet and bullet.dy < -0.9, "bullet travels along the weapon aim")

	local called = false
	local client = session.make_ent("weapon_base", session.meta_weapon)
	function client:SPLastShoot()
		called = true
		self:SetLastShootTime(4)
	end
	client:CallOnClient("SPLastShoot")
	T.eq(called, true, "CallOnClient runs the method")
	T.eq(client:LastShootTime(), 4, "last shoot time stored")
	client:CallOnClient("NoSuchClientMethod")
	T.eq(called, true, "missing CallOnClient name does nothing")

	local trace_mod = require("pure.trace")
	session.trace_slot.world = {
		brushes = { trace_mod.box_brush(-2048, -2048, -16, 2048, 2048, 0) },
	}
	local tr = session.env.util.TraceLine({
		start = session.env.Vector(0, 0, 64),
		endpos = session.env.Vector(0, 0, -64),
	})
	T.eq(tr.Hit, true, "ray hits the floor")
	T.near(tr.Fraction, 0.5, 1e-3, "ray fraction")
	T.near(tr.HitNormal.z, 1, 1e-6, "floor normal")
	T.eq(tr.Entity, session.env.NULL, "ray entity is NULL")
	T.eq(tr.HitWorld, true, "ray hit the world")
	T.eq(tr.MatType, 0, "ray material unknown")
	T.near(tr.HitPos.z, 0, 0.05, "hit near the slab")
	local miss = session.env.util.TraceLine({
		start = session.env.Vector(0, 0, 64),
		endpos = session.env.Vector(0, 0, 32),
	})
	T.eq(miss.Hit, false, "short ray misses")
	T.eq(miss.Fraction, 1, "miss fraction")
	local eye_ply = session.make_ent("player", session.meta_player)
	eye_ply:SetPos(session.env.Vector(0, 0, 0))
	eye_ply:SetAngles(session.env.Angle(90, 0, 0))
	local eye = eye_ply:GetEyeTrace()
	T.eq(eye.Hit, true, "GetEyeTrace hits the floor")
	T.eq(eye_ply:GetEyeTraceNoCursor().Hit, true, "GetEyeTraceNoCursor hits the floor")
	session.trace_slot.world = nil
	local noworld = session.env.util.TraceLine({
		start = session.env.Vector(0, 0, 64),
		endpos = session.env.Vector(0, 0, -64),
	})
	T.eq(noworld.Hit, false, "no world is a miss")
	T.eq(noworld.Fraction, 1, "no world fraction")

	local arc_stored = session.env.weapons.GetStored("arcticvr_hl2_pistol")
	T.ok(arc_stored ~= nil, "arcticvr_hl2_pistol is registered")
	local demo_arc = {
		{ kind = "weapon", class = "arcticvr_hl2_pistol", x = 8, y = 9, z = 10, yaw = 270, z_off = 0 },
	}
	local ashot = glua.exercise(session, demo_arc)
	T.ok(ashot.bullets >= 1, "arcticvr pistol recorded a bullet " .. table.concat(ashot.lines, " | "))
	local ab = demo_arc[1].bullets and demo_arc[1].bullets[1]
	T.eq(ab and ab.x, 8, "arcticvr bullet x")
	T.eq(ab and ab.y, 9, "arcticvr bullet y")
	T.eq(ab and ab.z, 10, "arcticvr bullet z")
	T.near(ab and ab.damage or -1, 14, 1e-6, "arcticvr damage is the midpoint")
	T.ok(ab and ab.dy < -0.9, "arcticvr bullet follows the aim")

	local demo_p = {
		{
			kind = "weapon",
			class = "weapon_pistol",
			x = 4,
			y = 5,
			z = 6,
			yaw = 270,
			z_off = 0,
			script_damage = pist and pist.script_damage,
			script_bullets = pist and pist.script_bullets,
		},
	}
	local pshot = glua.exercise(session, demo_p)
	T.eq(pshot.bullets, 1, "pistol script fired once")
	T.eq(pshot.attack_ok, 1, "pistol script counted")
	local pb = demo_p[1].bullets and demo_p[1].bullets[1]
	T.eq(pb and pb.damage, 12, "pistol damage 12")
	T.eq(pb and pb.x, 4, "pistol bullet x")
	T.ok(pb and pb.dy < -0.9, "pistol bullet follows the aim")

	local demo_both = {
		{
			kind = "weapon",
			class = "weapon_base",
			x = 10,
			y = 20,
			z = 30,
			yaw = 270,
			z_off = 0,
			script_damage = 99,
		},
	}
	local both = glua.exercise(session, demo_both)
	T.eq(both.bullets, 1, "lua primary is not double fired")
	T.ok(both.attack_bad == 0, "lua primary still clean " .. table.concat(both.lines, " | "))
	local bb = demo_both[1].bullets and demo_both[1].bullets[1]
	T.ok(bb and bb.damage ~= 99, "script damage did not replace the lua shot")

	local reg_ok = pcall(session.env.scripted_ents.Register, {
		Type = "anim",
		Base = "base_anim",
		Model = "models/weapons/w_missile_launch.mdl",
		Initialize = function(self)
			self:SetModel(self.Model)
			self:SetSubMaterial(0)
		end,
	}, "engine_test_rocket")
	T.eq(reg_ok, true, "test rocket registered")
	local rocket = session.env.ents.Create("engine_test_rocket")
	rocket:SetPos(session.env.Vector(3, 4, 5))
	local spawn_ok, spawn_err = pcall(function()
		rocket:Spawn()
	end)
	T.eq(spawn_ok, false, "Spawn does not hide a missing method " .. tostring(spawn_err))
	T.eq(rocket.__spawned, true, "spawn marked")
	T.eq(rocket:GetModel(), "models/weapons/w_missile_launch.mdl", "model set before the miss")
	T.eq(rocket:GetPos().x, 3, "spawn keeps the position")
	T.eq(session.env.IsValid(rocket:GetPhysicsObject()), false, "physics object is invalid")
	T.eq(rocket:GetPhysicsObject():IsValid(), false, "physics method is invalid")
	T.eq(rocket:IsWorld(), false, "projectile is not the world")
	rocket:Activate()
	T.eq(rocket.__active, true, "Activate marks the entity")
	local noc = session.env.constraint and session.env.constraint.NoCollide
	if type(noc) == "function" then
		local nok, nerr = pcall(noc, rocket, rocket, 0, 0)
		T.eq(nok, true, "NoCollide returns without a body " .. tostring(nerr))
	end

	local boxent = session.env.ents.Create("prop_physics")
	boxent:SetPos(session.env.Vector(10, 20, 30))
	boxent:PhysicsInit(6)
	T.eq(session.env.IsValid(boxent:GetPhysicsObject()), false, "PhysicsInit does not fake a collide")
	T.eq(boxent:GetPhysicsObject():IsValid(), false, "PhysicsInit method stays invalid")
	boxent:PhysicsInitBox(session.env.Vector(-1, -1, -1), session.env.Vector(1, 1, 1))
	local phys = boxent:GetPhysicsObject()
	T.eq(phys:IsValid(), true, "box phys is valid")
	T.eq(session.env.IsValid(phys), true, "global IsValid agrees with the box")
	phys:SetVelocityInstantaneous(session.env.Vector(0, -100, 0))
	T.near(phys:GetVelocity().y, -100, 1e-6, "box velocity y")
	phys:EnableGravity(false)
	T.eq(phys.__body.gravity, 0, "EnableGravity false clears solver gravity")
	phys:EnableGravity(true)
	T.eq(phys.__body.gravity, 600, "EnableGravity true restores 600")
	phys:SetMass(0)
	local cooked = phys.__body.mass
	local vphysics = require("pure.vphysics")
	T.near(cooked, vphysics.mass_kg(1, 1, 1), 1e-9, "box mass matches the 2 inch solver box")
	phys:SetMass(1)
	T.eq(phys.__body.mass, 1, "SetMass writes the solver body")
	phys:SetMass(0)
	T.eq(phys.__body.mass, 1, "SetMass ignores a non-positive mass")
	local shot = glua.velocity_bullet(boxent)
	T.eq(shot and shot.x, 10, "streak starts at the box")
	T.eq(shot and shot.z, 30, "streak keeps the box height")
	T.near(shot and shot.dy, -100, 1e-6, "streak uses the phys velocity")
	boxent:SetVelocity(session.env.Vector(0, 0, 40))
	local prefer = glua.velocity_bullet(boxent)
	T.near(prefer and prefer.dy, -100, 1e-6, "phys velocity wins over the entity")
	T.near(prefer and prefer.dz, 0, 1e-6, "entity velocity does not leak into the phys streak")
	local before_y = boxent:GetPos().y
	phys:EnableGravity(false)
	phys:SetVelocityInstantaneous(session.env.Vector(0, -66, 0))
	T.eq(glua.integrate_body(boxent, session.env.engine.TickInterval()), true, "box takes one tick")
	T.near(boxent:GetPos().y, before_y - 1, 1e-6, "one tick at 66 u/s moves 1 unit")
	T.near(boxent:GetPos().z, 30, 1e-6, "gravity off does not drop the box")

	local resting = session.env.ents.Create("prop_physics")
	resting:PhysicsInitBox(session.env.Vector(-1, -1, -1), session.env.Vector(1, 1, 1))
	T.eq(glua.velocity_bullet(resting), nil, "a resting box has no streak")

	local bolt = session.env.ents.Create("crossbow_bolt")
	bolt:SetPos(session.env.Vector(1, 2, 3))
	bolt:SetVelocity(session.env.Vector(0, -3000, 0))
	T.near(bolt:GetVelocity().y, -3000, 1e-6, "entity velocity roundtrip")
	local bb = glua.velocity_bullet(bolt)
	T.eq(bb and bb.x, 1, "bolt streak x")
	T.eq(bb and bb.z, 3, "bolt streak z")
	T.near(bb and bb.dy, -3000, 1e-6, "bolt streak uses entity velocity")
	T.eq(glua.integrate_body(bolt, session.env.engine.TickInterval()), false, "entity velocity is not a box step")
	T.eq(bolt:GetPos().y, 2, "bolt stays where Spawn left it")
	local ribbon = glua.streak_verts(bb, fwd.x, fwd.y, fwd.z)
	T.eq(ribbon and #ribbon, 18, "bolt velocity is a streak")

	T.eq(session.env.COLLISION_GROUP_INTERACTIVE_DEBRIS, 3, "interactive debris group")
	T.eq(session.env.MOVETYPE_FLYGRAVITY, 5, "flygravity movetype")
	T.eq(session.env.SOLID_VPHYSICS, 6, "vphysics solid")
	T.eq(session.env.GetConVar("not_a_shipped_cvar"), nil, "GetConVar does not invent")
	local xbow = session.env.GetConVar("sk_plr_dmg_crossbow")
	T.eq(xbow and xbow:GetFloat(), 100, "skill.cfg crossbow damage")
	boxent:SetCollisionGroup(session.env.COLLISION_GROUP_PROJECTILE)
	T.eq(boxent:GetCollisionGroup(), 13, "collision group is stored")
	boxent:SetSaveValue("m_flDamage", 200)
	T.eq(boxent:GetSaveValue("m_flDamage"), 200, "save value roundtrip")
	boxent:Fire("SetDamage", "100", 0)
	T.eq(boxent.__inputs and boxent.__inputs[1].input, "SetDamage", "Fire records the input")
	T.eq(boxent.__inputs[1].param, "100", "Fire keeps the parameter")
	T.eq(boxent:GetSaveValue("m_flDamage"), 200, "Fire does not apply damage")
	local patch = session.env.CreateSound(boxent, "weapons/rpg/rocket1.wav")
	T.eq(patch:IsPlaying(), false, "sound patch starts silent")
	patch:Play()
	T.eq(patch:IsPlaying(), true, "Play records the patch")
	T.eq(boxent.__sounds and boxent.__sounds[1], "weapons/rpg/rocket1.wav", "Play records the name")
	patch:Stop()
	T.eq(patch:IsPlaying(), false, "Stop clears the patch")

	local owner = session.make_ent("player", session.meta_player)
	owner:SetPos(session.env.Vector(8, 9, 10))
	owner:SetVelocity(session.env.Vector(50, 0, 0))
	T.eq(owner:GetAbsVelocity().x, 0, "standing player abs velocity stays 0")
	T.eq(glua.velocity_bullet(owner), nil, "a still player has no streak")
	T.eq(session.env.NULL:IsPlayer(), false, "NULL is not a player")
	T.eq(session.env.NULL:IsNPC(), false, "NULL is not an npc")
	T.eq(session.env.NULL:IsValid(), false, "NULL method is invalid")
	T.eq(session.env.IsValid(session.env.NULL), false, "global IsValid still rejects NULL")
	T.eq(session.env.vector_origin.x, 0, "vector_origin x")
	T.eq(session.env.vector_origin.y, 0, "vector_origin y")
	T.eq(session.env.SCREENFADE.IN, 1, "screen fade in")
	owner:ScreenFade(session.env.SCREENFADE.IN, session.env.Color(255, 225, 205, 64), 0.1, 0)
	T.eq(owner.__fades and owner.__fades[1].flags, 1, "ScreenFade records the flag")
	T.eq(owner.__fades[1].hold, 0, "ScreenFade records the hold")
	T.eq(owner:GetInfo("gmod_toolmode"), "", "unset info is empty")
	T.eq(session.env.GetConVar("gmod_toolmode"), nil, "GetInfo does not create the cvar")
	owner:SetAngles(session.env.Angle(0, 0, 0))
	owner:SetPos(session.env.Vector(0, 0, 0))
	local look = session.env.util.GetPlayerTrace(owner)
	T.eq(look.start.z, 64, "player trace starts at the eye")
	T.near(look.endpos.x, 32768, 1e-3, "player trace reaches 32768")
	T.eq(look.filter, owner, "player trace filters the owner")

	session.env.__pump(0.1)
	T.ok(fired_timer, "timer.Simple runs after pump")
end
