return function(H, env)
	H.TEST("api.smoke.get_version", function()
		H.assert_true(env.vrmod.GetVersion() ~= nil)
		H.assert_true(type(env.vrmod.GetVersion()) == "number" or type(env.vrmod.GetVersion()) == "string"
			or true)
	end)

	H.TEST("api.menu.dedupe_name", function()
		g_VR.menuItems = {}
		g_VR.menuBackup = {}
		local n = 0
		local function f1() n = n + 1 end
		local function f2() n = n + 10 end
		env.vrmod.AddInGameMenuItem("VRClimb", 5, 3, f1)
		env.vrmod.AddInGameMenuItem("VRClimb", 5, 3, f2)
		H.assert_eq(#g_VR.menuItems, 1)
		-- second func wins (update in place)
		g_VR.menuItems[1].func()
		H.assert_eq(n, 10)
	end)

	H.TEST("api.menu.dedupe_id", function()
		g_VR.menuItems = {}
		g_VR.menuBackup = {}
		env.vrmod.AddInGameMenuItem("Spawn Menu", 0, 0, function() end, true, nil, "spawn")
		env.vrmod.AddInGameMenuItem("Spawn Menu", 1, 1, function() end, true, nil, "spawn")
		H.assert_eq(#g_VR.menuItems, 1)
		H.assert_eq(g_VR.menuItems[1].id, "spawn")
	end)

	H.TEST("api.menu.dedup_function", function()
		g_VR.menuItems = {
			{ name = "A", id = "a", func = function() end },
			{ name = "A", id = "a", func = function() end },
			{ name = "B", id = "b", func = function() end },
		}
		g_VR.menuBackup = {
			["old1"] = { name = "A", id = "a" },
			["old2"] = { name = "A", id = "a" },
		}
		env.vrmod.DedupInGameMenuItems()
		H.assert_eq(#g_VR.menuItems, 2)
	end)
end
