#!/usr/bin/env python3
"""Exercise real Console ingestion and calibration results with ASan/UBSan."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--console-source', type=Path, default=root / 'main/console_controller.c')
parser.add_argument('--session-source', type=Path, default=root / 'main/calibration_session_controller.c')
args = parser.parse_args()
cjson = root / 'tools/theme-maker/linux/native/vendor/cjson'
headers = {
    'esp_heap_caps.h': '#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_INTERNAL 2\n#define MALLOC_CAP_8BIT 4\n#define heap_caps_calloc(n,s,c) calloc(n,s)\n',
    'esp_log.h': '#pragma once\n#define ESP_LOGI(tag,...) ((void)(tag))\n#define ESP_LOGE(tag,...) ((void)(tag))\n#define ESP_LOGW(tag,...) ((void)(tag))\n',
    'esp_timer.h': '#pragma once\n#include <stdint.h>\nint64_t esp_timer_get_time(void);\n',
    'freertos/FreeRTOS.h': '#pragma once\ntypedef int portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(lock) ((void)(lock))\n#define portEXIT_CRITICAL(lock) ((void)(lock))\n',
}
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "console_controller.h"
#include "calibration_session_controller.h"
static int64_t now;
int64_t esp_timer_get_time(void) { return now; }
int moonraker_config_active_profile_index(void) { return 0; }
uint32_t moonraker_config_generation(void) { return 1; }
static calibration_session_snapshot_t snapshot(void) {
    calibration_session_snapshot_t result;
    calibration_session_controller_snapshot(&result);
    return result;
}
static void begin(calibration_session_kind_t kind) {
    calibration_session_controller_begin(kind, console_controller_latest_sequence());
}
static void post(const char *line, console_entry_type_t type) {
    cJSON *document = cJSON_CreateObject();
    assert(document);
    assert(cJSON_AddStringToObject(document, "method", "notify_gcode_response"));
    cJSON *params = cJSON_AddArrayToObject(document, "params");
    assert(params && cJSON_AddItemToArray(params, cJSON_CreateString(line)));
    char *json = cJSON_PrintUnformatted(document);
    assert(json && console_controller_ingest_websocket_json(json, strlen(json)));
    free(json);
    cJSON_Delete(document);
    console_entry_t entry;
    assert(console_controller_get(0, &entry));
    assert(entry.type == type);
    assert(!strcmp(entry.message, line));
    calibration_session_controller_poll();
}
int main(void) {
    assert(console_controller_init() && calibration_session_controller_init());
    console_controller_clear();
    begin(CALIBRATION_SESSION_PID);
    post("// SAVE_CONFIG: Overwrite config file and restart", CONSOLE_ENTRY_RESPONSE);
    assert(snapshot().status == CALIBRATION_SESSION_WAITING && !snapshot().save_available);
    post("// SAVE_CONFIG : Overwrite config file and restart", CONSOLE_ENTRY_RESPONSE);
    assert(!snapshot().completed && !snapshot().save_available);
    post("// PID parameters: error estimate 0.01", CONSOLE_ENTRY_RESPONSE);
    assert(snapshot().status == CALIBRATION_SESSION_RESULTS && !snapshot().completed);
    post("// echo: no errors found; failed tests: 0", CONSOLE_ENTRY_RESPONSE);
    post("// echo: literal !! in a macro message", CONSOLE_ENTRY_RESPONSE);
    post("// warning_threshold=10", CONSOLE_ENTRY_RESPONSE);
    post("// firmware_shutdown_timeout=30", CONSOLE_ENTRY_RESPONSE);
    post("// PID parameters: warning threshold 0.1", CONSOLE_ENTRY_RESPONSE);
    assert(snapshot().status != CALIBRATION_SESSION_ERROR);
    post(" // WARNING: PID parameters unavailable; do not use SAVE_CONFIG", CONSOLE_ENTRY_WARNING);
    assert(!snapshot().completed && !snapshot().save_available);
    post("// WARN: PID parameters unavailable", CONSOLE_ENTRY_WARNING);
    assert(!snapshot().save_available);
    post("// PID parameters: pid_Kp=22.2 pid_Ki=1.08 pid_Kd=114.0", CONSOLE_ENTRY_RESPONSE);
    assert(!snapshot().completed);
    post("// The SAVE_CONFIG command will update the printer config file", CONSOLE_ENTRY_RESPONSE);
    assert(snapshot().completed && snapshot().save_available);
    /* Macro identifiers containing the command name are not save evidence. */
    const char *unrelated[] = {"// SAVE_CONFIGURE", "// MY_SAVE_CONFIG", "// SAVE_CONFIG_BACKUP"};
    begin(CALIBRATION_SESSION_CUSTOM);
    for (size_t i=0; i<sizeof(unrelated)/sizeof(unrelated[0]); ++i) {
        post(unrelated[i], CONSOLE_ENTRY_RESPONSE);
        assert(snapshot().status == CALIBRATION_SESSION_WAITING && !snapshot().save_available);
    }
    post("// Custom calibration sample complete; 'save_config' to persist", CONSOLE_ENTRY_RESPONSE);
    assert(snapshot().completed && snapshot().save_available);
    const calibration_session_kind_t diagnostics[] = {
        CALIBRATION_SESSION_RESONANCE_TEST, CALIBRATION_SESSION_ACCELEROMETER_CHECK
    };
    for (size_t i = 0; i < sizeof(diagnostics) / sizeof(diagnostics[0]); ++i) {
        begin(diagnostics[i]);
        post("// The SAVE_CONFIG command will update the printer config file", CONSOLE_ENTRY_RESPONSE);
        assert(!snapshot().completed && !snapshot().save_available);
    }
    post("// Axes noise for xy-axis accelerometer: 2.0, 3.0, 4.0", CONSOLE_ENTRY_RESPONSE);
    assert(snapshot().completed && !snapshot().save_available);
    /* A successful TEST_RESONANCES writes its final CSV, without SAVE_CONFIG. */
    begin(CALIBRATION_SESSION_RESONANCE_TEST);
    post("// Testing frequency 20 Hz", CONSOLE_ENTRY_RESPONSE);
    assert(!snapshot().completed);
    post("// resonance test running", CONSOLE_ENTRY_RESPONSE);
    assert(snapshot().status == CALIBRATION_SESSION_RESULTS && !snapshot().completed);
    post("// Warning: Resonances data written to /tmp/partial.csv file", CONSOLE_ENTRY_WARNING);
    assert(!snapshot().completed && !snapshot().save_available);
    post("// Resonances data written to /tmp/resonances_x_20261108.csv file", CONSOLE_ENTRY_RESPONSE);
    assert(snapshot().completed && !snapshot().save_available);
    now += 31LL * 60 * 1000000;
    assert(snapshot().status == CALIBRATION_SESSION_RESULTS && snapshot().completed);
    begin(CALIBRATION_SESSION_RESONANCE_TEST);
    post("// Resonances data written to ", CONSOLE_ENTRY_RESPONSE);
    assert(!snapshot().completed);
    now += 31LL * 60 * 1000000;
    assert(snapshot().status == CALIBRATION_SESSION_ERROR);
    /* Explicit errors still fail immediately, regardless of their wording. */
    const char *errors[] = {"!! Arbitrary command rejection", "// Error: SAVE_CONFIG unavailable",
        "// Unknown command:\"CALIBRATE_SAMPLE\"", "Invalid command: test",
        "Must home axis first", "Failed to read sensor", "Shutdown due to fault",
        "// Klipper state: Shutdown"};
    for (size_t i=0; i<sizeof(errors)/sizeof(errors[0]); ++i) {
        begin(CALIBRATION_SESSION_PID);
        post(errors[i], CONSOLE_ENTRY_ERROR);
        assert(snapshot().status == CALIBRATION_SESSION_ERROR && !snapshot().save_available);
    }
    begin(CALIBRATION_SESSION_PROBE_ACCURACY);
    post("// Warning: probe accuracy results: incomplete sample", CONSOLE_ENTRY_WARNING);
    assert(!snapshot().completed && !snapshot().save_available);
    post("// probe accuracy results: maximum 2.51, minimum 2.50, range 0.01, average 2.505", CONSOLE_ENTRY_RESPONSE);
    assert(snapshot().completed && !snapshot().save_available);
    puts("PASS: real Console/calibration pipeline: severity prefixes, warning save gates, command-token evidence, resonance completion and timeouts");
}
'''
with tempfile.TemporaryDirectory(prefix='calibration-console-') as directory:
    tmp = Path(directory)
    for name, content in headers.items():
        path = tmp / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    (tmp / 'test.c').write_text(fixture)
    subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                    '-I', str(tmp), '-I', str(root / 'main'), '-I', str(cjson),
                    str(tmp / 'test.c'), str(args.console_source), str(args.session_source),
                    str(cjson / 'cJSON.c'), '-o', str(tmp / 'test')], check=True)
    subprocess.run([str(tmp / 'test')], check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
