-- Offline proofs. No headset, no LÖVR, no claim that a map "is GMod".
local src = debug.getinfo(1, "S").source
if src:sub(1, 1) == "@" then
	src = src:sub(2)
end
local root = src:match("(.*/)tests/") or "./"
package.path = root .. "src/?.lua;" .. package.path

local fails = 0
local checks = 0

local function fail(msg)
	fails = fails + 1
	io.stderr:write("FAIL ", msg, "\n")
end

local function near(a, b, eps, msg)
	checks = checks + 1
	if math.abs(a - b) > eps then
		fail(msg .. " got " .. tostring(a) .. " expected " .. tostring(b))
	end
end

local function ok(cond, msg)
	checks = checks + 1
	if not cond then
		fail(msg)
	end
end

local function eq(a, b, msg)
	checks = checks + 1
	if a ~= b then
		fail(msg .. " got " .. tostring(a) .. " expected " .. tostring(b))
	end
end

local T = { near = near, ok = ok, eq = eq, fail = fail }

dofile(root .. "tests/test_units_coords.lua")(T)
dofile(root .. "tests/test_move.lua")(T)
dofile(root .. "tests/test_vphysics.lua")(T)
dofile(root .. "tests/test_trace_bsp.lua")(T)
dofile(root .. "tests/test_compat.lua")(T)
dofile(root .. "tests/test_content.lua")(T)
dofile(root .. "tests/test_mdl.lua")(T)

io.write(string.format("engine proofs: %d checks, %d failed\n", checks, fails))
if fails > 0 then
	os.exit(1)
end
