#include "ui_text_fit.h"
#include "ui_studio_layout.h"
#include "ui_value_update.h"
#include "ui_printer_info_cards.h"
#include "ui_hotend_readings.h"
#include "ui_studio_temperature.h"
#include "ui_text.h"
#include "printer_controller.h"
#include <string.h>
#include "ui_theme.h"
#include "ui_widgets.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "ui_font_fallback.h"

typedef struct {
    lv_event_cb_t callback;
    uint32_t long_press_tick;
} bed_card_event_ctx_t;

static void bed_card_event_cb(lv_event_t *e)
{
    if (!e) {
        return;
    }

    bed_card_event_ctx_t *ctx =
        (bed_card_event_ctx_t *)lv_event_get_user_data(e);

    if (!ctx || !ctx->callback) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_LONG_PRESSED) {
        ctx->long_press_tick = lv_tick_get();
        ctx->callback(e);
        lv_event_stop_processing(e);
        return;
    }

    if (code == LV_EVENT_CLICKED) {
        uint32_t elapsed = lv_tick_get() - ctx->long_press_tick;

        if (ctx->long_press_tick != 0 && elapsed < 1500U) {
            ctx->long_press_tick = 0;
            lv_event_stop_processing(e);
            return;
        }

        ctx->long_press_tick = 0;
        ctx->callback(e);
    }
}

static lv_color_t temp_value_color(double temp, double target)
{
    if (temp < -100.0) return UI_TEXT_DIM;
    if (target > 1.0 && temp < target - 5.0) return UI_WARN;
    if (target > 1.0 && temp >= target - 5.0) return UI_OK_BRIGHT;
    return UI_TEXT;
}

static lv_color_t progress_value_color(double progress)
{
    if (progress < 0.0) return UI_TEXT_DIM;
    if (progress >= 0.999) return UI_OK_BRIGHT;

    /*
     * Keep the PROGRESS heading unchanged. This color is applied only
     * to the value label containing the percentage.
     */
    return UI_TEXT_BRIGHT;
}

static lv_color_t fan_value_color(double fan)
{
    if (fan < 0.0) return UI_TEXT_DIM;
    if (fan <= 0.5) return UI_TEXT_DIM;
    if (fan >= 99.5) return UI_OK_BRIGHT;
    return UI_ACCENT_BRIGHT;
}

void ui_printer_info_cards_create(
    lv_obj_t *parent,
    ui_printer_info_cards_t *cards,
    lv_event_cb_t nozzle_cb,
    lv_event_cb_t bed_cb,
    lv_event_cb_t part_fan_cb)
{
    if (!parent || !cards) {
        return;
    }

    /* A rebuilt page may use a different theme. Retire Studio-only fields
     * before constructing either layout, so refresh cannot follow old labels. */
    memset(cards,0,sizeof(*cards));
    if(ui_theme_is_studio()) {
        const char *names[]={"Nozzle","Bed"};
        lv_obj_t **values[]={&cards->nozzle,&cards->bed};
        cards->studio = true;
        lv_event_cb_t callbacks[]={nozzle_cb,bed_cb};
        for(int i=0;i<2;i++) {
            lv_obj_t *area=studio_plane(parent,0,i*142,336,i ? 106 : 136);
            lv_obj_t *title = studio_text(area,names[i],12,0,312,UI_FONT_BODY_LARGE,UI_TEXT_DIM);
            if (i == 0) {
                cards->nozzle_title = title;
                cards->tools_title = studio_text(area,"Tools  •  Tap to set",12,0,312,
                    UI_FONT_BODY_LARGE,UI_TEXT_DIM);
                ui_hotend_visible(cards->tools_title, false);
                cards->hotend_layout = ui_studio_hotend_layout(12, 32);
                ui_hotend_create_readings(area, cards->hotend_readings);
            } else {
                cards->bed_title = title;
            }
            *values[i]=ui_studio_temperature_reading(area,12,i ? 24 : 32,312);
            lv_obj_t *setpoint=ui_studio_temperature_setpoint(area,12,i ? 82 : 92,312);
            if (i == 0) cards->nozzle_setpoint = setpoint;
            else cards->bed_setpoint = setpoint;
            if(callbacks[i]) {lv_obj_add_flag(area,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(area,callbacks[i],LV_EVENT_CLICKED,NULL);}
            studio_rule(area,12,i ? 104 : 132,312,1);
        }
        lv_obj_t *fan_area=studio_plane(parent,12,268,94,34);
        cards->part_fan=studio_text(fan_area,"--%",0,0,94,UI_FONT_VALUE_SMALL,UI_TEXT);
        if(part_fan_cb) {lv_obj_add_flag(fan_area,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(fan_area,part_fan_cb,LV_EVENT_CLICKED,NULL);}
        studio_text(parent,"Fan",12,251,94,UI_FONT_CAPTION,UI_TEXT_DIM);
        cards->elapsed=studio_text(parent,"--:--",124,268,94,UI_FONT_VALUE_SMALL,UI_TEXT);
        studio_text(parent,"Elapsed",124,251,94,UI_FONT_CAPTION,UI_TEXT_DIM);
        cards->remaining=studio_text(parent,"--:--",236,268,100,UI_FONT_VALUE_SMALL,UI_TEXT);
        studio_text(parent,"Remaining",236,251,100,UI_FONT_CAPTION,UI_TEXT_DIM);
        studio_rule(parent,12,304,312,1);
        studio_text(parent,"Progress",12,308,180,UI_FONT_BODY,UI_TEXT_DIM);
        cards->progress=studio_text(parent,"--%",204,306,116,UI_FONT_HEADING,UI_TEXT_BRIGHT);
        lv_label_set_long_mode(cards->progress, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(cards->progress, LV_TEXT_ALIGN_RIGHT, 0);
        cards->progress_bar = lv_bar_create(parent);
        lv_obj_set_pos(cards->progress_bar, 12, 333);
        lv_obj_set_size(cards->progress_bar, 180, 8);
        ui_apply_progress_bar_style(cards->progress_bar);
        return;
    }
    lv_obj_update_layout(parent);
    int parent_width = lv_obj_get_width(parent);
    int parent_height = lv_obj_get_height(parent);
    int card_width = 123;
    int card_height = 94;
    int card_gap = 12;
    int card_x[6] = {0, 135, 270, 405, 540, 675};
    int card_y[6] = {0, 0, 0, 0, 0, 0};

    /* Operator Shell dedicates a narrow right-hand telemetry column. */
    if (ui_theme_is_operator_shell() && parent_width >= 240 &&
        parent_width <= 320 && parent_height >= 200) {
        card_gap = 10;
        card_width = (parent_width - card_gap) / 2;
        card_height = (parent_height - (card_gap * 2)) / 3;
        for (int index = 0; index < 6; ++index) {
            card_x[index] = (index % 2) * (card_width + card_gap);
            card_y[index] = (index / 2) * (card_height + card_gap);
        }
    }

    cards->progress = ui_create_operator_info_card(
        parent,
        "Progress",
        "--%",
        card_x[0],
        card_y[0],
        card_width,
        card_height);

    if (cards->progress) {
        /*
         * Progress is the Printer page's primary at-a-glance value.
         * Give it stronger typography than the other compact cards.
         */
        lv_obj_set_style_text_font(cards->progress, ui_font_with_fallback(UI_FONT_HEADING), 0);
        lv_obj_t *box = lv_obj_get_parent(cards->progress);
        lv_obj_align(cards->progress, LV_ALIGN_CENTER, 0, 7);
        cards->progress_bar = lv_bar_create(box);
        lv_obj_set_size(cards->progress_bar, card_width - 24, 6);
        lv_obj_align(cards->progress_bar, LV_ALIGN_BOTTOM_MID, 0, -7);
        ui_apply_progress_bar_style(cards->progress_bar);
        lv_obj_set_style_text_align(
            cards->progress,
            LV_TEXT_ALIGN_CENTER,
            0);
    }

    cards->nozzle = ui_create_operator_info_card(
        parent,
        "Nozzle",
        UI_TEMPERATURE_PAIR_UNKNOWN,
        card_x[1],
        card_y[1],
        card_width,
        card_height);

    if (cards->nozzle) {
        lv_obj_t *box = lv_obj_get_parent(cards->nozzle);
        cards->nozzle_title = lv_obj_get_child(box, 0);
        cards->tools_title = lv_label_create(box);
        lv_label_set_text(cards->tools_title, ui_text("Tools"));
        ui_apply_custom_label_style(cards->tools_title, &lv_font_montserrat_14, UI_TEXT_DIM);
        lv_obj_set_style_text_font(cards->tools_title, ui_font_with_fallback(&lv_font_montserrat_14), 0);
        lv_obj_align(cards->tools_title, LV_ALIGN_TOP_MID, 0, 3);
        ui_hotend_visible(cards->tools_title, false);
        cards->hotend_layout = (ui_hotend_layout_t){7, 21, card_width - 14,
            card_height - 26, 1, 1, &lv_font_montserrat_22, &lv_font_montserrat_16};
        ui_hotend_create_readings(box, cards->hotend_readings);
    }

    cards->bed = ui_create_operator_info_card(
        parent,
        "Bed",
        UI_TEMPERATURE_PAIR_UNKNOWN,
        card_x[2],
        card_y[2],
        card_width,
        card_height);

    lv_obj_t *thermal[] = {cards->nozzle, cards->bed};
    for (size_t i = 0; i < sizeof(thermal) / sizeof(thermal[0]); ++i) {
        if (!thermal[i]) continue;
        lv_label_set_text(thermal[i], "888.8/888.8°");
        ui_text_fit_single_line(thermal[i], UI_FONT_VALUE_SMALL);
        lv_label_set_text(thermal[i], UI_TEMPERATURE_PAIR_UNKNOWN);
    }

    cards->part_fan = ui_create_operator_info_card(
        parent,
        "Part fan",
        "-- %",
        card_x[3],
        card_y[3],
        card_width,
        card_height);

    cards->elapsed = ui_create_operator_info_card(
        parent,
        "Elapsed",
        "--:--",
        card_x[4],
        card_y[4],
        card_width,
        card_height);

    cards->remaining = ui_create_operator_info_card(
        parent,
        "Remaining",
        "--:--",
        card_x[5],
        card_y[5],
        card_width,
        card_height);

    cards->eta = NULL;

    if (cards->nozzle && nozzle_cb) {
        lv_obj_t *box =
            lv_obj_get_parent(cards->nozzle);

        lv_obj_add_flag(
            box,
            LV_OBJ_FLAG_CLICKABLE);

        lv_obj_add_event_cb(
            box,
            nozzle_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    if (cards->bed && bed_cb) {
        lv_obj_t *box =
            lv_obj_get_parent(cards->bed);

        bed_card_event_ctx_t *ctx =
            calloc(1, sizeof(*ctx));

        if (ctx) {
            ctx->callback = bed_cb;

            lv_obj_add_flag(
                box,
                LV_OBJ_FLAG_CLICKABLE);

            lv_obj_add_event_cb(
                box,
                bed_card_event_cb,
                LV_EVENT_CLICKED,
                ctx);
            lv_obj_add_event_cb(
                box,
                bed_card_event_cb,
                LV_EVENT_LONG_PRESSED,
                ctx);
        } else {
            /*
             * Safe fallback: preserve normal BED-card behavior if the
             * small event context cannot be allocated.
             */
            lv_obj_add_flag(
                box,
                LV_OBJ_FLAG_CLICKABLE);

            lv_obj_add_event_cb(
                box,
                bed_cb,
                LV_EVENT_CLICKED,
                NULL);
            lv_obj_add_event_cb(
                box,
                bed_cb,
                LV_EVENT_LONG_PRESSED,
                NULL);
        }
    }

    if (cards->part_fan && part_fan_cb) {
        lv_obj_t *box =
            lv_obj_get_parent(cards->part_fan);

        lv_obj_add_flag(
            box,
            LV_OBJ_FLAG_CLICKABLE);

        lv_obj_add_event_cb(
            box,
            part_fan_cb,
            LV_EVENT_CLICKED,
            NULL);
    }
}


void ui_printer_info_cards_refresh(lv_obj_t *printer_panel,
                                   const ui_printer_info_cards_t *cards,
                                   double printer_progress,
                                   double printer_nozzle_temp,
                                   double printer_nozzle_target,
                                   double printer_bed_temp,
                                   double printer_bed_target,
                                   double printer_part_fan_speed,
                                   double printer_print_duration,
                                   const char *printer_eta_text,
                                   bool moonraker_ok)
{
    if (!printer_panel || !cards) return;

    if (!moonraker_ok) {
        printer_progress = -1.0;
        printer_nozzle_temp = printer_nozzle_target = -999.0;
        printer_bed_temp = printer_bed_target = -999.0;
        printer_part_fan_speed = printer_print_duration = -1.0;
        printer_eta_text = NULL;
    }

    int percent = printer_controller_progress_percent(printer_progress);
    if (cards->progress_bar)
        lv_bar_set_value(cards->progress_bar, percent < 0 ? 0 : percent, LV_ANIM_OFF);
    char pair[20];
    ui_format_temperature_pair(pair, sizeof(pair), printer_nozzle_temp, printer_nozzle_target);
    if (cards->studio)
        ui_studio_set_temperature(cards->nozzle, cards->nozzle_setpoint, pair);
    else
        ui_value_set_text(cards->nozzle, pair);
    ui_format_temperature_pair(pair, sizeof(pair), printer_bed_temp, printer_bed_target);
    if (cards->studio) {
        ui_studio_set_temperature(cards->bed, cards->bed_setpoint, pair);
        ui_value_set_text(cards->nozzle_title, moonraker_ok ? "Nozzle  •  Tap to set" : "Nozzle");
        ui_value_set_text(cards->bed_title, moonraker_ok ? "Bed  •  Tap to set" : "Bed");
        ui_value_set_color(cards->nozzle, ui_temperature_current_valid(printer_nozzle_temp) ? UI_TEXT : UI_TEXT_DIM, 0);
        ui_value_set_color(cards->bed, ui_temperature_current_valid(printer_bed_temp) ? UI_TEXT : UI_TEXT_DIM, 0);
        char t[64];
        snprintf(t,sizeof(t),moonraker_ok ? "%.0f%%" : "--%%",printer_part_fan_speed);ui_value_set_text(cards->part_fan,t);
        if (percent >= 0) snprintf(t, sizeof(t), "%d%%", percent);
        else snprintf(t, sizeof(t), "--%%");
        ui_value_set_text(cards->progress,t);
        ui_value_set_color(cards->progress, progress_value_color(percent < 0 ? -1.0 : printer_progress), 0);
        printer_controller_format_hhmm(t,sizeof(t),moonraker_ok?printer_print_duration:-1);ui_value_set_text(cards->elapsed,t);
        ui_value_set_text(cards->remaining,printer_eta_text&&printer_eta_text[0]?printer_eta_text:"--:--");
        return;
    }

    ui_value_set_text(cards->bed, pair);
    if (cards->progress) {
    char pbuf[32];
        if (percent >= 0) snprintf(pbuf, sizeof(pbuf), "%d%%", percent);
        else snprintf(pbuf, sizeof(pbuf), "--%%");
        ui_value_set_text(cards->progress, pbuf);
        ui_value_set_color(cards->progress, progress_value_color(percent < 0 ? -1.0 : printer_progress), 0);
    }

    if (cards->nozzle)
        ui_value_set_color(cards->nozzle, temp_value_color(printer_nozzle_temp, printer_nozzle_target), 0);
    if (cards->bed)
        ui_value_set_color(cards->bed, temp_value_color(printer_bed_temp, printer_bed_target), 0);

    if (cards->part_fan) {
        char pfbuf[24];
        if (printer_part_fan_speed >= 0.0) snprintf(pfbuf, sizeof(pfbuf), "%.0f %%", printer_part_fan_speed);
        else snprintf(pfbuf, sizeof(pfbuf), "-- %%");
        ui_value_set_text(cards->part_fan, pfbuf);
        ui_value_set_color(cards->part_fan, fan_value_color(printer_part_fan_speed), 0);
    }

    if (cards->eta) {
        if (printer_eta_text && printer_eta_text[0]) {
            ui_value_set_text(cards->eta, printer_eta_text);
            ui_value_set_color(cards->eta, UI_TEXT, 0);
        } else {
            ui_value_set_text(cards->eta, moonraker_ok ? ui_text(LV_SYMBOL_OK " ONLINE") : ui_text(LV_SYMBOL_CLOSE " OFFLINE"));
            ui_value_set_color(cards->eta, moonraker_ok ? UI_OK_BRIGHT : UI_DANGER_BRIGHT, 0);
        }
    }

    if (cards->elapsed) {
        char elapsed[32];
        printer_controller_format_hhmm(elapsed, sizeof(elapsed), printer_print_duration);
        ui_value_set_text(cards->elapsed, elapsed);
    }

    if (cards->remaining) {
        ui_value_set_text(cards->remaining, (printer_eta_text && printer_eta_text[0]) ? printer_eta_text : ui_text("--:--"));
    }
    lv_obj_t *values[] = { cards->progress, cards->part_fan, cards->eta, cards->elapsed, cards->remaining };
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        if (values[i]) ui_text_fit_single_line(values[i],
            i == 0 ? UI_FONT_HEADING : UI_FONT_VALUE_SMALL);
}

static void apply_card_online_state(
    lv_obj_t *value_label,
    bool online)
{
    if (!value_label) {
        return;
    }

    lv_obj_t *card =
        lv_obj_get_parent(value_label);

    if (!card) {
        return;
    }

    if (online) {
        lv_obj_remove_state(
            card,
            LV_STATE_DISABLED);

        lv_obj_add_flag(
            card,
            LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_add_state(
            card,
            LV_STATE_DISABLED);

        lv_obj_clear_flag(
            card,
            LV_OBJ_FLAG_CLICKABLE);
    }
}


static void apply_optional_card_capability(
    lv_obj_t *value_label,
    bool available)
{
    if (!value_label) {
        return;
    }

    lv_obj_t *card =
        lv_obj_get_parent(value_label);

    if (!card) {
        return;
    }

    if (available) {
        lv_obj_remove_state(
            card,
            LV_STATE_DISABLED);

        lv_obj_add_flag(
            card,
            LV_OBJ_FLAG_CLICKABLE);
        return;
    }

    ui_value_set_text(
        value_label,
        ui_text("N/A"));

    ui_value_set_color(
        value_label,
        UI_TEXT_DIM,
        0);

    lv_obj_add_state(
        card,
        LV_STATE_DISABLED);

    lv_obj_clear_flag(
        card,
        LV_OBJ_FLAG_CLICKABLE);
}


void ui_printer_info_cards_refresh_live(
    lv_obj_t *printer_panel,
    ui_printer_info_cards_t *cards,
    double progress,
    double nozzle_temp,
    double nozzle_target,
    double bed_temp,
    double bed_target,
    double part_fan_speed,
    double print_duration,
    const double *remaining_seconds,
    bool moonraker_ok,
    const moonraker_capabilities_t *capabilities)
{
    /*
     * Card pointers belong to the Printer page. Once that page is hidden,
     * its objects have been deleted and must not receive capability updates.
     */
    if (!printer_panel || !cards) {
        return;
    }

    char remaining_buf[32];

    if (remaining_seconds) printer_controller_format_hhmm(remaining_buf, sizeof(remaining_buf), *remaining_seconds);
    else printer_controller_format_remaining(
        remaining_buf,
        sizeof(remaining_buf),
        progress,
        print_duration);

    ui_printer_info_cards_refresh(
        printer_panel,
        cards,
        progress,
        nozzle_temp,
        nozzle_target,
        bed_temp,
        bed_target,
        part_fan_speed,
        print_duration,
        remaining_buf,
        moonraker_ok);

    apply_card_online_state(
        cards->nozzle,
        moonraker_ok);

    if (!moonraker_ok) {
        apply_card_online_state(cards->bed, false);
        apply_card_online_state(cards->part_fan, false);
        return;
    }

    if (capabilities &&
        capabilities->discovered) {
        apply_optional_card_capability(
            cards->bed,
            capabilities->has_heated_bed);
        if (!capabilities->has_heated_bed && cards->bed_title) {
            ui_value_set_text(cards->bed_title, "Bed");
            ui_value_set_text(cards->bed_setpoint, "");
        }

        apply_optional_card_capability(
            cards->part_fan,
            capabilities->has_part_fan);
    } else {
        apply_card_online_state(cards->bed, true);
        apply_card_online_state(cards->part_fan, true);
    }
}

void ui_printer_info_cards_set_hotends(ui_printer_info_cards_t *cards,
    const moonraker_hotend_t *hotends, size_t count, bool online)
{
    if (!cards || !cards->nozzle) return;
    bool multi = hotends && count > 1;
    ui_hotend_visible(cards->nozzle_title, !multi);
    ui_hotend_visible(cards->nozzle, !multi);
    ui_hotend_visible(cards->nozzle_setpoint, !multi);
    ui_hotend_visible(cards->tools_title, multi);
    size_t layout_count = multi ? LV_MIN(count, MOONRAKER_MAX_HOTENDS) : 0;
    if (cards->hotend_layout_count != layout_count) {
        cards->hotend_layout_count = layout_count;
        ui_hotend_layout_readings(cards->hotend_readings, &cards->hotend_layout, layout_count);
    }
    ui_hotend_update_readings(cards->hotend_readings, hotends, count, online);
}
