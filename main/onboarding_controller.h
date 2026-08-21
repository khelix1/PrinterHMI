#pragma once

#include <stdbool.h>

/* First-run completion is independent from Wi-Fi/printer configuration so
 * Setup can be opened again later without rewriting any operator settings. */
void onboarding_controller_init(void);
bool onboarding_controller_should_show(void);
bool onboarding_controller_mark_complete(void);
void onboarding_controller_reset(void);
