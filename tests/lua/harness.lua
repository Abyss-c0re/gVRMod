-- Offline test harness (mirrors C++ test_framework.h spirit)
local H = {
	passed = 0,
	failed = 0,
	asserts_ok = 0,
	asserts_fail = 0,
	current_failed = false,
	tests = {},
}

function H.TEST(name, fn)
	H.tests[#H.tests + 1] = { name = name, fn = fn }
end

function H.assert_true(expr, msg)
	if expr then
		H.asserts_ok = H.asserts_ok + 1
	else
		H.asserts_fail = H.asserts_fail + 1
		H.current_failed = true
		io.stderr:write(string.format("  FAIL assert_true: %s\n", msg or "?"))
	end
end

function H.assert_false(expr, msg)
	H.assert_true(not expr, msg or "assert_false")
end

function H.assert_eq(a, b, msg)
	if a == b then
		H.asserts_ok = H.asserts_ok + 1
	else
		H.asserts_fail = H.asserts_fail + 1
		H.current_failed = true
		io.stderr:write(string.format("  FAIL assert_eq: %s got %s vs %s\n",
			msg or "", tostring(a), tostring(b)))
	end
end

function H.assert_near(a, b, eps, msg)
	eps = eps or 1e-4
	if type(a) == "number" and type(b) == "number" and math.abs(a - b) <= eps then
		H.asserts_ok = H.asserts_ok + 1
	else
		H.asserts_fail = H.asserts_fail + 1
		H.current_failed = true
		io.stderr:write(string.format("  FAIL assert_near: %s got %s vs %s\n",
			msg or "", tostring(a), tostring(b)))
	end
end

function H.assert_vec_near(a, b, eps, msg)
	eps = eps or 0.01
	if a and b and math.abs(a.x - b.x) <= eps and math.abs(a.y - b.y) <= eps and math.abs(a.z - b.z) <= eps then
		H.asserts_ok = H.asserts_ok + 1
	else
		H.asserts_fail = H.asserts_fail + 1
		H.current_failed = true
		io.stderr:write(string.format("  FAIL assert_vec_near: %s\n", msg or ""))
	end
end

function H.run_all()
	print(string.format("Running %d Lua tests...\n", #H.tests))
	for _, t in ipairs(H.tests) do
		H.current_failed = false
		print("[RUN ] " .. t.name)
		local ok, err = pcall(t.fn)
		if not ok then
			H.current_failed = true
			io.stderr:write("  ERROR: " .. tostring(err) .. "\n")
		end
		if H.current_failed then
			print("[FAIL] " .. t.name)
			H.failed = H.failed + 1
		else
			print("[ OK ] " .. t.name)
			H.passed = H.passed + 1
		end
	end
	print("\n========================================")
	print(string.format("Lua: %d passed, %d failed (%d asserts ok, %d failed)",
		H.passed, H.failed, H.asserts_ok, H.asserts_fail))
	print("========================================")
	return H.failed > 0 and 1 or 0
end

return H
