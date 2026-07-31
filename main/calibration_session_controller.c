#include "calibration_session_controller.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "console_controller.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

typedef struct {
    calibration_session_snapshot_t snapshot;
} calibration_session_store_t;

static const char TAG[] = "calibration_session";
static calibration_session_store_t *s_store;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;


static const char *find_case_insensitive(
    const char *text,
    const char *needle)
{
    if (!text || !needle || !needle[0]) {
        return NULL;
    }

    size_t needle_length = strlen(needle);

    for (const char *start = text; *start; ++start) {
        size_t index = 0;

        while (index < needle_length &&
               start[index] &&
               toupper((unsigned char)start[index]) ==
                   toupper((unsigned char)needle[index])) {
            ++index;
        }

        if (index == needle_length) {
            return start;
        }
    }

    return NULL;
}


static bool contains_case_insensitive(
    const char *text,
    const char *needle)
{
    return find_case_insensitive(text, needle) != NULL;
}


static bool parse_screw_adjustment(
    const char *message,
    char *output,
    size_t output_size)
{
    if (!message || !output || output_size == 0) {
        return false;
    }

    const char *adjust =
        find_case_insensitive(message, "adjust ");

    if (!adjust) {
        return false;
    }

    const char *name = message;
    while (*name == '/' || *name == ' ' || *name == '\t') {
        ++name;
    }

    const char *separator = strchr(name, ':');
    if (!separator || separator >= adjust) {
        return false;
    }

    size_t name_length = (size_t)(separator - name);
    while (name_length > 0 &&
           isspace((unsigned char)name[name_length - 1])) {
        --name_length;
    }

    if (name_length == 0 || output_size < 4) {
        return false;
    }

    size_t prefix_max = output_size - 3;
    if (name_length > prefix_max) {
        name_length = prefix_max;
    }

    int written = snprintf(
        output,
        output_size,
        "%.*s: ",
        (int)name_length,
        name);

    if (written < 0 || (size_t)written >= output_size) {
        return false;
    }

    size_t used = (size_t)written;
    snprintf(
        output + used,
        output_size - used,
        "%.*s",
        (int)(output_size - used - 1),
        adjust);
    return true;
}


static void next_generation_locked(void)
{
    ++s_store->snapshot.generation;
    if (s_store->snapshot.generation == 0) {
        s_store->snapshot.generation = 1;
    }
}


static void append_result(
    const char *line,
    bool completed,
    bool save_available)
{
    if (!s_store || !line || !line[0]) {
        return;
    }

    portENTER_CRITICAL(&s_lock);

    size_t used = strlen(s_store->snapshot.results);
    if (used < sizeof(s_store->snapshot.results) - 1) {
        snprintf(
            s_store->snapshot.results + used,
            sizeof(s_store->snapshot.results) - used,
            "%s%.*s",
            used ? "\n" : "",
            (int)(sizeof(s_store->snapshot.results) -
                  used - (used ? 2 : 1)),
            line);
    }

    ++s_store->snapshot.adjustment_count;
    s_store->snapshot.status =
        CALIBRATION_SESSION_RESULTS;
    s_store->snapshot.completed =
        s_store->snapshot.completed || completed;
    s_store->snapshot.save_available =
        s_store->snapshot.save_available || save_available;
    next_generation_locked();

    portEXIT_CRITICAL(&s_lock);
}


bool calibration_session_controller_init(void)
{
    if (s_store) {
        return true;
    }

    s_store = heap_caps_calloc(
        1,
        sizeof(*s_store),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_store) {
        ESP_LOGI(
            TAG,
            "Calibration session allocated permanently in PSRAM: %u bytes",
            (unsigned)sizeof(*s_store));
        return true;
    }

    s_store = heap_caps_calloc(
        1,
        sizeof(*s_store),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!s_store) {
        ESP_LOGE(TAG, "Unable to allocate Calibration session");
        return false;
    }

    ESP_LOGW(TAG, "Calibration session using internal RAM fallback");
    return true;
}


void calibration_session_controller_begin(
    calibration_session_kind_t kind,
    uint32_t after_console_sequence)
{
    if (!s_store || kind == CALIBRATION_SESSION_KIND_NONE) {
        return;
    }

    portENTER_CRITICAL(&s_lock);

    uint32_t generation = s_store->snapshot.generation;
    memset(&s_store->snapshot, 0, sizeof(s_store->snapshot));
    s_store->snapshot.kind = kind;
    s_store->snapshot.status = CALIBRATION_SESSION_WAITING;
    s_store->snapshot.start_sequence = after_console_sequence;
    s_store->snapshot.last_sequence = after_console_sequence;
    s_store->snapshot.generation = generation;
    next_generation_locked();

    portEXIT_CRITICAL(&s_lock);
}


void calibration_session_controller_begin_screws_tilt(
    uint32_t after_console_sequence)
{
    calibration_session_controller_begin(
        CALIBRATION_SESSION_SCREWS_TILT,
        after_console_sequence);
}


void calibration_session_controller_mark_error(
    const char *message)
{
    if (!s_store) {
        return;
    }

    portENTER_CRITICAL(&s_lock);

    s_store->snapshot.status = CALIBRATION_SESSION_ERROR;
    s_store->snapshot.completed = false;
    s_store->snapshot.save_available = false;
    snprintf(
        s_store->snapshot.results,
        sizeof(s_store->snapshot.results),
        "%s",
        message && message[0]
            ? message
            : "Calibration command failed.");
    next_generation_locked();

    portEXIT_CRITICAL(&s_lock);
}


static bool generic_result_relevant(
    calibration_session_kind_t kind,
    const char *message)
{
    if (!message) {
        return false;
    }

    if (contains_case_insensitive(message, "SAVE_CONFIG")) {
        return true;
    }

    switch (kind) {
    case CALIBRATION_SESSION_PID:
        return contains_case_insensitive(
                   message,
                   "PID parameters") ||
            contains_case_insensitive(
                   message,
                   "PID_CALIBRATE");

    case CALIBRATION_SESSION_PROBE_Z:
        return contains_case_insensitive(
                   message,
                   "z_offset") ||
            contains_case_insensitive(
                   message,
                   "probe");

    case CALIBRATION_SESSION_INPUT_SHAPER:
        return contains_case_insensitive(
                   message,
                   "shaper");

    case CALIBRATION_SESSION_RESONANCE_TEST:
        return contains_case_insensitive(
                   message,
                   "resonance") ||
            contains_case_insensitive(
                   message,
                   "data written");

    case CALIBRATION_SESSION_ACCELEROMETER_CHECK:
        return contains_case_insensitive(
                   message,
                   "axes noise") ||
            contains_case_insensitive(
                   message,
                   "noise for");

    case CALIBRATION_SESSION_AXIS_TWIST:
        return contains_case_insensitive(
                   message,
                   "axis twist");

    case CALIBRATION_SESSION_CUSTOM:
        /*
         * Custom macros vary widely. Only the explicit SAVE_CONFIG evidence
         * handled above may unlock persistence.
         */
        return false;

    case CALIBRATION_SESSION_SCREWS_TILT:
    case CALIBRATION_SESSION_KIND_NONE:
    default:
        return false;
    }
}


void calibration_session_controller_poll(void)
{
    if (!s_store) {
        return;
    }

    calibration_session_kind_t kind;
    calibration_session_status_t status;
    uint32_t last_sequence;

    portENTER_CRITICAL(&s_lock);
    kind = s_store->snapshot.kind;
    status = s_store->snapshot.status;
    last_sequence = s_store->snapshot.last_sequence;
    portEXIT_CRITICAL(&s_lock);

    if (status == CALIBRATION_SESSION_IDLE ||
        status == CALIBRATION_SESSION_ERROR) {
        return;
    }

    size_t count = console_controller_count();

    for (size_t offset = count; offset > 0; --offset) {
        console_entry_t entry;

        if (!console_controller_get(offset - 1, &entry) ||
            entry.sequence <= last_sequence) {
            continue;
        }

        portENTER_CRITICAL(&s_lock);
        if (entry.sequence > s_store->snapshot.last_sequence) {
            s_store->snapshot.last_sequence = entry.sequence;
        }
        portEXIT_CRITICAL(&s_lock);
        last_sequence = entry.sequence;

        if (entry.type == CONSOLE_ENTRY_ERROR) {
            calibration_session_controller_mark_error(entry.message);
            return;
        }

        if (entry.type == CONSOLE_ENTRY_COMMAND ||
            entry.type == CONSOLE_ENTRY_SYSTEM) {
            continue;
        }

        if (kind == CALIBRATION_SESSION_SCREWS_TILT) {
            char adjustment[CONSOLE_MESSAGE_MAX];
            if (parse_screw_adjustment(
                    entry.message,
                    adjustment,
                    sizeof(adjustment))) {
                append_result(adjustment, true, false);
            }
            continue;
        }

        if (!generic_result_relevant(kind, entry.message)) {
            continue;
        }

        bool save_available =
            contains_case_insensitive(
                entry.message,
                "SAVE_CONFIG");
        append_result(
            entry.message,
            save_available,
            save_available);
    }
}


void calibration_session_controller_snapshot(
    calibration_session_snapshot_t *output)
{
    if (!output) {
        return;
    }

    memset(output, 0, sizeof(*output));
    if (!s_store) {
        return;
    }

    portENTER_CRITICAL(&s_lock);
    *output = s_store->snapshot;
    portEXIT_CRITICAL(&s_lock);
}


void calibration_session_controller_reset(void)
{
    if (!s_store) {
        return;
    }

    portENTER_CRITICAL(&s_lock);

    uint32_t generation = s_store->snapshot.generation;
    memset(&s_store->snapshot, 0, sizeof(s_store->snapshot));
    s_store->snapshot.generation = generation;
    next_generation_locked();

    portEXIT_CRITICAL(&s_lock);
}
