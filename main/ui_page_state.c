#include "ui_page_state.h"
#include "ui_text.h"

#include "ui_theme.h"

struct ui_page_state {
    lv_obj_t *root;
    lv_obj_t *icon;
    lv_obj_t *title;
    lv_obj_t *detail;
    lv_obj_t *spinner;
};

static void page_state_delete_cb(lv_event_t *event)
{
    ui_page_state_t *state =
        (ui_page_state_t *)lv_event_get_user_data(event);
    if (state) lv_free(state);
}

ui_page_state_t *ui_page_state_create(
    lv_obj_t *parent, int x, int y, int width, int height)
{
    if (!parent) return NULL;

    ui_page_state_t *state = lv_malloc(sizeof(*state));
    if (!state) return NULL;

    state->root = lv_obj_create(parent);
    state->icon = NULL;
    state->title = NULL;
    state->detail = NULL;
    state->spinner = NULL;

    if (!state->root) {
        lv_free(state);
        return NULL;
    }

    lv_obj_set_size(state->root, width, height);
    lv_obj_set_pos(state->root, x, y);
    lv_obj_clear_flag(state->root, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_root_style(state->root);
    lv_obj_add_event_cb(state->root,
                        page_state_delete_cb,
                        LV_EVENT_DELETE,
                        state);

    state->icon = lv_label_create(state->root);
    ui_apply_text_heading(state->icon);
    lv_obj_align(state->icon, LV_ALIGN_CENTER, 0, -48);

    state->title = lv_label_create(state->root);
    lv_obj_set_width(state->title, width - 80);
    lv_obj_set_style_text_align(state->title, LV_TEXT_ALIGN_CENTER, 0);
    ui_apply_text_title(state->title);
    ui_apply_label_bright(state->title);
    lv_obj_align(state->title, LV_ALIGN_CENTER, 0, 0);

    state->detail = lv_label_create(state->root);
    lv_obj_set_width(state->detail, width - 120);
    lv_label_set_long_mode(state->detail, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(state->detail, LV_TEXT_ALIGN_CENTER, 0);
    ui_apply_text_body(state->detail);
    ui_apply_label_dim(state->detail);
    lv_obj_align(state->detail, LV_ALIGN_CENTER, 0, 42);

    state->spinner = lv_spinner_create(state->root);
    lv_obj_set_size(state->spinner, 34, 34);
    lv_obj_align(state->spinner, LV_ALIGN_CENTER, 0, -50);
    lv_obj_add_flag(state->root, LV_OBJ_FLAG_HIDDEN);
    return state;
}

void ui_page_state_show(ui_page_state_t *state,
                            ui_page_state_kind_t kind,
                            const char *title,
                            const char *detail)
{
    if (!state || !state->root) return;

    const char *icon = LV_SYMBOL_WARNING;
    lv_color_t color = UI_TEXT_DIM;

    switch (kind) {
    case UI_PAGE_STATE_LOADING:
        icon = "";
        color = UI_ACCENT_CYAN;
        lv_obj_clear_flag(state->spinner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(state->icon, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_PAGE_STATE_EMPTY:
        icon = LV_SYMBOL_DIRECTORY;
        color = UI_ACCENT_CYAN;
        lv_obj_add_flag(state->spinner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(state->icon, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_PAGE_STATE_OFFLINE:
        icon = LV_SYMBOL_WIFI;
        color = UI_DANGER_BRIGHT;
        lv_obj_add_flag(state->spinner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(state->icon, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_PAGE_STATE_ERROR:
    default:
        color = UI_DANGER_BRIGHT;
        lv_obj_add_flag(state->spinner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(state->icon, LV_OBJ_FLAG_HIDDEN);
        break;
    }

    lv_label_set_text(state->icon, icon);
    ui_apply_custom_label_style(state->icon, UI_FONT_HEADING, color);
    lv_label_set_text(state->title, title ? title : ui_text(""));
    lv_label_set_text(state->detail, detail ? detail : ui_text(""));
    lv_obj_clear_flag(state->root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(state->root);
}

void ui_page_state_hide(ui_page_state_t *state)
{
    if (state && state->root) {
        lv_obj_add_flag(state->root, LV_OBJ_FLAG_HIDDEN);
    }
}
