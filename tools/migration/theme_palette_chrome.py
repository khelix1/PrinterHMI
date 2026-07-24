#!/usr/bin/env python3

from pathlib import Path

theme = Path("main/ui_theme.h")

if not theme.is_file():
    raise RuntimeError("missing main/ui_theme.h")

text = theme.read_text()

insertions = [
    (
        "#define UI_CONTROL_CANCEL       lv_color_hex(0x39465C)",
        """#define UI_CONTROL_CANCEL       lv_color_hex(0x39465C)
#define UI_PROGRESS_TRACK       lv_color_hex(0x182230)""",
        "UI_PROGRESS_TRACK",
    ),
    (
        "#define UI_BORDER_BRIGHT        lv_color_hex(0x8FD3FF)",
        """#define UI_BORDER_BRIGHT        lv_color_hex(0x8FD3FF)
#define UI_WIFI_INACTIVE        lv_color_hex(0x3A4654)""",
        "UI_WIFI_INACTIVE",
    ),
]

for anchor, replacement, token in insertions:
    if token in text:
        raise RuntimeError(f"{token} already exists")

    found = text.count(anchor)

    if found != 1:
        raise RuntimeError(
            f"expected one anchor for {token}, found {found}"
        )

    text = text.replace(anchor, replacement)

theme.write_text(text)

changes = {
    Path("main/ui_shell.c"): [
        ("lv_color_hex(0x3a4654)", "UI_WIFI_INACTIVE", 2),
    ],
    Path("main/ui_splash_v32.c"): [
        ("lv_color_hex(0x182230)", "UI_PROGRESS_TRACK", 1),
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
                f"{path}: expected {expected} occurrence(s) "
                f"of {old}, found {found}"
            )

        text = text.replace(old, new)

    path.write_text(text)

for path, replacements in changes.items():
    text = path.read_text()

    for old, new, expected in replacements:
        if old in text:
            raise RuntimeError(
                f"{path}: obsolete color remains: {old}"
            )

        if text.count(new) < expected:
            raise RuntimeError(
                f"{path}: replacement missing: {new}"
            )

print("PASS: shared chrome palette centralized")
print("Changed:")
print("  main/ui_theme.h")
print("  main/ui_shell.c")
print("  main/ui_splash_v32.c")
