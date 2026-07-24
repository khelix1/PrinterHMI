#include "moonraker_discovery.h"
#include "ui_button.h"
#include "ui_popup.h"

#include "lvgl.h"
#include "ui_theme.h"
#include "bsp/esp-bsp.h"
#include "moonraker_probe.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static lv_obj_t *s_popup = NULL;
static lv_obj_t *s_status_label = NULL;

static char s_status[512] = "Moonraker scan: not run";

static volatile bool s_cancelled = false;
static TaskHandle_t s_scan_task_handle = NULL;
static esp_ip4_addr_t s_scan_ip;

static moonraker_discovery_close_cb_t s_close_cb = NULL;
static moonraker_discovery_select_cb_t s_select_cb = NULL;


static void copy_text(char *dst,
                      size_t dst_size,
                      const char *src)
{
    if (!dst || dst_size == 0) return;
    if (!src) src = "";

    strlcpy(dst, src, dst_size);
}


static void close_event_cb(lv_event_t *e)
{
    (void)e;
    moonraker_discovery_close();
}


static void candidate_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    const char *host =
        (const char *)lv_event_get_user_data(e);

    if (host && s_select_cb) {
        s_select_cb(host);
    }
}


static bool host_already_seen(char seen[][32], int seen_count, const char *host)
{
    for (int i = 0; i < seen_count; i++) {
        if (strcmp(seen[i], host) == 0) return true;
    }
    return false;
}

static void scan_task(void *arg)
{
    (void)arg;

    uint8_t a = esp_ip4_addr1(&s_scan_ip);
    uint8_t b = esp_ip4_addr2(&s_scan_ip);
    uint8_t c = esp_ip4_addr3(&s_scan_ip);
    int self_d = esp_ip4_addr4(&s_scan_ip);

    char seen[96][32];
    int seen_count = 0;
    int found_count = 0;
    int y = 115;

    char msg[512];
    snprintf(msg, sizeof(msg),
             "Scanning for Moonraker on port 7125...\n"
             "Close cancels discovery.\n");
    moonraker_discovery_set_status(msg);

    int candidates[96];
    int cand_count = 0;

    /* Fast useful guesses first */
    candidates[cand_count++] = 1;      /* gateway/router area */
    candidates[cand_count++] = 100;
    candidates[cand_count++] = 101;
    candidates[cand_count++] = 110;
    candidates[cand_count++] = 120;
    candidates[cand_count++] = 121;
    candidates[cand_count++] = 122;
    candidates[cand_count++] = 123;
    candidates[cand_count++] = 124;
    candidates[cand_count++] = 125;

    for (int d = self_d - 20; d <= self_d + 20; d++) {
        if (d > 0 && d < 255 && cand_count < 40) {
            candidates[cand_count++] = d;
        }
    }

    /* Broader but still limited fallback */
    for (int d = 2; d <= 254 && cand_count < 96; d += 5) {
        candidates[cand_count++] = d;
    }

    for (int i = 0; i < cand_count; i++) {
        if (moonraker_discovery_is_cancelled()) break;

        int d = candidates[i];
        if (d <= 0 || d >= 255) continue;

        char host[32];
        snprintf(host, sizeof(host), "%u.%u.%u.%d", a, b, c, d);

        if (host_already_seen(seen, seen_count, host)) continue;
        if (seen_count < 96) {
            copy_text(seen[seen_count], sizeof(seen[seen_count]), host);
            seen_count++;
        }

        snprintf(msg, sizeof(msg),
                 "Scanning for Moonraker on port 7125...\n"
                 "Checking %s\n"
                 "Found: %d\n\n"
                 "Close cancels discovery.",
                 host, found_count);
        moonraker_discovery_set_status(msg);

        if (moonraker_probe_host(host, 7125)) {
            found_count++;

            if (!moonraker_discovery_is_cancelled() &&
                moonraker_discovery_is_open() &&
                bsp_display_lock(50)) {
                moonraker_discovery_add_candidate(host, y);
                bsp_display_unlock();
            }

            y += 58;

            snprintf(msg, sizeof(msg),
                     "Found Moonraker candidates:\n"
                     "%d found so far.\n\n"
                     "Tap one to save it. Close cancels discovery.",
                     found_count);
            moonraker_discovery_set_status(msg);

            if (found_count >= 8) break;
        }

        vTaskDelay(pdMS_TO_TICKS(15));
    }

    if (!moonraker_discovery_is_cancelled()) {
        if (found_count == 0) {
            snprintf(msg, sizeof(msg),
                     "No Moonraker found.\n"
                     "Checked smart targets on %u.%u.%u.x:7125.\n\n"
                     "Try TEST MOONRAKER or check host IP.",
                     a, b, c);
        } else {
            snprintf(msg, sizeof(msg),
                     "Moonraker discovery complete.\n"
                     "%d candidate(s) found.\n\n"
                     "Tap one to save it.",
                     found_count);
        }
        moonraker_discovery_set_status(msg);
    }

    s_scan_task_handle = NULL;
    vTaskDelete(NULL);
}

void moonraker_discovery_show(
    const char *status_text,
    moonraker_discovery_close_cb_t close_cb,
    moonraker_discovery_select_cb_t select_cb)
{
    s_close_cb = close_cb;
    s_select_cb = select_cb;

    if (status_text) {
        copy_text(s_status, sizeof(s_status), status_text);
    }

    if (s_popup) {
        lv_obj_move_foreground(s_popup);

        if (s_status_label) {
            lv_label_set_text(s_status_label, s_status);
        }

        return;
    }

    s_popup =
        ui_popup_create(
            lv_screen_active(),
            860,
            540,
            UI_POPUP_STANDARD);

    if (!s_popup) {
        return;
    }

    lv_obj_t *title =
        ui_popup_add_title(
            s_popup,
            "MOONRAKER HOSTS",
            false,
            4);

    if (title) {
        lv_obj_align(
            title,
            LV_ALIGN_TOP_LEFT,
            25,
            4);
    }

    ui_popup_add_header_divider(
        s_popup,
        48);

    s_status_label =
        ui_popup_add_status_label(
            s_popup,
            s_status,
            18,
            58,
            800);

    ui_popup_add_standard_footer_divider(s_popup);

    ui_popup_add_footer_action(
        s_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        160,
        UI_POPUP_FOOTER_CENTER,
        close_event_cb,
        NULL,
        NULL);
}


void moonraker_discovery_set_status(const char *status_text)
{
    if (!status_text) return;

    copy_text(s_status, sizeof(s_status), status_text);

    if (s_status_label && bsp_display_lock(50)) {
        lv_label_set_text(s_status_label, s_status);
        bsp_display_unlock();
    }
}


void moonraker_discovery_add_candidate(
    const char *host,
    int y)
{
    if (!s_popup || !host || !host[0]) return;

    char row[80];
    snprintf(row, sizeof(row), "%s : 7125", host);

    /*
     * Keep the existing copied user-data lifetime while delegating
     * all candidate-row appearance to the popup theme.
     */
    char *host_copy = strdup(host);

    if (!host_copy) {
        return;
    }

    lv_obj_t *button =
        ui_popup_add_selectable_row(
            s_popup,
            row,
            30,
            y,
            780,
            52,
            candidate_event_cb,
            host_copy);

    if (!button) {
        free(host_copy);
    }
}


bool moonraker_discovery_start(const esp_ip4_addr_t *ip)
{
    if (!ip || s_scan_task_handle) return false;

    s_scan_ip = *ip;
    s_cancelled = false;

    if (xTaskCreate(scan_task, "moon_scan", 8192, NULL, 4,
                    &s_scan_task_handle) != pdPASS) {
        s_scan_task_handle = NULL;
        return false;
    }

    return true;
}

bool moonraker_discovery_is_running(void)
{
    return s_scan_task_handle != NULL;
}

bool moonraker_discovery_is_cancelled(void)
{
    return s_cancelled;
}


bool moonraker_discovery_is_open(void)
{
    return s_popup != NULL;
}


void moonraker_discovery_close(void)
{
    s_cancelled = true;

    if (s_popup) {
        lv_obj_delete(s_popup);
        s_popup = NULL;
        s_status_label = NULL;
    }

    if (s_close_cb) {
        s_close_cb();
    }
}
