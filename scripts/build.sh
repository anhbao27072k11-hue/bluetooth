#!/usr/bin/env bash
# build.sh — configure + build the tool
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-build}"
BACKEND="${BACKEND:-hackrf}"

cmake -S . -B "$BUILD_DIR" -DUSE_HACKRF=ON -DUSE_UHD=OFF -DUSE_BLADERF=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "Built: $BUILD_DIR/bluetooth_signal_tool"
