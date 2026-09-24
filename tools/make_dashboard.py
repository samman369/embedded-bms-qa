"""Builds reports/dashboard.html from the pipeline results.

Inputs: reports/results.json (run_all.py), reports/functional/junit.xml, reports/coverage/*.json,
docs/requirements.md, docs/manual_test_cases.csv, docs/bug-reports/BMS-*.md
"""
from __future__ import annotations

import csv
import json
import re
import xml.etree.ElementTree as ET
from html import escape
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPORTS = ROOT / "reports"
DOCS = ROOT / "docs"
LINE_COVERAGE_GATE = 90.0

REQ_IN_NAME = re.compile(r"REQ_([A-Z]+)_(\d{3})")
REQ_ID = re.compile(r"REQ-[A-Z]+-\d{3}")


# ====================================================================== loaders
def load_requirements() -> list[dict]:
    reqs = []
    for line in (DOCS / "requirements.md").read_text(encoding="utf-8").splitlines():
        m = re.match(r"\|\s*(REQ-[A-Z]+-\d{3})\s*\|\s*(.+?)\s*\|\s*$", line)
        if m:
            reqs.append({"id": m[1], "text": re.sub(r"`", "", m[2])})
    return reqs


def load_functional() -> list[dict]:
    path = REPORTS / "functional" / "junit.xml"
    if not path.exists():
        return []
    tests = []
    for case in ET.parse(path).getroot().iter("testcase"):
        props = {p.get("name"): p.get("value") for p in case.iter("property")}
        failure = case.find("failure")
        if failure is None:
            failure = case.find("error")
        status = "SKIP" if case.find("skipped") is not None else ("FAIL" if failure is not None else "PASS")
        tests.append({
            "suite": case.get("classname", "").split(".")[-1],
            "name": case.get("name", ""),
            "status": status,
            "time": float(case.get("time", 0) or 0),
            "message": (failure.get("message", "") if failure is not None else "")[:300],
            "reqs": [r for r in (props.get("requirements") or "").split(",") if r],
        })
    return tests


def load_manual() -> list[dict]:
    with open(DOCS / "manual_test_cases.csv", newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    for r in rows:
        r["reqs"] = REQ_ID.findall(r.get("Requirement", ""))
    return rows


def load_bugs() -> list[dict]:
    bugs = []
    for path in sorted((DOCS / "bug-reports").glob("BMS-*.md"), key=lambda p: int(p.stem.split("-")[1])):
        text = path.read_text(encoding="utf-8")
        title = re.search(r"^#\s*(BMS-\d+)\s*[—-]\s*(.+)$", text, re.M)

        def field(name: str) -> str:
            m = re.search(rf"\|\s*\*\*{name}\*\*\s*\|\s*(.+?)\s*\|", text)
            return m[1] if m else ""
        bugs.append({"key": title[1] if title else path.stem, "summary": title[2] if title else "",
                     "status": field("Status"), "severity": field("Severity").split(" ")[0],
                     "priority": field("Priority").split(" ")[0], "component": field("Component"),
                     "found_by": field("Found by"), "file": path.name})
    return bugs


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}


# ====================================================================== html helpers
ICON = {"PASS": "✓", "FAIL": "✕", "SKIP": "–", "IGNORE": "–", "WARN": "!", "NOTRUN": "○"}
TONE = {"PASS": "good", "FAIL": "critical", "SKIP": "muted", "IGNORE": "muted", "WARN": "warning", "NOTRUN": "muted"}


def chip(kind: str, label: str | None = None) -> str:
    return (f'<span class="chip {TONE.get(kind, "muted")}"><span class="dot" aria-hidden="true">'
            f'{ICON.get(kind, "•")}</span>{escape(label or kind.title())}</span>')


def pct(n: float, d: float) -> float:
    return 100.0 * n / d if d else 0.0


def tile(label: str, value: str, sub: str, kind: str) -> str:
    return (f'<div class="tile"><div class="tile-label">{escape(label)}</div>'
            f'<div class="tile-value">{value}</div>'
            f'<div class="tile-sub">{chip(kind, sub)}</div></div>')


def pretty_test_name(name: str) -> str:
    base = re.sub(r"__REQ_[A-Z]+_\d{3}$", "", name)
    base = re.sub(r"^test_", "", base)
    return base.replace("_", " ")


# ====================================================================== build
def build() -> Path:
    results = load_json(REPORTS / "results.json")
    reqs = load_requirements()
    unit = results.get("unit", [])
    for u in unit:
        u["reqs"] = [f"REQ-{a}-{b}" for a, b in REQ_IN_NAME.findall(u["name"])]
    functional = load_functional()
    manual = load_manual()
    bugs = load_bugs()
    cov = load_json(REPORTS / "coverage" / "summary.json")
    cov_unit = load_json(REPORTS / "coverage" / "unit_summary.json")
    static = results.get("cppcheck", [])

    unit_pass = sum(u["status"] == "PASS" for u in unit)
    unit_fail = sum(u["status"] == "FAIL" for u in unit)
    fn_pass = sum(t["status"] == "PASS" for t in functional)
    fn_fail = sum(t["status"] == "FAIL" for t in functional)
    manual_scripted = [m for m in manual if m["TC_ID"].startswith("MTC")]
    manual_done = [m for m in manual if m.get("Status", "").strip().lower() in ("pass", "fail", "blocked")]
    manual_fail = [m for m in manual if m.get("Status", "").strip().lower() == "fail"]
    open_bugs = [b for b in bugs if not b["status"].lower().startswith(("closed", "resolved", "done"))]
    open_blocking = [b for b in open_bugs if b["severity"].lower() in ("critical", "major")]
    line_pct = float(cov.get("line_percent", 0) or 0)
    branch_pct = float(cov.get("branch_percent", 0) or 0)
    func_pct = float(cov.get("function_percent", 0) or 0)
    unit_line_pct = float(cov_unit.get("line_percent", 0) or 0)

    # ---------------------------------------------------------------- traceability
    trace = []
    for r in reqs:
        u = [t for t in unit if r["id"] in t["reqs"]]
        f = [t for t in functional if r["id"] in t["reqs"]]
        m = [t for t in manual if r["id"] in t["reqs"]]
        automated = u + f
        if any(t["status"] == "FAIL" for t in automated):
            status = ("FAIL", "Failing")
        elif automated:
            status = ("PASS", "Verified")
        elif m:
            status = ("WARN", "Manual only")
        else:
            status = ("FAIL", "Not covered")
        trace.append({**r, "unit": u, "func": f, "manual": m, "status": status})
    req_verified = sum(t["status"][0] == "PASS" for t in trace)

    # ---------------------------------------------------------------- quality gates (test plan §5)
    gates = [
        ("All automated tests pass", unit_fail == 0 and fn_fail == 0 and bool(unit) and bool(functional),
         f"{unit_pass + fn_pass} / {len(unit) + len(functional)} passed"),
        ("Every requirement covered", req_verified == len(reqs) and bool(reqs), f"{req_verified} / {len(reqs)} verified"),
        (f"Line coverage ≥ {LINE_COVERAGE_GATE:.0f} %", line_pct >= LINE_COVERAGE_GATE, f"{line_pct:.1f} %"),
        ("Static analysis clean", not static, f"{len(static)} cppcheck findings"),
        ("No open Critical / Major defects", not open_blocking, f"{len(open_blocking)} open"),
        ("All manual test cases executed", len(manual_done) == len(manual), f"{len(manual_done)} / {len(manual)} executed"),
    ]
    automated_ok = all(ok for _, ok, _ in gates[:5])
    all_ok = automated_ok and gates[5][1]
    if all_ok:
        verdict = chip("PASS", "Release criteria met")
    elif automated_ok:
        verdict = chip("WARN", "Automated gates passed · manual execution pending")
    else:
        verdict = chip("FAIL", "Release criteria not met")

    # ---------------------------------------------------------------- sections
    tiles = "".join([
        tile("Unit tests", f"{unit_pass}<small>/{len(unit)}</small>",
             "all passing" if unit_fail == 0 else f"{unit_fail} failing", "PASS" if unit_fail == 0 else "FAIL"),
        tile("Functional tests", f"{fn_pass}<small>/{len(functional)}</small>",
             "all passing" if fn_fail == 0 else f"{fn_fail} failing", "PASS" if fn_fail == 0 else "FAIL"),
        tile("Line coverage", f"{line_pct:.1f}<small>%</small>", f"branch {branch_pct:.1f} %",
             "PASS" if line_pct >= LINE_COVERAGE_GATE else "FAIL"),
        tile("Requirements", f"{req_verified}<small>/{len(reqs)}</small>", "traced & verified",
             "PASS" if req_verified == len(reqs) else "FAIL"),
        tile("Manual tests", f"{len(manual_done)}<small>/{len(manual)}</small>",
             "executed" if manual_done else "not yet executed",
             "FAIL" if manual_fail else ("PASS" if len(manual_done) == len(manual) else "NOTRUN")),
        tile("Defects (Jira)", f"{len(open_bugs)}<small> open</small>", f"{len(bugs)} reported",
             "PASS" if not open_blocking else "FAIL"),
    ])

    gate_rows = "".join(
        f"<tr><td>{escape(name)}</td><td class='num'>{escape(detail)}</td>"
        f"<td>{chip('PASS', 'Met') if ok else (chip('NOTRUN', 'Pending') if i == 5 else chip('FAIL', 'Not met'))}</td></tr>"
        for i, (name, ok, detail) in enumerate(gates))

    def count_cell(tests: list, manual_col: bool = False) -> str:
        if not tests:
            return '<td class="num muted">—</td>'
        if manual_col:
            ids = ", ".join(t["TC_ID"] for t in tests)
            return f'<td class="num" title="{escape(ids)}">{len(tests)}</td>'
        bad = sum(t["status"] == "FAIL" for t in tests)
        names = "\n".join(pretty_test_name(t["name"]) for t in tests)
        return f'<td class="num" title="{escape(names)}">{len(tests)}{" <b class=\"bad\">(" + str(bad) + " ✕)</b>" if bad else ""}</td>'

    trace_rows = "".join(
        f"<tr><td class='mono'>{t['id']}</td><td class='req-text'>{escape(t['text'])}</td>"
        f"{count_cell(t['unit'])}{count_cell(t['func'])}{count_cell(t['manual'], True)}"
        f"<td>{chip(*t['status'])}</td></tr>" for t in trace)

    def cov_bar(value: float) -> str:
        return (f'<div class="bar" role="img" aria-label="{value:.1f} percent">'
                f'<div class="bar-fill" style="width:{value:.1f}%"></div></div>')

    cov_rows = ""
    for f in sorted(cov.get("files", []), key=lambda x: x["filename"]):
        lp = float(f.get("line_percent", 0) or 0)
        bp = float(f.get("branch_percent", 0) or 0) if f.get("branch_total") else 100.0
        cov_rows += (f"<tr title='{f['line_covered']}/{f['line_total']} lines · {f.get('branch_covered', 0)}/{f.get('branch_total', 0)} branches'>"
                     f"<td class='mono'>{escape(Path(f['filename']).name)}</td>"
                     f"<td class='bar-cell'>{cov_bar(lp)}</td><td class='num'>{lp:.1f} %</td>"
                     f"<td class='num muted'>{bp:.1f} %</td></tr>")

    def suite_block(title: str, tests: list[dict], kind: str) -> str:
        suites: dict[str, list] = {}
        for t in tests:
            suites.setdefault(t["suite"], []).append(t)
        out = ""
        for name, items in suites.items():
            bad = sum(t["status"] == "FAIL" for t in items)
            rows = "".join(
                f"<tr><td>{chip(t['status'], t['status'].title())}</td>"
                f"<td>{escape(pretty_test_name(t['name']) if kind == 'unit' else t['name'])}"
                f"{'<div class=\"fail-msg\">' + escape(t['message']) + '</div>' if t['status'] == 'FAIL' and t['message'] else ''}</td>"
                f"<td class='mono small'>{' '.join(t['reqs'])}</td></tr>" for t in items)
            out += (f"<details{' open' if bad else ''}><summary><span class='mono'>{escape(name)}</span>"
                    f"<span class='summary-right'>{len(items) - bad}/{len(items)} "
                    f"{chip('PASS' if not bad else 'FAIL', 'pass' if not bad else f'{bad} fail')}</span></summary>"
                    f"<div class='table-wrap'><table><thead><tr><th>Result</th><th>Test</th><th>Requirement</th></tr></thead>"
                    f"<tbody>{rows}</tbody></table></div></details>")
        return f"<h3>{escape(title)}</h3>{out}"

    def manual_status(s: str) -> str:
        s = (s or "").strip().lower()
        return {"pass": chip("PASS"), "fail": chip("FAIL"), "blocked": chip("WARN", "Blocked")}.get(s, chip("NOTRUN", "Not run"))

    manual_rows = "".join(
        f"<tr><td class='mono'>{escape(m['TC_ID'])}</td><td>{escape(m['Title'])}</td>"
        f"<td class='small'>{escape(m['Technique'])}</td><td class='small'>{escape(m['Priority'])}</td>"
        f"<td>{manual_status(m.get('Status', ''))}</td><td class='mono small'>{escape(m.get('Jira_Bug', '') or '')}</td></tr>"
        for m in manual)

    bug_rows = "".join(
        f"<tr><td class='mono'><a href='../docs/bug-reports/{escape(b['file'])}'>{escape(b['key'])}</a></td>"
        f"<td>{escape(b['summary'])}</td><td>{escape(b['severity'])}</td><td>{escape(b['priority'])}</td>"
        f"<td class='small'>{escape(b['found_by'])}</td>"
        f"<td>{chip('PASS', b['status']) if b not in open_bugs else chip('FAIL', b['status'])}</td></tr>" for b in bugs)

    static_block = ("<p class='muted'>No findings — cppcheck (warning, style, performance, portability) is clean.</p>"
                    if not static else
                    "<div class='table-wrap'><table><thead><tr><th>Severity</th><th>Check</th><th>Location</th><th>Message</th></tr></thead><tbody>"
                    + "".join(f"<tr><td>{escape(s['severity'])}</td><td class='mono'>{escape(s['id'])}</td>"
                              f"<td class='mono small'>{escape(Path(s['file']).name)}:{s['line']}</td><td>{escape(s['msg'])}</td></tr>" for s in static)
                    + "</tbody></table></div>")

    git = results.get("git", {})
    meta = " · ".join(x for x in [
        f"generated {escape(results.get('generated', '')).replace('T', ' ').replace('+00:00', ' UTC')}",
        f"commit <span class='mono'>{escape(git.get('sha', ''))}</span>" if git.get("sha") else "",
        escape(results.get("compiler", "")),
    ] if x)

    page = TEMPLATE.format(
        meta=meta, verdict=verdict, tiles=tiles, gate_rows=gate_rows, trace_rows=trace_rows,
        cov_rows=cov_rows or "<tr><td colspan='4' class='muted'>No coverage data</td></tr>",
        line_pct=f"{line_pct:.1f}", branch_pct=f"{branch_pct:.1f}", func_pct=f"{func_pct:.1f}",
        unit_line_pct=f"{unit_line_pct:.1f}",
        unit_block=suite_block("Unit tests — Unity (C, host build with fake HAL)", unit, "unit"),
        func_block=suite_block("Functional tests — pytest against the simulated DUT", functional, "func"),
        manual_rows=manual_rows, manual_done=len(manual_done), manual_total=len(manual),
        manual_scripted=len(manual_scripted), bug_rows=bug_rows, static_block=static_block,
    )
    out = REPORTS / "dashboard.html"
    out.write_text(page, encoding="utf-8")
    return out


TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>BMS Test Dashboard</title>
<style>
:root {{
  --page: #f9f9f7; --surface: #fcfcfb; --ink: #0b0b0b; --ink-2: #52514e; --muted: #898781;
  --grid: #e1e0d9; --border: rgba(11,11,11,0.10); --accent: #2a78d6;
  --good: #0ca30c; --warning: #fab219; --critical: #d03b3b; --good-text: #006300;
  --chip-good: rgba(12,163,12,0.12); --chip-critical: rgba(208,59,59,0.12);
  --chip-warning: rgba(250,178,25,0.18); --chip-muted: rgba(137,135,129,0.14);
}}
@media (prefers-color-scheme: dark) {{
  :root:not([data-theme="light"]) {{
    --page: #0d0d0d; --surface: #1a1a19; --ink: #ffffff; --ink-2: #c3c2b7; --muted: #898781;
    --grid: #2c2c2a; --border: rgba(255,255,255,0.10); --accent: #3987e5; --good-text: #0ca30c;
    --chip-good: rgba(12,163,12,0.20); --chip-critical: rgba(208,59,59,0.22);
    --chip-warning: rgba(250,178,25,0.18); --chip-muted: rgba(137,135,129,0.20);
  }}
}}
:root[data-theme="dark"] {{
  --page: #0d0d0d; --surface: #1a1a19; --ink: #ffffff; --ink-2: #c3c2b7; --muted: #898781;
  --grid: #2c2c2a; --border: rgba(255,255,255,0.10); --accent: #3987e5; --good-text: #0ca30c;
  --chip-good: rgba(12,163,12,0.20); --chip-critical: rgba(208,59,59,0.22);
  --chip-warning: rgba(250,178,25,0.18); --chip-muted: rgba(137,135,129,0.20);
}}
* {{ box-sizing: border-box; }}
body {{ margin: 0; background: var(--page); color: var(--ink);
  font: 14px/1.5 system-ui, -apple-system, "Segoe UI", Roboto, sans-serif; }}
.wrap {{ max-width: 1180px; margin: 0 auto; padding: 32px 16px 64px; }}
header {{ display: flex; flex-wrap: wrap; gap: 12px 24px; align-items: flex-end; justify-content: space-between; margin-bottom: 24px; }}
.eyebrow {{ color: var(--muted); font-size: 12px; letter-spacing: .08em; text-transform: uppercase; margin: 0 0 4px; }}
h1 {{ font-size: 28px; line-height: 1.2; margin: 0; letter-spacing: -.01em; }}
h2 {{ font-size: 17px; margin: 0 0 4px; }}
h3 {{ font-size: 14px; margin: 20px 0 8px; color: var(--ink-2); font-weight: 600; }}
.meta {{ color: var(--muted); font-size: 12px; margin-top: 6px; }}
.section {{ background: var(--surface); border: 1px solid var(--border); border-radius: 12px; padding: 20px; margin-top: 16px; }}
.section > p.lead {{ color: var(--ink-2); margin: 0 0 12px; }}
.tiles {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(165px, 1fr)); gap: 12px; }}
.tile {{ background: var(--surface); border: 1px solid var(--border); border-radius: 12px; padding: 16px; }}
.tile-label {{ color: var(--ink-2); font-size: 12px; }}
.tile-value {{ font-size: 34px; font-weight: 650; letter-spacing: -.02em; line-height: 1.15; margin: 6px 0 8px; font-variant-numeric: tabular-nums; }}
.tile-value small {{ font-size: 16px; color: var(--muted); font-weight: 500; margin-left: 2px; }}
.chip {{ display: inline-flex; align-items: center; gap: 6px; padding: 2px 9px 2px 6px; border-radius: 999px; font-size: 12px; color: var(--ink); white-space: nowrap; background: var(--chip-muted); }}
.chip .dot {{ display: inline-grid; place-items: center; width: 16px; height: 16px; border-radius: 50%; font-size: 10px; font-weight: 700; color: #fff; background: var(--muted); }}
.chip.good {{ background: var(--chip-good); }}         .chip.good .dot {{ background: var(--good); }}
.chip.critical {{ background: var(--chip-critical); }} .chip.critical .dot {{ background: var(--critical); }}
.chip.warning {{ background: var(--chip-warning); }}   .chip.warning .dot {{ background: var(--warning); color: #0b0b0b; }}
.table-wrap {{ overflow-x: auto; }}
table {{ width: 100%; border-collapse: collapse; }}
th {{ text-align: left; font-size: 12px; font-weight: 600; color: var(--muted); padding: 8px 10px; border-bottom: 1px solid var(--grid); white-space: nowrap; }}
td {{ padding: 8px 10px; border-bottom: 1px solid var(--grid); vertical-align: top; }}
tbody tr:last-child td {{ border-bottom: 0; }}
tbody tr:hover td {{ background: var(--chip-muted); }}
.num {{ text-align: right; font-variant-numeric: tabular-nums; white-space: nowrap; }}
.mono {{ font-family: ui-monospace, "Cascadia Code", Consolas, monospace; font-size: 12.5px; }}
td.mono {{ white-space: nowrap; }}
.small {{ font-size: 12px; }} .muted {{ color: var(--muted); }}
.bad {{ color: var(--critical); font-weight: 600; }}
.req-text {{ color: var(--ink-2); min-width: 280px; }}
.bar-cell {{ width: 55%; vertical-align: middle; }}
.bar {{ height: 10px; background: var(--grid); border-radius: 4px; overflow: hidden; }}
.bar-fill {{ height: 100%; background: var(--accent); border-radius: 0 4px 4px 0; }}
.cov-summary {{ display: flex; flex-wrap: wrap; gap: 8px 28px; margin-bottom: 12px; color: var(--ink-2); }}
.cov-summary b {{ color: var(--ink); font-size: 18px; font-variant-numeric: tabular-nums; }}
details {{ border: 1px solid var(--grid); border-radius: 10px; margin-bottom: 8px; background: var(--surface); }}
summary {{ cursor: pointer; padding: 10px 14px; display: flex; justify-content: space-between; align-items: center; gap: 12px; list-style: none; }}
summary::-webkit-details-marker {{ display: none; }}
summary::before {{ content: "▸"; color: var(--muted); margin-right: 8px; transition: transform .15s; }}
details[open] summary::before {{ transform: rotate(90deg); }}
summary .mono {{ flex: 1; }}
.summary-right {{ display: flex; align-items: center; gap: 10px; color: var(--ink-2); font-variant-numeric: tabular-nums; }}
details table {{ border-top: 1px solid var(--grid); }}
.fail-msg {{ font-family: ui-monospace, Consolas, monospace; font-size: 12px; color: var(--critical); margin-top: 4px; }}
.two-col {{ display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1fr); gap: 16px; }}
.two-col .section {{ margin-top: 0; min-width: 0; }}
.pyramid {{ display: grid; gap: 6px; margin-top: 8px; }}
.layer {{ margin: 0 auto; padding: 8px 12px; border-radius: 6px; text-align: center; font-size: 13px; background: var(--chip-muted); border: 1px solid var(--border); }}
.layer b {{ font-variant-numeric: tabular-nums; }}
a {{ color: var(--accent); }}
.theme-toggle {{ background: var(--surface); color: var(--ink-2); border: 1px solid var(--border); border-radius: 8px; padding: 6px 10px; font: inherit; font-size: 12px; cursor: pointer; }}
@media (max-width: 900px) {{ .two-col {{ grid-template-columns: minmax(0, 1fr); }} .bar-cell {{ width: 40%; }} h1 {{ font-size: 22px; }} }}
</style>
</head>
<body>
<div class="wrap">
<header>
  <div>
    <p class="eyebrow">Embedded QA · 4S Battery Management System · firmware v1.0.0</p>
    <h1>BMS Test Dashboard</h1>
    <div class="meta">{meta}</div>
  </div>
  <div style="display:flex;gap:10px;align-items:center;flex-wrap:wrap">{verdict}
    <button class="theme-toggle" type="button" onclick="toggleTheme()">Light / dark</button></div>
</header>

<div class="tiles">{tiles}</div>

<div class="two-col" style="margin-top:16px">
  <div class="section">
    <h2>Release quality gates</h2>
    <p class="lead">Exit criteria from the test plan (TP-BMS-001 §5).</p>
    <div class="table-wrap"><table><tbody>{gate_rows}</tbody></table></div>
  </div>
  <div class="section">
    <h2>Test strategy</h2>
    <p class="lead">Each level catches what the one below cannot.</p>
    <div class="pyramid">
      <div class="layer" style="width:46%">Manual &amp; exploratory · <b>{manual_total}</b></div>
      <div class="layer" style="width:64%">Functional (black-box, simulated HIL) · pytest</div>
      <div class="layer" style="width:82%">Integration · ISR → parser → command → UART</div>
      <div class="layer" style="width:100%">Unit · Unity + fake HAL · static analysis</div>
    </div>
    <p class="small muted" style="margin:12px 0 0">Techniques: boundary values, equivalence classes, state transitions,
    decision tables, timing/debounce, error guessing, stress, defect seeding.</p>
  </div>
</div>

<div class="section">
  <h2>Requirements traceability matrix</h2>
  <p class="lead">Every requirement mapped to the tests that verify it. Hover a count to see the tests.</p>
  <div class="table-wrap"><table>
    <thead><tr><th>ID</th><th>Requirement</th><th class="num">Unit</th><th class="num">Functional</th><th class="num">Manual</th><th>Status</th></tr></thead>
    <tbody>{trace_rows}</tbody>
  </table></div>
</div>

<div class="section">
  <h2>Code coverage — firmware/src</h2>
  <div class="cov-summary">
    <span>Lines <b>{line_pct} %</b></span><span>Branches <b>{branch_pct} %</b></span>
    <span>Functions <b>{func_pct} %</b></span><span>Unit tests alone <b>{unit_line_pct} %</b> lines</span>
  </div>
  <div class="table-wrap"><table>
    <thead><tr><th>File</th><th>Line coverage</th><th class="num">Lines</th><th class="num">Branches</th></tr></thead>
    <tbody>{cov_rows}</tbody>
  </table></div>
  <p class="small muted" style="margin:10px 0 0">Unit + functional tests combined (gcov / gcovr). Line-by-line report: <a href="coverage/index.html">coverage/index.html</a></p>
</div>

<div class="section">
  <h2>Automated test results</h2>
  {unit_block}
  {func_block}
</div>

<div class="section">
  <h2>Manual test execution</h2>
  <p class="lead">{manual_scripted} scripted cases + exploratory charters, executed with <span class="mono">tools/bms_console.py</span>.
  Executed: <b>{manual_done} / {manual_total}</b>. Source: <span class="mono">docs/manual_test_cases.csv</span></p>
  <div class="table-wrap"><table>
    <thead><tr><th>ID</th><th>Title</th><th>Technique</th><th>Priority</th><th>Status</th><th>Jira</th></tr></thead>
    <tbody>{manual_rows}</tbody>
  </table></div>
</div>

<div class="two-col" style="margin-top:16px">
  <div class="section">
    <h2>Defects — Jira project BMS</h2>
    <div class="table-wrap"><table>
      <thead><tr><th>Key</th><th>Summary</th><th>Severity</th><th>Priority</th><th>Found by</th><th>Status</th></tr></thead>
      <tbody>{bug_rows}</tbody>
    </table></div>
  </div>
  <div class="section">
    <h2>Static analysis</h2>
    <p class="lead">GCC <span class="mono">-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror</span> + cppcheck.</p>
    {static_block}
  </div>
</div>
</div>
<script>
function toggleTheme() {{
  var root = document.documentElement;
  var dark = root.dataset.theme ? root.dataset.theme === "dark" : matchMedia("(prefers-color-scheme: dark)").matches;
  root.dataset.theme = dark ? "light" : "dark";
  try {{ localStorage.setItem("bms-theme", root.dataset.theme); }} catch (e) {{}}
}}
try {{ var t = localStorage.getItem("bms-theme"); if (t) document.documentElement.dataset.theme = t; }} catch (e) {{}}
</script>
</body>
</html>
"""

if __name__ == "__main__":
    print(build())
