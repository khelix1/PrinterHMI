#!/usr/bin/env bash
set -euo pipefail

firmware="${1:-build-idf6-hosted3/PrinterHMI.bin}"
max_bytes=$((3 * 1024 * 1024))

[[ -s "$firmware" ]] || {
    echo "ERROR: firmware image not found: $firmware" >&2
    exit 1
}

size_bytes="$(stat -c '%s' "$firmware")"
printf 'Firmware size: %d bytes (budget: %d bytes)\n' "$size_bytes" "$max_bytes"

if (( size_bytes > max_bytes )); then
    echo "ERROR: firmware exceeds the CI size budget" >&2
    exit 1
fi

echo "PASS: firmware is within the CI size budget"
