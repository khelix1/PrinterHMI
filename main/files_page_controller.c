#include "files_page_controller.h"

#include "moonraker.h"
#include "printer_files.h"
#include "files_row_preview.h"
#include "ui_files.h"
#include "ui_theme.h"
#include "moonraker_live_websocket.h"
#include "moonraker_config_controller.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"

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
    ui_files_set_file_thumbnail(file, image);
}

void files_page_controller_request_preview(const char *path)
{
    files_row_preview_request(path);
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
    ui_files_clear_rows();
    ui_files_set_breadcrumb(s_folder);
    ui_files_set_search_text(s_search);
    ui_files_set_sort_text(
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
        ui_files_add_folder_button(folders[i], full_path, y);
        y += ui_theme_density_metric(66, 78, 90);
    }
    for (size_t i = 0; i < visible_count; ++i) {
        ui_files_add_file_entry(visible[i]->path,
                                    visible[i]->size,
                                    visible[i]->modified,
                                    y);
        y += ui_theme_density_metric(88, 106, 118);
    }

    if (folder_count == 0 && visible_count == 0) {
        ui_files_set_status(
            s_search[0] ? "No files match the current search."
                        : "No files found in Moonraker gcodes root.");
    } else {
        ui_files_set_status("");
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

typedef struct {
    char host[MOONRAKER_CONFIG_HOST_LENGTH];
    char api_key[MOONRAKER_CONFIG_API_KEY_LENGTH];
    int port;
    uint32_t generation;
    uint32_t request;
    bool sd_available;
    lv_obj_t *page;
    char *body;
    bool fetched;
    int http_code;
    esp_err_t error;
} files_load_job_t;

static QueueHandle_t s_load_results;
static lv_timer_t *s_load_timer;
static files_load_job_t *s_pending_load;
static bool s_load_busy;
static uint32_t s_load_request;

static void files_load_free(files_load_job_t *job)
{
    if (!job) return;
    heap_caps_free(job->body);
    heap_caps_free(job);
}

static void files_load_worker(void *argument)
{
    files_load_job_t *job = argument;
    job->body = heap_caps_malloc(FILES_PAGE_LIST_CAPACITY,
                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!job->body) job->body = heap_caps_malloc(FILES_PAGE_LIST_CAPACITY,
                                               MALLOC_CAP_8BIT);
    job->error = ESP_FAIL;
    if (job->body) {
        for (int attempt = 0; attempt < 3 && !job->fetched; ++attempt) {
            if (job->generation != moonraker_config_generation()) break;
            memset(job->body, 0, FILES_PAGE_LIST_CAPACITY);
            job->fetched = moonraker_fetch_file_list(
                job->host, job->port, job->api_key, job->body,
                FILES_PAGE_LIST_CAPACITY, &job->http_code, &job->error);
            if (!job->fetched && attempt < 2) vTaskDelay(pdMS_TO_TICKS(180));
        }
    }
    /* One worker owns one result slot; LVGL consumes before launching another. */
    (void)xQueueSend(s_load_results, &job, portMAX_DELAY);
    vTaskDelete(NULL);
}

static void files_load_poll(lv_timer_t *timer);

static bool files_load_launch(files_load_job_t *job)
{
    if (!s_load_results) s_load_results = xQueueCreate(1, sizeof(job));
    if (!s_load_results) return false;
    if (!s_load_timer) s_load_timer = lv_timer_create(files_load_poll, 50, NULL);
    if (!s_load_timer) return false;
    s_load_busy = true;
    if (xTaskCreatePinnedToCoreWithCaps(files_load_worker, "files_load", 8192,
            job, 3, NULL, 0,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        s_load_busy = false;
        return false;
    }
    return true;
}

static void files_load_publish(files_load_job_t *job)
{
    if (!job->fetched) {
        char message[160];
        snprintf(message, sizeof(message), "File list failed.\nHTTP %d\n%s",
                 job->http_code, job->body ? esp_err_to_name(job->error) :
                 "Unable to allocate file-list buffer.");
        ui_files_set_status(message);
        return;
    }
    files_row_preview_begin(job->host, job->port, job->api_key,
                            job->sd_available, files_page_controller_preview_ready);
    memset(s_entries, 0,
           FILES_PAGE_ENTRY_CAPACITY * sizeof(*s_entries));
    int count = printer_files_parse_entries(
        job->body,
        s_entries,
        FILES_PAGE_ENTRY_CAPACITY);
    if (count == 0) {
        size_t fallback_count = 0;
        printer_files_for_each_path(job->body,
                                    fallback_add_path,
                                    &fallback_count);
        count = (int)fallback_count;
    }
    s_entry_count = count > 0 ? (size_t)count : 0;

    if (count == 0) {
        ui_files_set_status(
            "No files found in Moonraker gcodes root.");
    } else {
        render_entries();
    }
}

static void files_load_poll(lv_timer_t *timer)
{
    /* Keep the bounded result queued while a file confirmation is open. */
    if (ui_files_get_popup() && ui_files_detail_is_open()) return;
    files_load_job_t *job = NULL;
    if (xQueueReceive(s_load_results, &job, 0) == pdPASS) {
        s_load_busy = false;
        if (job->generation == moonraker_config_generation() &&
            job->request == s_load_request && job->page == ui_files_get_popup() &&
            job->page) files_load_publish(job);
        files_load_free(job);
    }
    if (!s_load_busy && s_pending_load) {
        job = s_pending_load;
        s_pending_load = NULL;
        if (job->generation != moonraker_config_generation() ||
            job->page != ui_files_get_popup() || !job->page) files_load_free(job);
        else if (!files_load_launch(job)) {
            files_load_free(job);
            ui_files_set_status("Unable to start file-list worker.");
        }
    }
    if (!s_load_busy && !s_pending_load) {
        s_load_timer = NULL;
        lv_timer_delete(timer);
    }
}

void files_page_controller_reload(
    bool wifi_connected, bool moonraker_connected, bool sd_available,
    const char *host, int port, const char *api_key)
{
    ++s_load_request;
    files_load_free(s_pending_load);
    s_pending_load = NULL;
    if (!wifi_connected) {
        ui_files_set_status("WiFi offline. Connect before loading files.");
        return;
    }
    if (!moonraker_connected) {
        ui_files_set_status("Moonraker offline. Check the active printer.");
        return;
    }
    if (!host || !host[0]) {
        ui_files_set_status("Moonraker host is not configured.");
        return;
    }
    ui_files_set_browser_callbacks(files_page_controller_set_search,
        files_page_controller_cycle_sort, files_page_controller_open_folder,
        files_page_controller_up_folder);
    if (!ensure_entries()) {
        ui_files_set_status("Unable to allocate the file browser index.");
        return;
    }
    files_load_job_t *job = heap_caps_calloc(1, sizeof(*job),
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!job) {
        ui_files_set_status("Unable to allocate file-list job.");
        return;
    }
    snprintf(job->host, sizeof(job->host), "%s", host);
    snprintf(job->api_key, sizeof(job->api_key), "%s", api_key ? api_key : "");
    job->port = port;
    job->generation = moonraker_config_generation();
    job->request = s_load_request;
    job->page = ui_files_get_popup();
    job->sd_available = sd_available;
    ui_files_set_status("Loading files...");
    if (s_load_busy) s_pending_load = job;
    else if (!files_load_launch(job)) {
        files_load_free(job);
        ui_files_set_status("Unable to start file-list worker.");
    }
}

void files_page_controller_process_live_notification(void)
{
    if (!moonraker_live_websocket_file_change_pending()) {
        return;
    }

    if (!ui_files_get_popup()) {
        /*
         * Opening Files always performs a fresh HTTP reload, so a notification
         * received while the page is hidden does not need to remain pending.
         */
        (void)moonraker_live_websocket_take_file_change();
        return;
    }

    if (ui_files_detail_is_open()) {
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
    ui_files_refresh();
}
