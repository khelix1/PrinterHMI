#!/usr/bin/env python3

from pathlib import Path

path = Path("main/ui_dashboard_status_v32.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

replacements = [
    (
        """    lv_obj_set_style_text_font(rt, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(rt, UI_TEXT_DIM, 0);""",
        """    ui_apply_text_body(rt);
    ui_apply_label_dim(rt);""",
    ),
    (
        """    lv_obj_set_style_text_font(out.state, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(out.state, UI_TEXT, 0);""",
        """    ui_apply_text_heading(out.state);
    ui_apply_label_primary(out.state);""",
    ),
    (
        """    lv_obj_set_style_text_font(out.progress, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(out.progress, UI_TEXT, 0);""",
        """    ui_apply_text_heading(out.progress);
    ui_apply_label_primary(out.progress);""",
    ),
    (
        """    lv_obj_set_style_text_font(el, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(el, UI_TEXT_DIM, 0);""",
        """    ui_apply_text_caption(el);
    ui_apply_label_dim(el);""",
    ),
    (
        """    lv_obj_set_style_text_font(out.elapsed, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(out.elapsed, UI_TEXT, 0);""",
        """    ui_apply_text_title(out.elapsed);
    ui_apply_label_primary(out.elapsed);""",
    ),
    (
        """    lv_obj_set_style_text_font(rl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(rl, UI_TEXT_DIM, 0);""",
        """    ui_apply_text_caption(rl);
    ui_apply_label_dim(rl);""",
    ),
    (
        """    lv_obj_set_style_text_font(out.remaining, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(out.remaining, UI_TEXT, 0);""",
        """    ui_apply_text_title(out.remaining);
    ui_apply_label_primary(out.remaining);""",
    ),
    (
        """    lv_obj_set_style_text_font(eta_l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(eta_l, UI_TEXT_DIM, 0);""",
        """    ui_apply_text_caption(eta_l);
    ui_apply_label_dim(eta_l);""",
    ),
    (
        """    lv_obj_set_style_text_font(out.eta, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(out.eta, UI_TEXT, 0);""",
        """    ui_apply_text_title(out.eta);
    ui_apply_label_primary(out.eta);""",
    ),
    (
        """    lv_obj_set_style_text_font(nt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(nt, UI_TEXT_DIM, 0);""",
        """    ui_apply_text_caption(nt);
    ui_apply_label_dim(nt);""",
    ),
    (
        """    lv_obj_set_style_text_font(out.nozzle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(out.nozzle, UI_TEXT, 0);""",
        """    ui_apply_text_value_small(out.nozzle);
    ui_apply_label_primary(out.nozzle);""",
    ),
    (
        """    lv_obj_set_style_text_font(bt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bt, UI_TEXT_DIM, 0);""",
        """    ui_apply_text_caption(bt);
    ui_apply_label_dim(bt);""",
    ),
    (
        """    lv_obj_set_style_text_font(out.bed, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(out.bed, UI_TEXT, 0);""",
        """    ui_apply_text_value_small(out.bed);
    ui_apply_label_primary(out.bed);""",
    ),
]

for old, new in replacements:
    found = text.count(old)

    if found != 1:
        raise RuntimeError(
            "expected one typography pair:\n"
            f"{old}\nfound {found}"
        )

    text = text.replace(old, new, 1)

path.write_text(text)

updated = path.read_text()

remaining_fonts = updated.count("lv_obj_set_style_text_font(")

if remaining_fonts != 0:
    raise RuntimeError(
        f"expected zero direct font assignments, found {remaining_fonts}"
    )

expected_dynamic_colors = 1
remaining_colors = updated.count("lv_obj_set_style_text_color(")

if remaining_colors != expected_dynamic_colors:
    raise RuntimeError(
        f"expected {expected_dynamic_colors} dynamic text-color assignment, "
        f"found {remaining_colors}"
    )

print("PASS: Dashboard status typography uses semantic theme helpers")
print("Dynamic progress coloring preserved")
print("Changed:")
print("  main/ui_dashboard_status_v32.c")
