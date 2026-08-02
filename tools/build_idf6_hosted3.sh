#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(git rev-parse --show-toplevel 2>/dev/null)" || {
    echo "ERROR: run this inside the PrinterHMI repository" >&2
    exit 1
}

PRINTERHMI_IDF6_ROOT="${PRINTERHMI_IDF6_PATH:-$HOME/esp/esp-idf-v6.0.2}"

if [[ ! -f "$PRINTERHMI_IDF6_ROOT/export.sh" ]]; then
    echo "ERROR: ESP-IDF 6.0.2 export script not found: $PRINTERHMI_IDF6_ROOT/export.sh" >&2
    exit 1
fi

source "$PRINTERHMI_IDF6_ROOT/export.sh"

cd "$repo_dir"

python3 "$repo_dir/tools/apply_idf6_esp_hosted_sdio_patch.py" \
    "$PRINTERHMI_IDF6_ROOT"

idf_version="$(idf.py --version)"

if [[ "$idf_version" != "ESP-IDF v6.0.2-dirty" ]]; then
    echo "ERROR: expected verified ESP-IDF v6.0.2-dirty, found: $idf_version" >&2
    exit 1
fi

build_dir="$repo_dir/build-idf6-hosted3"
sdkconfig="$repo_dir/sdkconfig.idf6"

idf.py \
    -B "$build_dir" \
    -D SDKCONFIG="$sdkconfig" \
    reconfigure

python3 "$repo_dir/tools/apply_esp_hosted_3_0_5_rssi_patch.py"

idf.py \
    -B "$build_dir" \
    -D SDKCONFIG="$sdkconfig" \
    build
