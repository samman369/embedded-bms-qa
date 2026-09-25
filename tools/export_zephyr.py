"""Converts docs/manual_test_cases.csv into a Zephyr Scale import file (one row per test step).

    python tools/export_zephyr.py   ->  docs/zephyr_test_cases.csv

Rows that continue a test case leave the test-case columns empty, which is how the
Zephyr Scale CSV importer groups several steps into one test case.
"""
from __future__ import annotations

import csv
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs" / "manual_test_cases.csv"
TARGET = ROOT / "docs" / "zephyr_test_cases.csv"
PLAIN_TARGET = ROOT / "docs" / "zephyr_test_cases_plaintext.csv"
SAFE_TARGET = ROOT / "docs" / "zephyr_test_cases_semicolon.csv"

PRIORITY = {"high": "High", "medium": "Normal", "low": "Low"}
COLUMNS = ["Name", "Objective", "Precondition", "Priority", "Status", "Folder", "Labels",
           "Step", "Test Data", "Expected Result"]

STEP_LINE = re.compile(r"^\s*(\d+)\.\s*(.+)$")
EXPECTED_LINE = re.compile(r"^\s*(?:Step\s*)?(\d+)\s*:\s*(.+)$", re.I)


def split_steps(text: str) -> list[str]:
    steps = [m[2].strip() for line in text.splitlines() if (m := STEP_LINE.match(line))]
    return steps or [text.strip()]


def split_expected(text: str, n_steps: int) -> list[str]:
    """Maps 'Step 3: ...' / '3: ...' lines to their step; anything else goes to the last step."""
    expected = [""] * n_steps
    for line in (l.strip() for l in text.splitlines() if l.strip()):
        m = EXPECTED_LINE.match(line)
        index = int(m[1]) - 1 if m and 0 < int(m[1]) <= n_steps else n_steps - 1
        value = m[2] if m and 0 < int(m[1]) <= n_steps else line
        expected[index] = f"{expected[index]}\n{value}".strip()
    return expected


def main() -> None:
    with open(SOURCE, newline="", encoding="utf-8") as f:
        cases = list(csv.DictReader(f))

    rows = []
    for case in cases:
        steps = split_steps(case["Steps"])
        expected = split_expected(case["Expected_Result"], len(steps))
        reqs = re.findall(r"REQ-[A-Z]+-\d{3}", case["Requirement"])
        folder = "/Exploratory" if case["TC_ID"].startswith("EXP") else (
            "/Communication" if any(r.startswith("REQ-COM") for r in reqs) and not any(
                r.startswith("REQ-BMS") for r in reqs) else "/Protection")
        for i, step in enumerate(steps):
            first = i == 0
            rows.append({
                "Name": f"{case['TC_ID']} {case['Title']}" if first else "",
                "Objective": (f"Verify {case['Requirement']} using {case['Technique'].lower()}."
                              if first else ""),
                "Precondition": case["Preconditions"] if first else "",
                "Priority": PRIORITY.get(case["Priority"].strip().lower(), "Normal") if first else "",
                "Status": "Approved" if first else "",
                "Folder": folder if first else "",
                "Labels": " ".join(reqs + [case["Technique"].split(" (")[0].lower().replace(" ", "-")]) if first else "",
                "Step": step,
                "Test Data": "",
                "Expected Result": expected[i] or ("Bench replies @OK" if step.startswith("@") else ""),
            })

    with open(TARGET, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=COLUMNS)
        writer.writeheader()
        writer.writerows(rows)
    print(f"{len(cases)} test cases, {len(rows)} step rows -> {TARGET}")

    # Fallback: one row per test case with a plain-text script (Zephyr "Test Script (Plain Text)").
    plain = []
    for row in rows:
        line = f"{row['Step']}  ->  expected: {row['Expected Result']}".replace("\n", "; ")
        if row["Name"]:
            plain.append({k: row[k] for k in COLUMNS[:7]} | {"Test Script Plain Text": ""})
            n = 0
        n += 1
        plain[-1]["Test Script Plain Text"] += f"{n}. {line}\n"
    with open(PLAIN_TARGET, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=COLUMNS[:7] + ["Test Script Plain Text"])
        writer.writeheader()
        writer.writerows(plain)
    print(f"{len(plain)} test cases (plain-text script) -> {PLAIN_TARGET}")

    # Import-safe variant: ';' delimiter, no line breaks or ';' inside values, so a
    # simple CSV reader cannot split protocol strings such as "ERR,FORMAT".
    def safe(value: str) -> str:
        return " | ".join(part.strip() for part in value.splitlines() if part.strip()).replace(";", ",")

    with open(SAFE_TARGET, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=COLUMNS, delimiter=";", quoting=csv.QUOTE_MINIMAL)
        writer.writeheader()
        writer.writerows({k: safe(v) for k, v in row.items()} for row in rows)
    print(f"{len(rows)} step rows (';' delimited, import-safe) -> {SAFE_TARGET}")


if __name__ == "__main__":
    main()
