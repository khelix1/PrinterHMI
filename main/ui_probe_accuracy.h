#pragma once
#include <stdbool.h>
#include "lvgl.h"
typedef bool (*ui_probe_accuracy_send_cb_t)(const char *command);
typedef bool (*ui_probe_accuracy_ready_cb_t)(const char *workflow);
void ui_probe_accuracy_create(lv_obj_t *card, ui_probe_accuracy_send_cb_t send,
                             ui_probe_accuracy_ready_cb_t ready);
void ui_probe_accuracy_refresh(bool available);
void ui_probe_accuracy_hide(void);
