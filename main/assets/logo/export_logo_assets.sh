#!/usr/bin/env sh
set -eu

asset_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

command -v inkscape >/dev/null 2>&1 || {
    echo "inkscape is required to export PrinterHMI logo assets" >&2
    exit 1
}

inkscape -T -o "$asset_dir/printerhmi-logo-dark-outlined.svg" \
    "$asset_dir/printerhmi-logo-dark.svg"
inkscape -T -o "$asset_dir/printerhmi-logo-light-outlined.svg" \
    "$asset_dir/printerhmi-logo-light.svg"
inkscape -T -o "$asset_dir/printerhmi-mark-outlined.svg" \
    "$asset_dir/printerhmi-mark.svg"

inkscape --export-background-opacity=0 --export-width=720 \
    -o "$asset_dir/printerhmi-logo-dark.png" \
    "$asset_dir/printerhmi-logo-dark.svg"
inkscape --export-background-opacity=0 --export-width=720 \
    -o "$asset_dir/printerhmi-logo-light.png" \
    "$asset_dir/printerhmi-logo-light.svg"
inkscape --export-background-opacity=0 --export-width=256 \
    -o "$asset_dir/printerhmi-mark.png" \
    "$asset_dir/printerhmi-mark.svg"
inkscape --export-background-opacity=0 --export-width=520 \
    -o "$asset_dir/printerhmi-splash.png" \
    "$asset_dir/printerhmi-logo-dark.svg"

od -An -v -t x1 "$asset_dir/printerhmi-splash.png" | awk '
    BEGIN {
        print "static const unsigned char printerhmi_splash_png[] = {"
        count = 0
    }
    {
        for (field = 1; field <= NF; ++field) {
            if (count % 12 == 0) printf "    "
            printf "0x%s", $field
            ++count
            if (count % 12 == 0) printf ",\n"
            else printf ", "
        }
    }
    END {
        if (count % 12 != 0) printf "\n"
        print "};"
        printf "static const unsigned int printerhmi_splash_png_len = %d;\n", count
    }
' > "$asset_dir/printerhmi-splash.inc"

echo "PrinterHMI logo assets exported in $asset_dir"
