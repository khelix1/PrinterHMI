#include "ui_ota_release_browser.h"
#include "ui_text.h"

#include "ota_release_catalog.h"
#include "ui_popup.h"
#include "ui_ota_popup.h"

#include "esp_heap_caps.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lv_obj_t *s_popup = NULL;
static lv_obj_t *s_confirm = NULL;
static lv_obj_t *s_status = NULL;
static lv_obj_t *s_list = NULL;
static lv_obj_t *s_stable_tab = NULL;
static lv_obj_t *s_nightly_tab = NULL;
static lv_timer_t *s_timer = NULL;
static ui_ota_start_cb_t s_start_fn = NULL;
static ui_ota_custom_cb_t s_custom_fn = NULL;
static ota_release_channel_t s_channel =
    OTA_RELEASE_CHANNEL_STABLE;
static ota_release_catalog_state_t s_rendered_state =
    OTA_RELEASE_CATALOG_IDLE;
static size_t s_rendered_count = SIZE_MAX;
static ota_release_channel_t s_rendered_channel =
    OTA_RELEASE_CHANNEL_STABLE;
static ota_release_entry_t *s_selected = NULL;


static const char *relation_text(
    const ota_release_entry_t *release)
{
    if (!release) return "";

    if (release->relation == OTA_RELEASE_INSTALLED) {
        return "INSTALLED";
    }

    if (release->channel == OTA_RELEASE_CHANNEL_NIGHTLY) {
        return "NIGHTLY";
    }

    switch (release->relation) {
        case OTA_RELEASE_NEWER: return "UPDATE";
        case OTA_RELEASE_OLDER: return "OLDER";
        case OTA_RELEASE_STABLE: return "STABLE";
        default: return "STABLE";
    }
}


static void confirm_close(void)
{
    if (s_confirm) {
        lv_obj_delete(s_confirm);
        s_confirm = NULL;
    }
}


static void browser_close(void)
{
    confirm_close();

    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }

    if (s_popup) {
        lv_obj_delete(s_popup);
        s_popup = NULL;
    }

    s_status = NULL;
    s_list = NULL;
    s_stable_tab = NULL;
    s_nightly_tab = NULL;
    s_start_fn = NULL;
    s_custom_fn = NULL;
    s_channel = OTA_RELEASE_CHANNEL_STABLE;
    s_rendered_state = OTA_RELEASE_CATALOG_IDLE;
    s_rendered_count = SIZE_MAX;
    s_rendered_channel = OTA_RELEASE_CHANNEL_STABLE;
    free(s_selected);
    s_selected = NULL;
}


static void close_cb(lv_event_t *event)
{
    (void)event;
    browser_close();
}


static void confirm_close_cb(lv_event_t *event)
{
    (void)event;
    confirm_close();
}


static void install_cb(lv_event_t *event)
{
    if (!event ||
        lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !s_selected ||
        !s_selected->asset_url[0]) {
        return;
    }

    ui_ota_start_cb_t start_fn = s_start_fn;
    char url[sizeof(s_selected->asset_url)];
    snprintf(url, sizeof(url), "%s", s_selected->asset_url);

    browser_close();
    ui_ota_popup_close();

    if (start_fn) {
        start_fn(url);
    }
}


static void release_select_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);

    if (!s_selected) {
        s_selected = heap_caps_calloc(
            1,
            sizeof(*s_selected),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_selected) {
            s_selected = calloc(1, sizeof(*s_selected));
        }
    }

    if (!s_selected ||
        !ota_release_catalog_entry(index, s_selected)) {
        return;
    }

    confirm_close();

    bool nightly =
        s_selected->channel == OTA_RELEASE_CHANNEL_NIGHTLY;
    bool installed =
        s_selected->relation == OTA_RELEASE_INSTALLED;
    bool downgrade =
        s_selected->relation == OTA_RELEASE_OLDER;
    bool danger = nightly || installed || downgrade;

    s_confirm = ui_popup_create(
        lv_layer_top(),
        720,
        400,
        danger ? UI_POPUP_DANGER : UI_POPUP_STANDARD);
    if (!s_confirm) return;

    const char *action =
        installed
            ? (nightly ? "REINSTALL NIGHTLY" : "REINSTALL")
            : (nightly
                ? "INSTALL NIGHTLY"
                : (downgrade ? "DOWNGRADE TO" : "INSTALL"));
    char title[112];
    snprintf(title, sizeof(title), "%s %s",
             action, s_selected->version);

    ui_popup_add_title(s_confirm, title, danger, 0);
    ui_popup_add_header_divider(s_confirm, 44);

    const char *warning = "";
    if (nightly && installed) {
        warning =
            "Warning: this rewrites the installed development build.\n\n";
    } else if (nightly) {
        warning =
            "Warning: development builds may contain unfinished or "
            "unverified changes.\n\n";
    } else if (installed) {
        warning =
            "Warning: this rewrites the currently installed firmware.\n\n";
    } else if (downgrade) {
        warning =
            "Warning: this replaces newer installed firmware.\n\n";
    }

    char body[560];
    snprintf(
        body,
        sizeof(body),
        "%sPublished: %s   |   Firmware: %.2f MB\n\n%s%s",
        warning,
        s_selected->published[0]
            ? s_selected->published
            : "Unknown",
        s_selected->asset_size / 1048576.0,
        s_selected->notes[0]
            ? s_selected->notes
            : "No release notes were provided.",
        installed
            ? "\n\nThe controller will reboot after reinstalling. "
              "Settings and printer profiles are retained."
            : "");

    ui_popup_add_body(s_confirm, body, 24, 62, 672);
    ui_popup_add_standard_footer_divider(s_confirm);

    ui_popup_add_footer_action(
        s_confirm,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        170,
        UI_POPUP_FOOTER_LEFT,
        confirm_close_cb,
        NULL,
        NULL);

    const char *button_text =
        installed
            ? LV_SYMBOL_REFRESH " REINSTALL"
            : (nightly
                ? LV_SYMBOL_WARNING " INSTALL NIGHTLY"
                : (downgrade
                    ? LV_SYMBOL_WARNING " DOWNGRADE"
                    : LV_SYMBOL_DOWNLOAD " INSTALL"));

    ui_popup_add_footer_action(
        s_confirm,
        danger
            ? UI_POPUP_ACTION_DANGER
            : UI_POPUP_ACTION_CONFIRM,
        button_text,
        nightly ? 230 : 190,
        UI_POPUP_FOOTER_RIGHT,
        install_cb,
        NULL,
        NULL);
}


static void render_catalog(
    const ota_release_catalog_snapshot_t *snapshot)
{
    if (!snapshot || !s_status || !s_list) return;

    if (snapshot->state == s_rendered_state &&
        snapshot->count == s_rendered_count &&
        s_channel == s_rendered_channel) {
        return;
    }

    s_rendered_state = snapshot->state;
    s_rendered_count = snapshot->count;
    s_rendered_channel = s_channel;
    lv_obj_clean(s_list);

    if (snapshot->state == OTA_RELEASE_CATALOG_LOADING) {
        lv_label_set_text(
            s_status,
            ui_text("Loading firmware releases from GitHub..."));
        return;
    }

    if (snapshot->state == OTA_RELEASE_CATALOG_ERROR) {
        lv_label_set_text(
            s_status,
            snapshot->error[0]
                ? snapshot->error
                : ui_text("Could not load GitHub releases."));
        return;
    }

    if (snapshot->state != OTA_RELEASE_CATALOG_READY) return;

    size_t visible = 0;
    for (size_t index = 0; index < snapshot->count; ++index) {
        ota_release_entry_t release;
        if (!ota_release_catalog_entry(index, &release) ||
            release.channel != s_channel) {
            continue;
        }

        char row[196];
        snprintf(
            row,
            sizeof(row),
            "%s   |   %s\nPublished %s   |   %.2f MB",
            release.version,
            relation_text(&release),
            release.published[0]
                ? release.published
                : "unknown",
            release.asset_size / 1048576.0);

        ui_popup_add_selectable_row(
            s_list,
            row,
            8,
            8 + (int32_t)visible * 68,
            804,
            60,
            release_select_cb,
            (void *)(uintptr_t)index);
        ++visible;
    }

    if (visible == 0) {
        lv_label_set_text(
            s_status,
            s_channel == OTA_RELEASE_CHANNEL_STABLE
                ? ui_text("No compatible stable releases were found.")
                : ui_text("No compatible nightly builds were found."));
    } else {
        lv_label_set_text(
            s_status,
            s_channel == OTA_RELEASE_CHANNEL_STABLE
                ? ui_text("Tap a stable release for details or installation.")
                : ui_text("Development builds: select one for details and warning."));
    }
}


static void timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ota_release_catalog_snapshot_t snapshot;
    ota_release_catalog_snapshot(&snapshot);
    render_catalog(&snapshot);
}


static void select_tab(ota_release_channel_t channel)
{
    s_channel = channel;
    ui_popup_set_selectable_row_selected(
        s_stable_tab,
        channel == OTA_RELEASE_CHANNEL_STABLE);
    ui_popup_set_selectable_row_selected(
        s_nightly_tab,
        channel == OTA_RELEASE_CHANNEL_NIGHTLY);
    s_rendered_count = SIZE_MAX;
    timer_cb(s_timer);
}


static void tab_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    select_tab((ota_release_channel_t)(uintptr_t)
        lv_event_get_user_data(event));
}


static void refresh_cb(lv_event_t *event)
{
    (void)event;
    s_rendered_state = OTA_RELEASE_CATALOG_IDLE;
    s_rendered_count = SIZE_MAX;
    ota_release_catalog_start();
}


static void custom_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    ui_ota_custom_cb_t custom_fn = s_custom_fn;
    browser_close();
    if (custom_fn) custom_fn();
}


void ui_ota_release_browser_show(
    ui_ota_start_cb_t start_cb,
    ui_ota_custom_cb_t custom_cb_fn)
{
    s_start_fn = start_cb;
    s_custom_fn = custom_cb_fn;

    if (s_popup) {
        lv_obj_move_foreground(s_popup);
        return;
    }

    s_popup = ui_popup_create(
        lv_layer_top(), 900, 540, UI_POPUP_STANDARD);
    if (!s_popup) {
        s_start_fn = NULL;
        s_custom_fn = NULL;
        return;
    }

    ui_popup_add_title(
        s_popup, ui_text("FIRMWARE RELEASES"), false, 0);
    ui_popup_add_header_divider(s_popup, 44);

    s_stable_tab = ui_popup_add_selectable_row(
        s_popup,
        ui_text("STABLE"),
        230, 52, 210, 44,
        tab_cb,
        (void *)(uintptr_t)OTA_RELEASE_CHANNEL_STABLE);
    s_nightly_tab = ui_popup_add_selectable_row(
        s_popup,
        ui_text("NIGHTLY"),
        460, 52, 210, 44,
        tab_cb,
        (void *)(uintptr_t)OTA_RELEASE_CHANNEL_NIGHTLY);

    s_status = ui_popup_add_status_label(
        s_popup,
        ui_text("Loading firmware releases from GitHub..."),
        24, 104, 852);
    s_list = ui_popup_add_list(
        s_popup,
        24, 134, 852, 310);

    if (!s_stable_tab || !s_nightly_tab ||
        !s_status || !s_list) {
        browser_close();
        return;
    }

    ui_popup_add_standard_footer_divider(s_popup);
    ui_popup_add_footer_action(
        s_popup,
        UI_POPUP_ACTION_SECONDARY,
        LV_SYMBOL_EDIT " CUSTOM OTA URL",
        220,
        UI_POPUP_FOOTER_LEFT,
        custom_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s_popup,
        UI_POPUP_ACTION_SECONDARY,
        LV_SYMBOL_REFRESH " REFRESH",
        170,
        UI_POPUP_FOOTER_CENTER,
        refresh_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        160,
        UI_POPUP_FOOTER_RIGHT,
        close_cb,
        NULL,
        NULL);

    s_channel = OTA_RELEASE_CHANNEL_STABLE;
    s_rendered_state = OTA_RELEASE_CATALOG_IDLE;
    s_rendered_count = SIZE_MAX;
    s_rendered_channel = OTA_RELEASE_CHANNEL_STABLE;
    select_tab(OTA_RELEASE_CHANNEL_STABLE);

    s_timer = lv_timer_create(timer_cb, 200, NULL);
    ota_release_catalog_start();
    timer_cb(s_timer);
}
