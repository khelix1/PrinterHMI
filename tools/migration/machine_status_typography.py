#!/usr/bin/env python3

from pathlib import Path

path = Path("main/ui_machine_status_v32.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

dead_helpers = """static lv_obj_t *make_name(lv_obj_t *parent, const char *txt, int x, int y)
{
    lv_obj_t *l = ui_create_card_subtitle(parent, txt);
    lv_obj_set_pos(l, x, y);
    return l;
}

static lv_obj_t *make_value(lv_obj_t *parent, const char *txt, int x, int y)
{
    lv_obj_t *v = ui_create_card_value(parent, txt);
    lv_obj_set_style_text_font(v, &lv_font_montserrat_22, 0);
    lv_obj_set_pos(v, x, y);
    return v;
}


"""

if text.count(dead_helpers) != 1:
    raise RuntimeError(
        "expected one dead make_name/make_value helper block, found "
        f"{text.count(dead_helpers)}"
    )

text = text.replace(dead_helpers, "", 1)

replacements = [
    (
        """    lv_obj_set_style_text_font(
        live,
        UI_FONT_CAPTION,
        0);

    lv_obj_set_style_text_color(
        live,
        UI_OK_BRIGHT,
        0);""",
        """    ui_apply_text_caption(live);
    ui_apply_label_success(live);""",
    ),
    (
        """        lv_obj_set_style_text_font(
            name,
            UI_FONT_CAPTION,
            0);

        lv_obj_set_style_text_color(
            name,
            UI_TEXT_DIM,
            0);""",
        """        ui_apply_text_caption(name);
        ui_apply_label_dim(name);""",
    ),
    (
        """        lv_obj_set_style_text_font(
            *values[index],
            UI_FONT_VALUE_SMALL,
            0);

        lv_obj_set_style_text_color(
            *values[index],
            UI_TEXT_BRIGHT,
            0);""",
        """        ui_apply_text_value_small(
            *values[index]);
        ui_apply_label_bright(
            *values[index]);""",
    ),
]

for old, new in replacements:
    found = text.count(old)

    if found != 1:
        raise RuntimeError(
            "expected one typography block:\n"
            f"{old}\nfound {found}"
        )

    text = text.replace(old, new, 1)

path.write_text(text)

updated = path.read_text()

for obsolete in (
    "make_name(",
    "make_value(",
    "lv_obj_set_style_text_font(",
    "lv_obj_set_style_text_color(",
):
    if obsolete in updated:
        raise RuntimeError(
            f"obsolete Machine Status code remains: {obsolete}"
        )

print("PASS: Machine Status typography uses semantic theme helpers")
print("PASS: dead local label helpers removed")
print("Changed:")
print("  main/ui_machine_status_v32.c")
