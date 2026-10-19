# test_string.py — run: pip install pyvisa pyvisa-py pytest, then pytest -v
import time, pyvisa, pytest

IP = "192.168.2.179"  # update if the device's DHCP/static IP changes


@pytest.fixture(scope="module")
def dev():
    rm = pyvisa.ResourceManager("@py")
    d = rm.open_resource(f"TCPIP::{IP}::5025::SOCKET")
    d.read_termination = d.write_termination = "\n"
    d.timeout = 2000
    d.write("*CLS")
    yield d
    d.close()


CASES = [
    ('TEST:ECHO? "hello"', '"hello"'),
    ('TEST:STRing:LENgth? "abc"', '3'),
    ('TEST:STRing:UPPer? "Ab1"', '"AB1"'),
    ('TEST:STRing:REVerse? "abc"', '"cba"'),
    ('TEST:STRing:CRC? "123456789"', '"CBF43926"'),
]


@pytest.mark.parametrize("cmd,expected", CASES)
def test_string(dev, cmd, expected):
    assert dev.query(cmd).strip() == expected
    assert dev.query("SYST:ERR?").startswith("0,") or dev.query("SYST:ERR?").startswith("+0,")


def test_idn(dev):
    idn = dev.query("*IDN?").strip()
    assert idn == "Company,CableTester,SN001,FW1.0"


def test_store_roundtrip(dev):
    dev.write('TEST:STRing:STORe "abc"')
    assert dev.query("TEST:STRing:STORe?").strip() == '"abc"'


def test_count_increases(dev):
    a = int(dev.query("TEST:COUNt?").strip())
    dev.query("TEST:COUNt?")
    b = int(dev.query("TEST:COUNt?").strip())
    assert b > a


def test_missing_param(dev):
    dev.write("TEST:STRing:LENgth?")
    assert dev.query("SYST:ERR?").startswith("-109")


def test_too_long(dev):
    dev.write(f'TEST:ECHO? "{"x" * 200}"')
    assert dev.query("SYST:ERR?").startswith("-223")


def test_latency(dev):
    n, t0 = 200, time.perf_counter()
    for _ in range(n):
        assert dev.query('TEST:ECHO? "ping"').strip() == '"ping"'
    rtt_ms = (time.perf_counter() - t0) / n * 1000
    print(f"average round trip {rtt_ms:.2f} ms over {n} requests")
