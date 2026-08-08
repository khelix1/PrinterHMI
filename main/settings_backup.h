#pragma once

#include <stdbool.h>
#include <stddef.h>

#define SETTINGS_BACKUP_PATH \
    "/sdcard/PrinterHMI/config_backup.json"

bool settings_backup_export(
    char *status,
    size_t status_size);

/* Validates and summarizes the SD backup without changing settings. */
bool settings_backup_preflight(
    char *status,
    size_t status_size);

bool settings_backup_restore(
    char *status,
    size_t status_size);
