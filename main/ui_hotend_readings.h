#pragma once

#include <stdio.h>
#include <string.h>

#include "moonraker.h"
#include "ui_font_fallback.h"
#include "ui_theme.h"
#include "ui_temperature_reading.h"
#include "ui_value_update.h"

/* The catalog is compact; an absent extruder1 must not rename extruder2. */
static inline unsigned ui_hotend_number(const moonraker_hotend_t *hotend, size_t index)
{
    const char *name = hotend->object_name;
    if (!strcmp(name, "extruder")) return 0;
    if (!strncmp(name, "extruder", 8) && name[8] >= '1' && name[8] <= '3' && !name[9])
        return (unsigned)(name[8] - '0');
    return (unsigned)index;
}

static inline void ui_hotend_visible(lv_obj_t *obj, bool visible)
{
    if (!obj) return;
    if (visible) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

typedef struct {
    int x, y, width, height;
    int two_columns, many_columns;
    const lv_font_t *two_font, *many_font;
} ui_hotend_layout_t;

/* Both Studio pages use the Dashboard's 220 px column as the common limit. */
static inline ui_hotend_layout_t ui_studio_hotend_layout(int x, int y)
{
    return (ui_hotend_layout_t){x, y, 220, 96, 1, 1,
        &ui_studio_font_32, &ui_studio_font_18};
}

static inline void ui_hotend_create_readings(lv_obj_t *parent, lv_obj_t **labels)
{
    for (int i = 0; i < MOONRAKER_MAX_HOTENDS; ++i) {
        lv_obj_t *label = labels[i] = lv_label_create(parent);
        ui_apply_custom_label_style(label, &lv_font_montserrat_14, UI_TEXT);
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        ui_hotend_visible(label, false);
    }
}

/* Lay out the tools when the catalog size changes. Fit the widest ordinary
 * reading up front so the font doesn't jump while temperatures move. */
static inline void ui_hotend_layout_readings(lv_obj_t *const *labels,
    const ui_hotend_layout_t *layout, size_t count)
{
    if (count < 2) return;
    if (count > MOONRAKER_MAX_HOTENDS) count = MOONRAKER_MAX_HOTENDS;
    int columns = count == 2 ? layout->two_columns : layout->many_columns;
    int rows = ((int)count + columns - 1) / columns;
    int cell = layout->width / columns;
    int pitch = layout->height / rows;
    const lv_font_t *preferred = count == 2 ? layout->two_font : layout->many_font;
    const lv_font_t *candidates[] = {preferred, &lv_font_montserrat_28,
        &lv_font_montserrat_24, &lv_font_montserrat_22, &lv_font_montserrat_20,
        &lv_font_montserrat_18, &lv_font_montserrat_16, &lv_font_montserrat_14,
        &lv_font_montserrat_12};
    const lv_font_t *font = ui_font_with_fallback(&lv_font_montserrat_12);
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (candidates[i]->line_height > preferred->line_height ||
            candidates[i]->line_height > pitch) continue;
        const lv_font_t *candidate = ui_font_with_fallback(candidates[i]);
        lv_point_t size;
        lv_text_get_size(&size, "T3 888.8/888.8°", candidate, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > cell - 6) continue;
        font = candidate;
        break;
    }
    for (size_t i = 0; i < count; ++i) {
        lv_obj_t *label = labels[i];
        if (!label) continue;
        lv_obj_set_style_text_font(label, font, 0);
        lv_obj_set_style_text_letter_space(label, 0, 0);
        lv_obj_set_pos(label, layout->x + ((int)i % columns) * cell,
            layout->y + ((int)i / columns) * pitch + (pitch - font->line_height) / 2);
        lv_obj_set_size(label, cell - 2, font->line_height);
    }
}

/* Use a compact, single-line current / target pair per tool. Keep the tool
 * ID visible if its readings are missing, but never display stale numbers. */
static inline void ui_hotend_update_readings(lv_obj_t *const *labels,
    const moonraker_hotend_t *hotends, size_t count, bool online)
{
    for (size_t i = 0; i < MOONRAKER_MAX_HOTENDS; ++i) {
        if (!labels[i]) continue;
        bool visible = hotends && count > 1 && i < count;
        ui_hotend_visible(labels[i], visible);
        if (!visible) continue;

        double current = online ? hotends[i].temperature : NAN;
        double target = online ? hotends[i].target : NAN;
        bool current_ok = ui_temperature_current_valid(current);
        char pair[20], text[32];
        ui_format_temperature_pair(pair, sizeof(pair), current, target);
        snprintf(text, sizeof(text), "T%u %s", ui_hotend_number(&hotends[i], i), pair);
        ui_value_set_text(labels[i], text);
        ui_value_set_color(labels[i], !current_ok ? UI_TEXT_DIM :
            hotends[i].active ? UI_OK_BRIGHT : UI_TEXT, 0);
    }
}
