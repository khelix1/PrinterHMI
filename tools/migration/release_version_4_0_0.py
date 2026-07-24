#!/usr/bin/env python3
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
CMAKE = ROOT / "CMakeLists.txt"
VERSION_FILE = ROOT / "version.txt"
MAIN_DIR = ROOT / "main"
WS_SOURCE = MAIN_DIR / "moonraker_live_websocket.c"

VERSION = "4.0.0"
DISPLAY_VERSION = "v4.0.0"
MARKER = "PRINTERHMI_RELEASE_VERSION_4_0_0"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


def replace_optional_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count > 1:
        raise RuntimeError(f"expected at most one {label}, found {count}")
    return text.replace(old, new, 1) if count == 1 else text


if not CMAKE.exists() or not MAIN_DIR.is_dir():
    raise RuntimeError("run this migration from the PrinterHMI repository")

cmake = CMAKE.read_text()

if MARKER in cmake:
    print("PASS: PrinterHMI v4.0.0 release identity already installed")
    raise SystemExit(0)


# -------------------------------------------------------------------------
# Stable project identity plus one ESP-IDF firmware-version source of truth.
# -------------------------------------------------------------------------

project_pattern = re.compile(
    r"(?m)^project\(PrinterHMI(?:_v[0-9_]+)?\)\s*$"
)

project_matches = project_pattern.findall(cmake)
if len(project_matches) != 1:
    raise RuntimeError(
        f"expected one PrinterHMI project() declaration, found "
        f"{len(project_matches)}"
    )

cmake = project_pattern.sub(
    f"# {MARKER}\nproject(PrinterHMI)",
    cmake,
    count=1,
)

# If an older explicit override exists, keep it synchronized. Normally
# version.txt is the sole owner and ESP-IDF embeds it in esp_app_desc_t.
project_ver_pattern = re.compile(
    r'(?m)^set\(PROJECT_VER\s+"?(?:v?3\.2(?:\.0)?|v?4\.0\.0)"?\)\s*$'
)

if project_ver_pattern.search(cmake):
    cmake = project_ver_pattern.sub(
        f'set(PROJECT_VER "{VERSION}")',
        cmake,
        count=1,
    )

cmake = cmake.replace(
    "# ESP-IDF project file for JC1060P470 Drybox HMI v3.0 OTA baseline",
    "# ESP-IDF project file for PrinterHMI v4.0.0",
)

if VERSION_FILE.exists():
    existing_version = VERSION_FILE.read_text().strip()
    accepted = {"3.2", "v3.2", "3.2.0", "v3.2.0", VERSION, DISPLAY_VERSION}
    if existing_version not in accepted:
        raise RuntimeError(
            f"refusing to replace unexpected version.txt value: "
            f"{existing_version!r}"
        )

VERSION_FILE.write_text(VERSION + "\n")


# -------------------------------------------------------------------------
# WebSocket identify now reports the running OTA image version directly.
# -------------------------------------------------------------------------

if WS_SOURCE.exists():
    websocket = WS_SOURCE.read_text()

    hardcoded_versions = (
        '        "\\\"version\\\":\\\"3.2\\\","\n',
        '        "\\\"version\\\":\\\"3.2.0\\\","\n',
        '        "\\\"version\\\":\\\"4.0.0\\\","\n',
    )

    hardcoded = [value for value in hardcoded_versions if value in websocket]

    if hardcoded:
        if len(hardcoded) != 1:
            raise RuntimeError("ambiguous WebSocket client version literals")

        websocket = replace_once(
            websocket,
            '#include "esp_log.h"\n',
            '#include "esp_log.h"\n#include "esp_app_desc.h"\n',
            "WebSocket app-description include",
        )

        websocket = replace_once(
            websocket,
            '''static bool send_identify(void)
{
    if (!s_client || !s_connected) return false;

    char request[512];
''',
            '''static bool send_identify(void)
{
    if (!s_client || !s_connected) return false;

    const esp_app_desc_t *app = esp_app_get_description();
    const char *app_version =
        app && app->version[0] ? app->version : "4.0.0";

    char request[512];
''',
            "WebSocket identify version owner",
        )

        websocket = websocket.replace(
            hardcoded[0],
            '        "\\\"version\\\":\\\"%s\\\","\n',
            1,
        )

        websocket = replace_once(
            websocket,
            '''        "\\\"id\\\":1000}",
        s_api_key);
''',
            '''        "\\\"id\\\":1000}",
        app_version,
        s_api_key);
''',
            "WebSocket identify arguments",
        )

        WS_SOURCE.write_text(websocket)


# -------------------------------------------------------------------------
# Active source and release documentation: public version literals only.
# Internal ui_*_v32 module names remain stable compatibility identifiers.
# -------------------------------------------------------------------------

text_suffixes = {".c", ".h", ".cpp", ".hpp", ".md", ".txt"}
release_files = []

for path in MAIN_DIR.rglob("*"):
    if path.is_file() and path.suffix.lower() in text_suffixes:
        release_files.append(path)

for name in ("README.md", "CHANGELOG.md", "RELEASE.md"):
    path = ROOT / name
    if path.exists():
        release_files.append(path)

for path in sorted(set(release_files)):
    text = path.read_text(errors="strict")
    updated = text

    # Most-specific forms first.
    updated = updated.replace("v3.2.0", DISPLAY_VERSION)
    updated = updated.replace("v3.2", DISPLAY_VERSION)
    updated = re.sub(r"(?<![0-9.])3\.2\.0(?![0-9.])", VERSION, updated)
    updated = updated.replace("PrinterHMI_v3_2", "PrinterHMI")
    updated = updated.replace("Operator Baseline", "Multi-Printer Release")

    # Legacy runtime log identity was never the product version owner.
    updated = updated.replace('"DryboxHMI_v3_1_WIFI"', '"PrinterHMI"')
    updated = updated.replace(
        "Drybox HMI v3.1 WiFi Merge Baseline",
        "PrinterHMI v4.0.0 Multi-Printer Release",
    )

    if updated != text:
        path.write_text(updated)


CMAKE.write_text(cmake)


# -------------------------------------------------------------------------
# Verification: reject stale public identities, but intentionally allow v32
# API/module symbols because they describe interface lineage, not firmware.
# -------------------------------------------------------------------------

stale_patterns = {
    "display version": re.compile(r"v3\.2(?:\.0)?"),
    "semantic version": re.compile(r"(?<![0-9.])3\.2\.0(?![0-9.])"),
    "old project target": re.compile(r"PrinterHMI_v3_2"),
    "old runtime tag": re.compile(r"DryboxHMI_v3_1_WIFI"),
}

stale = []
verification_files = [CMAKE, VERSION_FILE] + sorted(set(release_files))

for path in verification_files:
    text = path.read_text(errors="strict")
    for label, pattern in stale_patterns.items():
        if pattern.search(text):
            stale.append(f"{path.relative_to(ROOT)}: {label}")

if stale:
    raise RuntimeError(
        "stale public version references remain:\n  " + "\n  ".join(stale)
    )

if VERSION_FILE.read_text().strip() != VERSION:
    raise RuntimeError("version.txt verification failed")

if "project(PrinterHMI)" not in CMAKE.read_text():
    raise RuntimeError("stable CMake project identity verification failed")

print("PASS: PrinterHMI v4.0.0 release identity installed")
print("  - version.txt is the canonical ESP-IDF image version")
print("  - project/binary identity is now stable: PrinterHMI")
print("  - Settings reads 4.0.0 from the running esp_app_desc_t image")
print("  - WebSocket identify reads the same embedded image version")
print("  - active UI, logs and release documentation use v4.0.0")
print("  - internal v32 modules/APIs were intentionally preserved")
print("Next: idf.py fullclean && idf.py build")

