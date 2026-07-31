#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CALIBRATION_SESSION_RESULTS_MAX 1024

typedef enum {
    CALIBRATION_SESSION_KIND_NONE = 0,
    CALIBRATION_SESSION_SCREWS_TILT,
    CALIBRATION_SESSION_PID,
    CALIBRATION_SESSION_PROBE_Z,
    CALIBRATION_SESSION_INPUT_SHAPER,
    CALIBRATION_SESSION_RESONANCE_TEST,
    CALIBRATION_SESSION_ACCELEROMETER_CHECK,
    CALIBRATION_SESSION_AXIS_TWIST,
    CALIBRATION_SESSION_CUSTOM
} calibration_session_kind_t;

typedef enum {
    CALIBRATION_SESSION_IDLE = 0,
    CALIBRATION_SESSION_WAITING,
    CALIBRATION_SESSION_RESULTS,
    CALIBRATION_SESSION_ERROR
} calibration_session_status_t;

typedef struct {
    calibration_session_kind_t kind;
    calibration_session_status_t status;
    bool completed;
    bool save_available;
    uint32_t generation;
    uint32_t start_sequence;
    uint32_t last_sequence;
    size_t adjustment_count;
    char results[CALIBRATION_SESSION_RESULTS_MAX];
} calibration_session_snapshot_t;

/* Permanent PSRAM-first session state shared by guided calibration UIs. */
bool calibration_session_controller_init(void);

void calibration_session_controller_begin(
    calibration_session_kind_t kind,
    uint32_t after_console_sequence);

void calibration_session_controller_begin_screws_tilt(
    uint32_t after_console_sequence);

void calibration_session_controller_mark_error(
    const char *message);

/* Consumes new entries from the existing bounded Console history. */
void calibration_session_controller_poll(void);

void calibration_session_controller_snapshot(
    calibration_session_snapshot_t *output);

void calibration_session_controller_reset(void);
