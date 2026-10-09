return function(T)
	local units = require("pure.units")
	local coords = require("pure.coords")
	local angles = require("pure.angles")

	-- International inch. 100 source units = 2.54 m exactly.
	T.near(units.source_to_meters(100), 2.54, 1e-12, "100 units")
	T.near(units.SOURCE_PER_METER, 1 / 0.0254, 0, "inches per meter")
	T.near(units.meters_to_source(1), 39.37007874015748, 1e-9, "1 m in inches")
	-- 600 in/s^2 = 15.24 m/s^2, heavier than Earth.
	T.near(units.gravity_mps2(600), 15.24, 1e-12, "sv_gravity 600")
	T.ok(units.VRMOD_VIEW_SCALE < units.SOURCE_PER_METER, "comfort scale is below life size")
	T.near(units.headset_meters_to_source(1, 32.7), 32.7, 0, "vrmod scale")

	-- ConvertXrPose position cases from tests/test_input.cpp.
	local x, y, z = coords.xr_to_source_axes(0, 0, -1)
	T.near(x, 1, 1e-9, "forward x")
	T.near(y, 0, 1e-9, "forward y")
	T.near(z, 0, 1e-9, "forward z")
	x, y, z = coords.xr_to_source_axes(0, 1, 0)
	T.near(z, 1, 1e-9, "up")
	x, y, z = coords.xr_to_source_axes(1, 0, 0)
	T.near(y, -1, 1e-9, "right is -Y")

	-- det +1 and orthonormal columns: the remap is a rotation, so it round-trips.
	T.near(coords.det(), 1, 1e-9, "det")
	T.ok(coords.columns_orthonormal(), "orthonormal")
	local samples = {
		{ 0, 0, 0 },
		{ 1, 0, 0 },
		{ 0, 1, 0 },
		{ 0, 0, 1 },
		{ 3.5, -2, 64 },
		{ -100, 40, 0.25 },
	}
	for i = 1, #samples do
		local s = samples[i]
		local a, b, c = coords.source_axes_to_xr(s[1], s[2], s[3])
		local sx, sy, sz = coords.xr_to_source_axes(a, b, c)
		T.near(sx, s[1], 1e-9, "roundtrip x " .. i)
		T.near(sy, s[2], 1e-9, "roundtrip y " .. i)
		T.near(sz, s[3], 1e-9, "roundtrip z " .. i)
		local lx, ly, lz = coords.source_to_lovr(s[1], s[2], s[3], 32.7)
		sx, sy, sz = coords.lovr_to_source(lx, ly, lz, 32.7)
		T.near(sx, s[1], 1e-6, "scale roundtrip x")
		T.near(sy, s[2], 1e-6, "scale roundtrip y")
		T.near(sz, s[3], 1e-6, "scale roundtrip z")
	end

	-- IPD 64 mm at vrmod scale is 2.0928 source units. Life scale is 0.064/0.0254.
	T.near(0.064 * 32.7, 2.0928, 1e-9, "ipd comfort units")
	T.near(units.meters_to_source(0.064), 2.51968503937, 1e-6, "ipd inches")

	-- Eye centers sit on head-right, half an IPD apart, and the midpoint is the head.
	local lx, ly, lz = coords.eye_center(0, 0, 64, 0, -1, 0.064, 32.7)
	local rx, ry, rz = coords.eye_center(0, 0, 64, 0, 1, 0.064, 32.7)
	T.near(rx - lx, 0.064, 1e-9, "ipd along +X at yaw 0")
	T.near(ry, ly, 1e-9, "eyes level")
	T.near(rz, lz, 1e-9, "eyes same depth")
	local hx, hy, hz = coords.source_to_lovr(0, 0, 64, 32.7)
	T.near((lx + rx) / 2, hx, 1e-9, "mid x")
	T.near((ly + ry) / 2, hy, 1e-9, "mid y")
	T.near((lz + rz) / 2, hz, 1e-9, "mid z")

	-- Yaw 0: forward +X, right -Y, up +Z. Pitch 90 looks down (-Z).
	local f, r, u = angles.angle_vectors(0, 0, 0)
	T.near(f.x, 1, 1e-9, "fwd")
	T.near(r.y, -1, 1e-9, "right")
	T.near(u.z, 1, 1e-9, "up")
	f = angles.angle_vectors(0, 90, 0)
	T.near(f.y, 1, 1e-9, "yaw 90")
	T.near(f.x, 0, 1e-9, "yaw 90 x")
	f = angles.angle_vectors(90, 0, 0)
	T.near(f.z, -1, 1e-9, "pitch down")

	-- VectorAngles: straight up is pitch 270, matching the SDK branch.
	local p, yaw = angles.vector_angles(0, 0, 1)
	T.near(p, 270, 1e-6, "up pitch")
	T.near(yaw, 0, 1e-6, "up yaw")
	p, yaw = angles.vector_angles(1, 0, 0)
	T.near(p, 0, 1e-6, "fwd pitch")
	T.near(yaw, 0, 1e-6, "fwd yaw")

	-- Stereo disparity. 90° vertical FOV, height 480, IPD 0.064, depth 2 m.
	-- f = 240 / tan(45°) = 240. disparity = 240 * 0.064 / 2 = 7.68 px.
	local dpx = coords.stereo_disparity_px(0.064, 2, 90, 480)
	T.near(dpx, 7.68, 1e-9, "disparity")
	local left_x = coords.project(0.032, 0, -2, 90, 640, 480)
	local right_x = coords.project(-0.032, 0, -2, 90, 640, 480)
	T.near(left_x - right_x, dpx, 1e-6, "project matches disparity")
end
