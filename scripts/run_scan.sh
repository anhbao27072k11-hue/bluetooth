#!/usr/bin/env bash
# run_scan.sh — scan BLE or Classic
set -euo pipefail
cd "$(dirname "$0")/.."

MODE="${1:-ble}"
BACKEND="${2:-hackrf}"
GAIN="${GAIN:-40}"

case "$MODE" in
  ble)
    ./build/bluetooth_signal_tool scan ble --backend "$BACKEND" --gain "$GAIN"
    ;;
  classic)
    ./build/bluetooth_signal_tool scan classic --backend "$BACKEND" --gain "$GAIN"
    ;;
  *)
    echo "usage: $0 [ble|classic] [backend]"
    exit 1
    ;;
esac
