#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_http_client.h"

/*
 * Moonraker module.
 * Owns shared transport helpers now.
 * State object is the bridge toward removing globals from main.c.
 */

#define MOONRAKER_MAX_HOTENDS 4
#define MOONRAKER_HOTEND_NAME_MAX 24

typedef struct {
    char object_name[MOONRAKER_HOTEND_NAME_MAX];
    double temperature;
    double target;
    bool active;
} moonraker_hotend_t;

typedef struct {
    double chamber_temp;
    double air_temp;
    double humidity;

    int drybox_selected_program;
    int drybox_active_program;

    double heater_target;
    bool heater_on;
    double drybox_fan_speed;

    double part_fan_speed;
    double speed_factor;
    double flow_factor;

    double live_velocity;
    double live_flow;

    /*
     * Compatibility values used by the existing single-hotend UI. They
     * mirror the active hotend when capability-aware data is available.
     */
    double nozzle_temp;
    double nozzle_target;

    size_t hotend_count;
    char active_hotend[MOONRAKER_HOTEND_NAME_MAX];
    moonraker_hotend_t hotends[MOONRAKER_MAX_HOTENDS];

    double bed_temp;
    double bed_target;

    double progress;
    double print_duration;

    int current_layer;
    int total_layer;

    bool live_data_ok;
    bool moonraker_ok;

    char printer_state[32];
    char printer_file[256];
} moonraker_state_t;

#define MOONRAKER_EXCLUDE_MAX_OBJECTS 48
#define MOONRAKER_EXCLUDE_NAME_MAX 96
#define MOONRAKER_EXCLUDE_MAX_POLYGON_POINTS 64

typedef struct {
    double x;
    double y;
} moonraker_exclude_point_t;

typedef struct {
    char name[MOONRAKER_EXCLUDE_NAME_MAX];
    bool excluded;
    bool current;
    bool has_center;
    uint16_t polygon_count;
    moonraker_exclude_point_t center;
    moonraker_exclude_point_t
        polygon[MOONRAKER_EXCLUDE_MAX_POLYGON_POINTS];
} moonraker_exclude_object_t;

typedef struct {
    bool available;
    bool truncated;
    bool bed_bounds_valid;
    size_t object_count;
    double bed_min_x;
    double bed_min_y;
    double bed_max_x;
    double bed_max_y;
    char current_object[MOONRAKER_EXCLUDE_NAME_MAX];
    moonraker_exclude_object_t objects[MOONRAKER_EXCLUDE_MAX_OBJECTS];
} moonraker_exclude_state_t;

typedef struct {
    char *buf;
    int len;
    int cap;
} http_capture_t;

void moonraker_module_init(void);

const moonraker_state_t *moonraker_state_get(void);
void moonraker_state_snapshot(moonraker_state_t *out);
void moonraker_exclude_state_snapshot(moonraker_exclude_state_t *out);
bool moonraker_exclude_objects_available(void);
void moonraker_state_reset(void);

void moonraker_state_set_drybox_programs(
    int selected_program,
    int active_program);

void moonraker_state_set_connection(
    bool live_data_ok,
    bool moonraker_ok);

void moonraker_state_configure_hotends(
    const char names[][MOONRAKER_HOTEND_NAME_MAX],
    size_t count);

typedef enum {
    MOONRAKER_WEBSOCKET_MESSAGE_IGNORED = 0,
    MOONRAKER_WEBSOCKET_MESSAGE_STATUS,
    MOONRAKER_WEBSOCKET_MESSAGE_FILELIST_CHANGED,
    MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_READY,
    MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_SHUTDOWN,
    MOONRAKER_WEBSOCKET_MESSAGE_KLIPPY_DISCONNECTED
} moonraker_websocket_message_t;

/*
 * Merge or classify one complete Moonraker WebSocket JSON-RPC message.
 * Status messages update the synchronized live-state snapshot. File-list
 * and Klippy lifecycle notifications are returned to the transport owner.
 */
moonraker_websocket_message_t moonraker_state_merge_websocket_json(
    const char *json,
    size_t length);

void moonraker_state_update_from_legacy(
    double chamber_temp,
    double air_temp,
    double humidity,
    double heater_target,
    bool heater_on,
    double drybox_fan_speed,
    double part_fan_speed,
    double speed_factor,
    double flow_factor,
    double live_velocity,
    double live_flow,
    double nozzle_temp,
    double nozzle_target,
    double bed_temp,
    double bed_target,
    double progress,
    double print_duration,
    int current_layer,
    int total_layer,
    bool live_data_ok,
    bool moonraker_ok,
    const char *printer_state,
    const char *printer_file
);

esp_err_t moonraker_http_event_handler(esp_http_client_event_t *evt);

const char *json_find_string(const char *json, const char *key, char *out, size_t out_len);
bool json_find_number_after(const char *json, const char *anchor, const char *key, double *out);
bool json_find_number_after_key(const char *json, const char *key, double *out);
bool json_find_best_thumbnail_path(const char *json, char *out, size_t out_sz);

bool moonraker_test_connection(const char *host,
                                int port,
                                int *http_code,
                                esp_err_t *err_out);

bool moonraker_fetch_file_list(const char *host,
                               int port,
                               const char *api_key,
                               char *body,
                               size_t body_sz,
                               int *http_code,
                               esp_err_t *err_out);


bool moonraker_fetch_print_stats(const char *host,
                                 int port,
                                 const char *api_key,
                                 char *body,
                                 size_t body_sz,
                                 int *http_code,
                                 esp_err_t *err_out);

bool moonraker_fetch_file_metadata(const char *host,
                                   int port,
                                   const char *api_key,
                                   const char *encoded_filename,
                                   char *body,
                                   size_t body_sz,
                                   int *http_code,
                                   esp_err_t *err_out);

bool moonraker_send_gcode_script(const char *host,
                                  int port,
                                  const char *api_key,
                                  const char *cmd,
                                  int *http_code,
                                  esp_err_t *err_out);

bool moonraker_start_print_file(const char *host,
                                int port,
                                const char *api_key,
                                const char *filename,
                                int *http_code,
                                esp_err_t *err_out);


bool moonraker_fetch_thumbnail_encoded(const char *host,
                                       int port,
                                       const char *encoded_thumb_path,
                                       uint8_t **out_buf,
                                       size_t *out_len);
