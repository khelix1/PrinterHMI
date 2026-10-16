#!/usr/bin/env bash
set -euo pipefail

# Keep optional flashing/monitoring on the exact configuration just built.
for action in "$@"; do
    case "$action" in
        flash|app-flash|monitor) ;;
        *) echo "ERROR: optional actions are flash, app-flash and monitor" >&2; exit 1 ;;
    esac
done

repo_dir="$(git rev-parse --show-toplevel 2>/dev/null)" || {
    echo "ERROR: run this inside the PrinterHMI repository" >&2
    exit 1
}

PRINTERHMI_IDF6_ROOT="${PRINTERHMI_IDF6_PATH:-${IDF_PATH:-$HOME/esp/esp-idf-v6.0.2}}"

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

# Verified fill-only PPA is the default; 0 remains the software fallback.
# Retain the existing environment/config names for command compatibility.
case "${PRINTERHMI_PPA_TRIAL:-1}" in
    0) echo "Renderer: software (explicit fallback)" ;;
    1)
        echo "Renderer: PPA fills (default; images/rotation disabled)"
        python3 "$repo_dir/tools/prepare_ppa_trial.py" \
            "$sdkconfig" "$build_dir/sdkconfig.ppa-trial"
        sdkconfig="$build_dir/sdkconfig.ppa-trial"
        ;;
    *) echo "ERROR: PRINTERHMI_PPA_TRIAL must be 0 or 1" >&2; exit 1 ;;
esac

idf.py \
    -B "$build_dir" \
    -D SDKCONFIG="$sdkconfig" \
    reconfigure

if [[ "${PRINTERHMI_PPA_TRIAL:-1}" == "1" ]]; then
    python3 "$repo_dir/tools/prepare_ppa_trial.py" --verify "$sdkconfig"
    python3 "$repo_dir/tools/apply_lvgl95_ppa_alignment.py"
    python3 "$repo_dir/tools/apply_lvgl95_ppa_cache.py"
fi

python3 "$repo_dir/tools/apply_esp_hosted_3_0_5_rssi_patch.py"

idf.py \
    -B "$build_dir" \
    -D SDKCONFIG="$sdkconfig" \
    build "$@"
