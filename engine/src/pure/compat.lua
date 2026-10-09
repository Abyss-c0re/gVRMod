-- GMod Lua types used as the compatibility layer.
-- The sandbox does not pretend an unknown global exists. A missing name is
-- recorded and reads as nil, so a file that needs it errors and the audit
-- can show which symbol blocked it.
local angles = require("pure.angles")

local M = {}

local V = {}
V.__index = V

function M.Vector(x, y, z)
	if type(x) == "table" then
		return M.Vector(x.x, x.y, x.z)
	end
	return setmetatable({ x = x or 0, y = y or 0, z = z or 0 }, V)
end

function V:Add(o)
	self.x, self.y, self.z = self.x + o.x, self.y + o.y, self.z + o.z
	return self
end

function V:Sub(o)
	self.x, self.y, self.z = self.x - o.x, self.y - o.y, self.z - o.z
	return self
end

function V:Mul(s)
	if type(s) == "table" then
		self.x, self.y, self.z = self.x * s.x, self.y * s.y, self.z * s.z
	else
		self.x, self.y, self.z = self.x * s, self.y * s, self.z * s
	end
	return self
end

function V:Dot(o)
	return self.x * o.x + self.y * o.y + self.z * o.z
end

function V:Cross(o)
	return M.Vector(
		self.y * o.z - self.z * o.y,
		self.z * o.x - self.x * o.z,
		self.x * o.y - self.y * o.x
	)
end

function V:LengthSqr()
	return self.x * self.x + self.y * self.y + self.z * self.z
end

function V:Length()
	return math.sqrt(self:LengthSqr())
end

function V:GetNormalized()
	local l = self:Length()
	if l < 1e-8 then
		return M.Vector()
	end
	return M.Vector(self.x / l, self.y / l, self.z / l)
end

function V:Normalize()
	local l = self:Length()
	if l < 1e-8 then
		return 0
	end
	self.x, self.y, self.z = self.x / l, self.y / l, self.z / l
	return l
end

function V:Angle()
	local p, y, r = angles.vector_angles(self.x, self.y, self.z)
	return M.Angle(p, y, r)
end

function V:Unpack()
	return self.x, self.y, self.z
end

function V:Zero()
	self.x, self.y, self.z = 0, 0, 0
	return self
end

function V:DistToSqr(o)
	local dx, dy, dz = self.x - o.x, self.y - o.y, self.z - o.z
	return dx * dx + dy * dy + dz * dz
end

function V:Distance(o)
	return math.sqrt(self:DistToSqr(o))
end

function V.__add(a, b)
	return M.Vector(a.x + b.x, a.y + b.y, a.z + b.z)
end

function V.__sub(a, b)
	return M.Vector(a.x - b.x, a.y - b.y, a.z - b.z)
end

function V.__mul(a, b)
	if type(a) == "number" then
		return M.Vector(b.x * a, b.y * a, b.z * a)
	end
	if type(b) == "number" then
		return M.Vector(a.x * b, a.y * b, a.z * b)
	end
	return M.Vector(a.x * b.x, a.y * b.y, a.z * b.z)
end

function V.__unm(a)
	return M.Vector(-a.x, -a.y, -a.z)
end

function V.__eq(a, b)
	return a.x == b.x and a.y == b.y and a.z == b.z
end

function V.__tostring(a)
	return string.format("%.4f %.4f %.4f", a.x, a.y, a.z)
end

local A = {}
local ANGLE_ALIAS = { pitch = "p", yaw = "y", roll = "r" }

function A.__index(self, key)
	local real = ANGLE_ALIAS[key]
	if real then
		return rawget(self, real)
	end
	return A[key]
end

function A.__newindex(self, key, value)
	local real = ANGLE_ALIAS[key]
	if real then
		rawset(self, real, value)
		return
	end
	rawset(self, key, value)
end

function M.Angle(p, y, r)
	if type(p) == "table" then
		return M.Angle(p.p or p.pitch or p.x, p.y or p.yaw, p.r or p.roll or p.z)
	end
	return setmetatable({ p = p or 0, y = y or 0, r = r or 0 }, A)
end

function A:Forward()
	local f = angles.angle_vectors(self.p, self.y, self.r)
	return M.Vector(f.x, f.y, f.z)
end

function A:Right()
	local _, right = angles.angle_vectors(self.p, self.y, self.r)
	return M.Vector(right.x, right.y, right.z)
end

function A:Left()
	local right = self:Right()
	return M.Vector(-right.x, -right.y, -right.z)
end

function A:Up()
	local _, _, up = angles.angle_vectors(self.p, self.y, self.r)
	return M.Vector(up.x, up.y, up.z)
end

function A:Unpack()
	return self.p, self.y, self.r
end

function A.__add(a, b)
	return M.Angle(a.p + b.p, a.y + b.y, a.r + b.r)
end

function A.__mul(a, b)
	if type(b) == "number" then
		return M.Angle(a.p * b, a.y * b, a.r * b)
	end
	return M.Angle(a.p * b, a.y * b, a.r * b)
end

-- Stock weapon_medkit says NULL:IsPlayer and NULL:IsNPC return false.
-- The method IsValid is false too. A missing method stays missing.
local NULL = setmetatable({}, {
	__tostring = function()
		return "NULL"
	end,
	__index = {
		IsValid = function()
			return false
		end,
		IsPlayer = function()
			return false
		end,
		IsNPC = function()
			return false
		end,
		IsWorld = function()
			return false
		end,
		IsWeapon = function()
			return false
		end,
	},
})
M.NULL = NULL

function M.IsValid(o)
	if o == nil or o == NULL or o == false then
		return false
	end
	if type(o) == "table" and o.__invalid then
		return false
	end
	return true
end

function M.new_hook()
	local events = {}
	local hook = {}
	function hook.Add(event, name, fn)
		local list = events[event]
		if not list then
			list = {}
			events[event] = list
		end
		for i = 1, #list do
			if list[i].name == name then
				list[i].fn = fn
				return
			end
		end
		list[#list + 1] = { name = name, fn = fn }
	end
	function hook.Remove(event, name)
		local list = events[event]
		if not list then
			return
		end
		for i = #list, 1, -1 do
			if list[i].name == name then
				table.remove(list, i)
			end
		end
	end
	function hook.Run(event, ...)
		local list = events[event]
		if not list then
			return
		end
		for i = 1, #list do
			local a, b, c, d = list[i].fn(...)
			if a ~= nil then
				return a, b, c, d
			end
		end
	end
	function hook.GetTable()
		local copy = {}
		for event, list in pairs(events) do
			copy[event] = {}
			for i = 1, #list do
				copy[event][list[i].name] = list[i].fn
			end
		end
		return copy
	end
	-- Garry's Mod calls hook.Call. It is the same runner as hook.Run.
	hook.Call = hook.Run
	return hook
end

local SAFE = {
	"assert", "error", "ipairs", "next", "pairs", "pcall", "xpcall", "select",
	"tonumber", "tostring", "type", "unpack", "rawget", "rawset", "rawequal",
	"setmetatable", "getmetatable", "print",
}

function M.make_env(realm)
	local missing = {}
	local env = {}
	env._G = env
	env.Vector = M.Vector
	env.Angle = M.Angle
	env.IsValid = M.IsValid
	env.NULL = NULL
	env.hook = M.new_hook()
	env.SERVER = realm ~= "client"
	env.CLIENT = realm ~= "server"
	env.CurTime = function()
		return env.__curtime or 0
	end
	-- Server predicted hooks run once. Client prediction is not simulated.
	env.IsFirstTimePredicted = function()
		return realm ~= "client"
	end
	env.FrameTime = function()
		return env.__frametime or (1 / 66)
	end
	env.SysTime = function()
		return os.clock()
	end
	env.RealTime = env.SysTime
	env.Color = function(r, g, b, a)
		return { r = r or 255, g = g or 255, b = b or 255, a = a or 255 }
	end
	env.Either = function(c, a, b)
		if c then
			return a
		end
		return b
	end
	env.isvector = function(v)
		return type(v) == "table" and getmetatable(v) == V
	end
	env.isangle = function(v)
		return type(v) == "table" and getmetatable(v) == A
	end
	env.isstring = function(v)
		return type(v) == "string"
	end
	env.isnumber = function(v)
		return type(v) == "number"
	end
	-- NULL is an entity in GMod, not a table. weapons.Get copies tables and
	-- must keep this exact object or IsValid stops recognizing it.
	env.istable = function(v)
		if v == NULL then
			return false
		end
		return type(v) == "table"
	end
	-- GMod's IsColor is true for Color(r,g,b). Ours is a plain table, not a class.
	-- A Vector has x,y,z, so it does not pass.
	env.IsColor = function(v)
		return type(v) == "table" and type(v.r) == "number" and type(v.g) == "number" and type(v.b) == "number"
	end
	env.isfunction = function(v)
		return type(v) == "function"
	end
	env.isentity = function(v)
		return type(v) == "table" and v.__ent == true
	end
	env.MsgN = print
	env.Msg = function(s)
		io.write(tostring(s))
	end
	env.ErrorNoHalt = function(s)
		env.__errors = env.__errors or {}
		env.__errors[#env.__errors + 1] = tostring(s)
	end
	env.Include = nil
	for i = 1, #SAFE do
		local n = SAFE[i]
		env[n] = _G[n]
	end
	env.math = math
	env.string = string
	env.table = table
	env.bit = bit
	env.os = { clock = os.clock, time = os.time, date = os.date }
	local clock = { t = 0, items = {} }
	env.timer = {}
	function env.timer.Simple(delay, fn)
		clock.items[#clock.items + 1] = { t = clock.t + delay, fn = fn, once = true }
	end
	function env.timer.Create(name, delay, reps, fn)
		clock.items[#clock.items + 1] = { name = name, t = clock.t + delay, every = delay, reps = reps, left = reps, fn = fn }
	end
	function env.timer.Remove(name)
		for i = #clock.items, 1, -1 do
			if clock.items[i].name == name then
				table.remove(clock.items, i)
			end
		end
	end
	function env.__pump(dt)
		clock.t = clock.t + dt
		env.__curtime = clock.t
		for i = 1, #clock.items do
			local it = clock.items[i]
			if it and clock.t >= it.t then
				it.fn()
				if it.once then
					clock.items[i] = false
				elseif it.every then
					if it.left == 0 then
						it.t = clock.t + it.every
					else
						it.left = it.left - 1
						if it.left <= 0 then
							clock.items[i] = false
						else
							it.t = clock.t + it.every
						end
					end
				end
			end
		end
	end
	setmetatable(env, {
		__index = function(_, k)
			missing[k] = (missing[k] or 0) + 1
			return nil
		end,
	})
	env.__missing = missing
	return env
end

-- Garry's Mod compiles a small dialect: !=, &&, ||, unary !, // comments,
-- /* */ comments, and DEFINE_BASECLASS. Strings and -- comments are copied
-- through. continue is rewritten afterwards by rewrite_continue.
function M.gmod_preprocess(src)
	local n = #src
	local i = 1
	local out = {}
	local function emit(s)
		out[#out + 1] = s
	end
	while i <= n do
		local c = src:sub(i, i)
		local two = src:sub(i, i + 1)
		if two == "--" then
			if src:sub(i + 2, i + 3) == "[[" then
				local close = src:find("]]", i + 4, true)
				if not close then
					emit(src:sub(i))
					break
				end
				emit(src:sub(i, close + 1))
				i = close + 2
			else
				local nl = src:find("\n", i, true) or (n + 1)
				emit(src:sub(i, nl))
				i = nl + 1
			end
		elseif two == "//" then
			local nl = src:find("\n", i, true) or (n + 1)
			emit(" ")
			i = nl
		elseif two == "/*" then
			local close = src:find("*/", i + 2, true)
			if not close then
				emit(" ")
				break
			end
			emit(" ")
			i = close + 2
		elseif c == "[" and src:sub(i + 1, i + 1) == "[" then
			local close = src:find("]]", i + 2, true)
			if not close then
				emit(src:sub(i))
				break
			end
			emit(src:sub(i, close + 1))
			i = close + 2
		elseif c == '"' or c == "'" then
			local j = i + 1
			while j <= n do
				local d = src:sub(j, j)
				if d == "\\" then
					j = j + 2
				elseif d == c then
					j = j + 1
					break
				else
					j = j + 1
				end
			end
			emit(src:sub(i, j - 1))
			i = j
		elseif two == "!=" then
			emit("~=")
			i = i + 2
		elseif two == "&&" then
			emit(" and ")
			i = i + 2
		elseif two == "||" then
			emit(" or ")
			i = i + 2
		elseif c == "!" then
			emit(" not ")
			i = i + 1
		elseif c:match("[%a_]") then
			local j = i + 1
			while j <= n and src:sub(j, j):match("[%w_]") do
				j = j + 1
			end
			local word = src:sub(i, j - 1)
			if word == "DEFINE_BASECLASS" then
				emit("local BaseClass = baseclass.Get")
			else
				emit(word)
			end
			i = j
		else
			emit(c)
			i = i + 1
		end
	end
	return table.concat(out)
end

function M.load_file(path, env)
	local fh = io.open(path, "rb")
	if not fh then
		return false, "open failed " .. path
	end
	local src = fh:read("*a")
	fh:close()
	return M.load_source(path, src, env)
end

-- GMod's LuaJIT accepts `continue`. Stock LuaJIT does not.
-- Each continue becomes a goto. The label sits immediately before the
-- end/until that closes that loop. A label cannot follow a bare `break`
-- in the same block, so a real break inside a continue-loop is wrapped
-- as `do break end`. The break still leaves the loop; the label can follow.
function M.rewrite_continue(src)
	if not src:find("continue", 1, true) then
		return src, false
	end
	local n = #src
	local needs = {}

	local function walk(emit)
		local i = 1
		local stack = {}
		local loop_n = 0
		local function ident(c)
			return c and ((c >= 48 and c <= 57) or (c >= 65 and c <= 90) or (c >= 97 and c <= 122) or c == 95)
		end
		local function is_word_at(pos, len)
			local a = src:byte(pos - 1)
			-- `obj.continue` and `obj:continue` are field names, not the keyword.
			if a == 46 or a == 58 then
				return false
			end
			local b = src:byte(pos + len)
			return not ident(a) and not ident(b)
		end
		local function innermost_loop()
			for s = #stack, 1, -1 do
				if stack[s].loop then
					return stack[s]
				end
			end
		end
		while i <= n do
			local c = src:sub(i, i)
			if c == "-" and src:sub(i + 1, i + 1) == "-" then
				if src:sub(i + 2, i + 3) == "[[" then
					local close = src:find("]]", i + 4, true)
					if not close then
						emit(src:sub(i))
						break
					end
					emit(src:sub(i, close + 1))
					i = close + 2
				else
					local nl = src:find("\n", i, true) or (n + 1)
					emit(src:sub(i, nl))
					i = nl + 1
				end
			elseif c == "[" and src:sub(i + 1, i + 1) == "[" then
				local close = src:find("]]", i + 2, true)
				if not close then
					emit(src:sub(i))
					break
				end
				emit(src:sub(i, close + 1))
				i = close + 2
			elseif c == '"' or c == "'" then
				local j = i + 1
				while j <= n do
					local d = src:sub(j, j)
					if d == "\\" then
						j = j + 2
					elseif d == c then
						j = j + 1
						break
					else
						j = j + 1
					end
				end
				emit(src:sub(i, j - 1))
				i = j
			elseif c:match("[%a_]") then
				local j = i + 1
				while j <= n and src:sub(j, j):match("[%w_]") do
					j = j + 1
				end
				local word = src:sub(i, j - 1)
				if is_word_at(i, #word) then
					if word == "for" or word == "while" then
						loop_n = loop_n + 1
						-- The following `do` belongs to this loop; it is not its own block.
						stack[#stack + 1] = { loop = true, id = loop_n, expect_do = true }
						emit(word)
					elseif word == "repeat" then
						loop_n = loop_n + 1
						stack[#stack + 1] = { loop = true, id = loop_n }
						emit(word)
					elseif word == "do" then
						local top = stack[#stack]
						if top and top.expect_do then
							top.expect_do = false
							emit(word)
						else
							stack[#stack + 1] = { loop = false }
							emit(word)
						end
					elseif word == "function" or word == "if" then
						stack[#stack + 1] = { loop = false }
						emit(word)
					elseif word == "continue" then
						local top = innermost_loop()
						if top then
							needs[top.id] = true
							emit("goto __engine_c" .. top.id)
						else
							emit(word)
						end
					elseif word == "break" then
						local top = innermost_loop()
						if top and needs[top.id] then
							emit("do break end")
						else
							emit(word)
						end
					elseif word == "end" or word == "until" then
						local top = stack[#stack]
						stack[#stack] = nil
						if top and top.loop and needs[top.id] then
							emit("::__engine_c" .. top.id .. ":: ")
						end
						emit(word)
					else
						emit(word)
					end
					i = j
				else
					emit(c)
					i = i + 1
				end
			else
				emit(c)
				i = i + 1
			end
		end
	end

	walk(function() end)
	local out = {}
	walk(function(s)
		out[#out + 1] = s
	end)
	return table.concat(out), true
end

-- Compile only. The script host runs the chunk itself so a budget hook
-- does not count the preprocessor.
function M.compile(path, src, env)
	local body = M.gmod_preprocess(src)
	local rewritten = body ~= src
	if body:find("continue", 1, true) then
		local next_body, did = M.rewrite_continue(body)
		body = next_body
		rewritten = rewritten or did
	end
	local chunk, err = loadstring(body, "@" .. path)
	if not chunk then
		return false, err, rewritten
	end
	setfenv(chunk, env)
	return true, chunk, rewritten
end

function M.load_source(path, src, env)
	local ok, chunk, rewritten = M.compile(path, src, env)
	if not ok then
		return false, chunk, rewritten
	end
	local ran, runerr = pcall(chunk)
	if not ran then
		return false, runerr, rewritten
	end
	return true, nil, rewritten
end

return M
