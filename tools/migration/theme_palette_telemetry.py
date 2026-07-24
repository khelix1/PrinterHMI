#!/usr/bin/env python3

from pathlib import Path

theme = Path("main/ui_theme.h")

if not theme.is_file():
    raise RuntimeError("missing main/ui_theme.h")

text = theme.read_text()

anchor_replacement = """#define UI_ACCENT_PURPLE        lv_color_hex(0xB65CFF)

/* Telemetry surfaces and data series */
#define UI_TELEMETRY_ROOT_BG       lv_color_hex(0x040D17)
#define UI_TELEMETRY_CHART_BG      lv_color_hex(0x040C15)
#define UI_TELEMETRY_CHART_CARD_BG lv_color_hex(0x06111D)
#define UI_TELEMETRY_PANEL_BG      lv_color_hex(0x081522)
#define UI_TELEMETRY_CARD_BG       lv_color_hex(0x091827)
#define UI_TELEMETRY_GRID          lv_color_hex(0x173047)
#define UI_TELEMETRY_CHAMBER       lv_color_hex(0x35E0D0)
#define UI_TELEMETRY_BED_TRACE     lv_color_hex(0xFFCF66)
#define UI_TELEMETRY_HUMIDITY      lv_color_hex(0xA679FF)"""

anchor = "#define UI_ACCENT_PURPLE        lv_color_hex(0xB65CFF)"

if "UI_TELEMETRY_ROOT_BG" in text:
    raise RuntimeError("telemetry palette already exists")

if text.count(anchor) != 1:
    raise RuntimeError(
        f"expected one UI_ACCENT_PURPLE anchor, found {text.count(anchor)}"
    )

text = text.replace(anchor, anchor_replacement)
theme.write_text(text)

changes = {
    Path("main/ui_telemetry_charts.c"): [
        ("lv_color_hex(0x040C15)", "UI_TELEMETRY_CHART_BG", 1),
        ("lv_color_hex(0x06111D)", "UI_TELEMETRY_CHART_CARD_BG", 1),
        ("lv_color_hex(0x081522)", "UI_TELEMETRY_PANEL_BG", 1),
        ("lv_color_hex(0x173047)", "UI_TELEMETRY_GRID", 1),
        ("lv_color_hex(0x35E0D0)", "UI_TELEMETRY_CHAMBER", 2),
        ("lv_color_hex(0xFFCF66)", "UI_TELEMETRY_BED_TRACE", 1),
        ("lv_color_hex(0xA679FF)", "UI_TELEMETRY_HUMIDITY", 2),
    ],
    Path("main/ui_telemetry_components.c"): [
        ("lv_color_hex(0x091827)", "UI_TELEMETRY_CARD_BG", 1),
    ],
    Path("main/ui_telemetry_v32.c"): [
        ("lv_color_hex(0x040D17)", "UI_TELEMETRY_ROOT_BG", 1),
        ("lv_color_hex(0x35E0D0)", "UI_TELEMETRY_CHAMBER", 1),
        ("lv_color_hex(0xA679FF)", "UI_TELEMETRY_HUMIDITY", 1),
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
                f"{path}: obsolete telemetry literal remains: {old}"
            )

        if text.count(new) < expected:
            raise RuntimeError(
                f"{path}: replacement missing: {new}"
            )

print("PASS: telemetry palette centralized")
print("Changed:")
print("  main/ui_theme.h")
print("  main/ui_telemetry_charts.c")
print("  main/ui_telemetry_components.c")
print("  main/ui_telemetry_v32.c")
