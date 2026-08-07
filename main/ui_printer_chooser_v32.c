#include "ui_printer_chooser_v32.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "moonraker_probe.h"
#include "printer_profile_health.h"
#include "moonraker_live_websocket.h"
#include "printer_preview_cache_v32.h"
#include "ui_button.h"
#include "ui_theme.h"
#include "ui_page_geometry_v32.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *name;
    lv_obj_t *endpoint;
    lv_obj_t *status;
    lv_obj_t *preview_box;
    lv_obj_t *preview;
    lv_obj_t *preview_icon;
    lv_obj_t *preview_image;
    uint32_t preview_revision;
    lv_obj_t *active;
} chooser_card_t;

static lv_obj_t *s_root = NULL;
static lv_timer_t *s_timer = NULL;
static chooser_card_t s_cards[MOONRAKER_CONFIG_MAX_PROFILES];

static ui_printer_chooser_select_cb_t s_select_cb = NULL;
static ui_printer_chooser_manage_cb_t s_manage_cb = NULL;


static void apply_status_style(lv_obj_t *label, bool configured, bool online)
{
    if (!label) return;

    if (!configured) {
        ui_apply_label_dim(label);
    } else if (online) {
        ui_apply_label_success(label);
    } else {
        ui_apply_label_error(label);
    }
}


static void card_clicked_cb(lv_event_t *event)
{
    int index = (int)(intptr_t)lv_event_get_user_data(event);

    if (index < 0 || index >= MOONRAKER_CONFIG_MAX_PROFILES) return;

    const moonraker_profile_t *profile = moonraker_config_profile(index);

    if (!profile || !profile->configured) {
        if (s_manage_cb) s_manage_cb(event);
        return;
    }

    if (s_select_cb) s_select_cb(index);
}


static lv_obj_t *make_label(
    lv_obj_t *parent,
    const char *text,
    int x,
    int y,
    int width)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(label, x, y);
    return label;
}


static void create_card(int index, int x, int y)
{
    chooser_card_t *card = &s_cards[index];

    card->root = lv_obj_create(s_root);
    lv_obj_set_size(card->root, 390, 184);
    lv_obj_set_pos(card->root, x, y);
    lv_obj_clear_flag(card->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card->root, LV_OBJ_FLAG_CLICKABLE);
    ui_apply_card_style(card->root);

    lv_obj_add_event_cb(
        card->root,
        card_clicked_cb,
        LV_EVENT_CLICKED,
        (void *)(intptr_t)index);

    card->preview_box = lv_obj_create(card->root);
    lv_obj_set_size(card->preview_box, 116, 116);
    lv_obj_set_pos(card->preview_box, 12, 34);
    lv_obj_clear_flag(card->preview_box, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_preview_style(card->preview_box);

    card->preview_icon = lv_label_create(card->preview_box);
    lv_label_set_text(card->preview_icon, LV_SYMBOL_FILE);
    ui_apply_text_popup_title(card->preview_icon);
    ui_apply_label_dim(card->preview_icon);
    lv_obj_align(card->preview_icon, LV_ALIGN_TOP_MID, 0, 16);

    card->preview = lv_label_create(card->preview_box);
    lv_label_set_text(card->preview, "NO LIVE\nPREVIEW");
    lv_obj_set_width(card->preview, 96);
    lv_label_set_long_mode(card->preview, LV_LABEL_LONG_DOT);
    ui_apply_text_caption(card->preview);
    ui_apply_label_dim(card->preview);
    lv_obj_set_style_text_align(card->preview, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(card->preview, LV_ALIGN_BOTTOM_MID, 0, -14);

    card->name = make_label(card->root, "PRINTER", 146, 20, 210);
    ui_apply_text_title(card->name);
    ui_apply_label_bright(card->name);

    card->endpoint = make_label(card->root, "--", 146, 55, 220);
    ui_apply_text_caption(card->endpoint);
    ui_apply_label_dim(card->endpoint);

    card->status = make_label(card->root, "CHECKING...", 146, 88, 220);
    ui_apply_text_body_large(card->status);
    ui_apply_label_dim(card->status);

    lv_obj_t *hint = make_label(card->root, "TAP TO OPEN", 146, 128, 210);
    ui_apply_text_caption(hint);
    ui_apply_label_dim(hint);

    card->active = make_label(card->root, "ACTIVE", 292, 12, 72);
    ui_apply_text_caption(card->active);
    ui_apply_label_success(card->active);
    lv_obj_set_style_text_align(card->active, LV_TEXT_ALIGN_RIGHT, 0);
}


static void refresh_cards(void)
{
    int active = moonraker_config_active_profile_index();
    moonraker_state_t state_snapshot;
    moonraker_state_snapshot(&state_snapshot);
    const moonraker_state_t *state = &state_snapshot;

    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        chooser_card_t *card = &s_cards[index];
        const moonraker_profile_t *profile = moonraker_config_profile(index);
        bool configured = profile && profile->configured;
        bool known = false;
        bool online =
            configured &&
            printer_profile_health_get(index, &known);

        if (configured && index == active && state && state->moonraker_ok) {
            online = true;
            known = true;
        }

        if (!card->root) continue;

        const char *cached_file = NULL;
        uint32_t cached_revision = 0;

        const lv_image_dsc_t *cached_image =
            configured
                ? printer_preview_cache_v32_image(
                    index,
                    &cached_file,
                    &cached_revision)
                : NULL;

        if (cached_image) {
            if (!card->preview_image) {
                card->preview_image =
                    lv_image_create(card->preview_box);
            }

            if (card->preview_image &&
                card->preview_revision != cached_revision) {
                lv_image_set_src(card->preview_image, cached_image);

                int scale_x =
                    (108 * 256) / (int)cached_image->header.w;

                int scale_y =
                    (108 * 256) / (int)cached_image->header.h;

                int scale = scale_x < scale_y ? scale_x : scale_y;
                if (scale > 256) scale = 256;
                if (scale < 1) scale = 1;

                lv_image_set_scale(card->preview_image, scale);
                lv_obj_center(card->preview_image);
                card->preview_revision = cached_revision;
            }

            if (card->preview_image) {
                lv_obj_clear_flag(
                    card->preview_image,
                    LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(card->preview_image);
            }

            lv_obj_add_flag(card->preview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(card->preview_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            if (card->preview_image) {
                lv_obj_add_flag(
                    card->preview_image,
                    LV_OBJ_FLAG_HIDDEN);
            }

            lv_obj_clear_flag(card->preview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(card->preview_icon, LV_OBJ_FLAG_HIDDEN);
            card->preview_revision = 0;
        }

        if (!configured) {
            char empty_name[32];
            snprintf(empty_name, sizeof(empty_name), "ADD PRINTER %d", index + 1);
            lv_label_set_text(card->name, empty_name);
            lv_label_set_text(card->endpoint, "EMPTY PROFILE SLOT");
            lv_label_set_text(card->status, "NOT CONFIGURED");
            if (!cached_image)
                lv_label_set_text(card->preview, "ADD A\nPRINTER");
            lv_obj_add_flag(card->active, LV_OBJ_FLAG_HIDDEN);
            apply_status_style(card->status, false, false);
            continue;
        }

        char endpoint[96];
        snprintf(endpoint, sizeof(endpoint), "%s:%d", profile->host, profile->port);

        lv_label_set_text(card->name, profile->name);
        lv_label_set_text(card->endpoint, endpoint);
        bool active_live =
            index == active &&
            state &&
            state->moonraker_ok;
        bool status_online = active_live;
        const char *status_text = NULL;

        if (index == active) {
            /* The active card reports only synchronized live state. */
            if (active_live &&
                state->printer_state[0] &&
                strcmp(state->printer_state, "--") != 0) {
                status_text = state->printer_state;
            } else {
                status_text = "OFFLINE / RETRYING";
            }
        } else {
            char inactive_state[PRINTER_PROFILE_HEALTH_STATE_LENGTH] = "";
            bool has_live_state =
                known &&
                online &&
                printer_profile_health_get_live_state(
                    index, inactive_state, sizeof(inactive_state));
            bool inactive_online_fresh =
                known &&
                online &&
                printer_profile_health_live_state_fresh(
                    index, 5000000LL);
            bool verifying = known && online && !inactive_online_fresh;

            status_online = inactive_online_fresh;
            status_text = !known
                ? "VERIFYING..."
                : (!online
                    ? "OFFLINE"
                    : (verifying
                        ? "VERIFYING..."
                        : (has_live_state
                            ? inactive_state
                            : "ONLINE")));
        }

        lv_label_set_text(card->status, status_text);
        if (index != active &&
            known &&
            online &&
            !status_online) {
            ui_apply_label_dim(card->status);
        } else {
            apply_status_style(card->status, true, status_online);
        }

        if (index == active) {
            lv_obj_clear_flag(card->active, LV_OBJ_FLAG_HIDDEN);

            if (!cached_image) {
                if (state && state->live_data_ok && state->printer_file[0]) {
                    lv_label_set_text(card->preview, state->printer_file);
                    ui_apply_label_bright(card->preview);
                } else {
                    lv_label_set_text(card->preview, online ? "READY FOR\nLIVE DATA" : "NO LIVE\nPREVIEW");
                    ui_apply_label_dim(card->preview);
                }
            }
        } else {
            lv_obj_add_flag(card->active, LV_OBJ_FLAG_HIDDEN);
            if (!cached_image) {
                lv_label_set_text(card->preview, online ? "AVAILABLE\nTO OPEN" : "NO LIVE\nPREVIEW");
                ui_apply_label_dim(card->preview);
            }
        }
    }
}



static void chooser_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_cards();
}


void ui_printer_chooser_v32_refresh(void)
{
    if (!s_root) return;
    refresh_cards();
}


void ui_printer_chooser_v32_show(
    ui_printer_chooser_select_cb_t select_cb,
    ui_printer_chooser_manage_cb_t manage_cb)
{
    s_select_cb = select_cb;
    s_manage_cb = manage_cb;

    if (s_root) {
        refresh_cards();
        lv_obj_move_foreground(s_root);
        return;
    }

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root,
                    UI_PAGE_ROOT_WIDTH,
                    UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(s_root,
                   UI_PAGE_ROOT_X,
                   UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_root_style(s_root);

    lv_obj_t *title = make_label(s_root, "PRINTERS", 20, 12, 360);
    ui_apply_text_heading(title);
    ui_apply_label_bright(title);

    lv_obj_t *subtitle = make_label(
        s_root,
        "Choose a printer to open its live operator dashboard.",
        20,
        48,
        560);
    ui_apply_text_caption(subtitle);
    ui_apply_label_dim(subtitle);

    lv_obj_t *manage = ui_button_create_icon(
        s_root,
        UI_BUTTON_OUTLINED,
        LV_SYMBOL_SETTINGS,
        "MANAGE PRINTERS",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_HORIZONTAL);

    if (manage) {
        lv_obj_set_size(manage, 220, 46);
        lv_obj_set_pos(manage, 606, 12);

        if (s_manage_cb) {
            lv_obj_add_event_cb(
                manage,
                s_manage_cb,
                LV_EVENT_CLICKED,
                NULL);
        }
    }

    create_card(0, 20, 76);
    create_card(1, 424, 76);
    create_card(2, 20, 274);
    create_card(3, 424, 274);

    refresh_cards();

    s_timer = lv_timer_create(chooser_timer_cb, 500, NULL);
    chooser_timer_cb(s_timer);

    ESP_LOGI("printer_chooser", "Chooser visible");

    lv_obj_move_foreground(s_root);
}


void ui_printer_chooser_v32_hide(void)
{
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }

    if (s_root) {
        lv_obj_delete(s_root);
        s_root = NULL;
    }

    memset(s_cards, 0, sizeof(s_cards));
    s_select_cb = NULL;
    s_manage_cb = NULL;
}


bool ui_printer_chooser_v32_is_visible(void)
{
    return s_root != NULL;
}
