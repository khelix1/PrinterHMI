#include "ui_ota_release_browser.h"
#include "ui_text.h"

#include "ota_release_catalog.h"
#include "ota_manager.h"
#include "ui_popup.h"
#include "ui_ota_popup.h"
#include "ui_ota_layout.h"
#include "ui_value_update.h"
#include "firmware_identity.h"

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
static lv_obj_t *s_refresh_button = NULL;
static ota_release_channel_t s_row_channels[OTA_RELEASE_CATALOG_MAX];
static size_t s_row_count;
static int32_t s_channel_scroll[2];
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
static uint32_t s_browser_epoch;
static bool s_catalog_rendering, s_refresh_dispatching;
static bool s_custom_pending;
static lv_obj_t *s_custom_owner;
static void custom_transition(void *data);


static const char *relation_text(
    const ota_release_entry_t *release)
{
    if (!release) return "";

    if (release->relation == OTA_RELEASE_INSTALLED) {
        return "Installed";
    }

    switch (release->relation) {
        case OTA_RELEASE_NEWER: return "Update available";
        case OTA_RELEASE_OLDER: return "Older";
        case OTA_RELEASE_SWITCH_CHANNEL:
            return release->channel == OTA_RELEASE_CHANNEL_NIGHTLY ? "Switch to Nightly" : "Switch to Stable";
        case OTA_RELEASE_OTHER_BUILD: return "Different build";
        default: return "Available";
    }
}


static void confirm_close(void)
{
    lv_obj_t *confirm=s_confirm;
    s_confirm=NULL;
    if(confirm)lv_obj_delete(confirm);
}


static void browser_close(void)
{
    uint32_t epoch=++s_browser_epoch;
    bool installing=s_installing;
    lv_obj_t *popup=s_popup, *confirm=s_confirm;
    lv_timer_t *timer=s_timer;
    s_popup=s_confirm=s_status=s_list=s_stable_tab=s_nightly_tab=s_refresh_button=NULL;
    s_timer=NULL;
    if(s_custom_pending)lv_async_call_cancel(custom_transition,NULL);
    s_custom_pending=false;s_custom_owner=NULL;
    s_row_count=0;memset(s_channel_scroll,0,sizeof(s_channel_scroll));
    s_start_fn=NULL;s_custom_fn=NULL;s_channel=OTA_RELEASE_CHANNEL_STABLE;
    s_rendered_state=OTA_RELEASE_CATALOG_IDLE;s_rendered_count=SIZE_MAX;
    s_rendered_error[0]=0;s_rendered_channel=OTA_RELEASE_CHANNEL_STABLE;
    free(s_selected);s_selected=NULL;s_installing=false;
    if(timer)lv_timer_delete(timer);
    if(confirm)lv_obj_delete(confirm);
    if(popup)lv_obj_delete(popup);
    if(!installing && epoch==s_browser_epoch)ui_ota_popup_resume_camera();
}

static void browser_field_deleted(lv_event_t *event)
{
    lv_obj_t *target=lv_event_get_target_obj(event);
    if(target==s_status)s_status=NULL;
    else if(target==s_list){s_list=NULL;s_row_count=0;if(s_timer)lv_timer_delete(s_timer);s_timer=NULL;}
    else if(target==s_stable_tab)s_stable_tab=NULL;
    else if(target==s_nightly_tab)s_nightly_tab=NULL;
    else if(target==s_refresh_button)s_refresh_button=NULL;
    else return;
    ++s_browser_epoch;
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
        !s_popup || !s_confirm || ota_manager_is_running() ||
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

    if(!s_popup || !s_list || s_refresh_dispatching || s_catalog_rendering)return;
    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);
    uint32_t epoch=s_browser_epoch;
    ota_release_entry_t selected;
    bool found=ota_release_catalog_entry(index,&selected);
    if(epoch!=s_browser_epoch || !s_popup || !found)return;

    if (!s_selected) {
        s_selected = heap_caps_calloc(
            1,
            sizeof(*s_selected),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_selected) {
            s_selected = calloc(1, sizeof(*s_selected));
        }
    }

    if (!s_selected) return;
    *s_selected=selected;
    confirm_close();
    if(epoch!=s_browser_epoch || !s_popup || !s_selected)return;

    bool nightly =
        s_selected->channel == OTA_RELEASE_CHANNEL_NIGHTLY;
    bool installed =
        s_selected->relation == OTA_RELEASE_INSTALLED;
    bool downgrade =
        s_selected->relation == OTA_RELEASE_OLDER;
    bool switch_channel =
        s_selected->relation == OTA_RELEASE_SWITCH_CHANNEL;
    bool danger = nightly || installed || downgrade;

    s_confirm=ui_ota_dialog("Install firmware?",720,440,danger ? UI_POPUP_DANGER : UI_POPUP_STANDARD);
    if(!s_confirm)return;
    lv_obj_add_event_cb(s_confirm,confirm_deleted,LV_EVENT_DELETE,NULL);

    const char *action =
        installed
            ? (nightly ? "Reinstall Nightly" : "Reinstall")
            : (nightly
                ? (downgrade ? "Install older Nightly" : "Install Nightly")
                : (downgrade ? "Downgrade to" :
                   switch_channel ? "Switch to Stable" : "Install"));
    char title[112];
    snprintf(title, sizeof(title), "%s %s",
             action, s_selected->version);

    lv_label_set_text(lv_obj_get_child(s_confirm,0),title);

    const char *warning = "";
    if (nightly && installed) {
        warning =
            "Warning: this rewrites the installed development build.\n\n";
    } else if (nightly && downgrade) {
        warning = "Warning: this replaces the current nightly with an older development build.\n\n";
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
    } else if (switch_channel) {
        warning =
            "This changes the firmware channel to Stable. Development and "
            "nightly version numbers do not establish an upgrade or downgrade "
            "relative to a stable release. Review the release notes before switching.\n\n";
    }

    lv_obj_t *content = ui_ota_body(s_confirm);
    char installed_build[160];
    firmware_identity_format(installed_build, sizeof(installed_build));
    ui_ota_text(content, installed_build, UI_FONT_CAPTION, UI_TEXT_DIM);
    char target_build[160];
    snprintf(target_build, sizeof(target_build), "Selected: %s | %s",
             nightly ? "Nightly" : "Stable", s_selected->version);
    ui_ota_text(content, target_build, UI_FONT_BODY, UI_TEXT);
    if (warning[0])
        ui_ota_text(content, warning, UI_FONT_BODY, UI_TEXT);
    char details[160];
    snprintf(details, sizeof(details), "Published: %s | Download: %.2f MB",
             s_selected->published[0] ? s_selected->published : "Unknown",
             s_selected->asset_size / 1048576.0);
    ui_ota_text(content, details, UI_FONT_CAPTION, UI_TEXT_DIM);
    ui_ota_text(content, "The controller will reboot after installation. Keep power connected. Settings and printer profiles are retained.",
                UI_FONT_BODY, UI_TEXT);
    ui_ota_text(content, "Release notes", UI_FONT_CAPTION, UI_TEXT_DIM);
    ui_ota_text(content, s_selected->notes[0] ? s_selected->notes : "No release notes were provided.",
                UI_FONT_BODY, UI_TEXT);
    ui_ota_action(s_confirm,UI_POPUP_ACTION_CLOSE,"Close",confirm_close_cb,NULL);

    const char *button_text =
        installed
            ? LV_SYMBOL_REFRESH " Reinstall"
            : (nightly
                ? LV_SYMBOL_WARNING " Install Nightly"
                : (downgrade
                    ? LV_SYMBOL_WARNING " Downgrade"
                    : LV_SYMBOL_DOWNLOAD " Install"));

    ui_ota_action(s_confirm,danger ? UI_POPUP_ACTION_DANGER : UI_POPUP_ACTION_CONFIRM,
                   button_text,install_cb,NULL);
    ui_ota_finish(s_confirm);
}


/* Keep catalog rows alive while switching channels. Only explicit catalog
 * state changes rebuild them; tabs filter locally without network work. */
static void show_channel_rows(void)
{
    if(!s_list)return;
    size_t visible = 0;
    for (size_t i = 0; i < s_row_count; ++i) {
        lv_obj_t *row = lv_obj_get_child(s_list, (int32_t)i);
        if(!row)continue;
        if (s_row_channels[i] == s_channel) {
            lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);
            ++visible;
        } else lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
    }
    s_rendered_channel = s_channel;
    ui_value_set_text(s_status, visible
        ? (s_channel == OTA_RELEASE_CHANNEL_STABLE
            ? "Tap a stable release for details or installation."
            : "Development builds: select one for details and warning.")
        : (s_channel == OTA_RELEASE_CHANNEL_STABLE
            ? "No compatible stable releases were found."
            : "No compatible nightly builds were found."));
}

static void render_catalog_impl(
    const ota_release_catalog_snapshot_t *snapshot)
{
    if (!snapshot || !s_popup || !s_status || !s_list) return;
    uint32_t epoch=s_browser_epoch;
    if (s_refresh_button) {
        if (snapshot->state == OTA_RELEASE_CATALOG_LOADING)
            lv_obj_add_state(s_refresh_button, LV_STATE_DISABLED);
        else lv_obj_remove_state(s_refresh_button, LV_STATE_DISABLED);
        ui_value_set_text(lv_obj_get_child(s_refresh_button, 0),
            snapshot->state == OTA_RELEASE_CATALOG_ERROR ? "Retry" : "Refresh");
    }

    if (snapshot->state == s_rendered_state &&
        snapshot->count == s_rendered_count &&
        !strcmp(snapshot->error,s_rendered_error)) {
        if (snapshot->state == OTA_RELEASE_CATALOG_READY &&
            s_channel != s_rendered_channel) show_channel_rows();
        return;
    }

    snprintf(s_rendered_error,sizeof(s_rendered_error),"%s",snapshot->error);
    s_rendered_state = snapshot->state;
    s_rendered_count = snapshot->count;
    s_rendered_channel = s_channel;
    lv_obj_clean(s_list);
    if(epoch!=s_browser_epoch || !s_list || !s_popup)return;
    s_row_count = 0;
    memset(s_channel_scroll, 0, sizeof(s_channel_scroll));

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

    if (snapshot->state != OTA_RELEASE_CATALOG_READY) {
        lv_label_set_text(s_status, "Release catalog is idle. Tap Refresh to load firmware releases.");
        return;
    }

    for (size_t index = 0; index < snapshot->count && s_row_count < OTA_RELEASE_CATALOG_MAX; ++index) {
        ota_release_entry_t release;
        bool found=ota_release_catalog_entry(index,&release);
        if(epoch!=s_browser_epoch || !s_popup || !s_list)return;
        if(!found)continue;

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
        if (ui_theme_is_studio()) {
            ui_apply_surface_role(button,UI_SURFACE_TRANSPARENT);
            ui_apply_divider_border(button,LV_BORDER_SIDE_BOTTOM);
        }
        lv_obj_set_style_min_height(button,64,0);lv_obj_set_style_pad_all(button,12,0);
        if (release.relation == OTA_RELEASE_INSTALLED && !ui_theme_is_studio())
            lv_obj_set_style_border_color(button, UI_OK_BRIGHT, 0);
        lv_obj_set_flex_flow(button,LV_FLEX_FLOW_COLUMN);
        lv_obj_t *label=lv_obj_get_child(button,0);
        lv_obj_set_width(label,LV_PCT(100));lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
        ui_apply_custom_label_style(label,UI_FONT_BODY,UI_TEXT);
        lv_obj_add_event_cb(button,release_select_cb,LV_EVENT_CLICKED,(void *)(uintptr_t)index);
        s_row_channels[s_row_count++] = release.channel;
    }

    show_channel_rows();
}


static void timer_cb(lv_timer_t *timer)
{
    if(timer!=s_timer || !s_popup || !s_list || s_catalog_rendering || s_custom_pending)return;
    uint32_t epoch=s_browser_epoch;
    s_catalog_rendering=true;
    ota_release_catalog_snapshot_t snapshot;
    ota_release_catalog_snapshot(&snapshot);
    if(epoch==s_browser_epoch)render_catalog_impl(&snapshot);
    s_catalog_rendering=false;
}


static void select_tab(ota_release_channel_t channel)
{
    if (!s_popup || !s_list || !s_stable_tab || !s_nightly_tab) return;
    if (channel == s_channel && s_rendered_count != SIZE_MAX) return;
    s_channel_scroll[s_channel] = lv_obj_get_scroll_y(s_list);
    s_channel = channel;
    ui_button_set_filter_active(s_stable_tab,channel==OTA_RELEASE_CHANNEL_STABLE);
    ui_button_set_filter_active(s_nightly_tab,channel==OTA_RELEASE_CHANNEL_NIGHTLY);
    uint32_t epoch=s_browser_epoch;
    timer_cb(s_timer);
    if(epoch!=s_browser_epoch || !s_popup || !s_list)return;
    lv_obj_update_layout(s_popup);
    lv_obj_scroll_to_y(s_list,s_channel_scroll[s_channel],LV_ANIM_OFF);
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
    if(!s_popup || !s_list || s_refresh_dispatching)return;
    uint32_t epoch=s_browser_epoch;
    s_refresh_dispatching=true;
    ota_release_catalog_snapshot_t snapshot;
    ota_release_catalog_snapshot(&snapshot);
    if(epoch!=s_browser_epoch || snapshot.state==OTA_RELEASE_CATALOG_LOADING)goto done;
    confirm_close();
    if(epoch!=s_browser_epoch || !s_popup)goto done;
    s_rendered_state=OTA_RELEASE_CATALOG_IDLE;s_rendered_count=SIZE_MAX;
    (void)ota_release_catalog_start();
    if(epoch==s_browser_epoch)timer_cb(s_timer);
 done:
    s_refresh_dispatching=false;
}

static void custom_transition(void *data)
{
    (void)data;
    if (!s_custom_pending || !s_custom_owner || s_custom_owner != s_popup) return;
    ui_ota_custom_cb_t custom_fn = s_custom_fn;
    s_custom_pending = false;
    s_custom_owner = NULL;
    /* Keep camera transport quiet through this single UI transition. */
    s_installing = true;
    browser_close();
    if (custom_fn) custom_fn();
    else ui_ota_popup_resume_camera();
}

static void custom_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED ||
        s_custom_pending || !s_custom_fn || !s_popup) return;
    if (lv_async_call(custom_transition, NULL) != LV_RESULT_OK) {
        ui_value_set_text(s_status, "Could not open URL editor. Try again.");
        return;
    }
    s_custom_pending = true;
    s_custom_owner = s_popup;
    ui_value_set_text(s_status, "Opening URL editor...");
}


void ui_ota_release_browser_show(
    ui_ota_start_cb_t start_cb,
    ui_ota_custom_cb_t custom_cb_fn)
{
    if (ota_manager_is_running())return;
    if (s_popup) {
        lv_obj_move_foreground(s_popup);
        return;
    }
    ++s_browser_epoch;
    s_start_fn=start_cb;s_custom_fn=custom_cb_fn;
    ui_ota_popup_quiesce_camera();

    s_popup=ui_ota_dialog("Firmware releases",900,540,UI_POPUP_STANDARD);
    if(!s_popup){s_start_fn=NULL;s_custom_fn=NULL;ui_ota_popup_resume_camera();return;}
    lv_obj_add_event_cb(s_popup,browser_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_t *body=ui_ota_body(s_popup);
    char installed[160];
    firmware_identity_format(installed, sizeof(installed));
    ui_ota_text(body, installed, UI_FONT_CAPTION, UI_TEXT_DIM);
    lv_obj_clear_flag(body,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *tabs=lv_obj_create(body);ui_responsive_column(tabs);
    lv_obj_set_flex_flow(tabs,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(tabs,8,0);
    s_stable_tab=ui_button_create(tabs,UI_BUTTON_OUTLINED,"Stable");
    s_nightly_tab=ui_button_create(tabs,UI_BUTTON_OUTLINED,"Nightly");
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
    lv_obj_set_scroll_dir(s_list,LV_DIR_VER);lv_obj_set_scrollbar_mode(s_list,LV_SCROLLBAR_MODE_OFF);
    ui_ota_action(s_popup,UI_POPUP_ACTION_SECONDARY,"Custom URL",custom_cb,NULL);
    s_refresh_button=ui_ota_action(s_popup,UI_POPUP_ACTION_SECONDARY,"Refresh",refresh_cb,NULL);
    ui_ota_action(s_popup,UI_POPUP_ACTION_CLOSE,"Close",close_cb,NULL);

    s_channel = OTA_RELEASE_CHANNEL_STABLE;
    s_rendered_state = OTA_RELEASE_CATALOG_IDLE;
    s_rendered_count = SIZE_MAX;
    s_rendered_channel = OTA_RELEASE_CHANNEL_STABLE;
    firmware_identity_t installed_identity;
    firmware_identity_get(&installed_identity);
    select_tab(installed_identity.channel == FIRMWARE_CHANNEL_NIGHTLY
        ? OTA_RELEASE_CHANNEL_NIGHTLY : OTA_RELEASE_CHANNEL_STABLE);

    lv_obj_t *fields[]={s_status,s_list,s_stable_tab,s_nightly_tab,s_refresh_button};
    for(size_t i=0;i<5;i++)lv_obj_add_event_cb(fields[i],browser_field_deleted,LV_EVENT_DELETE,NULL);
    s_timer = lv_timer_create(timer_cb, 200, NULL);
    /* Reopening and switching channels use the in-memory catalog. Refresh
     * explicitly requests newer releases; initial load/retry remains async. */
    ota_release_catalog_snapshot_t snapshot;
    ota_release_catalog_snapshot(&snapshot);
    if (snapshot.state != OTA_RELEASE_CATALOG_READY &&
        snapshot.state != OTA_RELEASE_CATALOG_LOADING)
        (void)ota_release_catalog_start();
    timer_cb(s_timer);
    ui_ota_finish(s_popup);
    if(s_stable_tab)ui_text_fit_single_line(lv_obj_get_child(s_stable_tab,0),UI_FONT_BODY);
    if(s_nightly_tab)ui_text_fit_single_line(lv_obj_get_child(s_nightly_tab,0),UI_FONT_BODY);
}
