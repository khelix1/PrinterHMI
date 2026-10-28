#pragma once

#include <stdbool.h>

/* First-run completion is independent from Wi-Fi/printer configuration so
 * Setup can be opened again later without rewriting any operator settings. */
void onboarding_controller_init(void);
bool onboarding_controller_should_show(void);
bool onboarding_controller_mark_complete(void);

/* Informational and preference steps are tracked separately from live network,
 * profile and camera readiness. Previously finished setups satisfy these steps. */
typedef enum {
    ONBOARDING_STEP_WELCOME,
    ONBOARDING_STEP_TIMEZONE,
    ONBOARDING_STEP_DISPLAY,
    ONBOARDING_STEP_COUNT
} onboarding_step_t;
bool onboarding_controller_step_complete(onboarding_step_t step);
bool onboarding_controller_mark_step_complete(onboarding_step_t step);
