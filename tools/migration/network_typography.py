#!/usr/bin/env python3

from pathlib import Path

path = Path("main/ui_network_v32.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

replacements = [
    (
        """    lv_obj_set_style_text_font(
        label,
        &lv_font_montserrat_16,
        0);""",
        """    ui_apply_text_body(label);""",
    ),
    (
        """    lv_obj_set_style_text_color(
        label,
        UI_TEXT,
        0);""",
        """    ui_apply_label_primary(label);""",
    ),
    (
        """        lv_obj_set_style_text_color(
            name_label,
            UI_TEXT_DIM,
            0);""",
        """        ui_apply_label_dim(name_label);""",
    ),
    (
        """        lv_obj_set_style_text_font(
            s_network.banner_title,
            &lv_font_montserrat_20,
            0);""",
        """        ui_apply_text_value_small(
            s_network.banner_title);""",
    ),
    (
        """        lv_obj_set_style_text_color(
            s_network.banner_detail,
            UI_TEXT_DIM,
            0);""",
        """        ui_apply_label_dim(
            s_network.banner_detail);""",
    ),
    (
        """            lv_obj_set_style_text_font(
                s_network.banner_badge_label,
                &lv_font_montserrat_16,
                0);""",
        """            ui_apply_text_body(
                s_network.banner_badge_label);""",
    ),
    (
        """            lv_obj_set_style_text_color(
                s_network.banner_badge_label,
                UI_TEXT,
                0);""",
        """            ui_apply_label_primary(
                s_network.banner_badge_label);""",
    ),
    (
        """            lv_obj_set_style_text_font(
                s_network.networks_status,
                &lv_font_montserrat_14,
                0);""",
        """            ui_apply_text_caption(
                s_network.networks_status);""",
    ),
    (
        """            lv_obj_set_style_text_color(
                s_network.networks_status,
                UI_TEXT_DIM,
                0);""",
        """            ui_apply_label_dim(
                s_network.networks_status);""",
    ),
    (
        """            lv_obj_set_style_text_font(
                name,
                &lv_font_montserrat_16,
                0);""",
        """            ui_apply_text_body(name);""",
    ),
    (
        """            lv_obj_set_style_text_color(
                name,
                UI_TEXT,
                0);""",
        """            ui_apply_label_primary(name);""",
    ),
    (
        """            lv_obj_set_style_text_font(
                signal,
                &lv_font_montserrat_14,
                0);""",
        """            ui_apply_text_caption(signal);""",
    ),
]

for old, new in replacements:
    found = text.count(old)

    if found != 1:
        raise RuntimeError(
            "expected one typography assignment:\n"
            f"{old}\nfound {found}"
        )

    text = text.replace(old, new, 1)

path.write_text(text)

updated = path.read_text()

remaining_fonts = updated.count("lv_obj_set_style_text_font(")
remaining_colors = updated.count("lv_obj_set_style_text_color(")

if remaining_fonts != 0:
    raise RuntimeError(
        f"expected zero direct font assignments, found {remaining_fonts}"
    )

if remaining_colors != 4:
    raise RuntimeError(
        f"expected 4 semantic runtime/accent color assignments, "
        f"found {remaining_colors}"
    )

print("PASS: Network typography uses semantic theme helpers")
print("Preserved:")
print("  banner-title cyan accent")
print("  signal-strength cyan accent")
print("  live Wi-Fi state color")
print("  live Moonraker state color")
print("Changed:")
print("  main/ui_network_v32.c")
