#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

python3 - <<'PY'
from pathlib import Path
import re
import sys
from collections import Counter

root = Path("main")

if not root.is_dir():
    print("ERROR: main/ not found")
    sys.exit(1)

files = sorted(root.rglob("*.c"))

style_re = re.compile(r"\blv_obj_set_style_([A-Za-z0-9_]+)\s*\(")

shared_theme = {
    "ui_theme.c",
    "ui_theme_a.c",
    "ui_theme_b.c",
}

shared_components = shared_theme | {
    "ui_widgets.c",
    "ui_button.c",
    "ui_popup.c",
    "ui_settings_components.c",
}

def matching_lines(pattern, excluded=None):
    regex = re.compile(pattern)
    excluded = excluded or set()

    results = []

    for path in files:
        if path.name in excluded:
            continue

        try:
            lines = path.read_text(errors="replace").splitlines()
        except OSError:
            continue

        for lineno, line in enumerate(lines, 1):
            if regex.search(line):
                results.append((path, lineno, line.rstrip()))

    return results

def print_matches(title, pattern, excluded=None):
    print()
    print(f"===== {title} =====")

    results = matching_lines(pattern, excluded)

    if not results:
        print("None")
        return

    for path, lineno, line in results:
        print(f"{path}:{lineno}:{line}")

print("============================================================")
print("PRINTERHMI THEME STYLE OWNERSHIP AUDIT")
print("============================================================")

file_counts = Counter()
property_counts = Counter()

for path in files:
    try:
        text = path.read_text(errors="replace")
    except OSError:
        continue

    matches = list(style_re.finditer(text))

    if matches:
        file_counts[str(path)] = len(matches)

    for match in matches:
        property_counts["lv_obj_set_style_" + match.group(1)] += 1

print()
print("===== DIRECT STYLE CALLS BY FILE =====")

if file_counts:
    for path, count in sorted(
        file_counts.items(),
        key=lambda item: (-item[1], item[0])
    ):
        print(f"{path}:{count}")
else:
    print("None")

print()
print("===== DIRECT STYLE CALLS BY PROPERTY =====")

if property_counts:
    for name, count in sorted(
        property_counts.items(),
        key=lambda item: (-item[1], item[0])
    ):
        print(f"{count:4d} {name}")
else:
    print("None")

print_matches(
    "PAGE / FEATURE MODULE STYLE CALLS",
    r"\blv_obj_set_style_[A-Za-z0-9_]+\s*\(",
    shared_components,
)

print_matches(
    "HARDCODED COLORS OUTSIDE THEME INFRASTRUCTURE",
    r"\b(?:lv_color_hex|lv_palette_|lv_color_make)\b",
    shared_theme,
)

print_matches(
    "RAW RADII OUTSIDE SHARED COMPONENTS",
    r"\blv_obj_set_style_radius\s*\(",
    shared_components,
)

print_matches(
    "RAW BACKGROUND / BORDER DECISIONS",
    r"\blv_obj_set_style_(?:bg_color|bg_opa|border_color|border_width)\s*\(",
    shared_components,
)

print_matches(
    "RAW TYPOGRAPHY DECISIONS",
    r"\blv_obj_set_style_text_(?:font|color|align|letter_space|line_space)\s*\(",
    shared_components,
)

print_matches(
    "RAW BUTTON CREATION OUTSIDE SHARED BUTTON SYSTEM",
    r"\blv_(?:button|btn)_create\s*\(",
    {"ui_button.c", "ui_popup.c"},
)

print_matches(
    "RAW SCREEN-LEVEL OBJECT CREATION",
    r"\blv_obj_create\s*\(\s*lv_screen_active\s*\(\s*\)\s*\)",
)

print()
print("============================================================")
print("PASS: read-only theme style ownership audit complete")
print("============================================================")
PY
