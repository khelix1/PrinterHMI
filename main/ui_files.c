#include "ui_files.h"
#include "ui_text.h"
#include "ui_page_layout_profile.h"

#include "lvgl.h"
#include "ui_theme.h"
#include "ui_page_title.h"
#include "ui_button.h"
#include "ui_popup.h"
#include "ui_widgets.h"
#include "ui_page_state.h"
#include "ui_page_geometry.h"
#include "ui_preview_lightbox.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static lv_obj_t *s_printer_file_popup = NULL;
static lv_obj_t *s_printer_file_popup_label = NULL;
static lv_obj_t *s_printer_file_list = NULL;
static ui_page_state_t *s_files_state = NULL;

static ui_files_refresh_cb_t s_refresh_cb = NULL;
static ui_files_select_cb_t s_select_cb = NULL;
static ui_files_preview_cb_t s_preview_cb = NULL;
static ui_files_search_cb_t s_search_cb = NULL;
static ui_files_action_cb_t s_sort_cb = NULL;
static ui_files_folder_cb_t s_folder_cb = NULL;
static ui_files_action_cb_t s_up_cb = NULL;
static lv_obj_t *s_breadcrumb_label = NULL;
static lv_obj_t *s_sort_label = NULL;
static lv_obj_t *s_search_label = NULL;
static lv_obj_t *s_search_popup = NULL;
static lv_obj_t *s_search_textarea = NULL;
static char s_search_text[64];
static lv_timer_t *s_files_refresh_timer = NULL;
static bool s_files_refresh_pending = false;

typedef struct file_row_context {
    struct file_row_context *next;
    lv_obj_t *button;
    lv_obj_t *preview_frame;
    lv_obj_t *file_icon;
    lv_obj_t *preview_image;
    char *path;
    bool preview_requested;
    bool suppress_click;
} file_row_context_t;

static file_row_context_t *s_file_rows = NULL;

static void file_preview_clicked_cb(lv_event_t *event)
{
    file_row_context_t *row =
        (file_row_context_t *)lv_event_get_user_data(event);

    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !row) {
        return;
    }

    lv_event_stop_bubbling(event);

    if (row->preview_image) {
        ui_preview_lightbox_show_object(
            row->preview_image);
    }
}

lv_obj_t *ui_files_get_popup(void)
{
    return s_printer_file_popup;
}

void ui_files_set_status(const char *text)
{
    if (!s_files_state) return;

    const char *message = text ? text : "";
    if (strstr(message, "Loading")) {
        ui_page_state_show(s_files_state,
                               UI_PAGE_STATE_LOADING,
                               "LOADING FILES",
                               "Requesting the G-code library from Moonraker.");
    } else if (strstr(message, "No files")) {
        ui_page_state_show(s_files_state,
                               UI_PAGE_STATE_EMPTY,
                               "NO FILES FOUND",
                               "Upload G-code through Moonraker, then refresh.");
    } else if (strstr(message, "host") ||
               strstr(message, "WiFi") ||
               strstr(message, "offline")) {
        ui_page_state_show(s_files_state,
                               UI_PAGE_STATE_OFFLINE,
                               "FILES OFFLINE",
                               message);
    } else if (message[0]) {
        ui_page_state_show(s_files_state,
                               UI_PAGE_STATE_ERROR,
                               "FILES UNAVAILABLE",
                               message);
    } else {
        ui_page_state_hide(s_files_state);
    }
}

static void files_refresh_deferred_cb(lv_timer_t *timer)
{
    s_files_refresh_timer = NULL;
    lv_timer_delete(timer);

    /*
     * Run the blocking Moonraker request only after the UI has returned to
     * LVGL. The current page stays visible and is replaced in place when
     * the result arrives.
     */
    if (s_refresh_cb) {
        s_refresh_cb();
    }
    s_files_refresh_pending = false;
}

static void files_refresh_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED ||
        s_files_refresh_pending) {
        return;
    }

    s_files_refresh_pending = true;
    ui_files_set_status("Loading files...");
    s_files_refresh_timer = lv_timer_create(
        files_refresh_deferred_cb, 1, NULL);
    if (!s_files_refresh_timer) {
        s_files_refresh_pending = false;
        ui_files_set_status("Files refresh could not start.");
    }
}

static bool file_row_is_visible(const file_row_context_t *row)
{
    if (!row || !row->button || !s_printer_file_list) return false;

    lv_area_t row_area;
    lv_area_t list_area;
    lv_obj_get_coords(row->button, &row_area);
    lv_obj_get_coords(s_printer_file_list, &list_area);

    return row_area.y2 >= list_area.y1 &&
           row_area.y1 <= list_area.y2;
}

static void request_visible_file_previews(void)
{
    if (!s_preview_cb) return;

    for (file_row_context_t *row = s_file_rows;
         row;
         row = row->next) {
        if (!row->preview_requested && file_row_is_visible(row)) {
            row->preview_requested = true;
            s_preview_cb(row->path);
        }
    }
}

static void file_list_scroll_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SCROLL || code == LV_EVENT_SCROLL_END) {
        request_visible_file_previews();
    }
}

static void file_row_event_cb(lv_event_t *e)
{
    file_row_context_t *row =
        (file_row_context_t *)lv_event_get_user_data(e);

    if (!row) return;

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_DELETE) {
        file_row_context_t **link = &s_file_rows;
        while (*link && *link != row) link = &(*link)->next;
        if (*link == row) *link = row->next;

        free(row->path);
        free(row);
        return;
    }

    if (code == LV_EVENT_LONG_PRESSED) {
        row->suppress_click = true;
        if (s_select_cb) s_select_cb(row->path);
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        row->suppress_click = false;
        return;
    }

    if (code != LV_EVENT_CLICKED) return;

    if (row->suppress_click) {
        row->suppress_click = false;
        return;
    }

    if (s_select_cb) s_select_cb(row->path);
}

void ui_files_add_file_entry(const char *path,
                                 double size,
                                 double modified,
                                 int y)
{
    if (!s_printer_file_popup || !path || !path[0]) {
        return;
    }

    ui_page_state_hide(s_files_state);

    lv_obj_t *parent =
        s_printer_file_list
            ? s_printer_file_list
            : s_printer_file_popup;

    /*
     * TEST74_FILES_OPERATOR_LIST_ROW
     *
     * Files is a browser rather than a card dashboard. Each file is
     * therefore rendered as a flat full-width list entry with a single
     * restrained separator instead of an enclosed card border.
     *
     * Selection behavior and row placement remain unchanged.
     */
    lv_obj_t *btn =
        ui_button_create_empty(
            parent,
            UI_BUTTON_OUTLINED);

    if (!btn) {
        return;
    }

    const int32_t row_height =
        ui_theme_density_metric(76, 92, 104);
    const int32_t preview_size =
        ui_theme_density_metric(54, 66, 76);
    const int32_t text_x =
        ui_theme_density_metric(82, 94, 108);

    lv_obj_set_size(
        btn,
        790,
        row_height);

    lv_obj_set_pos(
        btn,
        10,
        y);

    lv_obj_clear_flag(
        btn,
        LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(btn, UI_SURFACE_LIST_ROW);

    /*
     * A bottom-only border gives the page a continuous file-browser
     * appearance without turning every row into a separate card.
     */
    lv_obj_set_style_border_width(
        btn,
        0,
        LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_border_side(
        btn,
        LV_BORDER_SIDE_BOTTOM,
        LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_border_width(
        btn,
        UI_BORDER_THIN,
        LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_pad_all(
        btn,
        0,
        0);

    /*
     * Restrained pressed feedback. The row remains part of the same
     * continuous page surface while still clearly acknowledging touch.
     */
    file_row_context_t *row =
        calloc(1, sizeof(*row));

    if (!row) {
        lv_obj_delete(btn);
        return;
    }

    row->path = strdup(path);
    if (!row->path) {
        free(row);
        lv_obj_delete(btn);
        return;
    }

    row->button = btn;
    row->next = s_file_rows;
    s_file_rows = row;

    lv_obj_add_event_cb(
        btn,
        file_row_event_cb,
        LV_EVENT_ALL,
        row);

    /*
     * Preview well. It starts with the cyan file glyph; the lazy Files-row
     * preview worker replaces only this glyph when Moonraker metadata exposes
     * a thumbnail.
     */
    row->preview_frame = lv_obj_create(btn);
    lv_obj_set_size(row->preview_frame, preview_size, preview_size);
    lv_obj_align(
        row->preview_frame,
        LV_ALIGN_LEFT_MID,
        ui_theme_density_metric(10, 12, 14),
        0);
    lv_obj_clear_flag(row->preview_frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row->preview_frame, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(row->preview_frame, 0, 0);
    ui_apply_surface_role(row->preview_frame, UI_SURFACE_PREVIEW_WELL);
    lv_obj_add_event_cb(
        row->preview_frame,
        file_preview_clicked_cb,
        LV_EVENT_CLICKED,
        row);

    lv_obj_t *icon =
        lv_label_create(row->preview_frame);

    row->file_icon = icon;

    lv_label_set_text(
        icon,
        LV_SYMBOL_FILE);

    ui_apply_custom_label_style(icon,
                                &lv_font_montserrat_28,
                                UI_ACCENT_CYAN);

    lv_obj_align(
        icon,
        LV_ALIGN_CENTER,
        0,
        0);

    /*
     * Primary filename.
     */
    lv_obj_t *label =
        lv_label_create(btn);

    const char *display_name = strrchr(path, '/');
    display_name = display_name ? display_name + 1 : path;
    lv_label_set_text(label, display_name);

    lv_obj_set_width(
        label,
        600);

    lv_label_set_long_mode(
        label,
        LV_LABEL_LONG_DOT);

    ui_apply_text_body(label);

    ui_apply_label_bright(label);

    lv_obj_align(
        label,
        LV_ALIGN_LEFT_MID,
        text_x,
        ui_theme_density_metric(-10, -13, -15));

    /*
     * Secondary action hint. Metadata is not added here because the
     * current file-list API supplies only the path.
     */
    lv_obj_t *meta =
        lv_label_create(btn);

    char meta_text[128];
    char date_text[32] = "";
    if (modified > 0.0) {
        time_t timestamp = (time_t)modified;
        struct tm local_time;
        if (localtime_r(&timestamp, &local_time)) {
            strftime(date_text, sizeof(date_text), "%b %d  %I:%M %p", &local_time);
        }
    }
    if (size > 0.0) {
        snprintf(meta_text,
                 sizeof(meta_text),
                 "%.1f MB  |  %s  |  HOLD FOR DETAILS",
                 size / (1024.0 * 1024.0),
                 date_text[0] ? date_text : "DATE --");
    } else {
        snprintf(meta_text, sizeof(meta_text), "G-CODE FILE  |  HOLD FOR DETAILS");
    }
    lv_label_set_text(meta, meta_text);

    lv_obj_set_width(
        meta,
        600);

    lv_label_set_long_mode(
        meta,
        LV_LABEL_LONG_DOT);

    ui_apply_custom_label_style(meta,
                                &lv_font_montserrat_12,
                                UI_TEXT_DIM);

    lv_obj_align(
        meta,
        LV_ALIGN_LEFT_MID,
        text_x,
        ui_theme_density_metric(13, 17, 20));

    /*
     * Right-side navigation indicator.
     */
    lv_obj_t *arrow =
        lv_label_create(btn);

    lv_label_set_text(
        arrow,
        LV_SYMBOL_RIGHT);

    ui_apply_custom_label_style(arrow,
                                &lv_font_montserrat_24,
                                UI_ACCENT_CYAN);

    lv_obj_align(
        arrow,
        LV_ALIGN_RIGHT_MID,
        -22,
        0);

    /* The first screenful starts loading immediately; later rows are lazy. */
    if (y < 422) {
        row->preview_requested = true;
        if (s_preview_cb) s_preview_cb(row->path);
    }
}

void ui_files_add_file_button(const char *path, int y)
{
    ui_files_add_file_entry(path, 0.0, 0.0, y);
}

typedef struct {
    char *path;
} folder_event_data_t;

static void folder_event_cb(lv_event_t *event)
{
    folder_event_data_t *data = lv_event_get_user_data(event);
    if (!data) return;
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && s_folder_cb) {
        s_folder_cb(data->path);
    } else if (lv_event_get_code(event) == LV_EVENT_DELETE) {
        free(data->path);
        free(data);
    }
}

void ui_files_add_folder_button(const char *name,
                                    const char *path,
                                    int y)
{
    if (!s_printer_file_list || !name || !path) return;
    lv_obj_t *button = ui_button_create_empty(s_printer_file_list, UI_BUTTON_OUTLINED);
    if (!button) return;
    lv_obj_set_size(
        button,
        790,
        ui_theme_density_metric(54, 64, 76));
    lv_obj_set_pos(button, 10, y);
    ui_apply_surface_role(button, UI_SURFACE_LIST_ROW);

    folder_event_data_t *data = calloc(1, sizeof(*data));
    if (data) data->path = strdup(path);
    if (!data || !data->path) {
        if (data) free(data);
        lv_obj_delete(button);
        return;
    }
    lv_obj_add_event_cb(button, folder_event_cb, LV_EVENT_ALL, data);

    lv_obj_t *label = lv_label_create(button);
    char text[190];
    snprintf(text, sizeof(text), LV_SYMBOL_DIRECTORY "  %s", name);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, 700);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    ui_apply_custom_label_style(label,
                                UI_FONT_BODY_LARGE,
                                UI_ACCENT_CYAN);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 18, 0);
}

void ui_files_clear_rows(void)
{
    if (s_printer_file_list) lv_obj_clean(s_printer_file_list);
    s_file_rows = NULL;
}

void ui_files_set_breadcrumb(const char *path)
{
    if (!s_breadcrumb_label) return;
    char text[190];
    snprintf(text, sizeof(text), "GCODE / %s", path && path[0] ? path : "ROOT");
    lv_label_set_text(s_breadcrumb_label, text);
}

void ui_files_set_sort_text(const char *text)
{
    if (s_sort_label) lv_label_set_text(s_sort_label, text ? text : ui_text("SORT"));
}

void ui_files_set_search_text(const char *text)
{
    snprintf(s_search_text, sizeof(s_search_text), "%s", text ? text : "");

    if (s_search_label) {
        lv_label_set_text(s_search_label,
                          s_search_text[0] ? ui_text("SEARCH*") : ui_text("SEARCH"));
    }

    if (s_search_text[0] && s_breadcrumb_label) {
        char breadcrumb[190];
        snprintf(breadcrumb,
                 sizeof(breadcrumb),
                 "SEARCH / %.170s",
                 s_search_text);
        lv_label_set_text(s_breadcrumb_label, breadcrumb);
    }
}

static void close_search_popup(void)
{
    if (s_search_popup) lv_obj_delete(s_search_popup);
    s_search_popup = NULL;
    s_search_textarea = NULL;
}

static void search_popup_deleted_cb(lv_event_t *event)
{
    if (event && lv_event_get_target(event) == s_search_popup) {
        s_search_popup = NULL;
        s_search_textarea = NULL;
    }
}

static void apply_search_and_close(bool async_close)
{
    char query[sizeof(s_search_text)];
    snprintf(query,
             sizeof(query),
             "%s",
             s_search_textarea
                 ? lv_textarea_get_text(s_search_textarea)
                 : "");

    lv_obj_t *popup = s_search_popup;
    s_search_popup = NULL;
    s_search_textarea = NULL;
    if (popup) {
        if (async_close) lv_obj_delete_async(popup);
        else lv_obj_delete(popup);
    }

    if (s_search_cb) s_search_cb(query);
}

static void search_keyboard_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY) {
        apply_search_and_close(true);
    } else if (code == LV_EVENT_CANCEL) {
        lv_obj_t *popup = s_search_popup;
        s_search_popup = NULL;
        s_search_textarea = NULL;
        if (popup) lv_obj_delete_async(popup);
    }
}

static void search_apply_button_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        apply_search_and_close(false);
    }
}

static void search_cancel_button_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) close_search_popup();
}

static void search_button_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    close_search_popup();
    s_search_popup = ui_popup_create(
        lv_layer_top(), 760, 500, UI_POPUP_STANDARD);
    if (!s_search_popup) return;
    lv_obj_add_event_cb(s_search_popup,
                        search_popup_deleted_cb,
                        LV_EVENT_DELETE,
                        NULL);

    ui_popup_add_title(s_search_popup, ui_text("SEARCH FILES"), false, 8);
    ui_popup_add_header_divider(s_search_popup, 44);

    s_search_textarea = ui_popup_add_textarea(
        s_search_popup,
        700,
        56,
        LV_ALIGN_TOP_MID,
        0,
        58,
        true,
        false,
        sizeof(s_search_text) - 1,
        ui_text("Search filenames and folders"),
        s_search_text,
        NULL);

    lv_obj_t *keyboard = ui_popup_add_keyboard(
        s_search_popup,
        s_search_textarea,
        700,
        300,
        LV_ALIGN_TOP_MID,
        0,
        126,
        LV_KEYBOARD_MODE_TEXT_LOWER);

    if (keyboard) {
        lv_obj_add_event_cb(keyboard,
                            search_keyboard_cb,
                            LV_EVENT_READY,
                            NULL);
        lv_obj_add_event_cb(keyboard,
                            search_keyboard_cb,
                            LV_EVENT_CANCEL,
                            NULL);
    }

    ui_popup_add_standard_footer_divider(s_search_popup);
    ui_popup_add_footer_action(
        s_search_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        search_cancel_button_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s_search_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_OK " SEARCH",
        170,
        UI_POPUP_FOOTER_RIGHT,
        search_apply_button_cb,
        NULL,
        NULL);

    if (s_search_textarea) {
        lv_obj_add_state(s_search_textarea, LV_STATE_FOCUSED);
    }
    lv_obj_move_foreground(s_search_popup);
}

static void sort_button_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && s_sort_cb) s_sort_cb();
}

static void up_button_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && s_up_cb) s_up_cb();
}

void ui_files_set_file_thumbnail(
    const char *path,
    const lv_image_dsc_t *image)
{
    if (!path || !image) return;

    for (file_row_context_t *row = s_file_rows;
         row;
         row = row->next) {
        if (strcmp(row->path, path) != 0 || !row->preview_frame) {
            continue;
        }

        if (row->file_icon) {
            lv_obj_add_flag(row->file_icon, LV_OBJ_FLAG_HIDDEN);
        }

        if (!row->preview_image) {
            row->preview_image = lv_image_create(row->preview_frame);
        }

        lv_image_set_src(row->preview_image, image);
        ui_thumbnail_fit_object(
            row->preview_image,
            row->preview_frame,
            (int)image->header.w,
            (int)image->header.h,
            2);
        return;
    }
}

void ui_files_show(void)
{
    if (s_printer_file_popup) {
        lv_obj_move_foreground(
            s_printer_file_popup);
        return;
    }

    const ui_files_layout_profile_t *layout =
        &ui_page_layout_profile_current()->files;

    /*
     * TEST71_FILES_THEME_B_SHELL
     *
     * Match the shared Operator page geometry used by Dashboard,
     * Drybox and Printer.
     */
    s_printer_file_popup =
        lv_obj_create(
            lv_screen_active());

    if (!s_printer_file_popup) {
        return;
    }

    lv_obj_set_size(
        s_printer_file_popup,
        UI_PAGE_ROOT_WIDTH,
        UI_PAGE_ROOT_HEIGHT);

    lv_obj_set_pos(
        s_printer_file_popup,
        UI_PAGE_ROOT_X,
        UI_PAGE_ROOT_Y);

    lv_obj_clear_flag(
        s_printer_file_popup,
        LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_root_style(
        s_printer_file_popup);

    ui_page_title_create(
        s_printer_file_popup,
        LV_SYMBOL_FILE " FILES",
        layout->subtitle);

    s_breadcrumb_label = lv_label_create(s_printer_file_popup);
    lv_obj_set_width(
        s_breadcrumb_label,
        layout->breadcrumb.width);
    lv_label_set_long_mode(s_breadcrumb_label, LV_LABEL_LONG_DOT);
    ui_apply_text_caption(s_breadcrumb_label);
    ui_apply_label_dim(s_breadcrumb_label);
    lv_obj_set_pos(
        s_breadcrumb_label,
        layout->breadcrumb.x,
        layout->breadcrumb.y);
    ui_files_set_breadcrumb(NULL);

    lv_obj_t *up = ui_button_create_icon(
        s_printer_file_popup, UI_BUTTON_OUTLINED,
        LV_SYMBOL_UP, "UP", UI_ACCENT_CYAN, UI_BUTTON_ICON_HORIZONTAL);
    if (up) {
        lv_obj_set_size(
            up,
            layout->up.width,
            layout->up.height);
        lv_obj_set_pos(
            up,
            layout->up.x,
            layout->up.y);
        lv_obj_add_event_cb(up, up_button_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *search = ui_button_create_icon(
        s_printer_file_popup, UI_BUTTON_OUTLINED,
        LV_SYMBOL_EDIT, "SEARCH", UI_ACCENT_CYAN, UI_BUTTON_ICON_HORIZONTAL);
    if (search) {
        lv_obj_set_size(
            search,
            layout->search.width,
            layout->search.height);
        lv_obj_set_pos(
            search,
            layout->search.x,
            layout->search.y);
        s_search_label = lv_obj_get_child(search, 1);
        ui_files_set_search_text(s_search_text);
        lv_obj_add_event_cb(search, search_button_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *sort = ui_button_create_icon(
        s_printer_file_popup, UI_BUTTON_OUTLINED,
        LV_SYMBOL_LIST, "NAME", UI_ACCENT_CYAN, UI_BUTTON_ICON_HORIZONTAL);
    if (sort) {
        lv_obj_set_size(
            sort,
            layout->sort.width,
            layout->sort.height);
        lv_obj_set_pos(
            sort,
            layout->sort.x,
            layout->sort.y);
        s_sort_label = lv_obj_get_child(sort, 1);
        lv_obj_add_event_cb(sort, sort_button_cb, LV_EVENT_CLICKED, NULL);
    }

    /*
     * Shared Operator outlined refresh action.
     */
    lv_obj_t *refresh =
        ui_button_create_icon(
            s_printer_file_popup,
            UI_BUTTON_OUTLINED,
            LV_SYMBOL_REFRESH,
            "REFRESH",
            UI_ACCENT_CYAN,
            UI_BUTTON_ICON_HORIZONTAL);

    if (refresh) {
        lv_obj_set_size(
            refresh,
            layout->refresh.width,
            layout->refresh.height);

        lv_obj_set_pos(
            refresh,
            layout->refresh.x,
            layout->refresh.y);

        lv_obj_add_event_cb(
            refresh,
            files_refresh_event_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    /*
     * TEST73_FILES_FULL_PAGE_LIST
     *
     * Files is a full-page browser, not a bordered card.
     * Keep the list surface transparent and explicitly scrollable.
     */
    s_printer_file_list =
        lv_obj_create(
            s_printer_file_popup);

    if (!s_printer_file_list) {
        lv_obj_delete(
            s_printer_file_popup);

        s_printer_file_popup = NULL;
        return;
    }

    lv_obj_set_size(
        s_printer_file_list,
        layout->list.width,
        layout->list.height);

    lv_obj_set_pos(
        s_printer_file_list,
        layout->list.x,
        layout->list.y);

    ui_apply_surface_role(s_printer_file_list, UI_SURFACE_TRANSPARENT);

    lv_obj_set_style_pad_row(
        s_printer_file_list,
        ui_theme_density_metric(10, 14, 18),
        0);

    lv_obj_add_flag(
        s_printer_file_list,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_scroll_dir(
        s_printer_file_list,
        LV_DIR_VER);

    lv_obj_set_scrollbar_mode(
        s_printer_file_list,
        LV_SCROLLBAR_MODE_AUTO);

    lv_obj_add_event_cb(
        s_printer_file_list,
        file_list_scroll_event_cb,
        LV_EVENT_ALL,
        NULL);

    s_files_state = ui_page_state_create(
        s_printer_file_popup,
        layout->list.x,
        layout->list.y,
        layout->list.width,
        layout->list.height);

    /*
     * Let LVGL draw the Files page before its synchronous Moonraker request.
     * This prevents a blank/late page transition on slower responses.
     */
    if (s_refresh_cb && !s_files_refresh_timer) {
        s_files_refresh_pending = true;
        ui_files_set_status("Loading files...");
        s_files_refresh_timer = lv_timer_create(
            files_refresh_deferred_cb, 20, NULL);
        if (!s_files_refresh_timer) {
            s_files_refresh_pending = false;
            ui_files_set_status("Files refresh could not start.");
        }
    }
}

void ui_files_hide(void)
{
    if (s_files_refresh_timer) {
        lv_timer_delete(s_files_refresh_timer);
        s_files_refresh_timer = NULL;
    }
    s_files_refresh_pending = false;
    close_search_popup();
    if (s_printer_file_popup) {
        lv_obj_delete(s_printer_file_popup);
        s_printer_file_popup = NULL;
        s_printer_file_popup_label = NULL;
        s_printer_file_list = NULL;
        s_files_state = NULL;
        s_breadcrumb_label = NULL;
        s_sort_label = NULL;
        s_search_label = NULL;
    }
}

void ui_files_set_browser_callbacks(
    ui_files_search_cb_t search_cb,
    ui_files_action_cb_t sort_cb,
    ui_files_folder_cb_t folder_cb,
    ui_files_action_cb_t up_cb)
{
    s_search_cb = search_cb;
    s_sort_cb = sort_cb;
    s_folder_cb = folder_cb;
    s_up_cb = up_cb;
}

void ui_files_refresh(void)
{
    ui_files_hide();
    ui_files_show();

    if (s_refresh_cb) {
        s_refresh_cb();
    }
}

void ui_files_set_callbacks(ui_files_refresh_cb_t refresh_cb,
                                ui_files_select_cb_t select_cb,
                                ui_files_preview_cb_t preview_cb)
{
    s_refresh_cb = refresh_cb;
    s_select_cb = select_cb;
    s_preview_cb = preview_cb;
}


/* File detail popup */

static lv_obj_t *s_file_detail_popup = NULL;
static ui_files_detail_cb_t s_detail_cancel_cb = NULL;
static ui_files_detail_cb_t s_detail_start_cb = NULL;
static lv_obj_t *s_detail_info_label = NULL;
static lv_obj_t *s_detail_start_button = NULL;

bool ui_files_detail_is_open(void)
{
    return s_file_detail_popup != NULL;
}

void ui_files_close_detail_popup(void)
{
    if (s_file_detail_popup) {
        lv_obj_delete(s_file_detail_popup);
        s_file_detail_popup = NULL;
        s_detail_info_label = NULL;
        s_detail_start_button = NULL;
    }
}

static void detail_cancel_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_detail_cancel_cb) {
        s_detail_cancel_cb();
    } else {
        ui_files_close_detail_popup();
    }
}

static void detail_start_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_detail_start_cb) {
        s_detail_start_cb();
    }
}

void ui_files_show_detail_popup(const char *filename_text,
                                    const char *metadata_text,
                                    lv_obj_t **thumb_box_out,
                                    ui_thumbnail_t **thumb_view_out,
                                    ui_files_detail_cb_t cancel_cb,
                                    ui_files_detail_cb_t start_cb)
{
    ui_files_close_detail_popup();

    s_detail_cancel_cb = cancel_cb;
    s_detail_start_cb = start_cb;

    if (thumb_box_out) {
        *thumb_box_out = NULL;
    }

    if (thumb_view_out) {
        *thumb_view_out = NULL;
    }

    /*
     * TEST79_FILES_SHARED_PREVIEW_LONG_FILENAME
     *
     * The popup now creates the same shared ui_thumbnail component
     * used by the rest of the application. The popup does not allocate
     * or own a separate image buffer.
     */
    s_file_detail_popup =
        ui_popup_create(
            lv_screen_active(),
            760,
            500,
            UI_POPUP_STANDARD);

    if (!s_file_detail_popup) {
        return;
    }

    /*
     * Header.
     */
    lv_obj_t *title =
        lv_label_create(
            s_file_detail_popup);

    lv_label_set_text(
        title,
        ui_text("FILE READY TO PRINT"));

    ui_apply_text_title(title);

    ui_apply_label_bright(title);

    lv_obj_set_pos(
        title,
        28,
        20);

    lv_obj_t *subtitle =
        lv_label_create(
            s_file_detail_popup);

    lv_label_set_text(
        subtitle,
        ui_text("Review the selected file before starting the print."));

    ui_apply_text_caption(subtitle);

    ui_apply_label_dim(subtitle);

    lv_obj_set_pos(
        subtitle,
        28,
        50);

    /*
     * Dedicated filename strip.
     *
     * LV_LABEL_LONG_SCROLL_CIRCULAR remains stationary for filenames
     * that fit and automatically scrolls only when the text overflows.
     */
    lv_obj_t *filename =
        lv_label_create(
            s_file_detail_popup);

    lv_label_set_text(
        filename,
        filename_text && filename_text[0]
            ? filename_text
            : ui_text("Unnamed file"));

    lv_obj_set_size(
        filename,
        704,
        24);

    lv_label_set_long_mode(
        filename,
        LV_LABEL_LONG_SCROLL_CIRCULAR);

    ui_apply_custom_label_style(filename,
                                UI_FONT_BODY,
                                UI_ACCENT_CYAN);

    lv_obj_set_style_text_align(
        filename,
        LV_TEXT_ALIGN_LEFT,
        0);

    lv_obj_set_pos(
        filename,
        28,
        78);

    lv_obj_t *header_line =
        lv_obj_create(
            s_file_detail_popup);

    lv_obj_set_size(
        header_line,
        704,
        1);

    lv_obj_set_pos(
        header_line,
        28,
        108);

    lv_obj_clear_flag(
        header_line,
        LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(header_line, UI_SURFACE_DIVIDER);

    /*
     * Shared thumbnail component.
     */
    ui_thumbnail_t *thumb_view =
        ui_thumbnail_create(
            s_file_detail_popup,
            28,
            128,
            270,
            250);

    if (!thumb_view) {
        lv_obj_delete(
            s_file_detail_popup);

        s_file_detail_popup = NULL;
        return;
    }

    ui_thumbnail_set_placeholder(
        thumb_view,
        "PRINT\nTHUMBNAIL");

    lv_obj_t *thumb_box =
        ui_thumbnail_box(
            thumb_view);

    if (thumb_box_out) {
        *thumb_box_out = thumb_box;
    }

    if (thumb_view_out) {
        *thumb_view_out = thumb_view;
    }

    /*
     * Metadata begins after the generated File section because the
     * filename now has its own dedicated strip above the preview.
     */
    const char *detail_text =
        metadata_text && metadata_text[0]
            ? metadata_text
            : "--";

    if (strncmp(detail_text, "File:\n", 6) == 0) {
        const char *section_end =
            strstr(detail_text, "\n\n");

        if (section_end && section_end[2]) {
            detail_text = section_end + 2;
        }
    }

    lv_obj_t *info_panel =
        ui_create_operator_card(
            s_file_detail_popup,
            318,
            128,
            414,
            250);

    if (!info_panel) {
        lv_obj_delete(
            s_file_detail_popup);

        s_file_detail_popup = NULL;

        if (thumb_box_out) {
            *thumb_box_out = NULL;
        }

        if (thumb_view_out) {
            *thumb_view_out = NULL;
        }

        return;
    }

    ui_create_operator_card_heading(
        info_panel,
        "PRINT DETAILS",
        18,
        14);

    lv_obj_t *info =
        lv_label_create(
            info_panel);

    lv_label_set_text(
        info,
        detail_text);

    lv_obj_set_width(
        info,
        374);

    lv_label_set_long_mode(
        info,
        LV_LABEL_LONG_WRAP);

    ui_apply_text_body(info);

    ui_apply_label_primary(info);

    lv_obj_set_pos(
        info,
        18,
        48);

    s_detail_info_label = info;

    /*
     * Footer.
     */
    ui_popup_add_standard_footer_divider(s_file_detail_popup);

    ui_popup_add_footer_action(
        s_file_detail_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_CLOSE " CANCEL",
        170,
        UI_POPUP_FOOTER_LEFT,
        detail_cancel_event_cb,
        NULL,
        NULL);

    lv_obj_t *start =
        ui_popup_add_footer_action(
            s_file_detail_popup,
            UI_POPUP_ACTION_CONFIRM,
            LV_SYMBOL_PLAY " START PRINT",
            190,
            UI_POPUP_FOOTER_RIGHT,
            detail_start_event_cb,
            NULL,
            NULL);

    if (start) {
        s_detail_start_button = start;
        lv_obj_add_state(start, LV_STATE_DISABLED);
    }

    lv_obj_move_foreground(
        s_file_detail_popup);
}

void ui_files_update_detail_metadata(
    const char *metadata_text,
    bool ready)
{
    if (!s_file_detail_popup || !s_detail_info_label) return;

    const char *detail = metadata_text && metadata_text[0]
        ? metadata_text
        : "Metadata unavailable.";

    if (strncmp(detail, "File:\n", 6) == 0) {
        const char *section_end = strstr(detail, "\n\n");
        if (section_end && section_end[2]) detail = section_end + 2;
    }

    lv_label_set_text(s_detail_info_label, detail);

    if (s_detail_start_button && ready) {
        lv_obj_clear_state(s_detail_start_button, LV_STATE_DISABLED);
    }
}
