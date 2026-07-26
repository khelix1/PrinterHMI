#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui_dashboard_layout_profile.h"
#include "ui_page_layout_profile.h"
#include "ui_theme.h"

#define CUSTOM_THEME_MAX_COUNT 8
#define CUSTOM_THEME_ID_MAX 32
#define CUSTOM_THEME_NAME_MAX 40
#define CUSTOM_THEME_AUTHOR_MAX 40
#define CUSTOM_THEME_DESCRIPTION_MAX 120

typedef struct {
    char id[CUSTOM_THEME_ID_MAX + 1];
    char name[CUSTOM_THEME_NAME_MAX + 1];
    char author[CUSTOM_THEME_AUTHOR_MAX + 1];
    char description[CUSTOM_THEME_DESCRIPTION_MAX + 1];
    ui_theme_id_t base_theme;
    uint32_t preview_background;
    uint32_t preview_card;
    uint32_t preview_accent;
    uint32_t preview_text;
} custom_theme_summary_t;

size_t custom_theme_scan_sd(void);
size_t custom_theme_count(void);
const custom_theme_summary_t *custom_theme_summary(size_t index);

bool custom_theme_activate(size_t index);
bool custom_theme_activate_id(const char *id);
void custom_theme_deactivate(void);
bool custom_theme_is_active(void);
const char *custom_theme_active_id(void);
const char *custom_theme_active_name(void);
ui_theme_id_t custom_theme_base(void);

bool custom_theme_remove(size_t index);

bool custom_theme_color_override(
    uint32_t operator_rgb,
    uint32_t foundry_rgb,
    uint32_t glass_rgb,
    uint32_t *rgb_out);

bool custom_theme_accent_override(
    uint8_t tone,
    uint32_t *rgb_out);

bool custom_theme_metric_override(
    int32_t operator_value,
    int32_t foundry_value,
    int32_t glass_value,
    int32_t *value_out);

bool custom_theme_surface_opacity(uint8_t *opacity_out);

const ui_dashboard_layout_profile_t *
custom_theme_dashboard_profile(void);

const ui_page_layout_profile_t *
custom_theme_page_profile(void);
