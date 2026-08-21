#include "ui_calibration_layout.h"
#include "ui_text.h"

#include <string.h>

#include "ui_cards.h"
#include "ui_theme.h"
#include "ui_widgets.h"

lv_obj_t *ui_calibration_layout_label(lv_obj_t *parent, const char *text,
                                      const lv_font_t *font, lv_color_t color,
                                      int x, int y, int width)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : ui_text("--"));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, width);
    lv_obj_set_pos(label, x, y);
    ui_apply_custom_label_style(label, font, color);
    return label;
}

lv_obj_t *ui_calibration_layout_card(lv_obj_t *parent, const char *title,
                                     int x, int y,
                                     ui_calibration_card_refs_t *refs)
{
    lv_obj_t *card = ui_create_operator_card(parent, x, y, 390, 176);
    if (!card) return NULL;
    ui_create_operator_card_heading(card, title, 16, 14);
    ui_create_operator_card_divider(card, 16, 45, 358);
    if (refs) {
        refs->summary = ui_calibration_layout_label(card,
            "Waiting for active-printer discovery.", UI_FONT_BODY,
            UI_TEXT_DIM, 16, 58, 358);
        refs->status = ui_calibration_layout_label(card, "AWAITING DISCOVERY",
            UI_FONT_CAPTION, UI_ACCENT_BRIGHT, 16, 142, 190);
    }
    return card;
}

void ui_calibration_layout_append_tool(char *output, size_t output_size,
                                       const char *tool)
{
    if (!output || output_size == 0 || !tool || !tool[0]) return;
    size_t used = strlen(output);
    if (used >= output_size - 1) return;
    lv_snprintf(output + used, output_size - used, "%s%s",
                used ? "  /  " : "", tool);
}

void ui_calibration_layout_set_card(ui_calibration_card_refs_t *refs,
                                    const char *summary, size_t count,
                                    size_t macro_count)
{
    if (!refs || !refs->summary || !refs->status) return;
    lv_label_set_text(refs->summary, summary && summary[0] ? summary :
                      ui_text("No applicable tools reported by this printer."));
    char status[48];
    if (macro_count > 0) {
        lv_snprintf(status, sizeof(status), "%u TOOL%s + %u MACRO%s",
                    (unsigned)count, count == 1 ? "" : "S",
                    (unsigned)macro_count, macro_count == 1 ? "" : "S");
    } else if (count > 0) {
        lv_snprintf(status, sizeof(status), "%u TOOL%s DETECTED",
                    (unsigned)count, count == 1 ? "" : "S");
    } else {
        lv_snprintf(status, sizeof(status), "NOT CONFIGURED");
    }
    lv_label_set_text(refs->status, status);
    if (count > 0 || macro_count > 0) ui_apply_label_bright(refs->status);
    else ui_apply_label_dim(refs->status);
}

void ui_calibration_layout_set_action_label(lv_obj_t *button,
                                            const char *text)
{
    lv_obj_t *label = button ? lv_obj_get_child(button, 0) : NULL;
    if (label) lv_label_set_text(label, text ? text : ui_text(""));
}
