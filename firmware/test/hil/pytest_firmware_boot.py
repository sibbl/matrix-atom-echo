def test_firmware_boot_and_status(serial_harness) -> None:
    status_output = serial_harness.command("status", "status:", timeout=5)
    assert "state=IDLE" in status_output

    help_output = serial_harness.command("help", "serial commands:", timeout=5)
    assert "single | double | long" in help_output
