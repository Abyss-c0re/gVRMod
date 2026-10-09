-- Host for the GarrysMod Lua that ships with the game.
-- Scripts are read from the Steam install at runtime. They are not copied here.
-- A name this host does not implement stays missing and reads as nil.
local compat = require("pure.compat")
local angles = require("pure.angles")
local activities = require("pure.activities")
local content = require("pure.content")
local gma_mod = require("pure.gma")

local M = {}

local function read_all(path)
	local fh = io.open(path, "rb")
	if not fh then
		return nil
	end
	local src = fh:read("*a")
	fh:close()
	return src
end

local function exists_file(path)
	local fh = io.open(path, "rb")
	if not fh then
		return false
	end
	fh:close()
	return true
end

local function is_dir(path)
	local p = io.popen("test -d " .. string.format("%q", path) .. " && printf y")
	if not p then
		return false
	end
	local s = p:read("*a")
	p:close()
	return s == "y"
end

local function ls(path)
	local p = io.popen("ls -1 " .. string.format("%q", path) .. " 2>/dev/null")
	if not p then
		return {}
	end
	local t = {}
	for line in p:lines() do
		if line ~= "" and line ~= "." and line ~= ".." then
			t[#t + 1] = line
		end
	end
	p:close()
	table.sort(t)
	return t
end

local function clean_join(dir, rel)
	local path = dir .. "/" .. rel
	local parts = {}
	for part in path:gmatch("[^/]+") do
		if part == ".." then
			parts[#parts] = nil
		elseif part ~= "." then
			parts[#parts + 1] = part
		end
	end
	if path:sub(1, 1) == "/" then
		return "/" .. table.concat(parts, "/")
	end
	return table.concat(parts, "/")
end

-- Published Half-Life 2 / Garry's Mod constants these autorun files read.
-- SF_NPC_* are the spawnflags in Valve source-sdk-2013 ai_basenpc.h.
-- SF_CITIZEN_* and CT_* are the npc_citizen spawnflags and citizentype key.
-- SF_FLOOR_TURRET_CITIZEN is the floor turret "Citizen modified" flag, 512.
local SPAWN_CONST = {
	CT_DEFAULT = 0,
	CT_DOWNTRODDEN = 1,
	CT_REFUGEE = 2,
	CT_REBEL = 3,
	CT_UNIQUE = 4,
	SF_NPC_DROP_HEALTHKIT = 8,
	SF_NPC_NO_PLAYER_PUSHAWAY = 16384,
	SF_CITIZEN_MEDIC = 131072,
	SF_CITIZEN_RANDOM_HEAD = 262144,
	SF_FLOOR_TURRET_CITIZEN = 512,
}

-- Brush contents and trace masks from Source SDK 2013 public/bspflags.h.
-- gmod_tool builds its trace mask with bit.bor of these at file scope.
local CONTENTS = {
	CONTENTS_EMPTY = 0,
	CONTENTS_SOLID = 0x1,
	CONTENTS_WINDOW = 0x2,
	CONTENTS_AUX = 0x4,
	CONTENTS_GRATE = 0x8,
	CONTENTS_SLIME = 0x10,
	CONTENTS_WATER = 0x20,
	CONTENTS_BLOCKLOS = 0x40,
	CONTENTS_OPAQUE = 0x80,
	LAST_VISIBLE_CONTENTS = 0x80,
	ALL_VISIBLE_CONTENTS = 0xFF,
	CONTENTS_TESTFOGVOLUME = 0x100,
	CONTENTS_UNUSED = 0x200,
	CONTENTS_UNUSED6 = 0x400,
	CONTENTS_TEAM1 = 0x800,
	CONTENTS_TEAM2 = 0x1000,
	CONTENTS_IGNORE_NODRAW_OPAQUE = 0x2000,
	CONTENTS_MOVEABLE = 0x4000,
	CONTENTS_AREAPORTAL = 0x8000,
	CONTENTS_PLAYERCLIP = 0x10000,
	CONTENTS_MONSTERCLIP = 0x20000,
	CONTENTS_CURRENT_0 = 0x40000,
	CONTENTS_CURRENT_90 = 0x80000,
	CONTENTS_CURRENT_180 = 0x100000,
	CONTENTS_CURRENT_270 = 0x200000,
	CONTENTS_CURRENT_UP = 0x400000,
	CONTENTS_CURRENT_DOWN = 0x800000,
	CONTENTS_ORIGIN = 0x1000000,
	CONTENTS_MONSTER = 0x2000000,
	CONTENTS_DEBRIS = 0x4000000,
	CONTENTS_DETAIL = 0x8000000,
	CONTENTS_TRANSLUCENT = 0x10000000,
	CONTENTS_LADDER = 0x20000000,
	CONTENTS_HITBOX = 0x40000000,
}

-- Studio models for HL2 classnames whose spawn-menu entry leaves Model unset.
-- npc_citizen is not in this table: CNPC_Citizen::SelectModel builds
-- models/Humans/<group>/head from citizentype (npc_citizen17.cpp).
-- A path that is not in the local VPKs is skipped, not invented as a mesh.
local NPC_MODEL = {
	npc_alyx = "models/alyx.mdl",
	npc_barney = "models/barney.mdl",
	npc_breen = "models/breen.mdl",
	npc_dog = "models/dog.mdl",
	npc_eli = "models/eli.mdl",
	npc_gman = "models/gman_high.mdl",
	npc_kleiner = "models/kleiner.mdl",
	npc_mossman = "models/mossman.mdl",
	npc_monk = "models/monk.mdl",
	npc_vortigaunt = "models/vortigaunt.mdl",
	npc_zombie = "models/zombie/classic.mdl",
	npc_zombie_torso = "models/zombie/classic_torso.mdl",
	npc_fastzombie = "models/zombie/fast.mdl",
	npc_fastzombie_torso = "models/zombie/fast_torso.mdl",
	npc_poisonzombie = "models/zombie/poison.mdl",
	npc_zombine = "models/zombie/zombie_soldier.mdl",
	npc_headcrab = "models/headcrabclassic.mdl",
	npc_headcrab_fast = "models/headcrab.mdl",
	npc_headcrab_black = "models/headcrabblack.mdl",
	npc_antlion = "models/antlion.mdl",
	npc_antlionguard = "models/antlion_guard.mdl",
	npc_antlion_worker = "models/antlion_worker.mdl",
	npc_antlion_grub = "models/antlion_grub.mdl",
	npc_barnacle = "models/barnacle.mdl",
	npc_manhack = "models/manhack.mdl",
	npc_rollermine = "models/roller.mdl",
	npc_turret_floor = "models/combine_turrets/floor_turret.mdl",
	npc_turret_ceiling = "models/combine_turrets/ceiling_turret.mdl",
	npc_combine_s = "models/combine_soldier.mdl",
	npc_metropolice = "models/police.mdl",
	npc_cscanner = "models/combine_scanner.mdl",
	npc_clawscanner = "models/shield_scanner.mdl",
	npc_stalker = "models/stalker.mdl",
	npc_strider = "models/combine_strider.mdl",
	npc_hunter = "models/hunter.mdl",
	npc_combinegunship = "models/gunship.mdl",
	npc_helicopter = "models/combine_helicopter.mdl",
	npc_combinedropship = "models/combine_dropship.mdl",
	npc_crow = "models/crow.mdl",
	npc_pigeon = "models/pigeon.mdl",
	npc_seagull = "models/seagull.mdl",
	npc_magnusson = "models/magnusson.mdl",
	npc_fisherman = "models/lostcoast/fisherman/fisherman.mdl",
}

local function citizen_model(entry)
	local kv = entry.KeyValues or {}
	local kind = tonumber(kv.citizentype) or SPAWN_CONST.CT_DOWNTRODDEN
	local flags = tonumber(entry.SpawnFlags) or 0
	local medic = bit.band(flags, SPAWN_CONST.SF_CITIZEN_MEDIC) ~= 0
	local folder = "Group01"
	if kind == SPAWN_CONST.CT_REFUGEE then
		folder = "Group02"
	elseif kind == SPAWN_CONST.CT_REBEL then
		folder = medic and "Group03m" or "Group03"
	end
	-- male_07 is one entry of g_ppszRandomHeads, not a dice roll.
	return "models/Humans/" .. folder .. "/" .. "male_07.mdl"
end

local function install_table(env)
	if not string.Explode then
		function string.Explode(sep, text)
			text = tostring(text or "")
			if sep == nil or sep == "" then
				local chars = {}
				for i = 1, #text do
					chars[i] = text:sub(i, i)
				end
				return chars
			end
			local out, start = {}, 1
			while true do
				local a, b = text:find(sep, start, true)
				if not a then
					out[#out + 1] = text:sub(start)
					break
				end
				out[#out + 1] = text:sub(start, a - 1)
				start = b + 1
			end
			return out
		end
	end
	local function copy(t, seen)
		if type(t) ~= "table" then
			return t
		end
		seen = seen or {}
		if seen[t] then
			return seen[t]
		end
		local out = {}
		seen[t] = out
		for k, v in pairs(t) do
			out[copy(k, seen)] = copy(v, seen)
		end
		return setmetatable(out, getmetatable(t))
	end
	if not table.Copy then
		function table.Copy(t)
			if t == nil then
				return nil
			end
			return copy(t)
		end
	end
	if not table.Merge then
		function table.Merge(dest, source)
			for k, v in pairs(source) do
				if type(v) == "table" and type(dest[k]) == "table" then
					table.Merge(dest[k], v)
				else
					dest[k] = v
				end
			end
			return dest
		end
	end
	if not table.GetKeys then
		function table.GetKeys(tab)
			local keys = {}
			for k in pairs(tab) do
				keys[#keys + 1] = k
			end
			return keys
		end
	end
	if not table.Inherit then
		function table.Inherit(t, base)
			for k, v in pairs(base) do
				if t[k] == nil then
					t[k] = v
				end
			end
			t.BaseClass = base
			return t
		end
	end
	if not table.Add then
		function table.Add(dest, source)
			for i = 1, #source do
				dest[#dest + 1] = source[i]
			end
			return dest
		end
	end
	if not table.HasValue then
		function table.HasValue(tab, val)
			for _, v in pairs(tab) do
				if v == val then
					return true
				end
			end
			return false
		end
	end
	-- Same shapes as garrysmod/lua/includes/extensions/string.lua.
	-- Method calls ("sh_a.lua"):StartWith("sh_") use the string metatable.
	if not string.PatternSafe then
		local pattern_escape = {
			["("] = "%(", [")"] = "%)", ["."] = "%.", ["%"] = "%%",
			["+"] = "%+", ["-"] = "%-", ["*"] = "%*", ["?"] = "%?",
			["["] = "%[", ["]"] = "%]", ["^"] = "%^", ["$"] = "%$",
			["\0"] = "%z",
		}
		function string.PatternSafe(str)
			return (tostring(str):gsub(".", pattern_escape))
		end
	end
	if not string.Trim then
		function string.Trim(s, char)
			s = tostring(s)
			char = char and string.PatternSafe(char) or "%s"
			return s:match("^" .. char .. "*(.-)" .. char .. "*$") or s
		end
	end
	if not string.TrimRight then
		function string.TrimRight(s, char)
			s = tostring(s)
			char = char and string.PatternSafe(char) or "%s"
			return s:match("^(.-)" .. char .. "*$") or s
		end
	end
	if not string.TrimLeft then
		function string.TrimLeft(s, char)
			s = tostring(s)
			char = char and string.PatternSafe(char) or "%s"
			return s:match("^" .. char .. "*(.-)$") or s
		end
	end
	if not string.StartsWith then
		function string.StartsWith(str, prefix)
			str = tostring(str)
			prefix = tostring(prefix)
			return str:sub(1, #prefix) == prefix
		end
		string.StartWith = string.StartsWith
	end
	if not string.EndsWith then
		function string.EndsWith(str, suffix)
			str = tostring(str)
			suffix = tostring(suffix)
			return suffix == "" or str:sub(-#suffix) == suffix
		end
	end
	if not string.GetFileFromFilename then
		function string.GetFileFromFilename(path)
			path = tostring(path or "")
			return path:match("([^/\\]+)$") or path
		end
	end
	if not string.Replace then
		function string.Replace(str, what, with)
			str = tostring(str)
			what = string.PatternSafe(tostring(what))
			with = tostring(with or ""):gsub("%%", "%%%%")
			return (str:gsub(what, with))
		end
	end
	env.table = table
	env.string = string
end

function M.script_model(text)
	if type(text) ~= "string" then
		return nil
	end
	local world = text:match('"[Pp]layer[Mm]odel"%s*"([^"]+)"')
	if world and world ~= "" then
		return world, "playermodel"
	end
	local view = text:match('"[Vv]iew[Mm]odel"%s*"([^"]+)"')
	if view and view ~= "" then
		return view, "viewmodel"
	end
	return nil
end

function M.npc_model_for(entry)
	if type(entry) ~= "table" then
		return nil
	end
	if type(entry.Model) == "string" and entry.Model ~= "" then
		return entry.Model, "list"
	end
	local class = entry.Class
	if class == "npc_citizen" then
		return citizen_model(entry), "citizen"
	end
	local known = class and NPC_MODEL[class]
	if known then
		return known, "class"
	end
	return nil
end

-- Hang the roster in the spawn camera's frustum.
-- Source fov 75 on a 1280×720 window is about ±atan(1.02) horizontal and
-- ±atan(0.58) vertical. Weapons fill the right side, NPCs the left side,
-- each mesh in its own cell on one plane. A street down the middle stays
-- empty so taller actors standing on the ground further ahead stay visible.
-- Yaw comes from the same AngleVectors basis as the player. Weapons get a
-- quarter turn so the barrel crosses the view. This is a reference pose,
-- not a sandbox spawn and not a thinking NPC.
function M.layout(items, origin, yaw)
	local forward, right = angles.angle_vectors(0, yaw or 0, 0)
	local weapons, npcs, big = {}, {}, {}
	for i = 1, #items do
		local it = items[i]
		local span = it.span or 16
		if (it.height or 72) > 160 or span > 140 then
			big[#big + 1] = it
		elseif it.kind == "weapon" then
			weapons[#weapons + 1] = it
		else
			npcs[#npcs + 1] = it
		end
	end
	local EYE = 64
	local INNER = 0.34
	local OUTER = 0.90
	local V_TAN = 0.52
	local function put(it, ahead, lateral, feet_z)
		it.x = origin.x + forward.x * ahead + right.x * lateral
		it.y = origin.y + forward.y * ahead + right.y * lateral
		it.z = feet_z
		if it.kind == "weapon" then
			it.yaw = (yaw or 0) + 90
		else
			it.yaw = (yaw or 0) + 180
		end
		it.pitch = 0
		it.roll = 0
	end
	local function pack_side(list, side, from_ground)
		if #list == 0 then
			return
		end
		table.sort(list, function(a, b)
			local ha = a.height or 0
			local hb = b.height or 0
			if ha ~= hb then
				return ha > hb
			end
			return tostring(a.class) < tostring(b.class)
		end)
		local function cell(it)
			-- Diameter is 2*span. Cap the slot so one long mesh does not
			-- push the whole side back until the small ones are specks.
			local w = math.max(20, (it.span or 16) * 2 + 6)
			if w > 56 then
				w = 56
			end
			local h = math.max(14, (it.height or 24) + 6)
			return w, h
		end
		local function try_pack(ahead, place)
			local inner = INNER * ahead
			local outer = OUTER * ahead
			local top = V_TAN * ahead
			-- NPCs grow up from just above the ground. Weapons use the full
			-- vertical cone, including the sky, so the gun wall stays close.
			local bot = from_ground and -36 or -top
			local x = inner
			local shelf = from_ground and bot or top
			local shelf_h = 0
			for i = 1, #list do
				local it = list[i]
				local w, h = cell(it)
				if w > (outer - inner) + 0.01 or h > (top - bot) + 0.01 then
					return false
				end
				if x + w > outer + 0.01 then
					shelf = from_ground and (shelf + shelf_h) or (shelf - shelf_h)
					x = inner
					shelf_h = 0
				end
				local overflow
				if from_ground then
					overflow = shelf + h > top + 0.01
				else
					overflow = shelf - h < bot - 0.01
				end
				if overflow then
					return false
				end
				if place then
					local lateral = side * (x + w * 0.5)
					local up = from_ground and (shelf + h * 0.5) or (shelf - h * 0.5)
					local feet = origin.z + EYE + up - (it.height or 0) * 0.5
					put(it, ahead, lateral, feet)
				end
				if h > shelf_h then
					shelf_h = h
				end
				x = x + w
			end
			return true
		end
		local lo, hi = 180, 180
		if not try_pack(hi, false) then
			for _ = 1, 16 do
				hi = hi * 1.3
				if try_pack(hi, false) or hi > 12000 then
					break
				end
			end
		end
		if try_pack(hi, false) then
			for _ = 1, 14 do
				local mid = (lo + hi) * 0.5
				if try_pack(mid, false) then
					hi = mid
				else
					lo = mid
				end
			end
			try_pack(hi, true)
			return
		end
		local ahead = 200
		for i = 1, #list do
			local it = list[i]
			local w, h = cell(it)
			local need = math.max(w / (OUTER - INNER), h / (2 * V_TAN), 180)
			if ahead < need then
				ahead = need
			end
			local lateral = side * (INNER * ahead + w * 0.5)
			local feet = origin.z + EYE - (it.height or 0) * 0.5
			put(it, ahead, lateral, feet)
			ahead = ahead + h + 8
		end
	end
	pack_side(weapons, 1, false)
	pack_side(npcs, -1, true)
	if #big == 0 then
		return
	end
	local widths = {}
	local total = 0
	local max_span = 0
	for i = 1, #big do
		local span = big[i].span or 80
		if span ~= span or span > 4000 then
			span = 256
		end
		local w = math.max(80, span * 2 + 48)
		widths[i] = w
		total = total + w
		if span > max_span then
			max_span = span
		end
	end
	local ahead = total / (2 * INNER * 0.85)
	local need_span = max_span / INNER + 400
	if need_span > ahead then
		ahead = need_span
	end
	if ahead < 1800 then
		ahead = 1800
	end
	local cursor = -total / 2
	for i = 1, #big do
		local lateral = cursor + widths[i] / 2
		cursor = cursor + widths[i]
		put(big[i], ahead, lateral, origin.z)
	end
end

function M.boot(opts)
	opts = opts or {}
	local gmod = opts.gmod
		or os.getenv("ENGINE_GMOD")
		or "/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod"
	local mapname = opts.map or "gm_construct"
	local env = compat.make_env("server")
	install_table(env)
	env.coroutine = coroutine
	for k, v in pairs(SPAWN_CONST) do
		env[k] = v
	end
	for k, v in pairs(CONTENTS) do
		env[k] = v
	end
	for k, v in pairs(activities) do
		env[k] = v
	end
	-- Source SDK 2013 public/tier1/iconvar.h. Scripts add these with +.
	local FCVAR = {
		FCVAR_NONE = 0,
		FCVAR_UNREGISTERED = 1,
		FCVAR_DEVELOPMENTONLY = 2,
		FCVAR_GAMEDLL = 4,
		FCVAR_CLIENTDLL = 8,
		FCVAR_HIDDEN = 16,
		FCVAR_PROTECTED = 32,
		FCVAR_SPONLY = 64,
		FCVAR_ARCHIVE = 128,
		FCVAR_NOTIFY = 256,
		FCVAR_USERINFO = 512,
		FCVAR_PRINTABLEONLY = 1024,
		FCVAR_UNLOGGED = 2048,
		FCVAR_NEVER_AS_STRING = 4096,
		FCVAR_REPLICATED = 8192,
		FCVAR_CHEAT = 16384,
		FCVAR_SS = 32768,
		FCVAR_DEMO = 65536,
		FCVAR_DONTRECORD = 131072,
		FCVAR_SS_ADDED = 262144,
		FCVAR_RELEASE = 524288,
		FCVAR_RELOAD_MATERIALS = 1048576,
		FCVAR_RELOAD_TEXTURES = 2097152,
		FCVAR_NOT_CONNECTED = 4194304,
		FCVAR_MATERIAL_SYSTEM_THREAD = 8388608,
		FCVAR_ARCHIVE_XBOX = 16777216,
		FCVAR_ACCESSIBLE_FROM_THREADS = 33554432,
		FCVAR_SERVER_CAN_EXECUTE = 268435456,
		FCVAR_SERVER_CANNOT_QUERY = 536870912,
		FCVAR_CLIENTCMD_CAN_EXECUTE = 1073741824,
	}
	for k, v in pairs(FCVAR) do
		env[k] = v
	end
	-- wiki.facepunch.com/gmod/Enums/MAT. These are the surface-type chars.
	local MAT = {
		MAT_ANTLION = 65,
		MAT_BLOODYFLESH = 66,
		MAT_CONCRETE = 67,
		MAT_DIRT = 68,
		MAT_EGGSHELL = 69,
		MAT_FLESH = 70,
		MAT_GRATE = 71,
		MAT_ALIENFLESH = 72,
		MAT_CLIP = 73,
		MAT_SNOW = 74,
		MAT_PLASTIC = 76,
		MAT_METAL = 77,
		MAT_SAND = 78,
		MAT_FOLIAGE = 79,
		MAT_COMPUTER = 80,
		MAT_SLOSH = 83,
		MAT_TILE = 84,
		MAT_GRASS = 85,
		MAT_VENT = 86,
		MAT_WOOD = 87,
		MAT_DEFAULT = 88,
		MAT_GLASS = 89,
		MAT_WARPSHIELD = 90,
	}
	for k, v in pairs(MAT) do
		env[k] = v
	end
	-- Source public/const.h damage bits. Used by weapon script bit.bor calls.
	local DMG = {
		DMG_GENERIC = 0,
		DMG_CRUSH = 1,
		DMG_BULLET = 2,
		DMG_SLASH = 4,
		DMG_BURN = 8,
		DMG_VEHICLE = 16,
		DMG_FALL = 32,
		DMG_BLAST = 64,
		DMG_CLUB = 128,
		DMG_SHOCK = 256,
		DMG_SONIC = 512,
		DMG_ENERGYBEAM = 1024,
		DMG_PREVENT_PHYSICS_FORCE = 2048,
		DMG_NEVERGIB = 4096,
		DMG_ALWAYSGIB = 8192,
		DMG_DROWN = 16384,
		DMG_PARALYZE = 32768,
		DMG_NERVEGAS = 65536,
		DMG_POISON = 131072,
		DMG_RADIATION = 262144,
		DMG_DROWNRECOVER = 524288,
		DMG_ACID = 1048576,
		DMG_SLOWBURN = 2097152,
		DMG_REMOVENORAGDOLL = 4194304,
		DMG_PHYSGUN = 8388608,
		DMG_PLASMA = 16777216,
		DMG_AIRBOAT = 33554432,
		DMG_DISSOLVE = 67108864,
		DMG_BLAST_SURFACE = 134217728,
		DMG_DIRECT = 268435456,
		DMG_BUCKSHOT = 536870912,
	}
	for k, v in pairs(DMG) do
		env[k] = v
	end
	local function mask(...)
		return bit.bor(...)
	end
	local C = env
	env.MASK_ALL = -1
	env.MASK_SOLID = mask(C.CONTENTS_SOLID, C.CONTENTS_MOVEABLE, C.CONTENTS_WINDOW, C.CONTENTS_MONSTER, C.CONTENTS_GRATE)
	env.MASK_PLAYERSOLID = mask(C.CONTENTS_SOLID, C.CONTENTS_MOVEABLE, C.CONTENTS_PLAYERCLIP, C.CONTENTS_WINDOW, C.CONTENTS_MONSTER, C.CONTENTS_GRATE)
	env.MASK_NPCSOLID = mask(C.CONTENTS_SOLID, C.CONTENTS_MOVEABLE, C.CONTENTS_MONSTERCLIP, C.CONTENTS_WINDOW, C.CONTENTS_MONSTER, C.CONTENTS_GRATE)
	env.MASK_WATER = mask(C.CONTENTS_WATER, C.CONTENTS_MOVEABLE, C.CONTENTS_SLIME)
	env.MASK_OPAQUE = mask(C.CONTENTS_SOLID, C.CONTENTS_MOVEABLE, C.CONTENTS_OPAQUE)
	env.MASK_OPAQUE_AND_NPCS = mask(env.MASK_OPAQUE, C.CONTENTS_MONSTER)
	env.MASK_BLOCKLOS = mask(C.CONTENTS_SOLID, C.CONTENTS_MOVEABLE, C.CONTENTS_BLOCKLOS)
	env.MASK_BLOCKLOS_AND_NPCS = mask(env.MASK_BLOCKLOS, C.CONTENTS_MONSTER)
	env.MASK_VISIBLE = mask(env.MASK_OPAQUE, C.CONTENTS_IGNORE_NODRAW_OPAQUE)
	env.MASK_VISIBLE_AND_NPCS = mask(env.MASK_OPAQUE_AND_NPCS, C.CONTENTS_IGNORE_NODRAW_OPAQUE)
	env.MASK_SHOT = mask(C.CONTENTS_SOLID, C.CONTENTS_MOVEABLE, C.CONTENTS_MONSTER, C.CONTENTS_WINDOW, C.CONTENTS_DEBRIS, C.CONTENTS_HITBOX)
	env.MASK_SHOT_HULL = mask(C.CONTENTS_SOLID, C.CONTENTS_MOVEABLE, C.CONTENTS_MONSTER, C.CONTENTS_WINDOW, C.CONTENTS_DEBRIS, C.CONTENTS_GRATE)
	env.MASK_SHOT_PORTAL = mask(C.CONTENTS_SOLID, C.CONTENTS_MOVEABLE, C.CONTENTS_WINDOW, C.CONTENTS_MONSTER)
	env.MASK_SOLID_BRUSHONLY = mask(C.CONTENTS_SOLID, C.CONTENTS_MOVEABLE, C.CONTENTS_WINDOW, C.CONTENTS_GRATE)
	env.MASK_PLAYERSOLID_BRUSHONLY = mask(C.CONTENTS_SOLID, C.CONTENTS_MOVEABLE, C.CONTENTS_WINDOW, C.CONTENTS_PLAYERCLIP, C.CONTENTS_GRATE)
	env.MASK_NPCSOLID_BRUSHONLY = mask(C.CONTENTS_SOLID, C.CONTENTS_MOVEABLE, C.CONTENTS_WINDOW, C.CONTENTS_MONSTERCLIP, C.CONTENTS_GRATE)
	env.MASK_NPCWORLDSTATIC = mask(C.CONTENTS_SOLID, C.CONTENTS_WINDOW, C.CONTENTS_MONSTERCLIP, C.CONTENTS_GRATE)
	env.MASK_SPLITAREAPORTAL = mask(C.CONTENTS_WATER, C.CONTENTS_SLIME)
	env.MASK_CURRENT = mask(C.CONTENTS_CURRENT_0, C.CONTENTS_CURRENT_90, C.CONTENTS_CURRENT_180, C.CONTENTS_CURRENT_270, C.CONTENTS_CURRENT_UP, C.CONTENTS_CURRENT_DOWN)
	env.MASK_DEADSOLID = mask(C.CONTENTS_SOLID, C.CONTENTS_PLAYERCLIP, C.CONTENTS_WINDOW, C.CONTENTS_GRATE)

	local missing = env.__missing
	local failures = {}
	local counts = {
		weapon_ok = 0, weapon_bad = 0, ent_ok = 0, ent_bad = 0,
		autorun_ok = 0, autorun_bad = 0,
		gma_autorun = 0, gma_weapon = 0, gma_ent = 0,
	}
	local ran = {}
	local loading = {}
	local include_depth = 0
	local roots = {}
	local include_stack = {}
	local precache = {}
	local cvars = {}
	local phrases = {}
	local commands = {}
	local allowed = {}
	local workshops = {}

	local function note_fail(kind, name, err)
		failures[#failures + 1] = { kind = kind, name = name, err = tostring(err) }
	end

	local function add_root(path)
		if path and is_dir(path) then
			roots[#roots + 1] = path
		end
	end

	local function addon_roots(parent)
		if not is_dir(parent) then
			return
		end
		for _, name in ipairs(ls(parent)) do
			add_root(parent .. "/" .. name .. "/lua")
		end
	end

	-- Repo addon after Steam so a matching relative path is found there first
	-- only when the caller put it first. include() walks roots in order.
	local repo = opts.repo_addon or (gmod .. "/../../../Dev/GMod/gVRMod/addon")
	-- The engine lives in <repo>/engine. Prefer that over a guessed absolute.
	do
		local src = debug.getinfo(1, "S").source
		if src:sub(1, 1) == "@" then
			src = src:sub(2)
		end
		local engine = src:match("^(.*)/src/pure/glua%.lua$")
		if engine then
			repo = engine .. "/../addon"
		end
	end
	if opts.repo_addon then
		repo = opts.repo_addon
	end
	addon_roots(repo)
	addon_roots(gmod .. "/addons")
	add_root(gmod .. "/gamemodes/sandbox/entities")
	add_root(gmod .. "/gamemodes/base/entities")
	add_root(gmod .. "/gamemodes/terrortown/entities")
	add_root(gmod .. "/gamemodes/sandbox/gamemode")
	add_root(gmod .. "/gamemodes/base/gamemode")
	add_root(gmod .. "/lua")

	-- Workshop Lua stays in the GMAD. The id in the path keeps two addons
	-- with the same relative name as two scripts. include() still resolves
	-- the first copy, which is what a mounted addon search does.
	local gma_by_id = {}
	local gma_first = {}
	local gma_pack_lua = {}
	local gma_list = {}
	local gma_ready = false
	local function gma_path(rec)
		return "gma/" .. tostring(rec.id) .. "/" .. rec.rel
	end
	local function gma_rec_from_path(path)
		if type(path) ~= "string" then
			return nil
		end
		local id, rel = path:match("^gma/(%d+)/(.*)$")
		if not id then
			return nil
		end
		local rec = gma_by_id[tonumber(id)]
		if rec and rec.rel == rel then
			return rec
		end
		return nil
	end
	local function index_workshop()
		if gma_ready then
			return
		end
		gma_ready = true
		local paths = content.gma_paths(content.workshop_dir(gmod))
		local id = 0
		for i = 1, #paths do
			local ok, pack = pcall(gma_mod.open, paths[i])
			if ok and pack and pack.files then
				local bucket = {}
				gma_pack_lua[pack] = bucket
				local names = {}
				for key in pairs(pack.files) do
					if key:sub(1, 4) == "lua/" and key:sub(-4) == ".lua" then
						names[#names + 1] = key
					end
				end
				table.sort(names)
				for n = 1, #names do
					local key = names[n]
					local rel = key:sub(5)
					id = id + 1
					local rec = { id = id, rel = rel, key = key, pack = pack }
					gma_by_id[id] = rec
					bucket[rel] = rec
					gma_list[#gma_list + 1] = rec
					if not gma_first[rel] then
						gma_first[rel] = rec
					end
				end
			end
		end
	end
	local function gma_hit(pack, rel)
		rel = tostring(rel or ""):gsub("\\", "/"):gsub("^/+", ""):lower()
		local bucket = pack and gma_pack_lua[pack]
		local rec = bucket and bucket[rel]
		if rec then
			return gma_path(rec)
		end
		rec = gma_first[rel]
		if rec then
			return gma_path(rec)
		end
		return nil
	end

	local function under_root(abs)
		local best, bestlen
		for i = 1, #roots do
			local root = roots[i]
			if #root > (bestlen or -1) and abs:sub(1, #root + 1) == root .. "/" then
				best = abs:sub(#root + 2)
				bestlen = #root
			end
		end
		return best
	end

	local function resolve(rel)
		if type(rel) ~= "string" or rel == "" then
			return nil
		end
		rel = rel:gsub("\\", "/")
		if rel:sub(-4) ~= ".lua" then
			rel = rel .. ".lua"
		end
		local trimmed = rel:gsub("^/+", "")
		local cur = include_stack[#include_stack]
		if cur then
			local grec = gma_rec_from_path(cur)
			if grec then
				local vdir = grec.rel:match("^(.*)/") or ""
				local joined = clean_join(vdir, trimmed)
				local hit = gma_hit(grec.pack, joined)
				if hit then
					return hit
				end
			else
				local dir = cur:match("^(.*)/") or ""
				local abs = clean_join(dir, rel)
				if exists_file(abs) then
					return abs
				end
				-- Sandbox stool.lua includes "stools/x.lua". Addon copies of that
				-- folder live under a different root (lua/weapons/gmod_tool/stools).
				-- Search the caller's path relative to its root, on every root.
				local from_root = under_root(cur)
				local vdir = from_root and (from_root:match("^(.*)/") or "")
				if vdir and vdir ~= "" then
					local joined = vdir .. "/" .. trimmed
					for i = 1, #roots do
						abs = roots[i] .. "/" .. joined
						if exists_file(abs) then
							return abs
						end
					end
					local hit = gma_hit(nil, joined)
					if hit then
						return hit
					end
				end
			end
		end
		for i = 1, #roots do
			local abs = roots[i] .. "/" .. trimmed
			if exists_file(abs) then
				return abs
			end
		end
		index_workshop()
		return gma_hit(nil, trimmed)
	end

	-- Counts bytecode in the game script, not in the preprocessor.
	local budget_steps = 0
	local budget_on = false
	local function budget_hook()
		budget_steps = budget_steps + 1
		if budget_steps > 250000 then
			error("script budget")
		end
	end
	local function budget_resume()
		if budget_on then
			debug.sethook(budget_hook, "", 100)
		end
	end

	local function run_file(path)
		if ran[path] then
			return true
		end
		if loading[path] then
			return false, "include cycle " .. path
		end
		if include_depth > 40 then
			return false, "include depth"
		end
		local src
		local grec = gma_rec_from_path(path)
		if grec then
			src = grec.pack:read(grec.key)
		else
			src = read_all(path)
		end
		if not src then
			return false, "missing " .. path
		end
		debug.sethook()
		local compiled, chunk = compat.compile(path, src, env)
		budget_resume()
		if not compiled then
			return false, chunk
		end
		include_depth = include_depth + 1
		loading[path] = true
		local outermost = include_depth == 1
		if outermost then
			budget_steps = 0
			budget_on = true
			debug.sethook(budget_hook, "", 100)
		end
		local ok, err = pcall(chunk)
		loading[path] = nil
		include_depth = include_depth - 1
		if outermost then
			budget_on = false
			debug.sethook()
		end
		if ok then
			ran[path] = true
		end
		return ok, err
	end

	function env.include(rel)
		local was = budget_on
		if was then
			debug.sethook()
		end
		local abs = resolve(rel)
		if not abs then
			budget_resume()
			error("Couldn't include file '" .. tostring(rel) .. "'", 2)
		end
		include_stack[#include_stack + 1] = abs
		local ok, err = run_file(abs)
		include_stack[#include_stack] = nil
		if was then
			budget_resume()
		end
		if not ok then
			error(tostring(err), 0)
		end
	end
	env.Include = env.include

	function env.AddCSLuaFile(rel)
		if rel == nil then
			rel = include_stack[#include_stack]
		end
		precache[tostring(rel)] = true
	end

	-- GMod's default tick is 66 Hz. Other engine.* calls stay missing.
	env.engine = setmetatable({
		TickInterval = function()
			return 1 / 66
		end,
		-- The sandbox files are what we load for gm_construct. The gamemode is not ticking.
		ActiveGamemode = function()
			return "sandbox"
		end,
	}, {
		__index = function(_, key)
			missing["engine." .. tostring(key)] = (missing["engine." .. tostring(key)] or 0) + 1
			return nil
		end,
	})
	local jit_os = (jit and jit.os) or ""
	env.system = {
		IsLinux = function()
			return jit_os == "Linux"
		end,
		IsWindows = function()
			return jit_os == "Windows"
		end,
		IsOSX = function()
			return jit_os == "OSX"
		end,
	}
	env.ErrorNoHaltWithStack = env.ErrorNoHalt
	local sound_props = {}
	env.sound = setmetatable({
		Add = function(t)
			if type(t) == "table" and type(t.name) == "string" then
				sound_props[t.name] = t
			end
		end,
		GetProperties = function(name)
			return sound_props[name]
		end,
	}, {
		__index = function(_, key)
			missing["sound." .. tostring(key)] = (missing["sound." .. tostring(key)] or 0) + 1
			return nil
		end,
	})
	-- Source public/soundflags.h channel numbers.
	env.CHAN_REPLACE = -1
	env.CHAN_AUTO = 0
	env.CHAN_WEAPON = 1
	env.CHAN_VOICE = 2
	env.CHAN_ITEM = 3
	env.CHAN_BODY = 4
	env.CHAN_STREAM = 5
	env.CHAN_STATIC = 6
	env.color_white = env.Color(255, 255, 255, 255)
	env.color_black = env.Color(0, 0, 0, 255)
	env.color_transparent = env.Color(255, 255, 255, 0)
	function env.MsgC(...)
		local parts = {}
		for i = 1, select("#", ...) do
			local v = select(i, ...)
			local is_color = type(v) == "table" and v.r ~= nil and v.g ~= nil and v.b ~= nil
			if not is_color then
				parts[#parts + 1] = tostring(v)
			end
		end
		if #parts > 0 then
			io.write(table.concat(parts))
		end
	end
	-- No Player/Weapon/Entity metatable is registered. FindMetaTable returns nil
	-- until something actually registers one. Callers that nil-check then return.
	local meta_tables = {}
	function env.FindMetaTable(name)
		return meta_tables[name]
	end
	function env.RegisterMetaTable(name, tab)
		meta_tables[name] = tab
		return tab
	end

	function env.Model(path)
		if path and path ~= "" then
			precache[path] = true
		end
		return path
	end
	function env.Sound(path)
		return path
	end

	local loaded_ns = {}
	local package = { loaded = loaded_ns }
	function package.seeall(mod)
		setmetatable(mod, { __index = env })
	end
	function env.module(name, ...)
		local ns = loaded_ns[name]
		if type(ns) ~= "table" then
			ns = {}
			loaded_ns[name] = ns
		end
		ns._NAME = name
		ns._M = ns
		ns._PACKAGE = name:match("^(.*%.)") or ""
		setfenv(2, ns)
		env[name] = ns
		for i = 1, select("#", ...) do
			local fn = select(i, ...)
			if type(fn) == "function" then
				fn(ns)
			end
		end
		return ns
	end
	env.package = package
	env.debug = { getmetatable = debug.getmetatable }

	local function convar(name, default)
		local cv = cvars[name]
		if cv then
			return cv
		end
		cv = { Name = name, value = default == nil and "" or tostring(default) }
		function cv:GetString()
			return self.value
		end
		function cv:GetFloat()
			return tonumber(self.value) or 0
		end
		function cv:GetInt()
			return math.floor(self:GetFloat())
		end
		function cv:GetBool()
			return self.value == "1" or self.value == "true"
		end
		cvars[name] = cv
		return cv
	end
	function env.CreateConVar(name, default)
		return convar(name, default)
	end
	env.CreateClientConVar = env.CreateConVar
	function env.GetConVar(name)
		return cvars[name]
	end
	function env.ConVarExists(name)
		return cvars[name] ~= nil
	end
	function env.GetConVarString(name)
		local cv = cvars[name]
		if not cv then
			missing["cvar:" .. tostring(name)] = (missing["cvar:" .. tostring(name)] or 0) + 1
			return ""
		end
		return cv:GetString()
	end
	function env.GetConVarNumber(name)
		local cv = cvars[name]
		if not cv then
			missing["cvar:" .. tostring(name)] = (missing["cvar:" .. tostring(name)] or 0) + 1
			return 0
		end
		return cv:GetFloat()
	end

	-- Change callbacks are stored. Nothing here fires them; a convar write does not pretend the game noticed.
	local change_cbs = {}
	env.cvars = {}
	function env.cvars.AddChangeCallback(name, fn, id)
		local key = tostring(name)
		local list = change_cbs[key]
		if not list then
			list = {}
			change_cbs[key] = list
		end
		list[tostring(id or (#list + 1))] = fn
	end
	function env.cvars.RemoveChangeCallback(name, id)
		local list = change_cbs[tostring(name)]
		if list then
			list[tostring(id)] = nil
		end
	end

	env.concommand = {}
	function env.concommand.Add(name, fn)
		commands[name] = fn
	end
	function env.concommand.GetTable()
		return commands
	end

	env.duplicator = {}
	local entity_modifiers = {}
	function env.duplicator.Allow(name)
		allowed[name] = true
	end
	function env.duplicator.RegisterEntityClass(name, fn)
		allowed[name] = fn or true
	end
	-- Same store as lua/includes/modules/duplicator.lua. Paste is not run.
	function env.duplicator.RegisterEntityModifier(name, fn)
		entity_modifiers[tostring(name)] = fn
	end
	function env.duplicator.StoreEntityModifier(ent, name, data)
		if type(ent) ~= "table" then
			return
		end
		ent.__modifiers = ent.__modifiers or {}
		ent.__modifiers[tostring(name)] = data
	end
	function env.duplicator.ClearEntityModifier(ent, name)
		if type(ent) == "table" and ent.__modifiers then
			ent.__modifiers[tostring(name)] = nil
		end
	end
	local constraint_classes = {}
	function env.duplicator.RegisterConstraint(name, fn)
		constraint_classes[tostring(name)] = fn or true
	end
	function env.duplicator.RegisterPlayerTable(name, fn)
		constraint_classes["player:" .. tostring(name)] = fn or true
	end

	-- numpad.Register is the stock one-liner: functions[name] = func.
	-- Key impulses are not fired.
	local numpad_fns = {}
	env.numpad = {}
	function env.numpad.Register(name, fn)
		numpad_fns[tostring(name)] = fn
	end

	-- net.Receive stores a handler. Nothing is sent or received.
	local net_receivers = {}
	env.net = {}
	function env.net.Receive(name, fn)
		net_receivers[tostring(name)] = fn
	end

	-- IMaterial is not rendered. The call has to return so entity scripts finish loading.
	function env.Material(path)
		local name = tostring(path or "")
		precache["material:" .. name] = true
		return { GetName = function() return name end }
	end
	function env.PrecacheParticleSystem(name)
		precache["particle:" .. tostring(name)] = true
	end

	-- lua/includes/util.lua. Closures for FORCE_* call these only when Set runs.
	env.FORCE_STRING = 1
	env.FORCE_NUMBER = 2
	env.FORCE_BOOL = 3
	env.FORCE_ANGLE = 4
	env.FORCE_COLOR = 5
	env.FORCE_VECTOR = 6
	function env.tobool(val)
		if val == nil or val == false or val == 0 or val == "0" or val == "false" then
			return false
		end
		return true
	end
	function env.AccessorFunc(tab, varname, name, iForce)
		if not tab then
			missing["AccessorFunc nil table"] = (missing["AccessorFunc nil table"] or 0) + 1
		end
		tab["Get" .. name] = function(self)
			return self[varname]
		end
		if iForce == env.FORCE_STRING then
			tab["Set" .. name] = function(self, v)
				self[varname] = tostring(v)
			end
			return
		end
		if iForce == env.FORCE_NUMBER then
			tab["Set" .. name] = function(self, v)
				self[varname] = tonumber(v)
			end
			return
		end
		if iForce == env.FORCE_BOOL then
			tab["Set" .. name] = function(self, v)
				self[varname] = env.tobool(v)
			end
			return
		end
		if iForce == env.FORCE_ANGLE then
			tab["Set" .. name] = function(self, v)
				self[varname] = env.Angle(v)
			end
			return
		end
		if iForce == env.FORCE_COLOR then
			tab["Set" .. name] = function(self, v)
				if type(v) == "table" and v.r and v.g and v.b then
					self[varname] = v
				else
					self[varname] = env.Color(255, 255, 255, 255)
				end
			end
			return
		end
		if iForce == env.FORCE_VECTOR then
			tab["Set" .. name] = function(self, v)
				self[varname] = env.Vector(v)
			end
			return
		end
		tab["Set" .. name] = function(self, v)
			self[varname] = v
		end
	end
	-- terrortown/gamemode/util.lua. Weapons call this before that file loads.
	function env.AccessorFuncDT(tbl, varname, name)
		tbl["Get" .. name] = function(s)
			return s.dt and s.dt[varname]
		end
		tbl["Set" .. name] = function(s, v)
			if s.dt then
				s.dt[varname] = v
			end
		end
	end

	-- Tool scripts call this while the file is loading. The name is recorded.
	-- Removal, undo, and the cleanup menu are not implemented.
	local cleanups = {}
	env.cleanup = {}
	function env.cleanup.Register(name)
		local key = tostring(name)
		cleanups[key] = cleanups[key] or {}
		return cleanups[key]
	end
	setmetatable(env.cleanup, {
		__index = function(_, key)
			missing["cleanup." .. tostring(key)] = (missing["cleanup." .. tostring(key)] or 0) + 1
			return nil
		end,
	})

	local spawned = {}
	local function make_ent(class)
		local ent = { __ent = true, ClassName = class or "" }
		function ent:GetClass()
			return self.ClassName
		end
		function ent:SetModel(model)
			self.__model = model
			return self
		end
		function ent:GetModel()
			return self.__model
		end
		function ent:SetPos(v)
			self.__pos = v
		end
		function ent:GetPos()
			return self.__pos or compat.Vector()
		end
		function ent:SetAngles(a)
			self.__ang = a
		end
		function ent:GetAngles()
			return self.__ang or compat.Angle()
		end
		return setmetatable(ent, {
			__index = function(_, key)
				missing["ent." .. tostring(key)] = (missing["ent." .. tostring(key)] or 0) + 1
				return nil
			end,
		})
	end

	env.ents = {}
	function env.ents.Create(class)
		local ent = make_ent(class)
		spawned[#spawned + 1] = ent
		return ent
	end
	function env.ents.Iterator()
		local i = 0
		return function()
			i = i + 1
			local ent = spawned[i]
			if ent then
				return i, ent
			end
		end
	end
	function env.ents.FindByClass(class)
		local out = {}
		for i = 1, #spawned do
			if spawned[i].ClassName == class then
				out[#out + 1] = spawned[i]
			end
		end
		return out
	end
	function env.ents.GetAll()
		local out = {}
		for i = 1, #spawned do
			out[i] = spawned[i]
		end
		return out
	end

	env.util = {}
	function env.util.PrecacheModel(model)
		if model and model ~= "" then
			precache[model] = true
		end
		return model
	end
	function env.util.PrecacheSound(snd)
		return snd
	end
	function env.util.AddNetworkString(name)
		precache["net:" .. tostring(name)] = true
	end
	function env.util.TableToJSON(val)
		local function enc(v, depth)
			if depth > 8 then
				return "null"
			end
			local kind = type(v)
			if kind == "nil" then
				return "null"
			end
			if kind == "boolean" then
				return v and "true" or "false"
			end
			if kind == "number" then
				return tostring(v)
			end
			if kind == "string" then
				return string.format("%q", v)
			end
			if kind ~= "table" then
				return "null"
			end
			local n = 0
			for k in pairs(v) do
				n = n + 1
				if type(k) ~= "number" then
					n = -1
					break
				end
			end
			if n >= 0 then
				local parts = {}
				for i = 1, #v do
					parts[i] = enc(v[i], depth + 1)
				end
				return "[" .. table.concat(parts, ",") .. "]"
			end
			local parts = {}
			for k, item in pairs(v) do
				parts[#parts + 1] = enc(tostring(k), depth + 1) .. ":" .. enc(item, depth + 1)
			end
			return "{" .. table.concat(parts, ",") .. "}"
		end
		return enc(val, 0)
	end
	setmetatable(env.util, {
		__index = function(_, key)
			missing["util." .. tostring(key)] = (missing["util." .. tostring(key)] or 0) + 1
			return nil
		end,
	})

	env.language = {}
	function env.language.Add(key, text)
		phrases[key] = text
	end
	function env.language.GetPhrase(key)
		return phrases[key] or key
	end

	env.resource = {}
	function env.resource.AddWorkshop(id)
		workshops[#workshops + 1] = tostring(id)
	end
	function env.resource.AddFile(path)
		precache["res:" .. tostring(path)] = true
	end

	local game = {}
	local decals = {}
	function game.GetMap()
		return mapname
	end
	function game.SinglePlayer()
		return true
	end
	function game.MaxPlayers()
		return 1
	end
	function game.AddDecal(name, material)
		decals[tostring(name)] = material
	end
	local particle_files = {}
	function game.AddParticles(path)
		particle_files[#particle_files + 1] = tostring(path)
	end
	local ammo_types = {}
	function game.AddAmmoType(t)
		if type(t) == "table" and t.name then
			ammo_types[tostring(t.name)] = t
		end
	end
	env.game = setmetatable(game, {
		__index = function(_, key)
			missing["game." .. tostring(key)] = (missing["game." .. tostring(key)] or 0) + 1
			return nil
		end,
	})

	local function mounted_vpk(file)
		return exists_file(gmod .. "/../sourceengine/" .. file)
			or exists_file(gmod .. "/../../sourceengine/" .. file)
	end
	function env.IsMounted(name)
		name = tostring(name or ""):lower()
		if name == "hl2" or name == "garrysmod" then
			return true
		end
		local files = {
			cstrike = "content_cstrike_dir.vpk",
			hl1 = "content_hl1_dir.vpk",
			hl1mp = "content_hl1mp_dir.vpk",
			portal = "content_portal_dir.vpk",
			episodic = "content_episodic_dir.vpk",
			ep2 = "content_ep2_dir.vpk",
			lostcoast = "content_lostcoast_dir.vpk",
			tf = "content_tf_dir.vpk",
			dod = "content_dod_dir.vpk",
		}
		local file = files[name]
		if not file then
			missing["mount:" .. name] = (missing["mount:" .. name] or 0) + 1
			return false
		end
		return mounted_vpk(file)
	end

	local function file_find(wild)
		wild = tostring(wild or ""):gsub("\\", "/")
		local dir, pat = wild:match("^(.*)/([^/]*)$")
		if not dir then
			dir, pat = "", wild
		end
		local lua_pat = {}
		for ci = 1, #pat do
			local ch = pat:sub(ci, ci)
			if ch == "*" then
				lua_pat[#lua_pat + 1] = ".*"
			elseif ch:match("[%^%$%(%)%%%.%[%]%+%-%?]") then
				lua_pat[#lua_pat + 1] = "%" .. ch
			else
				lua_pat[#lua_pat + 1] = ch
			end
		end
		pat = "^" .. table.concat(lua_pat) .. "$"
		local files, dirs, seen_f, seen_d = {}, {}, {}, {}
		for i = 1, #roots do
			local folder = roots[i] .. (dir ~= "" and ("/" .. dir) or "")
			if is_dir(folder) then
				for _, name in ipairs(ls(folder)) do
					local full = folder .. "/" .. name
					if is_dir(full) then
						if not seen_d[name] and name:match(pat) then
							seen_d[name] = true
							dirs[#dirs + 1] = name
						end
					elseif not seen_f[name] and name:match(pat) then
						seen_f[name] = true
						files[#files + 1] = name
					end
				end
			end
		end
		table.sort(files)
		table.sort(dirs)
		index_workshop()
		local prefix = dir:lower()
		if prefix ~= "" then
			prefix = prefix .. "/"
		end
		local extra_f, extra_d = {}, {}
		for gi = 1, #gma_list do
			local rel = gma_list[gi].rel
			if prefix == "" or rel:sub(1, #prefix) == prefix then
				local rest = rel:sub(#prefix + 1)
				local child, more = rest:match("^([^/]+)(/.*)$")
				if not child then
					child = rest
					more = nil
				end
				if child ~= "" and child:match(pat) then
					if more then
						if not seen_d[child] then
							seen_d[child] = true
							extra_d[#extra_d + 1] = child
						end
					elseif not seen_f[child] then
						seen_f[child] = true
						extra_f[#extra_f + 1] = child
					end
				end
			end
		end
		-- Disk stools register before a workshop stool can throw out of the tool loop.
		table.sort(extra_f)
		table.sort(extra_d)
		for i = 1, #extra_f do
			files[#files + 1] = extra_f[i]
		end
		for i = 1, #extra_d do
			dirs[#dirs + 1] = extra_d[i]
		end
		return files, dirs
	end

	env.file = {}
	function env.file.Find(wild, pathid)
		if pathid ~= nil and pathid ~= "LUA" and pathid ~= "lsv" and pathid ~= "GAME" then
			missing["file.Find:" .. tostring(pathid)] = 1
		end
		if pathid == "GAME" then
			local files, dirs = file_find(wild)
			if #files == 0 and #dirs == 0 then
				missing["file.Find:GAME"] = (missing["file.Find:GAME"] or 0) + 1
			end
			return files, dirs
		end
		return file_find(wild)
	end
	-- Session scratch for file.Write/Append. Not the player's Steam data folder.
	local data_dirs = {}
	local data_files = {}
	local function data_key(name)
		return tostring(name or ""):gsub("\\", "/"):gsub("^/+", ""):gsub("/+$", "")
	end
	function env.file.Exists(name, pathid)
		if pathid == "DATA" then
			local key = data_key(name)
			return data_dirs[key] == true or data_files[key] ~= nil
		end
		if pathid == "GAME" then
			local abs = gmod .. "/" .. tostring(name):gsub("\\", "/")
			return exists_file(abs)
		end
		return resolve(tostring(name)) ~= nil
	end
	function env.file.CreateDir(name)
		data_dirs[data_key(name)] = true
	end
	function env.file.Write(name, contents)
		data_files[data_key(name)] = tostring(contents or "")
	end
	function env.file.Append(name, contents)
		local key = data_key(name)
		data_files[key] = (data_files[key] or "") .. tostring(contents or "")
	end
	function env.file.Read(name, pathid)
		if pathid == "DATA" then
			return data_files[data_key(name)]
		end
		return nil
	end

	-- Identifiers are stored. Callbacks are not run; there is no think loop yet.
	local timers = {}
	env.timer = {}
	function env.timer.Create(id, delay, reps, fn)
		timers[tostring(id)] = { delay = delay, reps = reps, fn = fn }
	end
	function env.timer.Remove(id)
		timers[tostring(id)] = nil
	end
	function env.timer.Exists(id)
		return timers[tostring(id)] ~= nil
	end
	local simple_n = 0
	function env.timer.Simple(delay, fn)
		simple_n = simple_n + 1
		timers["#simple:" .. tostring(simple_n)] = { delay = delay, reps = 1, fn = fn }
	end
	setmetatable(env.file, {
		__index = function(_, key)
			missing["file." .. tostring(key)] = (missing["file." .. tostring(key)] or 0) + 1
			return nil
		end,
	})

	local function load_module(name)
		local path = gmod .. "/lua/includes/modules/" .. name .. ".lua"
		include_stack[#include_stack + 1] = path
		local ok, err = run_file(path)
		include_stack[#include_stack] = nil
		if not ok then
			note_fail("module", name, err)
			return false
		end
		return true
	end

	local modules_ok = load_module("list") and load_module("baseclass") and load_module("weapons") and load_module("scripted_ents")
	-- autorun/properties.lua calls properties.Add. The module is engine-loaded in GMod.
	load_module("properties")
	load_module("player_manager")
	load_module("constraint")

	local function fresh_swep(class)
		return {
			Primary = {},
			Secondary = {},
			Base = "weapon_base",
			Folder = "weapons/" .. class,
			ClassName = class,
		}
	end

	local function authored_swep(swep)
		return swep.PrintName ~= nil or swep.WorldModel ~= nil or swep.ViewModel ~= nil
			or swep.Purpose ~= nil or swep.Spawnable ~= nil or swep.HoldType ~= nil
	end

	local function load_weapon(item)
		if not env.weapons or not env.weapons.Register then
			note_fail("weapon", item.class, "weapons.Register missing")
			counts.weapon_bad = counts.weapon_bad + 1
			return
		end
		local swep = fresh_swep(item.class)
		env.SWEP = swep
		env.ENT = nil
		include_stack[#include_stack + 1] = item.file
		local ok, err = run_file(item.file)
		include_stack[#include_stack] = nil
		if authored_swep(swep) then
			local rok, rerr = pcall(env.weapons.Register, swep, item.class)
			if rok then
				counts.weapon_ok = counts.weapon_ok + 1
				if not ok then
					note_fail("weapon", item.class, err)
				end
			else
				counts.weapon_bad = counts.weapon_bad + 1
				note_fail("weapon", item.class, rerr)
			end
		else
			counts.weapon_bad = counts.weapon_bad + 1
			note_fail("weapon", item.class, err or "no SWEP fields")
		end
		env.SWEP = nil
	end

	local function fresh_ent(class)
		return { Type = "anim", Base = "base_anim", ClassName = class }
	end

	local function authored_ent(ent)
		return ent.PrintName ~= nil or ent.Model ~= nil or ent.Initialize ~= nil
			or ent.Spawnable ~= nil or ent.Base ~= "base_anim" or ent.Type ~= "anim"
	end

	local function load_ent(item)
		if not env.scripted_ents or not env.scripted_ents.Register then
			note_fail("ent", item.class, "scripted_ents.Register missing")
			counts.ent_bad = counts.ent_bad + 1
			return
		end
		local ent = fresh_ent(item.class)
		env.ENT = ent
		env.SWEP = nil
		include_stack[#include_stack + 1] = item.file
		local ok, err = run_file(item.file)
		include_stack[#include_stack] = nil
		local already = env.scripted_ents.GetStored and env.scripted_ents.GetStored(item.class)
		local registered = already ~= nil
		if not registered and authored_ent(ent) then
			local rok, rerr = pcall(env.scripted_ents.Register, ent, item.class)
			registered = rok
			if not rok then
				err = rerr
			end
		end
		if registered then
			counts.ent_ok = counts.ent_ok + 1
			if not ok then
				note_fail("ent", item.class, err)
			end
		else
			counts.ent_bad = counts.ent_bad + 1
			note_fail("ent", item.class, err or "no ENT fields")
		end
		env.ENT = nil
	end

	local function scripts_in(dir)
		local out = {}
		if not is_dir(dir) then
			return out
		end
		for _, name in ipairs(ls(dir)) do
			local full = dir .. "/" .. name
			if name:sub(-4) == ".lua" then
				out[#out + 1] = { class = name:sub(1, -5), file = full }
			elseif is_dir(full) then
				local init = full .. "/init.lua"
				local shared = full .. "/shared.lua"
				if exists_file(init) then
					out[#out + 1] = { class = name, file = init }
				elseif exists_file(shared) then
					out[#out + 1] = { class = name, file = shared }
				end
			end
		end
		return out
	end

	local function load_dir(dir, kind)
		local list = scripts_in(dir)
		for i = 1, #list do
			if kind == "weapon" then
				load_weapon(list[i])
			else
				load_ent(list[i])
			end
		end
	end

	local function on_disk(rel)
		for i = 1, #roots do
			if exists_file(roots[i] .. "/" .. rel) then
				return true
			end
		end
		return false
	end

	local function gma_classes(prefix)
		index_workshop()
		local best = {}
		local pre = prefix .. "/"
		for i = 1, #gma_list do
			local rec = gma_list[i]
			if rec.rel:sub(1, #pre) == pre then
				local rest = rec.rel:sub(#pre + 1)
				local class, rank
				local fileclass = rest:match("^([^/]+)%.lua$")
				if fileclass then
					class, rank = fileclass, 2
				else
					local folder, leaf = rest:match("^([^/]+)/([^/]+)%.lua$")
					if folder and leaf == "init" then
						class, rank = folder, 3
					elseif folder and leaf == "shared" then
						class, rank = folder, 1
					end
				end
				if class then
					local disk = on_disk(pre .. class .. ".lua")
						or on_disk(pre .. class .. "/init.lua")
						or on_disk(pre .. class .. "/shared.lua")
					if not disk then
						local cur = best[class]
						if not cur or rank > cur.rank then
							best[class] = { class = class, file = gma_path(rec), rank = rank }
						end
					end
				end
			end
		end
		local out = {}
		for _, item in pairs(best) do
			out[#out + 1] = item
		end
		table.sort(out, function(a, b)
			return a.class < b.class
		end)
		return out
	end

	local function run_gma_autorun()
		index_workshop()
		local list = {}
		for i = 1, #gma_list do
			local rel = gma_list[i].rel
			if (rel:match("^autorun/[^/]+%.lua$") or rel:match("^autorun/server/[^/]+%.lua$"))
				and not on_disk(rel) then
				list[#list + 1] = gma_list[i]
			end
		end
		table.sort(list, function(a, b)
			if a.rel == b.rel then
				return a.id < b.id
			end
			return a.rel < b.rel
		end)
		for i = 1, #list do
			local full = gma_path(list[i])
			counts.gma_autorun = counts.gma_autorun + 1
			include_stack[#include_stack + 1] = full
			local ok, err = run_file(full)
			include_stack[#include_stack] = nil
			if ok then
				counts.autorun_ok = counts.autorun_ok + 1
			else
				counts.autorun_bad = counts.autorun_bad + 1
				note_fail("autorun", full, err)
			end
		end
	end

	if modules_ok then
		-- Garry's Mod runs autorun before it scans weapons and entities.
		-- ArcVR weapons read convars that cl_arcticvr_misc.lua creates here.
		local function run_autorun(dir)
			if not is_dir(dir) then
				return
			end
			for _, name in ipairs(ls(dir)) do
				local full = dir .. "/" .. name
				if name:sub(-4) == ".lua" then
					include_stack[#include_stack + 1] = full
					local ok, err = run_file(full)
					include_stack[#include_stack] = nil
					if ok then
						counts.autorun_ok = counts.autorun_ok + 1
					else
						counts.autorun_bad = counts.autorun_bad + 1
						note_fail("autorun", full, err)
					end
				elseif name == "server" and is_dir(full) then
					run_autorun(full)
				end
			end
		end
		run_autorun(gmod .. "/lua/autorun")
		for _, name in ipairs(ls(gmod .. "/addons")) do
			run_autorun(gmod .. "/addons/" .. name .. "/lua/autorun")
		end
		for _, name in ipairs(ls(repo)) do
			run_autorun(repo .. "/" .. name .. "/lua/autorun")
		end
		run_gma_autorun()

		-- Entity scripts assign methods on GAMEMODE at file scope.
		-- An empty table stores the function. It does not run the gamemode.
		if type(env.GAMEMODE) ~= "table" then
			env.GAMEMODE = {}
		end

		load_dir(gmod .. "/gamemodes/base/entities/weapons", "weapon")
		load_dir(gmod .. "/lua/weapons", "weapon")
		load_dir(gmod .. "/gamemodes/sandbox/entities/weapons", "weapon")
		load_dir(gmod .. "/gamemodes/terrortown/entities/weapons", "weapon")
		for _, name in ipairs(ls(gmod .. "/addons")) do
			load_dir(gmod .. "/addons/" .. name .. "/lua/weapons", "weapon")
		end
		for _, name in ipairs(ls(repo)) do
			load_dir(repo .. "/" .. name .. "/lua/weapons", "weapon")
		end
		local gma_weapons = gma_classes("weapons")
		counts.gma_weapon = #gma_weapons
		for i = 1, #gma_weapons do
			load_weapon(gma_weapons[i])
		end

		load_dir(gmod .. "/gamemodes/base/entities/entities", "ent")
		load_dir(gmod .. "/lua/entities", "ent")
		load_dir(gmod .. "/gamemodes/sandbox/entities/entities", "ent")
		load_dir(gmod .. "/gamemodes/terrortown/entities/entities", "ent")
		for _, name in ipairs(ls(gmod .. "/addons")) do
			load_dir(gmod .. "/addons/" .. name .. "/lua/entities", "ent")
		end
		for _, name in ipairs(ls(repo)) do
			load_dir(repo .. "/" .. name .. "/lua/entities", "ent")
		end
		local gma_ents = gma_classes("entities")
		counts.gma_ent = #gma_ents
		for i = 1, #gma_ents do
			load_ent(gma_ents[i])
		end

		if env.weapons and env.weapons.OnLoaded then
			local ok, err = pcall(env.weapons.OnLoaded)
			if not ok then
				note_fail("module", "weapons.OnLoaded", err)
			end
		end
		if env.scripted_ents and env.scripted_ents.OnLoaded then
			local ok, err = pcall(env.scripted_ents.OnLoaded)
			if not ok then
				note_fail("module", "scripted_ents.OnLoaded", err)
			end
		end
	end

	local shown = {}
	for i = 1, math.min(12, #failures) do
		local f = failures[i]
		shown[#shown + 1] = f.kind .. " " .. f.name .. " " .. f.err:gsub("%s+", " "):sub(1, 140)
	end

	local summary = string.format(
		"scripts weapons %d ok %d fail ents %d ok %d fail autorun %d ok %d fail modules %s gma autorun %d weapons %d ents %d",
		counts.weapon_ok, counts.weapon_bad, counts.ent_ok, counts.ent_bad,
		counts.autorun_ok, counts.autorun_bad, tostring(modules_ok),
		counts.gma_autorun, counts.gma_weapon, counts.gma_ent
	)

	return {
		env = env,
		gmod = gmod,
		map = mapname,
		failures = failures,
		failure_lines = shown,
		counts = counts,
		modules_ok = modules_ok,
		summary = summary,
		spawned = spawned,
		make_ent = make_ent,
		precache = precache,
		workshops = workshops,
	}
end

function M.collect(session, mount)
	local items = {}
	local skipped = {}
	local env = session.env
	if not session.modules_ok or not env.weapons or not env.list then
		return items, skipped
	end
	local seen = {}
	local list_ok, weapons = pcall(env.weapons.GetList)
	if list_ok and weapons then
		for i = 1, #weapons do
			local stored = weapons[i]
			local class = stored.ClassName or stored.PrintName
			if class and not seen[class] then
				seen[class] = true
				local ok, full = pcall(env.weapons.Get, class)
				full = ok and full or stored
				local model = full.WorldModel
				local source = "world"
				if model == "" then
					model = nil
				end
				local view = full.ViewModel
				if type(view) ~= "string" or view == "" then
					view = nil
				end
				if not model and view then
					model = view
					source = "view"
				end
				if model then
					items[#items + 1] = {
						kind = "weapon",
						class = class,
						model = model,
						view = view,
						source = source,
					}
				else
					skipped[#skipped + 1] = "weapon " .. class .. " no model"
				end
			end
		end
	end
	local npc_ok, npcs = pcall(env.list.Get, "NPC")
	if npc_ok and npcs then
		local keys = {}
		for key in pairs(npcs) do
			keys[#keys + 1] = key
		end
		table.sort(keys, function(a, b)
			return tostring(a) < tostring(b)
		end)
		for i = 1, #keys do
			local key = keys[i]
			local entry = npcs[key]
			local model, source = M.npc_model_for(entry)
			local class = (type(entry) == "table" and entry.Class) or key
			if not model and env.scripted_ents and env.scripted_ents.Get then
				local got, full = pcall(env.scripted_ents.Get, class)
				if got and type(full) == "table" and type(full.Initialize) == "function" then
					local ent = session.make_ent(class)
					local steps = 0
					debug.sethook(function()
						steps = steps + 1
						if steps > 200000 then
							error("init budget")
						end
					end, "", 80)
					pcall(full.Initialize, ent)
					debug.sethook()
					if type(ent.GetModel) == "function" and ent:GetModel() then
						model = ent:GetModel()
						source = "init"
					end
				end
			end
			if model then
				items[#items + 1] = {
					kind = "npc",
					class = tostring(key),
					model = model,
					source = source or "npc",
					npc = class,
				}
			else
				skipped[#skipped + 1] = "npc " .. tostring(key) .. " no model"
			end
		end
	end
	if env.list and env.list.Get and mount and mount.read then
		local wok, listed = pcall(env.list.Get, "Weapon")
		if wok and listed then
			local keys = {}
			for key in pairs(listed) do
				keys[#keys + 1] = key
			end
			table.sort(keys)
			for i = 1, #keys do
				local class = keys[i]
				if not seen[class] then
					seen[class] = true
					local text = mount:read("scripts/weapons/" .. class .. ".txt")
						or mount:read("scripts/" .. class .. ".txt")
					local model, how = M.script_model(text)
					if model then
						items[#items + 1] = {
							kind = "weapon",
							class = class,
							model = model,
							source = how or "script",
						}
					else
						skipped[#skipped + 1] = "weapon " .. class .. " no script model"
					end
				end
			end
		end
	end
	table.sort(items, function(a, b)
		local rank = { weapon = 1, npc = 2, sent = 3 }
		local ra, rb = rank[a.kind] or 9, rank[b.kind] or 9
		if ra ~= rb then
			return ra < rb
		end
		return tostring(a.class) < tostring(b.class)
	end)
	return items, skipped
end

return M
