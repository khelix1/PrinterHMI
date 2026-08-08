#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

fail()
{
    echo "FAIL: $*" >&2
    exit 1
}

require_file()
{
    local path="$1"
    [ -f "$path" ] || fail "missing $path"
}

require_source_once()
{
    local source="$1"
    local count

    count="$(grep -F -c "\"$source\"" main/CMakeLists.txt || true)"
    [ "$count" -eq 1 ] ||
        fail "$source must appear exactly once in main/CMakeLists.txt"
}

require_reference()
{
    local path="$1"
    local pattern="$2"

    rg -q "$pattern" "$path" ||
        fail "$path does not reference $pattern"
}

sources=(
    ui_calibration_motion.c
    ui_calibration_manual_probe.c
    ui_calibration_pressure_advance.c
    ui_bed_mesh_gestures.c
    ui_bed_mesh_renderer.c
    ui_bed_mesh_profiles.c
    ui_devices_live_values.c
    ui_devices_catalog_view.c
    console_controller.c
    macro_controller.c
    ui_console.c
    ui_macros.c
)

for source in "${sources[@]}"; do
    require_file "main/$source"
    require_source_once "$source"
done

require_file main/ui_bed_mesh_view.h

require_reference \
    main/ui_calibration.c \
    'ui_calibration_motion'
require_reference \
    main/ui_calibration.c \
    'ui_calibration_manual_probe'
require_reference \
    main/ui_calibration.c \
    'ui_calibration_pressure_advance'

require_reference \
    main/ui_bed_mesh.c \
    'ui_bed_mesh_gestures'
require_reference \
    main/ui_bed_mesh.c \
    'ui_bed_mesh_renderer'
require_reference \
    main/ui_bed_mesh.c \
    'ui_bed_mesh_profiles'

require_reference \
    main/ui_devices.c \
    'ui_devices_catalog_view'
require_reference \
    main/ui_devices_catalog_view.c \
    'ui_devices_live_values'

require_reference \
    main/ui_console.c \
    'console_controller'
require_reference \
    main/ui_macros.c \
    'macro_controller'

if rg -q \
    'rasterize_smooth_triangle|pinch_active|save_as_profile_cb' \
    main/ui_bed_mesh.c; then
    fail "Bed Mesh page still contains extracted implementation details"
fi

if rg -q \
    'format_known_live_value|devices_refresh_timer_cb' \
    main/ui_devices.c; then
    fail "Devices page still contains extracted implementation details"
fi

git diff --check

echo "PASS: v5 feature architecture ownership and build membership"
