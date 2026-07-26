#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "custom_theme.h"
#include "ui_theme.h"

void theme_manager_init(void);

ui_theme_id_t theme_manager_active(void);
const char *theme_manager_active_label(void);

bool theme_manager_select(ui_theme_id_t theme);

size_t theme_manager_scan_custom_themes(void);
size_t theme_manager_custom_count(void);
const custom_theme_summary_t *
theme_manager_custom_summary(size_t index);
bool theme_manager_select_custom(size_t index);
bool theme_manager_select_custom_id(const char *id);
bool theme_manager_remove_custom(size_t index);
bool theme_manager_custom_active(void);

ui_accent_id_t theme_manager_accent(void);
const char *theme_manager_accent_label(void);
const char *theme_manager_accent_name(ui_accent_id_t accent);
bool theme_manager_select_accent(ui_accent_id_t accent);

ui_density_id_t theme_manager_density(void);
const char *theme_manager_density_label(void);
const char *theme_manager_density_name(ui_density_id_t density);
bool theme_manager_select_density(ui_density_id_t density);

ui_accessibility_t theme_manager_accessibility(void);
const char *theme_manager_accessibility_label(void);
bool theme_manager_set_accessibility(ui_accessibility_t options);
