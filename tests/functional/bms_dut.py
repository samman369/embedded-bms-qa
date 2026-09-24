"""Driver for the BMS device under test (DUT).

Talks to the simulator over stdin/stdout. The same class could be pointed at a
real board by swapping the transport for pyserial - the tests would not change.
"""
from __future__ import annotations

import os
import queue
import re
import subprocess
import threading
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SIM = ROOT / "build" / ("bms_sim.exe" if os.name == "nt" else "bms_sim")


def crc8(data: str) -> int:
    """CRC-8/SMBUS, independent reference implementation (test oracle)."""
    crc = 0
    for byte in data.encode("ascii"):
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def frame(payload: str) -> str:
    return f"${payload}*{crc8(payload):02X}"


FRAME_RE = re.compile(r"^\$(?P<payload>[^$*]+)\*(?P<crc>[0-9A-Fa-f]{2})$")


@dataclass
class Status:
    state: str
    faults: int
    vmin: int
    vmax: int
    temp: int
    cur: int


class ResponseError(AssertionError):
    pass


class BmsDut:
    def __init__(self, sim_path: str | os.PathLike | None = None, timeout: float = 2.0):
        self.sim_path = Path(sim_path or os.environ.get("BMS_SIM", DEFAULT_SIM))
        self.timeout = timeout
        self.log: list[str] = []          # full bench/UART transcript, attached on failure
        self._proc: subprocess.Popen | None = None
        self._lines: queue.Queue[str] = queue.Queue()

    # ------------------------------------------------------------ lifecycle
    def start(self) -> "BmsDut":
        if not self.sim_path.exists():
            raise FileNotFoundError(f"Simulator not found: {self.sim_path} (build the project first)")
        self._proc = subprocess.Popen(
            [str(self.sim_path)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            text=True, bufsize=1)
        threading.Thread(target=self._reader, daemon=True).start()
        banner = self._read_line()
        if not banner.startswith("@READY"):
            raise RuntimeError(f"Unexpected banner: {banner!r}")
        return self

    def stop(self) -> None:
        if self._proc and self._proc.poll() is None:
            try:
                self._write("@QUIT")
                self._proc.wait(timeout=2)
            except Exception:
                self._proc.kill()

    def _reader(self) -> None:
        assert self._proc and self._proc.stdout
        for line in self._proc.stdout:
            self._lines.put(line.rstrip("\r\n"))

    def _write(self, line: str) -> None:
        assert self._proc and self._proc.stdin
        self.log.append(f">> {line}")
        self._proc.stdin.write(line + "\n")
        self._proc.stdin.flush()

    def _read_line(self) -> str:
        try:
            line = self._lines.get(timeout=self.timeout)
        except queue.Empty:
            raise TimeoutError("DUT did not answer in time") from None
        self.log.append(f"<< {line}")
        return line

    # ------------------------------------------------------------ test bench
    def bench(self, command: str) -> str:
        self._write("@" + command)
        reply = self._read_line()
        if reply == "@ERR":
            raise ResponseError(f"Bench rejected: {command}")
        return reply

    def set_cell(self, index: int, mv: int) -> None:  self.bench(f"CELL {index} {mv}")
    def set_all_cells(self, mv: int) -> None:         self.bench(f"CELLS {mv}")
    def set_temp(self, deci_c: int) -> None:          self.bench(f"TEMP {deci_c}")
    def set_current(self, ma: int) -> None:           self.bench(f"CUR {ma}")
    def tick(self, n: int = 1) -> None:               self.bench(f"TICK {n}")
    def reset(self) -> None:                          self.bench("RESET")

    def fets(self) -> tuple[bool, bool]:
        m = re.match(r"@FETS CHG=(\d) DSG=(\d)", self.bench("FETS?"))
        assert m, "bad FETS reply"
        return m.group(1) == "1", m.group(2) == "1"

    # ------------------------------------------------------------ UART
    def send_raw(self, line: str) -> str:
        """Send a raw line to the DUT UART and return the decoded response payload."""
        self._write(line)
        reply = self._read_line()
        m = FRAME_RE.match(reply)
        if not m:
            raise ResponseError(f"Malformed response frame: {reply!r}")
        payload = m.group("payload")
        if int(m.group("crc"), 16) != crc8(payload):
            raise ResponseError(f"Response CRC mismatch: {reply!r}")
        return payload

    def cmd(self, payload: str) -> str:
        return self.send_raw(frame(payload))

    def status(self) -> Status:
        reply = self.cmd("GET_STATUS")
        m = re.match(r"OK,STATE=(\w+),FAULTS=0x([0-9A-F]{2}),VMIN=(\d+),VMAX=(\d+),TEMP=(-?\d+),CUR=(-?\d+)$", reply)
        if not m:
            raise ResponseError(f"Bad status: {reply!r}")
        return Status(m.group(1), int(m.group(2), 16), int(m.group(3)), int(m.group(4)),
                      int(m.group(5)), int(m.group(6)))
