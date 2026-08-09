#include "ui_drybox_page.h"
#include "ui_page_layout_profile.h"

#include <stdio.h>
#include <string.h>

#include "ui_page_title.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_button.h"
#include "ui_status_banner.h"
#include "ui_page_geometry.h"

/*
 * Theme A Drybox control surface.
 *
 * The public page structure intentionally retains the original field names
 * so the existing main.c adapter and live telemetry path remain unchanged.
 *
 * Field use in this design:
 *
 *   air_label       air-temperature value
 *   center_label    center-temperature value
 *   humidity_label  large humidity value
 *   target_label    large target-temperature value
 *   heater_label    heater state text
 *   fan_label       fan-speed value
 */

static ui_drybox_page_action_cb_t s_action_cb = NULL;

static lv_obj_t *s_heater_dot = NULL;
static lv_obj_t *s_fan_dot = NULL;
static lv_obj_t *s_active_dot = NULL;
static lv_obj_t *s_program_status_label = NULL;

static lv_obj_t *s_humidity_condition_label = NULL;
static lv_obj_t *s_humidity_bar = NULL;
static lv_obj_t *s_heater_activity_bar = NULL;

typedef enum {
    DRYBOX_PROGRAM_NONE = 0,
    DRYBOX_PROGRAM_PLA,
    DRYBOX_PROGRAM_PETG,
    DRYBOX_PROGRAM_HOLD,
    DRYBOX_PROGRAM_RESUME,
    DRYBOX_PROGRAM_STOP,
    DRYBOX_PROGRAM_COUNT
} drybox_program_t;

static lv_obj_t *s_program_buttons[DRYBOX_PROGRAM_COUNT] = {0};

static lv_color_t drybox_program_accent(
    drybox_program_t program)
{
    switch (program) {
        case DRYBOX_PROGRAM_PLA:
            return UI_ACCENT_CYAN;

        case DRYBOX_PROGRAM_PETG:
            return UI_ACCENT_INFO;

        case DRYBOX_PROGRAM_HOLD:
            return UI_WARN;

        case DRYBOX_PROGRAM_RESUME:
            return UI_OK_BRIGHT;

        case DRYBOX_PROGRAM_STOP:
            return UI_DANGER_BRIGHT;

        case DRYBOX_PROGRAM_NONE:
        default:
            return UI_BORDER_SOFT;
    }
}

static void set_label_color(
    lv_obj_t *label,
    lv_color_t color);

static bool text_contains(
    const char *text,
    const char *needle)
{
    return text && needle && strstr(text, needle) != NULL;
}

static bool drybox_banner_is_offline(const char *banner)
{
    return !banner ||
           !banner[0] ||
           text_contains(banner, "OFFLINE") ||
           text_contains(banner, "UNAVAILABLE") ||
           text_contains(banner, "DISCONNECTED") ||
           text_contains(banner, "NO CONNECTION") ||
           text_contains(banner, "NOT CONNECTED");
}

static const char *drybox_status_text(const char *banner)
{
    if (text_contains(banner, "UNAVAILABLE")) return "UNAVAILABLE";
    if (drybox_banner_is_offline(banner)) return "OFFLINE";
    if (text_contains(banner, "ACTIVE") ||
        text_contains(banner, "DRYING")) return "DRYING";
    if (text_contains(banner, "HEATING")) return "HEATING";
    if (text_contains(banner, "READY")) return "READY";
    return "MONITORING";
}


static drybox_program_t drybox_program_from_state(
    ui_drybox_program_t program)
{
    switch (program) {
        case UI_DRYBOX_PROGRAM_PLA:
            return DRYBOX_PROGRAM_PLA;

        case UI_DRYBOX_PROGRAM_PETG:
            return DRYBOX_PROGRAM_PETG;

        case UI_DRYBOX_PROGRAM_HOLD:
            return DRYBOX_PROGRAM_HOLD;

        case UI_DRYBOX_PROGRAM_NONE:
        default:
            return DRYBOX_PROGRAM_NONE;
    }
}

static void set_program_button_state(
    drybox_program_t active_program,
    bool online)
{
    for (int i = DRYBOX_PROGRAM_PLA;
         i < DRYBOX_PROGRAM_COUNT;
         ++i) {
        lv_obj_t *button = s_program_buttons[i];

        if (!button) {
            continue;
        }

        if (online) {
            lv_obj_clear_state(button, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(button, LV_STATE_DISABLED);
        }

        bool active =
            online &&
            active_program == (drybox_program_t)i;

        lv_obj_set_style_border_color(
            button,
            active ? drybox_program_accent((drybox_program_t)i) : UI_BORDER_SOFT,
            0);

        lv_obj_set_style_border_width(
            button,
            active
                ? ui_theme_accessible_border_width(3)
                : UI_BORDER_THIN,
            0);

        lv_obj_set_style_bg_color(
            button,
            active ? UI_CONTROL_ALT : UI_BG_DEEP,
            0);

        lv_obj_set_style_bg_opa(
            button,
            online
                ? LV_OPA_COVER
                : ui_theme_accessible_opacity(LV_OPA_50),
            0);
    }

    if (s_active_dot) {
        lv_obj_set_style_bg_color(
            s_active_dot,
            online && active_program != DRYBOX_PROGRAM_NONE
                ? drybox_program_accent(active_program)
                : UI_TEXT_DIM,
            0);
    }

    if (s_program_status_label) {
        const char *status = "READY";

        if (!online) {
            status = "OFFLINE";
        } else {
            switch (active_program) {
                case DRYBOX_PROGRAM_PLA:
                    status = "PLA ACTIVE";
                    break;

                case DRYBOX_PROGRAM_PETG:
                    status = "PETG ACTIVE";
                    break;

                case DRYBOX_PROGRAM_HOLD:
                    status = "HOLDING";
                    break;

                case DRYBOX_PROGRAM_RESUME:
                    status = "RESUMING";
                    break;

                case DRYBOX_PROGRAM_STOP:
                    status = "READY";
                    break;

                case DRYBOX_PROGRAM_NONE:
                default:
                    status = "READY";
                    break;
            }
        }

        lv_label_set_text(s_program_status_label, status);

        set_label_color(
            s_program_status_label,
            online ? UI_ACCENT_CYAN : UI_TEXT_DIM);
    }
}

static void set_humidity_presentation(float humidity)
{
    const char *condition = "IDEAL";
    lv_color_t color = UI_ACCENT_CYAN;

    if (humidity < 10.0f) {
        condition = "VERY DRY";
        color = UI_ACCENT_INFO;
    } else if (humidity < 25.0f) {
        condition = "IDEAL";
        color = UI_OK_BRIGHT;
    } else if (humidity < 40.0f) {
        condition = "MODERATE";
        color = UI_WARN;
    } else {
        condition = "HIGH";
        color = UI_DANGER_BRIGHT;
    }

    if (s_humidity_condition_label) {
        lv_label_set_text(
            s_humidity_condition_label,
            condition);

        set_label_color(
            s_humidity_condition_label,
            color);
    }

    if (s_humidity_bar) {
        int value = (int)humidity;

        if (value < 0) {
            value = 0;
        } else if (value > 100) {
            value = 100;
        }

        lv_bar_set_value(
            s_humidity_bar,
            value,
            LV_ANIM_OFF);

        lv_obj_set_style_bg_color(
            s_humidity_bar,
            color,
            LV_PART_INDICATOR);
    }
}

static void set_label_color(
    lv_obj_t *label,
    lv_color_t color)
{
    if (!label) {
        return;
    }

    lv_obj_set_style_text_color(label, color, 0);
}

static lv_obj_t *make_text(
    lv_obj_t *parent,
    const char *text,
    const lv_font_t *font,
    lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text ? text : "");
    ui_apply_custom_label_style(label, font, color);

    return label;
}

static lv_obj_t *make_indicator_dot(
    lv_obj_t *parent,
    int x,
    int y,
    lv_color_t color)
{
    lv_obj_t *dot = lv_obj_create(parent);

    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(dot, 18, 18);
    lv_obj_set_pos(dot, x, y);
    ui_apply_surface_role(dot, UI_SURFACE_INDICATOR);
    lv_obj_set_style_bg_color(dot, color, 0);

    return dot;
}

static lv_obj_t *make_panel(
    lv_obj_t *parent,
    int x,
    int y,
    int width,
    int height)
{
    return ui_create_operator_card(
        parent,
        x,
        y,
        width,
        height);
}

static void make_divider(
    lv_obj_t *parent,
    int x,
    int y,
    int width)
{
    ui_create_operator_card_divider(
        parent,
        x,
        y,
        width);
}

static void drybox_button_event_cb(lv_event_t *event)
{
    const char *command =
        (const char *)lv_event_get_user_data(event);

    if (!command || !s_action_cb) {
        return;
    }

    s_action_cb(command, event);
}

static lv_obj_t *make_program_button(
    lv_obj_t *parent,
    const char *symbol,
    const char *text,
    const char *command,
    int x,
    int width,
    lv_color_t accent,
    lv_color_t fill)
{
    /*
     * The fill argument remains for call-site compatibility.
     * Button surface styling is owned by UI_BUTTON_OUTLINED.
     */
    (void)fill;

    lv_obj_t *button =
        ui_button_create_icon(
            parent,
            UI_BUTTON_OUTLINED,
            symbol,
            text,
            accent,
            UI_BUTTON_ICON_VERTICAL);

    if (!button) {
        return NULL;
    }

    lv_obj_set_size(
        button,
        width,
        88);

    lv_obj_set_pos(
        button,
        x,
        45);

    lv_obj_add_event_cb(
        button,
        drybox_button_event_cb,
        LV_EVENT_CLICKED,
        (void *)command);

    return button;
}

static void build_environment_panel(
    ui_drybox_page_t *page)
{
    const ui_dashboard_rect_t *rect =
        &ui_page_layout_profile_current()->drybox.environment;

    lv_obj_t *panel =
        make_panel(page->panel,
                   rect->x,
                   rect->y,
                   rect->width,
                   rect->height);

    ui_create_operator_card_heading(
        panel,
        "ENVIRONMENT",
        22,
        18);

    page->humidity_label = make_text(
        panel,
        "--.-",
        UI_FONT_PERCENT,
        UI_ACCENT_CYAN);

    lv_obj_set_width(page->humidity_label, 230);
    lv_obj_set_style_text_align(
        page->humidity_label,
        LV_TEXT_ALIGN_CENTER,
        0);
    lv_obj_set_pos(page->humidity_label, 78, 42);

    lv_obj_t *unit = make_text(
        panel,
        "%RH",
        UI_FONT_TITLE,
        UI_ACCENT_CYAN);

    lv_obj_align_to(
        unit,
        page->humidity_label,
        LV_ALIGN_OUT_BOTTOM_MID,
        0,
        -4);

    s_humidity_condition_label = make_text(
        panel,
        "IDEAL",
        UI_FONT_CAPTION,
        UI_OK_BRIGHT);

    lv_obj_set_width(
        s_humidity_condition_label,
        120);

    lv_obj_set_style_text_align(
        s_humidity_condition_label,
        LV_TEXT_ALIGN_CENTER,
        0);

    lv_obj_set_pos(
        s_humidity_condition_label,
        132,
        111);

    s_humidity_bar = lv_bar_create(panel);
    lv_obj_set_size(s_humidity_bar, 300, 7);
    lv_obj_set_pos(s_humidity_bar, 42, 130);
    lv_bar_set_range(s_humidity_bar, 0, 100);
    lv_bar_set_value(s_humidity_bar, 0, LV_ANIM_OFF);
    ui_apply_progress_bar_style(s_humidity_bar);

    make_divider(panel, 20, 145, 345);

    lv_obj_t *air_title = make_text(
        panel,
        "AIR TEMP",
        UI_FONT_BODY,
        UI_TEXT);

    lv_obj_set_pos(air_title, 22, 161);

    page->air_label = make_text(
        panel,
        "--.- C",
        UI_FONT_VALUE_SMALL,
        UI_TEXT_BRIGHT);

    lv_obj_set_width(page->air_label, 145);
    lv_obj_set_style_text_align(
        page->air_label,
        LV_TEXT_ALIGN_RIGHT,
        0);
    lv_obj_set_pos(page->air_label, 215, 157);

    make_divider(panel, 20, 194, 345);

    lv_obj_t *center_title = make_text(
        panel,
        "CENTER TEMP",
        UI_FONT_BODY,
        UI_TEXT);

    lv_obj_set_pos(center_title, 22, 201);

    page->center_label = make_text(
        panel,
        "--.- C",
        UI_FONT_VALUE_SMALL,
        UI_TEXT_BRIGHT);

    lv_obj_set_width(page->center_label, 145);
    lv_obj_set_style_text_align(
        page->center_label,
        LV_TEXT_ALIGN_RIGHT,
        0);
    lv_obj_set_pos(page->center_label, 215, 197);
}

static void build_system_panel(
    ui_drybox_page_t *page)
{
    const ui_dashboard_rect_t *rect =
        &ui_page_layout_profile_current()->drybox.drying_system;

    lv_obj_t *panel =
        make_panel(page->panel,
                   rect->x,
                   rect->y,
                   rect->width,
                   rect->height);

    ui_create_operator_card_heading(
        panel,
        "DRYING SYSTEM",
        22,
        18);

    page->target_label = make_text(
        panel,
        "-- C",
        UI_FONT_PERCENT,
        UI_WARN);

    lv_obj_set_width(page->target_label, 250);
    lv_obj_set_style_text_align(
        page->target_label,
        LV_TEXT_ALIGN_CENTER,
        0);
    lv_obj_set_pos(page->target_label, 72, 39);

    lv_obj_t *target_caption = make_text(
        panel,
        "TARGET TEMPERATURE",
        UI_FONT_CAPTION,
        UI_TEXT_DIM);

    lv_obj_set_width(target_caption, 250);
    lv_obj_set_style_text_align(
        target_caption,
        LV_TEXT_ALIGN_CENTER,
        0);
    lv_obj_set_pos(target_caption, 72, 97);

    make_divider(panel, 20, 137, 355);

    s_heater_dot = make_indicator_dot(
        panel,
        24,
        157,
        UI_TEXT_DIM);

    lv_obj_t *heater_title = make_text(
        panel,
        "HEATER",
        UI_FONT_BODY_LARGE,
        UI_TEXT);

    lv_obj_set_pos(heater_title, 56, 153);

    page->heater_label = make_text(
        panel,
        "OFF",
        UI_FONT_VALUE_SMALL,
        UI_TEXT_DIM);

    lv_obj_set_width(page->heater_label, 110);
    lv_obj_set_style_text_align(
        page->heater_label,
        LV_TEXT_ALIGN_RIGHT,
        0);
    lv_obj_set_pos(page->heater_label, 255, 151);

    s_heater_activity_bar = lv_bar_create(panel);
    lv_obj_set_size(s_heater_activity_bar, 110, 6);
    lv_obj_set_pos(s_heater_activity_bar, 255, 179);
    lv_bar_set_range(s_heater_activity_bar, 0, 100);
    lv_bar_set_value(
        s_heater_activity_bar,
        0,
        LV_ANIM_OFF);
    ui_apply_progress_bar_style(s_heater_activity_bar);

    lv_obj_set_style_bg_color(
        s_heater_activity_bar,
        UI_OK_BRIGHT,
        LV_PART_INDICATOR);

    make_divider(panel, 20, 190, 355);

    s_fan_dot = make_indicator_dot(
        panel,
        24,
        201,
        UI_TEXT_DIM);

    lv_obj_t *fan_title = make_text(
        panel,
        "FAN",
        UI_FONT_BODY_LARGE,
        UI_TEXT);

    lv_obj_set_pos(fan_title, 56, 197);

    page->fan_label = make_text(
        panel,
        "0 %",
        UI_FONT_VALUE_SMALL,
        UI_ACCENT_INFO);

    lv_obj_set_width(page->fan_label, 110);
    lv_obj_set_style_text_align(
        page->fan_label,
        LV_TEXT_ALIGN_RIGHT,
        0);
    lv_obj_set_pos(page->fan_label, 255, 195);
}

static void build_program_panel(
    ui_drybox_page_t *page)
{
    const ui_dashboard_rect_t *rect =
        &ui_page_layout_profile_current()->drybox.material_program;

    lv_obj_t *panel =
        make_panel(page->panel,
                   rect->x,
                   rect->y,
                   rect->width,
                   rect->height);

    lv_obj_t *heading = make_text(
        panel,
        "MATERIAL PROGRAM",
        UI_FONT_BODY_LARGE,
        UI_TEXT_DIM);

    lv_obj_set_pos(heading, 18, 14);

    s_program_status_label = make_text(
        panel,
        "ACTIVE",
        UI_FONT_BODY,
        UI_ACCENT_CYAN);

    lv_obj_set_width(
        s_program_status_label,
        150);

    lv_label_set_long_mode(
        s_program_status_label,
        LV_LABEL_LONG_CLIP);

    lv_obj_set_style_text_align(
        s_program_status_label,
        LV_TEXT_ALIGN_RIGHT,
        0);

    lv_obj_set_pos(
        s_program_status_label,
        602,
        15);

    s_active_dot = make_indicator_dot(
        panel,
        760,
        16,
        UI_ACCENT_CYAN);

    s_program_buttons[DRYBOX_PROGRAM_PLA] =
        make_program_button(
        panel,
        LV_SYMBOL_PLAY,
        "PLA",
        "DRY_PLA",
        18,
        140,
        UI_ACCENT_CYAN,
        UI_BG_DEEP);

    s_program_buttons[DRYBOX_PROGRAM_PETG] =
        make_program_button(
        panel,
        LV_SYMBOL_PLAY,
        "PETG",
        "DRY_PETG",
        174,
        140,
        UI_ACCENT_INFO,
        UI_BG_DEEP);

    s_program_buttons[DRYBOX_PROGRAM_HOLD] =
        make_program_button(
        panel,
        LV_SYMBOL_PAUSE,
        "HOLD",
        "DRY_HOLD",
        330,
        140,
        UI_WARN,
        UI_BG_DEEP);

    s_program_buttons[DRYBOX_PROGRAM_RESUME] =
        make_program_button(
        panel,
        LV_SYMBOL_PLAY,
        "RESUME",
        "DRY_RESUME",
        486,
        140,
        UI_OK_BRIGHT,
        UI_BG_DEEP);

    s_program_buttons[DRYBOX_PROGRAM_STOP] =
        make_program_button(
        panel,
        LV_SYMBOL_STOP,
        "STOP",
        "DRY_STOP",
        642,
        140,
        UI_DANGER_BRIGHT,
        UI_BG_DANGER_POPUP);
}

bool ui_drybox_page_create(
    ui_drybox_page_t *page,
    ui_drybox_page_action_cb_t action_cb,
    ui_drybox_page_banner_text_cb_t banner_text_cb)
{
    if (!page || !action_cb || !banner_text_cb) {
        return false;
    }

    if (page->panel) {
        lv_obj_move_foreground(page->panel);
        return true;
    }

    s_action_cb = action_cb;

    page->panel = lv_obj_create(lv_screen_active());

    if (!page->panel) {
        return false;
    }

    lv_obj_set_size(page->panel,
                    UI_PAGE_ROOT_WIDTH,
                    UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(page->panel,
                   UI_PAGE_ROOT_X,
                   UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(
        page->panel,
        LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_root_style(page->panel);

    const ui_drybox_layout_profile_t *layout =
        &ui_page_layout_profile_current()->drybox;

    ui_page_title_create(
        page->panel,
        "DRYBOX",
        layout->subtitle);

    /*
     * Drybox uses the shared Operator banner shell.
     * READY defaults to blue.
     */
    lv_obj_t *banner =
        ui_status_banner_create(
            page->panel,
            UI_STATUS_BAR_X,
            UI_STATUS_BAR_Y,
            UI_STATUS_BAR_WIDTH,
            UI_STATUS_BAR_HEIGHT);

    if (!banner) {
        return false;
    }

    ui_status_banner_set_simple(
        banner,
        drybox_status_text(banner_text_cb()),
        "FILAMENT CONDITIONING");

    page->banner_label =
        ui_status_banner_state_label(banner);

    build_environment_panel(page);
    build_system_panel(page);
    build_program_panel(page);

    return true;
}

void ui_drybox_page_refresh(
    const ui_drybox_page_t *page,
    const ui_drybox_page_state_t *state)
{
    if (!page || !page->panel || !state) {
        return;
    }

    char buffer[48];

    const char *banner =
        state->banner_text
            ? state->banner_text
            : "DRYBOX";

    bool online = !drybox_banner_is_offline(banner);

    drybox_program_t active_program =
        online
            ? drybox_program_from_state(
                  state->active_program)
            : DRYBOX_PROGRAM_NONE;

    if (page->banner_label) {
        lv_obj_t *banner_box =
            lv_obj_get_parent(page->banner_label);

        const char *display_text =
            online ? banner : "DRYBOX OFFLINE";

        /*
         * ERROR / OFFLINE       = red
         * ACTIVE / DRYING       = green
         * HEATING               = amber
         * READY                 = blue
         *
         * ACTIVE takes priority over heater_on so an active drying
         * program remains green while its heater is operating.
         */
        ui_status_kind_t banner_kind =
            UI_STATUS_INFO;

        if (!online) {
            banner_kind = UI_STATUS_DANGER;
        } else if (text_contains(banner, "ACTIVE") ||
                   text_contains(banner, "DRYING")) {
            banner_kind = UI_STATUS_OK;
        } else if (state->heater_on ||
                   text_contains(banner, "HEATING")) {
            banner_kind = UI_STATUS_WARNING;
        } else if (text_contains(banner, "READY")) {
            banner_kind = UI_STATUS_INFO;
        }

        if (banner_box) {
            ui_status_banner_set_simple(
                banner_box,
                drybox_status_text(display_text),
                "FILAMENT CONDITIONING");
        }

        if (banner_box) {
            ui_operator_banner_set_status(
                banner_box,
                banner_kind);
        }
    }

    set_program_button_state(
        active_program,
        online);

    if (!online) {
        if (page->humidity_label) {
            lv_label_set_text(
                page->humidity_label,
                "--.-");

            set_label_color(
                page->humidity_label,
                UI_TEXT_DIM);
        }

        if (s_humidity_condition_label) {
            lv_label_set_text(
                s_humidity_condition_label,
                "NO DATA");

            set_label_color(
                s_humidity_condition_label,
                UI_TEXT_DIM);
        }

        if (s_humidity_bar) {
            lv_bar_set_value(
                s_humidity_bar,
                0,
                LV_ANIM_OFF);
        }

        if (page->air_label) {
            lv_label_set_text(page->air_label, "--.- C");
        }

        if (page->center_label) {
            lv_label_set_text(page->center_label, "--.- C");
        }

        if (page->target_label) {
            lv_label_set_text(page->target_label, "-- C");
            set_label_color(page->target_label, UI_TEXT_DIM);
        }

        if (page->heater_label) {
            lv_label_set_text(page->heater_label, "OFFLINE");
            set_label_color(page->heater_label, UI_TEXT_DIM);
        }

        if (page->fan_label) {
            lv_label_set_text(page->fan_label, "-- %");
            set_label_color(page->fan_label, UI_TEXT_DIM);
        }

        if (s_heater_dot) {
            lv_obj_set_style_bg_color(
                s_heater_dot,
                UI_TEXT_DIM,
                0);
        }

        if (s_fan_dot) {
            lv_obj_set_style_bg_color(
                s_fan_dot,
                UI_TEXT_DIM,
                0);
        }

        if (s_heater_activity_bar) {
            lv_bar_set_value(
                s_heater_activity_bar,
                0,
                LV_ANIM_OFF);
        }

        return;
    }

    if (page->humidity_label) {
        snprintf(
            buffer,
            sizeof(buffer),
            "%.1f",
            state->humidity);

        lv_label_set_text(
            page->humidity_label,
            buffer);
    }

    set_humidity_presentation(state->humidity);

    if (page->air_label) {
        snprintf(
            buffer,
            sizeof(buffer),
            "%.1f C",
            state->air_temp);

        lv_label_set_text(
            page->air_label,
            buffer);

        set_label_color(
            page->air_label,
            UI_TEXT_BRIGHT);
    }

    if (page->center_label) {
        snprintf(
            buffer,
            sizeof(buffer),
            "%.1f C",
            state->center_temp);

        lv_label_set_text(
            page->center_label,
            buffer);

        set_label_color(
            page->center_label,
            UI_TEXT_BRIGHT);
    }

    if (page->target_label) {
        snprintf(
            buffer,
            sizeof(buffer),
            "%.0f C",
            state->heater_target);

        lv_label_set_text(
            page->target_label,
            buffer);

        set_label_color(
            page->target_label,
            state->heater_target > 0.0f
                ? UI_WARN
                : UI_TEXT_DIM);
    }

    if (page->heater_label) {
        lv_label_set_text(
            page->heater_label,
            state->heater_on ? "HEATING" : "OFF");

        set_label_color(
            page->heater_label,
            state->heater_on
                ? UI_OK_BRIGHT
                : UI_TEXT_DIM);
    }

    if (s_heater_dot) {
        lv_obj_set_style_bg_color(
            s_heater_dot,
            state->heater_on
                ? UI_OK_BRIGHT
                : UI_TEXT_DIM,
            0);
    }

    if (s_heater_activity_bar) {
        lv_bar_set_value(
            s_heater_activity_bar,
            state->heater_on ? 100 : 0,
            LV_ANIM_OFF);
    }

    if (page->fan_label) {
        snprintf(
            buffer,
            sizeof(buffer),
            "%.0f %%",
            state->fan_speed);

        lv_label_set_text(
            page->fan_label,
            buffer);

        set_label_color(
            page->fan_label,
            state->fan_speed > 0.0f
                ? UI_ACCENT_INFO
                : UI_TEXT_DIM);
    }

    if (s_fan_dot) {
        lv_obj_set_style_bg_color(
            s_fan_dot,
            state->fan_speed > 0.0f
                ? UI_ACCENT_INFO
                : UI_TEXT_DIM,
            0);
    }
}

void ui_drybox_page_cleanup(
    ui_drybox_page_t *page)
{
    if (!page) {
        return;
    }

    if (page->panel) {
        lv_obj_delete(page->panel);
    }

    page->panel = NULL;
    page->banner_label = NULL;
    page->air_label = NULL;
    page->center_label = NULL;
    page->humidity_label = NULL;
    page->target_label = NULL;
    page->heater_label = NULL;
    page->fan_label = NULL;

    s_heater_dot = NULL;
    s_fan_dot = NULL;
    s_active_dot = NULL;
    s_program_status_label = NULL;
    s_humidity_condition_label = NULL;
    s_humidity_bar = NULL;
    s_heater_activity_bar = NULL;

    for (int i = 0; i < DRYBOX_PROGRAM_COUNT; ++i) {
        s_program_buttons[i] = NULL;
    }

    s_action_cb = NULL;
}
