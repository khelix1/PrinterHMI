#include "ui_telemetry_v32.h"
#include "ui_telemetry_components.h"
#include "ui_telemetry_charts.h"

#include <math.h>
#include <stdio.h>

#include "lvgl.h"

#include "telemetry_history.h"
#include "ui_theme.h"
#include "ui_page_geometry_v32.h"

static lv_obj_t *s_panel = NULL;

static lv_obj_t *s_nozzle_value = NULL;
static lv_obj_t *s_bed_value = NULL;
static lv_obj_t *s_air_value = NULL;
static lv_obj_t *s_humidity_value = NULL;


void ui_telemetry_v32_show(void)
{
    if (s_panel) {
        lv_obj_move_foreground(s_panel);
        ui_telemetry_charts_load_history();
        return;
    }

    telemetry_history_init();

    s_panel = lv_obj_create(lv_screen_active());

    lv_obj_set_size(s_panel,
                    UI_PAGE_ROOT_WIDTH,
                    UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(s_panel,
                   UI_PAGE_ROOT_X,
                   UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(s_panel, UI_SURFACE_TELEMETRY_ROOT);

    lv_obj_t *title = telemetry_make_label(
        s_panel,
        "TELEMETRY",
        &lv_font_montserrat_22,
        UI_TEXT_BRIGHT);

    lv_obj_set_pos(title, UI_PAGE_RAIL_X, 20);

    lv_obj_t *subtitle = telemetry_make_label(
        s_panel,
        "Live machine instrumentation",
        &lv_font_montserrat_14,
        UI_TEXT_DIM);

    lv_obj_set_pos(subtitle, UI_PAGE_RAIL_X, 52);

    lv_obj_t *live = telemetry_make_label(
        s_panel,
        "LIVE",
        &lv_font_montserrat_14,
        UI_OK_BRIGHT);

    lv_obj_align(live, LV_ALIGN_TOP_RIGHT, -26, 28);

    telemetry_create_metric_card(
        s_panel,
        UI_PAGE_RAIL_X,
        "NOZZLE",
        UI_ACCENT_CYAN,
        &s_nozzle_value);

    telemetry_create_metric_card(
        s_panel,
        UI_PAGE_RAIL_X + 204,
        "BED",
        UI_WARN,
        &s_bed_value);

    telemetry_create_metric_card(
        s_panel,
        UI_PAGE_RAIL_X + 408,
        "CHAMBER",
        UI_TELEMETRY_CHAMBER,
        &s_air_value);

    telemetry_create_metric_card(
        s_panel,
        UI_PAGE_RAIL_X + 612,
        "HUMIDITY",
        UI_TELEMETRY_HUMIDITY,
        &s_humidity_value);

    ui_telemetry_charts_create(s_panel);
}

void ui_telemetry_v32_hide(void)
{
    if (!s_panel) {
        return;
    }

    lv_obj_delete(s_panel);

    s_panel = NULL;

    s_nozzle_value = NULL;
    s_bed_value = NULL;
    s_air_value = NULL;
    s_humidity_value = NULL;

    ui_telemetry_charts_reset();
}

static void telemetry_set_temperature(
    lv_obj_t *label,
    double value)
{
    if (!label) {
        return;
    }

    char buf[32];

    if (isfinite(value) && value > -100.0) {
        snprintf(buf, sizeof(buf), "%.1f C", value);
    } else {
        snprintf(buf, sizeof(buf), "-- C");
    }

    lv_label_set_text(label, buf);
}

static void telemetry_set_humidity(
    lv_obj_t *label,
    double value)
{
    if (!label) {
        return;
    }

    char buf[32];

    if (isfinite(value) && value >= 0.0) {
        snprintf(buf, sizeof(buf), "%.1f %%RH", value);
    } else {
        snprintf(buf, sizeof(buf), "-- %%RH");
    }

    lv_label_set_text(label, buf);
}

void ui_telemetry_v32_refresh(
    const moonraker_state_t *state,
    int64_t now_us)
{
    bool sampled =
        telemetry_history_sample(state, now_us);

    if (!state || !s_panel) {
        return;
    }

    telemetry_set_temperature(
        s_nozzle_value,
        state->nozzle_temp);

    telemetry_set_temperature(
        s_bed_value,
        state->bed_temp);

    telemetry_set_temperature(
        s_air_value,
        state->air_temp);

    telemetry_set_humidity(
        s_humidity_value,
        state->humidity);

    if (!sampled) {
        return;
    }

    ui_telemetry_charts_push_sample(
        state->nozzle_temp,
        state->bed_temp,
        state->air_temp,
        state->humidity);
}
