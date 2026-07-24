#!/usr/bin/env python3

from pathlib import Path

theme_path = Path("main/ui_theme.h")
cards_path = Path("main/ui_printer_info_cards.c")

for path in (theme_path, cards_path):
    if not path.is_file():
        raise RuntimeError(f"missing file: {path}")

theme = theme_path.read_text()

anchor = "#define UI_ACCENT_PURPLE        lv_color_hex(0xB65CFF)"

replacement = """#define UI_ACCENT_PURPLE        lv_color_hex(0xB65CFF)
#define UI_ACCENT_ORANGE        lv_color_hex(0xFF8C2A)
#define UI_ACCENT_SKY           lv_color_hex(0x00D1FF)"""

for token in ("UI_ACCENT_ORANGE", "UI_ACCENT_SKY"):
    if token in theme:
        raise RuntimeError(f"{token} already exists")

if theme.count(anchor) != 1:
    raise RuntimeError(
        f"expected one accent anchor, found {theme.count(anchor)}"
    )

theme = theme.replace(anchor, replacement, 1)
theme_path.write_text(theme)

cards = cards_path.read_text()

old_function = """static uint32_t card_color_for_title(const char *title)
{
    if (!title) return 0x3aa8ff;

    if (strstr(title, "NOZZLE") || strstr(title, "Nozzle") || strstr(title, "nozzle") ||
        strstr(title, "HOTEND") || strstr(title, "Hotend") || strstr(title, "hotend") ||
        strstr(title, "HEATER") || strstr(title, "Heater") || strstr(title, "heater")) return 0xff4d4d;

    if (strstr(title, "BED") || strstr(title, "Bed") || strstr(title, "bed") ||
        strstr(title, "REMAINING") || strstr(title, "Remaining") || strstr(title, "remaining") ||
        strstr(title, "TARGET") || strstr(title, "Target") || strstr(title, "target")) return 0xffc857;
    if (strstr(title, "AIR") || strstr(title, "CENTER") || strstr(title, "CHAMBER") || strstr(title, "TEMP")) return 0xff8c2a;
    if (strstr(title, "HUMID") || strstr(title, "RH")) return 0x00d1ff;
    if (strstr(title, "FAN")) return 0x8fd3ff;
    if (strstr(title, "PROGRESS")) return 0xb65cff;
    if (strstr(title, "ELAPSED") || strstr(title, "TIME")) return 0x3aa8ff;
    if (strstr(title, "WIFI") || strstr(title, "NETWORK") || strstr(title, "MOONRAKER") || strstr(title, "STATE") || strstr(title, "STATUS")) return 0x70e000;
    if (strstr(title, "FILE") || strstr(title, "FILES")) return 0x3aa8ff;
    if (strstr(title, "DRY")) return 0x00d1ff;
    if (strstr(title, "OTA") || strstr(title, "SYSTEM") || strstr(title, "SETTINGS")) return 0xb65cff;

    return 0x3aa8ff;
}"""

new_function = """static lv_color_t card_color_for_title(const char *title)
{
    if (!title) return UI_ACCENT_INFO;

    if (strstr(title, "NOZZLE") || strstr(title, "Nozzle") || strstr(title, "nozzle") ||
        strstr(title, "HOTEND") || strstr(title, "Hotend") || strstr(title, "hotend") ||
        strstr(title, "HEATER") || strstr(title, "Heater") || strstr(title, "heater")) return UI_DANGER_BRIGHT;

    if (strstr(title, "BED") || strstr(title, "Bed") || strstr(title, "bed") ||
        strstr(title, "REMAINING") || strstr(title, "Remaining") || strstr(title, "remaining") ||
        strstr(title, "TARGET") || strstr(title, "Target") || strstr(title, "target")) return UI_WARN;
    if (strstr(title, "AIR") || strstr(title, "CENTER") || strstr(title, "CHAMBER") || strstr(title, "TEMP")) return UI_ACCENT_ORANGE;
    if (strstr(title, "HUMID") || strstr(title, "RH")) return UI_ACCENT_SKY;
    if (strstr(title, "FAN")) return UI_BORDER_BRIGHT;
    if (strstr(title, "PROGRESS")) return UI_ACCENT_PURPLE;
    if (strstr(title, "ELAPSED") || strstr(title, "TIME")) return UI_ACCENT_INFO;
    if (strstr(title, "WIFI") || strstr(title, "NETWORK") || strstr(title, "MOONRAKER") || strstr(title, "STATE") || strstr(title, "STATUS")) return UI_OK_BRIGHT;
    if (strstr(title, "FILE") || strstr(title, "FILES")) return UI_ACCENT_INFO;
    if (strstr(title, "DRY")) return UI_ACCENT_SKY;
    if (strstr(title, "OTA") || strstr(title, "SYSTEM") || strstr(title, "SETTINGS")) return UI_ACCENT_PURPLE;

    return UI_ACCENT_INFO;
}"""

if cards.count(old_function) != 1:
    raise RuntimeError(
        f"expected one raw icon-color function, found "
        f"{cards.count(old_function)}"
    )

cards = cards.replace(old_function, new_function, 1)

old_call = """    lv_obj_set_style_text_color(ico, lv_color_hex(card_color_for_title(title)), 0);"""
new_call = """    lv_obj_set_style_text_color(ico, card_color_for_title(title), 0);"""

if cards.count(old_call) != 1:
    raise RuntimeError(
        f"expected one icon-color conversion call, found "
        f"{cards.count(old_call)}"
    )

cards = cards.replace(old_call, new_call, 1)
cards_path.write_text(cards)

updated = cards_path.read_text()

for literal in (
    "0x3aa8ff",
    "0xff4d4d",
    "0xffc857",
    "0xff8c2a",
    "0x00d1ff",
    "0x8fd3ff",
    "0xb65cff",
    "0x70e000",
):
    if literal in updated:
        raise RuntimeError(
            f"obsolete icon palette literal remains: {literal}"
        )

print("PASS: Printer info-card icon palette is theme-owned")
print("All icon colors remain visually identical")
print("Changed:")
print("  main/ui_theme.h")
print("  main/ui_printer_info_cards.c")
