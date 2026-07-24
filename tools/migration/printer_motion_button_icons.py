#!/usr/bin/env python3

from pathlib import Path

path = Path("main/ui_printer_motion.c")

if not path.is_file():
    raise RuntimeError(f"missing file: {path}")

text = path.read_text()

helper_anchor = """    return btn;
}

void ui_printer_motion_update_step_highlight"""

icon_helper = """    return btn;
}

static lv_obj_t *ui_printer_motion_icon_button(
    lv_obj_t *parent,
    const char *symbol,
    const char *text,
    lv_color_t icon_color,
    ui_button_icon_layout_t layout,
    int x,
    int y,
    int w,
    int h,
    lv_event_cb_t cb,
    const char *user)
{
    lv_obj_t *btn =
        ui_button_create_icon(
            parent,
            UI_BUTTON_OUTLINED,
            symbol,
            text,
            icon_color,
            layout);

    if (!btn) {
        return NULL;
    }

    lv_obj_set_size(
        btn,
        w,
        h);

    lv_obj_set_pos(
        btn,
        x,
        y);

    if (cb) {
        lv_obj_add_event_cb(
            btn,
            cb,
            LV_EVENT_CLICKED,
            (void *)user);
    }

    return btn;
}

void ui_printer_motion_update_step_highlight"""

if text.count(helper_anchor) != 1:
    raise RuntimeError(
        f"expected one Motion helper anchor, "
        f"found {text.count(helper_anchor)}"
    )

text = text.replace(
    helper_anchor,
    icon_helper,
    1,
)

old_buttons = """    ui_printer_motion_button(popup, "Y+",   260,  85, 120, 70, jog_cb, "Y+");
    ui_printer_motion_button(popup, "X-",   125, 175, 120, 70, jog_cb, "X-");
    ui_printer_motion_button(popup, LV_SYMBOL_HOME " HOME", 260, 175, 120, 70, jog_cb, LV_SYMBOL_HOME " HOME");
    ui_printer_motion_button(popup, "X+",   395, 175, 120, 70, jog_cb, "X+");
    ui_printer_motion_button(popup, "Y-",   260, 265, 120, 70, jog_cb, "Y-");

    ui_printer_motion_button(popup, "Z+",   545, 130, 110, 70, jog_cb, "Z+");
    ui_printer_motion_button(popup, "Z-",   545, 225, 110, 70, jog_cb, "Z-");

    ui_printer_motion_button(popup, "EXTRUDE", 520, 310, 135, 50, extrude_cb, "EXTRUDE");
    ui_printer_motion_button(popup, "RETRACT", 520, 370, 135, 50, extrude_cb, "RETRACT");"""

new_buttons = """    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_UP,
        "Y+",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_VERTICAL,
        260, 85, 120, 70,
        jog_cb,
        "Y+");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_LEFT,
        "X-",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_VERTICAL,
        125, 175, 120, 70,
        jog_cb,
        "X-");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_HOME,
        "HOME",
        UI_OK_BRIGHT,
        UI_BUTTON_ICON_VERTICAL,
        260, 175, 120, 70,
        jog_cb,
        LV_SYMBOL_HOME " HOME");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_RIGHT,
        "X+",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_VERTICAL,
        395, 175, 120, 70,
        jog_cb,
        "X+");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_DOWN,
        "Y-",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_VERTICAL,
        260, 265, 120, 70,
        jog_cb,
        "Y-");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_UP,
        "Z+",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_VERTICAL,
        545, 130, 110, 70,
        jog_cb,
        "Z+");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_DOWN,
        "Z-",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_VERTICAL,
        545, 225, 110, 70,
        jog_cb,
        "Z-");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_PLUS,
        "EXTRUDE",
        UI_ACCENT_ORANGE,
        UI_BUTTON_ICON_HORIZONTAL,
        520, 310, 135, 50,
        extrude_cb,
        "EXTRUDE");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_MINUS,
        "RETRACT",
        UI_ACCENT_PURPLE,
        UI_BUTTON_ICON_HORIZONTAL,
        520, 370, 135, 50,
        extrude_cb,
        "RETRACT");"""

if text.count(old_buttons) != 1:
    raise RuntimeError(
        f"expected one text-only Motion action block, "
        f"found {text.count(old_buttons)}"
    )

text = text.replace(
    old_buttons,
    new_buttons,
    1,
)

path.write_text(text)

updated = path.read_text()

if updated.count(
    "static lv_obj_t *ui_printer_motion_icon_button("
) != 1:
    raise RuntimeError("Motion icon helper missing")

icon_calls = updated.count(
    "    ui_printer_motion_icon_button("
)

if icon_calls != 9:
    raise RuntimeError(
        f"expected 9 icon Motion buttons, found {icon_calls}"
    )

plain_calls = updated.count(
    "ui_printer_motion_button(popup,"
)

if plain_calls != 3:
    raise RuntimeError(
        f"expected 3 text-only step selectors, found {plain_calls}"
    )

print("PASS: Motion action buttons now have semantic icons")
print("Preserved: text-only 1 mm, 10 mm and 50 mm selectors")
print("Changed:")
print("  main/ui_printer_motion.c")
