import pytest

from bms_dut import BmsDut

FAULT_OV, FAULT_UV, FAULT_OT, FAULT_OC, FAULT_SENSOR = 0x01, 0x02, 0x04, 0x08, 0x10


def pytest_configure(config):
    config.addinivalue_line("markers", "requirement(*ids): requirement(s) verified by this test")


@pytest.fixture(autouse=True)
def _record_requirements(request, record_property):
    """Writes requirement IDs into the JUnit XML so the dashboard can build the traceability matrix."""
    marker = request.node.get_closest_marker("requirement")
    if marker:
        record_property("requirements", ",".join(marker.args))


@pytest.fixture
def dut(request):
    """A freshly powered-on DUT with nominal inputs, advanced past INIT."""
    d = BmsDut().start()
    d.tick(1)
    yield d
    d.stop()
    rep = getattr(request.node, "rep_call", None)
    if rep is not None and rep.failed:
        print("\n--- DUT transcript ---\n" + "\n".join(d.log))


@pytest.fixture
def cold_dut():
    """DUT straight after power-on, no cycle executed."""
    d = BmsDut().start()
    yield d
    d.stop()


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    rep = outcome.get_result()
    setattr(item, "rep_" + rep.when, rep)
