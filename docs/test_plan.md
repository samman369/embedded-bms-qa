# Test Plan — BMS Firmware v1.0

| Item | Value |
|------|-------|
| Document ID | TP-BMS-001 |
| Product | 4S Battery Management System firmware, v1.0.0 |
| Requirements baseline | [requirements.md](requirements.md) (23 requirements) |
| Defect tracking | Jira project **BMS** (see [bug-reports/](bug-reports/)) |

## 1. Objective
Verify that the BMS firmware meets every requirement in the SRS, with particular focus on
the safety functions (over/under-voltage, over-temperature, over-current, sensor faults),
and provide evidence through a requirements traceability matrix.

## 2. Scope
**In scope:** protection logic and state machine, UART protocol and command set,
UART RX driver (ring buffer), CRC, robustness against malformed input.
**Out of scope (v1.0):** real ADC accuracy and calibration, MOSFET switching timing on
hardware, EMC, cell balancing, SoC estimation.

## 3. Test levels

| Level | What | How | Tools | Owner |
|-------|------|-----|-------|-------|
| Static | Code & requirements review, static analysis | Warnings-as-errors build, cppcheck, peer review | GCC `-Wall -Wextra -Wconversion -Werror`, cppcheck | Dev + QA |
| Unit | Each C module in isolation, hardware faked | White-box, automated | Unity, fake HAL, gcov/gcovr | Dev + QA |
| Integration | ISR → ring buffer → parser → command → UART TX | Automated | Unity (`test_app.c`) | QA |
| System / functional | Firmware as a black box through UART + test bench | Automated, requirement-driven | pytest + BMS simulator (HIL-style) | QA |
| Manual | Scripted test cases + exploratory sessions | Manual execution with the test console | `tools/bms_console.py`, Excel/CSV, Jira | QA |
| Regression | Everything above, on every commit | CI pipeline | GitHub Actions | CI |

## 4. Test design techniques
- **Boundary value analysis** — every threshold is tested at limit − 1, limit, limit + 1
  (e.g. 4199 / 4200 / 4201 mV; 29 999 / 30 000 mA).
- **Equivalence partitioning** — valid / invalid cell readings, valid / invalid frames, command arguments.
- **State transition testing** — INIT → IDLE ⇄ CHARGING / DISCHARGING → FAULT → (clear) → operating states.
- **Decision table** — `CLEAR_FAULTS` outcome = f(fault latched?, condition still active?).
- **Timing / debounce** — faults must not trip at 200 ms, must trip at 300 ms.
- **Error guessing & robustness** — garbage input, over-long lines, CRLF, lowercase hex, 500-command stress.
- **Defect seeding** — a known defect was planted to prove the suite detects it (see BMS-1).

## 5. Entry / exit criteria
**Entry:** firmware builds with zero warnings; simulator starts; requirements baselined.
**Exit (release):**
- 100 % of automated tests pass
- 100 % of requirements covered by ≥ 1 test (traceability matrix)
- Line coverage of `firmware/src` ≥ 90 %
- All manual test cases executed; no open *Critical* or *Major* defects
- cppcheck: zero warnings

## 6. Test environment
- Windows 11 / Ubuntu (CI), GCC 13+, CMake 3.20+, Ninja, Python 3.10+
- DUT: `bms_sim` — the production firmware (`firmware/src`) linked with a simulated HAL.
  The **test bench channel** (`@` commands) injects sensor values and reads FET outputs,
  the **UART channel** carries the real protocol — mirroring a hardware-in-the-loop rig.

## 7. Defect management (Jira)

Workflow: `Open → In Progress → Resolved → Verified (retest) → Closed` (`Reopened` if retest fails).

| Severity | Definition (BMS context) |
|----------|--------------------------|
| Critical | Safety function fails: pack not protected, FETs stay ON in a fault |
| Major | Wrong fault / state reported, command executes incorrectly |
| Minor | Wrong error code, cosmetic protocol issue, tooling issue with workaround |
| Trivial | Documentation / typo |

Priority (P1–P4) is set by the team lead based on severity and release impact.
Every bug report must contain: steps to reproduce, expected vs actual, requirement ID,
firmware version, environment and evidence (test log / console transcript).

## 8. Deliverables
Test plan (this document) · manual test cases (`manual_test_cases.csv`) ·
exploratory charters · automated test suites · Jira bug reports ·
test dashboard with traceability matrix and coverage (`reports/dashboard.html`).

## 9. Risks
| Risk | Mitigation |
|------|------------|
| Simulator behaves differently from hardware | HAL boundary kept thin; same tests can run against a board by swapping the transport for pyserial |
| Timing only simulated (ticks) | Real-time behaviour to be verified on hardware with a logic analyser in a later phase |
| Test oracle errors | Expected CRC values computed with an independent Python implementation |
