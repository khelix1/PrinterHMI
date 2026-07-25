#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define OPERATOR_EVENT_LOG_CAPACITY 40
#define OPERATOR_EVENT_MESSAGE_MAX 112

typedef enum {
    OPERATOR_EVENT_INFO = 0,
    OPERATOR_EVENT_WARNING,
    OPERATOR_EVENT_ERROR
} operator_event_level_t;

typedef struct {
    uint32_t sequence;
    time_t timestamp;
    uint32_t uptime_seconds;
    operator_event_level_t level;
    char message[OPERATOR_EVENT_MESSAGE_MAX];
} operator_event_t;

/*
 * Allocates the bounded event ring in PSRAM when available, with an
 * internal-RAM fallback. Call once during application startup.
 */
bool operator_event_log_init(void);

void operator_event_log_add(
    operator_event_level_t level,
    const char *format,
    ...);

/* Events are returned newest first. */
size_t operator_event_log_count(void);
bool operator_event_log_get(
    size_t newest_index,
    operator_event_t *out);

void operator_event_log_clear(void);
