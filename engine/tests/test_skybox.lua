-- gl_warp.cpp MakeSkyVec. Source X forward, Y left, Z up. No textures required.
return function(T)
	local skybox = require("pure.skybox")
	T.eq(skybox.face_suffix(1), "rt", "draw axis 1 uses rt")
	T.eq(skybox.face_suffix(2), "lf", "draw axis 2 uses lf")
	T.eq(skybox.face_suffix(3), "bk", "draw axis 3 uses bk")
	T.eq(skybox.face_suffix(4), "ft", "draw axis 4 uses ft")
	T.eq(skybox.face_suffix(5), "up", "draw axis 5 uses up")
	T.eq(skybox.face_suffix(6), "dn", "draw axis 6 uses dn")

	local x, y, z, u, v = skybox.make_sky_vec(-1, -1, 1, 1)
	T.near(x, 1, 1e-6, "rt lower s x")
	T.near(y, 1, 1e-6, "rt lower s y")
	T.near(z, -1, 1e-6, "rt lower s z")
	T.near(u, 1 / 512, 1e-6, "rt corner u inset")
	T.near(v, 511 / 512, 1e-6, "rt corner v flip")

	x, y, z, u, v = skybox.make_sky_vec(0, 0, 5, 4)
	T.near(x, 0, 1e-6, "up center x")
	T.near(y, 0, 1e-6, "up center y")
	T.near(z, 4, 1e-6, "up center is +Z")
	T.near(u, 0.5, 1e-6, "up center u")
	T.near(v, 0.5, 1e-6, "up center v")

	local tris = skybox.face_tris(5, 4)
	T.eq(#tris, 30, "sky face is two triangles")
	T.near(tris[3], 4, 1e-6, "first up corner is +Z at the s=-1 t=-1 edge")
end
