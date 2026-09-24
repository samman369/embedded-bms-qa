# BMS-2 — Test console crashes on non-ASCII input (e.g. UTF-8 BOM)

| Field | Value |
|-------|-------|
| **Project** | BMS |
| **Issue Type** | Bug |
| **Status** | Closed (Verified) |
| **Severity** | Minor — test tooling, workaround exists |
| **Priority** | P3 — Medium |
| **Component** | Tools / `tools/bms_console.py` |
| **Affects Version** | 1.0.0-dev |
| **Fix Version** | 1.0.0 |
| **Requirement** | Test tooling (no firmware requirement) |
| **Found by** | Manual testing — scripted input piped from PowerShell |
| **Labels** | `tooling` `robustness` `encoding` |
| **Environment** | Windows 11, PowerShell 5.1, Python 3.14 |

## Summary
Piping a command file into the console from PowerShell crashes the tool with
`UnicodeEncodeError` on the first line. The session ends and the transcript is lost.

## Steps to reproduce
1. Create `input.txt` containing `PING` on the first line.
2. In Windows PowerShell 5.1 run: `Get-Content input.txt | python tools/bms_console.py`

## Expected result
`OK,PONG` is printed. Invalid characters are reported without ending the session.

## Actual result
```
File "tests/functional/bms_dut.py", line 23, in crc8
    for byte in data.encode("ascii"):
UnicodeEncodeError: 'ascii' codec can't encode character '﻿' in position 0
```

## Analysis
PowerShell 5.1 prefixes piped text with a UTF-8 byte-order mark (U+FEFF). The console
passed it into the CRC calculation, which only accepts ASCII, and the exception was not handled.
Any non-ASCII keystroke (e.g. `°`, `µ`) triggers the same crash.

## Resolution & verification
- Strip a leading BOM from each input line.
- Catch `UnicodeEncodeError` and print a clear message instead of crashing.
- Retest: same command file now prints `OK,PONG`; typing `PÏNG` shows the error message and the session continues.
