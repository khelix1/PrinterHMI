#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define CONSOLE_LOG_CAPACITY 64
#define CONSOLE_MESSAGE_MAX 192
#define CONSOLE_HISTORY_CAPACITY 16
#define CONSOLE_COMMAND_MAX 256

typedef enum {
    CONSOLE_ENTRY_COMMAND = 0,
    CONSOLE_ENTRY_RESPONSE,
    CONSOLE_ENTRY_WARNING,
    CONSOLE_ENTRY_ERROR,
    CONSOLE_ENTRY_SYSTEM
} console_entry_type_t;

typedef struct {
    uint32_t sequence;
    time_t timestamp;
    uint32_t uptime_seconds;
    console_entry_type_t type;
    char message[CONSOLE_MESSAGE_MAX];
} console_entry_t;

/*
 * Allocates one bounded permanent store in PSRAM, with internal RAM as a
 * fallback. The store is retained for the entire application lifetime.
 */
bool console_controller_init(void);

void console_controller_add(
    console_entry_type_t type,
    const char *format,
    ...);

void console_controller_add_command(const char *command);

size_t console_controller_count(void);
uint32_t console_controller_latest_sequence(void);

/* Entries and command history are returned newest first. */
bool console_controller_get(
    size_t newest_index,
    console_entry_t *out);

size_t console_controller_history_count(void);
bool console_controller_history_get(
    size_t newest_index,
    char *output,
    size_t output_size);

void console_controller_clear(void);

/*
 * Captures notify_gcode_response messages. Returns true only when the JSON
 * was a G-code response notification consumed by the console.
 */
bool console_controller_ingest_websocket_json(
    const char *json,
    size_t length);
