-- Open prop solver tuned to Source's published defaults.
-- This is not Havok. VPhysics in the engine binary is proprietary.
-- Defaults match scripts/surfaceproperties_manifest / the stock "default" surface:
--   density 2000 kg/m^3, elasticity 0.25, friction 0.8, gravity sv_gravity 600 u/s^2.
-- Mass uses the exact inch: V_m3 = (size_units * 0.0254)^3, mass = density * V_m3.
--
-- Integrator (resting slide and bounce tests lock this, not a second hidden solver):
--   v.z -= g * dt
--   p   += v * dt          semi-implicit Euler
--   if the AABB penetrates a horizontal floor:
--       snap, v.z = -e * v.z when incoming v.z < 0
--       horizontal speed bleeds by mu * g * dt (Coulomb, gravity-normal = g)
local units = require("pure.units")

local M = {}

M.DEFAULT_DENSITY = 2000
M.DEFAULT_ELASTICITY = 0.25
M.DEFAULT_FRICTION = 0.8
M.GRAVITY = 600

function M.mass_kg(hx, hy, hz, density)
	local sx = (hx * 2) * units.METERS_PER_INCH
	local sy = (hy * 2) * units.METERS_PER_INCH
	local sz = (hz * 2) * units.METERS_PER_INCH
	return (density or M.DEFAULT_DENSITY) * sx * sy * sz
end

function M.new_box(cx, cy, cz, hx, hy, hz)
	return {
		pos = { x = cx, y = cy, z = cz },
		vel = { x = 0, y = 0, z = 0 },
		half = { x = hx, y = hy, z = hz },
		elasticity = M.DEFAULT_ELASTICITY,
		friction = M.DEFAULT_FRICTION,
		gravity = M.GRAVITY,
		mass = M.mass_kg(hx, hy, hz, M.DEFAULT_DENSITY),
		floor_z = nil, -- set to enable the ground plane
	}
end

function M.step(body, dt)
	local v = body.vel
	local p = body.pos
	v.z = v.z - body.gravity * dt
	p.x = p.x + v.x * dt
	p.y = p.y + v.y * dt
	p.z = p.z + v.z * dt
	if body.floor_z ~= nil then
		local bottom = p.z - body.half.z
		if bottom < body.floor_z then
			p.z = body.floor_z + body.half.z
			if v.z < 0 then
				v.z = -body.elasticity * v.z
			end
			local h = math.sqrt(v.x * v.x + v.y * v.y)
			local bleed = body.friction * body.gravity * dt
			if h > 1e-8 then
				local nh = h - bleed
				if nh < 0 then
					nh = 0
				end
				local s = nh / h
				v.x = v.x * s
				v.y = v.y * s
			end
		end
	end
	return body
end

-- Continuous Coulomb slide distance from speed v0 to rest: v0^2 / (2 * mu * g).
function M.slide_distance(v0, friction, gravity)
	local a = (friction or M.DEFAULT_FRICTION) * (gravity or M.GRAVITY)
	if a <= 0 then
		return math.huge
	end
	return (v0 * v0) / (2 * a)
end

function M.slide_time(v0, friction, gravity)
	local a = (friction or M.DEFAULT_FRICTION) * (gravity or M.GRAVITY)
	return v0 / a
end

-- Impact speed after a drop from rest through height H, continuous: sqrt(2 g H).
function M.impact_speed(height, gravity)
	return math.sqrt(2 * (gravity or M.GRAVITY) * height)
end

return M
