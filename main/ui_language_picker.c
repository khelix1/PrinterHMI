#include "ui_language_picker.h"

#include "language_controller.h"
#include "ui_i18n.h"
#include "ui_popup.h"
#include "ui_theme.h"

#include <stdint.h>

static lv_obj_t *s_popup = NULL;
static ui_language_picker_changed_cb_t s_changed_cb = NULL;

void ui_language_picker_close(void)
{
    if (s_popup) lv_obj_delete(s_popup);
    s_popup = NULL;
    s_changed_cb = NULL;
}

static void picker_close_cb(lv_event_t *event)
{
    (void)event;
    ui_language_picker_close();
}

static void picker_apply_async(void *user_data)
{
    (void)user_data;
    ui_language_picker_changed_cb_t changed_cb = s_changed_cb;
    ui_language_picker_close();
    if (changed_cb) changed_cb();
}

static void picker_select_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    ui_language_id_t language = (ui_language_id_t)(uintptr_t)lv_event_get_user_data(event);
    if (!language_controller_select(language)) return;
    lv_async_call(picker_apply_async, NULL);
}

void ui_language_picker_show(ui_language_picker_changed_cb_t changed_cb)
{
    s_changed_cb = changed_cb;
    if (s_popup) { lv_obj_move_foreground(s_popup); return; }
    s_popup = ui_popup_create(lv_layer_top(), 680, 360, UI_POPUP_STANDARD);
    if (!s_popup) return;
    ui_popup_add_title(s_popup, ui_i18n_text(UI_TEXT_LANGUAGE_PICKER_TITLE), false, 8);
    ui_popup_add_header_divider(s_popup, 44);
    ui_popup_add_status_label(s_popup, ui_i18n_text(UI_TEXT_LANGUAGE_PICKER_BODY), 24, 52, 632);
    lv_obj_t *list = ui_popup_add_list(s_popup, 24, 110, 632, 126);
    if (!list) { ui_language_picker_close(); return; }
    const ui_language_id_t active = language_controller_active();
    for (int language = UI_LANGUAGE_ENGLISH; language < UI_LANGUAGE_COUNT; ++language) {
        lv_obj_t *row = ui_popup_add_selectable_row(
            list, language_controller_native_name((ui_language_id_t)language),
            8, 8 + language * 56, 600, 48,
            picker_select_cb, (void *)(uintptr_t)language);
        ui_popup_set_selectable_row_text_font(
            row, ui_i18n_language_font(
                (ui_language_id_t)language, UI_FONT_BODY_LARGE));
        ui_popup_set_selectable_row_selected(row, language == active);
    }
    ui_popup_add_standard_footer_divider(s_popup);
    ui_popup_add_footer_action(s_popup, UI_POPUP_ACTION_CLOSE,
        ui_i18n_text(UI_TEXT_CLOSE), 160, UI_POPUP_FOOTER_CENTER,
        picker_close_cb, NULL, NULL);
}
