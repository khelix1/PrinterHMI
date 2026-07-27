#include "files_page_controller.h"

#include "moonraker.h"
#include "printer_files.h"
#include "files_row_preview_v32.h"
#include "ui_files_v32.h"
#include "ui_theme.h"
#include "moonraker_live_websocket.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

#define FILES_PAGE_LIST_CAPACITY 16384
#define FILES_PAGE_ENTRY_CAPACITY 64

static const char *TAG = "files_page_controller";

typedef enum {
    FILE_SORT_NAME = 0,
    FILE_SORT_NEWEST,
    FILE_SORT_SIZE,
} file_sort_mode_t;

static printer_file_entry_t *s_entries;
static size_t s_entry_count;
static char s_folder[PRINTER_FILES_MAX_PATH];
static char s_search[64];
static file_sort_mode_t s_sort_mode;

static void files_page_controller_preview_ready(
    const char *file,
    const lv_image_dsc_t *image)
{
    ui_files_v32_set_file_thumbnail(file, image);
}

void files_page_controller_request_preview(const char *path)
{
    files_row_preview_v32_request(path);
}

static bool contains_ci(const char *text, const char *needle)
{
    if (!needle || !needle[0]) return true;
    if (!text) return false;
    for (; *text; ++text) {
        const char *a = text;
        const char *b = needle;
        while (*a && *b &&
               tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
            ++a;
            ++b;
        }
        if (!*b) return true;
    }
    return false;
}

static int compare_name(const void *left, const void *right)
{
    const printer_file_entry_t *a = *(const printer_file_entry_t * const *)left;
    const printer_file_entry_t *b = *(const printer_file_entry_t * const *)right;
    return strcasecmp(a->path, b->path);
}

static int compare_newest(const void *left, const void *right)
{
    const printer_file_entry_t *a = *(const printer_file_entry_t * const *)left;
    const printer_file_entry_t *b = *(const printer_file_entry_t * const *)right;
    return a->modified < b->modified ? 1 : (a->modified > b->modified ? -1 : 0);
}

static int compare_size(const void *left, const void *right)
{
    const printer_file_entry_t *a = *(const printer_file_entry_t * const *)left;
    const printer_file_entry_t *b = *(const printer_file_entry_t * const *)right;
    return a->size < b->size ? 1 : (a->size > b->size ? -1 : 0);
}

static int compare_folder_name(const void *left, const void *right)
{
    return strcasecmp((const char *)left, (const char *)right);
}

static void render_entries(void)
{
    ui_files_v32_clear_rows();
    ui_files_v32_set_breadcrumb(s_folder);
    ui_files_v32_set_search_text(s_search);
    ui_files_v32_set_sort_text(
        s_sort_mode == FILE_SORT_NEWEST ? "NEWEST" :
        s_sort_mode == FILE_SORT_SIZE ? "SIZE" : "NAME");

    printer_file_entry_t *visible[FILES_PAGE_ENTRY_CAPACITY];
    size_t visible_count = 0;
    char folders[24][80];
    size_t folder_count = 0;
    size_t prefix_length = strlen(s_folder);

    for (size_t i = 0; i < s_entry_count; ++i) {
        const char *path = s_entries[i].path;

        /* Search is library-wide so files inside folders are discoverable. */
        if (s_search[0]) {
            if (contains_ci(path, s_search) &&
                visible_count < FILES_PAGE_ENTRY_CAPACITY) {
                visible[visible_count++] = &s_entries[i];
            }
            continue;
        }

        if (prefix_length) {
            if (strncmp(path, s_folder, prefix_length) != 0 ||
                path[prefix_length] != '/') continue;
            path += prefix_length + 1;
        }

        const char *slash = strchr(path, '/');
        if (slash) {
            size_t length = (size_t)(slash - path);
            if (length == 0 || length >= sizeof(folders[0])) continue;
            char name[80];
            memcpy(name, path, length);
            name[length] = '\0';
            bool duplicate = false;
            for (size_t f = 0; f < folder_count; ++f) {
                if (strcasecmp(folders[f], name) == 0) duplicate = true;
            }
            if (!duplicate && folder_count < 24 && contains_ci(name, s_search)) {
                snprintf(folders[folder_count++], sizeof(folders[0]), "%s", name);
            }
        } else if (contains_ci(path, s_search) &&
                   visible_count < FILES_PAGE_ENTRY_CAPACITY) {
            visible[visible_count++] = &s_entries[i];
        }
    }

    qsort(folders, folder_count, sizeof(folders[0]), compare_folder_name);
    qsort(visible, visible_count, sizeof(visible[0]),
          s_sort_mode == FILE_SORT_NEWEST ? compare_newest :
          s_sort_mode == FILE_SORT_SIZE ? compare_size : compare_name);

    int y = 0;
    for (size_t i = 0; i < folder_count; ++i) {
        char full_path[PRINTER_FILES_MAX_PATH];
        full_path[0] = '\0';

        if (s_folder[0]) {
            strlcpy(full_path, s_folder, sizeof(full_path));
            strlcat(full_path, "/", sizeof(full_path));
        }

        strlcat(full_path, folders[i], sizeof(full_path));
        ui_files_v32_add_folder_button(folders[i], full_path, y);
        y += ui_theme_density_metric(66, 78, 90);
    }
    for (size_t i = 0; i < visible_count; ++i) {
        ui_files_v32_add_file_entry(visible[i]->path,
                                    visible[i]->size,
                                    visible[i]->modified,
                                    y);
        y += ui_theme_density_metric(88, 106, 118);
    }

    if (folder_count == 0 && visible_count == 0) {
        ui_files_v32_set_status(
            s_search[0] ? "No files match the current search."
                        : "No files found in Moonraker gcodes root.");
    } else {
        ui_files_v32_set_status("");
    }
}

void files_page_controller_set_search(const char *query)
{
    const char *start = query ? query : "";
    while (*start && isspace((unsigned char)*start)) start++;

    size_t length = strlen(start);
    while (length && isspace((unsigned char)start[length - 1])) length--;
    if (length >= sizeof(s_search)) length = sizeof(s_search) - 1;

    memcpy(s_search, start, length);
    s_search[length] = '\0';
    render_entries();
}

void files_page_controller_cycle_sort(void)
{
    s_sort_mode = (file_sort_mode_t)((s_sort_mode + 1) % 3);
    render_entries();
}

void files_page_controller_open_folder(const char *path)
{
    snprintf(s_folder, sizeof(s_folder), "%s", path ? path : "");
    render_entries();
}

void files_page_controller_up_folder(void)
{
    char *slash = strrchr(s_folder, '/');
    if (slash) *slash = '\0';
    else s_folder[0] = '\0';
    render_entries();
}

static bool ensure_entries(void)
{
    if (s_entries) return true;
    s_entries = heap_caps_calloc(
        FILES_PAGE_ENTRY_CAPACITY,
        sizeof(*s_entries),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_entries) {
        s_entries = calloc(FILES_PAGE_ENTRY_CAPACITY, sizeof(*s_entries));
    }
    return s_entries != NULL;
}

static void fallback_add_path(const char *path, void *user)
{
    size_t *count = user;
    if (!path || !count || *count >= FILES_PAGE_ENTRY_CAPACITY) return;
    snprintf(s_entries[*count].path, sizeof(s_entries[*count].path), "%s", path);
    (*count)++;
}

void files_page_controller_reload(
    bool wifi_connected,
    bool moonraker_connected,
    bool sd_available,
    const char *host,
    int port,
    const char *api_key)
{
    if (!wifi_connected) {
        ui_files_v32_set_status("WiFi offline. Connect before loading files.");
        return;
    }

    if (!moonraker_connected) {
        ui_files_v32_set_status("Moonraker offline. Check the active printer.");
        return;
    }

    if (!host || !host[0]) {
        ui_files_v32_set_status(
            "Moonraker host is not configured.");
        return;
    }

    if (!api_key) {
        api_key = "";
    }

    ui_files_v32_set_browser_callbacks(
        files_page_controller_set_search,
        files_page_controller_cycle_sort,
        files_page_controller_open_folder,
        files_page_controller_up_folder);

    if (!ensure_entries()) {
        ui_files_v32_set_status("Unable to allocate the file browser index.");
        return;
    }

    files_row_preview_v32_begin(
        host,
        port,
        api_key,
        sd_available,
        files_page_controller_preview_ready);

    ui_files_v32_set_status("Loading files...");

    char *file_list_body = heap_caps_malloc(
        FILES_PAGE_LIST_CAPACITY,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!file_list_body) {
        file_list_body = heap_caps_malloc(
            FILES_PAGE_LIST_CAPACITY,
            MALLOC_CAP_8BIT);
    }

    if (!file_list_body) {
        ui_files_v32_set_status(
            "Unable to allocate file-list buffer.");
        return;
    }

    memset(
        file_list_body,
        0,
        FILES_PAGE_LIST_CAPACITY);

    int http_code = 0;
    esp_err_t transport_error = ESP_FAIL;

    bool fetched = moonraker_fetch_file_list(
        host,
        port,
        api_key,
        file_list_body,
        FILES_PAGE_LIST_CAPACITY,
        &http_code,
        &transport_error);

    if (!fetched) {
        char message[160];

        snprintf(
            message,
            sizeof(message),
            "File list failed.\nHTTP %d\n%s",
            http_code,
            esp_err_to_name(transport_error));

        ui_files_v32_set_status(message);
        heap_caps_free(file_list_body);
        return;
    }

    memset(s_entries, 0,
           FILES_PAGE_ENTRY_CAPACITY * sizeof(*s_entries));
    int count = printer_files_parse_entries(
        file_list_body,
        s_entries,
        FILES_PAGE_ENTRY_CAPACITY);
    if (count == 0) {
        size_t fallback_count = 0;
        printer_files_for_each_path(file_list_body,
                                    fallback_add_path,
                                    &fallback_count);
        count = (int)fallback_count;
    }
    s_entry_count = count > 0 ? (size_t)count : 0;

    heap_caps_free(file_list_body);

    if (count == 0) {
        ui_files_v32_set_status(
            "No files found in Moonraker gcodes root.");
    } else {
        render_entries();
    }
}

void files_page_controller_process_live_notification(void)
{
    if (!moonraker_live_websocket_file_change_pending()) {
        return;
    }

    if (!ui_files_v32_get_popup()) {
        /*
         * Opening Files always performs a fresh HTTP reload, so a notification
         * received while the page is hidden does not need to remain pending.
         */
        (void)moonraker_live_websocket_take_file_change();
        return;
    }

    if (ui_files_v32_detail_is_open()) {
        /*
         * Preserve the confirmation/detail popup. The pending notification is
         * consumed by a later refresh cycle after the popup closes.
         */
        return;
    }

    if (!moonraker_live_websocket_take_file_change()) {
        return;
    }

    ESP_LOGI(TAG, "WS_FILELIST_REFRESH visible Files page");
    ui_files_v32_refresh();
}
