return function(T)
	local compat = require("pure.compat")

	local zero = compat.Vector()
	T.eq(zero:IsZero(), true, "zero vector")
	zero:Set(compat.Vector(3, 4, 5))
	T.eq(zero.y, 4, "vector set")
	local ang = compat.Angle(0, 0, 0)
	ang:Set(compat.Angle(10, 20, 30))
	T.near(ang.y, 20, 1e-6, "angle set")
	local v = compat.Vector(1, 2, 3) + compat.Vector(4, 5, 6)
	T.eq(v:IsZero(), false, "nonzero vector")
	T.eq(v[1], 5, "vector x index")
	T.eq(v[2], 7, "vector y index")
	T.eq(v[3], 9, "vector z index")
	T.near(v.x, 5, 0, "vec add")
	T.near(v:Dot(compat.Vector(1, 0, 0)), 5, 0, "dot")
	v[1] = 8
	T.eq(v.x, 8, "vector x write")
	local fwd = compat.Angle(0, 90, 0):Forward()
	T.near(fwd.y, 1, 1e-6, "angle forward")
	T.near(fwd.x, 0, 1e-6, "angle forward x")
	local up = compat.Vector(0, 0, 1):Angle()
	T.near(up.p, 270, 1e-4, "vector angle")
	local spun = compat.Angle(0, 0, 0)
	T.eq(spun:RotateAroundAxis(compat.Vector(0, 0, 1), 180), spun, "rotate returns the same angle")
	T.near(spun.y, 180, 1e-4, "yaw 180 around up")
	T.near(spun.p, 0, 1e-4, "pitch stays 0 around up")
	T.near(spun.r, 0, 1e-4, "roll stays 0 around up")
	local pitched = compat.Angle(0, 0, 0)
	pitched:RotateAroundAxis(compat.Vector(0, -1, 0), 90)
	T.near(pitched.p, 270, 1e-3, "pitch 270 around right")
	local rolled = compat.Angle(0, 0, 0)
	rolled:RotateAroundAxis(rolled:Forward(), 90)
	T.near(rolled.r, 90, 1e-3, "roll 90 around forward")
	T.near(rolled.y, 0, 1e-3, "yaw stays 0 around forward")

	local hook = compat.new_hook()
	local order = {}
	hook.Add("Think", "a", function()
		order[#order + 1] = "a"
	end)
	hook.Add("Think", "b", function()
		order[#order + 1] = "b"
		return true
	end)
	hook.Add("Think", "c", function()
		order[#order + 1] = "c"
	end)
	local stopped = hook.Run("Think")
	T.ok(stopped == true, "hook return")
	T.eq(order[1], "a", "hook order a")
	T.eq(order[2], "b", "hook order b")
	T.ok(order[3] == nil, "c did not run")

	local src = [[
local n = 0
for i = 1, 4 do
  if i == 2 then continue end
  n = n + i
end
HOOK_N = n
]]
	local body = compat.rewrite_continue(src)
	T.ok(body:find("goto __engine_c", 1, true) ~= nil, "continue rewritten")
	local env = compat.make_env("client")
	local chunk = assert(loadstring(body))
	setfenv(chunk, env)
	chunk()
	T.eq(env.HOOK_N, 8, "continue skips 2 (1+3+4)")

	-- The shape that failed in stools/camera.lua: break is the last
	-- statement of the loop, so a label cannot follow it.
	local src_break = [[
local found = false
local seen = 0
for i = 1, 5 do
  if i == 2 then continue end
  seen = seen + 1
  found = true
  break
end
BREAK_N = found and seen or -1
]]
	body = compat.rewrite_continue(src_break)
	T.ok(body:find("do break end", 1, true) ~= nil, "break wrapped")
	env = compat.make_env("client")
	chunk = assert(loadstring(body))
	setfenv(chunk, env)
	chunk()
	T.eq(env.BREAK_N, 1, "break leaves after first kept step")

	local src_until = [[
local i = 0
local s = 0
repeat
  i = i + 1
  if i == 2 then continue end
  s = s + i
until i >= 4
UNTIL_N = s
]]
	body = compat.rewrite_continue(src_until)
	env = compat.make_env("client")
	chunk = assert(loadstring(body))
	setfenv(chunk, env)
	chunk()
	T.eq(env.UNTIL_N, 8, "repeat continue still sees until")

	-- Fixture addon: only the compatibility surface, so it must run.
	local root = debug.getinfo(1, "S").source
	if root:sub(1, 1) == "@" then
		root = root:sub(2)
	end
	root = root:match("(.*/)tests/")
	local fix = root .. "fixtures/addon_hello/lua/autorun/hello.lua"
	env = compat.make_env("shared")
	local ok, err = compat.load_file(fix, env)
	T.ok(ok, "fixture load " .. tostring(err))
	T.eq(env.hook.Run("EngineHello"), "hello", "fixture hook")

	-- Every addon Lua file we ship or have installed is attempted.
	-- A syntax or missing-global error is a recorded result, not a silent skip.
	local function realm_of(path)
		local base = path:match("([^/]+)$") or path
		if path:find("/autorun/client/", 1, true) or path:find("/cl_", 1, true) or base:find("^cl_") then
			return "client"
		end
		if path:find("/autorun/server/", 1, true) or path:find("/sv_", 1, true) or base:find("^sv_") then
			return "server"
		end
		return "shared"
	end
	local function files_in(dir)
		local out = {}
		local p = io.popen("find -L " .. string.format("%q", dir) .. " -type f -name '*.lua' 2>/dev/null")
		if not p then
			return out
		end
		for line in p:lines() do
			out[#out + 1] = line
		end
		p:close()
		table.sort(out)
		return out
	end
	local roots = {
		root .. "../addon",
		"/home/voldemar/.local/share/Steam/steamapps/common/GarrysMod/garrysmod/addons",
	}
	local seen = {}
	local rows = {}
	local attempted = 0
	local passed = 0
	for r = 1, #roots do
		local list = files_in(roots[r])
		for i = 1, #list do
			local path = list[i]
			if not seen[path] then
				seen[path] = true
				local fh = io.open(path, "rb")
				local text = fh:read("*a")
				fh:close()
				local realm = realm_of(path)
				local realms = realm == "shared" and { "server", "client" } or { realm }
				for ri = 1, #realms do
					attempted = attempted + 1
					local one = compat.make_env(realms[ri])
					local ok_file, err_file = compat.load_source(path, text, one)
					local missing_n = 0
					for _ in pairs(one.__missing) do
						missing_n = missing_n + 1
					end
					if ok_file then
						passed = passed + 1
					end
					rows[#rows + 1] = string.format(
						"%s\t%s\t%s\t%s",
						ok_file and "pass" or "fail",
						realms[ri],
						path,
						ok_file and "" or tostring(err_file):gsub("%s+", " "):sub(1, 180)
					)
				end
			end
		end
	end
	T.ok(attempted > 0, "found addon lua")
	local outdir = root .. "qa/out"
	os.execute("mkdir -p " .. string.format("%q", outdir))
	local rf = io.open(outdir .. "/addon_matrix.tsv", "w")
	rf:write("result\trealm\tpath\terror\n")
	for i = 1, #rows do
		rf:write(rows[i], "\n")
	end
	rf:write(string.format("# attempted %d passed %d\n", attempted, passed))
	rf:close()
	io.write(string.format("addon matrix: %d attempted, %d passed (see engine/qa/out/addon_matrix.tsv)\n", attempted, passed))
end
