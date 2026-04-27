#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ -z "${IDF_PATH:-}" ]; then
  export IDF_PATH="$HOME/esp/esp-idf"
fi

if [ -z "${ESP_TARGET:-}" ]; then
  export ESP_TARGET="esp32"
fi

if [ ! -f "$IDF_PATH/export.sh" ]; then
  echo "ESP-IDF export script not found at $IDF_PATH/export.sh" >&2
  echo "Install ESP-IDF first, then rerun this script." >&2
  exit 1
fi

# shellcheck source=/dev/null
. "$IDF_PATH/export.sh"

if [ -z "${ESPPORT:-}" ]; then
  echo "ESPPORT is not set."
  echo "Available macOS serial ports:"
  ls /dev/cu.* || true
  echo
  echo "Set it, for example:"
  echo 'export ESPPORT="/dev/cu.usbmodem1101"'
  exit 1
fi

cd "$ROOT_DIR/firmware"
idf.py set-target "$ESP_TARGET"
idf.py -p "$ESPPORT" flash monitor
