#include "moonraker_discovery.h"
#include "ui_text.h"
#include "ui_button.h"
#include "ui_popup.h"
#include "ui_form_dialog.h"

#include "lvgl.h"
#include "ui_theme.h"
#include "bsp/esp-bsp.h"
#include "moonraker_probe.h"
#include "moonraker_config_controller.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lv_obj_t *s_popup = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_results_list = NULL;
static size_t s_candidate_count = 0;
static lv_obj_t *s_retry_action, *s_close_action;
static uint32_t s_discovery_epoch, s_scan_epoch, s_owner_generation;
static bool s_discovery_dispatching, s_close_dispatching;

typedef struct {
    char host[32];
    char identity[MOONRAKER_PROBE_IDENTITY_LENGTH];
    bool klippy_ready;
    int port;
    lv_obj_t *button;
} moonraker_discovery_endpoint_t;

static char s_status[512] = "Moonraker scan: not run";

static volatile bool s_cancelled = false;
static TaskHandle_t s_scan_task_handle = NULL;
static esp_ip4_addr_t s_scan_ip, s_retry_ip;
static bool s_has_scan_ip;

static moonraker_discovery_close_cb_t s_close_cb = NULL;
static moonraker_discovery_select_cb_t s_select_cb = NULL;

static bool discovery_control_event(lv_event_t *event,lv_obj_t *button)
{
    if(!s_popup || !button || lv_obj_has_state(button,LV_STATE_DISABLED))return false;
    return !event || (lv_event_get_code(event)==LV_EVENT_CLICKED &&
        lv_event_get_target_obj(event)==button && lv_event_get_current_target_obj(event)==button);
}
static void discovery_field_deleted(lv_event_t *event)
{
    lv_obj_t *object=lv_event_get_target_obj(event);
    if(object==s_status_label)s_status_label=NULL;
    if(object==s_results_list){s_results_list=NULL;s_candidate_count=0;s_cancelled=true;}
    if(object==s_retry_action)s_retry_action=NULL;
    if(object==s_close_action)s_close_action=NULL;
}
static void discovery_watch(lv_obj_t *object)
{
    if(object)lv_obj_add_event_cb(object,discovery_field_deleted,LV_EVENT_DELETE,NULL);
}


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
    if(discovery_control_event(e,s_close_action))moonraker_discovery_close();
}


static void candidate_event_cb(lv_event_t *e)
{
    if (!e || s_discovery_dispatching || lv_event_get_code(e) != LV_EVENT_CLICKED ||
        s_owner_generation!=moonraker_config_generation()) return;

    const moonraker_discovery_endpoint_t *endpoint =
        (const moonraker_discovery_endpoint_t *)
            lv_event_get_user_data(e);

    if (endpoint && s_select_cb && discovery_control_event(e,endpoint->button) &&
        s_results_list && lv_obj_get_parent(endpoint->button)==s_results_list) {
        moonraker_discovery_endpoint_t reviewed=*endpoint;
        moonraker_discovery_select_cb_t select=s_select_cb;
        s_discovery_dispatching=true;
        select(reviewed.host,reviewed.port,reviewed.identity);
        s_discovery_dispatching=false;
    }
}


static void candidate_data_deleted_cb(lv_event_t *e)
{
    if (!e || lv_event_get_code(e) != LV_EVENT_DELETE ||
        lv_event_get_target_obj(e)!=lv_event_get_current_target_obj(e)) return;

    free(lv_event_get_user_data(e));
}


static bool moonraker_discovery_add_candidate(
    const char *host,
    int port,
    const moonraker_probe_result_t *probe);




#define DISCOVERY_HOST_BATCH_SIZE 1

static void discovery_scan_set_status(const char *text)
{
    while(!moonraker_discovery_is_cancelled() && moonraker_discovery_is_open()) {
        if(bsp_display_lock(50)) {
            if(!moonraker_discovery_is_cancelled() && s_popup) {
                copy_text(s_status,sizeof(s_status),text);
                if(s_status_label)lv_label_set_text(s_status_label,s_status);
            }
            bsp_display_unlock();return;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

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
    discovery_scan_set_status(msg);

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
        discovery_scan_set_status(msg);

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
                                published = !moonraker_discovery_is_cancelled() && moonraker_discovery_add_candidate(
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
                        published = !moonraker_discovery_is_cancelled() && moonraker_discovery_add_candidate(
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
        discovery_scan_set_status(msg);
    }

    s_scan_task_handle = NULL;
    vTaskDelete(NULL);
}

static void discovery_deleted(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) != s_popup) return;
    s_cancelled = true;
    __atomic_add_fetch(&s_discovery_epoch,1,__ATOMIC_RELEASE);
    s_popup = s_status_label = s_results_list = NULL;
    s_candidate_count = 0;
    s_retry_action=s_close_action=NULL;
    s_close_cb=NULL;s_select_cb=NULL;
}

static void discovery_retry_cb(lv_event_t *event)
{
    if(s_discovery_dispatching || !s_results_list || !discovery_control_event(event,s_retry_action))return;
    if(s_owner_generation!=moonraker_config_generation()) {
        moonraker_discovery_set_status("Printer changed. Close and reopen discovery.");return;
    }
    if (!s_has_scan_ip) {
        moonraker_discovery_set_status("Connect to Wi-Fi and reopen discovery to scan this network.");
        return;
    }
    if (s_scan_task_handle) {
        moonraker_discovery_set_status("Previous scan is still finishing. Tap Retry when it completes.");
        return;
    }
    uint32_t epoch=s_discovery_epoch;
    s_discovery_dispatching=true;
    lv_obj_clean(s_results_list);
    s_discovery_dispatching=false;
    if(s_discovery_epoch!=epoch || !s_popup || !s_results_list)return;
    s_candidate_count = 0;
    (void)moonraker_discovery_start(&s_retry_ip);
}

static void moonraker_discovery_show_internal(
    lv_obj_t *parent,
    int popup_width,
    int popup_height,
    const char *status_text,
    moonraker_discovery_close_cb_t close_cb,
    moonraker_discovery_select_cb_t select_cb)
{
    if(s_popup){lv_obj_move_foreground(s_popup);return;}
    s_close_cb = close_cb;
    s_select_cb = select_cb;

    if (status_text) {
        copy_text(s_status, sizeof(s_status), status_text);
    }


    lv_obj_update_layout(parent);
    if (parent != lv_layer_top() && parent != lv_screen_active()) {
        int available = lv_obj_get_width(parent) - 16;
        if (available > 32 && popup_width > available) popup_width = available;
        available = lv_obj_get_height(parent) - 16;
        if (available > 32 && popup_height > available) popup_height = available;
    }
    s_popup = ui_form_dialog("DISCOVER MOONRAKER", popup_width, popup_height, UI_POPUP_STANDARD);
    if (!s_popup) {s_close_cb=NULL;s_select_cb=NULL;return;}
    __atomic_add_fetch(&s_discovery_epoch,1,__ATOMIC_RELEASE);
    s_owner_generation=moonraker_config_generation();
    /* The popup must stay above its top-layer blocker to receive touches. */
    ui_popup_set_owner(s_popup, parent);
    lv_obj_center(s_popup);
    lv_obj_add_event_cb(s_popup, discovery_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = ui_form_body(s_popup);
    s_status_label = ui_form_text(body, s_status, UI_FONT_BODY, UI_TEXT);
    discovery_watch(s_status_label);
    ui_form_text(body, "Select a verified printer. READY means Klipper responded.", UI_FONT_CAPTION, UI_TEXT_DIM);
    s_results_list = lv_obj_create(body);
    discovery_watch(s_results_list);
    lv_obj_remove_style_all(s_results_list);
    lv_obj_set_size(s_results_list, LV_PCT(100), 160);
    lv_obj_set_flex_flow(s_results_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_results_list, 8, 0);
    lv_obj_set_scroll_dir(s_results_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_results_list, LV_SCROLLBAR_MODE_AUTO);
    s_candidate_count = 0;
    s_retry_action=ui_form_action(s_popup, UI_BUTTON_SECONDARY, "RETRY", discovery_retry_cb, NULL);discovery_watch(s_retry_action);
    s_close_action=ui_form_action(s_popup, UI_BUTTON_OUTLINED, "CLOSE", close_event_cb, NULL);discovery_watch(s_close_action);
    ui_form_finish(s_popup);
}

void moonraker_discovery_show(
    const char *status_text,
    moonraker_discovery_close_cb_t close_cb,
    moonraker_discovery_select_cb_t select_cb)
{
    moonraker_discovery_show_internal(
        lv_layer_top(), 860, 540, status_text, close_cb, select_cb);
}

void moonraker_discovery_show_in_parent(
    lv_obj_t *parent,
    const char *status_text,
    moonraker_discovery_close_cb_t close_cb,
    moonraker_discovery_select_cb_t select_cb)
{
    if (!parent) return;
    /* The setup card is 720x440; leave a small inset so every discovery
     * result, status message, and CLOSE action remains inside that card. */
    moonraker_discovery_show_internal(
        parent, 704, 424, status_text, close_cb, select_cb);
}

void moonraker_discovery_set_status(const char *status_text)
{
    if (!status_text) return;

    uint32_t epoch=__atomic_load_n(&s_discovery_epoch,__ATOMIC_ACQUIRE);
    if(!bsp_display_lock(50))return;
    if(epoch!=s_discovery_epoch){bsp_display_unlock();return;}
    copy_text(s_status, sizeof(s_status), status_text);
    if(s_status_label)lv_label_set_text(s_status_label,s_status);
    bsp_display_unlock();
}


static bool moonraker_discovery_add_candidate(
    const char *host,
    int port,
    const moonraker_probe_result_t *probe)
{
    if (!s_results_list ||
        !s_popup || s_owner_generation!=moonraker_config_generation() ||
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

    lv_obj_t *button = ui_button_create(s_results_list, UI_BUTTON_OUTLINED, row);
    if (button) {
        lv_obj_set_size(button, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(button, 56, 0);
        lv_obj_set_style_pad_all(button, 12, 0);
        lv_obj_set_flex_flow(button, LV_FLEX_FLOW_COLUMN);
        lv_obj_t *label = lv_obj_get_child(button, 0);
        lv_obj_set_width(label, LV_PCT(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        ui_apply_custom_label_style(label, UI_FONT_BODY, UI_TEXT);
        lv_obj_add_event_cb(button, candidate_event_cb, LV_EVENT_CLICKED, endpoint);
    }

    if (!button) {
        free(endpoint);
        return false;
    }
    endpoint->button=button;

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
    if(!s_popup || !s_results_list || s_discovery_dispatching ||
        s_owner_generation!=moonraker_config_generation())return false;
    s_has_scan_ip = ip != NULL;
    if (ip) s_retry_ip = *ip;
    if (!ip || s_scan_task_handle) {
        moonraker_discovery_set_status(s_scan_task_handle
            ? "A discovery scan is already running. Wait for it to finish."
            : "No network address available. Connect Wi-Fi and reopen discovery.");
        return false;
    }

    s_scan_ip = *ip;
    s_has_scan_ip = true;
    s_cancelled = false;
    s_scan_epoch=__atomic_load_n(&s_discovery_epoch,__ATOMIC_ACQUIRE);

    uint32_t epoch=s_discovery_epoch;
    s_discovery_dispatching=true;
    bool created=xTaskCreate(scan_task,"moon_scan",8192,NULL,4,&s_scan_task_handle)==pdPASS;
    s_discovery_dispatching=false;
    if (!created) {
        s_scan_task_handle = NULL;
        if(s_discovery_epoch==epoch && s_popup)
            moonraker_discovery_set_status("Could not start discovery. Tap Retry to try again.");
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
    return s_cancelled || s_scan_epoch!=__atomic_load_n(&s_discovery_epoch,__ATOMIC_ACQUIRE) ||
        s_owner_generation!=moonraker_config_generation();
}


bool moonraker_discovery_is_open(void)
{
    return s_popup != NULL;
}


void moonraker_discovery_close(void)
{
    if(s_close_dispatching)return;
    lv_obj_t *popup=s_popup;
    moonraker_discovery_close_cb_t close=s_close_cb;
    s_close_dispatching=true;
    s_cancelled = true;
    __atomic_add_fetch(&s_discovery_epoch,1,__ATOMIC_RELEASE);
    s_popup=s_status_label=s_results_list=s_retry_action=s_close_action=NULL;
    s_candidate_count=0;s_close_cb=NULL;s_select_cb=NULL;
    if(popup)lv_obj_delete(popup);
    if(close)close();
    s_close_dispatching=false;
}
