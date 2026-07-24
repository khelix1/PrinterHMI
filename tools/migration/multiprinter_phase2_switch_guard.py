from pathlib import Path

config_h_path = Path("main/moonraker_config_controller.h")
config_c_path = Path("main/moonraker_config_controller.c")
history_h_path = Path("main/telemetry_history.h")
history_c_path = Path("main/telemetry_history.c")
main_path = Path("main/main.c")

config_h = config_h_path.read_text()
config_c = config_c_path.read_text()
history_h = history_h_path.read_text()
history_c = history_c_path.read_text()
main = main_path.read_text()

# ------------------------------------------------------------
# Profile generation API
# ------------------------------------------------------------

old_include = """#include <stdbool.h>
#include <stddef.h>
"""

new_include = """#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
"""

old_generation_declaration_anchor = """const char *moonraker_config_active_profile_name(void);

"""

new_generation_declaration = """const char *moonraker_config_active_profile_name(void);

/*
 * Changes whenever the active endpoint can change.
 * Network transactions use this to reject stale responses.
 */
uint32_t moonraker_config_generation(void);

"""

for text, anchor, description in [
    (config_h, old_include, "controller includes"),
    (
        config_h,
        old_generation_declaration_anchor,
        "generation declaration anchor",
    ),
]:
    if text.count(anchor) != 1:
        raise RuntimeError(
            f"expected one {description}, found {text.count(anchor)}")

config_h = config_h.replace(
    old_include,
    new_include,
    1)

config_h = config_h.replace(
    old_generation_declaration_anchor,
    new_generation_declaration,
    1)

old_state = """static int s_active_profile = 0;
"""

new_state = """static int s_active_profile = 0;

/*
 * A 32-bit read/write is atomic on the ESP32-P4. Volatile ensures the
 * polling task observes profile changes made by the LVGL task.
 */
static volatile uint32_t s_configuration_generation = 1;


static void advance_configuration_generation(void)
{
    ++s_configuration_generation;

    if (s_configuration_generation == 0) {
        s_configuration_generation = 1;
    }
}
"""

if config_c.count(old_state) != 1:
    raise RuntimeError("could not locate profile controller state")

config_c = config_c.replace(
    old_state,
    new_state,
    1)

old_getter_anchor = """const char *moonraker_config_active_profile_name(void)
{
    return
        s_profiles[s_active_profile].name;
}


"""

new_getter = """const char *moonraker_config_active_profile_name(void)
{
    return
        s_profiles[s_active_profile].name;
}


uint32_t moonraker_config_generation(void)
{
    return s_configuration_generation;
}


"""

if config_c.count(old_getter_anchor) != 1:
    raise RuntimeError("could not locate active-profile name getter")

config_c = config_c.replace(
    old_getter_anchor,
    new_getter,
    1)

old_select_commit = """    if (!persist_collection()) {
        s_active_profile =
            previous_active;

        return false;
    }

    ESP_LOGI(
        TAG,
        "Selected profile %d: %s (%s:%d)",
"""

new_select_commit = """    if (!persist_collection()) {
        s_active_profile =
            previous_active;

        return false;
    }

    advance_configuration_generation();

    ESP_LOGI(
        TAG,
        "Selected profile %d: %s (%s:%d)",
"""

if config_c.count(old_select_commit) != 1:
    raise RuntimeError("could not locate profile-selection commit")

config_c = config_c.replace(
    old_select_commit,
    new_select_commit,
    1)

old_save_commit = """    if (!persist_collection()) {
        s_profiles[profile_index] =
            previous;

        return false;
    }

    ESP_LOGI(
        TAG,
        "Saved profile %d: %s (%s:%d)",
"""

new_save_commit = """    if (!persist_collection()) {
        s_profiles[profile_index] =
            previous;

        return false;
    }

    if (profile_index == s_active_profile) {
        advance_configuration_generation();
    }

    ESP_LOGI(
        TAG,
        "Saved profile %d: %s (%s:%d)",
"""

if config_c.count(old_save_commit) != 1:
    raise RuntimeError("could not locate profile-save commit")

config_c = config_c.replace(
    old_save_commit,
    new_save_commit,
    1)

old_delete_state = """    int previous_active =
        s_active_profile;

    clear_profile(
"""

new_delete_state = """    int previous_active =
        s_active_profile;

    bool active_profile_affected =
        profile_index == previous_active;

    clear_profile(
"""

if config_c.count(old_delete_state) != 1:
    raise RuntimeError("could not locate profile-delete state")

config_c = config_c.replace(
    old_delete_state,
    new_delete_state,
    1)

old_delete_commit = """        return false;
    }

    return true;
}


bool moonraker_config_select_host(
"""

new_delete_commit = """        return false;
    }

    if (active_profile_affected) {
        advance_configuration_generation();
    }

    return true;
}


bool moonraker_config_select_host(
"""

if config_c.count(old_delete_commit) != 1:
    raise RuntimeError("could not locate profile-delete commit")

config_c = config_c.replace(
    old_delete_commit,
    new_delete_commit,
    1)

# ------------------------------------------------------------
# Telemetry history reset
# ------------------------------------------------------------

history_declaration_anchor = """bool telemetry_history_init(void);

"""

history_declaration = """bool telemetry_history_init(void);

/*
 * Clears samples when the active printer changes so histories from
 * different machines are never combined.
 */
void telemetry_history_reset(void);

"""

if history_h.count(history_declaration_anchor) != 1:
    raise RuntimeError("could not locate telemetry declaration anchor")

history_h = history_h.replace(
    history_declaration_anchor,
    history_declaration,
    1)

history_source_anchor = """static bool telemetry_state_is_valid(
"""

history_reset = """void telemetry_history_reset(void)
{
    if (s_samples) {
        memset(
            s_samples,
            0,
            TELEMETRY_HISTORY_CAPACITY *
                sizeof(*s_samples));
    }

    s_head = 0;
    s_count = 0;
    s_last_sample_us = 0;

    ESP_LOGI(TAG, "History reset for active-printer change");
}


"""

if history_c.count(history_source_anchor) != 1:
    raise RuntimeError("could not locate telemetry source anchor")

history_c = history_c.replace(
    history_source_anchor,
    history_reset + history_source_anchor,
    1)

# ------------------------------------------------------------
# Reject responses from a profile that stopped being active while
# the synchronous HTTP request was in flight.
# ------------------------------------------------------------

old_fetch = """    int live_http_status = 0;

    bool transport_ok = moonraker_live_transport_fetch(
        moonraker_config_host(),
        moonraker_config_port(),
        MOONRAKER_API_KEY,
        s_moonraker_objects,
        sizeof(s_moonraker_objects),
        &live_http_status);

    s_moonraker_code = live_http_status;

    if (!transport_ok) {
"""

new_fetch = """    int live_http_status = 0;

    uint32_t request_generation =
        moonraker_config_generation();

    char request_host[
        MOONRAKER_CONFIG_HOST_LENGTH];

    safe_copy(
        request_host,
        sizeof(request_host),
        moonraker_config_host());

    int request_port =
        moonraker_config_port();

    bool transport_ok = moonraker_live_transport_fetch(
        request_host,
        request_port,
        MOONRAKER_API_KEY,
        s_moonraker_objects,
        sizeof(s_moonraker_objects),
        &live_http_status);

    if (request_generation !=
        moonraker_config_generation()) {
        ESP_LOGW(
            TAG,
            "Discarding stale Moonraker response from %s:%d",
            request_host,
            request_port);

        return false;
    }

    s_moonraker_code = live_http_status;

    if (!transport_ok) {
"""

if main.count(old_fetch) != 1:
    raise RuntimeError("could not locate live transport transaction")

main = main.replace(
    old_fetch,
    new_fetch,
    1)

config_h_path.write_text(config_h)
config_c_path.write_text(config_c)
history_h_path.write_text(history_h)
history_c_path.write_text(history_c)
main_path.write_text(main)

print("Installed multi-printer switch generation guard.")
print("In-flight responses from old profiles are now discarded.")
print("Telemetry history now has an explicit reset API.")
