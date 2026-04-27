# Testing

## Overview

The project uses three test layers:

1. Bridge tests with Vitest
2. Firmware host-side logic tests
3. Firmware hardware-in-the-loop tests over USB

Use the combined wrapper when you want the standard local checks:

```bash
export ESP_TARGET="${ESP_TARGET:-esp32}"
./scripts/test-all.sh
```

## Bridge tests

Run:

```bash
cd bridge
npm install
npm run typecheck
npm test
npm run build
```

The bridge test suite must not require a real Matrix server or ElevenLabs API key.

Mock:

- Matrix media upload
- Matrix send message
- Matrix media download
- ElevenLabs TTS

The bridge defaults to local `echo` mode, so posting a recording immediately queues that same WAV for `/next-audio`. That gives fast end-to-end hardware testing even when Matrix and ElevenLabs are not configured.

The current suite covers config validation, optional device auth, device state helpers, thread-root reuse, reply queueing, and the main HTTP routes.

## Firmware host-side tests

Firmware logic should be structured so these parts can be tested without hardware:

- state machine
- button event classification
- WAV encoding
- config validation
- bridge endpoint construction

The current scaffold includes a host-side runner for the pure C modules under `firmware/main/`:

```bash
cd firmware
./test/host/run-host-tests.sh
```

For the ESP-IDF firmware build itself:

```bash
cd firmware
idf.py build
```

## Firmware hardware-in-the-loop tests

Hardware tests require a connected ESP32 over USB.

On macOS:

```bash
export ESPPORT="/dev/cu.usbmodemXXXX"
```

Manual flash and monitor loop:

```bash
cd firmware
idf.py -p "$ESPPORT" flash monitor
```

Optional pytest HIL scaffold:

```bash
cd firmware
python3 -m venv .venv-hil
. .venv-hil/bin/activate
pip install -r test/hil/requirements.txt
pytest test/hil --port "$ESPPORT" -k 'not roundtrip'
```

For the optional network round-trip test, also pass Wi-Fi and bridge settings:

```bash
pytest test/hil \
  --port "$ESPPORT" \
  --bridge-url "http://YOUR-LAPTOP-IP:3000" \
  --wifi-ssid "YOUR_WIFI" \
  --wifi-password "YOUR_PASSWORD"
```

The firmware exposes a serial command loop that the HIL suite uses:

- `idle`
- `status`
- `single`, `double`, `long`
- `health`, `poll`, `replay`, `thread-reset`
- `set bridge <url>`, `set ssid <name>`, `set pass <password>`, `set token <value>`, `wifi connect`

## Manual smoke test

Start the bridge scaffold:

```bash
cd bridge
npm install
npm run dev
```

In a second terminal, flash firmware:

```bash
cd firmware
idf.py -p "$ESPPORT" flash monitor
```

At the HTTP layer, the bridge accepts recordings and, in the default `echo` mode, queues that same audio back for `/next-audio`. Matrix and ElevenLabs integrations remain optional and stay in the bridge only.

## Curl bridge smoke test

Use this before testing real firmware:

```bash
curl -X POST \
  -H "Content-Type: audio/wav" \
  --data-binary @sample.wav \
  http://localhost:3000/devices/default/recording
```

Check the next-audio polling path:

```bash
curl -v \
  http://localhost:3000/devices/default/next-audio \
  --output reply.wav
```
