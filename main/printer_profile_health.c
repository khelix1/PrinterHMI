#include "printer_profile_health.h"

#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "moonraker_config_controller.h"
#include "moonraker_probe.h"

static const char *TAG = "printer_health";

static volatile bool
    s_known[MOONRAKER_CONFIG_MAX_PROFILES];

static volatile bool
    s_online[MOONRAKER_CONFIG_MAX_PROFILES];

/*
 * A busy Moonraker can miss one background probe while its live connection
 * remains healthy. Require two consecutive failures before taking a printer
 * that was known online offline. First discovery and recovery stay immediate.
 */
#define PROFILE_HEALTH_FAILURE_THRESHOLD 2

static volatile uint8_t
    s_failures[MOONRAKER_CONFIG_MAX_PROFILES];

/* Per-profile endpoint fingerprints are runtime-only and contain no
 * credentials.  They prevent an active-profile selection from clearing
 * valid health for every unchanged card. */
static bool s_endpoint_configured[MOONRAKER_CONFIG_MAX_PROFILES];
static char s_endpoint_host[MOONRAKER_CONFIG_MAX_PROFILES]
                           [MOONRAKER_CONFIG_HOST_LENGTH];
static int s_endpoint_port[MOONRAKER_CONFIG_MAX_PROFILES];
static char s_printer_state[MOONRAKER_CONFIG_MAX_PROFILES]
                           [PRINTER_PROFILE_HEALTH_STATE_LENGTH];
/* Successful inactive snapshots only.  A failed request must never
 * refresh this timestamp or prolong a stale ONLINE presentation. */
static int64_t s_last_live_success_us[MOONRAKER_CONFIG_MAX_PROFILES];

static int s_next_profile = 0;
static uint32_t s_generation = 0;


static bool endpoint_matches(
    int index,
    const moonraker_profile_t *profile)
{
    return index >= 0 &&
        index < MOONRAKER_CONFIG_MAX_PROFILES &&
        profile &&
        profile->configured &&
        s_endpoint_configured[index] &&
        s_endpoint_port[index] == profile->port &&
        strcmp(s_endpoint_host[index], profile->host) == 0;
}

static void remember_endpoint(
    int index,
    const moonraker_profile_t *profile)
{
    bool configured = profile && profile->configured;

    s_endpoint_configured[index] = configured;
    s_endpoint_port[index] = configured ? profile->port : 0;
    strlcpy(
        s_endpoint_host[index],
        configured ? profile->host : "",
        sizeof(s_endpoint_host[index]));
}

void printer_profile_health_reconcile(void)
{
    for (int index = 0;
         index < MOONRAKER_CONFIG_MAX_PROFILES;
         ++index) {
        const moonraker_profile_t *profile =
            moonraker_config_profile(index);

        if (!endpoint_matches(index, profile)) {
            s_known[index] = false;
            s_online[index] = false;
            s_failures[index] = 0;
            s_printer_state[index][0] = '\0';
            __atomic_store_n(
                &s_last_live_success_us[index], 0, __ATOMIC_RELEASE);
        }

        remember_endpoint(index, profile);
    }

    s_generation = moonraker_config_generation();
}

void printer_profile_health_reset(void)
{
    memset((void *)s_known, 0, sizeof(s_known));
    memset((void *)s_online, 0, sizeof(s_online));
    memset((void *)s_failures, 0, sizeof(s_failures));
    memset(s_endpoint_configured, 0, sizeof(s_endpoint_configured));
    memset(s_endpoint_host, 0, sizeof(s_endpoint_host));
    memset(s_endpoint_port, 0, sizeof(s_endpoint_port));
    memset(s_printer_state, 0, sizeof(s_printer_state));
    memset(
        s_last_live_success_us, 0, sizeof(s_last_live_success_us));
    s_next_profile = 0;
    printer_profile_health_reconcile();
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
        printer_profile_health_reconcile();
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
        s_printer_state[index][0] = '\0';
        return;
    }

    char host[MOONRAKER_CONFIG_HOST_LENGTH];
    strlcpy(host, profile->host, sizeof(host));
    int port = profile->port;

    bool online = moonraker_probe_host(host, port);

    if (generation_before != moonraker_config_generation()) {
        return;
    }

    bool changed = false;

    if (online) {
        s_failures[index] = 0;
        changed =
            !s_known[index] ||
            !s_online[index];
        s_online[index] = true;
        s_known[index] = true;
    } else if (!s_known[index]) {
        /*
         * Initial discovery should not leave an unresponsive configured
         * printer in an unknown state.
         */
        s_failures[index] = 1;
        s_online[index] = false;
        s_known[index] = true;
        changed = true;
    } else if (s_online[index]) {
        if (s_failures[index] < UINT8_MAX) {
            ++s_failures[index];
        }

        if (s_failures[index] >=
            PROFILE_HEALTH_FAILURE_THRESHOLD) {
            s_online[index] = false;
            changed = true;
        } else {
            ESP_LOGW(
                TAG,
                "Profile %d %s:%d probe missed (%u/%u)",
                index + 1,
                host,
                port,
                (unsigned)s_failures[index],
                (unsigned)PROFILE_HEALTH_FAILURE_THRESHOLD);
        }
    }

    if (changed) {
        ESP_LOGI(
            TAG,
            "Profile %d %s:%d is %s",
            index + 1,
            host,
            port,
            s_online[index] ? "online" : "offline");
    }
}



void printer_profile_health_set_live_state(
    int profile_index,
    bool known,
    bool online,
    const char *printer_state)
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
    s_failures[profile_index] = 0;
    strlcpy(
        s_printer_state[profile_index],
        online && printer_state ? printer_state : "",
        sizeof(s_printer_state[profile_index]));

    if (changed && known) {
        ESP_LOGI(
            TAG,
            "Profile %d is %s",
            profile_index + 1,
            online ? "online" : "offline");
    }
}

void printer_profile_health_set(
    int profile_index,
    bool known,
    bool online)
{
    printer_profile_health_set_live_state(
        profile_index, known, online, NULL);
}

bool printer_profile_health_get_live_state(
    int profile_index,
    char *out,
    size_t out_size)
{
    if (!out || out_size == 0 ||
        profile_index < 0 ||
        profile_index >= MOONRAKER_CONFIG_MAX_PROFILES) {
        return false;
    }

    strlcpy(
        out,
        s_printer_state[profile_index],
        out_size);
    return out[0] != '\0';
}

bool printer_profile_health_live_state_fresh(
    int profile_index,
    int64_t maximum_age_us)
{
    if (profile_index < 0 ||
        profile_index >= MOONRAKER_CONFIG_MAX_PROFILES ||
        maximum_age_us < 0) {
        return false;
    }

    int64_t updated = __atomic_load_n(
        &s_last_live_success_us[profile_index], __ATOMIC_ACQUIRE);
    return updated > 0 &&
        esp_timer_get_time() - updated <= maximum_age_us;
}

/* The preview worker reports transport results through this hysteretic
 * path. A transient Hosted/LAN collision is not evidence that another
 * printer is offline. */
void printer_profile_health_report_live_state(
    int profile_index,
    bool online,
    const char *printer_state)
{
    if (profile_index < 0 ||
        profile_index >= MOONRAKER_CONFIG_MAX_PROFILES) {
        return;
    }

    bool changed = false;

    if (online) {
        changed = !s_known[profile_index] || !s_online[profile_index];
        s_known[profile_index] = true;
        s_online[profile_index] = true;
        s_failures[profile_index] = 0;
        strlcpy(
            s_printer_state[profile_index],
            printer_state ? printer_state : "",
            sizeof(s_printer_state[profile_index]));
        __atomic_store_n(
            &s_last_live_success_us[profile_index],
            esp_timer_get_time(),
            __ATOMIC_RELEASE);
    } else if (!s_known[profile_index]) {
        /* No successful observation exists yet, so initial discovery can
         * state OFFLINE directly. */
        s_known[profile_index] = true;
        s_online[profile_index] = false;
        s_failures[profile_index] = PROFILE_HEALTH_FAILURE_THRESHOLD;
        s_printer_state[profile_index][0] = '\0';
        __atomic_store_n(
            &s_last_live_success_us[profile_index], 0, __ATOMIC_RELEASE);
        changed = true;
    } else if (s_online[profile_index]) {
        if (s_failures[profile_index] < UINT8_MAX) {
            ++s_failures[profile_index];
        }

        if (s_failures[profile_index] >=
            PROFILE_HEALTH_FAILURE_THRESHOLD) {
            s_online[profile_index] = false;
            s_printer_state[profile_index][0] = '\0';
            __atomic_store_n(
                &s_last_live_success_us[profile_index],
                0, __ATOMIC_RELEASE);
            changed = true;
        } else {
            ESP_LOGW(
                TAG,
                "Profile %d preview probe missed (%u/%u)",
                profile_index + 1,
                (unsigned)s_failures[profile_index],
                (unsigned)PROFILE_HEALTH_FAILURE_THRESHOLD);
        }
    }

    if (changed) {
        ESP_LOGI(
            TAG,
            "Profile %d is %s",
            profile_index + 1,
            s_online[profile_index] ? "online" : "offline");
    }
}



/* Klipper can briefly report startup after its Moonraker endpoint is
 * reachable. This is neither a live printer nor a confirmed fault. */
void printer_profile_health_set_verifying(int profile_index)
{
    if (profile_index < 0 ||
        profile_index >= MOONRAKER_CONFIG_MAX_PROFILES) {
        return;
    }

    s_known[profile_index] = false;
    s_online[profile_index] = false;
    s_failures[profile_index] = 0;
    s_printer_state[profile_index][0] = '\0';
    __atomic_store_n(
        &s_last_live_success_us[profile_index], 0, __ATOMIC_RELEASE);
}

int printer_profile_health_take_next_index(void)
{
    uint32_t generation = moonraker_config_generation();

    if (generation != s_generation) {
        printer_profile_health_reconcile();
    }

    /* Empty slots do not consume a polling turn. This makes the actual
     * configured profile set refresh as often as the one-second worker
     * permits, with no new concurrent network work. */
    for (int attempt = 0;
         attempt < MOONRAKER_CONFIG_MAX_PROFILES;
         ++attempt) {
        int index = s_next_profile;
        s_next_profile =
            (s_next_profile + 1) % MOONRAKER_CONFIG_MAX_PROFILES;

        const moonraker_profile_t *profile =
            moonraker_config_profile(index);
        if (profile && profile->configured) {
            return index;
        }
    }

    return -1;
}
