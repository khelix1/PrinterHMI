#include "console_controller.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "console_controller";

typedef struct {
    console_entry_t entries[CONSOLE_LOG_CAPACITY];
    char history[CONSOLE_HISTORY_CAPACITY][CONSOLE_COMMAND_MAX + 1];
    size_t entry_head;
    size_t entry_count;
    size_t history_head;
    size_t history_count;
    uint32_t next_sequence;
} console_store_t;

static console_store_t *s_store = NULL;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;


bool console_controller_init(void)
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
            "Console history allocated permanently in PSRAM: %u bytes",
            (unsigned)sizeof(*s_store));
    } else {
        s_store = heap_caps_calloc(
            1,
            sizeof(*s_store),
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    if (!s_store) {
        ESP_LOGE(TAG, "Unable to allocate console history");
        return false;
    }

    s_store->next_sequence = 1;
    console_controller_add(
        CONSOLE_ENTRY_SYSTEM,
        "Console ready; waiting for Klipper responses.");
    return true;
}


void console_controller_add(
    console_entry_type_t type,
    const char *format,
    ...)
{
    if (!s_store || !format) {
        return;
    }

    console_entry_t entry = {
        .timestamp = time(NULL),
        .uptime_seconds =
            (uint32_t)(esp_timer_get_time() / 1000000LL),
        .type = type,
    };

    va_list arguments;
    va_start(arguments, format);
    vsnprintf(
        entry.message,
        sizeof(entry.message),
        format,
        arguments);
    va_end(arguments);

    portENTER_CRITICAL(&s_lock);

    entry.sequence = s_store->next_sequence++;
    if (s_store->next_sequence == 0) {
        s_store->next_sequence = 1;
    }

    size_t write_index =
        (s_store->entry_head + s_store->entry_count) %
        CONSOLE_LOG_CAPACITY;

    if (s_store->entry_count == CONSOLE_LOG_CAPACITY) {
        write_index = s_store->entry_head;
        s_store->entry_head =
            (s_store->entry_head + 1) %
            CONSOLE_LOG_CAPACITY;
    } else {
        ++s_store->entry_count;
    }

    s_store->entries[write_index] = entry;
    portEXIT_CRITICAL(&s_lock);
}


void console_controller_add_command(const char *command)
{
    if (!s_store || !command || !command[0]) {
        return;
    }

    console_controller_add(
        CONSOLE_ENTRY_COMMAND,
        "%s",
        command);

    portENTER_CRITICAL(&s_lock);

    bool duplicate = false;
    if (s_store->history_count > 0) {
        size_t newest =
            (s_store->history_head +
             s_store->history_count - 1) %
            CONSOLE_HISTORY_CAPACITY;

        duplicate =
            strcmp(s_store->history[newest], command) == 0;
    }

    if (!duplicate) {
        size_t write_index =
            (s_store->history_head + s_store->history_count) %
            CONSOLE_HISTORY_CAPACITY;

        if (s_store->history_count ==
            CONSOLE_HISTORY_CAPACITY) {
            write_index = s_store->history_head;
            s_store->history_head =
                (s_store->history_head + 1) %
                CONSOLE_HISTORY_CAPACITY;
        } else {
            ++s_store->history_count;
        }

        snprintf(
            s_store->history[write_index],
            sizeof(s_store->history[write_index]),
            "%.*s",
            CONSOLE_COMMAND_MAX,
            command);
    }

    portEXIT_CRITICAL(&s_lock);
}


size_t console_controller_count(void)
{
    if (!s_store) {
        return 0;
    }

    size_t count;
    portENTER_CRITICAL(&s_lock);
    count = s_store->entry_count;
    portEXIT_CRITICAL(&s_lock);
    return count;
}


uint32_t console_controller_latest_sequence(void)
{
    if (!s_store) {
        return 0;
    }

    uint32_t sequence = 0;
    portENTER_CRITICAL(&s_lock);

    if (s_store->entry_count > 0) {
        size_t newest =
            (s_store->entry_head +
             s_store->entry_count - 1) %
            CONSOLE_LOG_CAPACITY;

        sequence = s_store->entries[newest].sequence;
    }

    portEXIT_CRITICAL(&s_lock);
    return sequence;
}


bool console_controller_get(
    size_t newest_index,
    console_entry_t *out)
{
    if (!s_store || !out) {
        return false;
    }

    bool found = false;
    portENTER_CRITICAL(&s_lock);

    if (newest_index < s_store->entry_count) {
        size_t chronological =
            s_store->entry_count - 1 - newest_index;

        size_t physical =
            (s_store->entry_head + chronological) %
            CONSOLE_LOG_CAPACITY;

        *out = s_store->entries[physical];
        found = true;
    }

    portEXIT_CRITICAL(&s_lock);
    return found;
}


size_t console_controller_history_count(void)
{
    if (!s_store) {
        return 0;
    }

    size_t count;
    portENTER_CRITICAL(&s_lock);
    count = s_store->history_count;
    portEXIT_CRITICAL(&s_lock);
    return count;
}


bool console_controller_history_get(
    size_t newest_index,
    char *output,
    size_t output_size)
{
    if (!s_store || !output || output_size == 0) {
        return false;
    }

    bool found = false;
    portENTER_CRITICAL(&s_lock);

    if (newest_index < s_store->history_count) {
        size_t chronological =
            s_store->history_count - 1 - newest_index;

        size_t physical =
            (s_store->history_head + chronological) %
            CONSOLE_HISTORY_CAPACITY;

        snprintf(
            output,
            output_size,
            "%s",
            s_store->history[physical]);
        found = true;
    }

    portEXIT_CRITICAL(&s_lock);
    return found;
}


void console_controller_clear(void)
{
    if (!s_store) {
        return;
    }

    portENTER_CRITICAL(&s_lock);
    memset(
        s_store->entries,
        0,
        sizeof(s_store->entries));
    s_store->entry_head = 0;
    s_store->entry_count = 0;
    portEXIT_CRITICAL(&s_lock);
}


static char console_ascii_lower(char character)
{
    if (character >= 'A' && character <= 'Z') {
        return (char)(character + ('a' - 'A'));
    }

    return character;
}


static bool console_contains_ignore_case(
    const char *text,
    const char *needle)
{
    if (!text || !needle || !needle[0]) {
        return false;
    }

    for (const char *start = text; *start; ++start) {
        const char *left = start;
        const char *right = needle;

        while (*left &&
               *right &&
               console_ascii_lower(*left) ==
                   console_ascii_lower(*right)) {
            ++left;
            ++right;
        }

        if (!*right) {
            return true;
        }
    }

    return false;
}


static console_entry_type_t response_type(const char *response)
{
    if (!response) {
        return CONSOLE_ENTRY_RESPONSE;
    }

    if (strstr(response, "!!") ||
        console_contains_ignore_case(response, "error") ||
        console_contains_ignore_case(response, "unknown command") ||
        console_contains_ignore_case(response, "invalid command") ||
        console_contains_ignore_case(response, "failed") ||
        console_contains_ignore_case(response, "must home") ||
        console_contains_ignore_case(response, "shutdown")) {
        return CONSOLE_ENTRY_ERROR;
    }

    if (console_contains_ignore_case(response, "warning") ||
        console_contains_ignore_case(response, "warn")) {
        return CONSOLE_ENTRY_WARNING;
    }

    return CONSOLE_ENTRY_RESPONSE;
}


static void add_response_lines(const char *response)
{
    if (!response || !response[0]) {
        return;
    }

    const char *line = response;

    while (*line) {
        const char *end = strchr(line, '\n');
        size_t length =
            end ? (size_t)(end - line) : strlen(line);

        while (length > 0 && line[length - 1] == '\r') {
            --length;
        }

        if (length > 0) {
            char message[CONSOLE_MESSAGE_MAX];
            size_t copy_length = length;

            if (copy_length >= sizeof(message)) {
                copy_length = sizeof(message) - 1;
            }

            memcpy(message, line, copy_length);
            message[copy_length] = '\0';

            console_controller_add(
                response_type(message),
                "%s",
                message);
        }

        if (!end) {
            break;
        }
        line = end + 1;
    }
}


bool console_controller_ingest_websocket_json(
    const char *json,
    size_t length)
{
    if (!s_store || !json || length == 0) {
        return false;
    }

    /*
     * Status updates dominate WebSocket traffic. Avoid a second JSON parse
     * unless the complete frame can actually be a console response.
     */
    if (!strstr(json, "\"notify_gcode_response\"")) {
        return false;
    }

    cJSON *root = cJSON_ParseWithLength(json, length);
    if (!root) {
        return false;
    }

    cJSON *method =
        cJSON_GetObjectItemCaseSensitive(root, "method");
    bool consumed =
        cJSON_IsString(method) &&
        method->valuestring &&
        strcmp(
            method->valuestring,
            "notify_gcode_response") == 0;

    if (consumed) {
        cJSON *params =
            cJSON_GetObjectItemCaseSensitive(root, "params");
        cJSON *response =
            cJSON_IsArray(params)
                ? cJSON_GetArrayItem(params, 0)
                : NULL;

        if (cJSON_IsString(response) &&
            response->valuestring) {
            add_response_lines(response->valuestring);
        }
    }

    cJSON_Delete(root);
    return consumed;
}
