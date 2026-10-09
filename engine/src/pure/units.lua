-- Source hammer unit is the international inch.
-- 1 inch = 0.0254 m exactly (International Yard and Pound Agreement, 1959).
-- Valve documents a Source unit as 1 inch.
local M = {}

M.METERS_PER_INCH = 0.0254
M.SOURCE_PER_METER = 1 / M.METERS_PER_INCH -- 39.37007874015748...

-- gVRMod comfort scale (vrmod_scale default). Not the inch.
-- 1 real meter of headset motion = 32.7 source units, so the world feels larger than life.
M.VRMOD_VIEW_SCALE = 32.7

-- HL2 / GMod sv_gravity. Heavier than Earth: 600 in/s^2 = 15.24 m/s^2.
M.SV_GRAVITY = 600
M.EARTH_GRAVITY_MPS2 = 9.80665

function M.source_to_meters(units)
	return units * M.METERS_PER_INCH
end

function M.meters_to_source(meters)
	return meters * M.SOURCE_PER_METER
end

function M.gravity_mps2(sv_gravity)
	return (sv_gravity or M.SV_GRAVITY) * M.METERS_PER_INCH
end

-- Headset meters to source units using the product view scale (default 32.7).
function M.headset_meters_to_source(meters, view_scale)
	return meters * (view_scale or M.VRMOD_VIEW_SCALE)
end

return M
