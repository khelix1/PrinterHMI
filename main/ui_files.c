#include "ui_files.h"
#include "ui_value_update.h"
#include "ui_text_fit.h"
#include "printer_connection_policy.h"
#include "ui_form_dialog.h"
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
static lv_obj_t *s_up_button;
static bool s_folder_has_parent;
static lv_obj_t *s_breadcrumb_label = NULL;
static lv_obj_t *s_studio_file_hint;
static lv_obj_t *s_sort_label = NULL;
static lv_obj_t *s_search_label = NULL;
static lv_obj_t *s_search_popup = NULL;
static lv_obj_t *s_search_textarea = NULL;
static lv_obj_t *s_search_button, *s_sort_button, *s_refresh_button;
static lv_obj_t *s_search_keyboard, *s_search_apply, *s_search_cancel;
static uint32_t s_files_epoch, s_files_generation;
static bool s_files_browser_dispatching;
static char s_search_text[64];
static char s_folder_breadcrumb[190] = "G-code files";
static void up_button_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)==s_up_button)s_up_button=NULL;
}
static void update_up_button(void)
{
    if(!s_up_button)return;
    if(!s_up_cb || (!s_folder_has_parent && !s_search_text[0]))lv_obj_add_state(s_up_button,LV_STATE_DISABLED);
    else lv_obj_remove_state(s_up_button,LV_STATE_DISABLED);
}


static lv_timer_t *s_files_refresh_timer = NULL;
static bool s_files_refresh_pending = false;
static bool s_files_print_dispatching;
static void files_page_deleted(lv_event_t *event);

static bool files_browser_ready(void)
{
    return s_printer_file_popup && !s_files_print_dispatching && !s_files_browser_dispatching &&
        s_files_generation==moonraker_config_generation();
}
static bool files_owned_click(lv_event_t *event,lv_obj_t *button)
{
    return event && files_browser_ready() && button && !lv_obj_has_state(button,LV_STATE_DISABLED) &&
        lv_event_get_code(event)==LV_EVENT_CLICKED && lv_event_get_target_obj(event)==button &&
        lv_event_get_current_target_obj(event)==button;
}
static void files_field_deleted(lv_event_t *event)
{
    lv_obj_t *object=lv_event_get_target_obj(event);
    lv_obj_t **fields[]={&s_printer_file_list,&s_breadcrumb_label,&s_studio_file_hint,
        &s_sort_label,&s_search_label,&s_search_textarea,&s_search_keyboard,
        &s_search_apply,&s_search_cancel,&s_search_button,&s_sort_button,&s_refresh_button};
    bool retired=false;
    for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);++i)
        if(*fields[i]==object){*fields[i]=NULL;retired=true;}
    if(retired)++s_files_epoch;
}
static void files_watch(lv_obj_t *object)
{
    if(object)lv_obj_add_event_cb(object,files_field_deleted,LV_EVENT_DELETE,NULL);
}
static void files_dispatch_path(void (*callback)(const char *),const char *path)
{
    if(!callback || !path || !files_browser_ready())return;
    char *copy=strdup(path);
    if(!copy)return;
    s_files_browser_dispatching=true;
    callback(copy);
    s_files_browser_dispatching=false;
    free(copy);
}

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
static bool s_files_preview_dispatching;

static void file_row_field_deleted(lv_event_t *event)
{
    file_row_context_t *row=lv_event_get_user_data(event);
    lv_obj_t *object=lv_event_get_target_obj(event);
    if(object==row->preview_frame)row->preview_frame=NULL;
    if(object==row->file_icon)row->file_icon=NULL;
    if(object==row->preview_image)row->preview_image=NULL;
    ++s_files_epoch;
}

static bool files_preview_ready(void)
{
    return s_printer_file_popup && !s_files_print_dispatching && !s_files_preview_dispatching &&
        s_files_generation==moonraker_config_generation();
}
static void files_request_preview(file_row_context_t *row,ui_files_preview_cb_t preview)
{
    if(!preview || !files_preview_ready())return;
    char *path=strdup(row->path);
    if(!path)return;
    row->preview_requested=true;
    bool dispatching=s_files_browser_dispatching;
    s_files_browser_dispatching=s_files_preview_dispatching=true;
    preview(path);
    s_files_preview_dispatching=false;s_files_browser_dispatching=dispatching;
    free(path);
}

static void file_preview_clicked_cb(lv_event_t *event)
{
    file_row_context_t *row =
        (file_row_context_t *)lv_event_get_user_data(event);

    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !row || !files_browser_ready() ||
        lv_event_get_target_obj(event)!=row->preview_frame || lv_obj_has_state(row->button,LV_STATE_DISABLED)) {
        return;
    }

    lv_event_stop_bubbling(event);
    if (row->preview_image) {
        ui_preview_lightbox_show_file_object(
            row->preview_image,
            row->path);
    } else if (s_select_cb) {
        /* Metadata/details remain reachable while the thumbnail is pending. */
        files_dispatch_path(s_select_cb,row->path);
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
    } else if (strstr(message, "current search")) {
        ui_page_state_show(s_files_state, UI_PAGE_STATE_EMPTY,
                           "NO SEARCH RESULTS",
                           "Try a different search or clear it to browse all files.");
    } else if (strstr(message, "No files")) {
        ui_page_state_show(s_files_state,
                               UI_PAGE_STATE_EMPTY,
                               "NO FILES FOUND",
                               strstr(message, "this folder")
                                   ? "This folder is empty. Go up to browse another folder."
                                   : "Upload G-code through Moonraker, then refresh.");
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
    if (!timer || timer != s_files_refresh_timer) return;
    s_files_refresh_timer = NULL;
    s_files_refresh_pending = false;
    lv_timer_delete(timer);

    /*
     * Enqueue the file-list worker after the first page draw. HTTP retries
     * run outside LVGL; its result timer updates the current page in place.
     */
    if (files_browser_ready() && s_refresh_cb) {
        ui_files_refresh_cb_t refresh=s_refresh_cb;
        s_files_browser_dispatching=true;refresh();s_files_browser_dispatching=false;
    }
}

static void files_refresh_event_cb(lv_event_t *e)
{
    if (!files_owned_click(e,s_refresh_button) ||
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
    if (!s_preview_cb || !files_preview_ready()) return;
    uint32_t epoch=s_files_epoch;
    ui_files_preview_cb_t preview=s_preview_cb;

    for (file_row_context_t *row = s_file_rows;
         row;
         row = row->next) {
        if (!row->preview_requested && file_row_is_visible(row)) {
            files_request_preview(row,preview);
            if(s_files_epoch!=epoch || !files_preview_ready())return;
        }
    }
}

/* An opaque list bounds scroll damage to the browser surface. A transparent
 * list forces LVGL to repaint the parent page under every scrolled row. */
static void files_style_scroll_surface(lv_obj_t *list)
{
    ui_apply_scroll_surface(list);
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
    lv_event_code_t code = lv_event_get_code(e);
    if(lv_event_get_target_obj(e)!=lv_event_get_current_target_obj(e))return;
    /* Child lifecycle events can bubble after the row context is freed. */
    if (code != LV_EVENT_DELETE && code != LV_EVENT_LONG_PRESSED &&
        code != LV_EVENT_PRESS_LOST && code != LV_EVENT_CLICKED) return;

    file_row_context_t *row =
        (file_row_context_t *)lv_event_get_user_data(e);

    if (!row || lv_event_get_target_obj(e) != row->button) return;

    if (code == LV_EVENT_DELETE) {
        ++s_files_epoch;
        lv_obj_t *fields[]={row->preview_frame,row->file_icon,row->preview_image};
        for(unsigned i=0;i<3;++i)if(fields[i])
            lv_obj_remove_event_cb_with_user_data(fields[i],file_row_field_deleted,row);
        file_row_context_t **link = &s_file_rows;
        while (*link && *link != row) link = &(*link)->next;
        if (*link == row) *link = row->next;

        free(row->path);
        free(row);
        return;
    }
    if(!files_browser_ready() || lv_obj_has_state(row->button,LV_STATE_DISABLED))return;

    if (code == LV_EVENT_LONG_PRESSED) {
        row->suppress_click = true;
        if (s_select_cb) files_dispatch_path(s_select_cb,row->path);
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

    if (s_select_cb) files_dispatch_path(s_select_cb,row->path);
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
    ui_apply_divider_border(btn,LV_BORDER_SIDE_BOTTOM);

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
    lv_obj_add_event_cb(row->preview_frame,file_row_field_deleted,LV_EVENT_DELETE,row);
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
    lv_obj_add_event_cb(icon,file_row_field_deleted,LV_EVENT_DELETE,row);

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
     * Secondary file metadata. Keep it compact; both tap and hold already
     * open details, so the row does not need a repeated gesture instruction.
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
    char size_text[32] = "G-code file";
    if (size >= 1024.0 * 1024.0 * 1024.0) {
        snprintf(size_text, sizeof(size_text), "%.1f GiB", size / (1024.0 * 1024.0 * 1024.0));
    } else if (size >= 1024.0 * 1024.0) {
        snprintf(size_text, sizeof(size_text), "%.1f MiB", size / (1024.0 * 1024.0));
    } else if (size >= 1024.0) {
        snprintf(size_text, sizeof(size_text), "%.1f KiB", size / 1024.0);
    } else if (size > 0.0) {
        snprintf(size_text, sizeof(size_text), "%.0f B", size);
    }
    if (date_text[0]) {
        snprintf(meta_text, sizeof(meta_text), "%s  |  %s", size_text, date_text);
    } else {
        snprintf(meta_text, sizeof(meta_text), "%s", size_text);
    }
    lv_label_set_text(meta, meta_text);

    lv_obj_set_width(
        meta,
        1);

    lv_label_set_long_mode(
        meta,
        LV_LABEL_LONG_DOT);

    ui_apply_custom_label_style(meta,
                                UI_FONT_CAPTION,
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
        files_request_preview(row,s_preview_cb);
    }
}

void ui_files_add_file_button(const char *path, int y)
{
    ui_files_add_file_entry(path, 0.0, 0.0, y);
}

typedef struct {
    char *path;
    lv_obj_t *button;
} folder_event_data_t;

static void folder_event_cb(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=lv_event_get_current_target_obj(event))return;
    folder_event_data_t *data = lv_event_get_user_data(event);
    if (!data || lv_event_get_target_obj(event)!=data->button) return;
    if (lv_event_get_code(event) == LV_EVENT_CLICKED && s_folder_cb) {
        if(files_owned_click(event,data->button))files_dispatch_path(s_folder_cb,data->path);
    } else if (lv_event_get_code(event) == LV_EVENT_DELETE) {
        ++s_files_epoch;
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
    data->button=button;
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
    ++s_files_epoch;
    if (s_printer_file_list) lv_obj_clean(s_printer_file_list);
}

static void files_update_breadcrumb(void)
{
    if (!s_breadcrumb_label) return;
    if (s_search_text[0]) {
        char text[190];
        snprintf(text, sizeof(text), "Search / %s", s_search_text);
        ui_value_set_text(s_breadcrumb_label, text);
    } else {
        ui_value_set_text(s_breadcrumb_label, s_folder_breadcrumb);
    }
}

void ui_files_set_breadcrumb(const char *path)
{
    s_folder_has_parent = path && path[0];
    snprintf(s_folder_breadcrumb, sizeof(s_folder_breadcrumb),
             "G-code files%s%s", s_folder_has_parent ? " / " : "",
             s_folder_has_parent ? path : "");
    update_up_button();
    files_update_breadcrumb();
}

void ui_files_set_sort_text(const char *text)
{
    const char *label = text ? text : "Sort";
    if (!strcmp(label, "NAME")) label = "Name";
    else if (!strcmp(label, "NEWEST")) label = "Newest";
    else if (!strcmp(label, "SIZE")) label = "Size";
    if (s_sort_label) ui_value_set_text(s_sort_label, ui_text(label));
}

void ui_files_set_search_text(const char *text)
{
    snprintf(s_search_text, sizeof(s_search_text), "%s", text ? text : "");
    update_up_button();
    if (s_search_label) ui_value_set_text(s_search_label, ui_text("Search"));
    ui_button_set_filter_active(s_search_button,s_search_text[0]!=0);
    files_update_breadcrumb();
}


static void close_search_popup(void)
{
    ++s_files_epoch;
    lv_obj_t *popup = s_search_popup;
    s_search_popup = s_search_textarea = NULL;
    s_search_keyboard=s_search_apply=s_search_cancel=NULL;
    if (popup) lv_obj_delete(popup);
}

static void search_popup_deleted_cb(lv_event_t *event)
{
    if (event && lv_event_get_target(event) == s_search_popup) {
        s_search_popup = NULL;
        s_search_textarea = NULL;
        s_search_keyboard=s_search_apply=s_search_cancel=NULL;
        ++s_files_epoch;
    }
}

static void apply_search_and_close(bool async_close)
{
    if (!files_browser_ready() || !s_search_popup || !s_search_textarea || !s_search_apply ||
        lv_obj_has_state(s_search_apply,LV_STATE_DISABLED)) return;
    char query[sizeof(s_search_text)];
    snprintf(query,
             sizeof(query),
             "%s",
             s_search_textarea
                 ? lv_textarea_get_text(s_search_textarea)
                 : "");

    lv_obj_t *popup = s_search_popup;
    lv_obj_t *page=s_printer_file_popup;
    uint32_t epoch=++s_files_epoch,generation=s_files_generation;
    ui_files_search_cb_t search=s_search_cb;
    s_search_popup = NULL;
    s_search_textarea = NULL;
    s_search_keyboard=s_search_apply=s_search_cancel=NULL;
    if (popup) {
        if (async_close) lv_obj_delete_async(popup);
        else lv_obj_delete(popup);
    }

    if (search && s_files_epoch==epoch && page==s_printer_file_popup &&
        generation==moonraker_config_generation())files_dispatch_path(search,query);
}

static void search_keyboard_cb(lv_event_t *event)
{
    if (!event || !files_browser_ready() || !s_search_keyboard ||
        lv_event_get_target_obj(event)!=s_search_keyboard ||
        lv_event_get_current_target_obj(event)!=s_search_keyboard)return;
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY) {
        apply_search_and_close(true);
    } else if (code == LV_EVENT_CANCEL) {
        lv_obj_t *popup = s_search_popup;
        s_search_popup = NULL;
        s_search_textarea = NULL;
        s_search_keyboard=s_search_apply=s_search_cancel=NULL;
        ++s_files_epoch;
        if (popup) lv_obj_delete_async(popup);
    }
}

static void search_apply_button_cb(lv_event_t *event)
{
    if (files_owned_click(event,s_search_apply)) {
        apply_search_and_close(false);
    }
}

static void search_cancel_button_cb(lv_event_t *event)
{
    if (files_owned_click(event,s_search_cancel)) close_search_popup();
}

static void search_button_cb(lv_event_t *event)
{
    if (!files_owned_click(event,s_search_button))return;
    if (s_search_popup) { lv_obj_move_foreground(s_search_popup); return; }
    s_search_popup=ui_form_dialog("Search files",760,540,UI_POPUP_STANDARD);
    if(!s_search_popup)return;
    ++s_files_epoch;
    lv_obj_add_event_cb(s_search_popup,search_popup_deleted_cb,LV_EVENT_DELETE,NULL);
    lv_obj_t *body=ui_form_body(s_search_popup);
    s_search_textarea=ui_form_input(body,"Search filenames and folders",s_search_text,false,sizeof(s_search_text)-1);
    files_watch(s_search_textarea);
    lv_obj_t *keyboard=ui_form_keyboard(s_search_popup,s_search_textarea,LV_KEYBOARD_MODE_TEXT_LOWER);
    s_search_keyboard=keyboard;files_watch(keyboard);
    ui_popup_keyboard_actions(keyboard,search_keyboard_cb,search_keyboard_cb);
    s_search_cancel=ui_form_action(s_search_popup,UI_BUTTON_CANCEL,"Cancel",search_cancel_button_cb,NULL);files_watch(s_search_cancel);
    s_search_apply=ui_form_action(s_search_popup,UI_BUTTON_PRIMARY,"Search",search_apply_button_cb,NULL);files_watch(s_search_apply);
    ui_form_finish(s_search_popup);ui_popup_focus_input(s_search_textarea);
    lv_obj_move_foreground(s_search_popup);
}

static void sort_button_cb(lv_event_t *event)
{
    if (files_owned_click(event,s_sort_button) && s_sort_cb) {
        ui_files_action_cb_t sort=s_sort_cb;s_files_browser_dispatching=true;sort();s_files_browser_dispatching=false;
    }
}

static void up_button_cb(lv_event_t *event)
{
    if (files_owned_click(event,s_up_button) && s_up_cb) {
        ui_files_action_cb_t up=s_up_cb;s_files_browser_dispatching=true;up();s_files_browser_dispatching=false;
    }
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
            lv_obj_add_event_cb(row->preview_image,file_row_field_deleted,LV_EVENT_DELETE,row);
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
    ++s_files_epoch;
    s_files_generation=moonraker_config_generation();

    if(ui_theme_is_studio()) {
        s_printer_file_popup=lv_obj_create(lv_screen_active());
        lv_obj_add_event_cb(s_printer_file_popup,files_page_deleted,LV_EVENT_DELETE,NULL);
        lv_obj_set_size(s_printer_file_popup,854,528);ui_apply_root_style(s_printer_file_popup);
        studio_text(s_printer_file_popup,"Job library",0,0,580,&ui_studio_font_48,UI_TEXT);
        s_up_button=studio_action(s_printer_file_popup,"Up",0,70,100,44,up_button_cb,NULL);
        lv_obj_add_event_cb(s_up_button,up_button_deleted,LV_EVENT_DELETE,NULL);
        lv_obj_t *search=studio_action(s_printer_file_popup,"Search",110,70,148,44,search_button_cb,NULL);
        s_search_button=search;files_watch(search);
        s_search_label=lv_obj_get_child(search,0);
        lv_obj_t *sort=studio_action(s_printer_file_popup,"Name",268,70,144,44,sort_button_cb,NULL);
        s_sort_button=sort;files_watch(sort);
        s_sort_label=lv_obj_get_child(sort,0);
        s_refresh_button=studio_action(s_printer_file_popup,"Refresh",422,70,154,44,files_refresh_event_cb,NULL);files_watch(s_refresh_button);
        s_breadcrumb_label=studio_text(s_printer_file_popup,"G-code files",0,120,576,UI_FONT_CAPTION,UI_TEXT_DIM);
        lv_label_set_long_mode(s_breadcrumb_label,LV_LABEL_LONG_DOT);
        ui_files_set_breadcrumb(NULL);ui_files_set_search_text(s_search_text);
        lv_obj_t *viewport=studio_plane(s_printer_file_popup,0,150,576,274);
        s_printer_file_list=studio_plane(viewport,0,0,576,274);
        files_style_scroll_surface(s_printer_file_list);
        lv_obj_add_flag(s_printer_file_list,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(s_printer_file_list,LV_DIR_VER);
        lv_obj_set_scrollbar_mode(s_printer_file_list,LV_SCROLLBAR_MODE_AUTO);
        lv_obj_add_event_cb(s_printer_file_list,file_list_scroll_event_cb,LV_EVENT_ALL,NULL);
        s_files_state=ui_page_state_create_owned(viewport,0,0,576,274,&s_files_state);
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
    lv_obj_add_event_cb(s_printer_file_popup,files_page_deleted,LV_EVENT_DELETE,NULL);

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
    lv_label_set_text(title, LV_SYMBOL_FILE " Files");
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
        LV_SYMBOL_UP, "Up", UI_ACCENT_CYAN, UI_BUTTON_ICON_HORIZONTAL);
    s_up_button=up;
    if (up) {
        lv_obj_add_event_cb(up,up_button_deleted,LV_EVENT_DELETE,NULL);
        ui_responsive_action(up);

        lv_obj_add_event_cb(up, up_button_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *search = ui_button_create_icon(
        toolbar, UI_BUTTON_OUTLINED,
        LV_SYMBOL_EDIT, "Search", UI_ACCENT_CYAN, UI_BUTTON_ICON_HORIZONTAL);
    if (search) {
        s_search_button=search;files_watch(search);
        ui_responsive_action(search);

        s_search_label = lv_obj_get_child(search, 1);
        ui_files_set_search_text(s_search_text);
        lv_obj_add_event_cb(search, search_button_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *sort = ui_button_create_icon(
        toolbar, UI_BUTTON_OUTLINED,
        LV_SYMBOL_LIST, "Name", UI_ACCENT_CYAN, UI_BUTTON_ICON_HORIZONTAL);
    if (sort) {
        s_sort_button=sort;files_watch(sort);
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
            "Refresh",
            UI_ACCENT_CYAN,
            UI_BUTTON_ICON_HORIZONTAL);

    if (refresh) {
        s_refresh_button=refresh;files_watch(refresh);
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
     * Keep the flat list opaque so scroll damage skips the parent page.
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
    files_style_scroll_surface(s_printer_file_list);
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

    s_files_state = ui_page_state_create_owned(
        viewport, 0, 0, lv_pct(100), lv_pct(100), &s_files_state);

schedule_files_refresh:
    files_watch(s_printer_file_list);files_watch(s_breadcrumb_label);
    files_watch(s_sort_label);files_watch(s_search_label);files_watch(s_studio_file_hint);
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
    ++s_files_epoch;
    lv_obj_t *popup = s_printer_file_popup;
    lv_timer_t *timer = s_files_refresh_timer;
    s_files_refresh_timer = NULL;
    s_files_refresh_pending = false;
    s_printer_file_popup = s_printer_file_popup_label = s_printer_file_list = NULL;
    s_files_state = NULL;
    s_breadcrumb_label = s_sort_label = s_search_label = s_studio_file_hint = s_up_button = NULL;
    s_search_button=s_sort_button=s_refresh_button=NULL;
    s_folder_has_parent=false;
    close_search_popup();
    ui_files_close_detail_popup();
    if (timer) lv_timer_delete(timer);
    if (popup) lv_obj_delete(popup);
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
    update_up_button();
}

void ui_files_refresh(void)
{
    if(!files_browser_ready())return;
    ui_files_set_status("Loading files...");
    if(s_refresh_cb){ui_files_refresh_cb_t refresh=s_refresh_cb;s_files_browser_dispatching=true;refresh();s_files_browser_dispatching=false;}
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
static lv_obj_t *s_detail_more_button, *s_detail_cancel_button;
static lv_obj_t *s_detail_preview_box, *s_detail_filename_label;
static lv_obj_t *s_print_confirm_popup;
static lv_obj_t *s_print_accept_button;
static lv_obj_t *s_print_connection_label;
static void print_confirm_cancel(lv_event_t *event);
static char *s_detail_filename;
static uint32_t s_print_confirm_generation;
static int s_print_confirm_profile;
static bool s_detail_metadata_ready;
static uint32_t s_detail_generation;
static lv_timer_t *s_detail_connection_timer;
static lv_obj_t *s_detail_connection_label;
static void detail_connection_refresh(lv_timer_t *timer)
{
    if(!s_file_detail_popup || (timer && timer != s_detail_connection_timer))return;
    moonraker_state_t state;moonraker_state_snapshot(&state);
    const char *reason=s_detail_generation!=moonraker_config_generation()?
        "Printer changed - reselect file.":printer_idle_reason(&state);
    if(s_detail_generation==moonraker_config_generation()) {
        if(!state.moonraker_ok)reason="Offline - reconnect before printing.";
        else if(!state.live_data_ok)reason="Waiting for Klipper before printing.";
    }
    if(!reason[0] && !s_detail_metadata_ready)reason="Loading file details - Print unavailable.";
    if(!reason[0] && !s_detail_start_cb)reason="Print action unavailable.";
    if(s_detail_start_button) {
        if(reason[0] || !s_detail_start_cb || s_files_print_dispatching)lv_obj_add_state(s_detail_start_button,LV_STATE_DISABLED);
        else lv_obj_remove_state(s_detail_start_button,LV_STATE_DISABLED);
    }
    if(s_print_accept_button) {
        if(reason[0] || !s_detail_start_cb || s_files_print_dispatching)lv_obj_add_state(s_print_accept_button,LV_STATE_DISABLED);
        else lv_obj_remove_state(s_print_accept_button,LV_STATE_DISABLED);
    }
    ui_value_set_text(s_print_connection_label,reason[0]?reason:"The selected printer will run this G-code file.");
    /* Inline readiness caption temporarily hidden at the operator's request. */
}
static void detail_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=s_file_detail_popup)return;
    ui_files_detail_cb_t cancel = s_detail_cancel_cb;
    s_file_detail_popup = NULL;
    s_detail_cancel_cb = NULL;
    ui_files_close_detail_popup();
    /* Notify the loader/thumbnail owner after UI references are retired. */
    if (cancel) cancel();
}
static void detail_watch(lv_obj_t *label)
{
    s_detail_connection_label=label;
    lv_label_set_text(label,"");
    lv_obj_add_flag(label,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_file_detail_popup,detail_deleted,LV_EVENT_DELETE,NULL);
    s_detail_connection_timer=lv_timer_create(detail_connection_refresh,250,NULL);
    detail_connection_refresh(NULL);
}

static void print_confirm_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)==s_print_confirm_popup) {
        s_print_confirm_popup=NULL;s_print_accept_button=NULL;s_print_connection_label=NULL;
    }
}

static void print_confirm_cancel(lv_event_t *event)
{
    (void)event;
    lv_obj_t *popup=s_print_confirm_popup;
    s_print_confirm_popup=s_print_accept_button=s_print_connection_label=NULL;
    if(popup)lv_obj_delete(popup);
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
    lv_obj_t *detail=s_file_detail_popup, *metadata=s_studio_metadata_popup;
    lv_timer_t *timer=s_detail_connection_timer;
    s_file_detail_popup=s_detail_info_label=s_detail_start_button=NULL;
    s_studio_metadata_popup=s_studio_metadata_label=NULL;
    s_detail_more_button=s_detail_cancel_button=NULL;
    s_detail_preview_box=s_detail_filename_label=NULL;
    s_detail_connection_timer=NULL;s_detail_connection_label=NULL;s_detail_metadata_ready=false;
    s_detail_cancel_cb=s_detail_start_cb=NULL;
    free(s_detail_filename);s_detail_filename=NULL;
    print_confirm_cancel(NULL);
    if(timer)lv_timer_delete(timer);
    if(metadata)lv_obj_delete(metadata);
    if(s_studio_file_hint)lv_obj_remove_flag(s_studio_file_hint,LV_OBJ_FLAG_HIDDEN);
    if(detail)lv_obj_delete(detail);
}

static void files_page_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=s_printer_file_popup)return;
    ui_files_detail_cb_t cancel=s_detail_cancel_cb;
    s_printer_file_popup=NULL;
    ui_files_hide();
    if(cancel)cancel();
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
    else if(s_detail_info_label) ui_value_set_text(s_detail_info_label,message);
}

static void print_confirm_accept(lv_event_t *event)
{
    (void)event;
    if(!s_print_confirm_popup || s_files_print_dispatching)return;
    detail_connection_refresh(NULL);
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
    lv_obj_t *popup=s_print_confirm_popup;
    uint32_t owner=s_print_confirm_generation;
    ui_files_detail_cb_t start=s_detail_start_cb;
    s_files_print_dispatching=true;
    detail_connection_refresh(NULL);
    start();
    s_files_print_dispatching=false;
    if(s_print_confirm_popup==popup && owner==moonraker_config_generation())detail_connection_refresh(NULL);
}

static void detail_start_event_cb(lv_event_t *event)
{
    (void)event;
    detail_connection_refresh(NULL);
    if(s_files_print_dispatching || s_print_confirm_popup || !s_detail_filename || !s_detail_start_cb ||
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
    s_print_connection_label=hint;
    lv_label_set_text(hint,"The selected printer will run this G-code file.");
    ui_apply_custom_label_style(hint,UI_FONT_CAPTION,UI_TEXT_DIM);lv_obj_set_width(hint,LV_PCT(100));
    lv_obj_t *footer=lv_obj_create(s_print_confirm_popup);
    lv_obj_remove_style_all(footer);lv_obj_set_size(footer,LV_PCT(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(footer,LV_FLEX_FLOW_ROW_WRAP);lv_obj_set_style_pad_column(footer,UI_GAP_ROW,0);lv_obj_set_style_pad_row(footer,UI_GAP_ROW,0);
    lv_obj_t *cancel=ui_popup_add_action_at(footer,UI_POPUP_ACTION_CANCEL,"Cancel",0,0,LV_PCT(48),48,print_confirm_cancel,NULL,NULL);
    lv_obj_set_flex_grow(cancel,1);
    lv_obj_t *start=ui_popup_add_action_at(footer,UI_POPUP_ACTION_CONFIRM,"Start print",0,0,LV_PCT(48),48,print_confirm_accept,NULL,NULL);
    lv_obj_set_flex_grow(start,1);
    s_print_accept_button=start;detail_connection_refresh(NULL);
}

static void file_metadata_scroll_style(lv_obj_t *body)
{
    /* studio_plane() clears CLICKABLE; a scroll body must receive pointer hits. */
    lv_obj_add_flag(body,LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(body,LV_OBJ_FLAG_SCROLL_CHAIN_VER | LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_obj_set_scroll_dir(body,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(body,UI_TEXT_MUTED,LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(body,LV_OPA_70,LV_PART_SCROLLBAR);
    lv_obj_set_style_width(body,4,LV_PART_SCROLLBAR);
}

static void studio_metadata_close(lv_event_t *e)
{
    (void)e;
    lv_obj_t *popup=s_studio_metadata_popup;
    s_studio_metadata_popup=s_studio_metadata_label=NULL;
    if(popup)lv_obj_delete(popup);
}
static void studio_metadata_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)==s_studio_metadata_popup)
        s_studio_metadata_popup=s_studio_metadata_label=NULL;
}
/* The standard metadata builder includes a duplicate filename, preview
 * transport diagnostics and a print instruction. Keep all actual fields. */
static char *file_detail_display_text(const char *metadata)
{
    const char *text=metadata && metadata[0]?metadata:"Metadata unavailable.";
    if(strncmp(text,"File:\n",6)==0) {
        const char *end=strstr(text,"\n\n");
        if(end && end[2])text=end+2;
    }
    char *display=malloc(strlen(text)+1);
    if(!display)return NULL;
    char *out=display;
    bool thumbnail_path=false;
    for(const char *line=text;*line;) {
        const char *end=strchr(line,'\n');
        size_t length=end?(size_t)(end-line):strlen(line);
        if(length && line[length-1]=='\r')--length;
        bool skip=length==0;
        if(thumbnail_path) {
            /* The generated next line is a path; preserve any named field. */
            if(!memchr(line,':',length))skip=true;
            thumbnail_path=false;
        }
        if(length>=10 && strncmp(line,"Thumbnail:",10)==0) {
            skip=true;thumbnail_path=true;
        }
        if(length==strlen("Confirm to start this print.") &&
           strncmp(line,"Confirm to start this print.",length)==0)skip=true;
        if(!skip) {
            if(out!=display)*out++='\n';
            memcpy(out,line,length);out+=length;
        }
        line=end?end+1:line+strlen(line);
    }
    *out='\0';
    return display;
}

static void detail_field_deleted(lv_event_t *event)
{
    lv_obj_t *object=lv_event_get_target_obj(event);
    if(object==s_detail_more_button)s_detail_more_button=NULL;
    if(object==s_detail_cancel_button)s_detail_cancel_button=NULL;
    if(object==s_detail_preview_box)s_detail_preview_box=NULL;
    if(object==s_detail_filename_label)s_detail_filename_label=NULL;
}

static void detail_preview_resized(lv_event_t *event)
{
    lv_obj_t *box=lv_event_get_target_obj(event);
    if(box!=s_detail_preview_box)return;
    for(uint32_t i=0;i<lv_obj_get_child_count(box);++i) {
        lv_obj_t *child=lv_obj_get_child(box,i);
        if(lv_obj_check_type(child,&lv_image_class)) {
            ui_thumbnail_fit_object(child,box,lv_image_get_src_width(child),lv_image_get_src_height(child),0);
            return; /* Fitting may invoke owner callbacks; do not reuse pointers. */
        }
    }
    for(uint32_t i=0;i<lv_obj_get_child_count(box);++i) {
        lv_obj_t *child=lv_obj_get_child(box,i);
        if(lv_obj_check_type(child,&lv_label_class)) {
            lv_obj_center(child);
            return;
        }
    }
}

static void detail_update_more_action(void)
{
    if(!s_detail_more_button || !s_file_detail_popup || !s_detail_info_label)return;
    lv_obj_t *owner=s_file_detail_popup,*label=s_detail_info_label,*more=s_detail_more_button;
    lv_obj_update_layout(owner);
    if(owner!=s_file_detail_popup || label!=s_detail_info_label || more!=s_detail_more_button)return;
    lv_obj_t *viewport=lv_obj_get_parent(label);
    if(s_detail_preview_box && s_detail_filename_label && s_detail_start_button) {
        /* Leave room for every measured metadata line before allocating the
         * remaining height to the preview. Long extras retain a usable preview. */
        int32_t bottom=lv_obj_get_y(s_detail_start_button)-16;
        int32_t filename_height=lv_obj_get_height(s_detail_filename_label);
        int32_t metadata_height=lv_obj_get_height(label);
        int32_t metadata_capacity=bottom-filename_height-20-128;
        if(metadata_capacity<1)metadata_capacity=1;
        if(metadata_height>metadata_capacity)metadata_height=metadata_capacity;
        if(metadata_height<1)metadata_height=1;
        int32_t preview_height=bottom-filename_height-20-metadata_height;
        if(preview_height<1)preview_height=1;
        lv_obj_set_height(s_detail_preview_box,preview_height);
        if(owner!=s_file_detail_popup || label!=s_detail_info_label)return;
        if(s_detail_filename_label)lv_obj_set_y(s_detail_filename_label,preview_height+12);
        if(owner!=s_file_detail_popup || label!=s_detail_info_label)return;
        lv_obj_set_y(viewport,preview_height+12+filename_height+8);
        lv_obj_set_height(viewport,metadata_height);
        lv_obj_update_layout(owner);
        if(owner!=s_file_detail_popup || label!=s_detail_info_label || more!=s_detail_more_button)return;
    }
    bool overflow=lv_obj_get_height(label)>lv_obj_get_content_height(viewport);
    if(overflow)lv_obj_remove_flag(more,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(more,LV_OBJ_FLAG_HIDDEN);
    if(s_detail_start_button)lv_obj_set_width(s_detail_start_button,overflow?128:168);
    if(s_detail_cancel_button) {
        lv_obj_set_x(s_detail_cancel_button,overflow?252:184);
        lv_obj_set_width(s_detail_cancel_button,overflow?100:168);
    }
}

static void studio_metadata_open(lv_event_t *e)
{
    (void)e;
    if(s_studio_metadata_popup || !s_detail_info_label || !s_detail_more_button ||
       lv_obj_has_flag(s_detail_more_button,LV_OBJ_FLAG_HIDDEN))return;
    int width = lv_display_get_horizontal_resolution(NULL) - 32;
    int height = lv_display_get_vertical_resolution(NULL) - 32;
    if (width > 640) width = 640;
    if (height > 440) height = 440;
    s_studio_metadata_popup=ui_popup_create(lv_layer_top(),width,height,UI_POPUP_STANDARD);
    if(!s_studio_metadata_popup)return;
    lv_obj_add_event_cb(s_studio_metadata_popup,studio_metadata_deleted,LV_EVENT_DELETE,NULL);
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
    s_detail_generation=moonraker_config_generation();
    s_detail_metadata_ready=false;

    if (thumb_box_out) {
        *thumb_box_out = NULL;
    }

    if (thumb_view_out) {
        *thumb_view_out = NULL;
    }

    if(ui_theme_is_studio() && s_printer_file_popup) {
        if(s_studio_file_hint)lv_obj_add_flag(s_studio_file_hint,LV_OBJ_FLAG_HIDDEN);
        s_file_detail_popup=studio_plane(s_printer_file_popup,624,0,352,424);
        ui_thumbnail_t *view=ui_thumbnail_create(s_file_detail_popup,0,0,352,176);
        if(!view){ui_files_close_detail_popup();return;}
        ui_thumbnail_set_placeholder(view,"PRINT\nTHUMBNAIL");
        if(thumb_box_out)*thumb_box_out=ui_thumbnail_box(view);
        if(thumb_view_out)*thumb_view_out=view;
        s_detail_preview_box=ui_thumbnail_box(view);
        lv_obj_add_event_cb(s_detail_preview_box,detail_field_deleted,LV_EVENT_DELETE,NULL);
        lv_obj_add_event_cb(s_detail_preview_box,detail_preview_resized,LV_EVENT_SIZE_CHANGED,NULL);
        lv_obj_t *filename=studio_text(s_file_detail_popup,filename_text && filename_text[0] ? filename_text : "Unnamed file",0,188,352,UI_FONT_TITLE,UI_TEXT);
        s_detail_filename_label=filename;
        lv_obj_add_event_cb(filename,detail_field_deleted,LV_EVENT_DELETE,NULL);
        lv_label_set_long_mode(filename,LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_t *metadata=studio_plane(s_file_detail_popup,0,228,352,128);
        file_metadata_scroll_style(metadata);
        lv_obj_set_style_bg_color(metadata,UI_BG,0);
        lv_obj_set_style_bg_opa(metadata,LV_OPA_COVER,0);
        s_detail_info_label=studio_text(metadata,metadata_text,0,0,344,UI_FONT_CAPTION,UI_TEXT_DIM);
        lv_label_set_long_mode(s_detail_info_label,LV_LABEL_LONG_WRAP);
        lv_obj_set_height(s_detail_info_label,LV_SIZE_CONTENT);
        s_detail_start_button=studio_action(s_file_detail_popup,"Print",0,372,128,52,detail_start_event_cb,NULL);
        lv_obj_add_state(s_detail_start_button,LV_STATE_DISABLED);
        s_detail_more_button=studio_action(s_file_detail_popup,"Details",140,372,100,52,studio_metadata_open,NULL);
        s_detail_cancel_button=studio_action(s_file_detail_popup,"Cancel",252,372,100,52,detail_cancel_event_cb,NULL);
        lv_obj_add_event_cb(s_detail_more_button,detail_field_deleted,LV_EVENT_DELETE,NULL);
        lv_obj_add_event_cb(s_detail_cancel_button,detail_field_deleted,LV_EVENT_DELETE,NULL);
        lv_obj_t *connection=studio_text(s_file_detail_popup,"",0,344,352,UI_FONT_CAPTION,UI_WARN);
        lv_label_set_long_mode(connection,LV_LABEL_LONG_WRAP);
        detail_watch(connection);
        ui_files_update_detail_metadata(metadata_text,false);
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
    char *display_metadata=file_detail_display_text(metadata_text);
    const char *detail=display_metadata?display_metadata:(metadata_text?metadata_text:"Metadata unavailable.");
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
    ui_value_set_text(s_detail_info_label,detail);
    free(display_metadata);
    ui_apply_custom_label_style(s_detail_info_label,UI_FONT_BODY,UI_TEXT);
    lv_obj_set_width(s_detail_info_label,LV_PCT(100));
    lv_label_set_long_mode(s_detail_info_label,LV_LABEL_LONG_WRAP);
    lv_obj_t *connection=lv_label_create(s_file_detail_popup);
    ui_apply_custom_label_style(connection,UI_FONT_CAPTION,UI_WARN);
    lv_obj_set_width(connection,LV_PCT(100));lv_label_set_long_mode(connection,LV_LABEL_LONG_WRAP);
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
    detail_watch(connection);
    lv_obj_move_foreground(s_file_detail_popup);

}

void ui_files_update_detail_metadata(
    const char *metadata_text,
    bool ready)
{
    if (!s_file_detail_popup || !s_detail_info_label ||
        s_detail_generation!=moonraker_config_generation()) return;

    char *display_metadata=file_detail_display_text(metadata_text);
    const char *detail=display_metadata?display_metadata:(metadata_text && metadata_text[0]?metadata_text:"Metadata unavailable.");
    ui_value_set_text(s_detail_info_label,detail);
    if(s_studio_metadata_label)ui_value_set_text(s_studio_metadata_label,detail);
    free(display_metadata);
    detail_update_more_action();

    if (s_detail_start_button) {
        s_detail_metadata_ready=ready;
        detail_connection_refresh(NULL);
    }
}
