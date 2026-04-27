#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "== Bridge tests =="
cd "$ROOT_DIR/bridge"
npm install
npm run typecheck
npm test
npm run build

echo "== Firmware host tests =="
cd "$ROOT_DIR/firmware"
./test/host/run-host-tests.sh

echo "== Firmware build =="
if [ -z "${IDF_PATH:-}" ]; then
  export IDF_PATH="$HOME/esp/esp-idf"
fi

if [ -z "${ESP_TARGET:-}" ]; then
  export ESP_TARGET="esp32"
fi

if [ ! -f "$IDF_PATH/export.sh" ]; then
  echo "ESP-IDF export script not found at $IDF_PATH/export.sh" >&2
  echo "Install ESP-IDF first or set IDF_PATH before running the full firmware build." >&2
  exit 1
fi

# shellcheck source=/dev/null
. "$IDF_PATH/export.sh"
idf.py set-target "$ESP_TARGET"
idf.py build

if [ "${RUN_HIL:-0}" = "1" ]; then
  : "${ESPPORT:?ESPPORT must be set when RUN_HIL=1}"

  HIL_VENV="${HIL_VENV:-$ROOT_DIR/firmware/.venv-hil}"

  if [ ! -x "$HIL_VENV/bin/pytest" ]; then
    python3 -m venv "$HIL_VENV"
    # shellcheck source=/dev/null
    . "$HIL_VENV/bin/activate"
    pip install -r "$ROOT_DIR/firmware/test/hil/requirements.txt"
  else
    # shellcheck source=/dev/null
    . "$HIL_VENV/bin/activate"
  fi

  echo "== Firmware HIL tests =="
  pytest test/hil --port "$ESPPORT"
fi
