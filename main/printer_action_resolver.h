#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRINTER_ACTION_COMMAND_MAX 640

typedef enum {
    PRINTER_ACTION_CONFIRM_NONE = 0,
    PRINTER_ACTION_CONFIRM_MOTION,
    PRINTER_ACTION_CONFIRM_HEAT,
    PRINTER_ACTION_CONFIRM_DESTRUCTIVE,
    PRINTER_ACTION_CONFIRM_UNKNOWN_MACRO,
} printer_action_confirmation_t;

typedef struct {
    char command[PRINTER_ACTION_COMMAND_MAX];
    bool macro_used;
    printer_action_confirmation_t confirmation;
} printer_action_resolution_t;

/*
 * Resolve one semantic HMI command. A detected public printer macro is
 * preferred when a known workflow alias exists. Unknown and parameterized
 * commands pass through unchanged.
 */
bool printer_action_resolver_resolve(
    const char *requested,
    printer_action_resolution_t *output);

#ifdef __cplusplus
}
#endif
