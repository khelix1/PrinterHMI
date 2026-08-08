#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SETTINGS_BACKUP_ENCRYPTED_WORKER_IDLE = 0,
    SETTINGS_BACKUP_ENCRYPTED_WORKER_PREPARING,
    SETTINGS_BACKUP_ENCRYPTED_WORKER_ENCRYPTING,
    SETTINGS_BACKUP_ENCRYPTED_WORKER_VERIFYING,
    SETTINGS_BACKUP_ENCRYPTED_WORKER_WRITING,
    SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED,
    SETTINGS_BACKUP_ENCRYPTED_WORKER_FAILED,
} settings_backup_encrypted_worker_state_t;

bool settings_backup_encrypted_worker_start_export(
    const char *passphrase,
    char *status,
    size_t status_size);

bool settings_backup_encrypted_worker_start_verify(
    const char *passphrase,
    char *status,
    size_t status_size);

settings_backup_encrypted_worker_state_t
settings_backup_encrypted_worker_state(void);

void settings_backup_encrypted_worker_status(
    char *status,
    size_t status_size);
