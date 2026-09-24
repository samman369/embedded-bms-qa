"""Functional tests of the UART command protocol, including negative and robustness tests."""
import re

import pytest

from bms_dut import crc8, frame

req = pytest.mark.requirement


@req("REQ-COM-010")
def test_ping(dut):
    assert dut.cmd("PING") == "OK,PONG"


@req("REQ-COM-011")
def test_version_is_semver(dut):
    assert re.fullmatch(r"OK,VERSION=\d+\.\d+\.\d+", dut.cmd("GET_VERSION"))


@req("REQ-COM-012")
def test_status_reflects_bench_inputs(dut):
    dut.set_cell(0, 3650)
    dut.set_cell(3, 3900)
    dut.set_temp(-105)
    dut.set_current(-2500)
    dut.tick(1)
    st = dut.status()
    assert (st.state, st.vmin, st.vmax, st.temp, st.cur) == ("DISCHARGING", 3650, 3900, -105, -2500)


@req("REQ-COM-013")
@pytest.mark.parametrize("index", [0, 1, 2, 3])
def test_get_cell_each_index(dut, index):
    dut.set_cell(index, 3600 + index)
    dut.tick(1)
    assert dut.cmd(f"GET_CELL,{index}") == f"OK,CELL{index}={3600 + index}"


@req("REQ-COM-013")
@pytest.mark.parametrize("arg", ["4", "-1", "X", "", "01234", "1,1"])
def test_get_cell_rejects_bad_index(dut, arg):
    assert dut.cmd(f"GET_CELL,{arg}") == "ERR,ARG"


@req("REQ-COM-014")
def test_clear_faults_without_fault(dut):
    assert dut.cmd("CLEAR_FAULTS") == "OK,CLEARED"


@req("REQ-COM-001")
def test_lowercase_crc_is_accepted(dut):
    assert dut.send_raw(f"$PING*{crc8('PING'):02x}") == "OK,PONG"


@req("REQ-COM-001")
def test_crlf_line_ending_is_accepted(dut):
    assert dut.send_raw(frame("PING") + "\r") == "OK,PONG"


@req("REQ-COM-002")
def test_wrong_crc_is_rejected_and_not_executed(dut):
    dut.set_temp(650)
    dut.tick(3)
    dut.set_temp(250)
    dut.tick(1)
    bad = f"$CLEAR_FAULTS*{(crc8('CLEAR_FAULTS') ^ 0xFF):02X}"
    assert dut.send_raw(bad) == "ERR,CRC"
    assert dut.status().state == "FAULT"          # command was NOT executed


@req("REQ-COM-003")
@pytest.mark.parametrize("line", [
    "PING*1F",          # missing $
    "$PING1F",          # missing *
    "$PING*ZZ",         # non-hex CRC
    "$*00",             # empty payload
    "$PI*NG*1F",        # illegal char in payload
    "hello world",      # garbage
])
def test_malformed_frames(dut, line):
    assert dut.send_raw(line) == "ERR,FORMAT"


@req("REQ-COM-004")
def test_max_length_frame_is_processed(dut):
    payload = "X" * 60                               # 60 + 4 overhead = 64 chars
    assert len(frame(payload)) == 64
    assert dut.cmd(payload) == "ERR,UNKNOWN_CMD"     # got past the length check


@req("REQ-COM-004")
def test_over_length_frame_rejected_and_link_recovers(dut):
    assert dut.cmd("X" * 61) == "ERR,LENGTH"         # 65 chars
    assert dut.send_raw("Y" * 300) == "ERR,LENGTH"
    assert dut.cmd("PING") == "OK,PONG"


@req("REQ-COM-005")
@pytest.mark.parametrize("payload", ["REBOOT", "ping", "PING ", "GET_STATUS,1"])
def test_unknown_commands(dut, payload):
    assert dut.cmd(payload) == "ERR,UNKNOWN_CMD"


@req("REQ-COM-001", "REQ-DRV-001")
def test_stress_500_commands_without_loss(dut):
    for i in range(500):
        assert dut.cmd("PING") == "OK,PONG", f"failed at iteration {i}"
    assert dut.bench("RXOVF?") == "@RXOVF 0"
