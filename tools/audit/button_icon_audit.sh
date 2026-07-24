#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

python3 - <<'PY'
from pathlib import Path
import re
import sys

root = Path("main")

if not root.is_dir():
    print("ERROR: main/ not found")
    sys.exit(1)

excluded_files = {
    "ui_button.c",
    "ui_popup.c",
    "ui_widgets.c",
    "ui_settings.c",
    "ui_settings_components.c",
}

def extract_call(text, start):
    open_paren = text.find("(", start)

    if open_paren < 0:
        return None, start

    depth = 0
    index = open_paren
    quote = None
    escaped = False
    line_comment = False
    block_comment = False

    while index < len(text):
        char = text[index]
        next_char = (
            text[index + 1]
            if index + 1 < len(text)
            else ""
        )

        if line_comment:
            if char == "\n":
                line_comment = False
            index += 1
            continue

        if block_comment:
            if char == "*" and next_char == "/":
                block_comment = False
                index += 2
                continue

            index += 1
            continue

        if quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None

            index += 1
            continue

        if char == "/" and next_char == "/":
            line_comment = True
            index += 2
            continue

        if char == "/" and next_char == "*":
            block_comment = True
            index += 2
            continue

        if char in ('"', "'"):
            quote = char
            index += 1
            continue

        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1

            if depth == 0:
                return text[start:index + 1], index + 1

        index += 1

    return None, start

groups = {
    "TEXT-ONLY SHARED BUTTONS": [
        "ui_button_create",
    ],
    "ICON SHARED BUTTONS": [
        "ui_button_create_icon",
    ],
    "POPUP ACTIONS": [
        "ui_popup_add_action",
        "ui_popup_add_button_at",
        "ui_popup_add_button_aligned",
    ],
    "LEGACY OPERATOR BUTTONS": [
        "ui_operator_button_create",
    ],
    "RAW LVGL BUTTONS": [
        "lv_button_create",
        "lv_btn_create",
    ],
}

results = {
    title: []
    for title in groups
}

for path in sorted(root.rglob("*.c")):
    if path.name in excluded_files:
        continue

    if "settings" in path.name.lower():
        continue

    text = path.read_text(errors="replace")

    for title, names in groups.items():
        for name in names:
            pattern = re.compile(
                rf"\b{re.escape(name)}\s*\("
            )

            for match in pattern.finditer(text):
                call, _ = extract_call(
                    text,
                    match.start(),
                )

                if not call:
                    continue

                lineno = (
                    text.count("\n", 0, match.start())
                    + 1
                )

                normalized = " ".join(
                    call.split()
                )

                results[title].append(
                    (path, lineno, normalized)
                )

print("============================================================")
print("PRINTERHMI BUTTON ICON AUDIT")
print("Settings intentionally excluded")
print("============================================================")

for title, entries in results.items():
    print()
    print(f"===== {title} ({len(entries)}) =====")

    if not entries:
        print("None")
        continue

    for path, lineno, call in entries:
        print(f"{path}:{lineno}")
        print(f"  {call}")

print()
print("===== OTHER BUTTON-LIKE CONSTRUCTORS =====")

other_pattern = re.compile(
    r"\b([A-Za-z_][A-Za-z0-9_]*"
    r"(?:button|btn)"
    r"[A-Za-z0-9_]*create"
    r"[A-Za-z0-9_]*)\s*\("
)

known = {
    name
    for names in groups.values()
    for name in names
}

other = set()

for path in sorted(root.rglob("*.c")):
    if path.name in excluded_files:
        continue

    if "settings" in path.name.lower():
        continue

    text = path.read_text(errors="replace")

    for match in other_pattern.finditer(text):
        name = match.group(1)

        if name not in known:
            lineno = (
                text.count("\n", 0, match.start())
                + 1
            )

            other.add(
                (str(path), lineno, name)
            )

if not other:
    print("None")
else:
    for path, lineno, name in sorted(other):
        print(f"{path}:{lineno}: {name}")

print()
print("============================================================")
print("PASS: read-only button icon audit complete")
print("============================================================")
PY
