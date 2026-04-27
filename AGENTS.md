# Agent guidance

## Project constraints
- Use ESP-IDF for firmware work. Do not switch to ESPHome, Home Assistant, or Arduino.
- Keep secrets out of firmware. Matrix and ElevenLabs configuration belongs in the bridge only.
- Do not commit machine-specific serial ports or local USB paths.

## macOS USB development loop
Assume the primary development machine may be macOS.

ESP-IDF setup on macOS should follow Espressif's official setup flow.
For the currently attached Atom Echo-class board, use the `esp32` target.
If you are using a different board such as an ESP32-S3 devkit, override `ESP_TARGET` accordingly.

```bash
brew install cmake ninja dfu-util python3
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd ~/esp/esp-idf
./install.sh esp32
. ./export.sh
```

For ESP32-S3 boards:

```bash
./install.sh esp32s3
```

For Apple Silicon Macs, if ESP-IDF tooling reports `bad CPU type in executable`, install Rosetta:

```bash
/usr/sbin/softwareupdate --install-rosetta --agree-to-license
```

To find the connected ESP32 serial port on macOS:

```bash
ls /dev/cu.*
```

Typical ports look like:

- `/dev/cu.usbserial-xxxx`
- `/dev/cu.usbmodemxxxx`
- `/dev/cu.SLAB_USBtoUART`
- `/dev/cu.wchusbserialxxxx`

Flash and monitor:

```bash
cd firmware
export ESP_TARGET="${ESP_TARGET:-esp32}"
idf.py set-target "$ESP_TARGET"
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

`idf.py flash` automatically builds before flashing, so a separate `idf.py build` is not required for the normal flash loop.

Exit the serial monitor with `Ctrl + ]`.

Preferred continuous local development command:

```bash
cd firmware
idf.py -p "$ESPPORT" flash monitor
```

Recommended shell setup:

```bash
export IDF_PATH="$HOME/esp/esp-idf"
. "$IDF_PATH/export.sh"
export ESP_TARGET="esp32"
export ESPPORT="/dev/cu.usbmodemXXXX"
```

If flashing fails:

- verify the serial port with `ls /dev/cu.*`
- unplug and replug the board
- try holding `BOOT` while flashing
- if `esptool` reports the wrong chip family, switch the target:

```bash
idf.py set-target esp32
```

- try a lower baud rate:

```bash
idf.py -p "$ESPPORT" -b 115200 flash monitor
```

## Test suite strategy
This project must have tests at three levels.

### 1. Bridge tests
Run on the host without ESP32 hardware.

```bash
cd bridge
npm run typecheck
npm test
npm run build
```

Bridge tests should use Vitest and mock Matrix and ElevenLabs where possible.

Required bridge coverage:

- config validation
- optional device auth
- device state creation
- reset thread
- push audio
- pop next audio
- last audio replay
- first Matrix send creates thread root
- later Matrix sends reuse thread root
- bot text reply queues TTS audio
- bot audio reply queues downloaded audio

### 2. Firmware host-side tests
Run without ESP32 hardware where possible.

Test pure logic such as:

- state machine transitions
- button event classification
- WAV header generation
- bridge URL construction
- retry and backoff helpers
- config parsing

Keep hardware-specific I2S, Wi-Fi, and peripheral code behind interfaces so the pure logic can be tested in isolation.

### 3. Firmware hardware-in-the-loop tests
Optional but preferred for local macOS USB development.

Use ESP-IDF Unity tests and pytest automation where possible.

Hardware tests should verify:

- firmware boots
- Wi-Fi config is loaded
- button events are logged
- bridge health check works
- recording start and stop transitions work
- `/next-audio` polling handles `204`
- playback command path is reachable

Hardware tests may require:

```bash
export ESPPORT="/dev/cu.usbmodemXXXX"
cd firmware
pytest test/hil --port "$ESPPORT"
```

If hardware test tooling is unavailable, document the blocker and still run the firmware build.

The firmware also exposes a serial-driven HIL command loop for automation:

- `idle`
- `status`
- `single`, `double`, `long`
- `health`, `poll`, `replay`, `thread-reset`
- `set bridge <url>`, `set ssid <name>`, `set pass <password>`, `set token <value>`, `wifi connect`

## Firmware test layout

```text
firmware/
  main/
    main.c
    state_machine.c
    state_machine.h
    wav_encoder.c
    wav_encoder.h
    button_controller.c
    button_controller.h
  test/
    host/
      run-host-tests.sh
      test_state_machine.c
      test_wav_encoder.c
      test_button_controller.c
    hil/
      conftest.py
      pytest_firmware_boot.py
      pytest_button_events.py
```

Separate pure logic from ESP-IDF hardware code so host-side tests stay fast and deterministic.
