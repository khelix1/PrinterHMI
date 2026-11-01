#include "demo_calibration.h"
#include "calibration_capability_controller.h"
#include "calibration_session_controller.h"
#include "console_controller.h"
#include "demo_mode.h"
#include "moonraker_config_controller.h"
#include "motion_diagnostics_controller.h"
#include "printer_connection_policy.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t owner;
static bool online;
static unsigned axis_samples;
bool demo_calibration_init(void)
{
    return console_controller_init() && calibration_session_controller_init();
}
void demo_calibration_refresh(const moonraker_state_t *state)
{
    bool ready = state && state->moonraker_ok && state->live_data_ok;
    uint32_t generation = moonraker_config_generation();
    if (owner == generation && online == ready) return;
    owner = generation;
    online = ready;
    axis_samples = 0;
    calibration_session_controller_reset();
    calibration_capability_controller_reset();
    if (ready && moonraker_config_active_profile_index() == 0) {
        cJSON *status = cJSON_Parse("{\"gcode\":{\"commands\":{\"AXIS_TWIST_COMPENSATION_CALIBRATE\":{},\"PROBE_ACCURACY\":{}}}}");
        if (status) calibration_capability_controller_merge_status(status);
        cJSON_Delete(status);
    }
}
static bool number(const char *text, const char *prefix, double lo, double hi)
{
    size_t n = strlen(prefix);
    if (strncmp(text, prefix, n)) return false;
    char *end;
    double value = strtod(text + n, &end);
    return end != text + n && !*end && isfinite(value) && value >= lo && value <= hi;
}
static bool is_session(const calibration_session_snapshot_t *s, calibration_session_kind_t kind)
{
    return s->kind == kind && s->status != CALIBRATION_SESSION_IDLE && s->status != CALIBRATION_SESSION_ERROR;
}
bool demo_calibration_send(const char *command)
{
    if (!demo_mode_enabled() || !command || !*command || strlen(command) >= 256) return false;
    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    if (printer_idle_reason(&state)[0]) return false;
    calibration_session_snapshot_t session;
    calibration_session_controller_snapshot(&session);
    const char *action = command;
    bool home = !strncmp(action, "G28\n", 4);
    if (home) action += 4;
    if (!strcmp(command, "G28")) return demo_printer_command("G28");
    if (home && strcmp(action, "SCREWS_TILT_CALCULATE") && strcmp(action, "Z_TILT_ADJUST") &&
        strcmp(action, "QUAD_GANTRY_LEVEL") && strcmp(action, "PROBE_CALIBRATE") &&
        strcmp(action, "AXIS_TWIST_COMPENSATION_CALIBRATE")) return false;
    if (!strcmp(action, "SCREWS_TILT_CALCULATE") && is_session(&session,CALIBRATION_SESSION_SCREWS_TILT)) {
        if (home && !demo_printer_command("G28")) return false;
        console_controller_add(CONSOLE_ENTRY_RESPONSE,"// front_left (base) : x=30.0, y=30.0, z=1.50000");
        console_controller_add(CONSOLE_ENTRY_RESPONSE,"// front_right : x=240.0, y=30.0, z=1.52000 : adjust CW 00:10");
        console_controller_add(CONSOLE_ENTRY_RESPONSE,"// rear_right : x=240.0, y=230.0, z=1.49000 : adjust CCW 00:05");
        return true;
    }
    if (!strcmp(action,"Z_TILT_ADJUST") || !strcmp(action,"QUAD_GANTRY_LEVEL")) {
        if (home && !demo_printer_command("G28")) return false;
        console_controller_add(CONSOLE_ENTRY_RESPONSE,"// Demo: gantry leveling complete."); return true;
    }
    if (!strcmp(action,"PROBE_CALIBRATE") && is_session(&session,CALIBRATION_SESSION_PROBE_Z)) {
        if (home && !demo_printer_command("G28")) return false;
        console_controller_add(CONSOLE_ENTRY_RESPONSE,"// probe: adjust TESTZ, then ACCEPT"); return true;
    }
    if (!strcmp(action,"AXIS_TWIST_COMPENSATION_CALIBRATE") && is_session(&session,CALIBRATION_SESSION_AXIS_TWIST)) {
        if (home && !demo_printer_command("G28")) return false;
        axis_samples=0; console_controller_add(CONSOLE_ENTRY_RESPONSE,"// axis twist: adjust TESTZ at the first sample, then ACCEPT"); return true;
    }
    bool manual = is_session(&session,CALIBRATION_SESSION_PROBE_Z) || is_session(&session,CALIBRATION_SESSION_AXIS_TWIST);
    if (manual && number(action,"TESTZ Z=",-1,1)) {
        if (!demo_printer_calibration_step(strtod(action+8,NULL))) return false;
        console_controller_add(CONSOLE_ENTRY_RESPONSE,"// probe: manual adjustment accepted"); return true;
    }
    if (manual && !strcmp(action,"ABORT")) { console_controller_add(CONSOLE_ENTRY_RESPONSE,"// Calibration aborted (demo)"); return true; }
    if (manual && !strcmp(action,"ACCEPT")) {
        if (session.kind==CALIBRATION_SESSION_AXIS_TWIST && ++axis_samples<3)
            console_controller_add(CONSOLE_ENTRY_RESPONSE,"// axis twist: next manual sample");
        else console_controller_add(CONSOLE_ENTRY_RESPONSE,"// %s calibration complete; SAVE_CONFIG to persist",session.kind==CALIBRATION_SESSION_AXIS_TWIST?"axis twist":"probe z_offset");
        return true;
    }
    const char *pid = "PID_CALIBRATE HEATER=";
    if (is_session(&session,CALIBRATION_SESSION_PID) && !strncmp(action,pid,strlen(pid))) {
        const char *heater=action+strlen(pid), *target=strstr(heater," TARGET=");
        size_t length=target?(size_t)(target-heater):0;
        bool hotend=length==8 && !strncmp(heater,"extruder",length);
        bool bed=length==10 && !strncmp(heater,"heater_bed",length);
        bool dry=length==13 && !strncmp(heater,"drybox_heater",length);
        if ((hotend || bed || dry) && number(target," TARGET=",1,hotend?300:120)) {
            console_controller_add(CONSOLE_ENTRY_RESPONSE,"// PID parameters: pid_Kp=22.2 pid_Ki=1.08 pid_Kd=114.0; SAVE_CONFIG to persist");
            return true;
        }
        return false;
    }
    if (is_session(&session,CALIBRATION_SESSION_PROBE_ACCURACY) && number(action,"PROBE_ACCURACY SAMPLES=",1,100)) {
        console_controller_add(CONSOLE_ENTRY_RESPONSE,"// probe accuracy results: maximum 1.509000, minimum 1.501000, range 0.008000, average 1.505000, median 1.505000, standard deviation 0.002000"); return true;
    }
    if (is_session(&session,CALIBRATION_SESSION_INPUT_SHAPER) && !strcmp(action,"SHAPER_CALIBRATE")) {
        console_controller_add(CONSOLE_ENTRY_RESPONSE,"// shaper_type_x=mzv shaper_freq_x=39.6, shaper_type_y=2hump_ei shaper_freq_y=66.0; SAVE_CONFIG to persist"); return true;
    }
    if (is_session(&session,CALIBRATION_SESSION_RESONANCE_TEST) && (!strcmp(action,"TEST_RESONANCES AXIS=X") || !strcmp(action,"TEST_RESONANCES AXIS=Y"))) {
        console_controller_add(CONSOLE_ENTRY_RESPONSE,"// resonance data written to local sample (no real file)"); return true;
    }
    if (is_session(&session,CALIBRATION_SESSION_ACCELEROMETER_CHECK) && !strcmp(action,"MEASURE_AXES_NOISE")) {
        console_controller_add(CONSOLE_ENTRY_RESPONSE,"// axes noise for xyz-axis accelerometer: 0.010000, 0.012000, 0.009000"); return true;
    }
    if (!strcmp(action,"SAVE_CONFIG") && session.completed && session.save_available) {
        console_controller_add(CONSOLE_ENTRY_SYSTEM,"Demo: simulated save; no printer configuration changed."); return true;
    }
    if (!strcmp(action,"SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY=1 ACCEL=500\nTUNING_TOWER COMMAND=SET_PRESSURE_ADVANCE PARAMETER=ADVANCE START=0 FACTOR=.005") ||
        !strcmp(action,"SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY=1 ACCEL=500\nTUNING_TOWER COMMAND=SET_PRESSURE_ADVANCE PARAMETER=ADVANCE START=0 FACTOR=.020")) {
        cJSON *status=cJSON_Parse("{\"toolhead\":{\"square_corner_velocity\":1,\"max_accel\":500}}");
        if (!status) return false;
        motion_diagnostics_controller_merge_status(status); cJSON_Delete(status); return true;
    }
    if (is_session(&session,CALIBRATION_SESSION_CUSTOM) && demo_printer_command(action)) {
        console_controller_add(CONSOLE_ENTRY_RESPONSE,"// Custom calibration sample complete; SAVE_CONFIG to persist"); return true;
    }
    return false;
}
