"""Interactive test console for MANUAL testing of the BMS (like a serial terminal + lab bench).

    python tools/bms_console.py

  PING                 send a command (CRC is added for you)
  @CELL 0 4200         bench: set cell 0 to 4200 mV   (@CELLS, @TEMP, @CUR, @TICK, @FETS?, @RESET)
  !raw $PING*00        send a raw line exactly as typed (for negative tests)
  status               pretty-print GET_STATUS + FET outputs
  log <file>           save this session's transcript (attach it to a Jira bug)
  help / quit
"""
from __future__ import annotations

import sys
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tests" / "functional"))
from bms_dut import BmsDut, ResponseError, frame  # noqa: E402

GREEN, RED, CYAN, DIM, RESET = "\033[32m", "\033[31m", "\033[36m", "\033[2m", "\033[0m"
FAULT_NAMES = {0x01: "OV", 0x02: "UV", 0x04: "OT", 0x08: "OC", 0x10: "SENSOR"}


def colour(text: str) -> str:
    if text.startswith("OK") or text == "@OK":
        return GREEN + text + RESET
    if text.startswith("ERR") or text == "@ERR":
        return RED + text + RESET
    return CYAN + text + RESET


def print_status(dut: BmsDut) -> None:
    st = dut.status()
    chg, dsg = dut.fets()
    faults = ", ".join(n for bit, n in FAULT_NAMES.items() if st.faults & bit) or "none"
    print(f"  State   : {st.state}")
    print(f"  Faults  : 0x{st.faults:02X} ({faults})")
    print(f"  Cells   : min {st.vmin} mV / max {st.vmax} mV")
    print(f"  Temp    : {st.temp / 10:.1f} °C      Current: {st.cur} mA")
    print(f"  FETs    : CHG={'ON' if chg else 'OFF'}  DSG={'ON' if dsg else 'OFF'}")


def main() -> int:
    dut = BmsDut().start()
    print(f"{CYAN}BMS test console{RESET} - connected to simulator. Type 'help'.")
    try:
        while True:
            try:
                line = input("bms> ").lstrip("﻿").strip()
            except EOFError:
                break
            if not line:
                continue
            try:
                if line in ("quit", "exit"):
                    break
                if line == "help":
                    print(__doc__)
                elif line == "status":
                    print_status(dut)
                elif line.startswith("log"):
                    name = line[3:].strip() or f"session_{datetime.now():%Y%m%d_%H%M%S}.log"
                    Path(name).write_text("\n".join(dut.log) + "\n", encoding="utf-8")
                    print(f"{DIM}transcript saved to {name}{RESET}")
                elif line.startswith("@"):
                    print("  " + colour(dut.bench(line[1:])))
                elif line.startswith("!raw "):
                    print("  " + colour(dut.send_raw(line[5:])))
                else:
                    print(f"  {DIM}tx {frame(line)}{RESET}")
                    print("  " + colour(dut.cmd(line)))
            except UnicodeEncodeError:
                print(f"  {RED}Only ASCII characters can be sent (use !raw to test other bytes){RESET}")
            except (ResponseError, TimeoutError) as exc:
                print(f"  {RED}{exc}{RESET}")
    finally:
        dut.stop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
