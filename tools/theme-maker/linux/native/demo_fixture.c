/* Desktop is already offline; this adapter never changes a real boot flag. */
#include "demo_mode.h"
static bool active=true;
void host_demo_fixture(int value) { active=value!=0; }
bool host_demo_scenario(const char *scenario);
bool demo_mode_enabled(void) { return active; }
bool demo_mode_select(bool enabled) { (void)enabled;return false; }
bool demo_printer_scenario(const char *scenario) { return host_demo_scenario(scenario); }
