#pragma once

#include <stdbool.h>
#include "print_estimates.h"

/*
 * Push the current synchronized Moonraker state into the Dashboard banner.
 *
 * moonraker_ok is retained as the application transport fallback used when
 * the synchronized printer state has not yet produced a visible status.
 */
void dashboard_live_controller_push_banner(bool moonraker_ok);
void dashboard_live_controller_push_banner_estimate(bool moonraker_ok,
    const print_estimate_result_t *estimate, uint32_t job_revision);

void dashboard_live_controller_push_machine(void);
