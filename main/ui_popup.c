#include "ui_popup.h"
#include "ui_text.h"

#include "ui_button.h"
#include "ui_theme.h"

#include <stdlib.h>

typedef struct {
    lv_obj_t *blocker;
    lv_obj_t *popup;
    lv_obj_t *footer;
    lv_obj_t *footer_buttons[3];
    int32_t footer_widths[3];
    int32_t footer_columns[4];
    int32_t footer_rows[2];
} ui_popup_modal_ctx_t;

static void modal_blocker_deleted_cb(lv_event_t *event)
{
    ui_popup_modal_ctx_t *ctx =
        event ? lv_event_get_user_data(event) : NULL;
    if (!ctx) return;

    ctx->blocker = NULL;
    if (!ctx->popup) free(ctx);
}

static void modal_popup_deleted_cb(lv_event_t *event)
{
    ui_popup_modal_ctx_t *ctx =
        event ? lv_event_get_user_data(event) : NULL;
    if (!ctx) return;

    ctx->popup = NULL;

    if (ctx->blocker) {
        lv_obj_t *blocker = ctx->blocker;
        ctx->blocker = NULL;
        lv_obj_delete(blocker);
        return;
    }

    free(ctx);
}

static lv_obj_t *popup_create_on_parent(lv_obj_t *modal_parent,
                          int32_t width,
                          int32_t height,
                          ui_popup_kind_t kind)
{
    if (!modal_parent) return NULL;

    /*
     * Popups are modal application surfaces. A transparent clickable
     * sibling beneath the popup owns the entire top layer, preventing a
     * touch outside the popup from reaching the page, shell, or an older
     * popup. It intentionally has no click action: the operator must use a
     * footer action to close the popup before interacting elsewhere.
     */

    lv_obj_t *blocker = lv_obj_create(modal_parent);
    if (!blocker) return NULL;

    lv_obj_remove_style_all(blocker);
    lv_obj_set_size(blocker, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(blocker, 0, 0);
    lv_obj_clear_flag(blocker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(blocker, LV_OBJ_FLAG_CLICKABLE);
    /* Every modal shares the same calm, readable operator backdrop. */
    lv_obj_set_style_bg_color(blocker, UI_BG_DEEP, 0);
    lv_obj_set_style_bg_opa(blocker, LV_OPA_60, 0);

    lv_obj_t *popup = lv_obj_create(modal_parent);
    if (!popup) {
        lv_obj_delete(blocker);
        return NULL;
    }

    ui_popup_modal_ctx_t *modal_ctx = calloc(1, sizeof(*modal_ctx));
    if (!modal_ctx) {
        lv_obj_delete(popup);
        lv_obj_delete(blocker);
        return NULL;
    }

    modal_ctx->blocker = blocker;
    modal_ctx->popup = popup;

    lv_obj_add_event_cb(
        blocker,
        modal_blocker_deleted_cb,
        LV_EVENT_DELETE,
        modal_ctx);
    lv_obj_add_event_cb(
        popup,
        modal_popup_deleted_cb,
        LV_EVENT_DELETE,
        modal_ctx);

    lv_obj_set_size(popup, width, height);
    lv_obj_center(popup);
    lv_obj_clear_flag(popup, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_popup_style(popup);

    if (kind == UI_POPUP_DANGER) {
        lv_obj_set_style_bg_color(popup, UI_BG_DANGER_POPUP, 0);
        lv_obj_set_style_border_color(popup, UI_DANGER_BRIGHT, 0);
    }

    lv_obj_move_foreground(popup);
    return popup;
}

lv_obj_t *ui_popup_create(lv_obj_t *parent, int32_t width, int32_t height,
                          ui_popup_kind_t kind)
{
    if (!parent) return NULL;
    lv_obj_t *layer = lv_layer_top();
    return popup_create_on_parent(layer ? layer : parent, width, height, kind);
}

lv_obj_t *ui_popup_create_on_screen(int32_t width, int32_t height,
                                    ui_popup_kind_t kind)
{
    /* Screen-local opaque descendants can occlude background drawing.
     * LVGL renders top-layer popups only after the active screen is drawn. */
    return popup_create_on_parent(lv_screen_active(), width, height, kind);
}

lv_obj_t *ui_popup_add_title(lv_obj_t *popup,
                             const char *text,
                             bool danger,
                             int32_t y_offset)
{
    if (!popup) return NULL;

    lv_obj_t *title = lv_label_create(popup);
    lv_label_set_text(title, text ? text : ui_text(""));
    ui_apply_text_popup_title(title);

    if (danger) {
        ui_apply_label_error(title);
    } else {
        ui_apply_label_primary(title);
    }

    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y_offset);
    return title;
}

lv_obj_t *ui_popup_add_message(lv_obj_t *popup,
                               const char *text,
                               bool muted,
                               int32_t y_offset)
{
    if (!popup) return NULL;

    lv_obj_t *message = lv_label_create(popup);
    lv_label_set_text(message, text ? text : ui_text(""));
    ui_apply_text_body_large(message);

    if (muted) {
        ui_apply_label_muted(message);
    } else {
        ui_apply_label_primary(message);
    }

    lv_obj_align(message, LV_ALIGN_TOP_MID, 0, y_offset);
    return message;
}

lv_obj_t *ui_popup_add_divider(lv_obj_t *popup,
                               int32_t x,
                               int32_t y,
                               int32_t width)
{
    if (!popup || width <= 0) {
        return NULL;
    }

    lv_obj_t *divider = lv_obj_create(popup);

    if (!divider) {
        return NULL;
    }

    lv_obj_set_size(divider, width, 1);
    lv_obj_set_pos(divider, x, y);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(divider, UI_SURFACE_DIVIDER);

    return divider;
}


lv_obj_t *ui_popup_add_header_divider(lv_obj_t *popup,
                                      int32_t y)
{
    if (!popup) {
        return NULL;
    }

    int32_t width = lv_obj_get_width(popup) - 48;

    return ui_popup_add_divider(
        popup,
        24,
        y,
        width);
}


lv_obj_t *ui_popup_add_footer_divider(lv_obj_t *popup,
                                      int32_t y)
{
    if (!popup) {
        return NULL;
    }

    int32_t width = lv_obj_get_width(popup) - 48;

    return ui_popup_add_divider(
        popup,
        24,
        y,
        width);
}

lv_obj_t *ui_popup_add_standard_footer_divider(lv_obj_t *popup)
{
    if (!popup) {
        return NULL;
    }

    return ui_popup_add_footer_divider(
        popup,
        lv_obj_get_height(popup) - 72);
}


lv_obj_t *ui_popup_add_body(lv_obj_t *popup,
                            const char *text,
                            int32_t x,
                            int32_t y,
                            int32_t width)
{
    if (!popup) {
        return NULL;
    }

    lv_obj_t *body = lv_label_create(popup);

    if (!body) {
        return NULL;
    }

    lv_label_set_text(
        body,
        text ? text : ui_text(""));

    lv_obj_set_width(
        body,
        width);

    lv_label_set_long_mode(
        body,
        LV_LABEL_LONG_WRAP);

    ui_apply_text_body_large(body);
    ui_apply_label_primary(body);

    lv_obj_set_style_text_align(
        body,
        LV_TEXT_ALIGN_LEFT,
        0);

    lv_obj_set_pos(
        body,
        x,
        y);

    return body;
}


lv_obj_t *ui_popup_add_status_label(lv_obj_t *popup,
                                    const char *text,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width)
{
    if (!popup) {
        return NULL;
    }

    lv_obj_t *status = lv_label_create(popup);

    if (!status) {
        return NULL;
    }

    lv_label_set_text(
        status,
        text ? text : ui_text(""));

    lv_obj_set_width(
        status,
        width);

    lv_label_set_long_mode(
        status,
        LV_LABEL_LONG_WRAP);

    ui_apply_text_body_large(status);
    ui_apply_label_muted(status);

    lv_obj_set_style_text_align(
        status,
        LV_TEXT_ALIGN_LEFT,
        0);

    lv_obj_set_pos(
        status,
        x,
        y);

    return status;
}


lv_obj_t *ui_popup_add_caption(lv_obj_t *popup,
                               const char *text,
                               int32_t x,
                               int32_t y,
                               int32_t width)
{
    if (!popup) return NULL;

    lv_obj_t *caption = lv_label_create(popup);
    lv_label_set_text(caption, text ? text : ui_text(""));
    lv_label_set_long_mode(caption, LV_LABEL_LONG_DOT);
    lv_obj_set_width(caption, width);
    lv_obj_set_pos(caption, x, y);

    ui_apply_custom_label_style(caption, UI_FONT_BODY, UI_TEXT_MUTED);

    return caption;
}

lv_obj_t *ui_popup_add_progress_status(lv_obj_t *popup,
                                       const char *text,
                                       int32_t x,
                                       int32_t y,
                                       int32_t width)
{
    if (!popup) return NULL;

    lv_obj_t *label = lv_label_create(popup);
    lv_label_set_text(label, text ? text : ui_text(""));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, width);
    lv_obj_set_pos(label, x, y);

    ui_apply_custom_label_style(label, UI_FONT_BODY_LARGE, UI_TEXT);

    lv_obj_set_style_text_align(
        label,
        LV_TEXT_ALIGN_CENTER,
        0);

    return label;
}

lv_obj_t *ui_popup_add_progress_value(lv_obj_t *popup,
                                      const char *text,
                                      int32_t x,
                                      int32_t y,
                                      int32_t width)
{
    if (!popup) return NULL;

    lv_obj_t *label = lv_label_create(popup);
    lv_label_set_text(label, text ? text : ui_text(""));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, width);
    lv_obj_set_pos(label, x, y);

    ui_apply_custom_label_style(label, UI_FONT_PERCENT, UI_ACCENT_BRIGHT);

    lv_obj_set_style_text_align(
        label,
        LV_TEXT_ALIGN_CENTER,
        0);

    return label;
}

lv_obj_t *ui_popup_add_progress_bar(lv_obj_t *popup,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height,
                                    int32_t minimum,
                                    int32_t maximum,
                                    int32_t value)
{
    if (!popup) return NULL;

    lv_obj_t *bar = lv_bar_create(popup);
    lv_obj_set_size(bar, width, height);
    lv_obj_set_pos(bar, x, y);

    lv_bar_set_range(bar, minimum, maximum);
    lv_bar_set_value(bar, value, LV_ANIM_OFF);

    /*
     * Progress-bar appearance belongs entirely to the active popup theme.
     */
    ui_apply_progress_bar_style(bar);

    return bar;
}

lv_obj_t *ui_popup_add_progress_detail(lv_obj_t *popup,
                                       const char *text,
                                       int32_t x,
                                       int32_t y,
                                       int32_t width)
{
    if (!popup) return NULL;

    lv_obj_t *label = lv_label_create(popup);
    lv_label_set_text(label, text ? text : ui_text(""));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, width);
    lv_obj_set_pos(label, x, y);

    ui_apply_custom_label_style(label, UI_FONT_BODY, UI_TEXT_DIM);

    lv_obj_set_style_text_align(
        label,
        LV_TEXT_ALIGN_CENTER,
        0);

    return label;
}


lv_obj_t *ui_popup_add_close_button(lv_obj_t *popup,
                                    int32_t width,
                                    int32_t height,
                                    lv_align_t align,
                                    int32_t x_offset,
                                    int32_t y_offset,
                                    ui_button_kind_t kind,
                                    lv_event_cb_t event_cb,
                                    void *user_data)
{
    return ui_popup_add_button_aligned(
        popup,
        LV_SYMBOL_CLOSE " CLOSE",
        width,
        height,
        align,
        x_offset,
        y_offset,
        kind,
        event_cb,
        user_data,
        NULL);
}


lv_obj_t *ui_popup_add_list(lv_obj_t *popup,
                            int32_t x,
                            int32_t y,
                            int32_t width,
                            int32_t height)
{
    if (!popup) return NULL;

    lv_obj_t *list = lv_obj_create(popup);
    lv_obj_set_size(list, width, height);
    lv_obj_set_pos(list, x, y);

    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    /*
     * Popup list appearance belongs to the shared popup theme.
     * Callers only choose geometry and populate the list.
     */
    ui_apply_surface_role(list, UI_SURFACE_POPUP_LIST);

    return list;
}

lv_obj_t *ui_popup_add_selectable_row(lv_obj_t *parent,
                                      const char *text,
                                      int32_t x,
                                      int32_t y,
                                      int32_t width,
                                      int32_t height,
                                      lv_event_cb_t event_cb,
                                      void *user_data)
{
    if (!parent || !text) return NULL;

    lv_obj_t *row =
        ui_button_create_empty(
            parent,
            UI_BUTTON_OUTLINED);

    if (!row) return NULL;

    lv_obj_set_size(row, width, height);
    lv_obj_set_pos(row, x, y);

    if (event_cb) {
        lv_obj_add_event_cb(
            row,
            event_cb,
            LV_EVENT_CLICKED,
            user_data);
    }

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, width - 32);
    ui_apply_custom_label_style(label, UI_FONT_BODY_LARGE, UI_TEXT);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 8, 0);

    return row;
}

void ui_popup_set_selectable_row_selected(lv_obj_t *row,
                                          bool selected)
{
    if (!row) return;

    if (selected) {
        ui_apply_button_status_style(row, UI_STATUS_ACTIVE);
    } else {
        ui_button_apply_kind(row, UI_BUTTON_OUTLINED);
    }
}


lv_obj_t *ui_popup_add_textarea(lv_obj_t *popup,
                                int32_t width,
                                int32_t height,
                                lv_align_t align,
                                int32_t x_offset,
                                int32_t y_offset,
                                bool one_line,
                                bool password_mode,
                                uint32_t max_length,
                                const char *placeholder,
                                const char *initial_text,
                                const char *accepted_chars)
{
    if (!popup) return NULL;

    lv_obj_t *textarea = lv_textarea_create(popup);
    lv_obj_set_size(textarea, width, height);
    lv_obj_align(textarea,
                 align,
                 x_offset,
                 y_offset);

    lv_textarea_set_one_line(textarea, one_line);
    lv_textarea_set_password_mode(textarea, password_mode);

    if (max_length > 0) {
        lv_textarea_set_max_length(textarea, max_length);
    }

    if (placeholder) {
        lv_textarea_set_placeholder_text(textarea, placeholder);
    }

    if (initial_text) {
        lv_textarea_set_text(textarea, initial_text);
    }

    if (accepted_chars) {
        lv_textarea_set_accepted_chars(textarea, accepted_chars);
    }

    /*
     * Popup input appearance belongs to the shared theme.
     */
    ui_apply_text_title(textarea);

    ui_apply_surface_role(textarea, UI_SURFACE_TEXT_INPUT);

    return textarea;
}

lv_obj_t *ui_popup_add_keyboard(lv_obj_t *popup,
                                lv_obj_t *textarea,
                                int32_t width,
                                int32_t height,
                                lv_align_t align,
                                int32_t x_offset,
                                int32_t y_offset,
                                lv_keyboard_mode_t mode)
{
    if (!popup || !textarea) return NULL;

    lv_obj_t *keyboard = lv_keyboard_create(popup);
    lv_obj_set_size(keyboard, width, height);
    lv_obj_align(keyboard,
                 align,
                 x_offset,
                 y_offset);

    lv_keyboard_set_mode(keyboard, mode);
    lv_keyboard_set_textarea(keyboard, textarea);

    /*
     * Keyboard surface and keys belong to the popup theme.
     */
    ui_apply_surface_role(keyboard, UI_SURFACE_KEYBOARD);

    return keyboard;
}

static ui_button_kind_t ui_popup_action_button_kind(
    ui_popup_action_t action)
{
    switch (action) {
        case UI_POPUP_ACTION_PRIMARY:
        case UI_POPUP_ACTION_CONFIRM:
            return UI_BUTTON_PRIMARY;

        case UI_POPUP_ACTION_DANGER:
            return UI_BUTTON_DANGER;

        case UI_POPUP_ACTION_SECONDARY:
        case UI_POPUP_ACTION_CANCEL:
        case UI_POPUP_ACTION_CLOSE:
        case UI_POPUP_ACTION_CHOICE:
        default:
            return UI_BUTTON_OUTLINED;
    }
}

static ui_popup_modal_ctx_t *modal_context(lv_obj_t *popup)
{
    for (uint32_t i = 0; popup && i < lv_obj_get_event_count(popup); ++i) {
        lv_event_dsc_t *dsc = lv_obj_get_event_dsc(popup, i);
        if (lv_event_dsc_get_cb(dsc) == modal_popup_deleted_cb)
            return lv_event_dsc_get_user_data(dsc);
    }
    return NULL;
}

lv_obj_t *ui_popup_find_owner(lv_obj_t *object)
{
    for (lv_obj_t *p = object; p; p = lv_obj_get_parent(p))
        if (modal_context(p)) return p;
    return NULL;
}

static void footer_layout(ui_popup_modal_ctx_t *ctx)
{
    if (!ctx || !ctx->footer) return;
    lv_obj_update_layout(ctx->popup);
    int32_t width = lv_obj_get_content_width(ctx->popup) - 48;
    if (width < 1) width = 1;
    lv_obj_set_size(ctx->footer, width, 48);
    lv_obj_align(ctx->footer, LV_ALIGN_BOTTOM_MID, 0, -12);
    unsigned count = 0;
    for (unsigned slot = 0; slot < 3; ++slot)
        if (ctx->footer_buttons[slot]) ++count;
    int32_t cell_width = count ? (width - 12 * ((int32_t)count - 1)) / (int32_t)count : width;
    if (cell_width < 1) cell_width = 1;
    unsigned column = 0;
    int32_t height = 48;
    for (unsigned slot = 0; slot < 3; ++slot) {
        lv_obj_t *button = ctx->footer_buttons[slot];
        if (!button) continue;
        /* Percent widths resolve against the whole footer, not its Grid cell. */
        lv_obj_t *label = lv_obj_get_child(button, 0);
        const lv_font_t *font = lv_obj_get_style_text_font(label, 0);
        lv_point_t natural, wrapped;
        int32_t pad = lv_obj_get_style_pad_left(button, 0) + lv_obj_get_style_pad_right(button, 0)
            + 2 * lv_obj_get_style_border_width(button, 0);
        lv_text_get_size(&natural, lv_label_get_text(label), font,
            lv_obj_get_style_text_letter_space(label, 0), lv_obj_get_style_text_line_space(label, 0),
            LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        int32_t requested = ctx->footer_widths[slot];
        if (requested < natural.x + pad) requested = natural.x + pad;
        int32_t actual = requested < cell_width ? requested : cell_width;
        lv_obj_set_width(button, actual);
        lv_text_get_size(&wrapped, lv_label_get_text(label), font,
            lv_obj_get_style_text_letter_space(label, 0), lv_obj_get_style_text_line_space(label, 0),
            actual > pad ? actual - pad : 1, LV_TEXT_FLAG_NONE);
        int32_t needed = wrapped.y + lv_obj_get_style_pad_top(button, 0)
            + lv_obj_get_style_pad_bottom(button, 0) + 2 * lv_obj_get_style_border_width(button, 0);
        if (needed > height) height = needed;
        ctx->footer_columns[column] = LV_GRID_FR(1);
        lv_grid_align_t align = slot == UI_POPUP_FOOTER_LEFT ? LV_GRID_ALIGN_START :
            slot == UI_POPUP_FOOTER_RIGHT ? LV_GRID_ALIGN_END : LV_GRID_ALIGN_CENTER;
        lv_obj_set_grid_cell(button, align, (int32_t)column++, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    }
    ctx->footer_columns[column] = LV_GRID_TEMPLATE_LAST;
    ctx->footer_rows[0] = height;
    ctx->footer_rows[1] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_height(ctx->footer, height);
    for (unsigned slot = 0; slot < 3; ++slot)
        if (ctx->footer_buttons[slot]) lv_obj_set_height(ctx->footer_buttons[slot], height);
    lv_obj_set_grid_dsc_array(ctx->footer, ctx->footer_columns, ctx->footer_rows);
}
static void popup_size_cb(lv_event_t *event)
{
    footer_layout(lv_event_get_user_data(event));
}

lv_obj_t *ui_popup_add_footer_action(lv_obj_t *popup,
                                     ui_popup_action_t action,
                                     const char *text,
                                     int32_t width,
                                     ui_popup_footer_slot_t slot,
                                     lv_event_cb_t event_cb,
                                     void *user_data,
                                     lv_obj_t **label_out)
{
    ui_popup_modal_ctx_t *ctx = modal_context(popup);
    if (!ctx || slot < UI_POPUP_FOOTER_LEFT || slot > UI_POPUP_FOOTER_RIGHT || ctx->footer_buttons[slot]) {
        lv_align_t align = slot == UI_POPUP_FOOTER_LEFT ? LV_ALIGN_BOTTOM_LEFT :
            slot == UI_POPUP_FOOTER_RIGHT ? LV_ALIGN_BOTTOM_RIGHT : LV_ALIGN_BOTTOM_MID;
        int32_t x = slot == UI_POPUP_FOOTER_LEFT ? 24 : slot == UI_POPUP_FOOTER_RIGHT ? -24 : 0;
        return ui_popup_add_action_aligned(popup, action, text, width, 48, align, x, -12,
            event_cb, user_data, label_out);
    }
    if (!ctx->footer) {
        ctx->footer = lv_obj_create(popup);
        lv_obj_remove_style_all(ctx->footer);
        lv_obj_remove_flag(ctx->footer, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_column(ctx->footer, 12, 0);
        lv_obj_add_event_cb(popup, popup_size_cb, LV_EVENT_SIZE_CHANGED, ctx);
    }
    lv_obj_t *button = ui_button_create(ctx->footer, ui_popup_action_button_kind(action), text);
    if (!button) return NULL;
    ctx->footer_buttons[slot] = button;
    ctx->footer_widths[slot] = width > 0 ? width : 1;
    lv_obj_set_height(button, 48);
    lv_obj_t *label = lv_obj_get_child(button, 0);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    if (label_out) *label_out = label;
    if (event_cb) lv_obj_add_event_cb(button, event_cb, LV_EVENT_CLICKED, user_data);
    footer_layout(ctx);
    return button;
}

lv_obj_t *ui_popup_add_form_grid(lv_obj_t *popup, int32_t x, int32_t y,
    int32_t width, int32_t height)
{
    lv_obj_t *form = ui_popup_add_list(popup, x, y, width, height);
    if (!form) return NULL;
    static const int32_t columns[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t rows[] = {
        LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
        LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
        LV_GRID_TEMPLATE_LAST
    };
    lv_obj_set_style_pad_all(form, 12, 0);
    lv_obj_set_style_pad_column(form, 16, 0);
    lv_obj_set_style_pad_row(form, 16, 0);
    lv_obj_set_grid_dsc_array(form, columns, rows);
    return form;
}
lv_obj_t *ui_popup_add_form_cell(lv_obj_t *form, unsigned index)
{
    if (!form || index >= 16) return NULL;
    lv_obj_t *cell = lv_obj_create(form);
    lv_obj_remove_style_all(cell);
    lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(cell, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cell, 8, 0);
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, (int32_t)(index % 2), 1,
        LV_GRID_ALIGN_START, (int32_t)(index / 2), 1);
    return cell;
}


lv_obj_t *ui_popup_add_action_at(lv_obj_t *popup,
                                 ui_popup_action_t action,
                                 const char *text,
                                 int32_t x,
                                 int32_t y,
                                 int32_t width,
                                 int32_t height,
                                 lv_event_cb_t event_cb,
                                 void *user_data,
                                 lv_obj_t **label_out)
{
    return ui_popup_add_button_at(
        popup,
        text,
        x,
        y,
        width,
        height,
        ui_popup_action_button_kind(action),
        event_cb,
        user_data,
        label_out);
}


lv_obj_t *ui_popup_add_action_aligned(lv_obj_t *popup,
                                      ui_popup_action_t action,
                                      const char *text,
                                      int32_t width,
                                      int32_t height,
                                      lv_align_t align,
                                      int32_t x_offset,
                                      int32_t y_offset,
                                      lv_event_cb_t event_cb,
                                      void *user_data,
                                      lv_obj_t **label_out)
{
    return ui_popup_add_button_aligned(
        popup,
        text,
        width,
        height,
        align,
        x_offset,
        y_offset,
        ui_popup_action_button_kind(action),
        event_cb,
        user_data,
        label_out);
}


lv_obj_t *ui_popup_add_button_at(lv_obj_t *popup,
                                 const char *text,
                                 int32_t x,
                                 int32_t y,
                                 int32_t width,
                                 int32_t height,
                                 ui_button_kind_t kind,
                                 lv_event_cb_t event_cb,
                                 void *user_data,
                                 lv_obj_t **label_out)
{
    if (!popup) return NULL;

    lv_obj_t *button =
        ui_button_create(
            popup,
            kind,
            text);

    if (!button) {
        return NULL;
    }

    lv_obj_set_size(button, width, height);
    lv_obj_set_pos(button, x, y);

    if (event_cb) {
        lv_obj_add_event_cb(
            button,
            event_cb,
            LV_EVENT_CLICKED,
            user_data);
    }

    if (label_out) {
        *label_out =
            lv_obj_get_child(button, 0);
    }

    return button;
}

lv_obj_t *ui_popup_add_button_aligned(lv_obj_t *popup,
                                      const char *text,
                                      int32_t width,
                                      int32_t height,
                                      lv_align_t align,
                                      int32_t x_offset,
                                      int32_t y_offset,
                                      ui_button_kind_t kind,
                                      lv_event_cb_t event_cb,
                                      void *user_data,
                                      lv_obj_t **label_out)
{
    if (!popup) return NULL;

    lv_obj_t *button =
        ui_button_create(
            popup,
            kind,
            text);

    if (!button) {
        return NULL;
    }

    lv_obj_set_size(button, width, height);
    lv_obj_align(
        button,
        align,
        x_offset,
        y_offset);

    if (event_cb) {
        lv_obj_add_event_cb(
            button,
            event_cb,
            LV_EVENT_CLICKED,
            user_data);
    }

    if (label_out) {
        *label_out =
            lv_obj_get_child(button, 0);
    }

    return button;
}
