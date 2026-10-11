#pragma once
#include "moonraker.h"
#include <string.h>

/* Emergency stop and firmware recovery deliberately do not use this policy. */
static inline const char *printer_connection_reason(const moonraker_state_t *state)
{
    if(!state || !state->moonraker_ok)return "Offline - reconnect to use controls.";
    if(!state->live_data_ok)return "Waiting for Klipper.";
    if(!strcmp(state->printer_state,"error") || !strcmp(state->printer_state,"shutdown"))return "Klipper error - clear to use controls.";
    if(!strcmp(state->printer_state,"--") || !strcmp(state->printer_state,"startup") || !strcmp(state->printer_state,"disconnected") || !strcmp(state->printer_state,"offline"))return "Waiting for Klipper.";
    return "";
}
static inline const char *printer_idle_reason(const moonraker_state_t *state)
{
    const char *reason=printer_connection_reason(state);if(reason[0])return reason;
    if(!strcmp(state->printer_state,"printing"))return "Printing - wait for completion.";
    if(!strcmp(state->printer_state,"paused"))return "Paused - resume or cancel first.";
    return "";
}
