"""Functional (black-box) tests of the protection behaviour, through the UART and test bench only."""
import pytest

from conftest import FAULT_OC, FAULT_OT, FAULT_OV, FAULT_SENSOR, FAULT_UV

req = pytest.mark.requirement


# ------------------------------------------------------------------ power-up
@req("REQ-BMS-001")
def test_power_up_sequence(cold_dut):
    assert cold_dut.status().state == "INIT"
    assert cold_dut.fets() == (False, False)
    cold_dut.tick(1)
    st = cold_dut.status()
    assert (st.state, st.faults) == ("IDLE", 0)
    assert cold_dut.fets() == (True, True)


# ------------------------------------------------------------------ thresholds (boundary value analysis)
@req("REQ-BMS-010")
@pytest.mark.parametrize("mv, expect_fault", [(4199, False), (4200, True), (4201, True)])
def test_over_voltage_threshold(dut, mv, expect_fault):
    dut.set_cell(1, mv)
    dut.tick(3)
    assert bool(dut.status().faults & FAULT_OV) is expect_fault


@req("REQ-BMS-020")
@pytest.mark.parametrize("mv, expect_fault", [(3001, False), (3000, True), (2999, True)])
def test_under_voltage_threshold(dut, mv, expect_fault):
    dut.set_cell(2, mv)
    dut.tick(3)
    assert bool(dut.status().faults & FAULT_UV) is expect_fault


@req("REQ-BMS-030")
@pytest.mark.parametrize("deci_c, expect_fault", [(599, False), (600, True)])
def test_over_temperature_threshold(dut, deci_c, expect_fault):
    dut.set_temp(deci_c)
    dut.tick(3)
    assert bool(dut.status().faults & FAULT_OT) is expect_fault


@req("REQ-BMS-040")
@pytest.mark.parametrize("ma, expect_fault", [
    (-29999, False), (-30000, True),     # discharge limit
    (9999, False), (10000, True),        # charge limit
])
def test_over_current_threshold(dut, ma, expect_fault):
    dut.set_current(ma)
    dut.tick(3)
    assert bool(dut.status().faults & FAULT_OC) is expect_fault


# ------------------------------------------------------------------ timing
@req("REQ-BMS-010", "REQ-BMS-050")
def test_fault_needs_300ms_and_fets_open_immediately(dut):
    dut.set_cell(0, 4300)
    dut.tick(2)                                   # 200 ms - still debouncing
    assert dut.status().state != "FAULT"
    assert dut.fets() == (True, True)
    dut.tick(1)                                   # 300 ms
    assert dut.status().state == "FAULT"
    assert dut.fets() == (False, False)


# ------------------------------------------------------------------ hysteresis & latching
@req("REQ-BMS-011", "REQ-BMS-051")
def test_over_voltage_hysteresis_and_recovery(dut):
    dut.set_cell(3, 4250)
    dut.tick(3)
    dut.set_cell(3, 4150)                          # inside hysteresis band
    dut.tick(3)
    assert dut.cmd("CLEAR_FAULTS") == "ERR,FAULT_ACTIVE"
    dut.set_cell(3, 4100)
    dut.tick(1)
    assert dut.cmd("CLEAR_FAULTS") == "OK,CLEARED"
    assert dut.status().state == "IDLE"
    assert dut.fets() == (True, True)


@req("REQ-BMS-021")
def test_under_voltage_release(dut):
    dut.set_all_cells(2800)
    dut.tick(3)
    dut.set_all_cells(3099)
    dut.tick(1)
    assert dut.cmd("CLEAR_FAULTS") == "ERR,FAULT_ACTIVE"
    dut.set_all_cells(3100)
    dut.tick(1)
    assert dut.cmd("CLEAR_FAULTS") == "OK,CLEARED"


@req("REQ-BMS-031")
def test_over_temperature_release(dut):
    dut.set_temp(700)
    dut.tick(3)
    dut.set_temp(551)
    dut.tick(1)
    assert dut.cmd("CLEAR_FAULTS") == "ERR,FAULT_ACTIVE"
    dut.set_temp(550)
    dut.tick(1)
    assert dut.cmd("CLEAR_FAULTS") == "OK,CLEARED"


@req("REQ-BMS-051")
def test_fault_stays_latched_without_clear(dut):
    dut.set_current(-40000)
    dut.tick(3)
    dut.set_current(0)
    dut.tick(50)                                   # 5 seconds later
    st = dut.status()
    assert (st.state, st.faults) == ("FAULT", FAULT_OC)
    assert dut.fets() == (False, False)


@req("REQ-BMS-051")
def test_fault_survives_until_power_cycle_or_clear(dut):
    dut.set_temp(650)
    dut.tick(3)
    dut.reset()                                    # power cycle: faults are RAM-only
    assert dut.status().state == "INIT"


# ------------------------------------------------------------------ sensor plausibility
@req("REQ-BMS-045")
def test_open_wire_gives_sensor_fault_not_under_voltage(dut):
    dut.set_cell(2, 0)
    dut.tick(1)
    st = dut.status()
    assert st.faults == FAULT_SENSOR
    assert st.vmin == 3700                          # broken reading excluded


# ------------------------------------------------------------------ operating state
@req("REQ-BMS-060")
@pytest.mark.parametrize("ma, state", [
    (0, "IDLE"), (99, "IDLE"), (100, "CHARGING"), (-99, "IDLE"), (-100, "DISCHARGING"),
])
def test_operating_state_follows_current(dut, ma, state):
    dut.set_current(ma)
    dut.tick(1)
    assert dut.status().state == state


# ------------------------------------------------------------------ end-to-end scenario
@req("REQ-BMS-060", "REQ-BMS-030", "REQ-BMS-051")
def test_scenario_charge_overheat_cool_down_resume(dut):
    """User story: charging pack overheats, BMS cuts off, pack cools, operator clears, charging resumes."""
    dut.set_current(3000)
    dut.tick(5)
    assert dut.status().state == "CHARGING"

    for t in range(560, 660, 20):                   # temperature ramps up
        dut.set_temp(t)
        dut.tick(1)
    dut.tick(3)
    assert dut.status().state == "FAULT"
    assert dut.fets() == (False, False)

    dut.set_temp(400)
    dut.tick(10)
    assert dut.cmd("CLEAR_FAULTS") == "OK,CLEARED"
    assert dut.status().state == "CHARGING"
