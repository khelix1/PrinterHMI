#include "ui_cards.h"
#include "ui_theme.h"

lv_obj_t *ui_info_card_create(lv_obj_t *parent,
                              const char *title,
                              const char *value,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height)
{
    if (!parent) return NULL;

    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(box, width, height);
    lv_obj_set_pos(box, x, y);

    ui_apply_card_style(box);

    /*
     * Preserve the established information-card layout.
     * Theme A owns the card surface, border, and radius.
     */
    lv_obj_set_style_pad_all(box, UI_PAD_PANEL, 0);

    lv_obj_t *title_label = lv_label_create(box);
    lv_label_set_text(title_label, title ? title : "");
    ui_apply_text_body_large(title_label);
    ui_apply_label_dim(title_label);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *value_label = lv_label_create(box);
    lv_label_set_text(value_label, value ? value : "");
    lv_obj_set_width(value_label,
                     width - (UI_PAD_PANEL * 2));
    lv_label_set_long_mode(value_label,
                           LV_LABEL_LONG_DOT);
    ui_apply_text_title(value_label);
    ui_apply_label_primary(value_label);
    lv_obj_align(value_label,
                 LV_ALIGN_BOTTOM_LEFT,
                 0,
                 0);

    return value_label;
}
