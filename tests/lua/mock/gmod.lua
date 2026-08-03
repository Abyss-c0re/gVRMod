-- Minimal GMod stubs for offline Lua unit tests (LuaJIT / Lua 5.1)
local M = {}

local function clamp(x, a, b)
	if x < a then return a end
	if x > b then return b end
	return x
end

function M.install(_G)
	_G.SERVER = false
	_G.CLIENT = true
	_G.ScrW = function() return 1920 end
	_G.ScrH = function() return 1080 end
	_G.CurTime = function() return _G.__test_time or 0 end
	_G.FrameNumber = function() return _G.__test_frame or 0 end
	_G.FrameTime = function() return 1 / 90 end
	_G.RealTime = _G.CurTime
	_G.isfunction = function(v) return type(v) == "function" end
	_G.isnumber = function(v) return type(v) == "number" end
	_G.isstring = function(v) return type(v) == "string" end
	_G.istable = function(v) return type(v) == "table" end
	_G.isentity = function() return false end
	_G.IsValid = function(v)
		if v == nil then return false end
		if type(v) == "table" and v.__invalid then return false end
		return true
	end
	_G.IsColor = function(v) return type(v) == "table" and v.r ~= nil and v.g ~= nil and v.b ~= nil end
	_G.unpack = table.unpack or unpack
	_G.bit = bit or {
		band = function(a, b) return a % (b * 2) -- simplified
		end,
	}
	if not bit then
		_G.bit = {
			band = function(a, b)
				local r, p = 0, 1
				a, b = math.floor(a), math.floor(b)
				while a > 0 and b > 0 do
					if a % 2 == 1 and b % 2 == 1 then r = r + p end
					a, b, p = math.floor(a / 2), math.floor(b / 2), p * 2
				end
				return r
			end,
		}
	end

	math.Clamp = math.Clamp or clamp
	math.NormalizeAngle = math.NormalizeAngle or function(a)
		while a > 180 do a = a - 360 end
		while a < -180 do a = a + 360 end
		return a
	end
	math.AngleDifference = math.AngleDifference or function(a, b)
		local d = math.NormalizeAngle(a - b)
		return d
	end

	local Vector_mt = {}
	Vector_mt.__index = Vector_mt
	function Vector_mt:DistToSqr(o)
		local dx, dy, dz = self.x - o.x, self.y - o.y, self.z - o.z
		return dx * dx + dy * dy + dz * dz
	end
	function Vector_mt:Length()
		return math.sqrt(self.x * self.x + self.y * self.y + self.z * self.z)
	end
	function Vector_mt:LengthSqr()
		return self.x * self.x + self.y * self.y + self.z * self.z
	end
	function Vector_mt:__add(o)
		return Vector(self.x + o.x, self.y + o.y, self.z + o.z)
	end
	function Vector_mt:__sub(o)
		return Vector(self.x - o.x, self.y - o.y, self.z - o.z)
	end
	function Vector_mt:__mul(s)
		if type(s) == "number" then return Vector(self.x * s, self.y * s, self.z * s) end
		return Vector(self.x * s.x, self.y * s.y, self.z * s.z)
	end
	function Vector_mt:__unm()
		return Vector(-self.x, -self.y, -self.z)
	end
	function Vector_mt:Angle()
		local l = math.sqrt(self.x * self.x + self.y * self.y)
		local pitch = math.deg(math.atan2(-self.z, l))
		local yaw = math.deg(math.atan2(self.y, self.x))
		return Angle(pitch, yaw, 0)
	end
	function Vector_mt:Set(o)
		self.x, self.y, self.z = o.x, o.y, o.z
	end
	function Vector_mt:GetNormalized()
		local l = self:Length()
		if l < 1e-8 then return Vector(0, 0, 0) end
		return Vector(self.x / l, self.y / l, self.z / l)
	end
	function Vector_mt:Forward() -- not on Vector; keep for safety
		return Vector(0, 0, 0)
	end

	function _G.Vector(x, y, z)
		if type(x) == "table" and x.x then
			return setmetatable({ x = x.x, y = x.y, z = x.z }, Vector_mt)
		end
		return setmetatable({ x = x or 0, y = y or 0, z = z or 0 }, Vector_mt)
	end
	_G.isvector = function(v) return type(v) == "table" and getmetatable(v) == Vector_mt end

	local Angle_mt = {}
	Angle_mt.__index = Angle_mt
	function Angle_mt:__add(o)
		return Angle(self.p + o.p, self.y + o.y, self.r + o.r)
	end
	function Angle_mt:__sub(o)
		return Angle(self.p - o.p, self.y - o.y, self.r - o.r)
	end
	function Angle_mt:__mul(s)
		return Angle(self.p * s, self.y * s, self.r * s)
	end
	function Angle_mt:Forward()
		local p, y = math.rad(self.p), math.rad(self.y)
		local cp = math.cos(p)
		return Vector(cp * math.cos(y), cp * math.sin(y), -math.sin(p))
	end
	function Angle_mt:Right()
		local y = math.rad(self.y)
		return Vector(math.sin(y), -math.cos(y), 0)
	end
	function Angle_mt:Up()
		return Vector(0, 0, 1)
	end
	function Angle_mt:Set(o)
		self.p, self.y, self.r = o.p, o.y, o.r
	end

	function _G.Angle(p, y, r)
		if type(p) == "table" and p.p then
			return setmetatable({ p = p.p, y = p.y, r = p.r, pitch = p.p, yaw = p.y, roll = p.r }, Angle_mt)
		end
		p, y, r = p or 0, y or 0, r or 0
		return setmetatable({ p = p, y = y, r = r, pitch = p, yaw = y, roll = r }, Angle_mt)
	end
	_G.isangle = function(v) return type(v) == "table" and getmetatable(v) == Angle_mt end

	function _G.Color(r, g, b, a)
		return { r = r or 255, g = g or 255, b = b or 255, a = a or 255 }
	end

	function _G.Lerp(t, a, b)
		return a + (b - a) * t
	end
	function _G.LerpVector(t, a, b)
		return a + (b - a) * t
	end
	function _G.LerpAngle(t, a, b)
		return Angle(
			a.p + math.AngleDifference(b.p, a.p) * t,
			a.y + math.AngleDifference(b.y, a.y) * t,
			a.r + math.AngleDifference(b.r, a.r) * t
		)
	end

	_G.hook = {
		_t = {},
		Add = function(ev, id, fn)
			_G.hook._t[ev] = _G.hook._t[ev] or {}
			_G.hook._t[ev][id] = fn
		end,
		Remove = function(ev, id)
			if _G.hook._t[ev] then _G.hook._t[ev][id] = nil end
		end,
		Call = function(ev, ...)
			local h = _G.hook._t[ev]
			if not h then return end
			for _, fn in pairs(h) do fn(...) end
		end,
		GetTable = function() return _G.hook._t end,
	}

	local convars = {}
	_G.CreateClientConVar = function(name, def, ...)
		convars[name] = convars[name] or { v = tostring(def) }
		local c = convars[name]
		return {
			GetString = function() return c.v end,
			GetInt = function() return tonumber(c.v) or 0 end,
			GetFloat = function() return tonumber(c.v) or 0 end,
			GetBool = function() return c.v == "1" or c.v == "true" end,
			SetString = function(_, s) c.v = tostring(s) end,
			SetInt = function(_, n) c.v = tostring(n) end,
			SetFloat = function(_, n) c.v = tostring(n) end,
			GetFlags = function() return 0 end,
		}
	end
	_G.CreateConVar = _G.CreateClientConVar
	_G.GetConVar = function(name)
		if not convars[name] then return nil end
		return _G.CreateClientConVar(name, convars[name].v)
	end
	_G.RunConsoleCommand = function() end
	_G.cvars = {
		AddChangeCallback = function() end,
		RemoveChangeCallback = function() end,
	}

	_G.util = {
		TraceHull = function() return { Hit = false, StartSolid = false, AllSolid = false } end,
		TraceLine = function() return { Hit = false } end,
	}
	_G.system = {
		IsWindows = function() return false end,
		IsLinux = function() return true end,
		HasFocus = function() return true end,
	}
	_G.player = { GetAll = function() return {} end }
	_G.ents = { FindByClass = function() return {} end }
	_G.game = { SinglePlayer = function() return true end, GetMap = function() return "gm_construct" end }
	_G.permissions = { EnableVoiceChat = function() end }
	_G.net = { Start = function() end, SendToServer = function() end, Receive = function() end, WriteVector = function() end, WriteBool = function() end, WriteFloat = function() end, WriteUInt = function() end, WriteTable = function() end, ReadTable = function() return {} end }
	_G.util.AddNetworkString = function() end
	_G.concommand = { Add = function() end }
	_G.Material = function() return { IsError = function() return true end } end
	_G.CreateMaterial = function() return { IsError = function() return true end, SetTexture = function() end } end
	_G.GetRenderTarget = function() return { IsValid = function() return true end, GetName = function() return "rt" end } end
	_G.render = {
		PushRenderTarget = function() end,
		PopRenderTarget = function() end,
		Clear = function() end,
		RenderView = function() end,
		CullMode = function() end,
	}
	_G.surface = {
		SetDrawColor = function() end,
		SetMaterial = function() end,
		DrawTexturedRectUV = function() end,
		CreateFont = function() end,
	}
	_G.cam = {}
	_G.draw = {}
	_G.LocalToWorld = function(lp, la, op, oa)
		return (op or Vector()) + (lp or Vector()), la or Angle()
	end
	_G.WorldToLocal = function(wp, wa, op, oa)
		return (wp or Vector()) - (op or Vector()), wa or Angle()
	end
	_G.file = {
		Exists = function() return false end,
		CreateDir = function() end,
		Write = function() end,
		Read = function() return nil end,
		Find = function() return {}, {} end,
	}
	_G.timer = {
		Simple = function(_, fn) if fn then fn() end end,
		Create = function() end,
		Remove = function() end,
	}
	_G.LocalPlayer = function() return nil end
	_G.MsgC = function() end
	_G.print = print
	_G.FCVAR_ARCHIVE = 128
	_G.FCVAR_REPLICATED = 8192
	_G.MASK_SOLID_BRUSHONLY = 0
	_G.TEXT_ALIGN_LEFT = 0
	_G.TEXT_ALIGN_CENTER = 1
	_G.TEXT_ALIGN_RIGHT = 2
	_G.TEXT_ALIGN_TOP = 3
end

return M
