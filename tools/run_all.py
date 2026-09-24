#!/usr/bin/env python3
"""One command for the whole QA pipeline (used locally and in CI):

  build (with coverage) -> unit tests -> functional tests -> coverage -> static analysis -> dashboard

    python tools/run_all.py
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-cov"
REPORTS = ROOT / "reports"
EXE = ".exe" if os.name == "nt" else ""

UNITY_LINE = re.compile(r"^(?P<file>.*):(?P<line>\d+):(?P<name>\w+):(?P<status>PASS|FAIL|IGNORE)(?::\s?(?P<msg>.*))?$")
CPPCHECK_IGNORE = {"missingIncludeSystem", "missingInclude", "checkersReport",
                   "normalCheckLevelMaxBranches", "toomanyconfigs"}


def find_toolchain() -> None:
    """Make GCC visible when it was installed by winget but the shell was not restarted."""
    if shutil.which("gcc") or os.name != "nt":
        return
    packages = Path(os.environ.get("LOCALAPPDATA", "")) / "Microsoft" / "WinGet" / "Packages"
    for bin_dir in packages.glob("BrechtSanders.WinLibs*/mingw64/bin"):
        os.environ["PATH"] = f"{bin_dir}{os.pathsep}{os.environ['PATH']}"
        return


def run(cmd: list[str], **kwargs) -> subprocess.CompletedProcess:
    print("  $", " ".join(str(c) for c in cmd))
    return subprocess.run([str(c) for c in cmd], cwd=ROOT, text=True, capture_output=True, **kwargs)


def step(title: str) -> float:
    print(f"\n== {title}")
    return time.perf_counter()


def git_info() -> dict:
    sha = run(["git", "rev-parse", "--short", "HEAD"])
    branch = run(["git", "rev-parse", "--abbrev-ref", "HEAD"])
    return {"sha": sha.stdout.strip() if sha.returncode == 0 else "",
            "branch": branch.stdout.strip() if branch.returncode == 0 else ""}


def gcovr(json_out: Path, html_out: Path | None = None) -> None:
    cmd = [sys.executable, "-m", "gcovr", "--root", ROOT, "--object-directory", BUILD,
           "--filter", "firmware/src/", "--exclude-unreachable-branches",
           "--json-summary-pretty", "--json-summary", json_out]
    if html_out:
        cmd += ["--html-details", html_out, "--html-title", "BMS firmware coverage"]
    r = run(cmd)
    if r.returncode != 0:
        print(r.stdout, r.stderr)


def main() -> int:
    find_toolchain()
    for sub in ("unit", "functional", "coverage", "static"):
        (REPORTS / sub).mkdir(parents=True, exist_ok=True)

    results: dict = {"generated": datetime.now(timezone.utc).isoformat(timespec="seconds"),
                     "git": git_info(), "durations": {}}

    # ------------------------------------------------------------ build
    t = step("Build firmware, simulator and unit tests (coverage instrumented)")
    r = run(["cmake", "-S", ".", "-B", BUILD, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug", "-DBMS_COVERAGE=ON"])
    if r.returncode == 0:
        r = run(["cmake", "--build", BUILD])
    results["build"] = {"ok": r.returncode == 0, "log": (r.stdout + r.stderr)[-4000:]}
    results["durations"]["build"] = time.perf_counter() - t
    if r.returncode != 0:
        print(r.stdout, r.stderr)
        return 1
    compiler = run(["gcc", "--version"]).stdout.splitlines()
    results["compiler"] = compiler[0] if compiler else "gcc"

    for gcda in BUILD.rglob("*.gcda"):
        gcda.unlink()

    # ------------------------------------------------------------ unit tests
    t = step("Unit tests (Unity)")
    unit: list[dict] = []
    for src in sorted((ROOT / "tests" / "unit").glob("test_*.c")):
        suite = src.stem
        r = run([BUILD / f"{suite}{EXE}"])
        (REPORTS / "unit" / f"{suite}.txt").write_text(r.stdout, encoding="utf-8")
        for line in r.stdout.splitlines():
            m = UNITY_LINE.match(line.strip())
            if m:
                unit.append({"suite": suite, "name": m["name"], "status": m["status"],
                             "message": m["msg"] or "", "line": int(m["line"])})
    results["unit"] = unit
    results["durations"]["unit"] = time.perf_counter() - t
    failed = sum(u["status"] == "FAIL" for u in unit)
    print(f"  {len(unit)} unit tests, {failed} failed")
    gcovr(REPORTS / "coverage" / "unit_summary.json")

    # ------------------------------------------------------------ functional tests
    t = step("Functional tests (pytest + simulator)")
    env = dict(os.environ, BMS_SIM=str(BUILD / f"bms_sim{EXE}"))
    r = run([sys.executable, "-m", "pytest", "-q", "--junitxml", REPORTS / "functional" / "junit.xml"], env=env)
    (REPORTS / "functional" / "pytest.txt").write_text(r.stdout + r.stderr, encoding="utf-8")
    print("  " + (r.stdout.strip().splitlines() or ["(no output)"])[-1])
    results["functional_rc"] = r.returncode
    results["durations"]["functional"] = time.perf_counter() - t

    # ------------------------------------------------------------ coverage (unit + functional)
    t = step("Coverage (gcov / gcovr)")
    gcovr(REPORTS / "coverage" / "summary.json", REPORTS / "coverage" / "index.html")
    results["durations"]["coverage"] = time.perf_counter() - t

    # ------------------------------------------------------------ static analysis
    t = step("Static analysis (cppcheck)")
    xml_out = REPORTS / "static" / "cppcheck.xml"
    r = run(["cppcheck", "--enable=warning,style,performance,portability", "--std=c99",
             "--inline-suppr", "--xml", "-q", "-I", "firmware/inc", "firmware/src"])
    xml_out.write_text(r.stderr, encoding="utf-8")
    findings = []
    for m in re.finditer(r'<error id="([^"]+)" severity="([^"]+)" msg="([^"]*)"[^>]*>(.*?)</error>', r.stderr, re.S):
        if m[1] in CPPCHECK_IGNORE:
            continue
        loc = re.search(r'<location file="([^"]+)" line="(\d+)"', m[4])
        findings.append({"id": m[1], "severity": m[2], "msg": m[3],
                         "file": loc[1] if loc else "", "line": int(loc[2]) if loc else 0})
    results["cppcheck"] = findings
    results["durations"]["static"] = time.perf_counter() - t
    print(f"  {len(findings)} findings")

    (REPORTS / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")

    # ------------------------------------------------------------ dashboard
    step("Dashboard")
    sys.path.insert(0, str(ROOT / "tools"))
    import make_dashboard
    out = make_dashboard.build()
    print(f"  written: {out}")

    ok = failed == 0 and results["functional_rc"] == 0 and not findings
    print("\nRESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
