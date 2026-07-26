#!/usr/bin/env bash
set -euo pipefail

# PRINTERHMI_VERSION_AUDIT_V1
repo_dir="$(git rev-parse --show-toplevel 2>/dev/null)" || {
    echo "ERROR: run this inside the PrinterHMI Git repository" >&2
    exit 1
}

cd "$repo_dir"

for command in rg grep; do
    command -v "$command" >/dev/null 2>&1 || {
        echo "ERROR: required command not found: $command" >&2
        exit 1
    }
done

version="$(tr -d '[:space:]' < version.txt)"

if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "ERROR: version.txt is not a semantic X.Y.Z version: $version" >&2
    exit 1
fi

require_text()
{
    local file="$1"
    local text="$2"

    grep -Fq -- "$text" "$file" || {
        echo "ERROR: $file is missing current version text:" >&2
        echo "  $text" >&2
        exit 1
    }
}

require_text CMakeLists.txt \
    '"${CMAKE_CURRENT_LIST_DIR}/version.txt"'
require_text CMakeLists.txt \
    "PRINTERHMI_VERSION"
require_text CMakeLists.txt \
    'set(PROJECT_VER "${PRINTERHMI_VERSION}")'
require_text CMakeLists.txt \
    "CMAKE_CONFIGURE_DEPENDS"

if rg -n \
    '"v?[0-9]+\.[0-9]+\.[0-9]+"' \
    main \
    --glob '*.c' \
    --glob '*.h'; then
    echo "ERROR: hard-coded semantic version remains in active firmware source" >&2
    echo "Runtime version text must come from esp_app_get_description()." >&2
    exit 1
fi

require_text main/ui_splash_v32.c "esp_app_get_description()"
require_text main/ui_dashboard_v32.c "esp_app_get_description()"
require_text main/ui_settings.c "esp_app_get_description()"
require_text main/moonraker_live_websocket.c "esp_app_get_description()"

require_text README.md "The v${version} firmware targets"
require_text README.md "- Firmware version: \`${version}\`"
require_text docs/README.md "current v${version} behavior"
require_text docs/ARCHITECTURE.md "PrinterHMI v${version}"
require_text docs/BUILDING.md "layout for v${version}"
require_text docs/HARDWARE.md "PrinterHMI v${version}"
require_text docs/PROJECT_FILE_CATALOG.md "v${version} source list"
require_text docs/history/README.md "current v${version} product"
require_text CHANGELOG.md "## [${version}]"

release_notes="docs/releases/v${version}.md"
[[ -s "$release_notes" ]] || {
    echo "ERROR: release notes are missing: $release_notes" >&2
    exit 1
}

if [[ -f tools/release_stable.sh ]]; then
    require_text tools/release_stable.sh "version=\"${version}\""
fi

echo "PASS: version.txt ${version} owns the build and runtime identity"
echo "PASS: current documentation and release tooling match ${version}"
