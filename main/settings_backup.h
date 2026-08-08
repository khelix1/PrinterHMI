#pragma once

#include <stdbool.h>
#include <stddef.h>

#define SETTINGS_BACKUP_PATH \
    "/sdcard/PrinterHMI/config_backup.json"
#define SETTINGS_BACKUP_ENCRYPTED_PATH \
    "/sdcard/PrinterHMI/config_backup.phmb"

bool settings_backup_export(
    char *status,
    size_t status_size);

typedef enum {
    SETTINGS_BACKUP_ENCRYPTED_PREPARING = 0,
    SETTINGS_BACKUP_ENCRYPTED_ENCRYPTING,
    SETTINGS_BACKUP_ENCRYPTED_WRITING,
} settings_backup_encrypted_progress_phase_t;

typedef void (*settings_backup_encrypted_progress_cb_t)(
    settings_backup_encrypted_progress_phase_t phase,
    void *user_data);

/* Portable passphrase-encrypted backup. Includes configured API keys. */
bool settings_backup_export_encrypted(
    const char *passphrase,
    char *status,
    size_t status_size);

/* Validates and summarizes the SD backup without changing settings. */
bool settings_backup_preflight(
    char *status,
    size_t status_size);

bool settings_backup_restore(
    char *status,
    size_t status_size);

bool settings_backup_export_encrypted_with_progress(
    const char *passphrase,
    settings_backup_encrypted_progress_cb_t progress_cb,
    void *progress_user_data,
    char *status,
    size_t status_size);

bool settings_backup_encrypted_preflight(
    const char *passphrase,
    char *status,
    size_t status_size);

bool settings_backup_restore_encrypted(
    const char *passphrase,
    char *status,
    size_t status_size);

/* Removes only backup artifacts from the SD card, never live settings. */
bool settings_backup_remove_all(
    char *status,
    size_t status_size);
