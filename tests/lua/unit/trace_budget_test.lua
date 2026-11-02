-- Trace queue budget: main thread may drain at most 8 engine traces per frame.
return function(H, env)
	local u = env.vrmod.utils

	H.TEST("util.trace_budget_law.admit", function()
		H.assert_eq(u.TraceBudget_MaxPerFrame(), 8)

		local function admit(used, requested, max)
			local run, defer = u.TraceBudget_Admit(used, requested, max)
			H.assert_eq(type(run), "number", "run type")
			H.assert_eq(type(defer), "number", "defer type")
			return run, defer
		end

		local run, defer = admit(0, 3, 8)
		H.assert_eq(run, 3)
		H.assert_eq(defer, 0)

		run, defer = admit(6, 5, 8)
		H.assert_eq(run, 2)
		H.assert_eq(defer, 3)
		H.assert_true(6 + run <= 8)

		run, defer = admit(8, 4, 8)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 4)

		run, defer = admit(10, 2, 8)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 2)

		run, defer = admit(0, 100, 8)
		H.assert_eq(run, 8)
		H.assert_eq(defer, 92)
		H.assert_true(0 + run <= 8)

		run, defer = admit(0, 0, 8)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 0)

		run, defer = admit(0, 5, 0)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 5)

		-- Negative inputs do not run.
		run, defer = admit(-1, 4, 8)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 4)

		run, defer = admit(0, -3, 8)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 0)

		run, defer = admit(1, 4, -8)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 4)

		run, defer = admit(-2, -2, -2)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 0)

		-- Non-numeric inputs do not run.
		run, defer = admit("2", 4, 8)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 4)

		run, defer = admit(0, "no", 8)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 0)

		run, defer = admit(0, 4, nil)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 4)

		run, defer = admit(nil, nil, nil)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 0)

		run, defer = admit(true, 1, 8)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 1)

		run, defer = admit(0 / 0, 3, 8)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 3)

		run, defer = admit(0, math.huge, 8)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 0)

		run, defer = admit(1.5, 3, 8)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 3)

		run, defer = admit(0, 2.9, 8)
		H.assert_eq(run, 0)
		H.assert_eq(defer, 0)

		-- Default cap is the max argument callers should pass.
		local cap = u.TraceBudget_MaxPerFrame()
		run, defer = admit(0, cap + 1, cap)
		H.assert_eq(run, cap)
		H.assert_eq(defer, 1)
		H.assert_true(run + 0 <= cap)
	end)
end
