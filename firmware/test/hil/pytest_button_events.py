def test_button_event_commands(serial_harness) -> None:
    first = serial_harness.command("single", "transition=IDLE->RECORDING", timeout=5)
    assert "action=start_recording" in first

    second = serial_harness.command("double", "transition=RECORDING->IDLE", timeout=5)
    assert "action=cancel_recording" in second

    replay = serial_harness.command("long", "transition=IDLE->IDLE", timeout=5)
    assert "action=replay_last_audio" in replay


def test_recording_start_stop_transition(serial_harness) -> None:
    serial_harness.command("single", "transition=IDLE->RECORDING", timeout=5)
    stop = serial_harness.command("single", "transition=RECORDING->WAITING_FOR_REPLY", timeout=10)
    assert "action=stop_recording" in stop
