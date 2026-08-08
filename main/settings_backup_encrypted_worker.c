#include "settings_backup_encrypted_worker.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "settings_backup.h"
#include "settings_backup_crypto.h"

#define WORKER_PASSPHRASE_MAX 96
#define WORKER_STATUS_MAX 256

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static settings_backup_encrypted_worker_state_t s_state =
    SETTINGS_BACKUP_ENCRYPTED_WORKER_IDLE;
static char s_passphrase[WORKER_PASSPHRASE_MAX + 1];
static char s_status[WORKER_STATUS_MAX];
static bool s_verify_only = false;

static void secure_clear(char *text, size_t text_size)
{
    volatile char *volatile_text = text;
    while (volatile_text && text_size-- > 0) {
        *volatile_text++ = '\0';
    }
}

static void set_state(
    settings_backup_encrypted_worker_state_t state,
    const char *status)
{
    portENTER_CRITICAL(&s_lock);
    s_state = state;
    strlcpy(s_status, status ? status : "", sizeof(s_status));
    portEXIT_CRITICAL(&s_lock);
}

static void progress_cb(
    settings_backup_encrypted_progress_phase_t phase,
    void *user_data)
{
    (void)user_data;
    switch (phase) {
    case SETTINGS_BACKUP_ENCRYPTED_PREPARING:
        set_state(SETTINGS_BACKUP_ENCRYPTED_WORKER_PREPARING,
            "Preparing configuration backup...");
        break;
    case SETTINGS_BACKUP_ENCRYPTED_ENCRYPTING:
        set_state(SETTINGS_BACKUP_ENCRYPTED_WORKER_ENCRYPTING,
            "Encrypting configuration and protected API keys...");
        break;
    case SETTINGS_BACKUP_ENCRYPTED_WRITING:
        set_state(SETTINGS_BACKUP_ENCRYPTED_WORKER_WRITING,
            "Writing encrypted backup to the SD card...");
        break;
    }
}

static void encrypted_backup_task(void *argument)
{
    (void)argument;
    char passphrase[WORKER_PASSPHRASE_MAX + 1];
    char status[WORKER_STATUS_MAX];

    portENTER_CRITICAL(&s_lock);
    strlcpy(passphrase, s_passphrase, sizeof(passphrase));
    bool verify_only = s_verify_only;
    secure_clear(s_passphrase, sizeof(s_passphrase));
    portEXIT_CRITICAL(&s_lock);

    bool saved = false;
    if (verify_only) {
        set_state(SETTINGS_BACKUP_ENCRYPTED_WORKER_VERIFYING,
            "Verifying encrypted backup passphrase and contents...");
        saved = settings_backup_encrypted_preflight(
            passphrase,
            status,
            sizeof(status));
    } else {
        saved = settings_backup_export_encrypted_with_progress(
            passphrase,
            progress_cb,
            NULL,
            status,
            sizeof(status));
    }
    secure_clear(passphrase, sizeof(passphrase));
    set_state(
        saved ? SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED
              : SETTINGS_BACKUP_ENCRYPTED_WORKER_FAILED,
        status);
    vTaskDelete(NULL);
}

static bool settings_backup_encrypted_worker_start(
    bool verify_only,
    const char *passphrase,
    char *status,
    size_t status_size)
{
    if (!passphrase ||
        strnlen(passphrase, sizeof(s_passphrase)) <
            SETTINGS_BACKUP_CRYPTO_MIN_PASSPHRASE_LENGTH) {
        if (status && status_size) {
            strlcpy(status, "Use a passphrase of at least 12 characters.", status_size);
        }
        return false;
    }

    portENTER_CRITICAL(&s_lock);
    if (s_state == SETTINGS_BACKUP_ENCRYPTED_WORKER_PREPARING ||
        s_state == SETTINGS_BACKUP_ENCRYPTED_WORKER_ENCRYPTING ||
        s_state == SETTINGS_BACKUP_ENCRYPTED_WORKER_VERIFYING ||
        s_state == SETTINGS_BACKUP_ENCRYPTED_WORKER_WRITING) {
        portEXIT_CRITICAL(&s_lock);
        if (status && status_size) {
            strlcpy(status, "An encrypted backup is already in progress.", status_size);
        }
        return false;
    }
    strlcpy(s_passphrase, passphrase, sizeof(s_passphrase));
    s_status[0] = '\0';
    s_verify_only = verify_only;
    s_state = verify_only
        ? SETTINGS_BACKUP_ENCRYPTED_WORKER_VERIFYING
        : SETTINGS_BACKUP_ENCRYPTED_WORKER_PREPARING;
    portEXIT_CRITICAL(&s_lock);

    BaseType_t created = xTaskCreate(
        encrypted_backup_task,
        verify_only ? "backup_verify" : "backup_encrypt",
        8192,
        NULL,
        4,
        NULL);
    if (created != pdPASS) {
        secure_clear(s_passphrase, sizeof(s_passphrase));
        set_state(SETTINGS_BACKUP_ENCRYPTED_WORKER_FAILED,
            verify_only
                ? "Could not start encrypted backup verification."
                : "Could not start encrypted backup work.");
        if (status && status_size) {
            strlcpy(status, s_status, status_size);
        }
        return false;
    }
    if (status && status_size) {
        strlcpy(
            status,
            verify_only
                ? "Encrypted backup verification started."
                : "Encrypted backup started.",
            status_size);
    }
    return true;
}

bool settings_backup_encrypted_worker_start_export(
    const char *passphrase,
    char *status,
    size_t status_size)
{
    return settings_backup_encrypted_worker_start(
        false, passphrase, status, status_size);
}

bool settings_backup_encrypted_worker_start_verify(
    const char *passphrase,
    char *status,
    size_t status_size)
{
    return settings_backup_encrypted_worker_start(
        true, passphrase, status, status_size);
}

settings_backup_encrypted_worker_state_t
settings_backup_encrypted_worker_state(void)
{
    portENTER_CRITICAL(&s_lock);
    settings_backup_encrypted_worker_state_t state = s_state;
    portEXIT_CRITICAL(&s_lock);
    return state;
}

void settings_backup_encrypted_worker_status(char *status, size_t status_size)
{
    if (!status || status_size == 0) {
        return;
    }
    portENTER_CRITICAL(&s_lock);
    strlcpy(status, s_status, status_size);
    portEXIT_CRITICAL(&s_lock);
}
