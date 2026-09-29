#!/usr/bin/env bash
# run_jam.sh — focused interference
set -euo pipefail
cd "$(dirname "$0")/.."

MODE="${1:-ble}"
BACKEND="${2:-hackrf}"
WAVE="${3:-noise}"

case "$MODE" in
  ble)
    ./build/bluetooth_signal_tool jam ble --backend "$BACKEND" --mode "$WAVE"
    ;;
  classic)
    ./build/bluetooth_signal_tool jam classic --backend "$BACKEND" --mode "$WAVE"
    ;;
  *)
    echo "usage: $0 [ble|classic] [backend] [noise|tone|chirp]"
    exit 1
    ;;
esac
