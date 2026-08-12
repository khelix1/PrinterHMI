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


static bool moonraker_discovery_add_candidate(
    const char *host,
    int port,
    const moonraker_probe_result_t *probe);




#define DISCOVERY_HOST_BATCH_SIZE 1

static void discovery_add_candidate_address(
    int *candidates,
    size_t *candidate_count,
    int host_octet,
    int self_octet)
{
    if (!candidates || !candidate_count || host_octet <= 0 ||
        host_octet >= 255 || host_octet == self_octet) {
        return;
    }

    for (size_t index = 0; index < *candidate_count; ++index) {
        if (candidates[index] == host_octet) {
            return;
        }
    }

    candidates[(*candidate_count)++] = host_octet;
}


static void scan_task(void *arg)
{
    (void)arg;

    static const int ports[] = {
        7125,
        7126,
        7127,
        7128,
        443,
        444,
        445,
        446,
    };

    uint8_t a = esp_ip4_addr1(&s_scan_ip);
    uint8_t b = esp_ip4_addr2(&s_scan_ip);
    uint8_t c = esp_ip4_addr3(&s_scan_ip);
    int self_d = esp_ip4_addr4(&s_scan_ip);

    /*
     * Start with common addresses and nearby hosts for fast first results,
     * then append every remaining usable address exactly once.
     */
    int candidates[254];
    size_t candidate_count = 0;
    static const int priority_hosts[] = {
        1, 100, 101, 110, 120, 121, 122, 123, 124, 125,
    };

    for (size_t index = 0;
         index < sizeof(priority_hosts) / sizeof(priority_hosts[0]);
         ++index) {
        discovery_add_candidate_address(
            candidates, &candidate_count, priority_hosts[index], self_d);
    }

    for (int host = self_d - 20; host <= self_d + 20; ++host) {
        discovery_add_candidate_address(
            candidates, &candidate_count, host, self_d);
    }

    for (int host = 1; host < 255; ++host) {
        discovery_add_candidate_address(
            candidates, &candidate_count, host, self_d);
    }

    char msg[512];
    snprintf(
        msg,
        sizeof(msg),
            "Scanning all %u.%u.%u.x addresses on ports 443-446 and 7125-7128.",
        a,
        b,
        c);
    moonraker_discovery_set_status(msg);

    size_t found_count = 0;
    size_t next_candidate = 0;

    while (next_candidate < candidate_count &&
           !moonraker_discovery_is_cancelled()) {
        char hosts[DISCOVERY_HOST_BATCH_SIZE][32];
        const char *host_refs[DISCOVERY_HOST_BATCH_SIZE];
        size_t host_count = 0;

        while (next_candidate < candidate_count &&
               host_count < DISCOVERY_HOST_BATCH_SIZE) {
            int host = candidates[next_candidate++];
            snprintf(
                hosts[host_count],
                sizeof(hosts[host_count]),
                "%u.%u.%u.%d",
                a,
                b,
                c,
                host);
            host_refs[host_count] = hosts[host_count];
            host_count++;
        }

        snprintf(
            msg,
            sizeof(msg),
            "Checking %u of %u addresses on ports 443-446, 7125-7128   |   %u found",
            (unsigned)next_candidate,
            (unsigned)candidate_count,
            (unsigned)found_count);
        moonraker_discovery_set_status(msg);

        bool open_ports[
            DISCOVERY_HOST_BATCH_SIZE *
            (sizeof(ports) / sizeof(ports[0]))] = {0};

        /*
         * ESP-Hosted is most reliable with one address worth of short
         * TCP probes at a time. The full scan remains non-blocking for
         * the interface because this worker owns the operation.
         */
        int tcp_timeout_ms = found_count == 0 ? 450 : 220;
        moonraker_probe_open_ports(
            host_refs[0],
            ports,
            sizeof(ports) / sizeof(ports[0]),
            tcp_timeout_ms,
            open_ports,
            sizeof(open_ports) / sizeof(open_ports[0]));

        for (size_t host_index = 0;
             host_index < host_count &&
             !moonraker_discovery_is_cancelled();
             ++host_index) {
            bool host_has_open_port = false;
            for (size_t port_index = 0;
                 port_index < sizeof(ports) / sizeof(ports[0]);
                 ++port_index) {
                if (open_ports[
                        host_index *
                        (sizeof(ports) / sizeof(ports[0])) +
                        port_index]) {
                    host_has_open_port = true;
                    break;
                }
            }

            if (!host_has_open_port) {
                continue;
            }

            /*
             * A shared Klipper host can accept one quick TCP SYN while
             * dropping sibling probes. Once an address is positive, verify
             * every supported Moonraker port serially. This finds separate
             * instances on one host without broad concurrent HTTP traffic.
             */
            for (size_t port_index = 0;
                 port_index < sizeof(ports) / sizeof(ports[0]) &&
                 !moonraker_discovery_is_cancelled();
                 ++port_index) {
                moonraker_probe_result_t probe = {0};
                int port = ports[port_index];
                if (port >= 443 && port <= 446) {
                    /* ESP-Hosted can drop sibling SYNs in the first broad
                     * sweep. Recheck each TLS proxy port alone so multiple
                     * Moonraker instances on one host are deterministic. */
                    bool tls_open = false;
                    (void)moonraker_probe_open_ports(
                        host_refs[host_index], &port, 1, 450,
                        &tls_open, 1);
                    if (tls_open) {
                        strlcpy(probe.identity, "HTTPS proxy (select CA)",
                            sizeof(probe.identity));
                        bool published = false;
                        while (!moonraker_discovery_is_cancelled() &&
                               moonraker_discovery_is_open()) {
                            if (bsp_display_lock(100)) {
                                published = moonraker_discovery_add_candidate(
                                    host_refs[host_index], port, &probe);
                                bsp_display_unlock();
                                break;
                            }
                            vTaskDelay(pdMS_TO_TICKS(10));
                        }
                        if (published) found_count++;
                    }
                    continue;
                }
                if (!moonraker_probe_endpoint(
                        host_refs[host_index], port, &probe)) {
                    continue;
                }

                bool published = false;
                while (!moonraker_discovery_is_cancelled() &&
                       moonraker_discovery_is_open()) {
                    if (bsp_display_lock(100)) {
                        published = moonraker_discovery_add_candidate(
                            host_refs[host_index],
                            port,
                            &probe);
                        bsp_display_unlock();
                        break;
                    }

                    vTaskDelay(pdMS_TO_TICKS(10));
                }

                if (published) {
                    found_count++;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (!moonraker_discovery_is_cancelled()) {
        if (found_count == 0) {
            snprintf(
                msg,
                sizeof(msg),
                "No Moonraker endpoints found on %u.%u.%u.x ports 443-446 or 7125-7128. "
                "Verify the printer address and try again.",
                a,
                b,
                c);
        } else {
            snprintf(
                msg,
                sizeof(msg),
                "Discovery complete   |   %u endpoint%s found",
                (unsigned)found_count,
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
