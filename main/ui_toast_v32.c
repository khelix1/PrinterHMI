#include "ui_toast_v32.h"

#include "lvgl.h"

static lv_obj_t *s_toast;
static lv_timer_t *s_timer;

void ui_toast_v32_close(void)
{
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    if (s_toast) {
        lv_obj_delete(s_toast);
        s_toast = NULL;
    }
}

static void toast_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_toast_v32_close();
}

void ui_toast_v32_show(ui_status_kind_t kind,
                       const char *title,
                       const char *detail)
{
    ui_toast_v32_close();

    s_toast = lv_obj_create(lv_screen_active());
    if (!s_toast) return;

    lv_obj_set_size(s_toast, 430, 76);
    lv_obj_align(s_toast, LV_ALIGN_BOTTOM_RIGHT, -18, -18);
    lv_obj_clear_flag(s_toast, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_popup_style(s_toast);
    lv_obj_set_style_border_color(s_toast, ui_status_color(kind), 0);
    lv_obj_set_style_border_width(
        s_toast,
        UI_BORDER_STRONG,
        0);
    lv_obj_set_style_pad_all(s_toast, 0, 0);

    lv_obj_t *accent = lv_obj_create(s_toast);
    lv_obj_set_size(accent, 6, 66);
    lv_obj_set_pos(accent, 4, 3);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_surface_role(accent, UI_SURFACE_INDICATOR);
    lv_obj_set_style_bg_color(accent, ui_status_color(kind), 0);

    lv_obj_t *title_label = lv_label_create(s_toast);
    lv_label_set_text(title_label, title ? title : "");
    lv_obj_set_width(title_label, 386);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
    ui_apply_custom_label_style(title_label,
                                UI_FONT_BODY_LARGE,
                                ui_status_color(kind));
    lv_obj_set_pos(title_label, 24, 11);

    lv_obj_t *detail_label = lv_label_create(s_toast);
    lv_label_set_text(detail_label, detail ? detail : "");
    lv_obj_set_width(detail_label, 386);
    lv_label_set_long_mode(detail_label, LV_LABEL_LONG_DOT);
    ui_apply_text_caption(detail_label);
    ui_apply_label_dim(detail_label);
    lv_obj_set_pos(detail_label, 24, 42);

    lv_obj_move_foreground(s_toast);
    s_timer = lv_timer_create(toast_timer_cb, 2600, NULL);
    if (s_timer) lv_timer_set_repeat_count(s_timer, 1);
}
