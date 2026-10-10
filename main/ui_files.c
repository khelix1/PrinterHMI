#include "ui_files.h"
#include "ui_text.h"
#include "ui_page_layout_profile.h"

#include "lvgl.h"
#include "ui_theme.h"
#include "ui_studio_layout.h"
#include "ui_page_title.h"
#include "ui_button.h"
#include "ui_popup.h"
#include "moonraker_config_controller.h"
#include "ui_toast.h"
#include "ui_widgets.h"
#include "ui_page_state.h"
#include "ui_page_geometry.h"
#include "ui_preview_lightbox.h"
#include "ui_responsive_layout.h"

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
static lv_obj_t *s_studio_file_hint;
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

    if (row->preview_image) {
        lv_event_stop_bubbling(event);
        ui_preview_lightbox_show_file_object(
            row->preview_image,
            row->path);
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
     * Enqueue the file-list worker after the first page draw. HTTP retries
     * run outside LVGL; its result timer updates the current page in place.
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

static void file_row_size_changed(lv_event_t *event)
{
    lv_obj_t *row = lv_event_get_target_obj(event);
    int32_t text_x = (int32_t)(intptr_t)lv_event_get_user_data(event);
    int32_t width = lv_obj_get_width(row) - text_x - 52;
    if (width < 1) width = 1;
    for (uint32_t i = 0; i < lv_obj_get_child_count(row); ++i) {
        lv_obj_t *child = lv_obj_get_child(row, i);
        if (lv_obj_check_type(child, &lv_label_class) &&
            lv_obj_get_style_align(child, 0) == LV_ALIGN_LEFT_MID)
            lv_obj_set_width(child, width);
    }
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
        lv_pct(100),
        row_height);

    lv_obj_set_pos(
        btn,
        0,
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
        1);

    lv_label_set_long_mode(
        label,
        LV_LABEL_LONG_DOT);

    ui_apply_text_body(label);

    ui_apply_label_bright(label);
    lv_obj_set_height(label, lv_font_get_line_height(lv_obj_get_style_text_font(label, 0)));

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
        1);

    lv_label_set_long_mode(
        meta,
        LV_LABEL_LONG_DOT);

    ui_apply_custom_label_style(meta,
                                &lv_font_montserrat_12,
                                UI_TEXT_DIM);
    lv_obj_set_height(meta, lv_font_get_line_height(lv_obj_get_style_text_font(meta, 0)));

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

    lv_obj_add_event_cb(btn, file_row_size_changed, LV_EVENT_SIZE_CHANGED,
                        (void *)(intptr_t)text_x);
    lv_obj_send_event(btn, LV_EVENT_SIZE_CHANGED, NULL);

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
        lv_pct(100),
        ui_theme_density_metric(54, 64, 76));
    lv_obj_set_pos(button, 0, y);
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
    lv_obj_set_width(label, 1);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    ui_apply_custom_label_style(label,
                                UI_FONT_BODY_LARGE,
                                UI_ACCENT_CYAN);
    lv_obj_set_height(label, lv_font_get_line_height(lv_obj_get_style_text_font(label, 0)));
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 18, 0);
    lv_obj_add_event_cb(button, file_row_size_changed, LV_EVENT_SIZE_CHANGED,
                        (void *)(intptr_t)18);
    lv_obj_send_event(button, LV_EVENT_SIZE_CHANGED, NULL);
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

    if(ui_theme_is_studio()) {
        s_printer_file_popup=lv_obj_create(lv_screen_active());
        lv_obj_set_size(s_printer_file_popup,854,528);ui_apply_root_style(s_printer_file_popup);
        studio_text(s_printer_file_popup,"Job library",0,0,580,&ui_studio_font_48,UI_TEXT);
        studio_action(s_printer_file_popup,"Up",0,70,100,44,up_button_cb,NULL);
        lv_obj_t *search=studio_action(s_printer_file_popup,"Search",110,70,148,44,search_button_cb,NULL);
        s_search_label=lv_obj_get_child(search,0);
        lv_obj_t *sort=studio_action(s_printer_file_popup,"Name",268,70,144,44,sort_button_cb,NULL);
        s_sort_label=lv_obj_get_child(sort,0);
        studio_action(s_printer_file_popup,"Refresh",422,70,154,44,files_refresh_event_cb,NULL);
        s_breadcrumb_label=studio_text(s_printer_file_popup,"/gcodes",0,120,576,UI_FONT_CAPTION,UI_TEXT_DIM);
        ui_files_set_breadcrumb(NULL);ui_files_set_search_text(s_search_text);
        lv_obj_t *viewport=studio_plane(s_printer_file_popup,0,150,576,274);
        s_printer_file_list=studio_plane(viewport,0,0,576,274);
        lv_obj_add_flag(s_printer_file_list,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(s_printer_file_list,LV_DIR_VER);
        lv_obj_set_scrollbar_mode(s_printer_file_list,LV_SCROLLBAR_MODE_AUTO);
        lv_obj_add_event_cb(s_printer_file_list,file_list_scroll_event_cb,LV_EVENT_ALL,NULL);
        s_files_state=ui_page_state_create(viewport,0,0,576,274);
        studio_rule(s_printer_file_popup,600,0,1,424);
        s_studio_file_hint=studio_text(s_printer_file_popup,"Select a file to inspect\nits preview and metadata",624,154,352,UI_FONT_BODY_LARGE,UI_TEXT_DIM);
        lv_label_set_long_mode(s_studio_file_hint,LV_LABEL_LONG_WRAP);lv_obj_set_height(s_studio_file_hint,100);
        goto schedule_files_refresh;
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

    lv_obj_set_flex_flow(s_printer_file_popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_printer_file_popup, 20, 0);
    lv_obj_set_style_pad_row(s_printer_file_popup, UI_GAP_ROW, 0);
    lv_obj_t *header = lv_obj_create(s_printer_file_popup);
    ui_responsive_column(header);
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_FILE " FILES");
    ui_apply_text_title(title);
    ui_apply_label_bright(title);
    lv_obj_t *subtitle = lv_label_create(header);
    lv_label_set_text(subtitle, layout->subtitle);
    lv_obj_set_width(subtitle, lv_pct(100));
    ui_apply_text_body(subtitle);
    ui_apply_label_dim(subtitle);
    lv_obj_t *toolbar = lv_obj_create(s_printer_file_popup);
    ui_responsive_column(toolbar);
    lv_obj_set_flex_flow(toolbar, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(toolbar, UI_GAP_ROW, 0);

    lv_obj_t *up = ui_button_create_icon(
        toolbar, UI_BUTTON_OUTLINED,
        LV_SYMBOL_UP, "UP", UI_ACCENT_CYAN, UI_BUTTON_ICON_HORIZONTAL);
    if (up) {
        ui_responsive_action(up);

        lv_obj_add_event_cb(up, up_button_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *search = ui_button_create_icon(
        toolbar, UI_BUTTON_OUTLINED,
        LV_SYMBOL_EDIT, "SEARCH", UI_ACCENT_CYAN, UI_BUTTON_ICON_HORIZONTAL);
    if (search) {
        ui_responsive_action(search);

        s_search_label = lv_obj_get_child(search, 1);
        ui_files_set_search_text(s_search_text);
        lv_obj_add_event_cb(search, search_button_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *sort = ui_button_create_icon(
        toolbar, UI_BUTTON_OUTLINED,
        LV_SYMBOL_LIST, "NAME", UI_ACCENT_CYAN, UI_BUTTON_ICON_HORIZONTAL);
    if (sort) {
        ui_responsive_action(sort);

        s_sort_label = lv_obj_get_child(sort, 1);
        lv_obj_add_event_cb(sort, sort_button_cb, LV_EVENT_CLICKED, NULL);
    }

    /*
     * Shared Operator outlined refresh action.
     */
    lv_obj_t *refresh =
        ui_button_create_icon(
            toolbar,
            UI_BUTTON_OUTLINED,
            LV_SYMBOL_REFRESH,
            "REFRESH",
            UI_ACCENT_CYAN,
            UI_BUTTON_ICON_HORIZONTAL);

    if (refresh) {
        ui_responsive_action(refresh);
        lv_obj_add_event_cb(
            refresh,
            files_refresh_event_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    /* Preserve each theme/custom profile's action order, using native flow. */
    if (layout->refresh.x < layout->up.x) {
        if (refresh) lv_obj_move_to_index(refresh, 0);
        if (sort) lv_obj_move_to_index(sort, 1);
        if (search) lv_obj_move_to_index(search, 2);
    }
    s_breadcrumb_label = lv_label_create(s_printer_file_popup);
    lv_obj_set_width(s_breadcrumb_label, lv_pct(100));
    lv_label_set_long_mode(s_breadcrumb_label, LV_LABEL_LONG_DOT);
    ui_apply_text_caption(s_breadcrumb_label);
    ui_apply_label_dim(s_breadcrumb_label);
    ui_files_set_breadcrumb(NULL);
    ui_files_set_search_text(s_search_text);
    lv_obj_t *viewport = lv_obj_create(s_printer_file_popup);
    ui_apply_surface_role(viewport, UI_SURFACE_TRANSPARENT);
    lv_obj_set_style_pad_all(viewport, 0, 0);
    lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(viewport, lv_pct(100), 0);
    lv_obj_set_flex_grow(viewport, 1);

    /*
     * TEST73_FILES_FULL_PAGE_LIST
     *
     * Files is a full-page browser, not a bordered card.
     * Keep the list surface transparent and explicitly scrollable.
     */
    s_printer_file_list =
        lv_obj_create(
            viewport);

    if (!s_printer_file_list) {
        lv_obj_delete(
            s_printer_file_popup);

        s_printer_file_popup = NULL;
        return;
    }

    lv_obj_set_size(s_printer_file_list, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(s_printer_file_list, 0, 0);

    ui_apply_surface_role(s_printer_file_list, UI_SURFACE_TRANSPARENT);
    lv_obj_set_style_pad_all(s_printer_file_list, 0, 0);

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
        viewport, 0, 0, lv_pct(100), lv_pct(100));

schedule_files_refresh:
    /*
     * Let LVGL draw the Files page before enqueueing its Moonraker worker.
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
    ui_files_close_detail_popup();
    if (s_printer_file_popup) {
        lv_obj_delete(s_printer_file_popup);
        s_printer_file_popup = NULL;
        s_printer_file_popup_label = NULL;
        s_printer_file_list = NULL;
        s_files_state = NULL;
        s_breadcrumb_label = NULL;
        s_sort_label = NULL;
        s_search_label = NULL;
        s_studio_file_hint = NULL;
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
    if(!s_printer_file_popup)return;
    ui_files_set_status("Loading files...");
    if(s_refresh_cb)s_refresh_cb();
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
static lv_obj_t *s_studio_metadata_popup = NULL;
static lv_obj_t *s_studio_metadata_label = NULL;
static ui_files_detail_cb_t s_detail_cancel_cb = NULL;
static ui_files_detail_cb_t s_detail_start_cb = NULL;
static lv_obj_t *s_detail_info_label = NULL;
static lv_obj_t *s_detail_start_button = NULL;
static lv_obj_t *s_print_confirm_popup;
static char *s_detail_filename;
static uint32_t s_print_confirm_generation;
static int s_print_confirm_profile;

static void print_confirm_deleted(lv_event_t *event)
{
    (void)event;s_print_confirm_popup=NULL;
}

static void print_confirm_cancel(lv_event_t *event)
{
    (void)event;
    if(s_print_confirm_popup)lv_obj_delete(s_print_confirm_popup);
}


bool ui_files_detail_is_open(void)
{
    return s_file_detail_popup != NULL;
}

bool ui_files_can_refresh_rows(void)
{
    return s_printer_file_popup && !s_print_confirm_popup && !s_studio_metadata_popup &&
        (!s_file_detail_popup || ui_theme_is_studio());
}

void ui_files_close_detail_popup(void)
{
    print_confirm_cancel(NULL);
    free(s_detail_filename);s_detail_filename=NULL;
    if (s_studio_metadata_popup) {lv_obj_delete(s_studio_metadata_popup);s_studio_metadata_popup=NULL;s_studio_metadata_label=NULL;}
    if (s_studio_file_hint) lv_obj_remove_flag(s_studio_file_hint,LV_OBJ_FLAG_HIDDEN);
    if (s_file_detail_popup) {
        lv_obj_delete(s_file_detail_popup);
        s_file_detail_popup = NULL;
        s_detail_info_label = NULL;
        s_detail_start_button = NULL;
    }
    s_detail_cancel_cb = NULL;
    s_detail_start_cb = NULL;
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

void ui_files_print_feedback(const char *message)
{
    if(s_print_confirm_popup) ui_popup_feedback(lv_obj_get_child(s_print_confirm_popup,1), UI_STATUS_WARNING, "PRINT NOT STARTED", message);
    else if(s_detail_info_label) lv_label_set_text(s_detail_info_label,message);
}

static void print_confirm_accept(lv_event_t *event)
{
    (void)event;
    if(!s_print_confirm_popup)return;
    bool ready=s_detail_start_cb && s_detail_start_button &&
        !lv_obj_has_state(s_detail_start_button,LV_STATE_DISABLED);
    bool same_printer=s_print_confirm_profile==moonraker_config_active_profile_index() &&
        s_print_confirm_generation==moonraker_config_generation();
    if(!same_printer) {
        ui_files_print_feedback("Printer changed. Cancel and select the file again before starting a print.");
        return;
    }
    if(!ready) {
        ui_files_print_feedback("Print not ready. Wait for the file details, then try again.");
        return;
    }
    s_detail_start_cb();
}

static void detail_start_event_cb(lv_event_t *event)
{
    (void)event;
    if(s_print_confirm_popup || !s_detail_filename || !s_detail_start_cb ||
       !s_detail_start_button || lv_obj_has_state(s_detail_start_button,LV_STATE_DISABLED))return;
    int width=lv_display_get_horizontal_resolution(NULL)-32;
    int height=lv_display_get_vertical_resolution(NULL)-32;
    if(width>640)width=640;
    if(height>340)height=340;
    s_print_confirm_popup=ui_popup_create(lv_layer_top(),width,height,UI_POPUP_STANDARD);
    if(!s_print_confirm_popup)return;
    lv_obj_add_event_cb(s_print_confirm_popup,print_confirm_deleted,LV_EVENT_DELETE,NULL);
    s_print_confirm_profile=moonraker_config_active_profile_index();
    s_print_confirm_generation=moonraker_config_generation();
    lv_obj_set_flex_flow(s_print_confirm_popup,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_print_confirm_popup,16,0);
    lv_obj_set_style_pad_row(s_print_confirm_popup,UI_GAP_ROW,0);
    lv_obj_t *title=lv_label_create(s_print_confirm_popup);
    lv_label_set_text(title,"Start this print?");ui_apply_custom_label_style(title,UI_FONT_TITLE,UI_TEXT);
    lv_obj_set_width(title,LV_PCT(100));
    lv_obj_t *body=lv_obj_create(s_print_confirm_popup);
    lv_obj_remove_style_all(body);lv_obj_set_size(body,LV_PCT(100),0);lv_obj_set_flex_grow(body,1);
    lv_obj_set_flex_flow(body,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(body,UI_GAP_ROW,0);
    lv_obj_set_scroll_dir(body,LV_DIR_VER);
    lv_obj_t *printer=lv_label_create(body);
    const char *name=moonraker_config_active_profile_name();
    lv_label_set_text_fmt(printer,"Printer: %s",name && name[0]?name:"Active printer");
    ui_apply_custom_label_style(printer,UI_FONT_BODY,UI_TEXT_DIM);lv_obj_set_width(printer,LV_PCT(100));
    lv_obj_t *file=lv_label_create(body);
    lv_label_set_text(file,s_detail_filename);ui_apply_custom_label_style(file,UI_FONT_BODY_LARGE,UI_TEXT);
    lv_obj_set_width(file,LV_PCT(100));lv_label_set_long_mode(file,LV_LABEL_LONG_WRAP);
    lv_obj_t *hint=lv_label_create(body);
    lv_label_set_text(hint,"The selected printer will run this G-code file.");
    ui_apply_custom_label_style(hint,UI_FONT_CAPTION,UI_TEXT_DIM);lv_obj_set_width(hint,LV_PCT(100));
    lv_obj_t *footer=lv_obj_create(s_print_confirm_popup);
    lv_obj_remove_style_all(footer);lv_obj_set_size(footer,LV_PCT(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(footer,LV_FLEX_FLOW_ROW_WRAP);lv_obj_set_style_pad_column(footer,UI_GAP_ROW,0);lv_obj_set_style_pad_row(footer,UI_GAP_ROW,0);
    lv_obj_t *cancel=ui_popup_add_action_at(footer,UI_POPUP_ACTION_CANCEL,"Cancel",0,0,LV_PCT(48),48,print_confirm_cancel,NULL,NULL);
    lv_obj_set_flex_grow(cancel,1);
    lv_obj_t *start=ui_popup_add_action_at(footer,UI_POPUP_ACTION_CONFIRM,"Start print",0,0,LV_PCT(48),48,print_confirm_accept,NULL,NULL);
    lv_obj_set_flex_grow(start,1);
}

static void file_metadata_scroll_style(lv_obj_t *body)
{
    lv_obj_add_flag(body,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(body,UI_TEXT_MUTED,LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(body,LV_OPA_70,LV_PART_SCROLLBAR);
    lv_obj_set_style_width(body,4,LV_PART_SCROLLBAR);
}

static void studio_metadata_close(lv_event_t *e)
{
    (void)e;
    if(s_studio_metadata_popup) {lv_obj_delete(s_studio_metadata_popup);s_studio_metadata_popup=NULL;s_studio_metadata_label=NULL;}
}
static void studio_metadata_open(lv_event_t *e)
{
    (void)e;
    if(s_studio_metadata_popup || !s_detail_info_label)return;
    int width = lv_display_get_horizontal_resolution(NULL) - 32;
    int height = lv_display_get_vertical_resolution(NULL) - 32;
    if (width > 640) width = 640;
    if (height > 440) height = 440;
    s_studio_metadata_popup=ui_popup_create(lv_layer_top(),width,height,UI_POPUP_STANDARD);
    if(!s_studio_metadata_popup)return;
    lv_obj_set_flex_flow(s_studio_metadata_popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_studio_metadata_popup, 16, 0);
    lv_obj_set_style_pad_row(s_studio_metadata_popup, UI_GAP_ROW, 0);
    lv_obj_t *title=studio_text(s_studio_metadata_popup,"File details",0,0,1,UI_FONT_TITLE,UI_TEXT);
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_t *body=studio_plane(s_studio_metadata_popup,0,0,1,0);
    lv_obj_set_width(body, LV_PCT(100));lv_obj_set_flex_grow(body,1);
    file_metadata_scroll_style(body);
    lv_obj_t *label=studio_text(body,lv_label_get_text(s_detail_info_label),0,0,1,UI_FONT_BODY,UI_TEXT);
    lv_obj_set_width(label,LV_PCT(100));
    lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
    lv_obj_set_height(label,LV_SIZE_CONTENT);
    s_studio_metadata_label=label;
    lv_obj_t *footer=studio_plane(s_studio_metadata_popup,0,0,1,48);
    lv_obj_set_width(footer,LV_PCT(100));
    lv_obj_set_flex_flow(footer,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer,LV_FLEX_ALIGN_END,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    studio_action(footer,"Close",0,0,128,48,studio_metadata_close,NULL);

}

void ui_files_show_detail_popup(const char *filename_text,
                                    const char *metadata_text,
                                    lv_obj_t **thumb_box_out,
                                    ui_thumbnail_t **thumb_view_out,
                                    ui_files_detail_cb_t cancel_cb,
                                    ui_files_detail_cb_t start_cb)
{
    ui_files_close_detail_popup();
    const char *selected_name=filename_text && filename_text[0]?filename_text:"Unnamed file";
    s_detail_filename=malloc(strlen(selected_name)+1);
    if(!s_detail_filename)return;
    memcpy(s_detail_filename,selected_name,strlen(selected_name)+1);

    s_detail_cancel_cb = cancel_cb;
    s_detail_start_cb = start_cb;

    if (thumb_box_out) {
        *thumb_box_out = NULL;
    }

    if (thumb_view_out) {
        *thumb_view_out = NULL;
    }

    if(ui_theme_is_studio() && s_printer_file_popup) {
        if(s_studio_file_hint)lv_obj_add_flag(s_studio_file_hint,LV_OBJ_FLAG_HIDDEN);
        s_file_detail_popup=studio_plane(s_printer_file_popup,624,0,352,424);
        ui_thumbnail_t *view=ui_thumbnail_create(s_file_detail_popup,0,0,352,228);
        if(!view){ui_files_close_detail_popup();return;}
        ui_thumbnail_set_placeholder(view,"PRINT\nTHUMBNAIL");
        if(thumb_box_out)*thumb_box_out=ui_thumbnail_box(view);
        if(thumb_view_out)*thumb_view_out=view;
        lv_obj_t *filename=studio_text(s_file_detail_popup,filename_text && filename_text[0] ? filename_text : "Unnamed file",0,240,352,UI_FONT_TITLE,UI_TEXT);
        lv_label_set_long_mode(filename,LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_t *metadata=studio_plane(s_file_detail_popup,0,282,352,74);
        file_metadata_scroll_style(metadata);
        s_detail_info_label=studio_text(metadata,metadata_text,0,0,344,UI_FONT_CAPTION,UI_TEXT_DIM);
        lv_label_set_long_mode(s_detail_info_label,LV_LABEL_LONG_WRAP);
        lv_obj_set_height(s_detail_info_label,LV_SIZE_CONTENT);
        s_detail_start_button=studio_action(s_file_detail_popup,"Print",0,372,128,52,detail_start_event_cb,NULL);
        lv_obj_add_state(s_detail_start_button,LV_STATE_DISABLED);
        studio_action(s_file_detail_popup,"Details",140,372,100,52,studio_metadata_open,NULL);
        studio_action(s_file_detail_popup,"Cancel",252,372,100,52,detail_cancel_event_cb,NULL);
        return;
    }

    int width=lv_display_get_horizontal_resolution(NULL)-32;
    int height=lv_display_get_vertical_resolution(NULL)-32;
    if(width>760)width=760;
    if(height>500)height=500;
    s_file_detail_popup=ui_popup_create(lv_screen_active(),width,height,UI_POPUP_STANDARD);
    if(!s_file_detail_popup)return;
    lv_obj_set_flex_flow(s_file_detail_popup,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_file_detail_popup,16,0);
    lv_obj_set_style_pad_row(s_file_detail_popup,UI_GAP_ROW,0);
    lv_obj_t *title=lv_label_create(s_file_detail_popup);
    lv_label_set_text(title,"FILE READY TO PRINT");
    ui_apply_custom_label_style(title,UI_FONT_TITLE,UI_TEXT_BRIGHT);
    lv_obj_set_width(title,LV_PCT(100));
    lv_obj_t *filename=lv_label_create(s_file_detail_popup);
    lv_label_set_text(filename,filename_text && filename_text[0] ? filename_text : "Unnamed file");
    ui_apply_custom_label_style(filename,UI_FONT_BODY,UI_ACCENT_CYAN);
    lv_obj_set_width(filename,LV_PCT(100));
    lv_label_set_long_mode(filename,LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_t *body=lv_obj_create(s_file_detail_popup);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body,LV_PCT(100),0);lv_obj_set_flex_grow(body,1);
    lv_obj_set_flex_flow(body,LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(body,UI_GAP_ROW,0);
    lv_obj_set_style_pad_row(body,UI_GAP_ROW,0);
    lv_obj_set_scroll_dir(body,LV_DIR_VER);
    ui_thumbnail_t *view=ui_thumbnail_create(body,0,0,270,250);
    if(!view){ui_files_close_detail_popup();return;}
    ui_thumbnail_set_placeholder(view,"PRINT\nTHUMBNAIL");
    lv_obj_t *box=ui_thumbnail_box(view);
    lv_obj_set_width(box,LV_PCT(40));
    if(thumb_box_out)*thumb_box_out=box;
    if(thumb_view_out)*thumb_view_out=view;
    const char *detail=metadata_text && metadata_text[0] ? metadata_text : "--";
    if(strncmp(detail,"File:\n",6)==0){const char *end=strstr(detail,"\n\n");if(end && end[2])detail=end+2;}
    lv_obj_t *info_panel=lv_obj_create(body);
    ui_apply_card_style(info_panel);
    lv_obj_set_size(info_panel,LV_PCT(56),250);lv_obj_set_flex_grow(info_panel,1);
    lv_obj_set_style_pad_all(info_panel,12,0);
    lv_obj_set_flex_flow(info_panel,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(info_panel,UI_GAP_ROW,0);
    file_metadata_scroll_style(info_panel);
    lv_obj_t *heading=lv_label_create(info_panel);
    lv_label_set_text(heading,"PRINT DETAILS");
    ui_apply_custom_label_style(heading,UI_FONT_BODY_LARGE,UI_TEXT_DIM);
    lv_obj_set_width(heading,LV_PCT(100));
    s_detail_info_label=lv_label_create(info_panel);
    lv_label_set_text(s_detail_info_label,detail);
    ui_apply_custom_label_style(s_detail_info_label,UI_FONT_BODY,UI_TEXT);
    lv_obj_set_width(s_detail_info_label,LV_PCT(100));
    lv_label_set_long_mode(s_detail_info_label,LV_LABEL_LONG_WRAP);
    lv_obj_t *footer=lv_obj_create(s_file_detail_popup);
    lv_obj_remove_style_all(footer);
    lv_obj_set_size(footer,LV_PCT(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(footer,LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(footer,UI_GAP_ROW,0);
    lv_obj_set_style_pad_row(footer,UI_GAP_ROW,0);
    lv_obj_t *cancel=ui_popup_add_action_at(footer,UI_POPUP_ACTION_CANCEL,LV_SYMBOL_CLOSE " CANCEL",0,0,LV_SIZE_CONTENT,48,detail_cancel_event_cb,NULL,NULL);
    lv_obj_set_flex_grow(cancel,1);
    s_detail_start_button=ui_popup_add_action_at(footer,UI_POPUP_ACTION_CONFIRM,LV_SYMBOL_PLAY " START PRINT",0,0,LV_SIZE_CONTENT,48,detail_start_event_cb,NULL,NULL);
    lv_obj_set_flex_grow(s_detail_start_button,1);
    lv_obj_add_state(s_detail_start_button,LV_STATE_DISABLED);
    lv_obj_move_foreground(s_file_detail_popup);

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
    if(s_studio_metadata_label)lv_label_set_text(s_studio_metadata_label,detail);

    if (s_detail_start_button) {
        if (ready) lv_obj_clear_state(s_detail_start_button, LV_STATE_DISABLED);
        else lv_obj_add_state(s_detail_start_button, LV_STATE_DISABLED);
    }
}
