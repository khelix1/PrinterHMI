#!/usr/bin/env bash
set -euo pipefail

usage()
{
    cat <<'USAGE'
Usage:
  ./tools/build_v6_stack.sh
  ./tools/build_v6_stack.sh --bootstrap

Options:
  --bootstrap  Install exact ESP-IDF v6.0.2 when missing.

Environment overrides:
  PRINTERHMI_IDF6_PATH
USAGE
}

bootstrap=false
case "${1:-}" in
    "") ;;
    --bootstrap) bootstrap=true ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
esac

repo_dir="$(git rev-parse --show-toplevel 2>/dev/null)" || {
    echo "ERROR: run this inside the PrinterHMI repository" >&2
    exit 1
}
cd "$repo_dir"

version="$(tr -d '[:space:]' < version.txt)"
source_commit="$(git rev-parse HEAD)"
short_commit="$(git rev-parse --short=8 HEAD)"
idf6="${PRINTERHMI_IDF6_PATH:-$HOME/esp/esp-idf-v6.0.2}"

work_root="$repo_dir/build-v6-stack"
dist_root="$repo_dir/dist"
bundle_name="PrinterHMI-v${version}-full-stack-${short_commit}"
bundle_dir="$dist_root/$bundle_name"
archive="$dist_root/${bundle_name}.tar.gz"

for command_name in awk cmp cp dd find git python3 sha256sum stat tar; do
    command -v "$command_name" >/dev/null 2>&1 || {
        echo "ERROR: required command not found: $command_name" >&2
        exit 1
    }
done

bootstrap_idf()
{
    local destination="$1"
    local tag="$2"
    local targets="$3"

    [[ -f "$destination/export.sh" ]] && return

    if ! $bootstrap; then
        echo "ERROR: $tag is not installed at $destination" >&2
        echo "Run with --bootstrap or set the matching path override." >&2
        exit 1
    fi

    [[ ! -e "$destination" ]] || {
        echo "ERROR: incomplete SDK destination exists: $destination" >&2
        exit 1
    }

    mkdir -p "$(dirname "$destination")"
    git clone --branch "$tag" --depth 1 --recursive \
        https://github.com/espressif/esp-idf.git "$destination"
    "$destination/install.sh" "$targets"
}

validate_idf()
{
    local destination="$1"
    local tag="$2"
    local head
    local tagged

    [[ -f "$destination/export.sh" ]] || {
        echo "ERROR: missing SDK export script: $destination/export.sh" >&2
        exit 1
    }
    [[ -d "$destination/.git" ]] || {
        echo "ERROR: SDK is not a Git checkout: $destination" >&2
        exit 1
    }

    head="$(git -C "$destination" rev-parse HEAD)"
    tagged="$(git -C "$destination" rev-list -n 1 "$tag")"
    [[ "$head" == "$tagged" ]] || {
        echo "ERROR: $destination is not exactly $tag" >&2
        exit 1
    }
}

run_idf()
{
    local sdk="$1"
    local working_directory="$2"
    shift 2

    env -i \
        HOME="$HOME" \
        USER="${USER:-}" \
        SHELL=/bin/bash \
        PATH=/usr/local/bin:/usr/bin:/bin \
        bash --noprofile --norc -c '
            set -euo pipefail
            sdk="$1"
            working_directory="$2"
            shift 2
            source "$sdk/export.sh"
            cd "$working_directory"
            "$@"
        ' _ "$sdk" "$working_directory" "$@"
}

copy_required()
{
    local source="$1"
    local destination="$2"
    [[ -s "$source" ]] || {
        echo "ERROR: missing build output: $source" >&2
        exit 1
    }
    cp "$source" "$destination"
}

bootstrap_idf "$idf6" v6.0.2 esp32p4,esp32c6
validate_idf "$idf6" v6.0.2

rm -rf "$work_root" "$bundle_dir"
rm -f "$archive" "${archive}.sha256"
mkdir -p "$work_root" "$bundle_dir/p4" "$bundle_dir/c6"

echo "===== BUILD NORMAL PRINTERHMI P4 APPLICATION ====="
PRINTERHMI_IDF6_PATH="$idf6" "$repo_dir/tools/build_idf6_hosted3.sh"

p4_build="$repo_dir/build-idf6-hosted3"
hosted_component="$repo_dir/managed_components/espressif__esp_hosted"
[[ -d "$hosted_component" ]] || {
    echo "ERROR: ESP-Hosted 3.0.5 was not resolved" >&2
    exit 1
}

echo "===== BUILD ESP32-C6 ESP-HOSTED 3.0.5 ====="
c6_source="$hosted_component/examples/wifi/sta/cp"
c6_project="$work_root/c6-esp-hosted-3.0.5"
[[ -d "$c6_source" ]] || {
    echo "ERROR: C6 example is missing: $c6_source" >&2
    exit 1
}

cp -a "$c6_source" "$c6_project"
rm -rf "$c6_project/build"
rm -f "$c6_project/sdkconfig" "$c6_project/sdkconfig.old" \
    "$c6_project/dependencies.lock"

cat > "$c6_project/main/idf_component.yml" <<'EOF_C6_MANIFEST'
dependencies:
  espressif/esp_hosted:
    version: "3.0.5"
  idf:
    version: ">=6.0,<6.1"
EOF_C6_MANIFEST

run_idf "$idf6" "$c6_project" idf.py -B build set-target esp32c6
run_idf "$idf6" "$c6_project" idf.py -B build build

c6_build="$c6_project/build"
c6_app="$c6_build/eh_cp_wifi_sta.bin"
c6_full="$bundle_dir/c6/esp-hosted-c6-3.0.5-full-flash.bin"

copy_required "$c6_app" \
    "$bundle_dir/c6/esp-hosted-c6-3.0.5-app.bin"
copy_required "$c6_build/bootloader/bootloader.bin" \
    "$bundle_dir/c6/esp-hosted-c6-3.0.5-bootloader.bin"
copy_required "$c6_build/partition_table/partition-table.bin" \
    "$bundle_dir/c6/esp-hosted-c6-3.0.5-partition-table.bin"
copy_required "$c6_build/ota_data_initial.bin" \
    "$bundle_dir/c6/esp-hosted-c6-3.0.5-ota-data.bin"

run_idf "$idf6" "$c6_project" python -m esptool \
    --chip esp32c6 merge-bin -o "$c6_full" \
    --flash-mode dio --flash-freq 80m --flash-size 4MB \
    0x0 "$c6_build/bootloader/bootloader.bin" \
    0x8000 "$c6_build/partition_table/partition-table.bin" \
    0xd000 "$c6_build/ota_data_initial.bin" \
    0x10000 "$c6_app"

echo "===== PACKAGE NORMAL P4 FIRMWARE ====="
copy_required "$p4_build/PrinterHMI.bin" \
    "$bundle_dir/p4/PrinterHMI-v${version}-ota.bin"
copy_required "$p4_build/bootloader/bootloader.bin" \
    "$bundle_dir/p4/PrinterHMI-bootloader.bin"
copy_required "$p4_build/partition_table/partition-table.bin" \
    "$bundle_dir/p4/PrinterHMI-partition-table.bin"
copy_required "$p4_build/ota_data_initial.bin" \
    "$bundle_dir/p4/PrinterHMI-ota-data.bin"

p4_full="$bundle_dir/p4/PrinterHMI-v${version}-full-flash.bin"
run_idf "$idf6" "$repo_dir" python -m esptool \
    --chip esp32p4 merge-bin -o "$p4_full" \
    --flash-mode dio --flash-freq 80m --flash-size 16MB \
    0x2000 "$p4_build/bootloader/bootloader.bin" \
    0x8000 "$p4_build/partition_table/partition-table.bin" \
    0xf000 "$p4_build/ota_data_initial.bin" \
    0x20000 "$p4_build/PrinterHMI.bin"

cat > "$bundle_dir/FLASHING.txt" <<EOF_FLASH
PrinterHMI v${version} ESP-IDF 6.0.2 firmware stack
Source commit: ${source_commit}

Normal P4 OTA:
  p4/PrinterHMI-v${version}-ota.bin

Normal P4 full flash:
  Chip ESP32-P4, offset 0x0, 16MB image
  p4/PrinterHMI-v${version}-full-flash.bin

C6 direct installation or recovery:
  Chip ESP32-C6, offset 0x0, 4MB image
  c6/esp-hosted-c6-3.0.5-full-flash.bin

The normal P4 application requires the ESP32-C6 to run ESP-Hosted 3.0.5.
Both images are built with the exact supported ESP-IDF v6.0.2 tag.
EOF_FLASH

cat > "$bundle_dir/STACK_MANIFEST.json" <<EOF_MANIFEST
{
  "product": "PrinterHMI",
  "version": "${version}",
  "source_commit": "${source_commit}",
  "esp_idf": "v6.0.2",
  "components": {
    "esp_hosted_p4": "3.0.5",
    "esp_hosted_c6": "3.0.5",
    "esp_wifi_remote": "1.5.3"
  },
  "offsets": {
    "p4_application": "0x20000",
    "c6_application": "0x10000"
  }
}
EOF_MANIFEST

(
    cd "$bundle_dir"
    find . -type f ! -name SHA256SUMS -print0 \
        | sort -z | xargs -0 sha256sum > SHA256SUMS
)

(
    cd "$dist_root"
    tar -czf "$(basename "$archive")" "$(basename "$bundle_dir")"
    sha256sum "$(basename "$archive")" > "$(basename "$archive").sha256"
)

echo "===== COMPLETE STACK ====="
find "$bundle_dir" -maxdepth 2 -type f -printf '%P\n' | sort
ls -lh "$archive" "${archive}.sha256"
cat "${archive}.sha256"
echo "PASS: complete PrinterHMI v${version} stack built and packaged"
