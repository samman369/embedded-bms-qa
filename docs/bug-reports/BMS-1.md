# BMS-1 — Over-voltage protection does not trip at exactly 4200 mV

| Field | Value |
|-------|-------|
| **Project** | BMS |
| **Issue Type** | Bug |
| **Status** | Closed (Verified) |
| **Severity** | Critical — safety function fails at the specified limit |
| **Priority** | P1 — Highest |
| **Component** | Protection / `firmware/src/bms.c` |
| **Affects Version** | 1.0.0-dev |
| **Fix Version** | 1.0.0 |
| **Requirement** | REQ-BMS-010 |
| **Found by** | Automated unit test `test_ov_trips_at_exactly_4200mV_after_3_cycles__REQ_BMS_010` |
| **Detection method** | Boundary value analysis (defect seeding exercise) |
| **Labels** | `safety` `boundary` `protection` `seeded-defect` |
| **Environment** | Windows 11, GCC 16.1 (WinLibs UCRT), Unity 2.6, host build with fake HAL |

## Summary
When a cell sits at exactly the over-voltage limit (4200 mV) for 3 cycles, no OV fault is
set and the FETs stay ON. The fault only trips from 4201 mV.

## Steps to reproduce
1. Build and run `build/test_bms.exe` — or manually in `tools/bms_console.py`:
2. `@TICK 1` (leave INIT)
3. `@CELL 2 4200`
4. `@TICK 3`
5. `status`

## Expected result
`State: FAULT`, `Faults: 0x01 (OV)`, charge and discharge FETs OFF (REQ-BMS-010: "≥ 4200 mV").

## Actual result
`State: IDLE`, `Faults: 0x00`, FETs ON. Unit test output:
```
test_bms.c:58:test_ov_trips_at_exactly_4200mV_after_3_cycles__REQ_BMS_010:FAIL: Expected 0x01 Was 0x00
31 Tests 1 Failures 0 Ignored
```
Full log: [evidence/BMS-1_unit_test_failure.txt](evidence/BMS-1_unit_test_failure.txt)

## Analysis
Off-by-one in the comparison operator:
```c
monitor_update(&s_mon[MON_OV], vmax >  BMS_OV_TRIP_MV, ...);   /* before */
monitor_update(&s_mon[MON_OV], vmax >= BMS_OV_TRIP_MV, ...);   /* after  */
```
Note the neighbouring test at 4199 mV **passed** — only a test exactly on the boundary finds this.

## Resolution & verification
- Fixed operator in `bms.c`.
- Retest: failing test passes. Regression: all 87 unit tests + 56 functional tests pass.
- Functional test `test_over_voltage_threshold[4200-True]` now also guards this boundary end-to-end.

## Note
This defect was deliberately seeded before the first test run to validate that the test
suite detects boundary errors in safety code (defect seeding / fault injection).
