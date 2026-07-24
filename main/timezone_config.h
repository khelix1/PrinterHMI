#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const char *id;
    const char *label;
    const char *abbreviation;
    const char *posix_tz;
} timezone_config_entry_t;

void timezone_config_init(void);

size_t timezone_config_count(void);
const timezone_config_entry_t *timezone_config_entry(size_t index);
size_t timezone_config_selected_index(void);
const char *timezone_config_selected_label(void);

bool timezone_config_select(size_t index);
