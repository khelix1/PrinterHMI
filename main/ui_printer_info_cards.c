#include "ui_printer_info_cards.h"
#include "printer_controller.h"
#include <string.h>
#include "ui_theme.h"
#include "ui_widgets.h"

#include <stdbool.h>
#include <stdio.h>

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
    return UI_ACCENT;
}

static void format_hhmm(char *out, size_t out_sz, double seconds)
{
    if (!out || out_sz == 0) return;

    if (seconds < 0.0) {
        snprintf(out, out_sz, "--:--");
        return;
    }

    int total_minutes = (int)((seconds / 60.0) + 0.5);
    int h = total_minutes / 60;
    int m = total_minutes % 60;
    snprintf(out, out_sz, "%02d:%02d", h, m);
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

    enum {
        CARD_X = 0,
        CARD_Y = 0,
        CARD_W = 123,
        CARD_H = 94,
        CARD_GAP = 12,
    };

    cards->progress = ui_create_operator_info_card(
        parent,
        "PROGRESS",
        "--%",
        CARD_X,
        CARD_Y,
        CARD_W,
        CARD_H);

    if (cards->progress) {
        /*
         * Progress is the Printer page's primary at-a-glance value.
         * Give it stronger typography than the other compact cards.
         */
        lv_obj_set_style_text_font(
            cards->progress,
            UI_FONT_HEADING,
            0);
        lv_obj_set_style_text_align(
            cards->progress,
            LV_TEXT_ALIGN_CENTER,
            0);
    }

    cards->nozzle = ui_create_operator_info_card(
        parent,
        "NOZZLE",
        "-- / -- C",
        CARD_X + (CARD_W + CARD_GAP),
        CARD_Y,
        CARD_W,
        CARD_H);

    cards->bed = ui_create_operator_info_card(
        parent,
        "BED",
        "-- / -- C",
        CARD_X + ((CARD_W + CARD_GAP) * 2),
        CARD_Y,
        CARD_W,
        CARD_H);

    cards->part_fan = ui_create_operator_info_card(
        parent,
        "PART FAN",
        "-- %",
        CARD_X + ((CARD_W + CARD_GAP) * 3),
        CARD_Y,
        CARD_W,
        CARD_H);

    cards->elapsed = ui_create_operator_info_card(
        parent,
        "ELAPSED",
        "--:--",
        CARD_X + ((CARD_W + CARD_GAP) * 4),
        CARD_Y,
        CARD_W,
        CARD_H);

    cards->remaining = ui_create_operator_info_card(
        parent,
        "REMAINING",
        "--:--",
        CARD_X + ((CARD_W + CARD_GAP) * 5),
        CARD_Y,
        CARD_W,
        CARD_H);

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

        lv_obj_add_flag(
            box,
            LV_OBJ_FLAG_CLICKABLE);

        lv_obj_add_event_cb(
            box,
            bed_cb,
            LV_EVENT_CLICKED,
            NULL);
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

    if (cards->progress) {
        char pbuf[32];
        if (printer_progress >= 0.0) snprintf(pbuf, sizeof(pbuf), "%.0f%%", printer_progress * 100.0);
        else snprintf(pbuf, sizeof(pbuf), "--%%");
        lv_label_set_text(cards->progress, pbuf);
        lv_obj_set_style_text_color(cards->progress, progress_value_color(printer_progress), 0);
    }

    if (cards->nozzle) {
        char nbuf[32];
        if (printer_nozzle_temp > -100.0) snprintf(nbuf, sizeof(nbuf), "%.1f / %.1f C", printer_nozzle_temp, printer_nozzle_target);
        else snprintf(nbuf, sizeof(nbuf), "-- / -- C");
        lv_label_set_text(cards->nozzle, nbuf);
        lv_obj_set_style_text_color(cards->nozzle, temp_value_color(printer_nozzle_temp, printer_nozzle_target), 0);
    }

    if (cards->bed) {
        char bbuf[32];
        if (printer_bed_temp > -100.0) snprintf(bbuf, sizeof(bbuf), "%.1f / %.1f C", printer_bed_temp, printer_bed_target);
        else snprintf(bbuf, sizeof(bbuf), "-- / -- C");
        lv_label_set_text(cards->bed, bbuf);
        lv_obj_set_style_text_color(cards->bed, temp_value_color(printer_bed_temp, printer_bed_target), 0);
    }

    if (cards->part_fan) {
        char pfbuf[24];
        if (printer_part_fan_speed >= 0.0) snprintf(pfbuf, sizeof(pfbuf), "%.0f %%", printer_part_fan_speed);
        else snprintf(pfbuf, sizeof(pfbuf), "-- %%");
        lv_label_set_text(cards->part_fan, pfbuf);
        lv_obj_set_style_text_color(cards->part_fan, fan_value_color(printer_part_fan_speed), 0);
    }

    if (cards->eta) {
        if (printer_eta_text && printer_eta_text[0]) {
            lv_label_set_text(cards->eta, printer_eta_text);
            lv_obj_set_style_text_color(cards->eta, UI_TEXT, 0);
        } else {
            lv_label_set_text(cards->eta, moonraker_ok ? LV_SYMBOL_OK " ONLINE" : LV_SYMBOL_CLOSE " OFFLINE");
            lv_obj_set_style_text_color(cards->eta, moonraker_ok ? UI_OK_BRIGHT : UI_DANGER_BRIGHT, 0);
        }
    }

    if (cards->elapsed) {
        char elapsed[32];
        format_hhmm(elapsed, sizeof(elapsed), printer_print_duration);
        lv_label_set_text(cards->elapsed, elapsed);
    }

    if (cards->remaining) {
        lv_label_set_text(cards->remaining, (printer_eta_text && printer_eta_text[0]) ? printer_eta_text : "--:--");
    }
}

static const char *card_icon_for_title(const char *title)
{
    if (!title) return LV_SYMBOL_OK;

    if (strstr(title, "WIFI") || strstr(title, "NETWORK")) return LV_SYMBOL_WIFI;
    if (strstr(title, "FILE") || strstr(title, "FILES")) return LV_SYMBOL_FILE;
    if (strstr(title, "TEMP") || strstr(title, "Temp") || strstr(title, "temp") ||
        strstr(title, "NOZZLE") || strstr(title, "Nozzle") || strstr(title, "nozzle") ||
        strstr(title, "BED") || strstr(title, "Bed") || strstr(title, "bed") ||
        strstr(title, "HEATER") || strstr(title, "Heater") || strstr(title, "heater")) return LV_SYMBOL_WARNING;
    if (strstr(title, "FAN")) return LV_SYMBOL_SETTINGS;
    if (strstr(title, "PROGRESS") || strstr(title, "REMAINING") || strstr(title, "ELAPSED") || strstr(title, "TIME")) return LV_SYMBOL_REFRESH;
    if (strstr(title, "MOONRAKER") || strstr(title, "STATE") || strstr(title, "STATUS") || strstr(title, "TARGET")) return LV_SYMBOL_OK;
    if (strstr(title, "DRY") || strstr(title, "HUMID") || strstr(title, "RH") || strstr(title, "CHAMBER")) return LV_SYMBOL_SETTINGS;
    if (strstr(title, "OTA") || strstr(title, "SYSTEM") || strstr(title, "SETTINGS")) return LV_SYMBOL_SETTINGS;

    return LV_SYMBOL_OK;
}

static lv_color_t card_color_for_title(const char *title)
{
    if (!title) return UI_ACCENT_INFO;

    if (strstr(title, "NOZZLE") || strstr(title, "Nozzle") || strstr(title, "nozzle") ||
        strstr(title, "HOTEND") || strstr(title, "Hotend") || strstr(title, "hotend") ||
        strstr(title, "HEATER") || strstr(title, "Heater") || strstr(title, "heater")) return UI_DANGER_BRIGHT;

    if (strstr(title, "BED") || strstr(title, "Bed") || strstr(title, "bed") ||
        strstr(title, "REMAINING") || strstr(title, "Remaining") || strstr(title, "remaining") ||
        strstr(title, "TARGET") || strstr(title, "Target") || strstr(title, "target")) return UI_WARN;
    if (strstr(title, "AIR") || strstr(title, "CENTER") || strstr(title, "CHAMBER") || strstr(title, "TEMP")) return UI_ACCENT_ORANGE;
    if (strstr(title, "HUMID") || strstr(title, "RH")) return UI_ACCENT_SKY;
    if (strstr(title, "FAN")) return UI_BORDER_BRIGHT;
    if (strstr(title, "PROGRESS")) return UI_ACCENT_PURPLE;
    if (strstr(title, "ELAPSED") || strstr(title, "TIME")) return UI_ACCENT_INFO;
    if (strstr(title, "WIFI") || strstr(title, "NETWORK") || strstr(title, "MOONRAKER") || strstr(title, "STATE") || strstr(title, "STATUS")) return UI_OK_BRIGHT;
    if (strstr(title, "FILE") || strstr(title, "FILES")) return UI_ACCENT_INFO;
    if (strstr(title, "DRY")) return UI_ACCENT_SKY;
    if (strstr(title, "OTA") || strstr(title, "SYSTEM") || strstr(title, "SETTINGS")) return UI_ACCENT_PURPLE;

    return UI_ACCENT_INFO;
}

void ui_printer_info_cards_add_vivid_icon(lv_obj_t *value_label, const char *title)
{
    if (!value_label || !title) return;

    lv_obj_t *box = lv_obj_get_parent(value_label);
    if (!box) return;

    lv_obj_t *ico = lv_label_create(box);
    lv_label_set_text(ico, card_icon_for_title(title));
    lv_obj_set_style_text_font(ico, UI_FONT_TITLE, 0);
    lv_obj_set_style_text_color(ico, card_color_for_title(title), 0);
    lv_obj_align(ico, LV_ALIGN_TOP_RIGHT, -10, 8);
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

    lv_label_set_text(
        value_label,
        "N/A");

    lv_obj_set_style_text_color(
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

    printer_controller_format_remaining(
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

        apply_optional_card_capability(
            cards->part_fan,
            capabilities->has_part_fan);
    } else {
        apply_card_online_state(cards->bed, true);
        apply_card_online_state(cards->part_fan, true);
    }
}
