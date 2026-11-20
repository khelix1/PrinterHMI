#include "console_controller.h"
#include "json_document_parse.h"

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
    size_t entry_capacity;
    char history[CONSOLE_HISTORY_CAPACITY][CONSOLE_COMMAND_MAX + 1];
    size_t entry_head;
    size_t entry_count;
    size_t history_head;
    size_t history_count;
    uint32_t next_sequence;
    console_entry_t entries[];
} console_store_t;

static console_store_t *s_store = NULL;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;


bool console_controller_init(void)
{
    if (s_store) {
        return true;
    }

    size_t capacity = CONSOLE_LOG_CAPACITY;
    size_t bytes = sizeof(*s_store) + capacity * sizeof(console_entry_t);
    s_store = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_store) {
        ESP_LOGI(TAG, "Console history: %u entries in PSRAM (%u bytes)",
                 (unsigned)capacity, (unsigned)bytes);
    } else {
        capacity = CONSOLE_LOG_FALLBACK_CAPACITY;
        bytes = sizeof(*s_store) + capacity * sizeof(console_entry_t);
        s_store = heap_caps_calloc(1, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    if (!s_store) {
        ESP_LOGE(TAG, "Unable to allocate console history");
        return false;
    }

    s_store->entry_capacity = capacity;
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
        s_store->entry_capacity;

    if (s_store->entry_count == s_store->entry_capacity) {
        write_index = s_store->entry_head;
        s_store->entry_head =
            (s_store->entry_head + 1) %
            s_store->entry_capacity;
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

    size_t length = strlen(command);
    if (length > CONSOLE_COMMAND_MAX) return;

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

        memcpy(s_store->history[write_index], command, length + 1);
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
            s_store->entry_capacity;

        sequence = s_store->entries[newest].sequence;
    }

    portEXIT_CRITICAL(&s_lock);
    return sequence;
}


size_t console_controller_window(uint32_t *newest)
{
    size_t count = 0;
    if (newest) *newest = 0;
    if (!s_store) return 0;
    portENTER_CRITICAL(&s_lock);
    count = s_store->entry_count;
    if (count && newest) {
        size_t index = (s_store->entry_head + count - 1) % s_store->entry_capacity;
        *newest = s_store->entries[index].sequence;
    }
    portEXIT_CRITICAL(&s_lock);
    return count;
}

bool console_controller_get_sequence(uint32_t sequence, console_entry_t *out)
{
    if (!s_store || !sequence || !out) return false;
    bool found = false;
    portENTER_CRITICAL(&s_lock);
    const size_t count = s_store->entry_count, capacity = s_store->entry_capacity;
    if (count) {
        size_t last = (s_store->entry_head + count - 1) % capacity;
        uint32_t distance = console_sequence_distance(s_store->entries[last].sequence, sequence);
        if (distance < count) {
            size_t index = (s_store->entry_head + count - 1 - distance) % capacity;
            if (s_store->entries[index].sequence == sequence) {
                *out = s_store->entries[index];
                found = true;
            }
        }
    }
    portEXIT_CRITICAL(&s_lock);
    return found;
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
            s_store->entry_capacity;

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
    if (!output || !output_size) return false;
    output[0] = 0;
    if (!s_store) return false;

    bool found = false;
    portENTER_CRITICAL(&s_lock);

    if (newest_index < s_store->history_count) {
        size_t chronological =
            s_store->history_count - 1 - newest_index;

        size_t physical =
            (s_store->history_head + chronological) %
            CONSOLE_HISTORY_CAPACITY;

        const char *command = s_store->history[physical];
        size_t length = strlen(command);
        if (length < output_size) {
            memcpy(output, command, length + 1);
            found = true;
        }
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
        s_store->entry_capacity * sizeof(s_store->entries[0]));
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


/* Severity is a response prefix, not a keyword embedded in printer output. */
static bool response_prefix(const char *text, const char *prefix)
{
    while (*prefix && *text &&
           console_ascii_lower(*text) == console_ascii_lower(*prefix)) {
        ++text;
        ++prefix;
    }
    return !*prefix && (!*text || *text == ':' || *text == ' ' || *text == '\t');
}

static console_entry_type_t response_type(const char *response)
{
    if (!response) return CONSOLE_ENTRY_RESPONSE;
    while (*response == ' ' || *response == '\t') ++response;
    if (!strncmp(response, "//", 2)) {
        response += 2;
        while (*response == ' ' || *response == '\t') ++response;
    }
    /* Klipper reports command errors with !!; retain explicit legacy labels
     * and Unknown command (which Klipper emits as informational output). */
    if (!strncmp(response, "!!", 2) ||
        response_prefix(response, "error") ||
        response_prefix(response, "unknown command") ||
        response_prefix(response, "invalid command") ||
        response_prefix(response, "failed") ||
        response_prefix(response, "must home") ||
        response_prefix(response, "shutdown") ||
        response_prefix(response, "Klipper state: Shutdown"))
        return CONSOLE_ENTRY_ERROR;
    if (response_prefix(response, "warning") || response_prefix(response, "warn"))
        return CONSOLE_ENTRY_WARNING;
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
    if (!s_store || !json || length == 0 || length > 128 * 1024) {
        return false;
    }

    /*
     * Status updates dominate WebSocket traffic. Avoid a second JSON parse
     * unless the complete frame can actually be a console response.
     */
    static const char marker[] = "\"notify_gcode_response\"";
    bool candidate = false;
    if (length >= sizeof(marker) - 1) {
        for (size_t i = 0; i <= length - (sizeof(marker) - 1); ++i) {
            if (json[i] == '"' && !memcmp(json + i, marker, sizeof(marker) - 1)) {
                candidate = true;
                break;
            }
        }
    }
    if (!candidate) return false;
    cJSON *root = json_document_parse_bounded(json, length, 128 * 1024, 16);
    if (!root) return false;

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
