#!/usr/bin/env bash
set -euo pipefail

FIRMWARE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$FIRMWARE_DIR/build-host"
CC="${CC:-cc}"
COMMON_FLAGS=(-std=c11 -Wall -Wextra -Werror -I"$FIRMWARE_DIR/main")

mkdir -p "$BUILD_DIR"

echo "== state machine =="
"$CC" "${COMMON_FLAGS[@]}" \
  "$FIRMWARE_DIR/test/host/test_state_machine.c" \
  "$FIRMWARE_DIR/main/state_machine.c" \
  -o "$BUILD_DIR/test_state_machine"
"$BUILD_DIR/test_state_machine"

echo "== wav encoder =="
"$CC" "${COMMON_FLAGS[@]}" \
  "$FIRMWARE_DIR/test/host/test_wav_encoder.c" \
  "$FIRMWARE_DIR/main/wav_encoder.c" \
  -o "$BUILD_DIR/test_wav_encoder"
"$BUILD_DIR/test_wav_encoder"

echo "== button controller =="
"$CC" "${COMMON_FLAGS[@]}" \
  "$FIRMWARE_DIR/test/host/test_button_controller.c" \
  "$FIRMWARE_DIR/main/button_controller.c" \
  -o "$BUILD_DIR/test_button_controller"
"$BUILD_DIR/test_button_controller"

echo "== app config =="
"$CC" "${COMMON_FLAGS[@]}" \
  "$FIRMWARE_DIR/test/host/test_app_config.c" \
  "$FIRMWARE_DIR/main/app_config.c" \
  -o "$BUILD_DIR/test_app_config"
"$BUILD_DIR/test_app_config"

echo "== bridge url =="
"$CC" "${COMMON_FLAGS[@]}" \
  "$FIRMWARE_DIR/test/host/test_bridge_url.c" \
  "$FIRMWARE_DIR/main/app_config.c" \
  "$FIRMWARE_DIR/main/bridge_url.c" \
  -o "$BUILD_DIR/test_bridge_url"
"$BUILD_DIR/test_bridge_url"

echo "== retry policy =="
"$CC" "${COMMON_FLAGS[@]}" \
  "$FIRMWARE_DIR/test/host/test_retry_policy.c" \
  "$FIRMWARE_DIR/main/retry_policy.c" \
  -o "$BUILD_DIR/test_retry_policy"
"$BUILD_DIR/test_retry_policy"

echo "Host-side firmware tests passed."
