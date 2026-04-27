# Matrix Atom Echo

An ESP-IDF firmware + bridge service for a USB-connected ESP32 voice device.

The bridge can run in simple **echo** mode with no external services, or it can integrate with **Matrix** and **ElevenLabs**. The firmware stays secret-free; all Matrix and ElevenLabs credentials live in the bridge only.

## What you get

- A bridge service you can run locally or in Docker
- A GHCR image published from GitHub Actions on `main`
- A sample `docker-compose.yaml` ready for Portainer stacks
- Optional bridge **Basic Auth**
- Optional device **Bearer token** auth for the ESP
- ESP-IDF firmware for `esp32` by default, with a configurable `ESP_TARGET`
- Host-side tests and serial-driven hardware-in-the-loop tests

## Intended end state

Yes: after this repo is pushed and the `main` branch workflow publishes the image, the remaining steps are:

1. Deploy the bridge image in Portainer with your environment variables.
2. Point the firmware at the bridge URL reachable from your LAN.
3. Flash the ESP and set Wi-Fi / bridge settings as described below.

## Bridge image

The GitHub workflow publishes the bridge container image to:

```text
ghcr.io/sibbl/matrix-atom-echo-bridge:latest
```

Additional tags:

- `:main`
- `:sha-<short-sha>`

The workflow only runs on pushes to the `main` branch.

## Portainer / Docker deployment

### 1. Copy the sample environment

Use `.env.example` as the template for your stack variables:

```bash
cp .env.example .env
```

Or, in Portainer, define the same values as stack environment variables.

### 2. Review the required bridge settings

Minimum recommended settings:

- `BRIDGE_BASE_URL=http://YOUR_LAN_IP:3000`
- `DEVICE_AUTH_TOKEN=some-long-random-string`
- `BRIDGE_REPLY_MODE=echo`

`BRIDGE_BASE_URL` must be reachable from the ESP32 on your local network.

### 3. Deploy with the sample compose file

```bash
docker compose up -d
```

For Portainer, use the root `docker-compose.yaml` as your stack file.

## Environment variables

| Variable | Required | Purpose |
| --- | --- | --- |
| `GHCR_IMAGE` | No | Container image override for Portainer / Compose. |
| `BRIDGE_PORT` | No | Host port mapping in `docker-compose.yaml`. |
| `BRIDGE_BASE_URL` | Yes | Public/LAN URL the ESP32 should call. |
| `BRIDGE_REPLY_MODE` | No | `echo`, `matrix`, or `none`. Default is `echo`. |
| `DEVICE_AUTH_TOKEN` | Strongly recommended | Bearer token accepted on device routes. |
| `BRIDGE_BASIC_AUTH_USERNAME` | No | Optional Basic Auth username for bridge admin/integration access. |
| `BRIDGE_BASIC_AUTH_PASSWORD` | No | Optional Basic Auth password. |
| `MATRIX_HOMESERVER_URL` | Matrix mode only | Matrix homeserver base URL. |
| `MATRIX_ACCESS_TOKEN` | Matrix mode only | Matrix access token. |
| `MATRIX_ROOM_ID` | Matrix mode only | Target Matrix room. |
| `MATRIX_USER_ID` | No | Optional Matrix sender user ID metadata. |
| `MATRIX_BOT_USER_ID` | No | Optional Matrix bot user ID metadata. |
| `ELEVENLABS_API_KEY` | ElevenLabs TTS only | ElevenLabs API key. |
| `ELEVENLABS_VOICE_ID` | ElevenLabs TTS only | Voice ID. |
| `ELEVENLABS_MODEL_ID` | No | Defaults to `eleven_multilingual_v2`. |
| `ELEVENLABS_OUTPUT_FORMAT` | No | Defaults to `mp3_44100_128`. |

## Security model

There are two independent auth layers:

### Device auth

Use `DEVICE_AUTH_TOKEN` to protect the ESP-facing routes:

- `POST /devices/:deviceId/recording`
- `GET /devices/:deviceId/next-audio`
- `POST /devices/:deviceId/replay-last-audio`
- `POST /devices/:deviceId/thread/reset`
- `GET /devices/:deviceId/state`

The ESP firmware is designed to use this token.

### Optional bridge Basic Auth

Set both:

- `BRIDGE_BASIC_AUTH_USERNAME`
- `BRIDGE_BASIC_AUTH_PASSWORD`

When enabled, the bridge requires HTTP Basic Auth for integration/admin routes and also accepts it on device routes. The ESP can still use `DEVICE_AUTH_TOKEN`, so enabling Basic Auth does **not** force firmware changes.

`/health` intentionally stays open for simple health checks.

## Default deployment mode

The safest way to get started is:

- `BRIDGE_REPLY_MODE=echo`
- `DEVICE_AUTH_TOKEN` set
- Basic Auth enabled if you want browser/admin protection

In `echo` mode, each uploaded WAV is queued straight back to `/next-audio`, which makes first deployment and hardware smoke tests simple.

## Firmware setup

The currently tested local hardware flow uses an **ESP32** target. If you are on an ESP32-S3 board, set `ESP_TARGET=esp32s3` instead.

### macOS prerequisites

```bash
brew install cmake ninja dfu-util python3
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd ~/esp/esp-idf
./install.sh esp32
. ./export.sh
```

### Recommended shell setup

```bash
export IDF_PATH="$HOME/esp/esp-idf"
. "$IDF_PATH/export.sh"
export ESP_TARGET="esp32"
export ESPPORT="/dev/cu.usbserial-XXXX"
```

Find the USB serial device on macOS:

```bash
ls /dev/cu.*
```

### Flash and monitor

```bash
cd firmware
idf.py set-target "$ESP_TARGET"
idf.py -p "$ESPPORT" flash monitor
```

Exit the monitor with `Ctrl + ]`.

### If flashing fails

Try:

```bash
idf.py -p "$ESPPORT" -b 115200 flash monitor
```

If `esptool` reports the wrong chip family, switch target first:

```bash
idf.py set-target esp32
```

## Firmware runtime configuration

The firmware supports:

- compile-time defaults via ESP-IDF config
- serial command configuration during HIL/dev workflows

Useful serial commands:

- `status`
- `idle`
- `single`, `double`, `long`
- `health`, `poll`, `replay`, `thread-reset`
- `set bridge <url>`
- `set device <id>`
- `set ssid <name>`
- `set pass <password>`
- `set token <device-auth-token>`
- `wifi connect`

For first-time local deployment:

1. Deploy the bridge in `echo` mode.
2. Flash the firmware.
3. Open the serial monitor.
4. Set the bridge URL and token if needed.
5. Connect Wi-Fi.
6. Trigger `single` / `single` to simulate record-start and record-stop.

## Local development

### Bridge

```bash
cd bridge
npm install
npm run typecheck
npm test
npm run build
npm run dev
```

### Docker image build

```bash
docker build -f bridge/Dockerfile bridge
```

### Firmware

```bash
export ESP_TARGET="${ESP_TARGET:-esp32}"
./scripts/test-all.sh
```

With HIL enabled:

```bash
export ESPPORT="/dev/cu.usbserial-XXXX"
export RUN_HIL=1
./scripts/test-all.sh
```

## Hardware-in-the-loop tests

The non-network serial HIL checks:

- boot/status output
- button/state transitions
- recording start/stop path

Run them manually:

```bash
cd firmware
python3 -m venv .venv-hil
. .venv-hil/bin/activate
pip install -r test/hil/requirements.txt
pytest test/hil --port "$ESPPORT" -k 'not roundtrip'
```

The optional network round-trip test also needs:

- `--bridge-url`
- `--wifi-ssid`
- `--wifi-password`

## GitHub Actions publishing

The workflow is defined in:

```text
.github/workflows/bridge-image.yml
```

On every push to `main`, it:

1. installs bridge dependencies
2. runs bridge typecheck/tests/build
3. builds the bridge Docker image
4. pushes it to GHCR

## Notes

- Keep secrets out of firmware.
- Keep Matrix and ElevenLabs credentials in the bridge only.
- Do not commit `.env`, local serial ports, or Wi-Fi credentials.
