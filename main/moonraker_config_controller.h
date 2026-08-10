#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MOONRAKER_CONFIG_MAX_PROFILES 4
#define MOONRAKER_CONFIG_NAME_LENGTH  32
#define MOONRAKER_CONFIG_HOST_LENGTH  64
#define MOONRAKER_CONFIG_API_KEY_LENGTH 160

typedef struct {
    bool configured;
    char name[MOONRAKER_CONFIG_NAME_LENGTH];
    char host[MOONRAKER_CONFIG_HOST_LENGTH];
    /* Optional Moonraker API key. Never exported in SD backups. */
    char api_key[MOONRAKER_CONFIG_API_KEY_LENGTH];
    int port;
    /* Opt-in HTTPS/WSS; false preserves current HTTP behavior. */
    bool secure_transport;
} moonraker_profile_t;

/*
 * Loads the persistent profile collection.
 *
 * Existing netcfg/moon_host and netcfg/moon_port values are migrated
 * automatically into profile 0 ("Printer 1").
 */
bool moonraker_config_load(void);

/*
 * Compatibility accessors used by all existing Moonraker consumers.
 * They resolve through the currently active profile.
 */
const char *moonraker_config_host(void);
int moonraker_config_port(void);
/* Empty when this profile uses Moonraker trusted-client access. */
const char *moonraker_config_api_key(void);

/*
 * Active-profile information.
 */
size_t moonraker_config_profile_capacity(void);
size_t moonraker_config_profile_count(void);
int moonraker_config_active_profile_index(void);
const char *moonraker_config_active_profile_name(void);

/*
 * Changes whenever the active endpoint can change.
 * Network transactions use this to reject stale responses.
 */
uint32_t moonraker_config_generation(void);

const moonraker_profile_t *moonraker_config_profile(
    int profile_index);

/*
 * Selects an existing configured profile.
 *
 * Only configuration ownership changes here. The application remains
 * responsible for clearing live state and reconnecting after selection.
 */
bool moonraker_config_select_profile(
    int profile_index);

/*
 * Creates or updates a profile. Runtime state changes only after the
 * complete profile collection has been committed to NVS.
 */
bool moonraker_config_save_profile(
    int profile_index,
    const char *name,
    const char *host,
    int port,
    const char *api_key);

/*
 * Deletes a profile. Deleting the final profile restores Printer 1
 * with the legacy default endpoint so an active configuration always exists.
 */
bool moonraker_config_delete_profile(
    int profile_index);

/*
 * Validates and atomically replaces the complete profile collection.
 * Used by versioned configuration restore.
 */
bool moonraker_config_replace_profiles(
    const moonraker_profile_t *profiles,
    size_t profile_capacity,
    int active_profile_index);

/*
 * Compatibility editing APIs. These update the active profile.
 */
bool moonraker_config_set_transport_security(int profile_index, bool secure);

bool moonraker_config_select_host(
    const char *host,
    int port);

bool moonraker_config_select_port(
    int port);
