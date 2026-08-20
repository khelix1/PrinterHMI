#include "ui_setup_wizard.h"
/* STEP252_SETUP_REPAIRED */
/* STEP251_SETUP_CARD */
/* STEP249_STABLE_SETUP_SCAN */
/* STEP248_SETUP_SCAN */
#include "ui_text.h"

#include "camera_catalog_controller.h"
#include "camera_discovery_controller.h"
#include "camera_test_controller.h"
#include "moonraker_config_controller.h"
#include "moonraker_discovery.h"
#include "moonraker_endpoint_test.h"
#include "onboarding_controller.h"
#include "ui_popup.h"
#include "ui_theme.h"

#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <stdio.h>
#include <string.h>

#define SETUP_MAX_WIFI 8

typedef struct {
    char ssid[33];
    int8_t rssi;
} setup_wifi_ap_t;

static lv_obj_t *s_center;
static lv_obj_t *s_complete_popup;
static lv_obj_t *s_step;
static lv_obj_t *s_status;
static lv_obj_t *s_name;
static lv_obj_t *s_password;
static lv_obj_t *s_keyboard;
static lv_timer_t *s_poll;
static ui_setup_wizard_wifi_connect_cb_t s_wifi_connect;
static setup_wifi_ap_t s_wifi[SETUP_MAX_WIFI];
static size_t s_wifi_count;
static volatile bool s_wifi_scan_done;
static char s_selected_ssid[33];
static char s_printer_host[64];
static int s_printer_port;
static int s_selected_camera = -1;
static bool s_wifi_done, s_printer_done, s_camera_done;


enum {
    SETUP_STEP_WELCOME = 0,
    SETUP_STEP_WIFI,
    SETUP_STEP_PRINTER,
    SETUP_STEP_CAMERA,
    SETUP_STEP_COMPLETE,
};

static int s_setup_step = SETUP_STEP_WELCOME;
static lv_obj_t *s_setup_content;

static void setup_render_step(int step);
static void wifi_scan_open(void);
static void wifi_save_cb(lv_event_t *event);
static void printer_test_poll(lv_timer_t *timer);
static void wifi_scan_open_cb(lv_event_t *event);
static void printer_test_start_cb(lv_event_t *event);

static void center_wifi_cb(lv_event_t *event);
static void center_printer_cb(lv_event_t *event);
static void center_camera_cb(lv_event_t *event);
static void finish_cb(lv_event_t *event);
static void later_cb(lv_event_t *event);
static void set_status(const char *text)
{
    if (s_status) {
        lv_label_set_text(s_status, text ? text : "");
    }
}
static void setup_refresh_completion_state(void);


static void setup_next_cb(lv_event_t *event)
{
    (void)event;
    if (s_setup_step < SETUP_STEP_COMPLETE) {
        setup_render_step(++s_setup_step);
    }
}

static void setup_skip_cb(lv_event_t *event)
{
    (void)event;
    if (s_setup_step < SETUP_STEP_COMPLETE) {
        setup_render_step(++s_setup_step);
    }
}

static void setup_select_step_cb(lv_event_t *event)
{
    s_setup_step = (int)(uintptr_t)lv_event_get_user_data(event);
    setup_render_step(s_setup_step);
}

static void setup_clear_content(void)
{
    /* STEP251_SETUP_CARD: never leave a keyboard above scan rows. */
    if (s_keyboard) {
        lv_obj_delete(s_keyboard);
        s_keyboard = NULL;
    }
    s_password = NULL;
    s_name = NULL;
    if (s_setup_content) {
        lv_obj_clean(s_setup_content);
    }
}

static void setup_add_nav_button(const char *label, int step, int y)
{
    if (!s_center) return;
    ui_popup_add_action_at(
        s_center,
        UI_POPUP_ACTION_SECONDARY,
        label,
        24,
        y,
        196,
        48,
        setup_select_step_cb,
        (void *)(uintptr_t)step,
        NULL);
}

static void setup_add_next_buttons(bool allow_skip)
{
    if (!s_setup_content) return;
    if (allow_skip) {
        ui_popup_add_action_at(
            s_setup_content,
            UI_POPUP_ACTION_SECONDARY,
            "SKIP",
            24,
            338,
            220,
            48,
            setup_skip_cb,
            NULL,
            NULL);
    }
    ui_popup_add_action_at(
        s_setup_content,
        UI_POPUP_ACTION_CONFIRM,
        "NEXT",
        380,
        338,
        220,
        48,
        setup_next_cb,
        NULL,
        NULL);
}

static void setup_render_step(int step)
{
    if (!s_setup_content) return;
    setup_clear_content();

    const char *title = "WELCOME";
    const char *detail = "Set up your print cell one step at a time.";
    if (step == SETUP_STEP_WIFI) {
        title = "CONNECT WI-FI";
        detail = s_wifi_done ? "Wi-Fi is connected and verified." : "Scan for Wi-Fi, select a network, and verify it.";
    } else if (step == SETUP_STEP_PRINTER) {
        title = "ADD PRINTER";
        detail = s_printer_done
            ? "A Moonraker printer is saved."
            : "Open printer setup for discovery, name, host, port, security, authentication, test, and save.";
    } else if (step == SETUP_STEP_CAMERA) {
        title = "ADD CAMERA";
        detail = s_camera_done ? "A camera is configured for this printer." : "Discover and test a camera, or skip this optional step.";
    } else if (step == SETUP_STEP_COMPLETE) {
        title = "SETUP COMPLETE";
        detail = (s_wifi_done && s_printer_done) ? "Your print cell is ready to operate." : "Finish Wi-Fi and printer setup before operating.";
    }

    ui_popup_add_caption(s_setup_content, title, 24, 28, 700);
    s_status = ui_popup_add_status_label(s_setup_content, detail, 24, 74, 700);

    if (step == SETUP_STEP_WELCOME) {
        ui_popup_add_progress_detail(s_setup_content, "Use the steps on the left to jump directly to any section.", 24, 142, 700);
        setup_add_next_buttons(false);
    } else if (step == SETUP_STEP_WIFI) {
        ui_popup_add_action_at(s_setup_content, UI_POPUP_ACTION_SECONDARY, s_wifi_done ? "REVIEW WI-FI" : "OPEN WI-FI SETUP", 24, 142, 280, 52, center_wifi_cb, NULL, NULL);
        setup_add_next_buttons(true);
    } else if (step == SETUP_STEP_PRINTER) {
        ui_popup_add_action_at(s_setup_content, UI_POPUP_ACTION_SECONDARY, s_printer_done ? "REVIEW PRINTER" : "OPEN PRINTER SETUP", 24, 142, 280, 52, center_printer_cb, NULL, NULL);
        setup_add_next_buttons(true);
    } else if (step == SETUP_STEP_CAMERA) {
        ui_popup_add_action_at(s_setup_content, UI_POPUP_ACTION_SECONDARY, s_camera_done ? "REVIEW CAMERA" : "OPEN CAMERA SETUP", 24, 142, 280, 52, center_camera_cb, NULL, NULL);
        setup_add_next_buttons(true);
    } else {
        ui_popup_add_action_at(s_setup_content, UI_POPUP_ACTION_CONFIRM, "FINISH SETUP", 380, 142, 220, 52, finish_cb, NULL, NULL);
    }
}

static void show_center(void)
{
    setup_refresh_completion_state();
    /* STEP245_SETUP_WIDE: use the full practical 1024px display width. */
    /* STEP246_SETUP_TALL: use the available 600px display height. */
    s_center = ui_popup_create(lv_screen_active(), 960, 580, UI_POPUP_STANDARD);
    if (!s_center) return;
    ui_popup_add_title(s_center, "SET UP YOUR PRINT CELL", false, 16);
    ui_popup_add_header_divider(s_center, 50);

    setup_add_nav_button("WELCOME", SETUP_STEP_WELCOME, 76);
    setup_add_nav_button("1  WI-FI", SETUP_STEP_WIFI, 132);
    setup_add_nav_button("2  PRINTER", SETUP_STEP_PRINTER, 188);
    setup_add_nav_button("3  CAMERA", SETUP_STEP_CAMERA, 244);
    setup_add_nav_button("COMPLETE", SETUP_STEP_COMPLETE, 300);

    /* STEP244_SETUP_LAYOUT: reserve the full right side for setup controls. */
    s_setup_content = lv_obj_create(s_center);
    lv_obj_set_size(s_setup_content, 720, 440);
    lv_obj_set_pos(s_setup_content, 204, 76);
    lv_obj_clear_flag(s_setup_content, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_surface_role(s_setup_content, UI_SURFACE_SECTION);
    s_setup_step = SETUP_STEP_WELCOME;
    setup_render_step(s_setup_step);

    ui_popup_add_standard_footer_divider(s_center);
    ui_popup_add_action_at(s_center, UI_POPUP_ACTION_CANCEL, "SET UP LATER", 24, 520, 160, 44, later_cb, NULL, NULL);
    lv_obj_move_foreground(s_center);
}

static void delete_poll(void) { if (s_poll) lv_timer_delete(s_poll); s_poll = NULL; }

static void destroy_step(void)
{
    delete_poll();
    if (s_step) lv_obj_delete(s_step);
    s_step = s_status = s_name = s_password = s_keyboard = NULL;
}

static void return_to_center(void)
{
    destroy_step();
    show_center();
}

void ui_setup_wizard_close(void)
{
    destroy_step();
    if (s_center) lv_obj_delete(s_center);
    s_center = NULL;
}

static void focus_cb(lv_event_t *event) { if (s_keyboard) lv_keyboard_set_textarea(s_keyboard, lv_event_get_target(event)); }

static void wifi_scan_task(void *ignored)
{
    (void)ignored;
    s_wifi_count = 0;
    s_wifi_scan_done = false;

    /* Do not disconnect the station here. The Network page scans while the
     * Wi-Fi state machine is running; setup must use the same path. */
    vTaskDelay(pdMS_TO_TICKS(150));
    esp_err_t scan_error = esp_wifi_scan_start(NULL, true);
    if (scan_error != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(200));
        scan_error = esp_wifi_scan_start(NULL, true);
    }

    if (scan_error == ESP_OK) {
        uint16_t count = 0;
        if (esp_wifi_scan_get_ap_num(&count) == ESP_OK && count) {
            wifi_ap_record_t records[SETUP_MAX_WIFI] = {0};
            if (count > SETUP_MAX_WIFI) count = SETUP_MAX_WIFI;
            if (esp_wifi_scan_get_ap_records(&count, records) == ESP_OK) {
                for (uint16_t i = 0; i < count; ++i) {
                    if (!records[i].ssid[0]) continue;
                    strlcpy(s_wifi[s_wifi_count].ssid,
                            (const char *)records[i].ssid,
                            sizeof(s_wifi[s_wifi_count].ssid));
                    s_wifi[s_wifi_count++].rssi = records[i].rssi;
                }
            }
        }
    }

    s_wifi_scan_done = true;
    vTaskDelete(NULL);
}

static void wifi_password_open(void)
{
    setup_clear_content();
    ui_popup_add_caption(s_setup_content, "WI-FI PASSWORD", 24, 28, 572);
    s_status = ui_popup_add_status_label(s_setup_content, s_selected_ssid, 24, 74, 572);
    s_password = ui_popup_add_textarea(
        s_setup_content,
        572,
        44,
        LV_ALIGN_TOP_LEFT,
        24,
        116,
        true,
        true,
        63,
        ui_text("Wi-Fi password"),
        ui_text(""),
        NULL);
    s_keyboard = ui_popup_add_keyboard(
        s_setup_content,
        s_password,
        572,
        150,
        LV_ALIGN_TOP_LEFT,
        24,
        168,
        LV_KEYBOARD_MODE_TEXT_LOWER);
    if (s_password) lv_obj_add_event_cb(s_password, focus_cb, LV_EVENT_CLICKED, NULL);
    ui_popup_add_action_at(s_setup_content, UI_POPUP_ACTION_CONFIRM, "CONNECT", 376, 330, 220, 44, wifi_save_cb, NULL, NULL);
}

static void wifi_selected_cb(lv_event_t *event)
{
    const uintptr_t index = (uintptr_t)lv_event_get_user_data(event);
    if (index >= s_wifi_count) return;
    strlcpy(s_selected_ssid, s_wifi[index].ssid, sizeof(s_selected_ssid));
    wifi_password_open();
}

static void wifi_scan_ready(lv_timer_t *timer)
{
    (void)timer;
    if (!s_wifi_scan_done) return;
    delete_poll();
    if (!s_setup_content) return;
    if (!s_wifi_count) {
        set_status("No networks found. Press SCAN WI-FI to try again.");
        return;
    }
    set_status("Tap a network, enter its password, and verify the connection.");
    for (size_t i = 0; i < s_wifi_count; ++i) {
        char row[56];
        snprintf(row, sizeof(row), "%.40s  (%d dBm)", s_wifi[i].ssid, (int)s_wifi[i].rssi);
        ui_popup_add_action_at(
            s_setup_content,
            UI_POPUP_ACTION_SECONDARY,
            row,
            24,
            126 + (int)i * 34,
            572,
            30,
            wifi_selected_cb,
            (void *)(uintptr_t)i,
            NULL);
    }
}

static void wifi_scan_open_cb(lv_event_t *event)
{
    (void)event;
    wifi_scan_open();
}

static void wifi_scan_open(void)
{
    if (s_keyboard) {
        lv_obj_delete(s_keyboard);
        s_keyboard = NULL;
    }
    s_password = NULL;
    s_name = NULL;
    setup_clear_content();
    s_keyboard = NULL;
    s_password = NULL;
    ui_popup_add_caption(s_setup_content, "CONNECT WI-FI", 24, 28, 572);
    s_status = ui_popup_add_status_label(s_setup_content, "Scanning nearby networks...", 24, 74, 572);
    ui_popup_add_action_at(s_setup_content, UI_POPUP_ACTION_SECONDARY, "SCAN WI-FI", 24, 104, 220, 44, wifi_scan_open_cb, NULL, NULL);
    s_wifi_scan_done = false;
    if (xTaskCreate(wifi_scan_task, "setup_wifi_scan", 4096, NULL, 4, NULL) != pdPASS) {
        set_status("Unable to start Wi-Fi scan. Try again.");
        return;
    }
    s_poll = lv_timer_create(wifi_scan_ready, 120, NULL);
}

static void wifi_return_timer(lv_timer_t *timer)
{
    (void)timer;
    setup_refresh_completion_state();
    setup_render_step(SETUP_STEP_WIFI);
}

static void wifi_verify(lv_timer_t *timer)
{
    (void)timer;
    wifi_ap_record_t info;
    if (esp_wifi_sta_get_ap_info(&info) != ESP_OK) return;
    delete_poll();
    s_wifi_done = true;
    set_status("Wi-Fi verified. Returning to Setup Center…");
    lv_timer_create(wifi_return_timer, 700, NULL);
}

static void wifi_save_cb(lv_event_t *event)
{
    (void)event;
    const char *password = s_password ? lv_textarea_get_text(s_password) : "";
    if (!s_wifi_connect || !s_wifi_connect(s_selected_ssid, password)) { set_status("Could not start the connection. Check the password and try again."); return; }
    set_status("Connecting and verifying Wi-Fi…");
    s_poll = lv_timer_create(wifi_verify, 250, NULL);
}



static void printer_test_start_cb(lv_event_t *event)
{
    (void)event;
    const char *host = s_password ? lv_textarea_get_text(s_password) : "";
    if (!host || !host[0]) {
        set_status("Enter a printer host or IP address first.");
        return;
    }
    strlcpy(s_printer_host, host, sizeof(s_printer_host));
    s_printer_port = 7125;
    if (!moonraker_endpoint_test_start(s_printer_host, s_printer_port, "")) {
        set_status("Could not start printer verification.");
        return;
    }
    set_status("Testing Moonraker endpoint...");
    s_poll = lv_timer_create(printer_test_poll, 150, NULL);
}

static void printer_save_cb(lv_event_t *event)
{
    (void)event;
    const char *name = s_name ? lv_textarea_get_text(s_name) : "Printer 1";
    if (!moonraker_config_save_profile(0, name, s_printer_host, s_printer_port, "")) {
        set_status("Could not save that Moonraker printer.");
        return;
    }
    (void)moonraker_config_select_profile(0);
    s_printer_done = true;
    setup_refresh_completion_state();
    setup_render_step(SETUP_STEP_PRINTER);
}

static void printer_test_poll(lv_timer_t *timer)
{
    (void)timer;
    moonraker_probe_result_t result;
    bool verified = false;
    if (!moonraker_endpoint_test_take_result(&result, &verified)) return;
    delete_poll();
    if (!verified) {
        set_status("Moonraker did not verify. Check the host and port.");
        return;
    }
    set_status("Printer verified. Press SAVE PRINTER to keep it.");
    ui_popup_add_action_at(s_setup_content, UI_POPUP_ACTION_CONFIRM, "SAVE PRINTER", 24, 330, 220, 44, printer_save_cb, NULL, NULL);
}

static void printer_open(void)
{
    setup_clear_content();
    ui_popup_add_caption(s_setup_content, "ADD PRINTER", 24, 28, 572);
    s_status = ui_popup_add_status_label(s_setup_content, "Enter the Moonraker endpoint, then test and save it.", 24, 74, 572);
    s_name = ui_popup_add_textarea(s_setup_content, 300, 42, LV_ALIGN_TOP_LEFT, 24, 116, true, false, 31, ui_text("Printer name"), ui_text("Printer 1"), NULL);
    s_password = ui_popup_add_textarea(s_setup_content, 300, 42, LV_ALIGN_TOP_LEFT, 332, 116, true, false, 63, ui_text("Host or IP address"), ui_text(""), NULL);
    s_keyboard = ui_popup_add_keyboard(s_setup_content, s_password, 572, 150, LV_ALIGN_TOP_LEFT, 24, 168, LV_KEYBOARD_MODE_TEXT_LOWER);
    if (s_name) lv_obj_add_event_cb(s_name, focus_cb, LV_EVENT_CLICKED, NULL);
    if (s_password) lv_obj_add_event_cb(s_password, focus_cb, LV_EVENT_CLICKED, NULL);
    ui_popup_add_action_at(s_setup_content, UI_POPUP_ACTION_CONFIRM, "TEST PRINTER", 24, 330, 220, 44, printer_test_start_cb, NULL, NULL);
    ui_popup_add_action_at(s_setup_content, UI_POPUP_ACTION_SECONDARY, "NEXT", 376, 330, 220, 44, setup_next_cb, NULL, NULL);
}

static void camera_save_cb(lv_event_t *event)
{
    (void)event;
    const int profile = moonraker_config_active_profile_index();
    camera_catalog_entry_t entry;
    if (s_selected_camera < 0 || !camera_catalog_get(profile, s_selected_camera, &entry) || !entry.configured) {
        set_status("Select a discovered camera first.");
        return;
    }
    (void)camera_catalog_set_default(profile, s_selected_camera);
    (void)moonraker_config_set_camera_stream_url(profile, entry.stream_url);
    s_camera_done = true;
    setup_refresh_completion_state();
    setup_render_step(SETUP_STEP_CAMERA);
}

static void camera_test_poll(lv_timer_t *timer)
{
    (void)timer;
    bool ok = false;
    int width = 0;
    int height = 0;
    size_t bytes = 0;
    if (!camera_test_take_result(&ok, &width, &height, &bytes)) return;
    delete_poll();
    if (!ok) {
        set_status("Camera test failed. Select another camera.");
        return;
    }
    char verified[96];
    snprintf(verified, sizeof(verified), "Camera verified: %d x %d JPEG. Save it for this printer.", width, height);
    set_status(verified);
    ui_popup_add_action_at(s_setup_content, UI_POPUP_ACTION_CONFIRM, "USE CAMERA", 24, 330, 220, 44, camera_save_cb, NULL, NULL);
}

static void camera_selected_cb(lv_event_t *event)
{
    s_selected_camera = (int)(uintptr_t)lv_event_get_user_data(event);
    const int profile = moonraker_config_active_profile_index();
    camera_catalog_entry_t entry;
    if (!camera_catalog_get(profile, s_selected_camera, &entry)) return;
    char detail[240]; snprintf(detail, sizeof(detail), "Selected: %s. Testing its stream…", entry.name);
    set_status(detail);
    if (!camera_test_start(entry.stream_url)) { set_status("Could not start the camera test."); return; }
    s_poll = lv_timer_create(camera_test_poll, 150, NULL);
}

static void camera_discovery_poll(lv_timer_t *timer)
{
    (void)timer;
    if (camera_discovery_busy()) return;
    bool found = false;
    size_t count = 0;
    moonraker_webcam_t ignored;
    if (!camera_discovery_take_result(&ignored, &found, &count)) return;
    delete_poll();
    const int profile = moonraker_config_active_profile_index();
    if (!found || !count) {
        set_status("No enabled cameras found for this printer.");
        return;
    }
    set_status("Select a camera to test it.");
    for (int slot = 0; slot < CAMERA_CATALOG_MAX_CAMERAS; ++slot) {
        camera_catalog_entry_t entry;
        if (!camera_catalog_get(profile, slot, &entry) || !entry.configured) continue;
        ui_popup_add_action_at(s_setup_content, UI_POPUP_ACTION_SECONDARY, entry.name, 24, 126 + slot * 40, 572, 34, camera_selected_cb, (void *)(uintptr_t)slot, NULL);
    }
}

static void camera_open(void)
{
    setup_clear_content();
    ui_popup_add_caption(s_setup_content, "ADD CAMERA", 24, 28, 572);
    s_status = ui_popup_add_status_label(s_setup_content, "Searching this printer for configured cameras...", 24, 74, 572);
    const int profile = moonraker_config_active_profile_index();
    const moonraker_profile_t *printer = moonraker_config_profile(profile);
    if (!printer || !printer->configured) {
        set_status("Add and verify a printer first, then return here.");
        return;
    }
    if (!camera_discovery_start(printer->host, printer->port, printer->api_key)) {
        set_status("Could not start camera discovery.");
        return;
    }
    s_poll = lv_timer_create(camera_discovery_poll, 150, NULL);
}

static bool setup_wifi_is_ready(void)
{
    wifi_ap_record_t info;
    return esp_wifi_sta_get_ap_info(&info) == ESP_OK;
}


static void setup_refresh_completion_state(void)
{
    s_wifi_done = setup_wifi_is_ready();

    const int profile_index = moonraker_config_active_profile_index();
    const moonraker_profile_t *profile = moonraker_config_profile(profile_index);
    s_printer_done = profile && profile->configured;

    const char *camera_url =
        moonraker_config_camera_stream_url(profile_index);
    s_camera_done = camera_url && camera_url[0];
}

static void setup_completion_done_cb(lv_event_t *event)
{
    (void)event;
    if (s_complete_popup) {
        lv_obj_delete(s_complete_popup);
        s_complete_popup = NULL;
    }
}


static void show_setup_completion(void)
{
    s_complete_popup = ui_popup_create(lv_screen_active(), 660, 280, UI_POPUP_STANDARD);
    if (!s_complete_popup) return;
    ui_popup_add_title(s_complete_popup, "SETUP COMPLETE", false, 16);
    ui_popup_add_header_divider(s_complete_popup, 50);
    ui_popup_add_status_label(s_complete_popup, "Your print cell is ready. Setup can be reopened later from Settings.", 28, 82, 604);
    ui_popup_add_progress_detail(s_complete_popup, "Wi-Fi and printer setup are saved. Camera setup remains optional.", 28, 136, 604);
    ui_popup_add_standard_footer_divider(s_complete_popup);
    ui_popup_add_action_at(s_complete_popup, UI_POPUP_ACTION_SECONDARY, "DONE", 370, 210, 260, 44, setup_completion_done_cb, NULL, NULL);
    lv_obj_move_foreground(s_complete_popup);
}


static void finish_cb(lv_event_t *event)
{
    (void)event;
    setup_refresh_completion_state();
    if (!s_wifi_done || !s_printer_done) {
        set_status("Connect Wi-Fi and verify a printer before finishing setup.");
        return;
    }
    if (!onboarding_controller_mark_complete()) {
        set_status("Could not save setup completion. Try again.");
        return;
    }
    ui_setup_wizard_close();
    show_setup_completion();
}
static void later_cb(lv_event_t *event) { (void)event; ui_setup_wizard_close(); }

static void center_wifi_cb(lv_event_t *event) { (void)event; wifi_scan_open(); }
static void center_printer_cb(lv_event_t *event) { (void)event; printer_open(); }
static void center_camera_cb(lv_event_t *event) { (void)event; camera_open(); }



void ui_setup_wizard_show(ui_setup_wizard_wifi_connect_cb_t wifi_connect_cb)
{
    setup_refresh_completion_state();
    s_wifi_connect = wifi_connect_cb;
    show_center();
}
