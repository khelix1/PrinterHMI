#include "moonraker_http_fallback_decoder.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void copy_text(char *destination, size_t destination_size,
                      const char *source)
{
    if (!destination || destination_size == 0) return;
    strlcpy(destination, source ? source : "", destination_size);
}

static int drybox_program_from_selected(double value)
{
    switch ((int)value) {
    case 1: return 1; /* PLA */
    case 2: return 2; /* PETG */
    default: return 0; /* none */
    }
}

static int drybox_program_from_active(double value)
{
    switch ((int)value) {
    case 1: return 1; /* PLA */
    case 2: return 2; /* PETG */
    case 3: return 3; /* hold */
    default: return 0; /* none */
    }
}

bool moonraker_http_fallback_decoder_decode(
    const moonraker_http_fallback_decoder_input_t *input,
    moonraker_http_fallback_decoder_output_t *output)
{
    if (!input || !input->objects || !output) return false;

    const char *json = input->objects;
    memset(output, 0, sizeof(*output));
    output->update = input->previous;
    output->drybox_selected_program = input->previous_drybox_selected_program;
    output->drybox_active_program = input->previous_drybox_active_program;
    output->current_z = input->cached_current_z;
    output->metadata_object_height = input->cached_metadata_object_height;
    output->metadata_layer_height = input->cached_metadata_layer_height;
    copy_text(output->printer_state, sizeof(output->printer_state),
              input->previous.printer_state);
    copy_text(output->printer_file, sizeof(output->printer_file),
              input->previous.printer_file);

    double value = 0.0;
    if (json_find_number_after(json, "\"temperature_sensor drybox_center\"",
                               "temperature", &value))
        output->update.chamber_temp = value;
    if (json_find_number_after(json, "\"sht3x drybox_env\"", "temperature", &value))
        output->update.air_temp = value;
    if (json_find_number_after(json, "\"sht3x drybox_env\"", "humidity", &value))
        output->update.humidity = value;
    if (json_find_number_after(json, "\"heater_generic drybox_heater\"", "target", &value))
        output->update.heater_target = value;
    if (json_find_number_after(json, "\"heater_generic drybox_heater\"", "power", &value))
        output->update.heater_on = value > 0.01;
    if (json_find_number_after(json, "\"fan\"", "speed", &value))
        output->update.part_fan_speed = value * 100.0;
    if (json_find_number_after(json, "\"gcode_move\"", "speed_factor", &value))
        output->update.speed_factor = value * 100.0;
    if (json_find_number_after(json, "\"gcode_move\"", "extrude_factor", &value))
        output->update.flow_factor = value * 100.0;
    if (json_find_number_after(json, "\"fan_generic drybox_fan\"", "speed", &value))
        output->update.drybox_fan_speed = value * 100.0;

    if (json_find_number_after(json, "\"gcode_macro DRYBOX_VARS\"",
                               "selected_program", &value))
        output->drybox_selected_program = drybox_program_from_selected(value);
    if (json_find_number_after(json, "\"gcode_macro DRYBOX_VARS\"",
                               "active_program", &value))
        output->drybox_active_program = drybox_program_from_active(value);

    const char *print_stats = strstr(json, "\"print_stats\"");
    const char *print_scope = print_stats ? print_stats : json;
    json_find_string(print_scope, "state", output->printer_state,
                     sizeof(output->printer_state));
    json_find_string(print_scope, "filename", output->printer_file,
                     sizeof(output->printer_file));
    if (!output->printer_state[0])
        copy_text(output->printer_state, sizeof(output->printer_state), "--");
    if (!output->printer_file[0])
        copy_text(output->printer_file, sizeof(output->printer_file), "No file");

    output->update.progress = 0.0;
    if (!json_find_number_after(json, "\"virtual_sdcard\"", "progress",
                                &output->update.progress))
        json_find_number_after(json, "\"display_status\"", "progress",
                               &output->update.progress);
    if (output->update.progress < 0.0) output->update.progress = 0.0;
    if (output->update.progress > 1.0) output->update.progress = 1.0;
    json_find_number_after(json, "\"print_stats\"", "print_duration",
                           &output->update.print_duration);

    double velocity = 0.0;
    if (json_find_number_after(json, "\"motion_report\"", "live_velocity", &velocity))
        output->update.live_velocity = velocity;
    double extruder_velocity = 0.0;
    if (json_find_number_after(json, "\"motion_report\"",
                               "live_extruder_velocity", &extruder_velocity)) {
        output->update.live_flow = fabs(extruder_velocity) * 2.405281875;
        if (output->update.live_flow < 0.01) output->update.live_flow = 0.0;
    }

    int current_layer = input->cached_current_layer;
    int total_layer = input->cached_total_layer;
    const char *print_info = print_stats ? strstr(print_stats, "\"info\"") : NULL;
    double layer = 0.0;
    if (print_info && json_find_number_after(print_info, "\"info\"",
                                              "current_layer", &layer))
        current_layer = (int)(layer + 0.5);
    if (print_info && json_find_number_after(print_info, "\"info\"",
                                              "total_layer", &layer))
        total_layer = (int)(layer + 0.5);

    const char *gcode_move = strstr(json, "\"gcode_move\"");
    const char *position = gcode_move ? strstr(gcode_move, "\"position\"") : NULL;
    const char *array = position ? strchr(position, '[') : NULL;
    if (array) {
        double x = 0.0, y = 0.0, z = 0.0;
        if (sscanf(array, "[ %lf , %lf , %lf", &x, &y, &z) == 3)
            output->current_z = z;
    }

    if ((current_layer <= 0 || total_layer <= 0) && input->file_metadata_valid &&
        input->file_object_height > 0.0 && input->file_layer_height > 0.0) {
        output->metadata_object_height = input->file_object_height;
        output->metadata_layer_height = input->file_layer_height;
        total_layer = (int)floor((input->file_object_height /
                                  input->file_layer_height) + 0.001);
        if (output->current_z >= 0.0) {
            current_layer = (int)floor((output->current_z /
                                        input->file_layer_height) + 0.001);
            if (current_layer < 1 && output->current_z > 0.0) current_layer = 1;
            if (current_layer > total_layer) current_layer = total_layer;
        }
    }

    output->update.current_layer = current_layer;
    output->update.total_layer = total_layer;
    json_find_number_after(json, "\"extruder\"", "temperature",
                           &output->update.nozzle_temp);
    json_find_number_after(json, "\"extruder\"", "target",
                           &output->update.nozzle_target);
    json_find_number_after(json, "\"heater_bed\"", "temperature",
                           &output->update.bed_temp);
    json_find_number_after(json, "\"heater_bed\"", "target",
                           &output->update.bed_target);

    output->update.live_data_ok = true;
    output->update.moonraker_ok = true;
    output->update.printer_state = output->printer_state;
    output->update.printer_file = output->printer_file;
    return true;
}
