#include "ui_setup_wizard.h"
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
static char s_printer_identity[64];
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
        detail = s_printer_done ? "A Moonraker printer is saved." : "Discover, verify, name, and save a Moonraker printer.";
    } else if (step == SETUP_STEP_CAMERA) {
        title = "ADD CAMERA";
        detail = s_camera_done ? "A camera is configured for this printer." : "Discover and test a camera, or skip this optional step.";
    } else if (step == SETUP_STEP_COMPLETE) {
        title = "SETUP COMPLETE";
        detail = (s_wifi_done && s_printer_done) ? "Your print cell is ready to operate." : "Finish Wi-Fi and printer setup before operating.";
    }

    ui_popup_add_caption(s_setup_content, title, 24, 28, 600);
    ui_popup_add_status_label(s_setup_content, detail, 24, 74, 600);

    if (step == SETUP_STEP_WELCOME) {
        ui_popup_add_progress_detail(s_setup_content, "Use the steps on the left to jump directly to any section.", 24, 142, 600);
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
    s_center = ui_popup_create(lv_screen_active(), 880, 520, UI_POPUP_STANDARD);
    if (!s_center) return;
    ui_popup_add_title(s_center, "SET UP YOUR PRINT CELL", false, 16);
    ui_popup_add_header_divider(s_center, 50);

    setup_add_nav_button("WELCOME", SETUP_STEP_WELCOME, 76);
    setup_add_nav_button("1  WI-FI", SETUP_STEP_WIFI, 132);
    setup_add_nav_button("2  PRINTER", SETUP_STEP_PRINTER, 188);
    setup_add_nav_button("3  CAMERA", SETUP_STEP_CAMERA, 244);
    setup_add_nav_button("COMPLETE", SETUP_STEP_COMPLETE, 300);

    s_setup_content = lv_obj_create(s_center);
    lv_obj_set_size(s_setup_content, 620, 368);
    lv_obj_set_pos(s_setup_content, 230, 76);
    lv_obj_clear_flag(s_setup_content, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_surface_role(s_setup_content, UI_SURFACE_SECTION);
    s_setup_step = SETUP_STEP_WELCOME;
    setup_render_step(s_setup_step);

    ui_popup_add_standard_footer_divider(s_center);
    ui_popup_add_action_at(s_center, UI_POPUP_ACTION_CANCEL, "SET UP LATER", 24, 464, 240, 44, later_cb, NULL, NULL);
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

static void back_cb(lv_event_t *event) { (void)event; return_to_center(); }
static void focus_cb(lv_event_t *event) { if (s_keyboard) lv_keyboard_set_textarea(s_keyboard, lv_event_get_target(event)); }

static void shell(const char *title, const char *detail, int height)
{
    s_step = ui_popup_create(lv_screen_active(), 800, height, UI_POPUP_STANDARD);
    if (!s_step) return;
    ui_popup_add_title(s_step, title, false, 16);
    ui_popup_add_header_divider(s_step, 50);
    s_status = ui_popup_add_status_label(s_step, detail, 28, 66, 744);
}

static void wifi_scan_task(void *ignored)
{
    (void)ignored;
    s_wifi_count = 0;
    s_wifi_scan_done = false;
    if (esp_wifi_scan_start(NULL, true) == ESP_OK) {
        uint16_t count = 0;
        if (esp_wifi_scan_get_ap_num(&count) == ESP_OK && count) {
            wifi_ap_record_t records[SETUP_MAX_WIFI] = {0};
            if (count > SETUP_MAX_WIFI) count = SETUP_MAX_WIFI;
            if (esp_wifi_scan_get_ap_records(&count, records) == ESP_OK) {
                for (uint16_t i = 0; i < count; ++i) {
                    if (!records[i].ssid[0]) continue;
                    strlcpy(s_wifi[s_wifi_count].ssid, (const char *)records[i].ssid, sizeof(s_wifi[s_wifi_count].ssid));
                    s_wifi[s_wifi_count++].rssi = records[i].rssi;
                }
            }
        }
    }
    s_wifi_scan_done = true;
    vTaskDelete(NULL);
}

static void wifi_password_open(void);
static void wifi_password_open_cb(lv_event_t *event) { (void)event; wifi_password_open(); }

static void wifi_selected_cb(lv_event_t *event)
{
    const uintptr_t index = (uintptr_t)lv_event_get_user_data(event);
    if (index >= s_wifi_count) return;
    strlcpy(s_selected_ssid, s_wifi[index].ssid, sizeof(s_selected_ssid));
    char status[96];
    snprintf(status, sizeof(status), "Selected: %s. Press NEXT to enter its password.", s_selected_ssid);
    set_status(status);
    ui_popup_add_action_at(s_step, UI_POPUP_ACTION_CONFIRM, ui_text(" NEXT"), 512, 480, 260, 44, wifi_password_open_cb, NULL, NULL);
}

static void wifi_scan_ready(lv_timer_t *timer)
{
    (void)timer;
    if (!s_wifi_scan_done) return;
    delete_poll();
    if (!s_step) return;
    if (!s_wifi_count) { set_status("No networks found. Move closer or scan again."); return; }
    set_status("Tap a Wi-Fi network to highlight it, then press NEXT.");
    for (size_t i = 0; i < s_wifi_count; ++i) {
        char row[56];
        snprintf(row, sizeof(row), "%.40s  (%d dBm)", s_wifi[i].ssid, (int)s_wifi[i].rssi);
        ui_popup_add_action_at(s_step, UI_POPUP_ACTION_SECONDARY, row, 28, 104 + (int)i * 44, 744, 40, wifi_selected_cb, (void *)(uintptr_t)i, NULL);
    }
}

static void wifi_scan_open(void)
{
    shell("SETUP 1 OF 3  |  FIND WI-FI", "Scanning nearby networks…", 540);
    if (!s_step) return;
    ui_popup_add_action_at(s_step, UI_POPUP_ACTION_CANCEL, ui_text(" BACK"), 28, 480, 260, 44, back_cb, NULL, NULL);
    s_wifi_scan_done = false;
    if (xTaskCreate(wifi_scan_task, "setup_wifi_scan", 4096, NULL, 4, NULL) != pdPASS) {
        set_status("Unable to start Wi-Fi scan. Try again.");
        return;
    }
    s_poll = lv_timer_create(wifi_scan_ready, 120, NULL);
}

static void wifi_return_timer(lv_timer_t *timer) { (void)timer; return_to_center(); }

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

static void wifi_password_open(void)
{
    destroy_step();
    shell("SETUP 1 OF 3  |  WI-FI PASSWORD", "Selected network. Enter password, then verify the connection.", 540);
    if (!s_step) return;
    ui_popup_add_caption(s_step, s_selected_ssid, 28, 108, 720);
    s_password = ui_popup_add_textarea(s_step, 720, 48, LV_ALIGN_TOP_MID, 0, 140, true, true, 63, ui_text("Wi-Fi password"), ui_text(""), NULL);
    s_keyboard = ui_popup_add_keyboard(s_step, s_password, 720, 202, LV_ALIGN_TOP_MID, 0, 204, LV_KEYBOARD_MODE_TEXT_LOWER);
    if (s_password) lv_obj_add_event_cb(s_password, focus_cb, LV_EVENT_CLICKED, NULL);
    ui_popup_add_standard_footer_divider(s_step);
    ui_popup_add_action_at(s_step, UI_POPUP_ACTION_CANCEL, ui_text(" BACK"), 28, 480, 260, 44, back_cb, NULL, NULL);
    ui_popup_add_action_at(s_step, UI_POPUP_ACTION_CONFIRM, ui_text(" CONNECT"), 512, 480, 260, 44, wifi_save_cb, NULL, NULL);
}

static void printer_save_cb(lv_event_t *event)
{
    (void)event;
    const char *name = s_name ? lv_textarea_get_text(s_name) : "Printer 1";
    if (!moonraker_config_save_profile(0, name, s_printer_host, s_printer_port, "")) { set_status("Could not save that Moonraker printer."); return; }
    (void)moonraker_config_select_profile(0);
    s_printer_done = true;
    return_to_center();
}

static void printer_test_poll(lv_timer_t *timer)
{
    (void)timer;
    moonraker_probe_result_t result;
    bool verified = false;
    if (!moonraker_endpoint_test_take_result(&result, &verified)) return;
    delete_poll();
    if (!verified) { set_status("Moonraker did not verify. Select another printer."); return; }
    set_status("Printer verified. Name it and save.");
    s_name = ui_popup_add_textarea(s_step, 720, 44, LV_ALIGN_TOP_MID, 0, 156, true, false, 31, ui_text("Printer name"), s_printer_identity[0] ? s_printer_identity : ui_text("Printer 1"), NULL);
    s_keyboard = ui_popup_add_keyboard(s_step, s_name, 720, 188, LV_ALIGN_TOP_MID, 0, 216, LV_KEYBOARD_MODE_TEXT_LOWER);
    if (s_name) lv_obj_add_event_cb(s_name, focus_cb, LV_EVENT_CLICKED, NULL);
    ui_popup_add_action_at(s_step, UI_POPUP_ACTION_CONFIRM, ui_text(" SAVE PRINTER"), 512, 480, 260, 44, printer_save_cb, NULL, NULL);
}

static void printer_discovery_selected(const char *host, int port, const char *identity)
{
    strlcpy(s_printer_host, host ? host : "", sizeof(s_printer_host));
    strlcpy(s_printer_identity, identity ? identity : "", sizeof(s_printer_identity));
    s_printer_port = port;
    shell("SETUP 2 OF 3  |  VERIFY PRINTER", "Testing the selected Moonraker endpoint…", 540);
    if (!s_step) return;
    ui_popup_add_action_at(s_step, UI_POPUP_ACTION_CANCEL, ui_text(" BACK"), 28, 480, 260, 44, back_cb, NULL, NULL);
    if (!moonraker_endpoint_test_start(s_printer_host, s_printer_port, "")) { set_status("Could not start printer verification."); return; }
    s_poll = lv_timer_create(printer_test_poll, 150, NULL);
}

static void printer_discovery_closed(void) { if (!s_step) show_center(); }

static void printer_open(void)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip = {0};
    if (!netif || esp_netif_get_ip_info(netif, &ip) != ESP_OK || ip.ip.addr == 0) {
        shell("SETUP 2 OF 3  |  PRINTER", "Connect Wi-Fi first, then return to find Moonraker printers.", 360);
        if (s_step) ui_popup_add_action_at(s_step, UI_POPUP_ACTION_CANCEL, ui_text(" BACK"), 28, 300, 260, 44, back_cb, NULL, NULL);
        return;
    }
    moonraker_discovery_show("SETUP: searching for Moonraker printers…", printer_discovery_closed, printer_discovery_selected);
    (void)moonraker_discovery_start(&ip.ip);
}

static void camera_save_cb(lv_event_t *event)
{
    (void)event;
    const int profile = moonraker_config_active_profile_index();
    camera_catalog_entry_t entry;
    if (s_selected_camera < 0 || !camera_catalog_get(profile, s_selected_camera, &entry) || !entry.configured) { set_status("Select a discovered camera first."); return; }
    (void)camera_catalog_set_default(profile, s_selected_camera);
    (void)moonraker_config_set_camera_stream_url(profile, entry.stream_url);
    s_camera_done = true;
    return_to_center();
}

static void camera_test_poll(lv_timer_t *timer)
{
    (void)timer;
    bool ok = false; int width = 0, height = 0; size_t bytes = 0;
    if (!camera_test_take_result(&ok, &width, &height, &bytes)) return;
    delete_poll();
    if (!ok) { set_status("Camera test failed. Select another discovered camera."); return; }
    char verified[96]; snprintf(verified, sizeof(verified), "Camera verified: %d x %d JPEG. Save it for this printer.", width, height);
    set_status(verified);
    ui_popup_add_action_at(s_step, UI_POPUP_ACTION_CONFIRM, ui_text(" USE CAMERA"), 512, 480, 260, 44, camera_save_cb, NULL, NULL);
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
    bool found = false; size_t count = 0; moonraker_webcam_t ignored;
    if (!camera_discovery_take_result(&ignored, &found, &count)) return;
    delete_poll();
    const int profile = moonraker_config_active_profile_index();
    if (!found || !count) { set_status("No enabled MJPEG/HTTP cameras found for this printer."); return; }
    set_status("Tap a discovered camera to highlight, test, and save it.");
    for (int slot = 0; slot < CAMERA_CATALOG_MAX_CAMERAS; ++slot) {
        camera_catalog_entry_t entry;
        if (!camera_catalog_get(profile, slot, &entry) || !entry.configured) continue;
        ui_popup_add_action_at(s_step, UI_POPUP_ACTION_SECONDARY, entry.name, 28, 108 + slot * 52, 744, 44, camera_selected_cb, (void *)(uintptr_t)slot, NULL);
    }
}

static void camera_open(void)
{
    const int profile = moonraker_config_active_profile_index();
    const moonraker_profile_t *printer = moonraker_config_profile(profile);
    shell("SETUP 3 OF 3  |  FIND CAMERA", "Searching this printer Moonraker webcam configuration...", 540);
    if (!s_step) return;
    ui_popup_add_action_at(s_step, UI_POPUP_ACTION_CANCEL, ui_text(" BACK"), 28, 480, 260, 44, back_cb, NULL, NULL);
    if (!printer || !printer->configured) { set_status("Add and verify a printer first, then return to find cameras."); return; }
    if (!camera_discovery_start(printer->host, printer->port, printer->api_key)) { set_status("Could not start camera discovery. Check the selected printer."); return; }
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

static void card(
    lv_obj_t *parent,
    const char *title,
    const char *detail,
    int x,
    int y,
    lv_event_cb_t cb,
    bool done)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, 398, 92);
    lv_obj_set_pos(box, x, y);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_surface_role(box, UI_SURFACE_SECTION);

    lv_obj_t *heading = lv_label_create(box);
    lv_label_set_text(heading, title);
    ui_apply_text_body_large(heading);
    ui_apply_label_bright(heading);
    lv_obj_set_pos(heading, 16, 12);

    lv_obj_t *body = lv_label_create(box);
    lv_label_set_text(body, detail);
    ui_apply_text_caption(body);
    ui_apply_label_dim(body);
    lv_obj_set_width(body, 250);
    lv_obj_set_pos(body, 16, 44);

    ui_popup_add_action_at(
        box,
        done ? UI_POPUP_ACTION_SECONDARY : UI_POPUP_ACTION_SECONDARY,
        done ? "DONE" : "OPEN",
        286,
        22,
        96,
        48,
        cb,
        NULL,
        NULL);
}
static void close_center_for_step(void)
{
    if (s_center) {
        lv_obj_delete(s_center);
        s_center = NULL;
    }
}

static void center_wifi_cb(lv_event_t *event) { (void)event; wifi_scan_open(); }
static void center_printer_cb(lv_event_t *event) { (void)event; printer_open(); }
static void center_camera_cb(lv_event_t *event) { (void)event; camera_open(); }

static void show_center(void)
{
    if (s_center) { lv_obj_move_foreground(s_center); return; }
    s_center = ui_popup_create(lv_screen_active(), 880, 520, UI_POPUP_STANDARD);
    if (!s_center) return;
    ui_popup_add_title(s_center, ui_text("SET UP YOUR PRINT CELL"), false, 16);
    ui_popup_add_header_divider(s_center, 50);
    ui_popup_add_status_label(s_center, ui_text("Discovery-first setup. Each step stays in its own guided popup and returns here."), 28, 68, 824);
    card(s_center, "1  CONNECT WI-FI", s_wifi_done ? "Verified and connected." : "Scan, select a network, enter password, and verify.", 28, 116, center_wifi_cb, s_wifi_done);
    card(s_center, "2  ADD PRINTER", s_printer_done ? "Moonraker printer saved." : "Discover, select, verify, name, and save Moonraker.", 454, 116, center_printer_cb, s_printer_done);
    card(s_center, "3  ADD CAMERA", s_camera_done ? "Live camera saved." : "Discover cameras for this printer, test one, and save.", 28, 220, center_camera_cb, s_camera_done);
    card(s_center,
        "4  READY TO OPERATE",
        (s_wifi_done && s_printer_done)
            ? "Required setup is complete. Finish now or revisit it from Settings."
            : "Finish after Wi-Fi and printer verification; camera setup is optional.",
        454,
        220,
        finish_cb,
        s_wifi_done && s_printer_done);
    ui_popup_add_progress_detail(s_center, ui_text("These are dedicated onboarding screens; normal Settings stays unchanged."), 28, 350, 824);
    ui_popup_add_standard_footer_divider(s_center);
    ui_popup_add_action_at(s_center, UI_POPUP_ACTION_CANCEL, ui_text("SET UP LATER"), 28, 464, 260, 44, later_cb, NULL, NULL);
    ui_popup_add_action_at(s_center, UI_POPUP_ACTION_CONFIRM, ui_text(" FINISH SETUP"), 592, 464, 260, 44, finish_cb, NULL, NULL);
}

void ui_setup_wizard_show(ui_setup_wizard_wifi_connect_cb_t wifi_connect_cb)
{
    setup_refresh_completion_state();
    s_wifi_connect = wifi_connect_cb;
    show_center();
}
