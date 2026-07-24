#!/usr/bin/env python3

from pathlib import Path

changes = {
    Path("main/ui_dashboard_status_v32.c"): [
        ("lv_color_hex(0x3aa8ff)", "UI_ACCENT_INFO", 1),
        ("lv_color_hex(0xb65cff)", "UI_ACCENT_PURPLE", 1),
    ],
    Path("main/main.c"): [
        ("lv_color_hex(0xe6edf5)", "UI_TEXT", 1),
    ],
}

for path, replacements in changes.items():
    if not path.is_file():
        raise RuntimeError(f"missing file: {path}")

    text = path.read_text()

    for old, new, expected in replacements:
        found = text.count(old)

        if found != expected:
            raise RuntimeError(
                f"{path}: expected {expected} occurrence(s) of "
                f"{old}, found {found}"
            )

        text = text.replace(old, new)

    path.write_text(text)

for path, replacements in changes.items():
    text = path.read_text()

    for old, new, expected in replacements:
        if old in text:
            raise RuntimeError(
                f"{path}: obsolete literal remains: {old}"
            )

        found = text.count(new)

        if found < expected:
            raise RuntimeError(
                f"{path}: replacement missing: {new}"
            )

print("PASS: exact theme palette aliases migrated")
print("Changed:")
print("  main/ui_dashboard_status_v32.c")
print("  main/main.c")
