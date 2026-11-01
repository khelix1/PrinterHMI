#pragma once
#include "moonraker.h"
/* Called by the demo backend while it owns its state mutex. */
void demo_services_publish(const moonraker_state_t *state, double light);
bool demo_services_endstops(const moonraker_state_t *state, uint32_t request);

bool demo_services_init(void);
