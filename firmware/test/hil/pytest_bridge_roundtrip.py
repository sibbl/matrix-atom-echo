def test_bridge_health_and_roundtrip(
    serial_harness,
    configured_network: dict[str, str],
) -> None:
    health_output = serial_harness.command("health", "bridge health status=200", timeout=20)
    assert "bridge health status=200" in health_output

    serial_harness.command("single", "transition=IDLE->RECORDING", timeout=5)
    stop_output = serial_harness.command("single", "transition=RECORDING->WAITING_FOR_REPLY", timeout=20)
    assert "recording upload accepted" in stop_output

    poll_output = serial_harness.command("poll", "received audio payload:", timeout=20)
    assert "received audio payload:" in poll_output

    playback_output = serial_harness.expect("playback done", timeout=20)
    assert "playback done" in playback_output
