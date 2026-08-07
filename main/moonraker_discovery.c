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
#include <stdlib.h>
#include <string.h>

static lv_obj_t *s_popup = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_results_list = NULL;
static size_t s_candidate_count = 0;

typedef struct {
    char host[32];
    char identity[MOONRAKER_PROBE_IDENTITY_LENGTH];
    bool klippy_ready;
    int port;
} moonraker_discovery_endpoint_t;

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

    const moonraker_discovery_endpoint_t *endpoint =
        (const moonraker_discovery_endpoint_t *)
            lv_event_get_user_data(e);

    if (endpoint && s_select_cb) {
        s_select_cb(
            endpoint->host,
            endpoint->port,
            endpoint->identity);
    }
}


static void candidate_data_deleted_cb(lv_event_t *e)
{
    if (!e || lv_event_get_code(e) != LV_EVENT_DELETE) return;

    free(lv_event_get_user_data(e));
}


static bool host_already_seen(char seen[][32], int seen_count, const char *host)
{
    for (int i = 0; i < seen_count; i++) {
        if (strcmp(seen[i], host) == 0) return true;
    }
    return false;
}

static bool moonraker_discovery_add_candidate(
    const char *host,
    int port,
    const moonraker_probe_result_t *probe);


static void scan_task(void *arg)
{
    (void)arg;

    static const int ports[] = {
        7125,
        7126,
        7127,
        7128,
    };

    uint8_t a = esp_ip4_addr1(&s_scan_ip);
    uint8_t b = esp_ip4_addr2(&s_scan_ip);
    uint8_t c = esp_ip4_addr3(&s_scan_ip);
    int self_d = esp_ip4_addr4(&s_scan_ip);

    char seen[96][32];
    int seen_count = 0;
    int found_count = 0;

    char msg[512];
    snprintf(
        msg,
        sizeof(msg),
        "Scanning %u.%u.%u.x on ports 7125-7128. Close cancels discovery.",
        a,
        b,
        c);
    moonraker_discovery_set_status(msg);

    int candidates[96];
    int cand_count = 0;

    /* Fast useful guesses first. */
    candidates[cand_count++] = 1;
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

    /* Broader but still limited fallback. */
    for (int d = 2; d <= 254 && cand_count < 96; d += 5) {
        candidates[cand_count++] = d;
    }

    for (int i = 0; i < cand_count && found_count < 8; i++) {
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

        bool open_ports[
            sizeof(ports) / sizeof(ports[0])] = {0};

        snprintf(
            msg,
            sizeof(msg),
            "Checking %s on ports 7125-7128   |   %d found",
            host,
            found_count);
        moonraker_discovery_set_status(msg);

        /*
         * Four non-blocking TCP attempts share one short wait, replacing four
         * full HTTP timeouts on addresses where no printer exists.
         */
        int tcp_timeout_ms = found_count == 0 ? 450 : 220;
        moonraker_probe_open_ports(
            host,
            ports,
            sizeof(ports) / sizeof(ports[0]),
            tcp_timeout_ms,
            open_ports,
            sizeof(open_ports) / sizeof(open_ports[0]));

        for (size_t port_index = 0;
             port_index < sizeof(ports) / sizeof(ports[0]) &&
             found_count < 8;
             ++port_index) {
            if (moonraker_discovery_is_cancelled()) break;
            if (!open_ports[port_index]) continue;

            int port = ports[port_index];

            moonraker_probe_result_t probe = {0};
            if (!moonraker_probe_endpoint(host, port, &probe)) continue;

            bool published = false;

            while (!moonraker_discovery_is_cancelled() &&
                   moonraker_discovery_is_open()) {
                if (bsp_display_lock(100)) {
                    published = moonraker_discovery_add_candidate(
                        host,
                        port,
                        &probe);
                    bsp_display_unlock();
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(10));
            }

            if (!published) {
                break;
            }

            found_count++;

            snprintf(
                msg,
                sizeof(msg),
                "%d Moonraker endpoint%s found. Select one below.",
                found_count,
                found_count == 1 ? "" : "s");
            moonraker_discovery_set_status(msg);
        }

        vTaskDelay(pdMS_TO_TICKS(15));
    }

    if (!moonraker_discovery_is_cancelled()) {
        if (found_count == 0) {
            snprintf(
                msg,
                sizeof(msg),
                "No Moonraker endpoints found on %u.%u.%u.x ports "
                "7125-7128. Verify the printer address and try again.",
                a,
                b,
                c);
        } else {
            snprintf(
                msg,
                sizeof(msg),
                "Discovery complete   |   %d endpoint%s found",
                found_count,
                found_count == 1 ? "" : "s");
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

    s_popup = ui_popup_create(
        lv_layer_top(),
        860,
        540,
        UI_POPUP_STANDARD);

    if (!s_popup) return;

    ui_popup_add_title(
        s_popup,
        LV_SYMBOL_WIFI " DISCOVER MOONRAKER",
        false,
        8);

    ui_popup_add_header_divider(s_popup, 48);

    s_status_label = ui_popup_add_status_label(
        s_popup,
        s_status,
        24,
        58,
        812);

    if (s_status_label) {
        lv_obj_set_height(s_status_label, 44);
    }

    ui_popup_add_caption(
        s_popup,
        "SELECT A VERIFIED PRINTER. READY MEANS KLIPPER RESPONDED.",
        24,
        106,
        812);

    s_results_list = ui_popup_add_list(
        s_popup,
        24,
        132,
        812,
        324);

    if (!s_status_label || !s_results_list) {
        lv_obj_delete(s_popup);
        s_popup = NULL;
        s_status_label = NULL;
        s_results_list = NULL;
        return;
    }

    s_candidate_count = 0;

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


static bool moonraker_discovery_add_candidate(
    const char *host,
    int port,
    const moonraker_probe_result_t *probe)
{
    if (!s_results_list ||
        !host ||
        !host[0] ||
        port <= 0 ||
        port >= 65536) {
        return false;
    }

    moonraker_discovery_endpoint_t *endpoint =
        calloc(1, sizeof(*endpoint));

    if (!endpoint) return false;

    copy_text(endpoint->host, sizeof(endpoint->host), host);
    endpoint->port = port;
    endpoint->klippy_ready = probe && probe->klippy_ready;
    copy_text(
        endpoint->identity,
        sizeof(endpoint->identity),
        probe && probe->identity[0] ? probe->identity : "Moonraker");

    char row[160];
    snprintf(
        row,
        sizeof(row),
        LV_SYMBOL_WIFI "  %s  |  %s:%d  |  %s",
        endpoint->identity,
        endpoint->host,
        endpoint->port,
        endpoint->klippy_ready ? "READY" : "MOONRAKER");

    int32_t row_width = lv_obj_get_width(s_results_list) - 16;
    if (row_width <= 0) row_width = 780;

    lv_obj_t *button = ui_popup_add_selectable_row(
        s_results_list,
        row,
        8,
        8 + (int32_t)s_candidate_count * 60,
        row_width,
        52,
        candidate_event_cb,
        endpoint);

    if (!button) {
        free(endpoint);
        return false;
    }

    lv_obj_add_event_cb(
        button,
        candidate_data_deleted_cb,
        LV_EVENT_DELETE,
        endpoint);

    s_candidate_count++;
    return true;
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
        s_results_list = NULL;
        s_candidate_count = 0;
    }

    if (s_close_cb) {
        s_close_cb();
    }
}
