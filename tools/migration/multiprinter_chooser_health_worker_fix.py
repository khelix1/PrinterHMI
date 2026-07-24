#!/usr/bin/env python3
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "main"


def read(name: str) -> str:
    path = MAIN / name
    if not path.exists():
        raise RuntimeError(f"missing required file: {path}")
    return path.read_text()


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


health_h = r'''#pragma once

#include <stdbool.h>

/*
 * Lightweight multi-printer health cache.
 *
 * The existing application runtime worker calls poll_one(). No additional
 * FreeRTOS task, timer, mutex, or scheduler allocation is introduced.
 */
void printer_profile_health_poll_one(void);
void printer_profile_health_reset(void);

bool printer_profile_health_get(
    int profile_index,
    bool *known_out);
'''


health_c = r'''#include "printer_profile_health.h"

#include <stdint.h>
#include <string.h>

#include "esp_log.h"

#include "moonraker_config_controller.h"
#include "moonraker_probe.h"

static const char *TAG = "printer_health";

static volatile bool
    s_known[MOONRAKER_CONFIG_MAX_PROFILES];

static volatile bool
    s_online[MOONRAKER_CONFIG_MAX_PROFILES];

static int s_next_profile = 0;
static uint32_t s_generation = 0;


void printer_profile_health_reset(void)
{
    memset((void *)s_known, 0, sizeof(s_known));
    memset((void *)s_online, 0, sizeof(s_online));
    s_next_profile = 0;
    s_generation = moonraker_config_generation();
}


bool printer_profile_health_get(
    int profile_index,
    bool *known_out)
{
    if (known_out) *known_out = false;

    if (profile_index < 0 ||
        profile_index >= MOONRAKER_CONFIG_MAX_PROFILES) {
        return false;
    }

    if (known_out) {
        *known_out = s_known[profile_index];
    }

    return s_online[profile_index];
}


void printer_profile_health_poll_one(void)
{
    uint32_t generation_before = moonraker_config_generation();

    if (generation_before != s_generation) {
        printer_profile_health_reset();
        generation_before = s_generation;
    }

    int index = s_next_profile;
    s_next_profile =
        (s_next_profile + 1) % MOONRAKER_CONFIG_MAX_PROFILES;

    const moonraker_profile_t *profile =
        moonraker_config_profile(index);

    if (!profile || !profile->configured) {
        s_online[index] = false;
        s_known[index] = true;
        return;
    }

    char host[MOONRAKER_CONFIG_HOST_LENGTH];
    strlcpy(host, profile->host, sizeof(host));
    int port = profile->port;

    bool online = moonraker_probe_host(host, port);

    if (generation_before != moonraker_config_generation()) {
        return;
    }

    bool changed =
        !s_known[index] ||
        s_online[index] != online;

    s_online[index] = online;
    s_known[index] = true;

    if (changed) {
        ESP_LOGI(
            TAG,
            "Profile %d %s:%d is %s",
            index + 1,
            host,
            port,
            online ? "online" : "offline");
    }
}
'''


chooser = read("ui_printer_chooser_v32.c")

chooser = chooser.replace(
    '#include "freertos/FreeRTOS.h"\n'
    '#include "freertos/task.h"\n\n',
    '')

if '#include "printer_profile_health.h"' not in chooser:
    chooser = replace_once(
        chooser,
        '#include "moonraker_probe.h"\n',
        '#include "moonraker_probe.h"\n'
        '#include "printer_profile_health.h"\n',
        "Moonraker probe include")

chooser = chooser.replace(
    '#define CHOOSER_PROBE_PERIOD_TICKS 20\n\n',
    '')

chooser, target_count = re.subn(
    r'typedef struct \{\n'
    r'    bool configured;\n'
    r'    char host\[MOONRAKER_CONFIG_HOST_LENGTH\];\n'
    r'    int port;\n'
    r'\} chooser_probe_target_t;\n\n',
    '',
    chooser,
    count=1)

if target_count != 1:
    raise RuntimeError(
        f"expected one chooser probe-target type, found {target_count}")

globals_pattern = re.compile(
    r'static chooser_probe_target_t\n'
    r'    s_probe_targets\[MOONRAKER_CONFIG_MAX_PROFILES\];\n\n'
    r'static bool s_probe_online\[MOONRAKER_CONFIG_MAX_PROFILES\];\n'
    r'static bool s_displayed_online\[MOONRAKER_CONFIG_MAX_PROFILES\];\n'
    r'static bool s_probe_running = false;\n'
    r'static bool s_probe_ready = false;\n'
    r'static uint32_t s_probe_generation = 0;\n'
    r'static uint32_t s_probe_ticks = CHOOSER_PROBE_PERIOD_TICKS;\n'
    r'static portMUX_TYPE s_probe_lock = portMUX_INITIALIZER_UNLOCKED;\n\n')

chooser, globals_count = globals_pattern.subn('', chooser, count=1)

if globals_count != 1:
    raise RuntimeError(
        f"expected one chooser FreeRTOS probe-global block, found {globals_count}")

online_old = r'''        bool configured = profile && profile->configured;
        bool online = configured && s_displayed_online[index];

        if (configured && index == active && state && state->moonraker_ok) {
            online = true;
        }
'''

online_new = r'''        bool configured = profile && profile->configured;
        bool known = false;
        bool online =
            configured &&
            printer_profile_health_get(index, &known);

        if (configured && index == active && state && state->moonraker_ok) {
            online = true;
            known = true;
        }
'''

chooser = replace_once(
    chooser,
    online_old,
    online_new,
    "chooser cached-online selection")

status_old = r'''        const char *status_text = online ? "ONLINE" : "OFFLINE";
'''

status_new = r'''        const char *status_text =
            !known
                ? "CHECKING..."
                : (online ? "ONLINE" : "OFFLINE");
'''

chooser = replace_once(
    chooser,
    status_old,
    status_new,
    "chooser status text")

worker_block = re.compile(
    r'\nstatic void probe_task\(void \*argument\)\n'
    r'.*?'
    r'\nstatic void chooser_timer_cb\(lv_timer_t \*timer\)\n'
    r'\{.*?\n\}\n',
    flags=re.S)

worker_replacement = r'''
static void chooser_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_cards();
}
'''

chooser, worker_count = worker_block.subn(
    '\n' + worker_replacement,
    chooser,
    count=1)

if worker_count != 1:
    raise RuntimeError(
        f"expected one chooser FreeRTOS worker block, found {worker_count}")

chooser = chooser.replace(
    '    s_probe_ticks = CHOOSER_PROBE_PERIOD_TICKS;\n',
    '')

chooser = chooser.replace(
    '    chooser_timer_cb(s_timer);\n',
    '    chooser_timer_cb(s_timer);\n\n'
    '    ESP_LOGI("printer_chooser", "Chooser visible");\n')

if '#include "esp_log.h"' not in chooser:
    chooser = replace_once(
        chooser,
        '#include <string.h>\n',
        '#include <string.h>\n\n#include "esp_log.h"\n',
        "chooser standard includes")


cmake = read("CMakeLists.txt")
if '"printer_profile_health.c"' not in cmake:
    cmake = replace_once(
        cmake,
        '        "ui_printer_chooser_v32.c"\n',
        '        "ui_printer_chooser_v32.c"\n'
        '        "printer_profile_health.c"\n',
        "chooser CMake registration")


main = read("main.c")

if '#include "printer_profile_health.h"' not in main:
    main = replace_once(
        main,
        '#include "ui_printer_chooser_v32.h"\n',
        '#include "ui_printer_chooser_v32.h"\n'
        '#include "printer_profile_health.h"\n',
        "chooser include")

poll_old = r'''        ESP_LOGI(TAG, "RUNTIME_LOOP: before moonraker");
        moonraker_live_poll_tasklet();
        ESP_LOGI(TAG, "RUNTIME_LOOP: after moonraker");
'''

poll_new = r'''        ESP_LOGI(TAG, "RUNTIME_LOOP: before moonraker");
        moonraker_live_poll_tasklet();
        printer_profile_health_poll_one();
        ESP_LOGI(TAG, "RUNTIME_LOOP: after moonraker");
'''

if "printer_profile_health_poll_one();" not in main:
    main = replace_once(
        main,
        poll_old,
        poll_new,
        "runtime Moonraker poll block")

bridge_old = r'''    ui_printer_chooser_v32_refresh();

    char status[128];
'''

bridge_new = r'''    printer_profile_health_reset();
    ui_printer_chooser_v32_refresh();

    char status[128];
'''

if "printer_profile_health_reset();" not in main:
    main = replace_once(
        main,
        bridge_old,
        bridge_new,
        "active-profile health reset")


# Validate that no new scheduler/task primitive remains in the chooser.
for forbidden in (
    "xTaskCreate(",
    "vTaskDelete(",
    "portENTER_CRITICAL",
    "portEXIT_CRITICAL",
    "portMUX_TYPE",
):
    if forbidden in chooser:
        raise RuntimeError(
            f"chooser still contains forbidden scheduler primitive: {forbidden}")


(MAIN / "printer_profile_health.h").write_text(health_h)
(MAIN / "printer_profile_health.c").write_text(health_c)
(MAIN / "ui_printer_chooser_v32.c").write_text(chooser)
(MAIN / "CMakeLists.txt").write_text(cmake)
(MAIN / "main.c").write_text(main)

print("PASS: chooser health checks moved onto existing runtime worker")
print("  - no new FreeRTOS task")
print("  - no chooser mutex or scheduler allocation")
print("  - one lightweight profile probe per existing runtime loop")
print("  - chooser logs 'Chooser visible' when shown")
print("Next: idf.py build")
