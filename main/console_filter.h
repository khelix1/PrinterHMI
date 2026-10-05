#pragma once
#include "console_controller.h"

typedef enum {
    CONSOLE_FILTER_ALL = 0,
    CONSOLE_FILTER_ALERTS,
    CONSOLE_FILTER_ERRORS,
    CONSOLE_FILTER_WARNINGS,
    CONSOLE_FILTER_COMMANDS,
    CONSOLE_FILTER_RESPONSES,
    CONSOLE_FILTER_SYSTEM,
    CONSOLE_FILTER_COUNT
} console_filter_kind_t;

/* Presentation only: never removes entries from the controller history. */
bool console_filter_matches(const console_entry_t *entry,
    console_filter_kind_t kind, const char *query, bool hide_temperatures);
