#include "ui_ota_release_browser.h"
#include "ui_text.h"

#include "ota_release_catalog.h"
#include "ui_popup.h"
#include "ui_ota_popup.h"
#include "ui_ota_layout.h"
#include "ui_value_update.h"

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
static bool s_installing = false;
static char s_rendered_error[128];


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

    if(s_popup) {lv_obj_t *popup=s_popup;s_popup=NULL;lv_obj_delete(popup);}

    s_status = NULL;
    s_list = NULL;
    s_stable_tab = NULL;
    s_nightly_tab = NULL;
    s_start_fn = NULL;
    s_custom_fn = NULL;
    s_channel = OTA_RELEASE_CHANNEL_STABLE;
    s_rendered_state = OTA_RELEASE_CATALOG_IDLE;
    s_rendered_count = SIZE_MAX;
    s_rendered_error[0]=0;
    s_rendered_channel = OTA_RELEASE_CHANNEL_STABLE;
    free(s_selected);
    s_selected = NULL;
    if (!s_installing) {
        ui_ota_popup_resume_camera();
    }
    s_installing = false;
}


static void browser_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=s_popup)return;
    s_popup=NULL;browser_close();
}
static void confirm_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)==s_confirm)s_confirm=NULL;
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

    s_installing = true;
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

    s_confirm=ui_ota_dialog("INSTALL FIRMWARE?",720,440,danger ? UI_POPUP_DANGER : UI_POPUP_STANDARD);
    if(!s_confirm)return;
    lv_obj_add_event_cb(s_confirm,confirm_deleted,LV_EVENT_DELETE,NULL);

    const char *action =
        installed
            ? (nightly ? "REINSTALL NIGHTLY" : "REINSTALL")
            : (nightly
                ? "INSTALL NIGHTLY"
                : (downgrade ? "DOWNGRADE TO" : "INSTALL"));
    char title[112];
    snprintf(title, sizeof(title), "%s %s",
             action, s_selected->version);

    lv_label_set_text(lv_obj_get_child(s_confirm,0),title);

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

    ui_ota_text(ui_ota_body(s_confirm),body,UI_FONT_BODY,UI_TEXT);
    ui_ota_action(s_confirm,UI_POPUP_ACTION_CLOSE,"CLOSE",confirm_close_cb,NULL);

    const char *button_text =
        installed
            ? LV_SYMBOL_REFRESH " REINSTALL"
            : (nightly
                ? LV_SYMBOL_WARNING " INSTALL NIGHTLY"
                : (downgrade
                    ? LV_SYMBOL_WARNING " DOWNGRADE"
                    : LV_SYMBOL_DOWNLOAD " INSTALL"));

    ui_ota_action(s_confirm,danger ? UI_POPUP_ACTION_DANGER : UI_POPUP_ACTION_CONFIRM,
                   button_text,install_cb,NULL);
}


static void render_catalog(
    const ota_release_catalog_snapshot_t *snapshot)
{
    if (!snapshot || !s_status || !s_list) return;

    if (snapshot->state == s_rendered_state &&
        snapshot->count == s_rendered_count &&
        s_channel == s_rendered_channel &&
        !strcmp(snapshot->error,s_rendered_error)) {
        return;
    }

    snprintf(s_rendered_error,sizeof(s_rendered_error),"%s",snapshot->error);
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

        lv_obj_t *button=ui_button_create(s_list,UI_BUTTON_OUTLINED,row);
        lv_obj_set_size(button,LV_PCT(100),LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(button,64,0);lv_obj_set_style_pad_all(button,12,0);
        lv_obj_set_flex_flow(button,LV_FLEX_FLOW_COLUMN);
        lv_obj_t *label=lv_obj_get_child(button,0);
        lv_obj_set_width(label,LV_PCT(100));lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
        ui_apply_custom_label_style(label,UI_FONT_BODY,UI_TEXT);
        lv_obj_add_event_cb(button,release_select_cb,LV_EVENT_CLICKED,(void *)(uintptr_t)index);
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
    if(channel==OTA_RELEASE_CHANNEL_STABLE) {lv_obj_add_state(s_stable_tab,LV_STATE_CHECKED);lv_obj_remove_state(s_nightly_tab,LV_STATE_CHECKED);}
    else {lv_obj_remove_state(s_stable_tab,LV_STATE_CHECKED);lv_obj_add_state(s_nightly_tab,LV_STATE_CHECKED);}
    s_rendered_count = SIZE_MAX;
    timer_cb(s_timer);
    lv_obj_update_layout(s_popup);
    ui_text_fit_single_line(lv_obj_get_child(s_stable_tab,0),UI_FONT_BODY);
    ui_text_fit_single_line(lv_obj_get_child(s_nightly_tab,0),UI_FONT_BODY);
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
    ui_ota_popup_quiesce_camera();

    if (s_popup) {
        lv_obj_move_foreground(s_popup);
        return;
    }

    s_popup=ui_ota_dialog("FIRMWARE RELEASES",900,540,UI_POPUP_STANDARD);
    if(!s_popup){s_start_fn=NULL;s_custom_fn=NULL;ui_ota_popup_resume_camera();return;}
    lv_obj_add_event_cb(s_popup,browser_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_t *body=ui_ota_body(s_popup);
    lv_obj_clear_flag(body,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *tabs=lv_obj_create(body);ui_responsive_column(tabs);
    lv_obj_set_flex_flow(tabs,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(tabs,8,0);
    s_stable_tab=ui_button_create(tabs,UI_BUTTON_OUTLINED,"STABLE");
    s_nightly_tab=ui_button_create(tabs,UI_BUTTON_OUTLINED,"NIGHTLY");
    lv_obj_t *buttons[]={s_stable_tab,s_nightly_tab};
    for(unsigned i=0;i<2;i++) {
        lv_obj_set_size(buttons[i],96,48);lv_obj_set_flex_grow(buttons[i],1);
        lv_obj_add_event_cb(buttons[i],tab_cb,LV_EVENT_CLICKED,(void *)(uintptr_t)i);
        lv_obj_add_event_cb(buttons[i],ui_ota_fit_action,LV_EVENT_SIZE_CHANGED,NULL);
    }
    s_status=ui_ota_text(body,"Loading firmware releases...",UI_FONT_CAPTION,UI_TEXT_DIM);
    s_list=lv_obj_create(body);lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list,LV_PCT(100),0);lv_obj_set_flex_grow(s_list,1);
    lv_obj_set_flex_flow(s_list,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(s_list,8,0);
    lv_obj_set_scroll_dir(s_list,LV_DIR_VER);lv_obj_set_scrollbar_mode(s_list,LV_SCROLLBAR_MODE_AUTO);
    ui_ota_action(s_popup,UI_POPUP_ACTION_SECONDARY,"CUSTOM URL",custom_cb,NULL);
    ui_ota_action(s_popup,UI_POPUP_ACTION_SECONDARY,"REFRESH",refresh_cb,NULL);
    ui_ota_action(s_popup,UI_POPUP_ACTION_CLOSE,"CLOSE",close_cb,NULL);

    s_channel = OTA_RELEASE_CHANNEL_STABLE;
    s_rendered_state = OTA_RELEASE_CATALOG_IDLE;
    s_rendered_count = SIZE_MAX;
    s_rendered_channel = OTA_RELEASE_CHANNEL_STABLE;
    select_tab(OTA_RELEASE_CHANNEL_STABLE);

    s_timer = lv_timer_create(timer_cb, 200, NULL);
    ota_release_catalog_start();
    timer_cb(s_timer);
}
