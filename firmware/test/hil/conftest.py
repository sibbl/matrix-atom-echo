from __future__ import annotations

import os
import subprocess
import time
from pathlib import Path

import pytest

serial = pytest.importorskip("serial")


class SerialHarness:
    def __init__(self, connection: serial.Serial) -> None:
        self.connection = connection

    def drain(self, duration: float = 0.5) -> str:
        deadline = time.monotonic() + duration
        chunks: list[str] = []

        while time.monotonic() < deadline:
            waiting = self.connection.in_waiting
            if waiting:
                chunks.append(self.connection.read(waiting).decode("utf-8", errors="replace"))
            else:
                time.sleep(0.05)

        return "".join(chunks)

    def expect(self, needle: str, timeout: float = 10.0) -> str:
        deadline = time.monotonic() + timeout
        chunks: list[str] = []

        while time.monotonic() < deadline:
            waiting = self.connection.in_waiting
            if waiting:
                chunks.append(self.connection.read(waiting).decode("utf-8", errors="replace"))
                combined = "".join(chunks)
                if needle in combined:
                    return combined
            else:
                time.sleep(0.05)

        raise AssertionError(f"Timed out waiting for {needle!r}. Output so far:\n{''.join(chunks)}")

    def command(self, command: str, expect: str, timeout: float = 10.0) -> str:
        self.drain(0.2)
        self.connection.write(f"{command}\n".encode("utf-8"))
        self.connection.flush()
        return self.expect(expect, timeout=timeout)


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--port",
        action="store",
        default=None,
        help="ESP32 serial port for hardware-in-the-loop tests",
    )
    parser.addoption(
        "--baud",
        action="store",
        default="115200",
        help="Serial baud rate",
    )
    parser.addoption(
        "--flash",
        action="store_true",
        default=False,
        help="Flash firmware with idf.py before running tests",
    )
    parser.addoption(
        "--idf-path",
        action="store",
        default=None,
        help="ESP-IDF path to source export.sh from when --flash is used",
    )
    parser.addoption(
        "--firmware-dir",
        action="store",
        default=str(Path(__file__).resolve().parents[2]),
        help="Firmware project directory",
    )
    parser.addoption(
        "--boot-timeout",
        action="store",
        default="15",
        help="Seconds to wait for initial boot output after opening the serial port",
    )
    parser.addoption(
        "--bridge-url",
        action="store",
        default=None,
        help="Bridge URL reachable from the ESP32 for optional network round-trip tests",
    )
    parser.addoption(
        "--wifi-ssid",
        action="store",
        default=None,
        help="Wi-Fi SSID for optional network round-trip tests",
    )
    parser.addoption(
        "--wifi-password",
        action="store",
        default="",
        help="Wi-Fi password for optional network round-trip tests",
    )
    parser.addoption(
        "--device-token",
        action="store",
        default=None,
        help="Optional device auth token to push over serial before network tests",
    )


@pytest.fixture(scope="session")
def serial_port(pytestconfig: pytest.Config) -> str:
    port = pytestconfig.getoption("port")
    if not port:
        pytest.skip("No serial port supplied. Use --port /dev/cu.usbserialXXXX")

    return str(port)


@pytest.fixture(scope="session")
def firmware_dir(pytestconfig: pytest.Config) -> Path:
    return Path(pytestconfig.getoption("firmware_dir")).resolve()


@pytest.fixture(scope="session", autouse=True)
def maybe_flash(pytestconfig: pytest.Config, serial_port: str, firmware_dir: Path) -> None:
    if not pytestconfig.getoption("flash"):
        return

    idf_path = pytestconfig.getoption("idf_path") or os.environ.get("IDF_PATH")
    if not idf_path:
        pytest.skip("--flash requires --idf-path or IDF_PATH in the environment")

    command = (
        f'export IDF_PATH="{idf_path}" && '
        f'. "$IDF_PATH/export.sh" && '
        f'cd "{firmware_dir}" && '
        f'idf.py -p "{serial_port}" flash'
    )
    subprocess.run(["bash", "-lc", command], check=True)


@pytest.fixture(scope="session")
def serial_connection(
    pytestconfig: pytest.Config,
    serial_port: str,
    maybe_flash: None,
) -> serial.Serial:
    connection = serial.Serial(
        serial_port,
        baudrate=int(pytestconfig.getoption("baud")),
        timeout=0.1,
        write_timeout=1,
    )
    time.sleep(float(pytestconfig.getoption("boot_timeout")))
    connection.reset_input_buffer()
    connection.reset_output_buffer()

    try:
        yield connection
    finally:
        connection.close()


@pytest.fixture
def serial_harness(serial_connection: serial.Serial) -> SerialHarness:
    return SerialHarness(serial_connection)


@pytest.fixture(autouse=True)
def ensure_idle(serial_harness: SerialHarness) -> None:
    serial_harness.command("idle", "state override:", timeout=5)


@pytest.fixture
def configured_network(pytestconfig: pytest.Config, serial_harness: SerialHarness) -> dict[str, str]:
    bridge_url = pytestconfig.getoption("bridge_url")
    wifi_ssid = pytestconfig.getoption("wifi_ssid")
    wifi_password = pytestconfig.getoption("wifi_password")
    device_token = pytestconfig.getoption("device_token")

    if not bridge_url or not wifi_ssid:
        pytest.skip("Network HIL tests require --bridge-url and --wifi-ssid")

    serial_harness.command(f"set bridge {bridge_url}", "bridge URL updated", timeout=5)
    serial_harness.command(f"set ssid {wifi_ssid}", "Wi-Fi SSID updated", timeout=5)
    serial_harness.command(f"set pass {wifi_password}", "Wi-Fi password updated", timeout=5)

    if device_token:
        serial_harness.command(f"set token {device_token}", "device auth token updated", timeout=5)

    connect_output = serial_harness.command("wifi connect", "wifi connect ->", timeout=45)
    assert "ESP_OK" in connect_output, connect_output

    return {
        "bridge_url": bridge_url,
        "wifi_ssid": wifi_ssid,
    }
