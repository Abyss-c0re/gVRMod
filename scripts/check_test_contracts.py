#!/usr/bin/env python3
"""Fail if inventory symbols are missing from contracts, or pure tested symbols lack tests."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LUA = ROOT / "addon" / "vrmod-x64" / "lua"
CONTRACTS = ROOT / "tests" / "contracts"


def scan_lua() -> set[str]:
    """Public API: vrmod.Name or vrmod.utils.Name (not vrmod.package.nested)."""
    found: set[str] = set()
    for p in LUA.rglob("*.lua"):
        text = p.read_text(errors="ignore")
        for m in re.finditer(r"function\s+(vrmod\.utils\.\w+)\s*\(", text):
            found.add(m.group(1))
        for m in re.finditer(r"function\s+(vrmod\.\w+)\s*\(", text):
            name = m.group(1)
            # skip vrmod.package.method — only two segments
            if name.count(".") == 1:
                found.add(name)
    return found


def parse_ids(yaml_path: Path) -> set[str]:
    ids: set[str] = set()
    if not yaml_path.exists():
        return ids
    for line in yaml_path.read_text().splitlines():
        m = re.match(r"\s*-\s*id:\s*(\S+)", line)
        if m:
            ids.add(m.group(1))
    return ids


def main() -> int:
    # Ensure contracts exist
    gen = ROOT / "scripts" / "gen_contracts.py"
    if not (CONTRACTS / "utils.yaml").exists():
        print("contracts missing — run scripts/gen_contracts.py first")
        return 1

    live = scan_lua()
    contracted = parse_ids(CONTRACTS / "utils.yaml") | parse_ids(CONTRACTS / "api.yaml")
    missing = sorted(live - contracted)
    extra = sorted(contracted - live)

    rc = 0
    if missing:
        print(f"FAIL: {len(missing)} symbols in code not in contracts (sample):")
        for s in missing[:25]:
            print(f"  - {s}")
        if len(missing) > 25:
            print(f"  ... +{len(missing) - 25} more")
        print("Hint: python3 scripts/gen_contracts.py")
        rc = 1
    else:
        print(f"OK: all {len(live)} Lua vrmod/utils symbols are contracted")

    if extra and len(extra) > 50:
        # regen often leaves stale; warn only if huge drift
        print(f"WARN: {len(extra)} contracted symbols not found in code (stale catalog?)")

    # Pure-tested symbols must not be 'pending' only
    pure_pending = []
    for yml in (CONTRACTS / "utils.yaml", CONTRACTS / "api.yaml"):
        text = yml.read_text()
        blocks = re.split(r"\n  - id: ", text)
        for b in blocks[1:]:
            lines = b.splitlines()
            sid = lines[0].strip()
            tier = ""
            tests = ""
            for ln in lines[1:]:
                if ln.strip().startswith("tier:"):
                    tier = ln.split(":", 1)[1].strip()
                if ln.strip().startswith("tests:"):
                    tests = ln.split(":", 1)[1].strip()
            if tier == "pure" and "pending" in tests and "[" in tests:
                # only flag if ONLY pending
                if tests.replace(" ", "") in ("[pending]", "[]"):
                    pure_pending.append(sid)

    # G21: pure-pending inventory must stay empty (or only intentional notes).
    # gen_contracts.py maps PURE_TESTED; new pure helpers need unit tests + map entry.
    if pure_pending:
        print(f"FAIL: {len(pure_pending)} pure symbols still pending tests:")
        for s in pure_pending[:30]:
            print(f"  - {s}")
        if len(pure_pending) > 30:
            print(f"  ... +{len(pure_pending) - 30} more")
        print("Hint: add unit test + entry in scripts/gen_contracts.py PURE_TESTED, then gen_contracts.py")
        rc = 1
    else:
        print("OK: pure symbols pending tests: 0")

    # Required offline suites present
    required_tests = [
        ROOT / "tests" / "lua" / "run.lua",
        ROOT / "tests" / "lua" / "unit" / "math_test.lua",
        ROOT / "tests" / "lua" / "unit" / "menu_test.lua",
        ROOT / "tests" / "lua" / "unit" / "experience_test.lua",
        ROOT / "tests" / "test_framework.h",
    ]
    for p in required_tests:
        if not p.exists():
            print(f"FAIL: missing required test file {p.relative_to(ROOT)}")
            rc = 1

    return rc


if __name__ == "__main__":
    sys.exit(main())
