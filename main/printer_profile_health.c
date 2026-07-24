#include "printer_profile_health.h"

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



void printer_profile_health_set(
    int profile_index,
    bool known,
    bool online)
{
    if (profile_index < 0 ||
        profile_index >= MOONRAKER_CONFIG_MAX_PROFILES) {
        return;
    }

    bool changed =
        s_known[profile_index] != known ||
        (known && s_online[profile_index] != online);

    s_known[profile_index] = known;
    s_online[profile_index] = online;

    if (changed && known) {
        ESP_LOGI(
            TAG,
            "Profile %d is %s",
            profile_index + 1,
            online ? "online" : "offline");
    }
}



int printer_profile_health_take_next_index(void)
{
    uint32_t generation = moonraker_config_generation();

    if (generation != s_generation) {
        printer_profile_health_reset();
    }

    int index = s_next_profile;
    s_next_profile =
        (s_next_profile + 1) % MOONRAKER_CONFIG_MAX_PROFILES;

    return index;
}
