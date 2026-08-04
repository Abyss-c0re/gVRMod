return function(H, env)
	H.TEST("util.experience.should_run_first_time", function()
		H.assert_true(env.vrmod.Experience_ShouldRunFromState({
			force = false,
			enabled = true,
			complete = false,
			native_wrapper = false,
			has_prior_cal = false,
		}))
	end)

	H.TEST("util.experience.skip_when_complete", function()
		H.assert_true(not env.vrmod.Experience_ShouldRunFromState({
			force = false,
			enabled = true,
			complete = true,
			native_wrapper = true,
			has_prior_cal = true,
		}))
	end)

	H.TEST("util.experience.g10_wrapper_plus_cal_skips", function()
		-- G10: native wrapper re-entry with Vision cal already on disk
		H.assert_true(not env.vrmod.Experience_ShouldRunFromState({
			force = false,
			enabled = true,
			complete = false,
			native_wrapper = true,
			has_prior_cal = true,
		}))
	end)

	H.TEST("util.experience.wrapper_without_cal_still_runs", function()
		H.assert_true(env.vrmod.Experience_ShouldRunFromState({
			force = false,
			enabled = true,
			complete = false,
			native_wrapper = true,
			has_prior_cal = false,
		}))
	end)

	H.TEST("util.experience.force_overrides_skip", function()
		H.assert_true(env.vrmod.Experience_ShouldRunFromState({
			force = true,
			enabled = true,
			complete = true,
			native_wrapper = true,
			has_prior_cal = true,
		}))
	end)

	H.TEST("util.experience.disabled_never_runs", function()
		H.assert_true(not env.vrmod.Experience_ShouldRunFromState({
			force = false,
			enabled = false,
			complete = false,
			native_wrapper = false,
			has_prior_cal = false,
		}))
	end)
end
