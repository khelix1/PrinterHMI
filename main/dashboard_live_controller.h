#pragma once

#include <stdbool.h>

/*
 * Push the current synchronized Moonraker state into the Dashboard banner.
 *
 * moonraker_ok is retained as the application transport fallback used when
 * the synchronized printer state has not yet produced a visible status.
 */
void dashboard_live_controller_push_banner(bool moonraker_ok);

void dashboard_live_controller_push_machine(void);
