return function(H, env)
	local u = env.vrmod.utils

	H.TEST("util.workshop_map.hold", function()
		H.assert_true(not u.WorkshopMap_PassMapOnCommandLine())

		local downloading = u.WorkshopMap_Decide({
			want_map = "cs_office",
			elapsed_sec = 30,
			workshop_busy = true,
			workshop_idle_seen = false,
		})
		H.assert_eq(downloading.action, "wait")
		H.assert_eq(downloading.reason, "workshop_downloading")
		H.assert_true(not downloading.pass_map_on_command_line)

		local early = u.WorkshopMap_Decide({
			want_map = "gm_construct",
			elapsed_sec = 1,
			workshop_busy = false,
			workshop_idle_seen = false,
		})
		H.assert_eq(early.action, "wait")
		H.assert_eq(early.reason, "workshop_settle")

		local ready = u.WorkshopMap_Decide({
			want_map = "maps/gm_construct.bsp",
			elapsed_sec = 1,
			workshop_busy = false,
			workshop_idle_seen = true,
		})
		H.assert_eq(ready.action, "load")
		H.assert_eq(ready.reason, "addons_ready")
		H.assert_eq(ready.want_map, "gm_construct")

		local settling = u.WorkshopMap_Decide({
			want_map = "de_dust2",
			elapsed_sec = 8,
			workshop_busy = false,
			workshop_idle_seen = false,
		})
		H.assert_eq(settling.action, "wait")
		H.assert_eq(settling.reason, "workshop_settle")

		-- No WorkshopEnd and no addon list. Quiet long enough still loads.
		-- Waiting forever here left Cube on the main menu.
		local settled = u.WorkshopMap_Decide({
			want_map = "de_dust2",
			elapsed_sec = 30,
			workshop_busy = false,
			workshop_idle_seen = false,
		})
		H.assert_eq(settled.action, "load")
		H.assert_eq(settled.reason, "addons_ready")

		local fresh_quiet = u.WorkshopMap_Decide({
			want_map = "de_dust2",
			elapsed_sec = 30,
			quiet_sec = 0,
			workshop_busy = false,
			workshop_idle_seen = false,
		})
		H.assert_eq(fresh_quiet.action, "wait")
		H.assert_eq(fresh_quiet.reason, "workshop_settle")

		local nolist = u.WorkshopMap_Decide({
			want_map = "cs_office",
			elapsed_sec = 4,
			list_ready = false,
			workshop_idle_seen = false,
			pending_downloads = 0,
		})
		H.assert_eq(nolist.action, "wait")
		H.assert_eq(nolist.reason, "list_not_ready")

		local nolist_quiet = u.WorkshopMap_Decide({
			want_map = "cs_office",
			elapsed_sec = 20,
			list_ready = false,
			quiet_sec = 12,
			pending_downloads = 0,
			workshop_idle_seen = false,
		})
		H.assert_eq(nolist_quiet.action, "load")
		H.assert_eq(nolist_quiet.reason, "addons_ready")

		local pending_unlisted = u.WorkshopMap_Decide({
			want_map = "cs_office",
			elapsed_sec = 40,
			list_ready = false,
			pending_downloads = 2,
			quiet_sec = 40,
		})
		H.assert_eq(pending_unlisted.action, "wait")
		H.assert_eq(pending_unlisted.reason, "workshop_downloading")

		local cached_list = u.WorkshopMap_Decide({
			want_map = "cs_office",
			elapsed_sec = 4,
			list_ready = true,
			addon_count = 352,
			pending_downloads = 0,
			quiet_sec = 4,
			require_bsp = true,
			bsp_visible = true,
		})
		H.assert_eq(cached_list.action, "wait")
		H.assert_eq(cached_list.reason, "workshop_confirm")

		local held = u.WorkshopMap_Decide({
			want_map = "cs_office",
			elapsed_sec = 12,
			list_ready = true,
			addon_count = 352,
			pending_downloads = 0,
			quiet_sec = 12,
			require_bsp = true,
			bsp_visible = true,
		})
		H.assert_eq(held.action, "load")
		H.assert_eq(held.reason, "addons_ready")

		local just_ended = u.WorkshopMap_Decide({
			want_map = "cs_office",
			elapsed_sec = 20,
			workshop_idle_seen = true,
			list_ready = true,
			addon_count = 352,
			pending_downloads = 0,
			quiet_sec = 0.4,
			require_bsp = true,
			bsp_visible = true,
		})
		H.assert_eq(just_ended.action, "wait")
		H.assert_eq(just_ended.reason, "workshop_confirm")

		local subs = u.WorkshopMap_Decide({
			want_map = "cs_office",
			elapsed_sec = 40,
			subscriptions_busy = true,
			list_ready = true,
			addon_count = 352,
			pending_downloads = 0,
			quiet_sec = 40,
			require_bsp = true,
			bsp_visible = true,
		})
		H.assert_eq(subs.action, "wait")
		H.assert_eq(subs.reason, "workshop_downloading")

		local pending = u.WorkshopMap_Decide({
			want_map = "cs_office",
			elapsed_sec = 20,
			list_ready = true,
			addon_count = 12,
			pending_downloads = 3,
			workshop_busy = false,
			require_bsp = true,
			bsp_visible = true,
		})
		H.assert_eq(pending.action, "wait")
		H.assert_eq(pending.reason, "workshop_downloading")

		local empty = u.WorkshopMap_Decide({
			want_map = "gm_flatgrass",
			elapsed_sec = 1,
			list_ready = true,
			addon_count = 0,
			pending_downloads = 0,
			require_bsp = true,
			bsp_visible = true,
		})
		H.assert_eq(empty.action, "wait")
		H.assert_eq(empty.reason, "list_settle")

		local cached = u.WorkshopMap_Decide({
			want_map = "gm_flatgrass",
			current_map = "",
			elapsed_sec = 6,
			list_ready = true,
			addon_count = 0,
			pending_downloads = 0,
			require_bsp = true,
			bsp_visible = true,
		})
		H.assert_eq(cached.action, "load")

		local on = u.WorkshopMap_Decide({
			want_map = "cs_office",
			current_map = "cs_office",
			elapsed_sec = 0,
			workshop_busy = true,
		})
		H.assert_eq(on.action, "already")

		local nobsp = u.WorkshopMap_Decide({
			want_map = "cs_office",
			elapsed_sec = 10,
			workshop_idle_seen = true,
			require_bsp = true,
			bsp_visible = false,
		})
		H.assert_eq(nobsp.action, "wait")
		H.assert_eq(nobsp.reason, "bsp_missing")

		local bad = u.WorkshopMap_Decide({ want_map = "gm_construct; quit" })
		H.assert_eq(bad.action, "idle")
		H.assert_eq(bad.reason, "bad_map")
	end)
end
