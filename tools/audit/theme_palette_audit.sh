#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

python3 - <<'PY'
from pathlib import Path
from collections import defaultdict
import re
import sys

root = Path("main")

if not root.is_dir():
    print("ERROR: main/ not found")
    sys.exit(1)

source_files = sorted(
    list(root.rglob("*.c")) +
    list(root.rglob("*.h"))
)

theme_files = [
    root / "ui_theme.h",
    root / "ui_theme.c",
    root / "ui_theme_a.h",
    root / "ui_theme_a.c",
    root / "ui_theme_b.h",
    root / "ui_theme_b.c",
]

color_re = re.compile(
    r"lv_color_hex\s*\(\s*(0x[0-9A-Fa-f]+)\s*\)"
)

uses = defaultdict(list)

for path in source_files:
    if path.name.startswith("ui_theme"):
        continue

    lines = path.read_text(errors="replace").splitlines()

    for lineno, line in enumerate(lines, 1):
        for match in color_re.finditer(line):
            uses[match.group(1).upper()].append(
                (path, lineno, lines)
            )

print("============================================================")
print("PRINTERHMI THEME PALETTE AUDIT")
print("============================================================")

print()
print("===== CURRENT THEME DEFINITIONS =====")

for path in theme_files:
    if not path.exists():
        continue

    print()
    print(f"----- {path} -----")

    for lineno, line in enumerate(
        path.read_text(errors="replace").splitlines(),
        1
    ):
        if (
            "#define UI_" in line
            or "lv_color_hex" in line
            or "lv_color_t" in line
            or "ui_theme_" in line
        ):
            print(f"{lineno:5d}: {line}")

print()
print("===== HARDCODED COLOR SUMMARY =====")

if not uses:
    print("None")
else:
    for color, locations in sorted(
        uses.items(),
        key=lambda item: (-len(item[1]), item[0])
    ):
        print(f"{color}: {len(locations)} occurrence(s)")

print()
print("===== HARDCODED COLOR CONTEXT =====")

for color, locations in sorted(uses.items()):
    print()
    print(f"----- {color} -----")

    for path, lineno, lines in locations:
        start = max(1, lineno - 3)
        end = min(len(lines), lineno + 3)

        print()
        print(f"{path}:{lineno}")

        for current in range(start, end + 1):
            marker = ">" if current == lineno else " "
            print(f"{marker}{current:5d}: {lines[current - 1]}")

print()
print("============================================================")
print("PASS: read-only theme palette audit complete")
print("============================================================")
PY
