#pragma once

#include <stdbool.h>

typedef enum {
    UI_LANGUAGE_ENGLISH = 0,
    UI_LANGUAGE_SPANISH,
    UI_LANGUAGE_COUNT
} ui_language_id_t;

bool language_controller_init(void);
ui_language_id_t language_controller_active(void);
bool language_controller_select(ui_language_id_t language);
const char *language_controller_native_name(ui_language_id_t language);
