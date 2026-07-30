#include "printer_action_resolver.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "macro_controller.h"


typedef struct {
    const char *canonical;
    const char *aliases[5];
    printer_action_confirmation_t confirmation;
} action_aliases_t;


static const action_aliases_t ACTIONS[] = {
    {
        "Z_TILT_ADJUST",
        {"Z_TILT", "Z_TILT_CALIBRATE", "CALIBRATE_Z_TILT", NULL},
        PRINTER_ACTION_CONFIRM_MOTION,
    },
    {
        "QUAD_GANTRY_LEVEL",
        {"QGL", "GANTRY_LEVEL", "QUAD_GANTRY_LEVEL", NULL},
        PRINTER_ACTION_CONFIRM_MOTION,
    },
    {
        "SCREWS_TILT_CALCULATE",
        {"SCREWS_TILT", "LEVEL_SCREWS", "SCREWS_TILT_CALCULATE", NULL},
        PRINTER_ACTION_CONFIRM_MOTION,
    },
    {
        "BED_MESH_CALIBRATE",
        {"BED_MESH_CALIBRATE", "CALIBRATE_BED", NULL},
        PRINTER_ACTION_CONFIRM_MOTION,
    },
    {
        "PROBE_CALIBRATE",
        {"PROBE_CALIBRATE", "Z_OFFSET_CALIBRATE", "CALIBRATE_Z_OFFSET", NULL},
        PRINTER_ACTION_CONFIRM_MOTION,
    },
    {
        "SHAPER_CALIBRATE",
        {"SHAPER_CALIBRATE", "INPUT_SHAPER_CALIBRATE", NULL},
        PRINTER_ACTION_CONFIRM_MOTION,
    },
    {
        "M600",
        {"M600", "FILAMENT_CHANGE", "CHANGE_FILAMENT", NULL},
        PRINTER_ACTION_CONFIRM_MOTION,
    },
    {
        "LOAD_FILAMENT",
        {"LOAD_FILAMENT", NULL},
        PRINTER_ACTION_CONFIRM_HEAT,
    },
    {
        "UNLOAD_FILAMENT",
        {"UNLOAD_FILAMENT", NULL},
        PRINTER_ACTION_CONFIRM_HEAT,
    },
    {
        "PAUSE",
        {"PAUSE", NULL},
        PRINTER_ACTION_CONFIRM_NONE,
    },
    {
        "RESUME",
        {"RESUME", NULL},
        PRINTER_ACTION_CONFIRM_NONE,
    },
    {
        "CANCEL_PRINT",
        {"CANCEL_PRINT", NULL},
        PRINTER_ACTION_CONFIRM_DESTRUCTIVE,
    },
    {
        "G28",
        {"G28", "HOME_ALL", NULL},
        PRINTER_ACTION_CONFIRM_NONE,
    },
    {
        "HOME_ALL",
        {"HOME_ALL", "G28", NULL},
        PRINTER_ACTION_CONFIRM_NONE,
    },
    {
        "SAVE_CONFIG",
        {"SAVE_CONFIG", NULL},
        PRINTER_ACTION_CONFIRM_DESTRUCTIVE,
    },
    {
        "FIRMWARE_RESTART",
        {"FIRMWARE_RESTART", NULL},
        PRINTER_ACTION_CONFIRM_DESTRUCTIVE,
    },
    {
        "RESTART",
        {"RESTART", NULL},
        PRINTER_ACTION_CONFIRM_DESTRUCTIVE,
    },
};


static bool equal_case_insensitive(
    const char *left,
    const char *right)
{
    if (!left || !right) {
        return false;
    }

    while (*left && *right) {
        if (toupper((unsigned char)*left) !=
            toupper((unsigned char)*right)) {
            return false;
        }
        ++left;
        ++right;
    }

    return *left == '\0' && *right == '\0';
}


static bool find_macro(
    const char *candidate,
    char *output,
    size_t output_size)
{
    if (!candidate || !candidate[0] ||
        !output || output_size == 0) {
        return false;
    }

    macro_controller_status_t status;
    macro_controller_status(&status);

    for (size_t index = 0; index < status.count; ++index) {
        char name[MACRO_CONTROLLER_NAME_MAX];

        if (macro_controller_get(
                index,
                name,
                sizeof(name)) &&
            equal_case_insensitive(name, candidate)) {
            snprintf(
                output,
                output_size,
                "%s",
                name);
            return true;
        }
    }

    return false;
}


static bool canonical_script_matches(
    const char *requested,
    const char *canonical)
{
    if (equal_case_insensitive(requested, canonical)) {
        return true;
    }

    static const char HOME_PREFIX[] = "G28\n";
    size_t prefix_length = sizeof(HOME_PREFIX) - 1;

    return strncmp(
               requested,
               HOME_PREFIX,
               prefix_length) == 0 &&
        equal_case_insensitive(
            requested + prefix_length,
            canonical);
}


static bool is_single_command_name(const char *requested)
{
    if (!requested || !requested[0]) {
        return false;
    }

    for (const unsigned char *cursor =
             (const unsigned char *)requested;
         *cursor;
         ++cursor) {
        if (isspace(*cursor)) {
            return false;
        }
    }

    return true;
}


bool printer_action_resolver_resolve(
    const char *requested,
    printer_action_resolution_t *output)
{
    if (!requested || !requested[0] || !output) {
        return false;
    }

    memset(output, 0, sizeof(*output));
    snprintf(
        output->command,
        sizeof(output->command),
        "%s",
        requested);

    /*
     * A literal command that is itself a detected macro already has the
     * correct printer-specific behavior. Preserve its exact catalog name.
     */
    if (is_single_command_name(requested) &&
        find_macro(
            requested,
            output->command,
            sizeof(output->command))) {
        output->macro_used = true;
    }

    for (size_t action_index = 0;
         action_index < sizeof(ACTIONS) / sizeof(ACTIONS[0]);
         ++action_index) {
        const action_aliases_t *action =
            &ACTIONS[action_index];

        if (!canonical_script_matches(
                requested,
                action->canonical)) {
            continue;
        }

        output->confirmation =
            action->confirmation;

        for (size_t alias_index = 0;
             alias_index <
                 sizeof(action->aliases) /
                     sizeof(action->aliases[0]);
             ++alias_index) {
            const char *alias =
                action->aliases[alias_index];

            if (!alias) {
                break;
            }

            if (find_macro(
                    alias,
                    output->command,
                    sizeof(output->command))) {
                output->macro_used = true;
                return true;
            }
        }

        /*
         * HOME_ALL is an HMI semantic action, not a standard Klipper command.
         * Without a printer macro its proven fallback is G28.
         */
        if (equal_case_insensitive(
                action->canonical,
                "HOME_ALL")) {
            snprintf(
                output->command,
                sizeof(output->command),
                "G28");
        }

        /*
         * Otherwise keep the caller's standard Klipper fallback, including
         * an explicit G28 prefix when that workflow requested it.
         */
        return true;
    }

    if (output->macro_used) {
        output->confirmation =
            PRINTER_ACTION_CONFIRM_UNKNOWN_MACRO;
    }

    return true;
}
