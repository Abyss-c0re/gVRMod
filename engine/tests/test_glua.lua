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

	session.env.__pump(0.1)
	T.ok(fired_timer, "timer.Simple runs after pump")
end
