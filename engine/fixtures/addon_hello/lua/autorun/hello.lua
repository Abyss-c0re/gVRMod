-- Tiny addon that stays inside the compatibility surface.
local origin = Vector(0, 0, 64)
local look = Angle(0, 90, 0)
local forward = look:Forward()
hook.Add("EngineHello", "fixture", function()
	if not IsValid(NULL) and forward.y > 0.5 and origin.z == 64 then
		return "hello"
	end
end)
