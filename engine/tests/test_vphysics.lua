return function(T)
	local phys = require("pure.vphysics")
	local units = require("pure.units")

	-- 1x1x1 inch cube at 2000 kg/m^3.
	-- volume = 0.0254^3 m^3. mass = 2000 * that.
	local inch = units.METERS_PER_INCH
	local mass = phys.mass_kg(0.5, 0.5, 0.5, 2000)
	T.near(mass, 2000 * inch * inch * inch, 1e-12, "unit cube mass")

	-- Continuous slide: mu 0.8, g 600, v0 480 → a = 480, stop in 1 s, distance 240.
	T.near(phys.slide_time(480, 0.8, 600), 1, 1e-12, "slide time")
	T.near(phys.slide_distance(480, 0.8, 600), 240, 1e-9, "slide distance")

	-- Discrete solver must track the semi-implicit sum, and stay near the continuous distance.
	local body = phys.new_box(0, 0, 8, 8, 8, 8)
	body.floor_z = 0
	body.vel.x = 480
	body.elasticity = 0
	local dt = 1 / 66
	local steps = 66
	-- Same order as vphysics.step: move with the current speed, then bleed.
	local predicted = 0
	local v = 480
	for _ = 1, steps do
		predicted = predicted + v * dt
		local bleed = 0.8 * 600 * dt
		v = v - bleed
		if v < 0 then
			v = 0
		end
	end
	for _ = 1, steps do
		phys.step(body, dt)
	end
	T.near(body.vel.x, 0, 1e-3, "stopped")
	T.near(body.pos.x, predicted, 1e-3, "matches discrete spec")
	T.near(body.pos.x, 240, 5, "within a few units of continuous 240")
	T.near(body.pos.z, 8, 1e-6, "rests on the floor")

	-- Drop. Impact speed approaches sqrt(2 g H). Rebound vertical speed is e times that.
	local H = 100
	local drop = phys.new_box(0, 0, H + 8, 8, 8, 8)
	drop.floor_z = 0
	drop.friction = 0
	drop.elasticity = 0.25
	local impact = 0
	for _ = 1, 4000 do
		local before = drop.vel.z
		phys.step(drop, 1 / 1000)
		if drop.pos.z <= 8 + 1e-4 and before < 0 and drop.vel.z > 0 then
			impact = -before
			break
		end
	end
	local ideal = phys.impact_speed(H, 600)
	T.ok(impact > 0, "hit the floor")
	T.near(impact, ideal, ideal * 0.02, "impact within 2% of sqrt(2 g H)")
end
