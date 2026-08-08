#include "settings_backup.h"
#include "settings_backup_crypto.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "moonraker_config_controller.h"
#include "operator_event_log.h"
#include "theme_manager.h"
#include "timezone_config.h"
#include "ui_settings.h"

#define SETTINGS_BACKUP_SCHEMA 1
#define SETTINGS_BACKUP_PRODUCT "PrinterHMI"
#define SETTINGS_BACKUP_DIRECTORY "/sdcard/PrinterHMI"
#define SETTINGS_BACKUP_TEMP_PATH \
    "/sdcard/PrinterHMI/config_backup.tmp"
#define SETTINGS_BACKUP_PREVIOUS_PATH \
    "/sdcard/PrinterHMI/config_backup.previous"
#define SETTINGS_BACKUP_MAX_BYTES (16 * 1024)
#define SETTINGS_BACKUP_ENCRYPTED_TEMP_PATH \
    "/sdcard/PrinterHMI/config_backup.phmb.tmp"
#define SETTINGS_BACKUP_ENCRYPTED_PREVIOUS_PATH \
    "/sdcard/PrinterHMI/config_backup.phmb.previous"
#define SETTINGS_BACKUP_ENCRYPTED_MAX_BYTES \
    (SETTINGS_BACKUP_MAX_BYTES + 128)


typedef struct {
    moonraker_profile_t profiles[
        MOONRAKER_CONFIG_MAX_PROFILES];
    int active_profile;

    ui_theme_id_t theme;
    char custom_theme_id[CUSTOM_THEME_ID_MAX + 1];
    ui_accent_id_t accent;
    ui_density_id_t density;
    ui_accessibility_t accessibility;

    size_t timezone_index;
    int brightness;
    uint8_t sleep_timeout;
} settings_backup_snapshot_t;


static void set_status(
    char *status,
    size_t status_size,
    const char *message)
{
    if (!status || status_size == 0) {
        return;
    }

    strlcpy(
        status,
        message ? message : "",
        status_size);
}


static bool valid_sleep_timeout(int value)
{
    return value == 0 ||
           value == 5 ||
           value == 15 ||
           value == 30;
}


static bool json_integer(
    const cJSON *object,
    const char *name,
    int minimum,
    int maximum,
    int *output)
{
    const cJSON *item =
        cJSON_GetObjectItemCaseSensitive(
            object,
            name);

    if (!cJSON_IsNumber(item)) {
        return false;
    }

    int value = item->valueint;

    if ((double)value != item->valuedouble ||
        value < minimum ||
        value > maximum) {
        return false;
    }

    if (output) {
        *output = value;
    }

    return true;
}


static bool json_boolean(
    const cJSON *object,
    const char *name,
    bool *output)
{
    const cJSON *item =
        cJSON_GetObjectItemCaseSensitive(
            object,
            name);

    if (!cJSON_IsBool(item)) {
        return false;
    }

    if (output) {
        *output = cJSON_IsTrue(item);
    }

    return true;
}


static bool json_text(
    const cJSON *object,
    const char *name,
    char *output,
    size_t output_size)
{
    const cJSON *item =
        cJSON_GetObjectItemCaseSensitive(
            object,
            name);

    if (!cJSON_IsString(item) ||
        !item->valuestring ||
        !item->valuestring[0] ||
        strlen(item->valuestring) >= output_size) {
        return false;
    }

    strlcpy(
        output,
        item->valuestring,
        output_size);

    return true;
}


static void capture_current(
    settings_backup_snapshot_t *snapshot)
{
    if (!snapshot) {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));

    for (int index = 0;
         index < MOONRAKER_CONFIG_MAX_PROFILES;
         ++index) {
        const moonraker_profile_t *profile =
            moonraker_config_profile(index);

        if (profile) {
            snapshot->profiles[index] = *profile;
        }
    }

    snapshot->active_profile =
        moonraker_config_active_profile_index();

    snapshot->theme = theme_manager_active();
    if (theme_manager_custom_active()) {
        strlcpy(
            snapshot->custom_theme_id,
            custom_theme_active_id(),
            sizeof(snapshot->custom_theme_id));
    }
    snapshot->accent = theme_manager_accent();
    snapshot->density = theme_manager_density();
    snapshot->accessibility =
        theme_manager_accessibility();

    snapshot->timezone_index =
        timezone_config_selected_index();

    snapshot->brightness =
        ui_settings_brightness_percent();

    snapshot->sleep_timeout =
        ui_settings_sleep_timeout_minutes();
}


static cJSON *snapshot_to_json(
    const settings_backup_snapshot_t *snapshot,
    bool include_api_keys)
{
    if (!snapshot) {
        return NULL;
    }

    cJSON *root = cJSON_CreateObject();

    if (!root) {
        return NULL;
    }

    cJSON_AddStringToObject(
        root,
        "product",
        SETTINGS_BACKUP_PRODUCT);

    cJSON_AddNumberToObject(
        root,
        "schema",
        SETTINGS_BACKUP_SCHEMA);

    cJSON_AddNumberToObject(
        root,
        "created_unix",
        (double)time(NULL));

    cJSON_AddNumberToObject(
        root,
        "active_profile",
        snapshot->active_profile);

    cJSON *profiles =
        cJSON_AddArrayToObject(
            root,
            "profiles");

    for (int index = 0;
         profiles &&
         index < MOONRAKER_CONFIG_MAX_PROFILES;
         ++index) {
        const moonraker_profile_t *profile =
            &snapshot->profiles[index];

        cJSON *item =
            cJSON_CreateObject();

        if (!item) {
            cJSON_Delete(root);
            return NULL;
        }

        cJSON_AddNumberToObject(
            item,
            "slot",
            index);

        cJSON_AddBoolToObject(
            item,
            "configured",
            profile->configured);

        if (profile->configured) {
            cJSON_AddStringToObject(
                item,
                "name",
                profile->name);

            cJSON_AddStringToObject(
                item,
                "host",
                profile->host);

            cJSON_AddNumberToObject(
                item,
                "port",
                profile->port);

            if (include_api_keys) {
                cJSON_AddStringToObject(
                    item,
                    "api_key",
                    profile->api_key);
            }
        }

        cJSON_AddItemToArray(
            profiles,
            item);
    }

    cJSON *appearance =
        cJSON_AddObjectToObject(
            root,
            "appearance");

    cJSON_AddNumberToObject(
        appearance,
        "theme",
        snapshot->theme);

    cJSON_AddStringToObject(
        appearance,
        "custom_theme_id",
        snapshot->custom_theme_id);

    cJSON_AddNumberToObject(
        appearance,
        "accent",
        snapshot->accent);

    cJSON_AddNumberToObject(
        appearance,
        "density",
        snapshot->density);

    cJSON *accessibility =
        cJSON_AddObjectToObject(
            appearance,
            "accessibility");

    cJSON_AddBoolToObject(
        accessibility,
        "large_text",
        snapshot->accessibility.large_text);

    cJSON_AddBoolToObject(
        accessibility,
        "high_contrast",
        snapshot->accessibility.high_contrast);

    cJSON_AddBoolToObject(
        accessibility,
        "reduced_transparency",
        snapshot->accessibility.reduced_transparency);

    cJSON_AddBoolToObject(
        accessibility,
        "reduced_motion",
        snapshot->accessibility.reduced_motion);

    cJSON_AddNumberToObject(
        root,
        "timezone_index",
        (double)snapshot->timezone_index);

    cJSON *display =
        cJSON_AddObjectToObject(
            root,
            "display");

    cJSON_AddNumberToObject(
        display,
        "brightness",
        snapshot->brightness);

    cJSON_AddNumberToObject(
        display,
        "sleep_timeout_minutes",
        snapshot->sleep_timeout);

    return root;
}


static bool parse_snapshot(
    const cJSON *root,
    settings_backup_snapshot_t *snapshot,
    bool include_api_keys,
    char *status,
    size_t status_size)
{
    if (!cJSON_IsObject(root) || !snapshot) {
        set_status(
            status,
            status_size,
            "Backup document is not a JSON object.");
        return false;
    }

    const cJSON *product =
        cJSON_GetObjectItemCaseSensitive(
            root,
            "product");

    int schema = 0;

    if (!cJSON_IsString(product) ||
        !product->valuestring ||
        strcmp(
            product->valuestring,
            SETTINGS_BACKUP_PRODUCT) != 0 ||
        !json_integer(
            root,
            "schema",
            SETTINGS_BACKUP_SCHEMA,
            SETTINGS_BACKUP_SCHEMA,
            &schema)) {
        set_status(
            status,
            status_size,
            "Backup identity or schema is not supported.");
        return false;
    }

    memset(snapshot, 0, sizeof(*snapshot));

    if (!json_integer(
            root,
            "active_profile",
            0,
            MOONRAKER_CONFIG_MAX_PROFILES - 1,
            &snapshot->active_profile)) {
        set_status(
            status,
            status_size,
            "Backup active-printer slot is invalid.");
        return false;
    }

    const cJSON *profiles =
        cJSON_GetObjectItemCaseSensitive(
            root,
            "profiles");

    if (!cJSON_IsArray(profiles) ||
        cJSON_GetArraySize(profiles) !=
            MOONRAKER_CONFIG_MAX_PROFILES) {
        set_status(
            status,
            status_size,
            "Backup must contain all four printer slots.");
        return false;
    }

    bool slots_seen[
        MOONRAKER_CONFIG_MAX_PROFILES] = {0};

    int configured_count = 0;
    const cJSON *profile_json = NULL;

    cJSON_ArrayForEach(profile_json, profiles) {
        int slot = -1;
        bool configured = false;

        if (!cJSON_IsObject(profile_json) ||
            !json_integer(
                profile_json,
                "slot",
                0,
                MOONRAKER_CONFIG_MAX_PROFILES - 1,
                &slot) ||
            slots_seen[slot] ||
            !json_boolean(
                profile_json,
                "configured",
                &configured)) {
            set_status(
                status,
                status_size,
                "Backup contains an invalid printer slot.");
            return false;
        }

        slots_seen[slot] = true;

        moonraker_profile_t *profile =
            &snapshot->profiles[slot];

        profile->configured = configured;
        profile->port = 7125;

        if (!configured) {
            continue;
        }

        if (!json_text(
                profile_json,
                "name",
                profile->name,
                sizeof(profile->name)) ||
            !json_text(
                profile_json,
                "host",
                profile->host,
                sizeof(profile->host)) ||
            !json_integer(
                profile_json,
                "port",
                1,
                65535,
                &profile->port)) {
            set_status(
                status,
                status_size,
                "Backup contains invalid printer settings.");
            return false;
        }

        if (include_api_keys) {
            const cJSON *api_key =
                cJSON_GetObjectItemCaseSensitive(
                    profile_json,
                    "api_key");
            if (!cJSON_IsString(api_key) ||
                !api_key->valuestring ||
                strlen(api_key->valuestring) >=
                    sizeof(profile->api_key)) {
                set_status(
                    status,
                    status_size,
                    "Encrypted backup contains an invalid API key.");
                return false;
            }
            strlcpy(
                profile->api_key,
                api_key->valuestring,
                sizeof(profile->api_key));
        }

        ++configured_count;
    }

    if (configured_count == 0 ||
        !snapshot->profiles[
            snapshot->active_profile].configured) {
        set_status(
            status,
            status_size,
            "Backup has no valid active printer.");
        return false;
    }

    const cJSON *appearance =
        cJSON_GetObjectItemCaseSensitive(
            root,
            "appearance");

    const cJSON *accessibility =
        cJSON_IsObject(appearance)
            ? cJSON_GetObjectItemCaseSensitive(
                appearance,
                "accessibility")
            : NULL;

    int theme = 0;
    int accent = 0;
    int density = 0;

    if (!cJSON_IsObject(appearance) ||
        !json_integer(
            appearance,
            "theme",
            UI_THEME_CLASSIC,
            UI_THEME_GLASS,
            &theme) ||
        !json_integer(
            appearance,
            "accent",
            UI_ACCENT_DEFAULT,
            UI_ACCENT_COUNT - 1,
            &accent) ||
        !json_integer(
            appearance,
            "density",
            0,
            UI_DENSITY_COUNT - 1,
            &density) ||
        !cJSON_IsObject(accessibility) ||
        !json_boolean(
            accessibility,
            "large_text",
            &snapshot->accessibility.large_text) ||
        !json_boolean(
            accessibility,
            "high_contrast",
            &snapshot->accessibility.high_contrast) ||
        !json_boolean(
            accessibility,
            "reduced_transparency",
            &snapshot->accessibility.reduced_transparency) ||
        !json_boolean(
            accessibility,
            "reduced_motion",
            &snapshot->accessibility.reduced_motion)) {
        set_status(
            status,
            status_size,
            "Backup appearance settings are invalid.");
        return false;
    }

    snapshot->theme = (ui_theme_id_t)theme;
    snapshot->accent = (ui_accent_id_t)accent;
    snapshot->density = (ui_density_id_t)density;

    const cJSON *custom_theme_id =
        cJSON_GetObjectItemCaseSensitive(
            appearance,
            "custom_theme_id");
    if (cJSON_IsString(custom_theme_id) &&
        custom_theme_id->valuestring) {
        if (strlen(custom_theme_id->valuestring) >
            CUSTOM_THEME_ID_MAX) {
            set_status(
                status,
                status_size,
                "Backup custom-theme identity is invalid.");
            return false;
        }
        strlcpy(
            snapshot->custom_theme_id,
            custom_theme_id->valuestring,
            sizeof(snapshot->custom_theme_id));
    }

    int timezone_index = 0;
    int brightness = 0;
    int sleep_timeout = 0;

    const cJSON *display =
        cJSON_GetObjectItemCaseSensitive(
            root,
            "display");

    if (!json_integer(
            root,
            "timezone_index",
            0,
            (int)timezone_config_count() - 1,
            &timezone_index) ||
        !cJSON_IsObject(display) ||
        !json_integer(
            display,
            "brightness",
            10,
            100,
            &brightness) ||
        !json_integer(
            display,
            "sleep_timeout_minutes",
            0,
            30,
            &sleep_timeout) ||
        !valid_sleep_timeout(sleep_timeout)) {
        set_status(
            status,
            status_size,
            "Backup regional or display settings are invalid.");
        return false;
    }

    snapshot->timezone_index =
        (size_t)timezone_index;

    snapshot->brightness = brightness;
    snapshot->sleep_timeout =
        (uint8_t)sleep_timeout;

    return true;
}


static bool apply_snapshot(
    const settings_backup_snapshot_t *snapshot)
{
    if (!snapshot) {
        return false;
    }

    bool theme_applied =
        snapshot->custom_theme_id[0]
            ? theme_manager_select_custom_id(
                snapshot->custom_theme_id)
            : theme_manager_select(snapshot->theme);

    return
        theme_applied &&
        theme_manager_select_accent(snapshot->accent) &&
        theme_manager_select_density(snapshot->density) &&
        theme_manager_set_accessibility(
            snapshot->accessibility) &&
        timezone_config_select(
            snapshot->timezone_index) &&
        ui_settings_restore_display_preferences(
            snapshot->brightness,
            snapshot->sleep_timeout) &&
        moonraker_config_replace_profiles(
            snapshot->profiles,
            MOONRAKER_CONFIG_MAX_PROFILES,
            snapshot->active_profile);
}


bool settings_backup_export(
    char *status,
    size_t status_size)
{
    settings_backup_snapshot_t snapshot;
    capture_current(&snapshot);

    cJSON *root = snapshot_to_json(&snapshot, false);

    if (!root) {
        set_status(
            status,
            status_size,
            "Unable to create backup document.");
        return false;
    }

    char *document =
        cJSON_PrintBuffered(
            root,
            2048,
            true);

    cJSON_Delete(root);

    if (!document) {
        set_status(
            status,
            status_size,
            "Unable to encode backup document.");
        return false;
    }

    if (mkdir(
            SETTINGS_BACKUP_DIRECTORY,
            0775) != 0 &&
        errno != EEXIST) {
        cJSON_free(document);
        set_status(
            status,
            status_size,
            "SD card is unavailable or not writable.");
        return false;
    }

    FILE *file = fopen(
        SETTINGS_BACKUP_TEMP_PATH,
        "wb");

    if (!file) {
        cJSON_free(document);
        set_status(
            status,
            status_size,
            "Could not create the backup on the SD card.");
        return false;
    }

    size_t length = strlen(document);
    bool written =
        fwrite(document, 1, length, file) == length &&
        fwrite("\n", 1, 1, file) == 1 &&
        fflush(file) == 0 &&
        fsync(fileno(file)) == 0;

    bool closed = fclose(file) == 0;
    cJSON_free(document);

    if (!written || !closed) {
        unlink(SETTINGS_BACKUP_TEMP_PATH);
        set_status(
            status,
            status_size,
            "Backup write failed; existing backup was preserved.");
        return false;
    }

    /*
     * FATFS rename does not reliably replace an existing destination. Rotate
     * the known-good backup out of the way first, then restore it if promotion
     * of the fully written temporary file fails.
     */
    if (unlink(SETTINGS_BACKUP_PREVIOUS_PATH) != 0 && errno != ENOENT) {
        unlink(SETTINGS_BACKUP_TEMP_PATH);
        set_status(
            status,
            status_size,
            "Backup rotation could not clear its previous recovery copy.");
        return false;
    }

    bool previous_backup = false;
    if (rename(SETTINGS_BACKUP_PATH, SETTINGS_BACKUP_PREVIOUS_PATH) == 0) {
        previous_backup = true;
    } else if (errno != ENOENT) {
        unlink(SETTINGS_BACKUP_TEMP_PATH);
        set_status(
            status,
            status_size,
            "Existing backup could not be prepared for replacement.");
        return false;
    }

    if (rename(SETTINGS_BACKUP_TEMP_PATH, SETTINGS_BACKUP_PATH) != 0) {
        if (previous_backup) {
            (void)rename(
                SETTINGS_BACKUP_PREVIOUS_PATH,
                SETTINGS_BACKUP_PATH);
        }
        unlink(SETTINGS_BACKUP_TEMP_PATH);
        set_status(
            status,
            status_size,
            "Backup replacement failed; the previous backup was restored.");
        return false;
    }

    if (previous_backup) {
        (void)unlink(SETTINGS_BACKUP_PREVIOUS_PATH);
    }

    set_status(
        status,
        status_size,
        "Configuration backed up to the SD card.");

    operator_event_log_add(
        OPERATOR_EVENT_INFO,
        "Configuration backup saved to SD card");

    return true;
}


static bool load_backup_snapshot(
    settings_backup_snapshot_t *snapshot,
    char *status,
    size_t status_size)
{
    if (!snapshot) {
        set_status(status, status_size, "Backup destination is unavailable.");
        return false;
    }

    FILE *file = fopen(
        SETTINGS_BACKUP_PATH,
        "rb");

    if (!file) {
        set_status(
            status,
            status_size,
            "No configuration backup was found on the SD card.");
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        set_status(
            status,
            status_size,
            "Unable to inspect the backup file.");
        return false;
    }

    long file_size = ftell(file);

    if (file_size <= 0 ||
        file_size > SETTINGS_BACKUP_MAX_BYTES ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        set_status(
            status,
            status_size,
            "Backup file size is invalid.");
        return false;
    }

    char *document = heap_caps_malloc(
        (size_t)file_size + 1,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!document) {
        document = malloc((size_t)file_size + 1);
    }

    if (!document) {
        fclose(file);
        set_status(
            status,
            status_size,
            "Not enough memory to validate the backup.");
        return false;
    }

    size_t read_size = fread(
        document,
        1,
        (size_t)file_size,
        file);

    bool closed = fclose(file) == 0;
    document[read_size] = '\0';

    if (read_size != (size_t)file_size || !closed) {
        free(document);
        set_status(
            status,
            status_size,
            "Backup file could not be read completely.");
        return false;
    }

    cJSON *root = cJSON_ParseWithLength(document, read_size);
    free(document);

    if (!root) {
        set_status(
            status,
            status_size,
            "Backup file contains invalid JSON.");
        return false;
    }

    bool valid = parse_snapshot(
        root, snapshot, false, status, status_size);
    cJSON_Delete(root);
    return valid;
}


static bool write_encrypted_backup_file(
    const uint8_t *data,
    size_t data_size,
    char *status,
    size_t status_size)
{
    if (mkdir(SETTINGS_BACKUP_DIRECTORY, 0775) != 0 && errno != EEXIST) {
        set_status(status, status_size, "SD card is unavailable or not writable.");
        return false;
    }

    FILE *file = fopen(SETTINGS_BACKUP_ENCRYPTED_TEMP_PATH, "wb");
    if (!file) {
        set_status(status, status_size, "Could not create encrypted backup on the SD card.");
        return false;
    }

    bool written = fwrite(data, 1, data_size, file) == data_size &&
        fflush(file) == 0 && fsync(fileno(file)) == 0;
    bool closed = fclose(file) == 0;
    if (!written || !closed) {
        unlink(SETTINGS_BACKUP_ENCRYPTED_TEMP_PATH);
        set_status(status, status_size, "Encrypted backup write failed; existing backup was preserved.");
        return false;
    }

    if (unlink(SETTINGS_BACKUP_ENCRYPTED_PREVIOUS_PATH) != 0 && errno != ENOENT) {
        unlink(SETTINGS_BACKUP_ENCRYPTED_TEMP_PATH);
        set_status(status, status_size, "Encrypted backup rotation could not clear recovery copy.");
        return false;
    }

    bool previous_backup = false;
    if (rename(SETTINGS_BACKUP_ENCRYPTED_PATH,
               SETTINGS_BACKUP_ENCRYPTED_PREVIOUS_PATH) == 0) {
        previous_backup = true;
    } else if (errno != ENOENT) {
        unlink(SETTINGS_BACKUP_ENCRYPTED_TEMP_PATH);
        set_status(status, status_size, "Existing encrypted backup could not be replaced.");
        return false;
    }

    if (rename(SETTINGS_BACKUP_ENCRYPTED_TEMP_PATH,
               SETTINGS_BACKUP_ENCRYPTED_PATH) != 0) {
        if (previous_backup) {
            (void)rename(SETTINGS_BACKUP_ENCRYPTED_PREVIOUS_PATH,
                         SETTINGS_BACKUP_ENCRYPTED_PATH);
        }
        unlink(SETTINGS_BACKUP_ENCRYPTED_TEMP_PATH);
        set_status(status, status_size, "Encrypted backup replacement failed; previous backup was restored.");
        return false;
    }
    if (previous_backup) {
        (void)unlink(SETTINGS_BACKUP_ENCRYPTED_PREVIOUS_PATH);
    }
    return true;
}

bool settings_backup_export_encrypted_with_progress(
    const char *passphrase,
    settings_backup_encrypted_progress_cb_t progress_cb,
    void *progress_user_data,
    char *status,
    size_t status_size)
{
    if (progress_cb) {
        progress_cb(SETTINGS_BACKUP_ENCRYPTED_PREPARING, progress_user_data);
    }

    settings_backup_snapshot_t snapshot;
    capture_current(&snapshot);
    cJSON *root = snapshot_to_json(&snapshot, true);
    if (!root) {
        set_status(status, status_size, "Unable to create encrypted backup document.");
        return false;
    }
    char *document = cJSON_PrintBuffered(root, 2048, true);
    cJSON_Delete(root);
    if (!document) {
        set_status(status, status_size, "Unable to encode encrypted backup document.");
        return false;
    }

    if (progress_cb) {
        progress_cb(SETTINGS_BACKUP_ENCRYPTED_ENCRYPTING, progress_user_data);
    }

    uint8_t *encrypted = NULL;
    size_t encrypted_size = 0;
    bool encrypted_ok = settings_backup_crypto_encrypt(
        passphrase, (const uint8_t *)document, strlen(document),
        &encrypted, &encrypted_size, status, status_size);
    cJSON_free(document);
    if (!encrypted_ok) {
        return false;
    }

    if (progress_cb) {
        progress_cb(SETTINGS_BACKUP_ENCRYPTED_WRITING, progress_user_data);
    }

    bool written = write_encrypted_backup_file(
        encrypted, encrypted_size, status, status_size);
    settings_backup_crypto_free(encrypted, encrypted_size);
    if (!written) {
        return false;
    }
    set_status(status, status_size,
        "Encrypted configuration backup saved. API keys are protected inside it.");
    operator_event_log_add(OPERATOR_EVENT_INFO,
        "Encrypted configuration backup saved to SD card");
    return true;
}

bool settings_backup_export_encrypted(
    const char *passphrase,
    char *status,
    size_t status_size)
{
    return settings_backup_export_encrypted_with_progress(
        passphrase,
        NULL,
        NULL,
        status,
        status_size);
}


static bool load_encrypted_backup_snapshot(
    const char *passphrase,
    settings_backup_snapshot_t *snapshot,
    char *status,
    size_t status_size)
{
    FILE *file = fopen(SETTINGS_BACKUP_ENCRYPTED_PATH, "rb");
    if (!file) {
        set_status(status, status_size, "No encrypted configuration backup was found on the SD card.");
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        set_status(status, status_size, "Unable to inspect encrypted backup.");
        return false;
    }
    long file_size = ftell(file);
    if (file_size <= 0 || file_size > SETTINGS_BACKUP_ENCRYPTED_MAX_BYTES ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        set_status(status, status_size, "Encrypted backup file size is invalid.");
        return false;
    }
    uint8_t *encrypted = heap_caps_malloc((size_t)file_size,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!encrypted) {
        encrypted = malloc((size_t)file_size);
    }
    if (!encrypted) {
        fclose(file);
        set_status(status, status_size, "Not enough memory to read encrypted backup.");
        return false;
    }
    size_t read_size = fread(encrypted, 1, (size_t)file_size, file);
    bool closed = fclose(file) == 0;
    if (read_size != (size_t)file_size || !closed) {
        settings_backup_crypto_free(encrypted, (size_t)file_size);
        set_status(status, status_size, "Encrypted backup file could not be read completely.");
        return false;
    }

    uint8_t *document = NULL;
    size_t document_size = 0;
    bool decrypted = settings_backup_crypto_decrypt(
        passphrase, encrypted, (size_t)file_size, &document, &document_size,
        status, status_size);
    settings_backup_crypto_free(encrypted, (size_t)file_size);
    if (!decrypted) {
        return false;
    }
    cJSON *root = cJSON_ParseWithLength((const char *)document, document_size);
    settings_backup_crypto_free(document, document_size + 1);
    if (!root) {
        set_status(status, status_size, "Encrypted backup contains invalid JSON.");
        return false;
    }
    bool valid = parse_snapshot(root, snapshot, true, status, status_size);
    cJSON_Delete(root);
    return valid;
}

static void format_preflight(
    const settings_backup_snapshot_t *snapshot,
    bool encrypted,
    char *status,
    size_t status_size)
{
    int profile_count = 0;
    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        if (snapshot->profiles[index].configured) {
            ++profile_count;
        }
    }
    const moonraker_profile_t *active =
        &snapshot->profiles[snapshot->active_profile];
    snprintf(status, status_size,
        "Backup verified: schema %d, %d printer %s.\n"
        "Active printer: %s.\n"
        "Appearance and display settings included.\n%s",
        SETTINGS_BACKUP_SCHEMA, profile_count,
        profile_count == 1 ? "profile" : "profiles",
        active->name[0] ? active->name : "unnamed printer",
        encrypted ? "Moonraker API keys are included and encrypted."
                  : "Moonraker API keys are not included in backups.");
}

bool settings_backup_encrypted_preflight(
    const char *passphrase,
    char *status,
    size_t status_size)
{
    settings_backup_snapshot_t snapshot;
    if (!load_encrypted_backup_snapshot(passphrase, &snapshot, status, status_size)) {
        return false;
    }
    format_preflight(&snapshot, true, status, status_size);
    return true;
}

bool settings_backup_restore_encrypted(
    const char *passphrase,
    char *status,
    size_t status_size)
{
    settings_backup_snapshot_t restored;
    if (!load_encrypted_backup_snapshot(passphrase, &restored, status, status_size)) {
        operator_event_log_add(OPERATOR_EVENT_ERROR,
            "Encrypted configuration restore rejected: validation failed");
        return false;
    }
    settings_backup_snapshot_t previous;
    capture_current(&previous);
    if (!apply_snapshot(&restored)) {
        (void)apply_snapshot(&previous);
        set_status(status, status_size,
            "Encrypted restore failed; previous settings were reapplied.");
        operator_event_log_add(OPERATOR_EVENT_ERROR,
            "Encrypted configuration restore failed");
        return false;
    }
    set_status(status, status_size,
        "Encrypted configuration restored. Reboot the controller to finish.");
    operator_event_log_add(OPERATOR_EVENT_INFO,
        "Encrypted configuration restored from SD card");
    return true;
}

bool settings_backup_remove_all(char *status, size_t status_size)
{
    const char *paths[] = {
        SETTINGS_BACKUP_PATH,
        SETTINGS_BACKUP_TEMP_PATH,
        SETTINGS_BACKUP_PREVIOUS_PATH,
        SETTINGS_BACKUP_ENCRYPTED_PATH,
        SETTINGS_BACKUP_ENCRYPTED_TEMP_PATH,
        SETTINGS_BACKUP_ENCRYPTED_PREVIOUS_PATH,
    };
    int removed = 0;
    for (size_t index = 0; index < sizeof(paths) / sizeof(paths[0]); ++index) {
        if (unlink(paths[index]) == 0) {
            ++removed;
        } else if (errno != ENOENT) {
            set_status(status, status_size,
                "Could not remove every backup; existing files were preserved.");
            return false;
        }
    }
    snprintf(status, status_size, "Removed %d configuration backup file%s.",
        removed, removed == 1 ? "" : "s");
    operator_event_log_add(OPERATOR_EVENT_INFO,
        "Configuration backup files removed from SD card");
    return true;
}

bool settings_backup_preflight(
    char *status,
    size_t status_size)
{
    settings_backup_snapshot_t snapshot;

    if (!load_backup_snapshot(&snapshot, status, status_size)) {
        return false;
    }

    int profile_count = 0;
    for (int index = 0;
         index < MOONRAKER_CONFIG_MAX_PROFILES;
         ++index) {
        if (snapshot.profiles[index].configured) {
            ++profile_count;
        }
    }

    const moonraker_profile_t *active =
        &snapshot.profiles[snapshot.active_profile];

    snprintf(
        status,
        status_size,
        "Backup verified: schema %d, %d printer %s.\n"
        "Active printer: %s.\n"
        "Appearance and display settings included.\n"
        "Moonraker API keys are not included in backups.",
        SETTINGS_BACKUP_SCHEMA,
        profile_count,
        profile_count == 1 ? "profile" : "profiles",
        active->name[0] ? active->name : "unnamed printer");
    return true;
}


bool settings_backup_restore(
    char *status,
    size_t status_size)
{
    settings_backup_snapshot_t restored;

    if (!load_backup_snapshot(&restored, status, status_size)) {
        operator_event_log_add(
            OPERATOR_EVENT_ERROR,
            "Configuration restore rejected: validation failed");
        return false;
    }

    settings_backup_snapshot_t previous;
    capture_current(&previous);

    if (!apply_snapshot(&restored)) {
        (void)apply_snapshot(&previous);

        set_status(
            status,
            status_size,
            "Restore failed; previous settings were reapplied.");

        operator_event_log_add(
            OPERATOR_EVENT_ERROR,
            "Configuration restore failed");
        return false;
    }

    set_status(
        status,
        status_size,
        "Configuration restored. Reboot the controller to finish.");

    operator_event_log_add(
        OPERATOR_EVENT_INFO,
        "Configuration restored from SD card");

    return true;
}
