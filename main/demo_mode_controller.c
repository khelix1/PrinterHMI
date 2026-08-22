#include "demo_mode_controller.h"

#include <math.h>
#include <string.h>

#include "esp_timer.h"
#include "moonraker.h"

static int64_t s_started_us = 0;
static bool s_paused = false;
static bool s_cancelled = false;

bool demo_mode_controller_enabled(void)
{
    return true;
}

void demo_mode_controller_init(void)
{
    s_started_us = esp_timer_get_time();

    static const char hotends[][MOONRAKER_HOTEND_NAME_MAX] = { "extruder" };
    moonraker_state_configure_hotends(hotends, 1);

    const moonraker_capabilities_t capabilities = {
        .discovered = true, .has_heated_bed = true, .has_part_fan = true,
        .has_exclude_object = true, .has_bed_mesh = true,
        .has_drybox_center_sensor = true,
        .has_drybox_environment_sensor = true,
        .has_drybox_heater = true, .has_drybox_fan = true,
        .has_drybox_macros = true,
    };
    moonraker_state_configure_capabilities(&capabilities);
}

void demo_mode_controller_tick(void)
{
    const int64_t now_us = esp_timer_get_time();
    const double elapsed = (double)(now_us - s_started_us) / 1000000.0;
    const double wave = sin(elapsed / 5.0);
    const double progress = 0.62 + fmod(elapsed, 900.0) / 900.0 * 0.25;
    const char *state = s_cancelled ? "complete" :
                        (s_paused ? "paused" : "printing");
    const char *file = s_cancelled ? "Bench_Bunny_Demo.3mf" :
                                     "Modular_Desk_Organizer.3mf";

    moonraker_state_publish_http_fallback(
        &(moonraker_http_fallback_update_t) {
            .chamber_temp = 31.8 + wave * 0.3,
            .air_temp = 29.6 + wave * 0.2,
            .humidity = 24.0 + wave * 0.6,
            .heater_target = 45.0, .heater_on = true,
            .drybox_fan_speed = 55.0,
            .part_fan_speed = s_paused || s_cancelled ? 0.0 : 82.0,
            .speed_factor = 100.0, .flow_factor = 100.0,
            .live_velocity = s_paused || s_cancelled ? 0.0 : 118.0 + wave * 4.0,
            .live_flow = s_paused || s_cancelled ? 0.0 : 8.4 + wave * 0.2,
            .nozzle_temp = 214.0 + wave * 0.5, .nozzle_target = 215.0,
            .bed_temp = 59.5 + wave * 0.2, .bed_target = 60.0,
            .progress = s_cancelled ? 1.0 : progress,
            .print_duration = 12840.0 + elapsed,
            .current_layer = s_cancelled ? 312 : 194 + (int)(elapsed / 30.0),
            .total_layer = 312, .live_data_ok = true, .moonraker_ok = true,
            .printer_state = state, .printer_file = file,
        });
}

bool demo_mode_controller_handle_command(const char *command)
{
    if (!command || !command[0]) return false;
    if (strstr(command, "PAUSE")) {
        s_paused = true; s_cancelled = false;
    } else if (strstr(command, "RESUME")) {
        s_paused = false; s_cancelled = false;
    } else if (strstr(command, "CANCEL") || strstr(command, "M112")) {
        s_paused = false; s_cancelled = true;
    } else {
        return false;
    }
    demo_mode_controller_tick();
    return true;
}
