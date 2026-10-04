#!/usr/bin/env python3
"""Host checks for Tools action gates and calibration ownership (requires cc)."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    start = source.index('static ', source.index(name) - 30)
    brace = source.index('{', source.index(name))
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


with tempfile.TemporaryDirectory(prefix='printerhmi-tools-test-') as directory:
    tmp = Path(directory)
    (tmp / 'freertos').mkdir()
    stubs = {
        'esp_heap_caps.h': '''#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_INTERNAL 4
#define heap_caps_calloc(n,s,c) calloc(n,s)
''',
        'esp_log.h': '''#define ESP_LOGI(...) ((void)TAG)
#define ESP_LOGW(...) ((void)TAG)
#define ESP_LOGE(...) ((void)TAG)
''',
        'esp_timer.h': '#include <stdint.h>\nint64_t esp_timer_get_time(void);\n',
        'freertos/FreeRTOS.h': '''typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
''',
    }
    for name, contents in stubs.items():
        (tmp / name).write_text(contents)
    (tmp / 'session.c').write_text(r'''
#include <assert.h>
#include <string.h>
#include "calibration_session_controller.h"
#include "console_controller.h"
static int profile;
static uint32_t config_generation = 1;
static int64_t now;
static console_entry_t entry;
static size_t entries;
int moonraker_config_active_profile_index(void) { return profile; }
uint32_t moonraker_config_generation(void) { return config_generation; }
int64_t esp_timer_get_time(void) { return now; }
size_t console_controller_count(void) { return entries; }
bool console_controller_get(size_t index, console_entry_t *out) {
    if (index >= entries) return false;
    *out = entry;
    return true;
}
static calibration_session_snapshot_t snapshot(void) {
    calibration_session_snapshot_t out;
    calibration_session_controller_snapshot(&out);
    return out;
}
static void response(const char *text) {
    ++entry.sequence;
    entry.type = CONSOLE_ENTRY_RESPONSE;
    snprintf(entry.message, sizeof(entry.message), "%s", text);
    entries = 1;
    calibration_session_controller_poll();
}
static void begin(calibration_session_kind_t kind) {
    calibration_session_controller_begin(kind, entry.sequence);
}
int main(void) {
    assert(calibration_session_controller_init());
    begin(CALIBRATION_SESSION_PID);
    response("PID parameters: SAVE_CONFIG to persist");
    assert(snapshot().completed && snapshot().save_available);
    now += 3600LL * 1000000;
    assert(snapshot().save_available); /* Completed results have no waiting deadline. */
    profile = 1;
    /* No poll: Save's snapshot itself must reject the old owner. */
    assert(snapshot().status == CALIBRATION_SESSION_ERROR);
    assert(!snapshot().save_available);
    begin(CALIBRATION_SESSION_PID);
    response("PID parameters: SAVE_CONFIG");
    ++config_generation; /* Same profile index, edited endpoint. */
    assert(!snapshot().save_available);
    begin(CALIBRATION_SESSION_ACCELEROMETER_CHECK);
    now += 119LL * 1000000;
    assert(snapshot().status == CALIBRATION_SESSION_WAITING);
    now += 1000000;
    assert(snapshot().status == CALIBRATION_SESSION_ERROR);
    assert(strstr(snapshot().results, "not been cancelled"));
    response("SAVE_CONFIG"); /* Late results cannot revive a timed-out session. */
    assert(snapshot().status == CALIBRATION_SESSION_ERROR);
    begin(CALIBRATION_SESSION_PID);
    response("PID parameters: partial result");
    assert(snapshot().status == CALIBRATION_SESSION_RESULTS);
    now += 1799LL * 1000000;
    assert(snapshot().status == CALIBRATION_SESSION_RESULTS);
    now += 1000000;
    assert(snapshot().status == CALIBRATION_SESSION_ERROR);
    begin(CALIBRATION_SESSION_ACCELEROMETER_CHECK);
    response("axes noise: 0.01, 0.02, 0.03");
    now += 120LL * 1000000;
    assert(snapshot().completed && !snapshot().save_available);
    begin(CALIBRATION_SESSION_PROBE_ACCURACY);
    response("SAVE_CONFIG"); /* Other calibration output cannot unlock persistence. */
    assert(snapshot().status == CALIBRATION_SESSION_WAITING);
    response("probe accuracy results: maximum 2.51, minimum 2.50, range 0.01, average 2.505, median 2.505, standard deviation 0.005");
    assert(snapshot().completed && !snapshot().save_available);
    begin(CALIBRATION_SESSION_PROBE_ACCURACY);
    now += 300LL * 1000000;
    assert(snapshot().status == CALIBRATION_SESSION_ERROR);
    begin(CALIBRATION_SESSION_SCREWS_TILT);
    now += 599LL * 1000000;
    assert(snapshot().status == CALIBRATION_SESSION_WAITING);
    now += 1000000;
    assert(snapshot().status == CALIBRATION_SESSION_ERROR);
    begin(CALIBRATION_SESSION_SCREWS_TILT);
    response("front: adjust CW 00:10");
    assert(snapshot().completed && !snapshot().save_available);
    calibration_session_controller_reset();
    assert(snapshot().status == CALIBRATION_SESSION_IDLE);
    return 0;
}
'''.replace('#include <string.h>', '#include <string.h>\n#include <stdio.h>'))
    main = (ROOT / 'main/main.c').read_text()
    profiles = (ROOT / 'main/ui_bed_mesh_profiles.c').read_text()
    (tmp / 'actions.c').write_text(r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { bool moonraker_ok, live_data_ok; char printer_state[32]; } moonraker_state_t;
typedef bool (*ui_bed_mesh_profiles_command_cb_t)(const char *);
static moonraker_state_t state = {true, true, "ready"};
static bool s_got_ip = true, accepted = true;
static uint32_t s_bed_mesh_owner_generation = 1, generation = 1;
static int sends, recorded;
static char sent[256];
static struct { const char *pending_profile_name; ui_bed_mesh_profiles_command_cb_t command; } s;
#define UI_STATUS_DANGER 0
static uint32_t moonraker_config_generation(void) { return generation; }
static void moonraker_state_snapshot(moonraker_state_t *out) { *out = state; }
static void ui_toast_show(int status, const char *title, const char *detail) {
    (void)status; (void)title; (void)detail;
}
static void console_controller_add_command(const char *cmd) { (void)cmd; ++recorded; }
static bool moonraker_send_gcode(const char *cmd) {
    ++sends; snprintf(sent, sizeof(sent), "%s", cmd); return accepted;
}
static void close_profile_popup_cb(void *event) { (void)event; }
''' + function(main, 'bed_mesh_send_gcode_bridge') + '\n' +
        function(profiles, 'confirmed_profile_command') + r'''
int main(void) {
    const char *blocked[] = {"printing", "paused", "error", "shutdown"};
    for (unsigned i = 0; i < 4; ++i) {
        snprintf(state.printer_state, sizeof(state.printer_state), "%s", blocked[i]);
        assert(!bed_mesh_send_gcode_bridge("BED_MESH_CALIBRATE"));
    }
    strcpy(state.printer_state, "ready");
    s_got_ip = false; assert(!bed_mesh_send_gcode_bridge("test")); s_got_ip = true;
    state.moonraker_ok = false; assert(!bed_mesh_send_gcode_bridge("test")); state.moonraker_ok = true;
    state.live_data_ok = false; assert(!bed_mesh_send_gcode_bridge("test")); state.live_data_ok = true;
    ++generation; assert(!bed_mesh_send_gcode_bridge("test")); --generation;
    assert(sends == 0 && recorded == 0);
    s.command = bed_mesh_send_gcode_bridge; s.pending_profile_name = "default";
    confirmed_profile_command("SAVE");
    assert(sends == 1 && !strcmp(sent, "BED_MESH_PROFILE SAVE=default\nSAVE_CONFIG"));
    confirmed_profile_command("REMOVE");
    assert(sends == 2 && !strcmp(sent, "BED_MESH_PROFILE REMOVE=default\nSAVE_CONFIG"));
    strcpy(state.printer_state, "printing");
    confirmed_profile_command("REMOVE"); /* State changed with confirmation open. */
    assert(sends == 2);
    strcpy(state.printer_state, "ready"); accepted = false;
    assert(!bed_mesh_send_gcode_bridge("BED_MESH_CALIBRATE"));
    assert(sends == 3);
    return 0;
}
''')
    for name, sources in (
        ('session', [ROOT / 'main/calibration_session_controller.c', tmp / 'session.c']),
        ('actions', [tmp / 'actions.c']),
    ):
        executable = tmp / name
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-I', str(tmp), '-I', str(ROOT / 'main'),
                        *map(str, sources), '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
        print(f'PASS: {name}')
