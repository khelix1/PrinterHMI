#pragma once

#include "lvgl.h"

typedef struct {
    lv_obj_t *progress;
    lv_obj_t *nozzle;
    lv_obj_t *bed;
    lv_obj_t *part_fan;
    lv_obj_t *elapsed;
    lv_obj_t *remaining;
    lv_obj_t *eta;
} ui_printer_info_cards_t;

void ui_printer_info_cards_create(lv_obj_t *parent,
                                  ui_printer_info_cards_t *cards,
                                  lv_event_cb_t nozzle_cb,
                                  lv_event_cb_t bed_cb,
                                  lv_event_cb_t part_fan_cb);

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
                                   bool moonraker_ok);

void ui_printer_info_cards_add_vivid_icon(lv_obj_t *value_label,
                                             const char *title);

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
    bool moonraker_ok);
